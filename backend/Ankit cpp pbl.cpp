// ============================================================
//                         ANKIT
//              DISTRIBUTION & TRANSACTIONS
// ============================================================

class Transaction {
public:
    int transactionId,patientId,quantity;
    string patientName,medicine,status;
    Transaction(int tid,int id,string n,string m,int q,string s)
        : transactionId(tid),patientId(id),quantity(q),patientName(n),medicine(m),status(s) {}
    void show() const {
        cout << transactionId << " | " << patientId << " | " << patientName
             << " | " << medicine << " | " << quantity << " | " << status << endl;
    }
};

Transaction makeTransaction(int tid,Patient p,string status) {
    return Transaction(tid,p.id,p.name,p.medicine,p.quantity,status);
}

// ---------------- Database: transactions, notifications & summary ----------------

long long Database::transaction(long long requestId,long long patientId,const string& patientName,long long medicineId,
                                const string& medicineName,long long pharmacyId,int quantity,const string& status,
                                long long userId,const string& notes) {
    run("INSERT INTO distribution_transactions(request_id,patient_id,patient_name,medicine_id,medicine_name,pharmacy_id,"
        "quantity,status,performed_by,notes) VALUES(?,?,?,?,?,?,?,?,?,?)",
        {requestId,patientId,patientName,medicineId,medicineName,
         pharmacyId>0?Val(pharmacyId):Val(),quantity,status,userId>0?Val(userId):Val(),notes});
    return lastId();
}

long long Database::notify(long long userId,long long patientId,const string& role,const string& type,
                           const string& title,const string& message) {
    run("INSERT INTO notifications(user_id,patient_id,target_role,type,title,message) VALUES(?,?,?,?,?,?)",
        {userId>0?Val(userId):Val(),patientId>0?Val(patientId):Val(),role.empty()?Val():Val(role),type,title,message});
    return lastId();
}

void Database::showTransactions() {
    cout << "\n----- TRANSACTIONS -----\n";
    Json rows = query("SELECT transaction_id,patient_id,patient_name,medicine_name,quantity,status "
                      "FROM distribution_transactions ORDER BY transaction_id");
    for(const auto& r : rows.items())
        cout << r.at("transaction_id").integer() << " | " << r.at("patient_id").integer() << " | "
             << r.at("patient_name").str() << " | " << r.at("medicine_name").str() << " | "
             << r.at("quantity").integer() << " | " << r.at("status").str() << endl;
}

void Database::summary() {
    cout << "\n----- SQLITE DATABASE SUMMARY -----\n";
    Json tables = query("SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name");
    for(const auto& t : tables.items()) {
        string table=t.at("name").str();
        cout << table << " : " << scalar("SELECT COUNT(*) FROM "+table) << endl;
    }
}

// Largest quantity of each medicine available (unexpired) in a single open pharmacy.
static unordered_map<int,int> maxAvailability(Database& db) {
    unordered_map<int,int> out;
    Json rows = db.query("SELECT medicine_id,MAX(q) mx FROM (SELECT b.medicine_id,b.pharmacy_id,SUM(b.quantity) q FROM medicine_batches b "
                         "JOIN pharmacies p ON p.pharmacy_id=b.pharmacy_id WHERE b.expiry_date>=date('now') AND p.status='Open' "
                         "GROUP BY b.medicine_id,b.pharmacy_id) GROUP BY medicine_id");
    for(const auto& r : rows.items()) out[(int)r.at("medicine_id").integer()]=(int)r.at("mx").integer();
    return out;
}
