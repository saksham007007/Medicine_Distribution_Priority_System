// ============================================================
//                   SHAURYA - SERVICES & API
//           analytics, DSA-backed reports, data export
// ============================================================

Json MedicineSystem::analyticsSummary() {
    reloadQueue();
    Database& db = database; Json j = Json::object();
    string near_days = "+" + db.setting("near_expiry_days", "60") + " days";
    j["patients"] = db.scalar("SELECT COUNT(*) FROM patients WHERE active=1");
    j["medicines"] = db.scalar("SELECT COUNT(*) FROM medicines WHERE status='Active'");
    j["pharmacies"] = db.scalar("SELECT COUNT(*) FROM pharmacies");
    j["critical_patients"] = db.scalar("SELECT COUNT(*) FROM medicine_requests WHERE urgency='Critical' AND status IN('Pending','Waiting')");
    j["pending_requests"] = (long long)pq.size();
    j["waiting_requests"] = (long long)waitingList.size();
    j["completed_distributions"] = db.scalar("SELECT COUNT(*) FROM distribution_transactions WHERE status='Distributed'");
    j["units_distributed"] = db.scalar("SELECT COALESCE(SUM(quantity),0) FROM distribution_transactions WHERE status='Distributed'");
    j["low_stock_items"] = db.scalar("SELECT COUNT(*) FROM (SELECT i.pharmacy_id FROM pharmacy_inventory i LEFT JOIN medicine_batches b ON b.pharmacy_id=i.pharmacy_id AND b.medicine_id=i.medicine_id "
                                     "GROUP BY i.pharmacy_id,i.medicine_id HAVING COALESCE(SUM(CASE WHEN b.expiry_date>=date('now') THEN b.quantity END),0)<=MAX(i.reorder_level) AND MAX(i.quantity)>0)");
    j["expired_batches"] = db.scalar("SELECT COUNT(*) FROM medicine_batches WHERE expiry_date<date('now') AND quantity>0");
    j["expired_units"] = db.scalar("SELECT COALESCE(SUM(quantity),0) FROM medicine_batches WHERE expiry_date<date('now')");
    j["near_expiry_batches"] = db.scalar("SELECT COUNT(*) FROM medicine_batches WHERE expiry_date>=date('now') AND expiry_date<=date('now',?) AND quantity>0", {near_days});
    j["unavailable_medicines"] = db.scalar("SELECT COUNT(*) FROM medicines m WHERE m.status='Active' AND NOT EXISTS(SELECT 1 FROM medicine_batches b WHERE b.medicine_id=m.medicine_id AND b.expiry_date>=date('now') AND b.quantity>0)");
    j["data_notice"] = db.setting("data_notice");
    return j;
}

static string csvCell(const Json& v) {
    string s = v.isString() ? v.str() : (v.isNull() ? "" : v.dump());
    if (!s.empty() && (s[0] == '=' || s[0] == '+' || s[0] == '-' || s[0] == '@')) s = "'" + s;       // neutralise spreadsheet formula injection
    if (s.find_first_of(",\"\n\r") != string::npos) { string o = "\""; for (char c : s) { if (c == '"') o += '"'; o += c; } return o + "\""; }
    return s;
}

inline void registerAnalyticsRoutes(Router& r, MedicineSystem& sys) {
    Database& db = sys.database;

    r.get("/api/analytics/summary", [&](HttpRequest& req, HttpResponse& res) { require(sys, req, {"Admin", "Pharmacist"}); res.send(200, sys.analyticsSummary()); });

    r.get("/api/analytics/top-medicines", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin", "Pharmacist"}); long long limit = queryInt(req, "limit", 10, 1, 50), days = queryInt(req, "days", 0, 0, 3650);
        Json j = Json::object();
        j["items"] = db.query("SELECT m.generic_name medicine,COUNT(*) requests,SUM(r.quantity) units FROM medicine_requests r JOIN medicines m ON m.medicine_id=r.medicine_id "
                              "WHERE r.status<>'Cancelled' AND (?1=0 OR r.requested_at>=datetime('now',?2)) GROUP BY m.generic_name ORDER BY requests DESC,m.generic_name LIMIT ?3", {days, "-" + to_string(days) + " days", limit});
        res.send(200, j);
    });
    r.get("/api/analytics/trends", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin", "Pharmacist"}); int days = (int)queryInt(req, "days", 14, 1, 90);
        string start = util::addDays(util::today(), -(days - 1));
        Json rows = db.query("SELECT date(created_at) d,COUNT(*) distributions,SUM(quantity) units FROM distribution_transactions WHERE status='Distributed' AND date(created_at)>=? GROUP BY date(created_at)", {start});
        map<string, pair<long long, long long>> byDay; for (auto& x : rows.items()) byDay[x.at("d").str()] = {x.at("distributions").integer(), x.at("units").integer()};
        Json items = Json::array();
        for (int i = 0; i < days; i++) { string d = util::addDays(start, i); Json e = Json::object(); e["date"] = d; e["distributions"] = byDay[d].first; e["units"] = byDay[d].second; items.push_back(e); }
        Json j = Json::object(); j["items"] = items; res.send(200, j);
    });
    r.get("/api/analytics/pharmacy-inventory", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin", "Pharmacist"}); Json j = Json::object();
        j["items"] = db.query("SELECT p.pharmacy_id,p.name pharmacy,p.city,p.status,COUNT(DISTINCT i.medicine_id) medicines,COALESCE(SUM(i.quantity),0) units,"
                              "(SELECT COALESCE(SUM(b.quantity),0) FROM medicine_batches b WHERE b.pharmacy_id=p.pharmacy_id AND b.expiry_date<date('now')) expired_units "
                              "FROM pharmacies p LEFT JOIN pharmacy_inventory i ON i.pharmacy_id=p.pharmacy_id GROUP BY p.pharmacy_id ORDER BY units DESC");
        res.send(200, j);
    });
    r.get("/api/analytics/category-stock", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin", "Pharmacist"}); Json j = Json::object();
        j["items"] = db.query("SELECT c.name category,COUNT(DISTINCT m.medicine_id) medicines,COALESCE(SUM(CASE WHEN b.expiry_date>=date('now') THEN b.quantity END),0) units "
                              "FROM medicine_categories c JOIN medicines m ON m.category_id=c.category_id LEFT JOIN medicine_batches b ON b.medicine_id=m.medicine_id GROUP BY c.category_id ORDER BY units DESC");
        res.send(200, j);
    });
    r.get("/api/analytics/urgency", [&](HttpRequest& req, HttpResponse& res) {          // priority report computed from the in-memory queue
        require(sys, req, {"Admin", "Pharmacist"}); sys.reloadQueue();
        vector<Patient> all = sys.orderedQueue(); for (auto& w : sys.waitingList) all.push_back(w);
        Json j = Json::object(); j["counts"] = priorityCounts(all); j["demand"] = demandJson(all); res.send(200, j);
    });
    r.get("/api/analytics/status-breakdown", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin", "Pharmacist"}); Json j = Json::object(); j["items"] = db.query("SELECT status,COUNT(*) count FROM medicine_requests GROUP BY status ORDER BY count DESC"); res.send(200, j);
    });
    r.get("/api/reports/transactions.csv", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin", "Pharmacist"});
        Json rows = db.query("SELECT t.transaction_id,t.created_at,t.patient_name,t.medicine_name,ph.name pharmacy,t.quantity,t.status,t.notes FROM distribution_transactions t "
                             "LEFT JOIN pharmacies ph ON ph.pharmacy_id=t.pharmacy_id ORDER BY t.transaction_id");
        string csv = "transaction_id,created_at,patient_name,medicine_name,pharmacy,quantity,status,notes\n";
        for (auto& x : rows.items()) { bool first = true; for (auto& kv : x.members()) { if (!first) csv += ','; first = false; csv += csvCell(kv.second); } csv += '\n'; }
        db.audit(a, "REPORT_EXPORTED", "report", "transactions.csv"); res.status = 200; res.body = csv; res.contentType = "text/csv";
    });
}
