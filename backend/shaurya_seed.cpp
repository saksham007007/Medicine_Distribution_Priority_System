// ---------------- Database: repeatable demo-data seeding (Shaurya) ----------------
// Every insert is "OR IGNORE" on a natural key and all pseudo-randomness is derived from
// stable hashes, so running the seeder repeatedly never creates duplicate records.
// ALL generated data is synthetic demonstration data.
#include "seed_catalogue.h"

namespace seed {
struct Formulation { string generic, category, form, strength; int rx; double price; int strengthIdx; };

inline uint64_t mix(uint64_t x) {                  // splitmix64
    x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; return x ^ (x >> 31);
}
inline double unit(uint64_t a, uint64_t b = 0, uint64_t c = 0) { return (double)(mix(a * 1000003ULL ^ mix(b * 7919ULL ^ mix(c))) % 1000000) / 1000000.0; }
inline int pick(int n, uint64_t a, uint64_t b = 0, uint64_t c = 0) { return (int)(unit(a, b, c) * n) % n; }

inline vector<string> splitStr(const string& s, char d) {
    vector<string> v; string cur;
    for (char c : s) { if (c == d) { v.push_back(cur); cur.clear(); } else cur += c; }
    v.push_back(cur); return v;
}
inline vector<Formulation> parseCatalogue(vector<string>& genericOrder) {
    vector<Formulation> out; set<string> seen;
    for (const char* chunk : CATALOGUE) {
        for (const string& line : splitStr(chunk, '\n')) {
            if (line.empty()) continue;
            auto f = splitStr(line, '|'); if (f.size() != 5) continue;
            if (seen.insert(f[0]).second) genericOrder.push_back(f[0]);
            int si = 0;
            for (const string& fs : splitStr(f[4], ';')) {
                size_t eq = fs.find('='); string form = fs.substr(0, eq);
                si = 0;
                for (const string& st : splitStr(fs.substr(eq + 1), ',')) {
                    out.push_back({f[0], f[1], form, st, atoi(f[2].c_str()), atof(f[3].c_str()), si++});
                }
            }
        }
    }
    return out;
}
inline string alphaLower(const string& s) { string r; for (char c : s) if (isalpha((unsigned char)c)) r += (char)tolower((unsigned char)c); return r; }
inline string stemFor(const string& generic, set<string>& used) {
    auto words = splitStr(generic, ' '); vector<string> w;
    for (auto& x : words) { string a = alphaLower(x); if (!a.empty()) w.push_back(a); }
    string base = w.empty() ? "med" : w[0];
    if (generic.find('+') != string::npos && w.size() >= 2) {
        size_t second = 1; while (second < w.size() && (w[second] == "acid" || w[second].size() < 3)) second++;
        if (second >= w.size()) second = w.size() - 1;
        string s = w[0].substr(0, 3) + w[second].substr(0, 3);
        if (used.insert(s).second) return s;
    }
    for (size_t len = 4; len <= base.size() && len <= 8; len++) { string s = base.substr(0, len); if (used.insert(s).second) return s; }
    for (int i = 2;; i++) { string s = base.substr(0, 4) + to_string(i); if (used.insert(s).second) return s; }
}
inline double formMultiplier(const string& f) {
    static const map<string, double> m = {{"Tablet",1},{"Capsule",1.2},{"Extended-release Tablet",1.6},{"Syrup",9},{"Suspension",9},
        {"Injection",12},{"Cream",6},{"Ointment",6},{"Gel",7},{"Lotion",6},{"Solution",6},{"Inhaler",25},{"Eye Drops",8},
        {"Nasal Drops",6},{"Ear Drops",6},{"Powder",4},{"Shampoo",8},{"Respules",5}};
    auto it = m.find(f); return it == m.end() ? 1 : it->second;
}
inline string storageFor(const string& f) {
    if (f == "Injection") return "Store below 25 C, protect from light. Insulin products: refrigerate at 2-8 C, do not freeze.";
    if (f == "Syrup" || f == "Suspension") return "Store below 25 C. Shake well; discard 30 days after opening unless label states otherwise.";
    if (f == "Inhaler" || f == "Respules") return "Store below 30 C away from heat and direct sunlight. Do not puncture the canister.";
    if (f.find("Drops") != string::npos) return "Store below 25 C. Discard 28 days after first opening.";
    if (f == "Cream" || f == "Ointment" || f == "Gel" || f == "Lotion" || f == "Solution" || f == "Shampoo") return "Store below 30 C. Keep the cap tightly closed.";
    return "Store below 30 C in a dry place, protected from light and moisture.";
}
} // namespace seed

void Database::seedDemoData() {
    using namespace seed;
    static const char* MFG[24][2] = {{"Ganga Valley Labs","Haridwar"},{"Doon Remedies","Dehradun"},{"Bharat Biocare","Delhi"},
        {"Meru Healthcare","Rishikesh"},{"Aravali Lifesciences","Jaipur"},{"Kaveri Pharma","Bengaluru"},{"Narmada Therapeutics","Bhopal"},
        {"Sahyadri Drugs","Pune"},{"Vindhya Formulations","Indore"},{"Konkan Remedies","Mumbai"},{"Deccan Biotech","Hyderabad"},
        {"Malabar Healthcare","Kochi"},{"Godavari Pharma","Nashik"},{"Brahmaputra Labs","Guwahati"},{"Chambal Lifesciences","Gwalior"},
        {"Yamuna Pharmaceuticals","Noida"},{"Sutlej Remedies","Ludhiana"},{"Tapti Healthcare","Surat"},{"Mahanadi Biologics","Bhubaneswar"},
        {"Hooghly Pharma","Kolkata"},{"Periyar Drugs","Chennai"},{"Ravi Valley Pharma","Amritsar"},{"Teesta Therapeutics","Siliguri"},{"Beas Healthcare","Chandigarh"}};
    static const char* SUFFIX[24] = {"zen","vax","ora","lyn","mina","cor","tex","pro","dyn","ful","gen","lex","nova","rel","sia","tab","vita","mex","ron","cil","bis","aid","tor","xin"};
    const string today = util::today();
    Tx tx(*this);

    // ----- categories, manufacturers, medicines -----
    vector<string> genericOrder; auto forms = parseCatalogue(genericOrder);
    for (auto& f : forms) run("INSERT OR IGNORE INTO medicine_categories(name) VALUES(?)", {f.category});
    for (auto& m : MFG) run("INSERT OR IGNORE INTO manufacturers(name,city) VALUES(?,?)", {m[0], m[1]});
    set<string> usedStems; map<string, string> stem; map<string, int> genericIdx;
    for (size_t i = 0; i < genericOrder.size(); i++) { stem[genericOrder[i]] = stemFor(genericOrder[i], usedStems); genericIdx[genericOrder[i]] = (int)i; }
    map<string, int> catId, mfgId;
    Json catRows = query("SELECT category_id,name FROM medicine_categories"), mfgRows = query("SELECT manufacturer_id,name FROM manufacturers");
    for (auto& r : catRows.items()) catId[r.at("name").str()] = (int)r.at("category_id").integer();
    for (auto& r : mfgRows.items()) mfgId[r.at("name").str()] = (int)r.at("manufacturer_id").integer();

    for (size_t fi = 0; fi < forms.size(); fi++) {
        const auto& f = forms[fi]; int gi = genericIdx[f.generic];
        int brands = 2 + ((gi * 7 + (int)fi * 3) % 3 == 0 ? 1 : 0);
        int m0 = pick(24, gi, fi);
        for (int k = 0; k < brands; k++) {
            int mi = (m0 + k * 7) % 24;
            string brand = stem[f.generic]; brand[0] = (char)toupper((unsigned char)brand[0]); brand += SUFFIX[mi];
            string strength = f.strength; strength.erase(remove(strength.begin(), strength.end(), ' '), strength.end());
            string name = brand + " " + strength + " " + f.form;
            double price = f.price * (1 + 0.3 * f.strengthIdx) * formMultiplier(f.form) * (0.85 + 0.1 * k + 0.1 * unit(gi, fi, k));
            price = max(0.5, floor(price * 2 + 0.5) / 2);
            string comp = f.generic + " " + f.strength;
            int minStock = 15 + 5 * pick(4, gi, fi, 77);
            run("INSERT OR IGNORE INTO medicines(name,generic_name,brand_name,category_id,manufacturer_id,composition,strength,"
                "dosage_form,requires_prescription,description,unit_price,storage_instructions,min_stock) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
                {name, f.generic, brand, catId[f.category], mfgId[MFG[mi][0]], comp, f.strength, f.form, f.rx,
                 f.generic + " (" + f.category + ") " + f.strength + " " + f.form + ". Fictional demonstration brand; not a real product listing.",
                 price, storageFor(f.form), minStock});
        }
    }

    // ----- alternatives: same composition (other brands) + same generic/form with a different strength -----
    {
        Json meds = query("SELECT medicine_id,generic_name,strength,dosage_form FROM medicines ORDER BY medicine_id");
        map<string, vector<long long>> byComp; map<string, map<string, vector<long long>>> byGenForm;
        for (auto& m : meds.items()) {
            string g = m.at("generic_name").str(), s = m.at("strength").str(), fm = m.at("dosage_form").str();
            long long id = m.at("medicine_id").integer();
            byComp[g + "|" + s + "|" + fm].push_back(id); byGenForm[g + "|" + fm][s].push_back(id);
        }
        for (auto& g : byComp) for (long long a : g.second) for (long long b : g.second) if (a != b)
            run("INSERT OR IGNORE INTO medicine_alternatives(medicine_id,alternative_id,match_type) VALUES(?,?,'Same composition')", {a, b});
        for (auto& gf : byGenForm) for (auto& s1 : gf.second) for (long long a : s1.second) {
            int linked = 0;
            for (auto& s2 : gf.second) { if (s2.first == s1.first || linked >= 3) continue;
                run("INSERT OR IGNORE INTO medicine_alternatives(medicine_id,alternative_id,match_type) VALUES(?,?,'Same generic, different strength')", {a, s2.second[0]}); linked++; }
        }
    }

    // ----- pharmacies (30 synthetic) -----
    struct City { const char* name; const char* state; double lat, lon; const char* pin; const char* roads[3]; };
    static const City C[] = {
        {"Dehradun","Uttarakhand",30.3165,78.0322,"2480",{"Rajpur Road","Ballupur Chowk","Haridwar Bypass Road"}},
        {"Haridwar","Uttarakhand",29.9457,78.1642,"2494",{"Jwalapur Main Road","Railway Road","Bhupatwala"}},
        {"Rishikesh","Uttarakhand",30.0869,78.2676,"2491",{"Dehradun Road","Muni Ki Reti","Tapovan"}},
        {"Delhi","Delhi",28.6139,77.2090,"1100",{"Lajpat Nagar Market","Karol Bagh Main Road","Dwarka Sector 10"}},
        {"Noida","Uttar Pradesh",28.5355,77.3910,"2013",{"Sector 18","Sector 62","Sector 50"}},
        {"Gurugram","Haryana",28.4595,77.0266,"1220",{"Sector 14 Market","MG Road","Sohna Road"}},
        {"Mumbai","Maharashtra",19.0760,72.8777,"4000",{"Andheri West","Dadar East","Powai"}},
        {"Pune","Maharashtra",18.5204,73.8567,"4110",{"FC Road","Kothrud","Viman Nagar"}},
        {"Bengaluru","Karnataka",12.9716,77.5946,"5600",{"Indiranagar 100ft Road","Jayanagar 4th Block","Whitefield Main Road"}},
        {"Hyderabad","Telangana",17.3850,78.4867,"5000",{"Banjara Hills Road No. 12","Madhapur","Secunderabad"}},
        {"Chennai","Tamil Nadu",13.0827,80.2707,"6000",{"Anna Nagar","T Nagar","Adyar"}},
        {"Kolkata","West Bengal",22.5726,88.3639,"7000",{"Park Street","Salt Lake Sector V","Gariahat"}},
        {"Jaipur","Rajasthan",26.9124,75.7873,"3020",{"MI Road","Malviya Nagar","Vaishali Nagar"}},
        {"Lucknow","Uttar Pradesh",26.8467,80.9462,"2260",{"Hazratganj","Gomti Nagar","Aliganj"}},
        {"Ahmedabad","Gujarat",23.0225,72.5714,"3800",{"CG Road","Navrangpura","Satellite"}},
        {"Chandigarh","Chandigarh",30.7333,76.7794,"1600",{"Sector 17","Sector 35","Sector 22"}},
        {"Bhopal","Madhya Pradesh",23.2599,77.4126,"4620",{"MP Nagar","New Market","Arera Colony"}}};
    static const int CITY_PLAN[30] = {0,0,0,0,0,0,1,1,2,2,3,3,3,4,5,6,6,7,7,8,8,9,10,11,12,13,14,15,16,0};
    static const char* PNAME[30] = {"Sanjeevani Medico","Arogya Pharmacy","Jeevan Rekha Chemists","Shree Dhanvantari Medicals","Rajpur Road Medical Store",
        "Clock Tower Pharmacy","Ganga Care Medicos","Har Ki Pauri Chemist","Vedant Pharmacy","Lifeline Medicals","Apna Dawakhana",
        "Swasthya Sathi Pharmacy","Lajpat Health Chemists","Medi Hub Noida","Millennium Chemists","Sagar Medico","Matunga Health Pharmacy",
        "Deccan Chemists","Kothrud Care Pharmacy","Namma Medicals","Cubbon Pharmacy","Charminar Drugstore","Tamil Medico Centre",
        "Hooghly Health Pharmacy","Pink City Pharmacy","Nawab Chemists","Sabarmati Medicals","Sector 17 Pharmacy","Lake City Medicos","Prakash Drug House"};
    for (int i = 0; i < 30; i++) {
        const City& c = C[CITY_PLAN[i]];
        double lat = c.lat + (unit(i, 1) - 0.5) * 0.08, lon = c.lon + (unit(i, 2) - 0.5) * 0.08;
        string pin = string(c.pin) + (char)('0' + pick(10, i, 3)) + (char)('0' + pick(10, i, 4));
        string phone = "+91-00000-0" + to_string(1000 + i * 37 % 9000);
        string slug; for (char ch : string(PNAME[i])) slug += isalnum((unsigned char)ch) ? (char)tolower((unsigned char)ch) : '.';
        int status = i == 11 ? 1 : (i == 24 ? 2 : 0);       // one Closed, one Suspended for realistic filtering
        run("INSERT OR IGNORE INTO pharmacies(name,address,city,state,pin_code,latitude,longitude,phone,email,opening_time,closing_time,status,delivery_available) "
            "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
            {PNAME[i], "Shop " + to_string(1 + pick(60, i, 5)) + ", " + c.roads[i % 3], c.name, c.state, pin, lat, lon, phone,
             slug + "@example.com", i % 5 == 0 ? "00:00" : "08:30", i % 5 == 0 ? "23:59" : (i % 2 ? "21:30" : "22:00"),
             status == 0 ? "Open" : (status == 1 ? "Closed" : "Suspended"), i % 3 != 0 ? 1 : 0});
    }

    // ----- pharmacy inventory + batches (deterministic, partial coverage, mix of healthy/low/near-expiry/expired) -----
    Json pharm = query("SELECT pharmacy_id FROM pharmacies ORDER BY pharmacy_id");
    Json medIds = query("SELECT medicine_id,min_stock FROM medicines ORDER BY medicine_id");
    for (auto& p : pharm.items()) {
        long long pid = p.at("pharmacy_id").integer();
        double coverage = 0.10 + 0.22 * unit(pid, 9);
        for (auto& m : medIds.items()) {
            long long mid = m.at("medicine_id").integer();
            if (mid % 17 == 0) continue;                                  // shortage medicines: stocked nowhere
            if (unit(pid, mid, 1) >= coverage) continue;
            int reorder = 10 + pick(21, pid, mid, 2);
            bool low = unit(pid, mid, 3) < 0.08;
            int total = low ? 1 + pick(reorder - 1, pid, mid, 4) : 25 + pick(226, pid, mid, 4);
            int nb = 1 + (pick(3, pid, mid, 5) == 0) + (pick(7, pid, mid, 6) == 0);
            if (total < nb) nb = 1;
            struct B { string no, mfg, exp; int qty; }; vector<B> batches; int left = total;
            for (int k = 0; k < nb; k++) {
                int q = (k == nb - 1) ? left : max(1, left / (nb - k)); left -= q;
                double r = unit(pid, mid, 10 + k); string exp;
                if (r < 0.04) exp = util::addDays(today, -(10 + pick(190, pid, mid, 20 + k)));
                else if (r < 0.10) exp = util::addDays(today, 5 + pick(50, pid, mid, 20 + k));
                else exp = util::addDays(today, 90 + pick(810, pid, mid, 20 + k));
                string mfg = util::addDays(min(util::addDays(exp, -730), util::addDays(today, -30 - pick(300, pid, mid, 30 + k))), 0);
                batches.push_back({"B" + to_string(pid) + "-" + to_string(mid) + "-" + to_string(k + 1), mfg, exp, q});
            }
            run("INSERT OR IGNORE INTO pharmacy_inventory(pharmacy_id,medicine_id,quantity,reorder_level) VALUES(?,?,?,?)", {pid, mid, total, reorder});
            for (auto& b : batches)
                run("INSERT OR IGNORE INTO medicine_batches(pharmacy_id,medicine_id,batch_number,manufacturing_date,expiry_date,quantity) VALUES(?,?,?,?,?,?)",
                    {pid, mid, b.no, b.mfg, b.exp, b.qty});
        }
    }

    // ----- demo users, patients, prescriptions, requests, history (only on a fresh database) -----
    if (scalar("SELECT COUNT(*) FROM patients") == 0) {
        static const char* FN[] = {"Aarav","Vivaan","Aditya","Rohan","Ananya","Diya","Isha","Kavya","Meera","Priya","Rahul","Sanjay","Neha","Pooja","Amit","Sunita","Deepak","Anjali","Vikram","Rekha"};
        static const char* LN[] = {"Sharma","Verma","Rawat","Negi","Bisht","Joshi","Gupta","Singh","Patel","Nair","Iyer","Reddy","Das","Khan","Mehta","Chauhan"};
        static const bool FEMALE[20] = {0,0,0,0,1,1,1,1,1,1,0,0,1,1,0,1,0,1,0,1};
        for (int i = 0; i < 40; i++) {
            const City& c = C[CITY_PLAN[(i * 5) % 30]];
            string name = string(FN[i % 20]) + " " + LN[(i * 7 + 3) % 16];
            run("INSERT INTO patients(name,age,gender,contact,email,address,city,latitude,longitude) VALUES(?,?,?,?,?,?,?,?,?)",
                {name, 5 + pick(80, i, 50), FEMALE[i % 20] ? "Female" : "Male",
                 "+91-00000-1" + to_string(2000 + i * 53), "patient" + to_string(i + 1) + "@example.com",
                 "House " + to_string(1 + pick(200, i, 51)) + ", " + c.roads[i % 3], c.name,
                 c.lat + (unit(i, 52) - 0.5) * 0.06, c.lon + (unit(i, 53) - 0.5) * 0.06});
        }
        // Demo accounts (passwords documented in README; change them before any real use).
        struct U { const char* user; const char* pass; const char* role; const char* name; int patient; int pharmacy; };
        const U users[] = {{"admin","Admin@123","Admin","System Administrator",0,0},
                           {"pharmacist","Pharma@123","Pharmacist","Network Pharmacist",0,0},
                           {"pharmacist.doon","Pharma@123","Pharmacist","Sanjeevani Medico Pharmacist",0,1},
                           {"patient","Patient@123","Patient","Demo Patient",1,0},
                           {"patient2","Patient@123","Patient","Demo Patient Two",2,0}};
        for (auto& u : users) {
            if (!one("SELECT 1 FROM users WHERE username=?", {u.user}).isNull()) continue;
            run("INSERT INTO users(username,password_hash,role,full_name,email,patient_id,pharmacy_id) VALUES(?,?,?,?,?,?,?)",
                {u.user, crypto::hashPassword(u.pass), u.role, u.name, string(u.user) + "@example.com",
                 u.patient ? Val(u.patient) : Val(), u.pharmacy ? Val(u.pharmacy) : Val()});
        }
        // Candidate medicines: stocked (unexpired) somewhere vs. stocked nowhere.
        Json stocked = query("SELECT m.medicine_id,m.name,m.requires_prescription rx FROM medicines m WHERE EXISTS("
                             "SELECT 1 FROM medicine_batches b WHERE b.medicine_id=m.medicine_id AND b.expiry_date>=date('now') AND b.quantity>=30) ORDER BY m.medicine_id");
        Json outOf = query("SELECT medicine_id,name FROM medicines WHERE requires_prescription=0 AND medicine_id%17=0 AND EXISTS("
                           "SELECT 1 FROM medicine_alternatives a JOIN medicine_batches b ON b.medicine_id=a.alternative_id "
                           "WHERE a.medicine_id=medicines.medicine_id AND b.expiry_date>=date('now') AND b.quantity>0) ORDER BY medicine_id LIMIT 3");
        static const char* URG[] = {"Normal","Critical","Medium","High","Normal","High","Medium","Critical"};
        auto addRequest = [&](int patient, long long med, int qty, const string& urg, const string& status, int minutesAgo, long long rx) {
            run("INSERT INTO medicine_requests(patient_id,medicine_id,quantity,urgency,priority,status,prescription_id,requested_at,updated_at) "
                "VALUES(?,?,?,?,?,?,?,datetime('now',?),datetime('now',?))",
                {patient, med, qty, urg, Patient::priorityOf(urg), status, rx > 0 ? Val(rx) : Val(),
                 "-" + to_string(minutesAgo) + " minutes", "-" + to_string(minutesAgo) + " minutes"});
            return lastId();
        };
        for (int i = 0; i < 16 && stocked.size() > 20; i++) {                   // pending queue
            const Json& m = stocked.items()[(size_t)pick((int)stocked.size(), 100 + i)];
            long long rx = 0;
            if (m.at("rx").integer() == 1) {
                run("INSERT INTO prescriptions(patient_id,medicine_id,quantity,doctor_name,doctor_reg_no,issued_date,verified,verified_at) VALUES(?,?,?,?,?,?,?,?)",
                    {i % 40 + 1, m.at("medicine_id").integer(), 10, "Dr. Sample Physician", "DEMO-REG-" + to_string(1000 + i), util::addDays(today, -(i % 9)),
                     i % 2, i % 2 ? Val(util::utcNow()) : Val()});
                rx = lastId();
            }
            long long rid = addRequest(i % 40 + 1, m.at("medicine_id").integer(), 5 + pick(20, 200 + i), URG[i % 8], "Pending", 5 + i * 17, rx);
            run("INSERT INTO patient_priority_queue(request_id,priority,status) VALUES(?,?,'Queued')", {rid, Patient::priorityOf(URG[i % 8])});
            run("INSERT INTO priority_history(request_id,new_urgency,reason) VALUES(?,?,'Initial triage')", {rid, URG[i % 8]});
        }
        for (size_t i = 0; i < outOf.size(); i++) {                             // waiting list (out of stock everywhere)
            long long med = outOf.items()[i].at("medicine_id").integer();
            long long rid = addRequest(20 + (int)i, med, 10, i == 0 ? "Critical" : "High", "Waiting", 120 + (int)i * 30, 0);
            run("INSERT INTO patient_priority_queue(request_id,priority,status) VALUES(?,?,'Waiting')", {rid, Patient::priorityOf(i == 0 ? "Critical" : "High")});
            run("INSERT INTO waiting_list(request_id,patient_id,medicine_id,quantity) VALUES(?,?,?,10)", {rid, 20 + (int)i, med});
            run("INSERT INTO distribution_transactions(request_id,patient_id,patient_name,medicine_id,medicine_name,quantity,status,notes) "
                "SELECT ?,p.patient_id,p.name,m.medicine_id,m.name,10,'Waiting','Out of stock - added to waiting list' FROM patients p,medicines m WHERE p.patient_id=? AND m.medicine_id=?",
                {rid, 20 + (int)i, med});
        }
        // Illustrative history (14 days) for dashboards. These rows do not alter current stock.
        Json holders = query("SELECT i.pharmacy_id,i.medicine_id,m.name FROM pharmacy_inventory i JOIN medicines m ON m.medicine_id=i.medicine_id "
                             "JOIN pharmacies p ON p.pharmacy_id=i.pharmacy_id WHERE p.status='Open' AND i.quantity>=60 AND m.requires_prescription=0 ORDER BY i.medicine_id,i.pharmacy_id");
        for (int i = 0; i < 60 && holders.size() > 10; i++) {
            const Json& h = holders.items()[(size_t)pick((int)holders.size(), 300 + i)];
            int patient = 1 + pick(40, 301 + i), daysAgo = pick(14, 302 + i), qty = 3 + pick(18, 303 + i);
            string urg = URG[pick(8, 304 + i)];
            long long rid = addRequest(patient, h.at("medicine_id").integer(), qty, urg, "Distributed", daysAgo * 1440 + pick(600, 305 + i), 0);
            run("UPDATE medicine_requests SET fulfilled_pharmacy_id=? WHERE request_id=?", {h.at("pharmacy_id").integer(), rid});
            run("INSERT INTO patient_priority_queue(request_id,priority,status) VALUES(?,?,'Processed')", {rid, Patient::priorityOf(urg)});
            run("INSERT INTO distribution_transactions(request_id,patient_id,patient_name,medicine_id,medicine_name,pharmacy_id,quantity,status,notes,created_at) "
                "SELECT ?,p.patient_id,p.name,?,?,?,?,'Distributed','Seeded sample history',datetime('now',?) FROM patients p WHERE p.patient_id=?",
                {rid, h.at("medicine_id").integer(), h.at("name").str(), h.at("pharmacy_id").integer(), qty,
                 "-" + to_string(daysAgo) + " days", patient});
        }
        run("INSERT INTO notifications(patient_id,type,title,message) VALUES(1,'Info','Welcome','Welcome to the Medicine Distribution Priority System demo.')");
    }
    setSetting("seed_version", "1", "Demo data seed version");
    tx.commit();
}

bool Database::seedIfEmpty() {
    if (scalar("SELECT COUNT(*) FROM medicines") > 0) return false;
    seedDemoData();
    return true;
}
