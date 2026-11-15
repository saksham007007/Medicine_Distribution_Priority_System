// ============================================================
//                    NIKITA - SERVICES & API
//     patients, medicine requests, prescriptions, priority queue
// ============================================================

// Validates patient fields; returns column -> value for the provided keys.
static vector<pair<string, Val>> patientFields(const Json& d, bool partial) {
    vector<pair<string, Val>> f;
    if (!partial || d.has("name")) f.push_back({"name", Val(reqStr(d, "name", 2, 100))});
    if (d.has("age")) f.push_back({"age", Val(reqInt(d, "age", 0, 120))});
    if (d.has("gender")) { string g = optStr(d, "gender", 10); if (g != "Male" && g != "Female" && g != "Other") throw ApiError(422, "gender must be Male, Female or Other"); f.push_back({"gender", Val(g)}); }
    if (d.has("contact")) { string c = optStr(d, "contact", 25); if (!util::validPhone(c)) throw ApiError(422, "Invalid contact number"); f.push_back({"contact", Val(c)}); }
    if (d.has("email")) { string e = optStr(d, "email", 120); if (!util::validEmail(e)) throw ApiError(422, "Invalid email address"); f.push_back({"email", Val(e)}); }
    if (d.has("address")) f.push_back({"address", Val(optStr(d, "address", 300))});
    if (d.has("city")) f.push_back({"city", Val(optStr(d, "city", 80))});
    if (d.has("latitude")) { double v = d.at("latitude").num(1000); if (v < -90 || v > 90) throw ApiError(422, "latitude out of range"); f.push_back({"latitude", Val(v)}); }
    if (d.has("longitude")) { double v = d.at("longitude").num(1000); if (v < -180 || v > 180) throw ApiError(422, "longitude out of range"); f.push_back({"longitude", Val(v)}); }
    return f;
}

long long MedicineSystem::createPatient(const Json& d) {
    auto f = patientFields(d, false);
    string cols, marks; vector<Val> vals;
    for (auto& kv : f) { cols += (cols.empty() ? "" : ",") + kv.first; marks += (marks.empty() ? "?" : ",?"); vals.push_back(kv.second); }
    database.run("INSERT INTO patients(" + cols + ") VALUES(" + marks + ")", vals);
    return database.lastId();
}

void MedicineSystem::updatePatient(long long id, const Json& d) {
    auto f = patientFields(d, true);
    if (f.empty()) throw ApiError(422, "No updatable fields supplied");
    string set; vector<Val> vals;
    for (auto& kv : f) { set += (set.empty() ? "" : ",") + kv.first + "=?"; vals.push_back(kv.second); }
    vals.push_back(id);
    if (database.run("UPDATE patients SET " + set + " WHERE patient_id=? AND active=1", vals) == 0) throw ApiError(404, "Patient not found");
}

// Creates a request and places it in the priority queue (status Pending).
Json MedicineSystem::createRequest(long long patientId, int medicineId, int quantity, const string& urgency,
                                   long long prescriptionId, const string& notes, const Auth& by) {
    if (Patient::priorityOf(urgency) == 0) throw ApiError(422, "urgency must be Critical, High, Medium or Normal");
    if (quantity < 1 || quantity > 1000) throw ApiError(422, "quantity must be between 1 and 1000");
    Json pt = database.one("SELECT patient_id,name FROM patients WHERE patient_id=? AND active=1", {patientId});
    if (pt.isNull()) throw ApiError(404, "Patient not found");
    Json med = database.one("SELECT medicine_id,name,requires_prescription rx,status FROM medicines WHERE medicine_id=?", {medicineId});
    if (med.isNull()) throw ApiError(404, "Medicine not found");
    if (med.at("status").str() != "Active") throw ApiError(409, "This medicine is discontinued");
    if (!database.one("SELECT 1 FROM medicine_requests WHERE patient_id=? AND medicine_id=? AND status IN('Pending','Waiting')", {patientId, medicineId}).isNull())
        throw ApiError(409, "This patient already has an open request for this medicine");
    if (med.at("rx").integer() == 1) {
        if (prescriptionId <= 0) throw ApiError(422, "This medicine requires a prescription (prescription_id)");
        Json rx = database.one("SELECT patient_id,medicine_id FROM prescriptions WHERE prescription_id=?", {prescriptionId});
        if (rx.isNull() || rx.at("patient_id").integer() != patientId || rx.at("medicine_id").integer() != medicineId)
            throw ApiError(422, "Prescription does not match this patient and medicine");
    }
    Tx tx(database);
    database.run("INSERT INTO medicine_requests(patient_id,medicine_id,quantity,urgency,priority,prescription_id,notes) VALUES(?,?,?,?,?,?,?)",
                 {patientId, medicineId, quantity, urgency, Patient::priorityOf(urgency), prescriptionId > 0 ? Val(prescriptionId) : Val(), notes.empty() ? Val() : Val(notes)});
    long long rid = database.lastId();
    database.setRequestStatus(rid, "Pending", "Queued");
    database.run("INSERT INTO priority_history(request_id,new_urgency,changed_by,reason) VALUES(?,?,?,?)",
                 {rid, urgency, actorVal(by), "Initial triage"});
    database.audit(by, "REQUEST_CREATED", "request", to_string(rid), med.at("name").str() + " x" + to_string(quantity) + " [" + urgency + "]");
    tx.commit();
    reloadQueue();
    Json out = Json::object(); out["request_id"] = rid; out["status"] = "Pending"; out["urgency"] = urgency;
    int pos = 0, i = 1; for (auto& p : orderedQueue()) { if (p.requestId == rid) pos = i; i++; }
    out["queue_position"] = pos; out["queue_length"] = (long long)pq.size();
    auto avail = maxAvailability(database); auto av = avail.find(medicineId);
    out["in_stock_now"] = av != avail.end() && av->second >= quantity;
    out["requires_prescription"] = med.at("rx").integer() == 1;
    return out;
}

void MedicineSystem::changeUrgency(long long requestId, const string& urgency, const string& reason, const Auth& by) {
    if (!by.staff()) throw ApiError(403, "Only staff can change urgency");
    if (Patient::priorityOf(urgency) == 0) throw ApiError(422, "urgency must be Critical, High, Medium or Normal");
    if (reason.size() < 5) throw ApiError(422, "A reason (at least 5 characters) is required for every priority change");
    Json r = database.one("SELECT urgency,status,patient_id FROM medicine_requests WHERE request_id=?", {requestId});
    if (r.isNull()) throw ApiError(404, "Request not found");
    if (r.at("status").str() != "Pending" && r.at("status").str() != "Waiting") throw ApiError(409, "Only Pending or Waiting requests can be re-prioritised");
    if (r.at("urgency").str() == urgency) throw ApiError(409, "Request already has this urgency");
    Tx tx(database);
    database.run("UPDATE medicine_requests SET urgency=?,priority=?,updated_at=datetime('now') WHERE request_id=?", {urgency, Patient::priorityOf(urgency), requestId});
    database.run("UPDATE patient_priority_queue SET priority=?,updated_at=datetime('now') WHERE request_id=?", {Patient::priorityOf(urgency), requestId});
    database.run("INSERT INTO priority_history(request_id,old_urgency,new_urgency,changed_by,reason) VALUES(?,?,?,?,?)",
                 {requestId, r.at("urgency").str(), urgency, actorVal(by), reason});
    database.notify(0, r.at("patient_id").integer(), "", "Priority", "Request priority updated",
                    "Your request #" + to_string(requestId) + " is now marked " + urgency + ".");
    database.audit(by, "PRIORITY_CHANGED", "request", to_string(requestId), r.at("urgency").str() + " -> " + urgency + ": " + reason);
    tx.commit();
    reloadQueue();
}

void MedicineSystem::cancelRequest(long long requestId, const Auth& by) {
    Json r = database.one("SELECT patient_id,status FROM medicine_requests WHERE request_id=?", {requestId});
    if (r.isNull()) throw ApiError(404, "Request not found");
    if (by.is("Patient") && r.at("patient_id").integer() != by.patientId) throw ApiError(403, "You can only cancel your own requests");
    if (r.at("status").str() != "Pending" && r.at("status").str() != "Waiting") throw ApiError(409, "Only Pending or Waiting requests can be cancelled");
    Tx tx(database);
    database.setRequestStatus(requestId, "Cancelled", "Removed");
    database.run("UPDATE waiting_list SET status='Cancelled',completed_at=datetime('now') WHERE request_id=? AND status='Waiting'", {requestId});
    database.audit(by, "REQUEST_CANCELLED", "request", to_string(requestId));
    tx.commit();
    reloadQueue();
}

Json MedicineSystem::queueSnapshot(long long onlyPatientId) {
    reloadQueue();
    auto avail = maxAvailability(database);
    Json queue = Json::array(); int pos = 0;
    for (const auto& p : orderedQueue()) {
        pos++;
        if (onlyPatientId && p.id != onlyPatientId) continue;
        Json j = p.toJson(); j["queue_position"] = pos; j["estimated_order"] = pos;
        auto it = avail.find(p.medicineId); j["in_stock_now"] = it != avail.end() && it->second >= p.quantity;
        queue.push_back(j);
    }
    Json waiting = Json::array();
    vector<Patient> w = waitingList; sort(w.begin(), w.end(), Compare());
    reverse(w.begin(), w.end());       // Compare is a max-heap comparator; reverse gives highest priority first
    for (const auto& p : w) { if (onlyPatientId && p.id != onlyPatientId) continue; Json j = p.toJson(); waiting.push_back(j); }
    Json out = Json::object(); out["queue"] = queue; out["waiting"] = waiting;
    out["queue_length"] = (long long)pq.size(); out["waiting_length"] = (long long)waitingList.size();
    return out;
}

// Reusable SQL for request rows (joins patient + medicine names).
static const char* REQUEST_SELECT =
    "SELECT r.request_id,r.patient_id,pt.name patient_name,r.medicine_id,m.name medicine,m.generic_name,m.requires_prescription rx,r.quantity,r.urgency,r.priority,"
    "r.status,r.prescription_id,r.substitute_medicine_id,sm.name substitute_medicine,r.substitution_status,r.fulfilled_pharmacy_id,r.notes,r.requested_at,r.updated_at,"
    "CAST((strftime('%s','now')-strftime('%s',r.requested_at))/60 AS INTEGER) waiting_minutes "
    "FROM medicine_requests r JOIN patients pt ON pt.patient_id=r.patient_id JOIN medicines m ON m.medicine_id=r.medicine_id "
    "LEFT JOIN medicines sm ON sm.medicine_id=r.substitute_medicine_id ";

static void requireOwnPatient(const Auth& a, long long patientId) {
    if (a.is("Patient") && a.patientId != patientId) throw ApiError(403, "You can only access your own records");
}

inline void registerPatientRoutes(Router& r, MedicineSystem& sys) {
    Database& db = sys.database;

    // ----- patients -----
    r.get("/api/patients", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin", "Pharmacist"});
        string q = req.q("q"), city = req.q("city"); int active = req.q("active", "1") == "0" ? 0 : 1;
        long long limit = queryInt(req, "limit", 50, 1, 200), offset = queryInt(req, "offset", 0, 0, 1000000);
        string pat = q.empty() ? string("%") : likePattern(q);
        string where = "WHERE active=?1 AND (?2='%' OR name LIKE ?2 ESCAPE '\\' OR contact LIKE ?2 ESCAPE '\\' OR email LIKE ?2 ESCAPE '\\') AND (?3='' OR city=?3 COLLATE NOCASE)";
        Json rows = db.query("SELECT patient_id,name,age,gender,contact,email,address,city,active,created_at,"
                             "(SELECT COUNT(*) FROM medicine_requests r WHERE r.patient_id=patients.patient_id AND r.status IN('Pending','Waiting')) open_requests "
                             "FROM patients " + where + " ORDER BY name LIMIT ?4 OFFSET ?5", {active, pat, city, limit, offset});
        Json j = Json::object(); j["items"] = rows;
        j["total"] = db.scalar("SELECT COUNT(*) FROM patients " + where, {active, pat, city}); res.send(200, j);
    });
    r.post("/api/patients", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"});
        long long id = sys.createPatient(req.json()); db.audit(a, "PATIENT_CREATED", "patient", to_string(id));
        Json j = message("Patient created"); j["patient_id"] = id; res.send(201, j);
    });
    r.get("/api/patients/:id", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); long long id = req.paramInt("id"); requireOwnPatient(a, id);
        Json p = db.one("SELECT * FROM patients WHERE patient_id=?", {id}); if (p.isNull()) throw ApiError(404, "Patient not found"); res.send(200, p);
    });
    r.put("/api/patients/:id", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); long long id = req.paramInt("id"); requireOwnPatient(a, id);
        sys.updatePatient(id, req.json()); db.audit(a, "PATIENT_UPDATED", "patient", to_string(id)); res.send(200, message("Patient updated"));
    });
    r.del("/api/patients/:id", [&](HttpRequest& req, HttpResponse& res) {          // soft delete: history is retained
        Auth a = require(sys, req, {"Admin"}); long long id = req.paramInt("id");
        if (db.scalar("SELECT COUNT(*) FROM medicine_requests WHERE patient_id=? AND status IN('Pending','Waiting')", {id}) > 0)
            throw ApiError(409, "Patient has open requests; cancel them first");
        if (db.run("UPDATE patients SET active=0 WHERE patient_id=? AND active=1", {id}) == 0) throw ApiError(404, "Patient not found");
        db.run("UPDATE users SET active=0 WHERE patient_id=?", {id});
        db.audit(a, "PATIENT_DEACTIVATED", "patient", to_string(id)); res.send(200, message("Patient deactivated (history retained)"));
    });
    r.get("/api/patients/:id/history", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); long long id = req.paramInt("id"); requireOwnPatient(a, id);
        Json p = db.one("SELECT patient_id,name,age,city FROM patients WHERE patient_id=?", {id}); if (p.isNull()) throw ApiError(404, "Patient not found");
        Json j = Json::object(); j["patient"] = p;
        j["requests"] = db.query(string(REQUEST_SELECT) + "WHERE r.patient_id=? ORDER BY r.request_id DESC", {id});
        j["transactions"] = db.query("SELECT t.transaction_id,t.request_id,t.medicine_name,t.quantity,t.status,t.created_at,ph.name pharmacy FROM distribution_transactions t "
                                     "LEFT JOIN pharmacies ph ON ph.pharmacy_id=t.pharmacy_id WHERE t.patient_id=? ORDER BY t.transaction_id DESC", {id});
        j["prescriptions"] = db.query("SELECT x.prescription_id,x.medicine_id,m.name medicine,x.quantity,x.doctor_name,x.issued_date,x.verified FROM prescriptions x "
                                      "JOIN medicines m ON m.medicine_id=x.medicine_id WHERE x.patient_id=? ORDER BY x.prescription_id DESC", {id});
        res.send(200, j);
    });

    // ----- prescriptions -----
    r.get("/api/prescriptions", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); long long pid = a.is("Patient") ? a.patientId : queryInt(req, "patient_id", 0, 0, 1000000000);
        string v = req.q("verified");
        Json rows = db.query("SELECT x.prescription_id,x.patient_id,pt.name patient_name,x.medicine_id,m.name medicine,x.quantity,x.doctor_name,x.doctor_reg_no,x.issued_date,x.notes,x.verified,x.verified_at "
                             "FROM prescriptions x JOIN patients pt ON pt.patient_id=x.patient_id JOIN medicines m ON m.medicine_id=x.medicine_id "
                             "WHERE (?1=0 OR x.patient_id=?1) AND (?2='' OR x.verified=?2) ORDER BY x.prescription_id DESC LIMIT 300", {pid, v == "1" ? string("1") : (v == "0" ? string("0") : string(""))});
        Json j = Json::object(); j["items"] = rows; res.send(200, j);
    });
    r.post("/api/prescriptions", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); Json b = req.json();
        long long pid = a.is("Patient") ? a.patientId : reqInt(b, "patient_id", 1, 1000000000);
        if (db.one("SELECT 1 FROM patients WHERE patient_id=? AND active=1", {pid}).isNull()) throw ApiError(404, "Patient not found");
        long long mid = reqInt(b, "medicine_id", 1, 1000000000);
        if (db.one("SELECT 1 FROM medicines WHERE medicine_id=?", {mid}).isNull()) throw ApiError(404, "Medicine not found");
        string issued = reqStr(b, "issued_date", 10, 10);
        if (!util::validDate(issued) || issued > util::today()) throw ApiError(422, "issued_date must be a valid past or current date (YYYY-MM-DD)");
        if (issued < util::addDays(util::today(), -180)) throw ApiError(422, "Prescription is older than 180 days");
        db.run("INSERT INTO prescriptions(patient_id,medicine_id,quantity,doctor_name,doctor_reg_no,issued_date,notes) VALUES(?,?,?,?,?,?,?)",
               {pid, mid, reqInt(b, "quantity", 1, 1000), reqStr(b, "doctor_name", 3, 100), optStr(b, "doctor_reg_no", 40), issued, optStr(b, "notes", 300)});
        long long id = db.lastId(); db.audit(a, "PRESCRIPTION_ADDED", "prescription", to_string(id));
        Json j = message("Prescription recorded; a pharmacist must verify it before distribution"); j["prescription_id"] = id; res.send(201, j);
    });
    r.post("/api/prescriptions/:id/verify", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Pharmacist"}); long long id = req.paramInt("id");
        if (db.run("UPDATE prescriptions SET verified=1,verified_by=?,verified_at=datetime('now') WHERE prescription_id=? AND verified=0", {a.userId, id}) == 0)
            throw ApiError(404, "Prescription not found or already verified");
        db.audit(a, "PRESCRIPTION_VERIFIED", "prescription", to_string(id)); res.send(200, message("Prescription verified"));
    });

    // ----- requests -----
    r.get("/api/requests", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req);
        long long pid = a.is("Patient") ? a.patientId : queryInt(req, "patient_id", 0, 0, 1000000000);
        string status = req.q("status"), urgency = req.q("urgency"), q = req.q("q");
        long long mid = queryInt(req, "medicine_id", 0, 0, 1000000000), limit = queryInt(req, "limit", 50, 1, 200), offset = queryInt(req, "offset", 0, 0, 1000000);
        string pat = q.empty() ? string("%") : likePattern(q);
        string where = "WHERE (?1=0 OR r.patient_id=?1) AND (?2='' OR r.status=?2) AND (?3='' OR r.urgency=?3) AND (?4=0 OR r.medicine_id=?4) AND (?5='%' OR pt.name LIKE ?5 ESCAPE '\\' OR m.name LIKE ?5 ESCAPE '\\') ";
        Json rows = db.query(string(REQUEST_SELECT) + where + "ORDER BY r.request_id DESC LIMIT ?6 OFFSET ?7", {pid, status, urgency, mid, pat, limit, offset});
        Json j = Json::object(); j["items"] = rows;
        j["total"] = db.scalar("SELECT COUNT(*) FROM medicine_requests r JOIN patients pt ON pt.patient_id=r.patient_id JOIN medicines m ON m.medicine_id=r.medicine_id " + where, {pid, status, urgency, mid, pat});
        res.send(200, j);
    });
    r.get("/api/requests/:id", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); long long id = req.paramInt("id");
        Json rq = db.one(string(REQUEST_SELECT) + "WHERE r.request_id=?", {id}); if (rq.isNull()) throw ApiError(404, "Request not found");
        requireOwnPatient(a, rq.at("patient_id").integer());
        rq["priority_history"] = db.query("SELECT h.old_urgency,h.new_urgency,h.reason,h.changed_at,u.username changed_by FROM priority_history h LEFT JOIN users u ON u.user_id=h.changed_by WHERE h.request_id=? ORDER BY h.history_id", {id});
        rq["transactions"] = db.query("SELECT transaction_id,status,quantity,pharmacy_id,created_at,notes FROM distribution_transactions WHERE request_id=? ORDER BY transaction_id", {id});
        if (rq.at("status").str() == "Pending") { int pos = 0, i = 1; for (auto& p : sys.orderedQueue()) { if (p.requestId == id) pos = i; i++; } rq["queue_position"] = pos; }
        res.send(200, rq);
    });
    r.post("/api/requests", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); Json b = req.json();
        long long pid = a.is("Patient") ? a.patientId : reqInt(b, "patient_id", 1, 1000000000);
        if (a.is("Patient") && pid == 0) throw ApiError(403, "Your account is not linked to a patient record");
        // Patients cannot choose their own priority: self-service requests enter as Normal until staff triage.
        string urgency = a.is("Patient") ? "Normal" : (b.has("urgency") ? reqStr(b, "urgency", 3, 10) : "Normal");
        long long rx = b.has("prescription_id") ? reqInt(b, "prescription_id", 1, 1000000000) : 0;
        Json out = sys.createRequest(pid, (int)reqInt(b, "medicine_id", 1, 1000000000), (int)reqInt(b, "quantity", 1, 1000), urgency, rx, optStr(b, "notes", 300), a);
        res.send(201, out);
    });
    r.put("/api/requests/:id/urgency", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"}); long long id = req.paramInt("id"); Json b = req.json();
        sys.changeUrgency(id, reqStr(b, "urgency", 3, 10), optStr(b, "reason", 300), a); res.send(200, message("Urgency updated and recorded"));
    });
    r.post("/api/requests/:id/cancel", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); sys.cancelRequest(req.paramInt("id"), a); res.send(200, message("Request cancelled"));
    });

    // ----- priority queue -----
    r.get("/api/queue", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = requireAny(sys, req); res.send(200, sys.queueSnapshot(a.is("Patient") ? a.patientId : 0));
    });
}
