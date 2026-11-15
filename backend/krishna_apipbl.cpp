// ============================================================
//                   KRISHNA - SERVICES & API
//          pharmacies, availability search, medicine alternatives
// ============================================================

// Pharmacies holding at least minQty unexpired units of a medicine, nearest first when coordinates are known.
Json MedicineSystem::searchPharmacies(long long medicineId, double lat, double lon, bool hasCoords, int minQty) {
    Json rows = database.query("SELECT p.pharmacy_id,p.name,p.address,p.city,p.state,p.pin_code,p.phone,p.email,p.opening_time,p.closing_time,p.status,p.delivery_available,"
                               "p.latitude,p.longitude,SUM(b.quantity) available,MIN(b.expiry_date) earliest_expiry FROM medicine_batches b "
                               "JOIN pharmacies p ON p.pharmacy_id=b.pharmacy_id WHERE b.medicine_id=? AND b.expiry_date>=date('now') AND b.quantity>0 AND p.status='Open' "
                               "GROUP BY p.pharmacy_id HAVING available>=?", {medicineId, minQty});
    vector<Json> v;
    for (auto& r : rows.items()) {
        Json j = r; j["delivery_available"] = r.at("delivery_available").integer() == 1;
        if (hasCoords && r.at("latitude").isNumber()) j["distance_km"] = floor(util::haversineKm(lat, lon, r.at("latitude").num(), r.at("longitude").num()) * 10 + 0.5) / 10;
        else j["distance_km"] = Json();
        v.push_back(j);
    }
    sort(v.begin(), v.end(), [&](const Json& a, const Json& b) {
        if (hasCoords && a.at("distance_km").isNumber() && b.at("distance_km").isNumber() && a.at("distance_km").num() != b.at("distance_km").num()) return a.at("distance_km").num() < b.at("distance_km").num();
        if (a.at("available").integer() != b.at("available").integer()) return a.at("available").integer() > b.at("available").integer();
        return a.at("name").str() < b.at("name").str();
    });
    Json out = Json::array(); for (auto& j : v) out.push_back(j); return out;
}

Json MedicineSystem::alternativesFor(long long medicineId) {
    Json base = database.one("SELECT m.medicine_id,m.name,m.generic_name,m.strength,m.dosage_form,m.requires_prescription rx,m.unit_price,m.composition FROM medicines m WHERE m.medicine_id=?", {medicineId});
    if (base.isNull()) throw ApiError(404, "Medicine not found");
    Json alts = database.query("SELECT a.medicine_id,a.name,a.generic_name,a.brand_name,a.strength,a.dosage_form,a.requires_prescription rx,a.unit_price,a.composition,a.status,x.match_type,mf.name manufacturer "
                               "FROM medicine_alternatives x JOIN medicines a ON a.medicine_id=x.alternative_id JOIN manufacturers mf ON mf.manufacturer_id=a.manufacturer_id "
                               "WHERE x.medicine_id=? AND a.status='Active' ORDER BY x.match_type='Same composition' DESC,a.unit_price LIMIT 25", {medicineId});
    Json list = Json::array();
    for (auto& a : alts.items()) {
        Json j = a; j["requires_prescription"] = a.at("rx").integer() == 1;
        j["same_generic"] = a.at("generic_name").str() == base.at("generic_name").str();
        j["same_strength"] = a.at("strength").str() == base.at("strength").str();
        j["same_form"] = a.at("dosage_form").str() == base.at("dosage_form").str();
        j["price_difference"] = floor((a.at("unit_price").num() - base.at("unit_price").num()) * 100 + 0.5) / 100;
        Json ph = searchPharmacies(a.at("medicine_id").integer(), 0, 0, false, 1);
        long long units = 0; for (auto& p : ph.items()) units += p.at("available").integer();
        j["available_units"] = units; j["pharmacy_count"] = (long long)ph.size();
        Json top = Json::array(); for (size_t i = 0; i < ph.size() && i < 3; i++) { Json t = Json::object(); t["pharmacy_id"] = ph.items()[i].at("pharmacy_id"); t["name"] = ph.items()[i].at("name"); t["city"] = ph.items()[i].at("city"); t["available"] = ph.items()[i].at("available"); top.push_back(t); }
        j["pharmacies"] = top; list.push_back(j);
    }
    stable_sort(list.items().begin(), list.items().end(), [](const Json& a, const Json& b) { return (a.at("available_units").integer() > 0) > (b.at("available_units").integer() > 0); });
    Json out = Json::object(); out["medicine"] = base; out["alternatives"] = list;
    out["requires_pharmacist_approval"] = true;
    out["disclaimer"] = "Suggestions only. A substitute must be confirmed by an authorised pharmacist or healthcare professional; prescription requirements and clinical suitability must be respected.";
    return out;
}

void MedicineSystem::proposeSubstitution(long long requestId, long long altId, const Auth& by) {
    Json rq = database.one("SELECT medicine_id,status,patient_id,substitution_status FROM medicine_requests WHERE request_id=?", {requestId});
    if (rq.isNull()) throw ApiError(404, "Request not found");
    if (rq.at("status").str() != "Waiting") throw ApiError(409, "Substitution can only be proposed for requests on the waiting list");
    Json link = database.one("SELECT match_type FROM medicine_alternatives WHERE medicine_id=? AND alternative_id=?", {rq.at("medicine_id").integer(), altId});
    if (link.isNull()) throw ApiError(422, "That medicine is not a recognised alternative for the requested medicine");
    Json alt = database.one("SELECT name,status FROM medicines WHERE medicine_id=?", {altId});
    if (alt.at("status").str() != "Active") throw ApiError(409, "Alternative is discontinued");
    database.run("UPDATE medicine_requests SET substitute_medicine_id=?,substitution_status='Proposed',substitution_by=NULL,updated_at=datetime('now') WHERE request_id=?", {altId, requestId});
    database.notify(0, rq.at("patient_id").integer(), "", "Substitution", "Alternative medicine proposed",
                    "A pharmacist is reviewing an alternative (" + alt.at("name").str() + ") for request #" + to_string(requestId) + ". Nothing is substituted until it is approved.");
    database.audit(by, "SUBSTITUTION_PROPOSED", "request", to_string(requestId), alt.at("name").str() + " [" + link.at("match_type").str() + "]");
}

void MedicineSystem::decideSubstitution(long long requestId, bool approve, const string& note, const Auth& by) {
    Json rq = database.one("SELECT r.medicine_id,r.status,r.patient_id,r.substitution_status,r.substitute_medicine_id,r.prescription_id FROM medicine_requests r WHERE r.request_id=?", {requestId});
    if (rq.isNull()) throw ApiError(404, "Request not found");
    if (rq.at("substitution_status").str() != "Proposed") throw ApiError(409, "No substitution is awaiting a decision for this request");
    long long altId = rq.at("substitute_medicine_id").integer();
    Json link = database.one("SELECT match_type FROM medicine_alternatives WHERE medicine_id=? AND alternative_id=?", {rq.at("medicine_id").integer(), altId});
    Json alt = database.one("SELECT name,requires_prescription rx FROM medicines WHERE medicine_id=?", {altId});
    if (approve) {
        if (!link.isNull() && link.at("match_type").str() != "Same composition" && note.size() < 5)
            throw ApiError(422, "This alternative differs in strength: a clinical justification note is required to approve it");
        if (alt.at("rx").integer() == 1) {
            Json rx = rq.at("prescription_id").isNull() ? Json() : database.one("SELECT verified FROM prescriptions WHERE prescription_id=?", {rq.at("prescription_id").integer()});
            if (rx.isNull() || rx.at("verified").integer() != 1) throw ApiError(422, "The alternative needs a verified prescription before it can be approved");
        }
    }
    database.run("UPDATE medicine_requests SET substitution_status=?,substitution_by=?,updated_at=datetime('now') WHERE request_id=?", {approve ? "Approved" : "Rejected", actorVal(by), requestId});
    database.notify(0, rq.at("patient_id").integer(), "", "Substitution", approve ? "Alternative medicine approved" : "Alternative medicine declined",
                    approve ? "A pharmacist approved " + alt.at("name").str() + " for request #" + to_string(requestId) + "." : "The proposed alternative for request #" + to_string(requestId) + " was declined; you remain on the waiting list for the original medicine.");
    database.audit(by, approve ? "SUBSTITUTION_APPROVED" : "SUBSTITUTION_REJECTED", "request", to_string(requestId), alt.at("name").str() + (note.empty() ? "" : " | " + note));
    if (approve) { reloadStock(); promoteWaiting(); }
}

// Validates pharmacy fields.
static void pharmacyFields(const Json& b, bool partial, vector<pair<string, Val>>& f) {
        auto text = [&](const char* k, size_t lo, size_t hi) { if (!partial || b.has(k)) f.push_back({k, Val(reqStr(b, k, lo, hi))}); };
        text("name", 3, 100); text("address", 3, 200); text("city", 2, 60); text("state", 2, 60);
        if (!partial || b.has("pin_code")) { string pin = reqStr(b, "pin_code", 6, 6); for (char c : pin) if (!isdigit((unsigned char)c)) throw ApiError(422, "pin_code must be 6 digits"); f.push_back({"pin_code", Val(pin)}); }
        if (b.has("phone")) { string ph = optStr(b, "phone", 25); if (!util::validPhone(ph)) throw ApiError(422, "Invalid phone number"); f.push_back({"phone", Val(ph)}); }
        if (b.has("email")) { string e = optStr(b, "email", 120); if (!util::validEmail(e)) throw ApiError(422, "Invalid email address"); f.push_back({"email", Val(e)}); }
        for (const char* k : {"opening_time", "closing_time"}) if (b.has(k)) {
            string t = optStr(b, k, 5); if (t.size() != 5 || t[2] != ':' || !isdigit((unsigned char)t[0]) || !isdigit((unsigned char)t[1]) || !isdigit((unsigned char)t[3]) || !isdigit((unsigned char)t[4]) || atoi(t.substr(0, 2).c_str()) > 23 || atoi(t.substr(3, 2).c_str()) > 59) throw ApiError(422, string(k) + " must be HH:MM");
            f.push_back({k, Val(t)});
        }
        if (b.has("status")) { string s = reqStr(b, "status", 4, 10); if (s != "Open" && s != "Closed" && s != "Suspended") throw ApiError(422, "status must be Open, Closed or Suspended"); f.push_back({"status", Val(s)}); }
        if (b.has("delivery_available")) f.push_back({"delivery_available", Val(b.at("delivery_available").boolean() ? 1 : 0)});
        if (b.has("latitude")) { double v = b.at("latitude").num(1000); if (v < -90 || v > 90) throw ApiError(422, "latitude out of range"); f.push_back({"latitude", Val(v)}); }
        if (b.has("longitude")) { double v = b.at("longitude").num(1000); if (v < -180 || v > 180) throw ApiError(422, "longitude out of range"); f.push_back({"longitude", Val(v)}); }
    }

inline void registerPharmacyRoutes(Router& r, MedicineSystem& sys) {
    Database& db = sys.database;

    r.get("/api/pharmacies", [&](HttpRequest& req, HttpResponse& res) {
        requireAny(sys, req); string q = util::lower(req.q("q")), city = util::lower(req.q("city")), status = req.q("status"); bool delivery = req.q("delivery") == "1";
        Json items = Json::array();
        for (const auto& p : sys.pharmacies) {
            if (!q.empty() && util::lower(p.name).find(q) == string::npos && util::lower(p.address).find(q) == string::npos && util::lower(p.city).find(q) == string::npos && p.pin.find(q) == string::npos) continue;
            if (!city.empty() && util::lower(p.city) != city) continue;
            if (!status.empty() && p.status != status) continue;
            if (delivery && !p.delivery) continue;
            items.push_back(p.toJson());
        }
        Json j = Json::object(); j["total"] = (long long)items.size(); j["items"] = items; res.send(200, j);
    });
    r.get("/api/pharmacies/search", [&](HttpRequest& req, HttpResponse& res) {       // patients may search for pharmacies stocking a medicine
        requireAny(sys, req); long long mid = queryInt(req, "medicine_id", 0, 0, 1000000000);
        if (!mid) { string name = req.q("medicine"); if (name.empty()) throw ApiError(400, "medicine_id or medicine is required");
                    auto it = sys.inventory.find(name); if (it == sys.inventory.end()) throw ApiError(404, "Medicine not found"); mid = it->second.id; }
        bool coords = !req.q("lat").empty() && !req.q("lon").empty(); double lat = 0, lon = 0;
        if (coords) { lat = atof(req.q("lat").c_str()); lon = atof(req.q("lon").c_str()); if (lat < -90 || lat > 90 || lon < -180 || lon > 180) throw ApiError(400, "Invalid coordinates"); }
        Json med = db.one("SELECT medicine_id,name,generic_name,strength,dosage_form FROM medicines WHERE medicine_id=?", {mid}); if (med.isNull()) throw ApiError(404, "Medicine not found");
        Json j = Json::object(); j["medicine"] = med; j["pharmacies"] = sys.searchPharmacies(mid, lat, lon, coords, (int)queryInt(req, "min_qty", 1, 1, 100000));
        j["note"] = "Availability reflects sample database stock, not live pharmacy inventory. Distances are straight-line estimates."; res.send(200, j);
    });
    r.get("/api/pharmacies/:id", [&](HttpRequest& req, HttpResponse& res) {
        requireAny(sys, req); long long id = req.paramInt("id");
        Json p = db.one("SELECT * FROM pharmacies WHERE pharmacy_id=?", {id}); if (p.isNull()) throw ApiError(404, "Pharmacy not found");
        p["delivery_available"] = p.at("delivery_available").integer() == 1;
        p["distinct_medicines"] = db.scalar("SELECT COUNT(*) FROM pharmacy_inventory WHERE pharmacy_id=? AND quantity>0", {id});
        p["total_units"] = db.scalar("SELECT COALESCE(SUM(quantity),0) FROM pharmacy_inventory WHERE pharmacy_id=?", {id}); res.send(200, p);
    });
    r.get("/api/pharmacies/:id/inventory", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"}); long long id = req.paramInt("id");
        if (db.one("SELECT 1 FROM pharmacies WHERE pharmacy_id=?", {id}).isNull()) throw ApiError(404, "Pharmacy not found");
        res.send(200, inventoryQuery(sys, a, req, id));
    });

    r.post("/api/pharmacies", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); Json b = req.json(); vector<pair<string, Val>> f; pharmacyFields(b, false, f);
        string cols, marks; vector<Val> vals; for (auto& kv : f) { cols += (cols.empty() ? "" : ",") + kv.first; marks += (marks.empty() ? "?" : ",?"); vals.push_back(kv.second); }
        if (!db.one("SELECT 1 FROM pharmacies WHERE name=?", {reqStr(b, "name")}).isNull()) throw ApiError(409, "A pharmacy with this name already exists");
        db.run("INSERT INTO pharmacies(" + cols + ",is_demo) VALUES(" + marks + ",0)", vals); long long id = db.lastId();
        db.audit(a, "PHARMACY_CREATED", "pharmacy", to_string(id)); sys.reloadStock(); Json j = message("Pharmacy created"); j["pharmacy_id"] = id; res.send(201, j);
    });
    r.put("/api/pharmacies/:id", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); long long id = req.paramInt("id"); Json b = req.json(); vector<pair<string, Val>> f; pharmacyFields(b, true, f);
        if (f.empty()) throw ApiError(422, "No updatable fields supplied");
        string set; vector<Val> vals; for (auto& kv : f) { set += (set.empty() ? "" : ",") + kv.first + "=?"; vals.push_back(kv.second); } vals.push_back(id);
        if (db.run("UPDATE pharmacies SET " + set + " WHERE pharmacy_id=?", vals) == 0) throw ApiError(404, "Pharmacy not found");
        db.audit(a, "PHARMACY_UPDATED", "pharmacy", to_string(id)); sys.reloadStock(); res.send(200, message("Pharmacy updated"));
    });
    r.del("/api/pharmacies/:id", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); long long id = req.paramInt("id");
        if (db.one("SELECT 1 FROM pharmacies WHERE pharmacy_id=?", {id}).isNull()) throw ApiError(404, "Pharmacy not found");
        if (db.scalar("SELECT (SELECT COUNT(*) FROM pharmacy_inventory WHERE pharmacy_id=?1)+(SELECT COUNT(*) FROM distribution_transactions WHERE pharmacy_id=?1)+(SELECT COUNT(*) FROM stock_movements WHERE pharmacy_id=?1)", {id}) > 0)
            throw ApiError(409, "Pharmacy has stock or history and cannot be deleted. Set its status to Suspended instead.");
        db.run("UPDATE users SET pharmacy_id=NULL WHERE pharmacy_id=?", {id}); db.run("DELETE FROM pharmacies WHERE pharmacy_id=?", {id});
        db.audit(a, "PHARMACY_DELETED", "pharmacy", to_string(id)); sys.reloadStock(); res.send(200, message("Pharmacy deleted"));
    });

    // ----- alternatives -----
    r.get("/api/medicines/:id/alternatives", [&](HttpRequest& req, HttpResponse& res) { requireAny(sys, req); res.send(200, sys.alternativesFor(req.paramInt("id"))); });
    r.get("/api/alternatives/compare", [&](HttpRequest& req, HttpResponse& res) {
        requireAny(sys, req); Json j = Json::object(); Json arr = Json::array();
        for (const char* k : {"a", "b"}) {
            long long id = queryInt(req, k, 0, 0, 1000000000); if (!id) throw ApiError(400, "Query parameters a and b (medicine ids) are required");
            Json m = db.one("SELECT m.medicine_id,m.name,m.generic_name,m.brand_name,m.composition,m.strength,m.dosage_form,m.requires_prescription rx,m.unit_price,mf.name manufacturer,c.name category "
                            "FROM medicines m JOIN manufacturers mf ON mf.manufacturer_id=m.manufacturer_id JOIN medicine_categories c ON c.category_id=m.category_id WHERE m.medicine_id=?", {id});
            if (m.isNull()) throw ApiError(404, "Medicine not found");
            m["requires_prescription"] = m.at("rx").integer() == 1;
            m["available_units"] = db.scalar("SELECT COALESCE(SUM(quantity),0) FROM medicine_batches WHERE medicine_id=? AND expiry_date>=date('now')", {id}); arr.push_back(m);
        }
        const Json &x = arr.items()[0], &y = arr.items()[1];
        Json cmp = Json::object(); cmp["same_generic"] = x.at("generic_name").str() == y.at("generic_name").str(); cmp["same_composition"] = x.at("composition").str() == y.at("composition").str();
        cmp["same_strength"] = x.at("strength").str() == y.at("strength").str(); cmp["same_form"] = x.at("dosage_form").str() == y.at("dosage_form").str();
        cmp["same_prescription_requirement"] = x.at("rx").integer() == y.at("rx").integer();
        j["medicines"] = arr; j["comparison"] = cmp; j["disclaimer"] = "Comparison is informational. Substitution requires pharmacist approval."; res.send(200, j);
    });
    r.post("/api/requests/:id/substitution", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"}); Json b = req.json();
        sys.proposeSubstitution(req.paramInt("id"), reqInt(b, "alternative_id", 1, 1000000000), a); res.send(200, message("Alternative proposed; a pharmacist must approve it"));
    });
    r.post("/api/requests/:id/substitution/decision", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Pharmacist"}); Json b = req.json();
        if (!b.at("approve").isBool()) throw ApiError(422, "'approve' (true/false) is required");
        sys.decideSubstitution(req.paramInt("id"), b.at("approve").boolean(), optStr(b, "note", 300), a);
        res.send(200, message(b.at("approve").boolean() ? "Alternative approved" : "Alternative rejected"));
    });
}
