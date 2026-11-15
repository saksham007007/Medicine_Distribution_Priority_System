// ============================================================
//                         SAKSHAM
//             OOP ARCHITECTURE & INTEGRATION
// ============================================================
// Single translation unit. Inclusion order (no circular dependencies):
//   1. Infrastructure headers + Database class declaration (this file)
//   2. Member data classes and their Database functions
//   3. MedicineSystem service class (this file)
//   4. Member API files: service-method definitions + REST routes
#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <string>
#include <limits>
#include <map>
#include <set>
#include <memory>
#include <random>
#include <mutex>
#include <sqlite3.h>
#include "include/json.h"
#include "include/util.h"
#include "include/crypto.h"
#include "include/http_server.h"
#include "include/static_files.h"
#include "include/schema.h"
using namespace std;

// SQL parameter value (null / integer / real / text).
struct Val {
    enum T { Null, Int, Real, Text } t = Null;
    long long i = 0; double d = 0; string s;
    Val() {}
    Val(nullptr_t) {}
    template<class I, typename enable_if<is_integral<I>::value, int>::type = 0> Val(I v) : t(Int), i((long long)v) {}
    Val(double v) : t(Real), d(v) {}
    Val(const char* v) : t(Text), s(v ? v : "") {}
    Val(const string& v) : t(Text), s(v) {}
    static Val optText(const Json& j) { return j.isString() ? Val(j.str()) : Val(); }
    static Val optNum(const Json& j) { return j.isNumber() ? Val(j.num()) : Val(); }
};

// Authenticated caller (resolved from the bearer token on every request).
struct Auth {
    long long userId = 0, patientId = 0, pharmacyId = 0;
    string username, role;
    bool is(const string& r) const { return role == r; }
    bool staff() const { return role == "Admin" || role == "Pharmacist"; }
};

class Patient; class Medicine; class Pharmacy;

// Shared Database interface. Each function is defined in the file of the
// member responsible for it (see docs/CONTRIBUTIONS.md).
class Database {
    sqlite3* db = nullptr;
    void bindAll(sqlite3_stmt* st, const vector<Val>& p);
    [[noreturn]] void raise(int rc, const string& sql);
public:
    ~Database();                                                     // Shaurya
    bool open(const string& path);                                   // Shaurya
    void initSchema();                                               // Shaurya
    void exec(const string& sql);                                    // Shaurya
    Json query(const string& sql, const vector<Val>& p = {});        // Shaurya
    Json one(const string& sql, const vector<Val>& p = {});          // Shaurya
    long long scalar(const string& sql, const vector<Val>& p = {}, long long def = 0); // Shaurya
    long long run(const string& sql, const vector<Val>& p = {});     // Shaurya
    long long lastId() { return sqlite3_last_insert_rowid(db); }
    void begin(); void commit(); void rollback();                    // Shaurya
    string setting(const string& key, const string& def = "");       // Shaurya
    void setSetting(const string& key, const string& value, const string& desc = ""); // Shaurya
    void audit(const Auth& by, const string& action, const string& entity, const string& id, const string& details = ""); // Shaurya
    bool seedIfEmpty(); void seedDemoData();                         // Shaurya
    sqlite3* get() { return db; }

    // Nikita: patients, requests, priority queue
    long long patient(const Patient& p, const string& contact, const string& address); // Nikita
    void loadPatients(vector<Patient>& pending);                     // Nikita (pending queue entries)
    void loadWaitingList(vector<Patient>& waiting);                  // Nikita
    void setRequestStatus(long long requestId, const string& status, const string& queueStatus); // Nikita
    // Vansh: medicines and stock
    void loadMedicines(unordered_map<string, Medicine>& inventory);  // Vansh
    // Krishna: pharmacies and alternatives
    void loadPharmacies(vector<Pharmacy>& pharmacies);               // Krishna
    void loadAlternatives(unordered_map<string, Medicine>& inventory); // Krishna
    // Ankit: transactions and notifications
    long long transaction(long long requestId, long long patientId, const string& patientName, long long medicineId,
                          const string& medicineName, long long pharmacyId, int quantity, const string& status,
                          long long userId, const string& notes); // Ankit
    void showTransactions();                                         // Ankit
    void summary();                                                  // Ankit
    long long notify(long long userId, long long patientId, const string& role, const string& type,
                     const string& title, const string& message);    // Ankit
};

// RAII database transaction: rolls back unless commit() is called.
struct Tx {
    Database& d; bool done = false;
    explicit Tx(Database& db) : d(db) { d.begin(); }
    void commit() { d.commit(); done = true; }
    ~Tx() { if (!done) { try { d.rollback(); } catch (...) {} } }
};

#include "nikita.cpp"
#include "vansh.cpp"
#include "krishna.cpp"
#include "ankit.cpp"
#include "shaurya.cpp"

// ------------------------------------------------------------
// MedicineSystem: service layer. Holds the in-memory DSA structures
// (priority queue, hash-map inventory, pharmacy vector) and keeps them
// synchronised with SQLite, which is the source of truth.
// ------------------------------------------------------------
class MedicineSystem {
public:
    Database& database;
    vector<Pharmacy> pharmacies;
    vector<Patient> waitingList;
    priority_queue<Patient, vector<Patient>, Compare> pq;
    unordered_map<string, Medicine> inventory;      // medicine name -> Medicine (hash map)
    unordered_map<int, string> medicineName;        // medicine id -> name

    explicit MedicineSystem(Database& db) : database(db) {}

    void loadMasterData() {
        reloadStock();
        reloadQueue();
    }
    void reloadStock() {
        inventory.clear(); pharmacies.clear(); medicineName.clear();
        database.loadMedicines(inventory);
        database.loadAlternatives(inventory);
        database.loadPharmacies(pharmacies);
        for (auto& x : inventory) medicineName[x.second.id] = x.first;
    }
    void reloadQueue() {
        waitingList.clear();
        pq = priority_queue<Patient, vector<Patient>, Compare>();
        vector<Patient> pending;
        database.loadPatients(pending);
        for (const auto& p : pending) pq.push(p);
        database.loadWaitingList(waitingList);
    }
    // Pending requests in processing order (non-destructive copy of the queue).
    vector<Patient> orderedQueue() const {
        auto copy = pq; vector<Patient> out;
        while (!copy.empty()) { out.push_back(copy.top()); copy.pop(); }
        return out;
    }

    // ---- Nikita: patients, requests, priority queue ----
    long long createPatient(const Json& d);
    void updatePatient(long long id, const Json& d);
    Json createRequest(long long patientId, int medicineId, int quantity, const string& urgency,
                       long long prescriptionId, const string& notes, const Auth& by);
    void changeUrgency(long long requestId, const string& urgency, const string& reason, const Auth& by);
    void cancelRequest(long long requestId, const Auth& by);
    Json queueSnapshot(long long onlyPatientId = 0);
    // ---- Vansh: medicines & inventory ----
    Json restock(long long pharmacyId, long long medicineId, int qty, const string& batch,
                 const string& mfg, const string& exp, const Auth& by);
    long long writeOffExpired(long long pharmacyId, const Auth& by);
    Json listMedicines(const HttpRequest& req);
    // ---- Krishna: pharmacies & alternatives ----
    Json searchPharmacies(long long medicineId, double lat, double lon, bool hasCoords, int minQty);
    Json alternativesFor(long long medicineId);
    void proposeSubstitution(long long requestId, long long altId, const Auth& by);
    void decideSubstitution(long long requestId, bool approve, const string& note, const Auth& by);
    // ---- Ankit: distribution, waiting list, transactions ----
    Json recommend(long long requestId);
    Json evaluateNext(const Auth& by);
    Json distribute(long long requestId, long long pharmacyId, const Auth& by);
    int promoteWaiting();
    // ---- Shaurya: analytics ----
    Json analyticsSummary();
};

#include "saksham_api.cpp"
#include "nikita_api.cpp"
#include "vansh_api.cpp"
#include "krishna_api.cpp"
#include "ankit_api.cpp"
#include "shaurya_api.cpp"

// Registers every module's routes on the router.
inline void buildRoutes(Router& r, MedicineSystem& sys) {
    registerAuthRoutes(r, sys);          // Saksham
    registerPatientRoutes(r, sys);       // Nikita
    registerMedicineRoutes(r, sys);      // Vansh
    registerPharmacyRoutes(r, sys);      // Krishna
    registerDistributionRoutes(r, sys);  // Ankit
    registerAnalyticsRoutes(r, sys);     // Shaurya
}
