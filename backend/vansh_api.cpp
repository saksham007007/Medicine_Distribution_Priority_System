// ============================================================
//                    VANSH - SERVICES & API
//            medicine catalogue, inventory, batches, restocking
// ============================================================

// Catalogue search/filter/sort over the in-memory hash map (std::sort + pagination).
Json MedicineSystem::listMedicines(const HttpRequest& req) {
    string q = util::lower(req.q("q")), generic = util::lower(req.q("generic")), manuf = util::lower(req.q("manufacturer"));
    string cat = util::lower(req.q("category")), form = util::lower(req.q("form")), rx = req.q("rx"), status = req.q("status", "Active");
    bool onlyAvail = req.q("available") == "1", onlyLow = req.q("low_stock") == "1";
    vector<const Medicine*> v;
    for (const auto& kv : inventory) {
        const Medicine& m = kv.second;
        if (status != "all" && m.status != status) continue;
        if (!q.empty() && util::lower(m.name).find(q) == string::npos && util::lower(m.genericName).find(q) == string::npos &&
            util::lower(m.brand).find(q) == string::npos && util::lower(m.manufacturer).find(q) == string::npos) continue;
        if (!generic.empty() && util::lower(m.genericName).find(generic) == string::npos) continue;
        if (!manuf.empty() && util::lower(m.manufacturer).find(manuf) == string::npos) continue;
        if (!cat.empty() && util::lower(m.category) != cat) continue;
        if (!form.empty() && util::lower(m.dosageForm) != form) continue;
        if (rx == "1" && !m.requiresPrescription) continue;
        if (rx == "0" && m.requiresPrescription) continue;
        if (onlyAvail && m.stock <= 0) continue;
        if (onlyLow && !m.isLowStock()) continue;
        v.push_back(&m);
    }
    string sortBy = req.q("sort", "name"); bool desc = req.q("order") == "desc";
    if (sortBy != "name" && sortBy != "price" && sortBy != "availability" && sortBy != "generic") throw ApiError(400, "sort must be name, price, availability or generic");
    auto ascending = [&](const Medicine* a, const Medicine* b) {
        if (sortBy == "price") return a->price != b->price ? a->price < b->price : a->name < b->name;
        if (sortBy == "availability") return a->stock != b->stock ? a->stock < b->stock : a->name < b->name;
        if (sortBy == "generic") return a->genericName != b->genericName ? a->genericName < b->genericName : a->name < b->name;
        return a->name < b->name;
    };
    sort(v.begin(), v.end(), [&](const Medicine* a, const Medicine* b) { return desc ? ascending(b, a) : ascending(a, b); });
    long long limit = queryInt(req, "limit", 25, 1, 200), page = queryInt(req, "page", 1, 1, 100000);
    Json items = Json::array();
    for (size_t i = (size_t)((page - 1) * limit); i < v.size() && i < (size_t)(page * limit); i++) items.push_back(v[i]->toJson());
    Json out = Json::object(); out["total"] = (long long)v.size(); out["page"] = page; out["limit"] = limit; out["items"] = items;
    return out;
}

Json MedicineSystem::restock(long long pharmacyId, long long medicineId, int qty, const string& batch,
                             const string& mfg, const string& exp, const Auth& by) {
    if (qty < 1 || qty > 100000) throw ApiError(422, "quantity must be between 1 and 100000");
    if (batch.size() < 2 || batch.size() > 40) throw ApiError(422, "batch_number must be 2-40 characters");
    if (!util::validDate(mfg) || !util::validDate(exp)) throw ApiError(422, "Dates must be valid YYYY-MM-DD values");
    if (mfg > util::today()) throw ApiError(422, "manufacturing_date cannot be in the future");
    if (exp <= mfg) throw ApiError(422, "expiry_date must be after manufacturing_date");
    if (exp < util::today()) throw ApiError(422, "This batch is already expired and cannot be added to stock");
    requirePharmacyAccess(by, pharmacyId);
    Json ph = database.one("SELECT status FROM pharmacies WHERE pharmacy_id=?", {pharmacyId}); if (ph.isNull()) throw ApiError(404, "Pharmacy not found");
    Json md = database.one("SELECT min_stock,status,name FROM medicines WHERE medicine_id=?", {medicineId}); if (md.isNull()) throw ApiError(404, "Medicine not found");
    if (md.at("status").str() != "Active") throw ApiError(409, "Medicine is discontinued");
    Tx tx(database);
    database.run("INSERT INTO pharmacy_inventory(pharmacy_id,medicine_id,quantity,reorder_level) VALUES(?,?,0,?) ON CONFLICT(pharmacy_id,medicine_id) DO NOTHING",
                 {pharmacyId, medicineId, atoll(database.setting("default_reorder_level", "20").c_str())});
    Json existing = database.one("SELECT manufacturing_date,expiry_date FROM medicine_batches WHERE pharmacy_id=? AND medicine_id=? AND batch_number=?", {pharmacyId, medicineId, batch});
    if (!existing.isNull()) {
        if (existing.at("manufacturing_date").str() != mfg || existing.at("expiry_date").str() != exp) throw ApiError(409, "Batch number already exists with different dates");
        database.run("UPDATE medicine_batches SET quantity=quantity+? WHERE pharmacy_id=? AND medicine_id=? AND batch_number=?", {qty, pharmacyId, medicineId, batch});
    } else {
        database.run("INSERT INTO medicine_batches(pharmacy_id,medicine_id,batch_number,manufacturing_date,expiry_date,quantity) VALUES(?,?,?,?,?,?)", {pharmacyId, medicineId, batch, mfg, exp, qty});
    }
    database.run("UPDATE pharmacy_inventory SET quantity=quantity+?,last_updated=datetime('now') WHERE pharmacy_id=? AND medicine_id=?", {qty, pharmacyId, medicineId});
    database.run("INSERT INTO stock_movements(pharmacy_id,medicine_id,change_qty,reason,reference,performed_by) VALUES(?,?,?,'Restock',?,?)", {pharmacyId, medicineId, qty, "BATCH " + batch, actorVal(by)});
    database.run("INSERT INTO restocking_history(pharmacy_id,medicine_id,batch_number,quantity,performed_by) VALUES(?,?,?,?,?)", {pharmacyId, medicineId, batch, qty, actorVal(by)});
    database.audit(by, "RESTOCK", "inventory", to_string(pharmacyId) + "/" + to_string(medicineId), md.at("name").str() + " +" + to_string(qty) + " batch " + batch);
    tx.commit();
    reloadStock();
    int promoted = promoteWaiting();        // waiting-list requests that can now be served return to the priority queue
    Json out = Json::object(); out["message"] = "Stock updated"; out["promoted_from_waiting_list"] = promoted;
    out["new_quantity"] = database.scalar("SELECT quantity FROM pharmacy_inventory WHERE pharmacy_id=? AND medicine_id=?", {pharmacyId, medicineId});
    return out;
}

long long MedicineSystem::writeOffExpired(long long pharmacyId, const Auth& by) {
    requirePharmacyAccess(by, pharmacyId);
    if (database.one("SELECT 1 FROM pharmacies WHERE pharmacy_id=?", {pharmacyId}).isNull()) throw ApiError(404, "Pharmacy not found");
    Tx tx(database);
    Json rows = database.query("SELECT medicine_id,SUM(quantity) q FROM medicine_batches WHERE pharmacy_id=? AND expiry_date<date('now') AND quantity>0 GROUP BY medicine_id", {pharmacyId});
    long long total = 0;
    for (const auto& r : rows.items()) {
        long long mid = r.at("medicine_id").integer(), q = r.at("q").integer(); total += q;
        database.run("UPDATE pharmacy_inventory SET quantity=quantity-?,last_updated=datetime('now') WHERE pharmacy_id=? AND medicine_id=?", {q, pharmacyId, mid});
        database.run("INSERT INTO stock_movements(pharmacy_id,medicine_id,change_qty,reason,reference,performed_by) VALUES(?,?,?,'Expired write-off','Expired batches',?)", {pharmacyId, mid, -q, actorVal(by)});
    }
    database.run("UPDATE medicine_batches SET quantity=0 WHERE pharmacy_id=? AND expiry_date<date('now') AND quantity>0", {pharmacyId});
    database.audit(by, "EXPIRED_WRITE_OFF", "inventory", to_string(pharmacyId), "units=" + to_string(total));
    tx.commit();
    reloadStock();
    return total;
}

// Shared inventory query (also used by the pharmacy module).
static Json inventoryQuery(MedicineSystem& sys, const Auth& a, const HttpRequest& req, long long forcePharmacy = 0) {
    Database& db = sys.database;
    long long pharmacy = forcePharmacy ? forcePharmacy : queryInt(req, "pharmacy_id", 0, 0, 1000000000);
    if (a.is("Pharmacist") && a.pharmacyId > 0 && !forcePharmacy) pharmacy = a.pharmacyId;
    long long medicine = queryInt(req, "medicine_id", 0, 0, 1000000000), limit = queryInt(req, "limit", 50, 1, 500), offset = queryInt(req, "offset", 0, 0, 10000000);
    string q = req.q("q"), status = req.q("status"), pat = q.empty() ? string("%") : likePattern(q);
   string nearDays = "+" + db.setting("near_expiry_days", "60") + " days";
    string having;
    if (status == "low") having = "HAVING available<=i.reorder_level AND i.quantity>0";
    else if (status == "expired") having = "HAVING expired_qty>0";
    else if (status == "near") having = "HAVING near_qty>0";
    else if (status == "out") having = "HAVING available=0";
    else if (!status.empty() && status != "all") throw ApiError(400, "status must be low, expired, near, out or all");
    string from = "FROM pharmacy_inventory i JOIN pharmacies p ON p.pharmacy_id=i.pharmacy_id JOIN medicines m ON m.medicine_id=i.medicine_id "
                  "LEFT JOIN medicine_batches b ON b.pharmacy_id=i.pharmacy_id AND b.medicine_id=i.medicine_id "
                  "WHERE (?1=0 OR i.pharmacy_id=?1) AND (?2=0 OR i.medicine_id=?2) AND (?3='%' OR m.name LIKE ?3 ESCAPE '\\' OR m.generic_name LIKE ?3 ESCAPE '\\') "
                  "GROUP BY i.pharmacy_id,i.medicine_id ";
    string sel = "SELECT i.pharmacy_id,p.name pharmacy,i.medicine_id,m.name medicine,m.generic_name,m.dosage_form,i.quantity,i.reserved_quantity,i.reorder_level,i.last_updated,"
                 "COALESCE(SUM(CASE WHEN b.expiry_date>=date('now') THEN b.quantity END),0) available,"
                 "COALESCE(SUM(CASE WHEN b.expiry_date<date('now') THEN b.quantity END),0) expired_qty,"
                 "COALESCE(SUM(CASE WHEN b.expiry_date>=date('now') AND b.expiry_date<=date('now',?4) THEN b.quantity END),0) near_qty,"
                 "MIN(CASE WHEN b.expiry_date>=date('now') AND b.quantity>0 THEN b.expiry_date END) earliest_expiry ";
    Json rows = db.query(sel + from + having + " ORDER BY p.name,m.name LIMIT ?5 OFFSET ?6", {pharmacy, medicine, pat, nearDays, limit, offset});
    Json total = db.one("SELECT COUNT(*) c FROM (SELECT i.pharmacy_id,i.medicine_id,i.reorder_level,i.quantity,"
                        "COALESCE(SUM(CASE WHEN b.expiry_date>=date('now') THEN b.quantity END),0) available,"
                        "COALESCE(SUM(CASE WHEN b.expiry_date<date('now') THEN b.quantity END),0) expired_qty,"
                        "COALESCE(SUM(CASE WHEN b.expiry_date>=date('now') AND b.expiry_date<=date('now',?4) THEN b.quantity END),0) near_qty " + from + having + ")",
                        {pharmacy, medicine, pat, near});
    for (auto& r : rows.items()) {
        r["low_stock"] = r.at("available").integer() <= r.at("reorder_level").integer();
        r["status"] = r.at("available").integer() == 0 ? "Out of stock" : (r.at("low_stock").boolean() ? "Low stock" : "OK");
    }
    Json out = Json::object(); out["items"] = rows; out["total"] = total.at("c"); return out;
}

// Validates medicine fields (creating category/manufacturer rows on demand).
static void medicineFields(Database& db, const Json& b, bool partial, vector<pair<string, Val>>& f) {
        auto text = [&](const char* k, size_t lo, size_t hi) { if (!partial || b.has(k)) f.push_back({k, Val(reqStr(b, k, lo, hi))}); };
        text("name", 3, 120); text("generic_name", 2, 100); text("brand_name", 2, 80); text("composition", 2, 200);
        text("strength", 1, 40); text("dosage_form", 2, 40);
        if (b.has("description")) f.push_back({"description", Val(optStr(b, "description", 600))});
        if (b.has("storage_instructions")) f.push_back({"storage_instructions", Val(optStr(b, "storage_instructions", 300))});
        if (!partial || b.has("unit_price")) { double p = b.at("unit_price").num(-1); if (p < 0 || p > 1000000) throw ApiError(422, "unit_price must be between 0 and 1000000"); f.push_back({"unit_price", Val(p)}); }
        if (b.has("requires_prescription")) f.push_back({"requires_prescription", Val(b.at("requires_prescription").boolean() ? 1 : 0)});
        if (b.has("min_stock")) f.push_back({"min_stock", Val(reqInt(b, "min_stock", 0, 100000))});
        if (b.has("status")) { string s = reqStr(b, "status", 6, 12); if (s != "Active" && s != "Discontinued") throw ApiError(422, "status must be Active or Discontinued"); f.push_back({"status", Val(s)}); }
        if (!partial || b.has("category")) {
            string c = reqStr(b, "category", 2, 60); db.run("INSERT OR IGNORE INTO medicine_categories(name) VALUES(?)", {c});
            f.push_back({"category_id", Val(db.scalar("SELECT category_id FROM medicine_categories WHERE name=?", {c}))});
        }
        if (!partial || b.has("manufacturer")) {
            string m = reqStr(b, "manufacturer", 2, 100); db.run("INSERT OR IGNORE INTO manufacturers(name) VALUES(?)", {m});
            f.push_back({"manufacturer_id", Val(db.scalar("SELECT manufacturer_id FROM manufacturers WHERE name=?", {m}))});
        }
}

inline void registerMedicineRoutes(Router& r, MedicineSystem& sys) {
    Database& db = sys.database;

    // ----- catalogue -----
    r.get("/api/medicines", [&](HttpRequest& req, HttpResponse& res) { requireAny(sys, req); res.send(200, sys.listMedicines(req)); });
    r.get("/api/medicines/:id", [&](HttpRequest& req, HttpResponse& res) {
        requireAny(sys, req); long long id = req.paramInt("id");
        Json m = db.one("SELECT m.*,c.name category,mf.name manufacturer FROM medicines m JOIN medicine_categories c ON c.category_id=m.category_id "
                        "JOIN manufacturers mf ON mf.manufacturer_id=m.manufacturer_id WHERE m.medicine_id=?", {id});
        if (m.isNull()) throw ApiError(404, "Medicine not found");
        m["requires_prescription"] = m.at("requires_prescription").integer() == 1;
        m["availability"] = db.query("SELECT p.pharmacy_id,p.name pharmacy,p.city,p.status,p.phone,SUM(b.quantity) available,MIN(b.expiry_date) earliest_expiry "
                                     "FROM medicine_batches b JOIN pharmacies p ON p.pharmacy_id=b.pharmacy_id WHERE b.medicine_id=? AND b.expiry_date>=date('now') AND b.quantity>0 "
                                     "GROUP BY p.pharmacy_id ORDER BY available DESC", {id});
        long long total = 0; for (auto& a : m.at("availability").items()) total += a.at("available").integer();
        m["total_stock"] = total; res.send(200, m);
    });
    r.get("/api/categories", [&](HttpRequest& req, HttpResponse& res) {
        requireAny(sys, req); Json j = Json::object(); j["items"] = db.query("SELECT c.category_id,c.name,COUNT(m.medicine_id) medicines FROM medicine_categories c LEFT JOIN medicines m ON m.category_id=c.category_id GROUP BY c.category_id ORDER BY c.name"); res.send(200, j);
    });
    r.get("/api/manufacturers", [&](HttpRequest& req, HttpResponse& res) {
        requireAny(sys, req); Json j = Json::object(); j["items"] = db.query("SELECT f.manufacturer_id,f.name,f.city,COUNT(m.medicine_id) medicines FROM manufacturers f LEFT JOIN medicines m ON m.manufacturer_id=f.manufacturer_id GROUP BY f.manufacturer_id ORDER BY f.name"); res.send(200, j);
    });
    r.get("/api/dosage-forms", [&](HttpRequest& req, HttpResponse& res) {
        requireAny(sys, req); Json j = Json::object(); j["items"] = db.query("SELECT DISTINCT dosage_form name FROM medicines ORDER BY dosage_form"); res.send(200, j);
    });

    r.post("/api/medicines", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); Json b = req.json(); vector<pair<string, Val>> f; Tx tx(db); medicineFields(db, b, false, f);
        string cols, marks; vector<Val> vals; for (auto& kv : f) { cols += (cols.empty() ? "" : ",") + kv.first; marks += (marks.empty() ? "?" : ",?"); vals.push_back(kv.second); }
        cols += ",is_demo"; marks += ",0";
        db.run("INSERT INTO medicines(" + cols + ") VALUES(" + marks + ")", vals); long long id = db.lastId();
        db.audit(a, "MEDICINE_CREATED", "medicine", to_string(id)); tx.commit(); sys.reloadStock();
        Json j = message("Medicine created"); j["medicine_id"] = id; res.send(201, j);
    });
    r.put("/api/medicines/:id", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); long long id = req.paramInt("id"); Json b = req.json();
        if (db.one("SELECT 1 FROM medicines WHERE medicine_id=?", {id}).isNull()) throw ApiError(404, "Medicine not found");
        vector<pair<string, Val>> f; Tx tx(db); medicineFields(db, b, true, f);
        if (f.empty()) throw ApiError(422, "No updatable fields supplied");
        string set; vector<Val> vals; for (auto& kv : f) { set += (set.empty() ? "" : ",") + kv.first + "=?"; vals.push_back(kv.second); }
        vals.push_back(id); db.run("UPDATE medicines SET " + set + " WHERE medicine_id=?", vals);
        db.audit(a, "MEDICINE_UPDATED", "medicine", to_string(id)); tx.commit(); sys.reloadStock(); res.send(200, message("Medicine updated"));
    });
    r.del("/api/medicines/:id", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); long long id = req.paramInt("id");
        if (db.one("SELECT 1 FROM medicines WHERE medicine_id=?", {id}).isNull()) throw ApiError(404, "Medicine not found");
        bool referenced = db.scalar("SELECT (SELECT COUNT(*) FROM pharmacy_inventory WHERE medicine_id=?1)+(SELECT COUNT(*) FROM medicine_requests WHERE medicine_id=?1 OR substitute_medicine_id=?1)"
                                    "+(SELECT COUNT(*) FROM distribution_transactions WHERE medicine_id=?1)+(SELECT COUNT(*) FROM prescriptions WHERE medicine_id=?1)", {id}) > 0;
        Json j;
        if (referenced) {
            if (db.scalar("SELECT COUNT(*) FROM medicine_requests WHERE (medicine_id=?1 OR substitute_medicine_id=?1) AND status IN('Pending','Waiting')", {id}) > 0)
                throw ApiError(409, "Medicine has open requests; resolve them before discontinuing");
            db.run("UPDATE medicines SET status='Discontinued' WHERE medicine_id=?", {id}); j = message("Medicine is in use, so it was marked Discontinued instead of deleted");
        } else { db.run("DELETE FROM medicines WHERE medicine_id=?", {id}); j = message("Medicine deleted"); }
        db.audit(a, "MEDICINE_REMOVED", "medicine", to_string(id), referenced ? "discontinued" : "deleted"); sys.reloadStock(); res.send(200, j);
    });

    // ----- inventory -----
    r.get("/api/inventory", [&](HttpRequest& req, HttpResponse& res) { Auth a = require(sys, req, {"Admin", "Pharmacist"}); res.send(200, inventoryQuery(sys, a, req)); });
    r.get("/api/inventory/alerts", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"}); Json j = Json::object();
        for (const char* st : {"low", "expired", "near", "out"}) {
            HttpRequest sub = req; sub.query["status"] = st; sub.query["limit"] = "50"; sub.query["offset"] = "0";
            Json x = inventoryQuery(sys, a, sub); Json e = Json::object(); e["total"] = x.at("total"); e["items"] = x.at("items"); j[st] = e;
        }
        res.send(200, j);
    });
    r.get("/api/inventory/batches", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"});
        long long ph = a.is("Pharmacist") && a.pharmacyId > 0 ? a.pharmacyId : queryInt(req, "pharmacy_id", 0, 0, 1000000000), md = queryInt(req, "medicine_id", 0, 0, 1000000000);
        if (!ph && !md) throw ApiError(400, "pharmacy_id or medicine_id is required");
        Json j = Json::object(); j["items"] = db.query("SELECT b.batch_id,b.pharmacy_id,p.name pharmacy,b.medicine_id,m.name medicine,b.batch_number,b.manufacturing_date,b.expiry_date,b.quantity,"
                                                       "CASE WHEN b.expiry_date<date('now') THEN 'Expired' WHEN b.expiry_date<=date('now',?3) THEN 'Near expiry' ELSE 'OK' END batch_status "
                                                       "FROM medicine_batches b JOIN pharmacies p ON p.pharmacy_id=b.pharmacy_id JOIN medicines m ON m.medicine_id=b.medicine_id "
                                                       "WHERE (?1=0 OR b.pharmacy_id=?1) AND (?2=0 OR b.medicine_id=?2) ORDER BY b.expiry_date LIMIT 500", {ph, md, "+" + db.setting("near_expiry_days", "60") + " days"});
        res.send(200, j);
    });
    r.post("/api/inventory/restock", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"}); Json b = req.json();
        res.send(200, sys.restock(reqInt(b, "pharmacy_id", 1, 1000000000), reqInt(b, "medicine_id", 1, 1000000000), (int)reqInt(b, "quantity", 1, 100000),
                                  reqStr(b, "batch_number", 2, 40), reqStr(b, "manufacturing_date", 10, 10), reqStr(b, "expiry_date", 10, 10), a));
    });
    r.put("/api/inventory/:pharmacy/:medicine", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"}); long long ph = req.paramInt("pharmacy"), md = req.paramInt("medicine"); requirePharmacyAccess(a, ph);
        Json b = req.json(); long long lvl = reqInt(b, "reorder_level", 0, 100000);
        if (db.run("UPDATE pharmacy_inventory SET reorder_level=?,last_updated=datetime('now') WHERE pharmacy_id=? AND medicine_id=?", {lvl, ph, md}) == 0) throw ApiError(404, "Inventory record not found");
        db.audit(a, "REORDER_LEVEL_SET", "inventory", to_string(ph) + "/" + to_string(md), to_string(lvl)); res.send(200, message("Reorder level updated"));
    });
    r.post("/api/inventory/write-off-expired", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"}); Json b = req.json();
        Json j = Json::object(); j["units_written_off"] = sys.writeOffExpired(reqInt(b, "pharmacy_id", 1, 1000000000), a); res.send(200, j);
    });
    r.get("/api/inventory/movements", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"});
        long long ph = a.is("Pharmacist") && a.pharmacyId > 0 ? a.pharmacyId : queryInt(req, "pharmacy_id", 0, 0, 1000000000), md = queryInt(req, "medicine_id", 0, 0, 1000000000);
        long long limit = queryInt(req, "limit", 100, 1, 500), offset = queryInt(req, "offset", 0, 0, 10000000);
        Json j = Json::object();
        j["items"] = db.query("SELECT s.movement_id,s.pharmacy_id,p.name pharmacy,s.medicine_id,m.name medicine,s.change_qty,s.reason,s.reference,s.created_at,u.username performed_by "
                              "FROM stock_movements s JOIN pharmacies p ON p.pharmacy_id=s.pharmacy_id JOIN medicines m ON m.medicine_id=s.medicine_id LEFT JOIN users u ON u.user_id=s.performed_by "
                              "WHERE (?1=0 OR s.pharmacy_id=?1) AND (?2=0 OR s.medicine_id=?2) AND (?3='' OR s.reason=?3) ORDER BY s.movement_id DESC LIMIT ?4 OFFSET ?5", {ph, md, req.q("reason"), limit, offset});
        res.send(200, j);
    });
    r.get("/api/inventory/restocks", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin", "Pharmacist"}); Json j = Json::object();
        j["items"] = db.query("SELECT h.restock_id,p.name pharmacy,m.name medicine,h.batch_number,h.quantity,h.created_at,u.username performed_by FROM restocking_history h "
                              "JOIN pharmacies p ON p.pharmacy_id=h.pharmacy_id JOIN medicines m ON m.medicine_id=h.medicine_id LEFT JOIN users u ON u.user_id=h.performed_by ORDER BY h.restock_id DESC LIMIT 200");
        res.send(200, j);
    });
}
