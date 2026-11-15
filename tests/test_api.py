#!/usr/bin/env python3
"""End-to-end API tests. Python is used ONLY as an HTTP client / SQLite inspector; the system under test is the C++ server.
Usage: python3 tests/test_api.py http://127.0.0.1:8099 path/to/medicine_system.db"""
import sys, json, sqlite3, urllib.request, urllib.error, re

BASE = sys.argv[1] if len(sys.argv) > 1 else "http://127.0.0.1:8080"
DBFILE = sys.argv[2] if len(sys.argv) > 2 else None
passed = failed = 0
failures = []

def C(method, path, body=None, token=None, raw=False):
    data = None if body is None else (body if isinstance(body, bytes) else json.dumps(body).encode())
    req = urllib.request.Request(BASE + path, data=data, method=method)
    if token: req.add_header("Authorization", "Bearer " + token)
    if data is not None: req.add_header("Content-Type", "application/json")
    try:
        with urllib.request.urlopen(req, timeout=30) as r: status, txt = r.status, r.read().decode()
    except urllib.error.HTTPError as e: status, txt = e.code, e.read().decode()
    if raw: return status, txt
    try: return status, json.loads(txt)
    except Exception: return status, {"_raw": txt}

def check(cond, name, detail=""):
    global passed, failed
    if cond: passed += 1
    else:
        failed += 1; failures.append(name + (" :: " + str(detail)[:300] if detail else "")); print("  FAIL:", name, str(detail)[:300])

def section(t): print("\n== " + t)
def login(u, p):
    s, r = C("POST", "/api/auth/login", {"username": u, "password": p}); assert s == 200, (u, s, r); return r["token"]
def db():
    c = sqlite3.connect(DBFILE); c.row_factory = sqlite3.Row; return c

section("Health & authentication")
s, r = C("GET", "/api/health"); check(s == 200 and r["status"] == "ok", "health")
s, r = C("GET", "/api/public/info"); check(s == 200 and "Demonstration" in r["data_notice"], "public data notice")
check(C("POST", "/api/auth/login", {"username": "admin", "password": "wrong"})[0] == 401, "wrong password -> 401")
check(C("POST", "/api/auth/login", {"username": "nobody", "password": "x"})[0] == 401, "unknown user -> 401")
check(C("POST", "/api/auth/login", b"{bad json")[0] == 400, "malformed JSON -> 400")
check(C("GET", "/api/patients")[0] == 401, "no token -> 401")
check(C("GET", "/api/patients", token="x" * 64)[0] == 401, "garbage token -> 401")
ADMIN = login("admin", "Admin@123"); PH = login("pharmacist", "Pharma@123"); PH1 = login("pharmacist.doon", "Pharma@123")
PAT = login("patient", "Patient@123"); PAT2 = login("patient2", "Patient@123")
s, r = C("GET", "/api/auth/me", token=PAT); check(s == 200 and r["role"] == "Patient" and r["patient_id"] == 1, "me (patient)")
s, r = C("GET", "/api/auth/me", token=PH1); check(r.get("pharmacy_id") == 1, "me (scoped pharmacist)")

section("Role-based authorization")
check(C("GET", "/api/patients", token=PAT)[0] == 403, "patient cannot list patients")
check(C("GET", "/api/users", token=PH)[0] == 403, "pharmacist cannot list users")
check(C("GET", "/api/users", token=ADMIN)[0] == 200, "admin can list users")
check(C("POST", "/api/medicines", {"name": "x"}, token=PH)[0] == 403, "pharmacist cannot create medicines")
check(C("POST", "/api/inventory/restock", {}, token=PAT)[0] == 403, "patient cannot restock")
check(C("GET", "/api/analytics/summary", token=PAT)[0] == 403, "patient cannot see analytics")
check(C("GET", "/api/audit-logs", token=PH)[0] == 403, "pharmacist cannot read audit log")
check(C("POST", "/api/distribution/next", {}, token=PAT)[0] == 403, "patient cannot run distribution")
check(C("GET", "/api/patients/2", token=PAT)[0] == 403, "patient cannot read another patient")
check(C("GET", "/api/patients/1", token=PAT)[0] == 200, "patient can read self")
s, r = C("GET", "/api/requests", token=PAT); check(s == 200 and all(x["patient_id"] == 1 for x in r["items"]), "patient sees only own requests")
s, r = C("GET", "/api/transactions", token=PAT2); check(s == 200 and all(x["patient_id"] == 2 for x in r["items"]), "patient sees only own transactions")

section("Medicine catalogue (1000+ distinct, search, filter, sort)")
s, r = C("GET", "/api/medicines?limit=1", token=ADMIN); check(r["total"] >= 1000, "catalogue has >= 1000 medicines", r["total"])
s, r = C("GET", "/api/medicines?q=paracetamol&limit=200", token=ADMIN); check(r["total"] > 5 and all("paracetamol" in (m["generic_name"] + m["name"]).lower() for m in r["items"]), "search by generic/name")
s, r = C("GET", "/api/medicines?sort=price&order=asc&limit=100", token=ADMIN); pr = [m["unit_price"] for m in r["items"]]; check(pr == sorted(pr) and len(pr) == 100, "sort price asc")
s, r = C("GET", "/api/medicines?sort=price&order=desc&limit=100", token=ADMIN); pr = [m["unit_price"] for m in r["items"]]; check(pr == sorted(pr, reverse=True), "sort price desc")
s, r = C("GET", "/api/medicines?rx=1&limit=200", token=ADMIN); check(r["total"] > 100 and all(m["requires_prescription"] for m in r["items"]), "filter prescription-only")
s, r = C("GET", "/api/medicines?category=Antibiotics&form=Capsule&limit=200", token=ADMIN); check(r["total"] > 0 and all(m["category"] == "Antibiotics" and m["dosage_form"] == "Capsule" for m in r["items"]), "filter category+form")
s, r = C("GET", "/api/medicines?q=" + urllib.request.quote("' OR 1=1 --"), token=ADMIN); check(s == 200 and r["total"] == 0, "SQL-injection style search returns nothing")
s, r = C("GET", "/api/medicines?sort=bogus", token=ADMIN); check(s == 400, "invalid sort rejected")
s, r = C("GET", "/api/medicines/5", token=PAT); check(s == 200 and "availability" in r, "medicine detail with availability")
check(C("GET", "/api/medicines/99999999", token=PAT)[0] == 404, "unknown medicine -> 404")
check(C("GET", "/api/medicines/abc", token=PAT)[0] == 400, "non-numeric id -> 400")
s, r = C("GET", "/api/categories", token=PAT); cats = {c["name"] for c in r["items"]}
check({"Antibiotics", "Analgesics", "Antipyretics", "Antacids", "Antihistamines", "Antidiabetics", "Cardiovascular", "Dermatological", "Vitamins and Supplements", "Respiratory", "Gastrointestinal", "Neurological"} <= cats, "all 12 required categories present")

section("Medicine CRUD (Admin)")
s, r = C("POST", "/api/medicines", {"name": "TestMed 10mg Tablet", "generic_name": "Testamine", "brand_name": "TestMed", "category": "Antibiotics", "manufacturer": "Test Labs",
                                      "composition": "Testamine 10 mg", "strength": "10mg", "dosage_form": "Tablet", "unit_price": 12.5, "requires_prescription": False}, token=ADMIN)
check(s == 201, "create medicine", r); mid_new = r.get("medicine_id")
check(C("POST", "/api/medicines", {"name": "TestMed 10mg Tablet", "generic_name": "Testamine", "brand_name": "TestMed", "category": "Antibiotics", "manufacturer": "Test Labs", "composition": "x y", "strength": "10mg", "dosage_form": "Tablet", "unit_price": 1}, token=ADMIN)[0] == 409, "duplicate medicine name -> 409")
check(C("POST", "/api/medicines", {"name": "Bad", "generic_name": "B", "brand_name": "Bad", "category": "A", "manufacturer": "M", "composition": "c", "strength": "1", "dosage_form": "Tablet", "unit_price": -5}, token=ADMIN)[0] == 422, "negative price rejected")
check(C("PUT", f"/api/medicines/{mid_new}", {"unit_price": 15}, token=ADMIN)[0] == 200, "update medicine")
s, r = C("GET", f"/api/medicines/{mid_new}", token=ADMIN); check(r["unit_price"] == 15, "update persisted")
s, r = C("DELETE", f"/api/medicines/{mid_new}", token=ADMIN); check(s == 200 and "deleted" in r["message"], "delete unreferenced medicine")

section("Patients CRUD & validation")
s, r = C("POST", "/api/patients", {"name": "Test Patient", "age": 34, "gender": "Male", "contact": "+91-00000-99999", "city": "Dehradun"}, token=PH); check(s == 201, "create patient", r); P = r.get("patient_id")
check(C("POST", "/api/patients", {"name": "X", "age": 30}, token=PH)[0] == 422, "short name rejected")
check(C("POST", "/api/patients", {"name": "Valid Name", "age": 500}, token=PH)[0] == 422, "age out of range rejected")
check(C("POST", "/api/patients", {"name": "Valid Name", "contact": "abc"}, token=PH)[0] == 422, "invalid phone rejected")
check(C("POST", "/api/patients", {"name": "Valid Name", "email": "nope"}, token=PH)[0] == 422, "invalid email rejected")
check(C("PUT", f"/api/patients/{P}", {"age": 35, "city": "Haridwar"}, token=PH)[0] == 200, "update patient")
s, r = C("GET", f"/api/patients/{P}", token=PH); check(r["age"] == 35 and r["city"] == "Haridwar", "patient update persisted")
s, r = C("GET", "/api/patients?q=Test%20Patient", token=PH); check(r["total"] >= 1, "search patients")
s, r = C("GET", f"/api/patients/{P}/history", token=PH); check(s == 200 and "requests" in r, "patient history")

section("Priority queue ordering (Critical > High > Medium > Normal, waiting-time tie-break)")
s, r = C("GET", "/api/medicines?rx=0&available=1&sort=availability&order=desc&limit=40", token=ADMIN); stocked = [m for m in r["items"] if m["total_stock"] >= 400]
check(len(stocked) >= 5, "enough well-stocked OTC medicines for tests", len(stocked))
new_ids = {}
for i, urg in enumerate(["Normal", "Critical", "Medium", "High"]):
    s, r = C("POST", "/api/requests", {"patient_id": P, "medicine_id": stocked[i]["medicine_id"], "quantity": 2, "urgency": urg}, token=PH); check(s == 201, f"create {urg} request", r); new_ids[urg] = r["request_id"]
s, snap = C("GET", "/api/queue", token=PH); q = snap["queue"]
prios = [x["priority"] for x in q]; check(prios == sorted(prios), "whole queue sorted by priority", prios)
check([x["queue_position"] for x in q] == list(range(1, len(q) + 1)), "queue positions are consecutive")
pos = {x["request_id"]: x["queue_position"] for x in q}
check(pos[new_ids["Critical"]] < pos[new_ids["High"]] < pos[new_ids["Medium"]] < pos[new_ids["Normal"]], "Critical<High<Medium<Normal positions", pos)
for p in (1, 2, 3, 4):
    same = [x for x in q if x["priority"] == p]; keys = [(x["requested_at"], x["request_id"]) for x in same]
    check(keys == sorted(keys), f"within priority {p}: older request first (waiting-time tie-break)")
s, r = C("GET", "/api/queue", token=PAT); check(all(x["patient_id"] == 1 for x in r["queue"]) and all("queue_position" in x for x in r["queue"]), "patient queue view is limited to own entries")
check(C("POST", "/api/requests", {"patient_id": P, "medicine_id": stocked[0]["medicine_id"], "quantity": 2, "urgency": "Normal"}, token=PH)[0] == 409, "duplicate open request -> 409")

section("Priority changes are controlled and recorded")
rid = new_ids["Normal"]
check(C("PUT", f"/api/requests/{rid}/urgency", {"urgency": "Critical", "reason": "Patient condition worsened"}, token=PAT)[0] == 403, "patient cannot change urgency")
check(C("PUT", f"/api/requests/{rid}/urgency", {"urgency": "Critical"}, token=PH)[0] == 422, "reason required")
check(C("PUT", f"/api/requests/{rid}/urgency", {"urgency": "Urgent!", "reason": "valid reason here"}, token=PH)[0] == 422, "invalid urgency rejected")
before = {x["request_id"]: x["queue_position"] for x in C("GET", "/api/queue", token=PH)[1]["queue"]}[rid]
check(C("PUT", f"/api/requests/{rid}/urgency", {"urgency": "Critical", "reason": "Patient condition worsened"}, token=PH)[0] == 200, "staff changes urgency")
after = {x["request_id"]: x["queue_position"] for x in C("GET", "/api/queue", token=PH)[1]["queue"]}[rid]
check(after < before, "request moved up the queue", (before, after))
s, r = C("GET", f"/api/requests/{rid}", token=PH); check(len(r["priority_history"]) == 2 and r["priority_history"][1]["new_urgency"] == "Critical" and r["priority_history"][1]["reason"], "priority change recorded with reason", r.get("priority_history"))
s, r = C("POST", "/api/requests", {"medicine_id": stocked[4]["medicine_id"], "quantity": 1, "urgency": "Critical"}, token=PAT); check(s == 201, "patient self-service request", r)
s, r2 = C("GET", f"/api/requests/{r['request_id']}", token=PAT); check(r2["urgency"] == "Normal", "patient cannot self-assign Critical (forced Normal)")
check(C("POST", f"/api/requests/{r['request_id']}/cancel", {}, token=PAT2)[0] == 403, "patient cannot cancel someone else's request")
check(C("POST", f"/api/requests/{r['request_id']}/cancel", {}, token=PAT)[0] == 200, "patient cancels own request")

section("Prescription rules")
s, r = C("GET", "/api/medicines?rx=1&available=1&limit=50", token=ADMIN); rxmed = r["items"][0]["medicine_id"]
check(C("POST", "/api/requests", {"patient_id": P, "medicine_id": rxmed, "quantity": 1, "urgency": "Normal"}, token=PH)[0] == 422, "Rx medicine without prescription -> 422")
today = __import__("datetime").date.today().isoformat()
s, r = C("POST", "/api/prescriptions", {"patient_id": P, "medicine_id": rxmed, "quantity": 5, "doctor_name": "Dr Test Doc", "issued_date": today}, token=PH); check(s == 201, "record prescription", r); rxid = r.get("prescription_id")
check(C("POST", "/api/prescriptions", {"patient_id": P, "medicine_id": rxmed, "quantity": 5, "doctor_name": "Dr Test Doc", "issued_date": "2999-01-01"}, token=PH)[0] == 422, "future-dated prescription rejected")
check(C("POST", f"/api/prescriptions/{rxid}/verify", {}, token=ADMIN)[0] == 403, "only a pharmacist may verify prescriptions")
s, r = C("POST", "/api/requests", {"patient_id": P, "medicine_id": rxmed, "quantity": 1, "urgency": "Normal", "prescription_id": rxid}, token=PH); check(s == 201, "Rx request with prescription accepted", r); rx_req = r.get("request_id")

section("Distribution workflow: drain the queue in priority order, verify stock deduction")
def avail(ph, med):
    s, r = C("GET", f"/api/inventory?pharmacy_id={ph}&medicine_id={med}", token=ADMIN); return r["items"][0]["available"] if r["items"] else 0
check(C("POST", "/api/distribution/distribute", {"request_id": rx_req, "pharmacy_id": 1}, token=PH)[0] in (409, 422), "cannot distribute out of priority order / unverified Rx")
popped = []; guard = 0; distributed = 0; waited = 0; verified_rx = 0
while guard < 200:
    guard += 1
    s, r = C("POST", "/api/distribution/next", {}, token=PH); check(s == 200, "evaluate next", r)
    act = r["action"]
    if act == "queue_empty": break
    req = r["request"]; rid2 = req["request_id"]
    if act == "blocked_prescription":
        rq = C("GET", f"/api/requests/{rid2}", token=PH)[1]
        check(C("POST", f"/api/prescriptions/{rq['prescription_id']}/verify", {}, token=PH)[0] == 200, "verify blocking prescription"); verified_rx += 1; continue
    popped.append((req["priority"], req["requested_at"], rid2))
    if act == "ready":
        rec = r["details"]["recommendations"][0]; med = r["details"]["medicine"]["medicine_id"]; qty = req["quantity"]
        b = avail(rec["pharmacy_id"], med); s, d = C("POST", "/api/distribution/distribute", {"request_id": rid2, "pharmacy_id": rec["pharmacy_id"]}, token=PH)
        check(s == 200, "distribute head of queue", d); a2 = avail(rec["pharmacy_id"], med)
        check(b - a2 == qty, "stock reduced by exactly the distributed quantity", (b, a2, qty)); distributed += 1
    elif act == "added_to_waiting_list": waited += 1
    else: check(False, "unexpected action " + act)
check(popped == sorted(popped), "requests were processed in (priority, request time, id) order", popped[:8])
check(distributed >= 10, "distributed many requests", distributed)
s, r = C("GET", "/api/queue", token=PH); check(len(r["queue"]) == 0, "queue empty after draining")
s, r = C("GET", "/api/transactions?status=Distributed&limit=500", token=ADMIN); check(r["total"] >= distributed, "transactions recorded")
tx = C("GET", "/api/transactions?limit=1", token=ADMIN)[1]["items"][0]
s, r = C("GET", f"/api/transactions/{tx['transaction_id']}", token=ADMIN); check(s == 200 and (tx["status"] != "Distributed" or len(r["batches"]) >= 1), "transaction exposes batches used (FEFO)")
s, r = C("GET", "/api/notifications", token=PAT); check(s == 200 and r["unread"] >= 0, "notifications endpoint")

section("Atomicity & scoped pharmacist")
med = stocked[0]["medicine_id"]; s, r = C("POST", "/api/requests", {"patient_id": P, "medicine_id": med, "quantity": 3, "urgency": "High"}, token=PH)
rq_id = r["request_id"]; ntx_before = C("GET", "/api/transactions?limit=1", token=ADMIN)[1]["total"]
s, r = C("GET", f"/api/distribution/recommend/{rq_id}", token=PH); rec = r["recommendations"]; ok_ph = rec[0]["pharmacy_id"]
bad_ph = db().execute("SELECT pharmacy_id FROM pharmacies WHERE status='Open' AND pharmacy_id NOT IN (SELECT pharmacy_id FROM pharmacy_inventory WHERE medicine_id=?) LIMIT 1", (med,)).fetchone()[0] if DBFILE else 30
s, r = C("POST", "/api/distribution/distribute", {"request_id": rq_id, "pharmacy_id": bad_ph}, token=PH); check(s == 409, "distribution from pharmacy without stock rejected", r)
check(C("GET", f"/api/requests/{rq_id}", token=PH)[1]["status"] == "Pending", "failed distribution leaves request Pending")
check(C("GET", "/api/transactions?limit=1", token=ADMIN)[1]["total"] == ntx_before, "failed distribution wrote no transaction")
other = 2 if ok_ph != 2 else 3
check(C("POST", "/api/distribution/distribute", {"request_id": rq_id, "pharmacy_id": other}, token=PH1)[0] == 403, "pharmacist scoped to pharmacy 1 cannot distribute from another")
check(C("POST", "/api/distribution/distribute", {"request_id": rq_id, "pharmacy_id": ok_ph}, token=PH)[0] == 200, "valid distribution succeeds")
check(C("POST", "/api/distribution/distribute", {"request_id": rq_id, "pharmacy_id": ok_ph}, token=PH)[0] == 409, "cannot distribute the same request twice")
s, r = C("POST", "/api/distribution/reject", {"request_id": 999999, "reason": "does not exist"}, token=PH); check(s == 404, "reject unknown request -> 404")

section("Waiting list, alternatives, pharmacist-approved substitution, restock promotion")
s, r = C("GET", "/api/medicines?rx=0&sort=availability&order=asc&limit=200", token=ADMIN)
cand = [m for m in r["items"] if m["total_stock"] == 0]; check(len(cand) > 5, "medicines with zero stock exist (shortage scenario)", len(cand))
target = alt = None
for m in cand:
    a = C("GET", f"/api/medicines/{m['medicine_id']}/alternatives", token=PH)[1]
    good = [x for x in a["alternatives"] if x["available_units"] > 0 and x["match_type"] == "Same composition" and not x["requires_prescription"]]
    if good: target, alt = m, good[0]; break
check(target is not None, "found out-of-stock medicine with an available same-composition alternative")
check(a["requires_pharmacist_approval"] is True and "pharmacist" in a["disclaimer"].lower(), "alternatives flagged as needing pharmacist approval")
s, r = C("POST", "/api/requests", {"patient_id": 1, "medicine_id": target["medicine_id"], "quantity": 4, "urgency": "Critical"}, token=PH); check(s == 201, "request for unavailable medicine", r); wid = r["request_id"]
check(r["in_stock_now"] is False, "creation reports medicine not in stock")
s, r = C("POST", "/api/distribution/next", {}, token=PH); check(r["action"] == "added_to_waiting_list" and r["request"]["request_id"] == wid, "head request moved to waiting list", r.get("action"))
check(len(r["details"]["alternatives"]) > 0, "alternatives returned with the waiting-list decision")
s, r = C("GET", "/api/waiting-list", token=PAT); check(any(x["request_id"] == wid for x in r["items"]), "patient sees own waiting-list entry")
s, r = C("GET", "/api/notifications", token=PAT); check(any("unavailable" in n["title"].lower() for n in r["items"]), "patient notified about waiting list")
check(C("POST", f"/api/requests/{wid}/substitution", {"alternative_id": alt["medicine_id"]}, token=PAT)[0] == 403, "patient cannot propose substitution")
check(C("POST", f"/api/requests/{wid}/substitution", {"alternative_id": 1}, token=PH)[0] == 422, "unrelated medicine rejected as alternative")
check(C("POST", f"/api/requests/{wid}/substitution", {"alternative_id": alt["medicine_id"]}, token=PH)[0] == 200, "pharmacist proposes alternative")
check(C("POST", f"/api/requests/{wid}/substitution/decision", {"approve": True}, token=ADMIN)[0] == 403, "admin cannot approve clinical substitution")
check(C("GET", f"/api/requests/{wid}", token=PH)[1]["status"] == "Waiting", "request unchanged until approval")
check(C("POST", f"/api/requests/{wid}/substitution/decision", {"approve": True}, token=PH)[0] == 200, "pharmacist approves substitution")
rq = C("GET", f"/api/requests/{wid}", token=PH)[1]; check(rq["status"] == "Pending" and rq["substitution_status"] == "Approved", "approved request returns to the priority queue", rq["status"])
s, r = C("POST", "/api/distribution/next", {}, token=PH); check(r["action"] == "ready" and r["details"]["medicine"]["medicine_id"] == alt["medicine_id"], "queue now serves the approved alternative", r.get("action"))
rec = r["details"]["recommendations"][0]; b = avail(rec["pharmacy_id"], alt["medicine_id"])
s, d = C("POST", "/api/distribution/distribute", {"request_id": wid, "pharmacy_id": rec["pharmacy_id"]}, token=PH); check(s == 200, "distribute approved alternative", d)
check(b - avail(rec["pharmacy_id"], alt["medicine_id"]) == 4, "alternative's stock reduced (not the unavailable medicine)")
check(C("GET", "/api/waiting-list?status=Completed", token=PH)[1]["items"] != [], "waiting entry marked Completed")
# restock promotion
s, r = C("POST", "/api/requests", {"patient_id": P, "medicine_id": target["medicine_id"], "quantity": 6, "urgency": "High"}, token=PH); w2 = r["request_id"]
C("POST", "/api/distribution/next", {}, token=PH); check(C("GET", f"/api/requests/{w2}", token=PH)[1]["status"] == "Waiting", "second request waits (no stock)")
exp = (__import__("datetime").date.today() + __import__("datetime").timedelta(days=400)).isoformat(); mfg = (__import__("datetime").date.today() - __import__("datetime").timedelta(days=20)).isoformat()
check(C("POST", "/api/inventory/restock", {"pharmacy_id": 1, "medicine_id": target["medicine_id"], "quantity": 50, "batch_number": "T-EXP", "manufacturing_date": mfg, "expiry_date": "2020-01-01"}, token=PH)[0] == 422, "restock with expired batch rejected")
check(C("POST", "/api/inventory/restock", {"pharmacy_id": 1, "medicine_id": target["medicine_id"], "quantity": -5, "batch_number": "T-NEG", "manufacturing_date": mfg, "expiry_date": exp}, token=PH)[0] == 422, "negative restock rejected")
check(C("POST", "/api/inventory/restock", {"pharmacy_id": 1, "medicine_id": target["medicine_id"], "quantity": 5, "batch_number": "T-FUT", "manufacturing_date": "2999-01-01", "expiry_date": "3000-01-01"}, token=PH)[0] == 422, "future manufacturing date rejected")
check(C("POST", "/api/inventory/restock", {"pharmacy_id": 2, "medicine_id": target["medicine_id"], "quantity": 50, "batch_number": "T-OK", "manufacturing_date": mfg, "expiry_date": exp}, token=PH1)[0] == 403, "scoped pharmacist cannot restock another pharmacy")
s, r = C("POST", "/api/inventory/restock", {"pharmacy_id": 1, "medicine_id": target["medicine_id"], "quantity": 50, "batch_number": "T-OK", "manufacturing_date": mfg, "expiry_date": exp}, token=PH1)
check(s == 200 and r["promoted_from_waiting_list"] >= 1, "restock promotes waiting request", r)
check(C("GET", f"/api/requests/{w2}", token=PH)[1]["status"] == "Pending", "waiting request is back in the queue")
check(avail(1, target["medicine_id"]) == 50, "restocked quantity visible in inventory")
s, r = C("GET", f"/api/inventory/movements?pharmacy_id=1&medicine_id={target['medicine_id']}", token=PH); check(any(m["reason"] == "Restock" and m["change_qty"] == 50 for m in r["items"]), "stock movement recorded")
s, r = C("GET", f"/api/medicines/{target['medicine_id']}", token=PH); check(r["total_stock"] == 50, "availability on medicine detail updated")
C("POST", "/api/distribution/next", {}, token=PH); s, d = C("POST", "/api/distribution/distribute", {"request_id": w2, "pharmacy_id": 1}, token=PH); check(s == 200, "serve promoted request", d)
check(avail(1, target["medicine_id"]) == 44, "FEFO deduction after promotion")

section("Inventory: expiry, low stock, write-off")
for st in ("expired", "near", "low", "out"):
    s, r = C("GET", f"/api/inventory?status={st}&limit=5", token=ADMIN); check(s == 200 and r["total"] > 0, f"inventory status={st} returns rows", r.get("total"))
s, r = C("GET", "/api/inventory?status=expired&limit=1", token=ADMIN); exp_ph = r["items"][0]["pharmacy_id"]; exp_units = r["items"][0]["expired_qty"]
s, r = C("POST", "/api/inventory/write-off-expired", {"pharmacy_id": exp_ph}, token=ADMIN); check(s == 200 and r["units_written_off"] >= exp_units, "write off expired stock", r)
s, r = C("GET", f"/api/inventory?status=expired&pharmacy_id={exp_ph}&limit=5", token=ADMIN); check(r["total"] == 0, "no expired stock left at that pharmacy")
s, r = C("GET", "/api/inventory/alerts", token=PH); check(all(k in r for k in ("low", "expired", "near", "out")), "alerts endpoint")
check(C("PUT", f"/api/inventory/1/{target['medicine_id']}", {"reorder_level": 40}, token=PH1)[0] == 200, "set reorder level")

section("Pharmacies (30+, search, distance, CRUD)")
s, r = C("GET", "/api/pharmacies", token=PAT); check(r["total"] >= 30, "30+ pharmacies", r["total"])
check(C("GET", "/api/pharmacies?q=dehradun", token=PAT)[1]["total"] >= 5, "pharmacy search by city")
s, r = C("GET", f"/api/pharmacies/search?medicine_id={stocked[0]['medicine_id']}&lat=30.3165&lon=78.0322", token=PAT)
d = [p["distance_km"] for p in r["pharmacies"] if p["distance_km"] is not None]; check(len(d) > 1 and d == sorted(d), "pharmacy search sorted by distance", d[:5])
check("sample database" in r["note"], "search response labels sample data")
check(C("GET", "/api/pharmacies/search?lat=1&lon=2", token=PAT)[0] == 400, "search without medicine -> 400")
check(C("GET", f"/api/pharmacies/search?medicine_id={stocked[0]['medicine_id']}&lat=999&lon=2", token=PAT)[0] == 400, "invalid coordinates rejected")
check(C("POST", "/api/pharmacies", {"name": "T", "address": "a", "city": "c", "state": "s", "pin_code": "12"}, token=ADMIN)[0] == 422, "invalid pharmacy rejected")
s, r = C("POST", "/api/pharmacies", {"name": "Test Pharmacy One", "address": "1 Test Road", "city": "Dehradun", "state": "Uttarakhand", "pin_code": "248001", "phone": "+91-00000-05555", "email": "t@example.com", "latitude": 30.3, "longitude": 78.0}, token=ADMIN)
check(s == 201, "create pharmacy", r); np = r.get("pharmacy_id")
check(C("PUT", f"/api/pharmacies/{np}", {"status": "Closed"}, token=ADMIN)[0] == 200, "update pharmacy")
check(C("POST", "/api/pharmacies", {"name": "Test Pharmacy One", "address": "1 Test Road", "city": "Dehradun", "state": "Uttarakhand", "pin_code": "248001"}, token=ADMIN)[0] == 409, "duplicate pharmacy name -> 409")
check(C("DELETE", "/api/pharmacies/1", token=ADMIN)[0] == 409, "pharmacy with stock/history cannot be deleted")
check(C("DELETE", f"/api/pharmacies/{np}", token=ADMIN)[0] == 200, "delete empty pharmacy")

section("Patient deletion rules, self-registration")
rq = C("POST", "/api/requests", {"patient_id": P, "medicine_id": stocked[3]["medicine_id"], "quantity": 1, "urgency": "Normal"}, token=PH)[1]["request_id"]
check(C("DELETE", f"/api/patients/{P}", token=ADMIN)[0] == 409, "patient with open request cannot be deleted")
check(C("POST", f"/api/requests/{rq}/cancel", {}, token=PH)[0] == 200, "staff cancels request")
s, r = C("GET", f"/api/patients/{P}/history", token=PH); check(len(r["requests"]) >= 5 and len(r["transactions"]) >= 2, "history shows requests and transactions")
check(C("DELETE", f"/api/patients/{P}", token=ADMIN)[0] == 200, "patient deactivated after cancelling")
check(C("POST", "/api/auth/register", {"username": "newpat", "password": "short", "full_name": "New Patient"})[0] == 422, "weak password rejected")
s, r = C("POST", "/api/auth/register", {"username": "newpat", "password": "Str0ngPass1", "full_name": "New Patient", "age": 29, "city": "Dehradun"}); check(s == 201, "patient self-registration", r)
check(C("POST", "/api/auth/register", {"username": "newpat", "password": "Str0ngPass1", "full_name": "New Patient"})[0] == 409, "duplicate username -> 409")
NP = login("newpat", "Str0ngPass1"); s, r = C("GET", "/api/auth/me", token=NP); check(r["role"] == "Patient" and r["patient_id"], "new patient account linked")

section("Users, settings, audit, throttling")
check(C("POST", "/api/users", {"username": "pharm2", "password": "Pharm2Pass9", "role": "Pharmacist", "full_name": "Second Pharmacist", "pharmacy_id": 3}, token=ADMIN)[0] == 201, "admin creates pharmacist")
uid = C("GET", "/api/users?q=pharm2", token=ADMIN)[1]["items"][0]["user_id"]
check(C("PUT", f"/api/users/{uid}", {"active": False}, token=ADMIN)[0] == 200, "admin deactivates user")
check(C("POST", "/api/auth/login", {"username": "pharm2", "password": "Pharm2Pass9"})[0] == 401, "deactivated user cannot log in")
check(C("DELETE", "/api/users/1", token=ADMIN)[0] == 409, "admin cannot deactivate self")
s, r = C("GET", "/api/settings", token=ADMIN); check(any(x["key"] == "near_expiry_days" for x in r["items"]), "settings listed")
check(C("PUT", "/api/settings", {"near_expiry_days": 90}, token=ADMIN)[0] == 200, "update setting")
check(C("PUT", "/api/settings", {"bogus": 1}, token=ADMIN)[0] == 422, "unknown setting rejected")
check(C("PUT", "/api/settings", {"near_expiry_days": 0}, token=ADMIN)[0] == 422, "out-of-range setting rejected")
s, r = C("GET", "/api/audit-logs?q=PRIORITY", token=ADMIN); check(r["items"] and r["items"][0]["action"] == "PRIORITY_CHANGED", "audit log records priority change")
s, r = C("GET", "/api/audit-logs?q=DISTRIBUTED", token=ADMIN); check(len(r["items"]) >= 10, "audit log records distributions")
check(C("POST", "/api/auth/change-password", {"current_password": "wrong", "new_password": "NewPass123"}, token=NP)[0] == 403, "change password requires current password")
check(C("POST", "/api/auth/change-password", {"current_password": "Str0ngPass1", "new_password": "NewPass123"}, token=NP)[0] == 200, "change password")
check(C("GET", "/api/auth/me", token=NP)[0] == 401, "sessions revoked after password change")
codes = [C("POST", "/api/auth/login", {"username": "patient2", "password": "bad" + str(i)})[0] for i in range(7)]
check(429 in codes, "login throttled after repeated failures", codes)
check(C("POST", "/api/auth/login", {"username": "patient2", "password": "Patient@123"})[0] == 429, "locked even with the right password while throttled")
s, r = C("POST", "/api/auth/logout", {}, token=PAT); check(s == 200 and C("GET", "/api/auth/me", token=PAT)[0] == 401, "logout invalidates token")

section("Analytics (computed from the database)")
s, r = C("GET", "/api/analytics/summary", token=ADMIN)
if DBFILE:
    c = db()
    check(r["patients"] == c.execute("SELECT COUNT(*) FROM patients WHERE active=1").fetchone()[0], "summary patients matches DB")
    check(r["medicines"] == c.execute("SELECT COUNT(*) FROM medicines WHERE status='Active'").fetchone()[0], "summary medicines matches DB")
    check(r["completed_distributions"] == c.execute("SELECT COUNT(*) FROM distribution_transactions WHERE status='Distributed'").fetchone()[0], "summary distributions matches DB")
    check(r["expired_batches"] == c.execute("SELECT COUNT(*) FROM medicine_batches WHERE expiry_date<date('now') AND quantity>0").fetchone()[0], "summary expired batches matches DB")
check(r["pharmacies"] >= 30 and r["unavailable_medicines"] > 0, "summary sane", r)
s, r = C("GET", "/api/analytics/trends?days=14", token=ADMIN); check(len(r["items"]) == 14 and sum(x["distributions"] for x in r["items"]) > 10, "14-day trend with zero-filled days")
s, r = C("GET", "/api/analytics/top-medicines?limit=5", token=ADMIN); check(len(r["items"]) == 5 and r["items"][0]["requests"] >= r["items"][-1]["requests"], "top medicines sorted")
s, r = C("GET", "/api/analytics/pharmacy-inventory", token=ADMIN); check(len(r["items"]) >= 30, "pharmacy-wise inventory")
s, r = C("GET", "/api/analytics/category-stock", token=ADMIN); check(len(r["items"]) >= 12, "category stock")
s, r = C("GET", "/api/analytics/urgency", token=ADMIN); check(set(r["counts"]) == {"Critical", "High", "Medium", "Normal"}, "urgency counts")
s, txt = C("GET", "/api/reports/transactions.csv", token=ADMIN, raw=True); check(s == 200 and txt.startswith("transaction_id,created_at"), "CSV export")
check(C("GET", "/api/reports/transactions.csv", token=PAT2)[0] == 403, "CSV export restricted to staff")
check(C("GET", "/api/nope")[0] == 404 and C("DELETE", "/api/health")[0] == 405, "404 / 405 handling")

section("Database invariants after all operations")
if DBFILE:
    c = db()
    bad = c.execute("SELECT COUNT(*) FROM pharmacy_inventory i WHERE i.quantity<>(SELECT COALESCE(SUM(quantity),0) FROM medicine_batches b WHERE b.pharmacy_id=i.pharmacy_id AND b.medicine_id=i.medicine_id)").fetchone()[0]
    check(bad == 0, "inventory quantity == sum(batch quantities) for every row", bad)
    check(c.execute("SELECT COUNT(*) FROM medicine_batches WHERE quantity<0").fetchone()[0] == 0, "no negative batch stock")
    check(c.execute("PRAGMA foreign_key_check").fetchall() == [], "no foreign key violations")
    check(c.execute("PRAGMA integrity_check").fetchone()[0] == "ok", "SQLite integrity_check ok")
    check(c.execute("SELECT COUNT(*) FROM medicine_requests WHERE status='Distributed' AND request_id NOT IN (SELECT request_id FROM distribution_transactions WHERE status='Distributed')").fetchone()[0] <= 0, "every Distributed request has a transaction")
    check(c.execute("SELECT COUNT(*) FROM (SELECT request_id FROM distribution_transactions WHERE status='Distributed' GROUP BY request_id HAVING COUNT(*)>1)").fetchone()[0] == 0, "no request distributed twice")
    check(c.execute("SELECT COUNT(*) FROM waiting_list WHERE status='Waiting' AND request_id IN (SELECT request_id FROM medicine_requests WHERE status<>'Waiting')").fetchone()[0] == 0, "waiting list consistent with request status")
    try: c.execute("UPDATE pharmacy_inventory SET quantity=-1 WHERE rowid=1"); neg = False
    except sqlite3.IntegrityError: neg = True
    check(neg, "database rejects negative stock (CHECK constraint)")
    tb = c.execute("SELECT COUNT(*) FROM (SELECT t.transaction_id,t.quantity,(SELECT COALESCE(SUM(quantity),0) FROM transaction_batches b WHERE b.transaction_id=t.transaction_id) s FROM distribution_transactions t WHERE t.status='Distributed' AND t.notes<>'Seeded sample history') WHERE quantity<>s").fetchone()[0]
    check(tb == 0, "live transactions: batch deductions sum to distributed quantity", tb)

print(f"\n{passed} passed, {failed} failed")
if failures: print("FAILURES:\n - " + "\n - ".join(failures))
sys.exit(1 if failed else 0)
