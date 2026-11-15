// ============================================================
//                    SAKSHAM - API & SECURITY
//   shared request helpers, authentication, users, settings
// ============================================================

// ---------- validation helpers ----------
inline string reqStr(const Json& j, const string& k, size_t minLen = 1, size_t maxLen = 200) {
    if (!j.at(k).isString()) throw ApiError(422, "'" + k + "' is required");
    string v = util::trim(j.at(k).str());
    if (v.size() < minLen || v.size() > maxLen)
        throw ApiError(422, "'" + k + "' must be " + to_string(minLen) + "-" + to_string(maxLen) + " characters");
    return v;
}
inline string optStr(const Json& j, const string& k, size_t maxLen = 500) {
    if (!j.has(k)) return "";
    if (!j.at(k).isString()) throw ApiError(422, "'" + k + "' must be text");
    string v = util::trim(j.at(k).str());
    if (v.size() > maxLen) throw ApiError(422, "'" + k + "' is too long");
    return v;
}
inline long long reqInt(const Json& j, const string& k, long long lo, long long hi) {
    if (!j.at(k).isNumber() || !j.at(k).isInteger()) throw ApiError(422, "'" + k + "' must be an integer");
    long long v = j.at(k).integer();
    if (v < lo || v > hi) throw ApiError(422, "'" + k + "' must be between " + to_string(lo) + " and " + to_string(hi));
    return v;
}
inline long long queryInt(const HttpRequest& r, const string& k, long long def, long long lo, long long hi) {
    string v = r.q(k); if (v.empty()) return def;
    if (v.size() > 12) throw ApiError(400, "Invalid '" + k + "'");
    for (size_t i = 0; i < v.size(); i++) if (!isdigit((unsigned char)v[i]) && !(i == 0 && v[i] == '-' && v.size() > 1)) throw ApiError(400, "Invalid '" + k + "'");
    return max(lo, min(hi, stoll(v)));
}
inline string likePattern(const string& s) {          // escape % _ for LIKE ... ESCAPE '\'
    string o = "%"; for (char c : s) { if (c == '%' || c == '_' || c == '\\') o += '\\'; o += c; } return o + "%";
}
inline Json message(const string& m) { Json j = Json::object(); j["message"] = m; return j; }
inline void requirePassword(const string& p) {
    bool letter = false, digit = false;
    for (char c : p) { if (isalpha((unsigned char)c)) letter = true; if (isdigit((unsigned char)c)) digit = true; }
    if (p.size() < 8 || p.size() > 100 || !letter || !digit) throw ApiError(422, "Password must be 8-100 characters and contain a letter and a digit");
}

// Value for a performed_by / changed_by column: NULL when the actor is not a database user (e.g. the CLI).
inline Val actorVal(const Auth& a) { return a.userId > 0 ? Val(a.userId) : Val(); }

// ---------- authentication ----------
inline Json userJson(const Json& u) {
    Json j = Json::object();
    for (const char* k : {"user_id", "username", "role", "full_name", "email", "patient_id", "pharmacy_id", "active", "created_at"})
        if (u.at(k).type() != Json::Null || string(k) == "patient_id" || string(k) == "pharmacy_id") j[k] = u.at(k);
    return j;
}
inline string tokenHash(const string& token) { return crypto::hex(crypto::sha256(crypto::bytes(token))); }

inline Auth authenticate(MedicineSystem& sys, HttpRequest& req) {
    string h = req.header("authorization");
    if (h.size() < 8 || util::lower(h.substr(0, 7)) != "bearer ") throw ApiError(401, "Authentication required");
    string token = util::trim(h.substr(7));
    if (token.size() < 32 || token.size() > 128) throw ApiError(401, "Invalid or expired session");
    Json u = sys.database.one("SELECT u.user_id,u.username,u.role,u.patient_id,u.pharmacy_id FROM sessions s JOIN users u ON u.user_id=s.user_id "
                              "WHERE s.token_hash=? AND s.expires_at>datetime('now') AND u.active=1", {tokenHash(token)});
    if (u.isNull()) throw ApiError(401, "Invalid or expired session");
    Auth a; a.userId = u.at("user_id").integer(); a.username = u.at("username").str(); a.role = u.at("role").str();
    a.patientId = u.at("patient_id").integer(); a.pharmacyId = u.at("pharmacy_id").integer();
    return a;
}
inline Auth require(MedicineSystem& sys, HttpRequest& req, initializer_list<const char*> roles) {
    Auth a = authenticate(sys, req);
    for (const char* r : roles) if (a.role == r) return a;
    throw ApiError(403, "Your role (" + a.role + ") is not permitted to perform this action");
}
inline Auth requireAny(MedicineSystem& sys, HttpRequest& req) { return authenticate(sys, req); }
inline void requirePharmacyAccess(const Auth& a, long long pharmacyId) {
    if (a.role == "Pharmacist" && a.pharmacyId > 0 && a.pharmacyId != pharmacyId)
        throw ApiError(403, "You are assigned to a different pharmacy");
}

// Failed-login throttling (in memory): 5 failures lock the username for 60 seconds.
struct LoginThrottle {
    map<string, pair<int, time_t>> st;
    void check(const string& u) {
        auto it = st.find(u);
        if (it != st.end() && it->second.first >= 5 && time(nullptr) < it->second.second) throw ApiError(429, "Too many failed attempts. Try again in a minute.");
    }
    void fail(const string& u) { auto& e = st[u]; e.first++; e.second = time(nullptr) + 60; }
    void ok(const string& u) { st.erase(u); }
};

inline Json createSession(MedicineSystem& sys, long long userId) {
    string token = crypto::hex(crypto::randomBytes(32));
    int hours = (int)max(1LL, min(168LL, atoll(sys.database.setting("session_hours", "12").c_str())));
    sys.database.run("DELETE FROM sessions WHERE expires_at<=datetime('now')");
    sys.database.run("INSERT INTO sessions(token_hash,user_id,expires_at) VALUES(?,?,datetime('now',?))", {tokenHash(token), userId, "+" + to_string(hours) + " hours"});
    Json j = Json::object(); j["token"] = token; j["expires_in_hours"] = hours; return j;
}

inline void registerAuthRoutes(Router& r, MedicineSystem& sys) {
    auto throttle = make_shared<LoginThrottle>();
    Database& db = sys.database;

    r.get("/api/health", [&](HttpRequest&, HttpResponse& res) {
        Json j = Json::object(); j["status"] = "ok"; j["time_utc"] = util::utcNow(); j["medicines"] = (long long)sys.inventory.size();
        j["pharmacies"] = (long long)sys.pharmacies.size(); res.send(200, j);
    });
    r.get("/api/public/info", [&](HttpRequest&, HttpResponse& res) {
        Json j = Json::object(); j["data_notice"] = db.setting("data_notice"); j["project"] = "Medicine Distribution Priority System"; j["team"] = "Bug Busters"; res.send(200, j);
    });

    r.post("/api/auth/login", [&, throttle](HttpRequest& req, HttpResponse& res) {
        Json b = req.json(); string user = util::lower(reqStr(b, "username", 1, 60)), pass = b.at("password").str();
        throttle->check(user);
        Json u = db.one("SELECT * FROM users WHERE username=?", {user});
        static const string dummy = crypto::hashPassword("dummy-password-for-timing");
        bool ok = crypto::verifyPassword(pass, u.isNull() ? dummy : u.at("password_hash").str());
        if (u.isNull() || !ok || u.at("active").integer() != 1) {
            throttle->fail(user);
            db.audit(Auth(), "LOGIN_FAILED", "user", user, "ip=" + req.ip);
            throw ApiError(401, "Invalid username or password");
        }
        throttle->ok(user);
        Json s = createSession(sys, u.at("user_id").integer());
        Auth a; a.userId = u.at("user_id").integer(); a.username = u.at("username").str();
        db.audit(a, "LOGIN", "user", to_string(a.userId), "ip=" + req.ip);
        s["user"] = userJson(u); res.send(200, s);
    });

    r.post("/api/auth/register", [&](HttpRequest& req, HttpResponse& res) {   // patient self-registration
        Json b = req.json(); string user = util::lower(reqStr(b, "username", 3, 40)), pass = b.at("password").str();
        for (char c : user) if (!isalnum((unsigned char)c) && c != '.' && c != '_') throw ApiError(422, "Username may contain letters, digits, '.' and '_' only");
        requirePassword(pass);
        string email = optStr(b, "email", 120); if (!email.empty() && !util::validEmail(email)) throw ApiError(422, "Invalid email address");
        if (!db.one("SELECT 1 FROM users WHERE username=?", {user}).isNull()) throw ApiError(409, "Username already taken");
        Tx tx(db);
        Json pj = b; pj["name"] = b.at("full_name"); long long pid = sys.createPatient(pj);
        db.run("INSERT INTO users(username,password_hash,role,full_name,email,patient_id) VALUES(?,?,'Patient',?,?,?)",
               {user, crypto::hashPassword(pass), reqStr(b, "full_name", 2, 100), email.empty() ? Val() : Val(email), pid});
        long long uid = db.lastId(); Auth a; a.userId = uid; a.username = user;
        db.audit(a, "REGISTER", "user", to_string(uid), "patient_id=" + to_string(pid));
        tx.commit();
        Json j = message("Registration successful. Please log in."); j["user_id"] = uid; j["patient_id"] = pid; res.send(201, j);
    });

    r.post("/api/auth/logout", [&](HttpRequest& req, HttpResponse& res) {
        authenticate(sys, req); string t = util::trim(req.header("authorization").substr(7));
        db.run("DELETE FROM sessions WHERE token_hash=?", {tokenHash(t)}); res.send(200, message("Logged out"));
    });

    r.get("/api/auth/me", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = authenticate(sys, req);
        Json u = db.one("SELECT user_id,username,role,full_name,email,patient_id,pharmacy_id,active,created_at FROM users WHERE user_id=?", {a.userId});
        Json j = userJson(u);
        if (a.patientId) j["patient"] = db.one("SELECT * FROM patients WHERE patient_id=?", {a.patientId});
        if (a.pharmacyId) j["pharmacy"] = db.one("SELECT pharmacy_id,name,city FROM pharmacies WHERE pharmacy_id=?", {a.pharmacyId});
        res.send(200, j);
    });

    r.post("/api/auth/change-password", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = authenticate(sys, req); Json b = req.json();
        Json u = db.one("SELECT password_hash FROM users WHERE user_id=?", {a.userId});
        if (!crypto::verifyPassword(b.at("current_password").str(), u.at("password_hash").str())) throw ApiError(403, "Current password is incorrect");
        string np = b.at("new_password").str(); requirePassword(np);
        db.run("UPDATE users SET password_hash=? WHERE user_id=?", {crypto::hashPassword(np), a.userId});
        db.run("DELETE FROM sessions WHERE user_id=?", {a.userId});             // force re-login everywhere
        db.audit(a, "PASSWORD_CHANGED", "user", to_string(a.userId)); res.send(200, message("Password changed. Please log in again."));
    });

    // ----- user management (Admin) -----
    r.get("/api/users", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin"});
        string q = req.q("q"), role = req.q("role");
        Json rows = db.query("SELECT user_id,username,role,full_name,email,patient_id,pharmacy_id,active,created_at FROM users "
                             "WHERE (?1='' OR role=?1) AND (?2='%' OR username LIKE ?2 ESCAPE '\\' OR full_name LIKE ?2 ESCAPE '\\') ORDER BY user_id LIMIT 500",
                             {role, q.empty() ? string("%") : likePattern(q)});
        Json j = Json::object(); j["items"] = rows; res.send(200, j);
    });
    r.post("/api/users", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); Json b = req.json();
        string user = util::lower(reqStr(b, "username", 3, 40)), role = reqStr(b, "role", 5, 12), pass = b.at("password").str();
        if (role != "Admin" && role != "Pharmacist" && role != "Patient") throw ApiError(422, "role must be Admin, Pharmacist or Patient");
        requirePassword(pass);
        if (!db.one("SELECT 1 FROM users WHERE username=?", {user}).isNull()) throw ApiError(409, "Username already taken");
        Val pid, phid;
        if (b.has("patient_id")) { long long v = reqInt(b, "patient_id", 1, 1000000000); if (db.one("SELECT 1 FROM patients WHERE patient_id=?", {v}).isNull()) throw ApiError(422, "Unknown patient_id"); pid = Val(v); }
        if (b.has("pharmacy_id")) { long long v = reqInt(b, "pharmacy_id", 1, 1000000000); if (db.one("SELECT 1 FROM pharmacies WHERE pharmacy_id=?", {v}).isNull()) throw ApiError(422, "Unknown pharmacy_id"); phid = Val(v); }
        if (role == "Patient" && pid.t == Val::Null) throw ApiError(422, "Patient accounts must be linked to a patient_id");
        string email = optStr(b, "email", 120); if (!email.empty() && !util::validEmail(email)) throw ApiError(422, "Invalid email address");
        db.run("INSERT INTO users(username,password_hash,role,full_name,email,patient_id,pharmacy_id) VALUES(?,?,?,?,?,?,?)",
               {user, crypto::hashPassword(pass), role, reqStr(b, "full_name", 2, 100), email.empty() ? Val() : Val(email), pid, phid});
        long long id = db.lastId(); db.audit(a, "USER_CREATED", "user", to_string(id), user + " (" + role + ")");
        Json j = message("User created"); j["user_id"] = id; res.send(201, j);
    });
    r.put("/api/users/:id", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); long long id = req.paramInt("id"); Json b = req.json();
        Json u = db.one("SELECT * FROM users WHERE user_id=?", {id}); if (u.isNull()) throw ApiError(404, "User not found");
        string role = b.has("role") ? reqStr(b, "role", 5, 12) : u.at("role").str();
        if (role != "Admin" && role != "Pharmacist" && role != "Patient") throw ApiError(422, "Invalid role");
        int active = b.has("active") ? (b.at("active").boolean() ? 1 : 0) : (int)u.at("active").integer();
        if (id == a.userId && (role != "Admin" || !active)) throw ApiError(409, "You cannot demote or deactivate your own account");
        if (u.at("role").str() == "Admin" && (role != "Admin" || !active) && db.scalar("SELECT COUNT(*) FROM users WHERE role='Admin' AND active=1 AND user_id<>?", {id}) == 0)
            throw ApiError(409, "At least one active Admin must remain");
        string email = b.has("email") ? optStr(b, "email", 120) : u.at("email").str(); if (!email.empty() && !util::validEmail(email)) throw ApiError(422, "Invalid email address");
        Val phid = u.at("pharmacy_id").isNull() ? Val() : Val(u.at("pharmacy_id").integer());
        if (b.has("pharmacy_id")) { long long v = reqInt(b, "pharmacy_id", 0, 1000000000); if (v == 0) phid = Val(); else { if (db.one("SELECT 1 FROM pharmacies WHERE pharmacy_id=?", {v}).isNull()) throw ApiError(422, "Unknown pharmacy_id"); phid = Val(v); } }
        db.run("UPDATE users SET full_name=?,email=?,role=?,active=?,pharmacy_id=? WHERE user_id=?",
               {b.has("full_name") ? reqStr(b, "full_name", 2, 100) : u.at("full_name").str(), email.empty() ? Val() : Val(email), role, active, phid, id});
        if (b.has("password")) { requirePassword(b.at("password").str()); db.run("UPDATE users SET password_hash=? WHERE user_id=?", {crypto::hashPassword(b.at("password").str()), id}); }
        if (!active || role != u.at("role").str() || b.has("password")) db.run("DELETE FROM sessions WHERE user_id=?", {id});
        db.audit(a, "USER_UPDATED", "user", to_string(id)); res.send(200, message("User updated"));
    });
    r.del("/api/users/:id", [&](HttpRequest& req, HttpResponse& res) {          // deactivation (history is preserved)
        Auth a = require(sys, req, {"Admin"}); long long id = req.paramInt("id");
        if (id == a.userId) throw ApiError(409, "You cannot deactivate your own account");
        Json u = db.one("SELECT role FROM users WHERE user_id=?", {id}); if (u.isNull()) throw ApiError(404, "User not found");
        if (u.at("role").str() == "Admin" && db.scalar("SELECT COUNT(*) FROM users WHERE role='Admin' AND active=1 AND user_id<>?", {id}) == 0) throw ApiError(409, "At least one active Admin must remain");
        db.run("UPDATE users SET active=0 WHERE user_id=?", {id}); db.run("DELETE FROM sessions WHERE user_id=?", {id});
        db.audit(a, "USER_DEACTIVATED", "user", to_string(id)); res.send(200, message("User deactivated"));
    });

    // ----- settings & audit log (Admin) -----
    r.get("/api/settings", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin"}); Json j = Json::object(); j["items"] = db.query("SELECT key,value,description,updated_at FROM system_settings ORDER BY key"); res.send(200, j);
    });
    r.put("/api/settings", [&](HttpRequest& req, HttpResponse& res) {
        Auth a = require(sys, req, {"Admin"}); Json b = req.json();
        static const map<string, pair<long long, long long>> numeric = {{"near_expiry_days", {1, 730}}, {"session_hours", {1, 168}}, {"default_reorder_level", {0, 10000}}};
        for (auto& kv : b.members()) {
            const string& k = kv.first;
            if (numeric.count(k)) {
                if (!kv.second.isNumber() || !kv.second.isInteger() || kv.second.integer() < numeric.at(k).first || kv.second.integer() > numeric.at(k).second) throw ApiError(422, "Invalid value for " + k);
                db.setSetting(k, to_string(kv.second.integer()));
            } else if (k == "enforce_priority") { db.setSetting(k, kv.second.boolean() || kv.second.str() == "1" || kv.second.integer() == 1 ? "1" : "0"); }
            else if (k == "data_notice") { if (!kv.second.isString() || kv.second.str().size() > 500) throw ApiError(422, "Invalid data_notice"); db.setSetting(k, kv.second.str()); }
            else throw ApiError(422, "Unknown setting: " + k);
        }
        db.audit(a, "SETTINGS_UPDATED", "settings", "", b.dump()); res.send(200, message("Settings saved"));
    });
    r.get("/api/audit-logs", [&](HttpRequest& req, HttpResponse& res) {
        require(sys, req, {"Admin"});
        string q = req.q("q"); long long limit = queryInt(req, "limit", 100, 1, 500), offset = queryInt(req, "offset", 0, 0, 1000000);
        string pat = q.empty() ? string("%") : likePattern(q);
        Json rows = db.query("SELECT log_id,user_id,username,action,entity,entity_id,details,created_at FROM audit_logs WHERE (?1='%' OR action LIKE ?1 ESCAPE '\\' OR username LIKE ?1 ESCAPE '\\' OR entity LIKE ?1 ESCAPE '\\' OR details LIKE ?1 ESCAPE '\\') "
                             "ORDER BY log_id DESC LIMIT ?2 OFFSET ?3", {pat, limit, offset});
        Json j = Json::object(); j["items"] = rows; j["total"] = db.scalar("SELECT COUNT(*) FROM audit_logs"); res.send(200, j);
    });
}
