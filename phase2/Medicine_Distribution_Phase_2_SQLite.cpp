#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <sqlite3.h>
using namespace std;

// ============================================================
//                         DATABASE
//                    PHASE 2 - SQLITE
// ============================================================

class Database {
    sqlite3* db = nullptr;

    string safe(string s) {
        string r;
        for(char c : s) {
            if(c == '\'') r += "''";
            else r += c;
        }
        return r;
    }

    void execute(string sql) {
        char* error = nullptr;
        if(sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
            if(error) {
                cerr << "SQLite Error: " << error << endl;
                sqlite3_free(error);
            }
        }
    }

public:
    bool open() {
        if(sqlite3_open("medicine_system_phase2.db", &db) != SQLITE_OK)
            return false;

        execute("CREATE TABLE IF NOT EXISTS patients("
                "patient_id INTEGER PRIMARY KEY,"
                "name TEXT, urgency TEXT, priority INTEGER,"
                "medicine TEXT, quantity INTEGER,"
                "wait_time INTEGER, status TEXT)");

        execute("CREATE TABLE IF NOT EXISTS medicines("
                "medicine_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "name TEXT UNIQUE, category TEXT, stock INTEGER,"
                "expiry TEXT, min_stock INTEGER)");

        execute("CREATE TABLE IF NOT EXISTS pharmacies("
                "pharmacy_id INTEGER PRIMARY KEY,"
                "name TEXT, location TEXT, distance REAL)");

        execute("CREATE TABLE IF NOT EXISTS pharmacy_stock("
                "pharmacy_id INTEGER, medicine TEXT, stock INTEGER,"
                "PRIMARY KEY(pharmacy_id, medicine))");

        execute("CREATE TABLE IF NOT EXISTS alternatives("
                "medicine TEXT, alternative TEXT,"
                "PRIMARY KEY(medicine, alternative))");

        execute("CREATE TABLE IF NOT EXISTS transactions("
                "transaction_id INTEGER PRIMARY KEY,"
                "patient_id INTEGER, patient_name TEXT,"
                "medicine TEXT, quantity INTEGER, status TEXT)");

        execute("CREATE TABLE IF NOT EXISTS waiting_list("
                "waiting_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "patient_id INTEGER, medicine TEXT,"
                "quantity INTEGER, status TEXT)");

        return true;
    }

    ~Database() {
        if(db) sqlite3_close(db);
    }

    void patient(int id,string name,string urgency,int priority,
                 string medicine,int quantity,int waitTime,string status) {
        execute("INSERT OR REPLACE INTO patients VALUES(" +
                to_string(id) + ",'" + safe(name) + "','" +
                safe(urgency) + "'," + to_string(priority) + ",'" +
                safe(medicine) + "'," + to_string(quantity) + "," +
                to_string(waitTime) + ",'" + safe(status) + "')");
    }

    void medicine(string name,string category,int stock,
                  string expiry,int minStock) {
        execute("INSERT OR REPLACE INTO medicines"
                "(name,category,stock,expiry,min_stock) VALUES('" +
                safe(name) + "','" + safe(category) + "'," +
                to_string(stock) + ",'" + safe(expiry) + "'," +
                to_string(minStock) + ")");
    }

    void pharmacy(int id,string name,string location,double distance) {
        execute("INSERT OR REPLACE INTO pharmacies VALUES(" +
                to_string(id) + ",'" + safe(name) + "','" +
                safe(location) + "'," + to_string(distance) + ")");
    }

    void pharmacyStock(int id,string medicine,int stock) {
        execute("INSERT OR REPLACE INTO pharmacy_stock VALUES(" +
                to_string(id) + ",'" + safe(medicine) + "'," +
                to_string(stock) + ")");
    }

    void alternative(string medicine,string alt) {
        execute("INSERT OR REPLACE INTO alternatives VALUES('" +
                safe(medicine) + "','" + safe(alt) + "')");
    }

    void transaction(int id,int patientId,string name,string medicine,
                     int quantity,string status) {
        execute("INSERT OR REPLACE INTO transactions VALUES(" +
                to_string(id) + "," + to_string(patientId) + ",'" +
                safe(name) + "','" + safe(medicine) + "'," +
                to_string(quantity) + ",'" + safe(status) + "')");
    }

    void waiting(int patientId,string medicine,int quantity,string status) {
        execute("INSERT INTO waiting_list"
                "(patient_id,medicine,quantity,status) VALUES(" +
                to_string(patientId) + ",'" + safe(medicine) + "'," +
                to_string(quantity) + ",'" + safe(status) + "')");
    }

    void updateStock(string medicine,int stock) {
        execute("UPDATE medicines SET stock=" + to_string(stock) +
                " WHERE name='" + safe(medicine) + "'");
    }

    void updatePatient(int id,string status) {
        execute("UPDATE patients SET status='" + safe(status) +
                "' WHERE patient_id=" + to_string(id));
    }

    void summary() {
        cout << "\n----- SQLITE DATABASE SUMMARY -----\n";
        string tables[]={"patients","medicines","pharmacies",
                         "pharmacy_stock","alternatives",
                         "transactions","waiting_list"};

        for(string table:tables) {
            sqlite3_stmt* stmt;
            string sql="SELECT COUNT(*) FROM " + table;
            int count=0;

            if(sqlite3_prepare_v2(db,sql.c_str(),-1,&stmt,nullptr)==SQLITE_OK) {
                if(sqlite3_step(stmt)==SQLITE_ROW)
                    count=sqlite3_column_int(stmt,0);
                sqlite3_finalize(stmt);
            }

            cout << table << " : " << count << endl;
        }
    }
};


// ============================================================
//                         NIKITA
//              PATIENT MANAGEMENT & PRIORITY
// ============================================================

class Patient {
public:
    int id, priority, quantity, waitTime;
    string name, urgency, medicine, status;

    Patient(int i,string n,string u,string m,int q) {
        id=i;
        name=n;
        urgency=u;
        medicine=m;
        quantity=q;
        waitTime=0;
        status="Waiting";

        if(u=="Critical") priority=1;
        else if(u=="High") priority=2;
        else if(u=="Medium") priority=3;
        else priority=4;
    }

    void show() {
        cout << id << " | " << name << " | "
             << urgency << " | " << medicine << " | "
             << quantity << " | " << priority << " | "
             << status << endl;
    }

    void increaseWait() { waitTime++; }

    void updateUrgency(string u) {
        urgency=u;

        if(u=="Critical") priority=1;
        else if(u=="High") priority=2;
        else if(u=="Medium") priority=3;
        else priority=4;
    }

    void updateMedicine(string m,int q) {
        medicine=m;
        quantity=q;
    }

    void changeStatus(string s) { status=s; }

    void showDetails() {
        cout << "\nPatient ID   : " << id;
        cout << "\nName         : " << name;
        cout << "\nUrgency      : " << urgency;
        cout << "\nPriority     : " << priority;
        cout << "\nMedicine     : " << medicine;
        cout << "\nQuantity     : " << quantity;
        cout << "\nWaiting Time : " << waitTime;
        cout << "\nStatus       : " << status << endl;
    }
};

struct Compare {
    bool operator()(Patient a,Patient b) {
        if(a.priority!=b.priority)
            return a.priority>b.priority;
        return a.waitTime<b.waitTime;
    }
};


// ============================================================
//                         VANSH
//                  MEDICINE & INVENTORY
// ============================================================

class Medicine {
public:
    string name,category,expiry;
    int stock,minStock;
    vector<string> alternatives;

    Medicine() {}

    Medicine(string n,string c,int s,string e,int minS) {
        name=n;
        category=c;
        stock=s;
        expiry=e;
        minStock=minS;
    }

    bool available(int q) { return stock>=q; }

    void removeStock(int q) {
        if(available(q)) stock-=q;
    }

    void addStock(int q) { stock+=q; }

    void show() {
        cout << name << " | " << category
             << " | Stock: " << stock
             << " | Expiry: " << expiry << endl;
    }

    void checkStock() {
        if(stock<=minStock)
            cout << "LOW STOCK: " << name << endl;
    }

    void addAlternative(string m) {
        alternatives.push_back(m);
    }

    void showAlternatives() {
        cout << "Alternatives: ";

        if(alternatives.empty())
            cout << "None";
        else
            for(auto &x:alternatives)
                cout << x << " ";

        cout << endl;
    }

    bool isLowStock() {
        return stock<=minStock;
    }

    void showFullDetails() {
        cout << "\nMedicine Name : " << name;
        cout << "\nCategory      : " << category;
        cout << "\nStock         : " << stock;
        cout << "\nMinimum Stock : " << minStock;
        cout << "\nExpiry        : " << expiry << endl;
        showAlternatives();
    }
};


// ============================================================
//                         KRISHNA
//                PHARMACY & ALTERNATIVES
// ============================================================

class Pharmacy {
public:
    int id;
    string name,location;
    double distance;
    unordered_map<string,int> stock;

    Pharmacy(int i,string n,string l,double d) {
        id=i;
        name=n;
        location=l;
        distance=d;
    }

    void addMedicine(string m,int q) {
        stock[m]=q;
    }

    bool hasMedicine(string m) {
        return stock.find(m)!=stock.end() && stock[m]>0;
    }

    int getStock(string m) {
        if(stock.find(m)!=stock.end())
            return stock[m];
        return 0;
    }

    void removeMedicine(string m,int q) {
        if(hasMedicine(m) && stock[m]>=q)
            stock[m]-=q;
    }

    void show() {
        cout << id << " | " << name << " | "
             << location << " | " << distance << " km" << endl;
    }

    void showMedicineStock() {
        cout << "\n" << name << " stock:\n";
        for(auto &x:stock)
            cout << x.first << " : " << x.second << endl;
    }
};

void findPharmacy(vector<Pharmacy>& pharmacies,string medicine) {
    sort(pharmacies.begin(),pharmacies.end(),
         [](Pharmacy a,Pharmacy b) {
             return a.distance<b.distance;
         });

    bool found=false;

    cout << "\nPharmacies for " << medicine << ":\n";

    for(auto &p:pharmacies) {
        if(p.hasMedicine(medicine)) {
            cout << p.name << " - "
                 << p.distance << " km - Stock: "
                 << p.getStock(medicine) << endl;
            found=true;
        }
    }

    if(!found) cout << "No pharmacy found.\n";
}

void findAlternative(unordered_map<string,Medicine>& inventory,
                     string medicine) {
    if(inventory.find(medicine)==inventory.end()) {
        cout << "Medicine not found.\n";
        return;
    }

    cout << "\nAlternatives for " << medicine << ":\n";
    inventory[medicine].showAlternatives();
}


// ============================================================
//                         ANKIT
//              DISTRIBUTION & TRANSACTIONS
// ============================================================

class Transaction {
public:
    int transactionId,patientId,quantity;
    string patientName,medicine,status;

    Transaction(int tid,int id,string n,string m,int q,string s) {
        transactionId=tid;
        patientId=id;
        patientName=n;
        medicine=m;
        quantity=q;
        status=s;
    }

    void show() {
        cout << transactionId << " | "
             << patientId << " | "
             << patientName << " | "
             << medicine << " | "
             << quantity << " | "
             << status << endl;
    }
};

Transaction makeTransaction(int tid,Patient p,string status) {
    return Transaction(tid,p.id,p.name,p.medicine,p.quantity,status);
}

bool checkDistribution(Patient p,
                       unordered_map<string,Medicine>& inventory) {
    if(inventory.find(p.medicine)==inventory.end()) {
        cout << "Medicine unavailable.\n";
        return false;
    }

    if(!inventory[p.medicine].available(p.quantity)) {
        cout << "Insufficient stock.\n";
        return false;
    }

    return true;
}


// ============================================================
//                         SHAURYA
//                   DSA & DATA MANAGEMENT
// ============================================================

int searchPatient(vector<Patient>& patients,int id) {
    for(int i=0;i<patients.size();i++)
        if(patients[i].id==id)
            return i;

    return -1;
}

bool searchMedicine(unordered_map<string,Medicine>& inventory,
                    string name) {
    return inventory.find(name)!=inventory.end();
}

void sortPatients(vector<Patient>& patients) {
    sort(patients.begin(),patients.end(),
         [](Patient a,Patient b) {
             return a.priority<b.priority;
         });
}

void sortByWaitingTime(vector<Patient>& patients) {
    sort(patients.begin(),patients.end(),
         [](Patient a,Patient b) {
             return a.waitTime>b.waitTime;
         });
}

void demandReport(vector<Patient>& patients) {
    unordered_map<string,int> demand;

    for(auto &p:patients)
        demand[p.medicine]++;

    cout << "\n----- MEDICINE DEMAND -----\n";

    for(auto &x:demand)
        cout << x.first << " : "
             << x.second << " requests\n";
}

void priorityReport(vector<Patient>& patients) {
    int critical=0,high=0,medium=0,normal=0;

    for(auto &p:patients) {
        if(p.urgency=="Critical") critical++;
        else if(p.urgency=="High") high++;
        else if(p.urgency=="Medium") medium++;
        else normal++;
    }

    cout << "\n----- PRIORITY REPORT -----\n";
    cout << "Critical : " << critical << endl;
    cout << "High     : " << high << endl;
    cout << "Medium   : " << medium << endl;
    cout << "Normal   : " << normal << endl;
}

void filterUrgency(vector<Patient>& patients,string urgency) {
    cout << "\nPatients with " << urgency << " urgency:\n";

    for(auto &p:patients)
        if(p.urgency==urgency)
            p.show();
}


// ============================================================
//                         SAKSHAM
//             OOP ARCHITECTURE & INTEGRATION
// ============================================================

class MedicineSystem {
public:
    vector<Patient> patients;
    vector<Pharmacy> pharmacies;
    vector<Transaction> transactions;
    vector<Patient> waitingList;

    priority_queue<Patient,vector<Patient>,Compare> pq;
    unordered_map<string,Medicine> inventory;

    Database& database;

    MedicineSystem(Database& db):database(db) {}

    void addPatient(Patient p) {
        patients.push_back(p);
        pq.push(p);

        database.patient(
            p.id,p.name,p.urgency,p.priority,
            p.medicine,p.quantity,p.waitTime,p.status
        );
    }

    void addMedicine(Medicine m) {
        inventory[m.name]=m;

        database.medicine(
            m.name,m.category,m.stock,
            m.expiry,m.minStock
        );

        for(auto &a:m.alternatives)
            database.alternative(m.name,a);
    }

    void addPharmacy(Pharmacy p) {
        pharmacies.push_back(p);

        database.pharmacy(
            p.id,p.name,p.location,p.distance
        );

        for(auto &x:p.stock)
            database.pharmacyStock(
                p.id,x.first,x.second
            );
    }

    void addWaiting(Patient p) {
        p.status="Waiting";
        waitingList.push_back(p);

        database.waiting(
            p.id,p.medicine,p.quantity,p.status
        );

        cout << p.name
             << " added to waiting list.\n";
    }

    void distribute() {
        if(pq.empty()) return;

        Patient p=pq.top();
        pq.pop();

        cout << "\nProcessing: " << p.name << endl;
        cout << "Priority: " << p.priority << endl;

        if(inventory.find(p.medicine)==inventory.end()) {
            cout << "Medicine unavailable.\n";
            addWaiting(p);

            transactions.push_back(
                makeTransaction(
                    transactions.size()+1,p,"Waiting"
                )
            );

            database.transaction(
                transactions.back().transactionId,
                p.id,p.name,p.medicine,
                p.quantity,"Waiting"
            );

            return;
        }

        Medicine &med=inventory[p.medicine];

        if(med.available(p.quantity)) {
            med.removeStock(p.quantity);
            p.status="Distributed";

            cout << "Medicine distributed!\n";
            cout << "Remaining stock: "
                 << med.stock << endl;

            transactions.push_back(
                makeTransaction(
                    transactions.size()+1,p,"Distributed"
                )
            );

            database.transaction(
                transactions.back().transactionId,
                p.id,p.name,p.medicine,
                p.quantity,"Distributed"
            );

            database.updateStock(
                p.medicine,med.stock
            );

            database.updatePatient(
                p.id,"Distributed"
            );

            med.checkStock();
        }
        else {
            cout << "Insufficient stock.\n";
            med.showAlternatives();

            addWaiting(p);

            transactions.push_back(
                makeTransaction(
                    transactions.size()+1,p,"Waiting"
                )
            );

            database.transaction(
                transactions.back().transactionId,
                p.id,p.name,p.medicine,
                p.quantity,"Waiting"
            );
        }
    }

    void restock(string medicine,int quantity) {
        if(inventory.find(medicine)!=inventory.end()) {
            inventory[medicine].addStock(quantity);

            cout << "\nStock updated for "
                 << medicine << endl;

            cout << "New stock: "
                 << inventory[medicine].stock << endl;

            database.updateStock(
                medicine,
                inventory[medicine].stock
            );
        }
    }

    void checkWaiting(string medicine) {
        if(inventory.find(medicine)==inventory.end())
            return;

        for(auto &p:waitingList) {
            if(p.medicine==medicine &&
               inventory[medicine].available(p.quantity)) {

                p.status="Medicine Available";

                cout << "\n[NOTIFICATION]\n";
                cout << p.name << ", your medicine "
                     << medicine
                     << " is now available.\n";
            }
        }
    }

    void reorderCheck() {
        cout << "\n----- REORDER CHECK -----\n";

        for(auto &x:inventory)
            if(x.second.isLowStock())
                cout << "Reorder required: "
                     << x.first << endl;
    }

    void expiryCheck() {
        cout << "\n----- EXPIRY MONITORING -----\n";

        for(auto &x:inventory)
            cout << x.first
                 << " | Expiry: "
                 << x.second.expiry << endl;
    }

    void showWaiting() {
        cout << "\n----- WAITING LIST -----\n";

        for(auto &p:waitingList)
            cout << p.id << " | "
                 << p.name << " | "
                 << p.medicine << " | "
                 << p.quantity << " | "
                 << p.status << endl;
    }

    void dashboard() {
        int distributed=0;

        for(auto &t:transactions)
            if(t.status=="Distributed")
                distributed++;

        cout << "\n================================\n";
        cout << "        ADMIN DASHBOARD\n";
        cout << "================================\n";
        cout << "Patients    : " << patients.size() << endl;
        cout << "Waiting     : " << waitingList.size() << endl;
        cout << "Distributed : " << distributed << endl;
        cout << "Medicines   : " << inventory.size() << endl;
        cout << "Pharmacies  : " << pharmacies.size() << endl;
        cout << "================================\n";
    }
};


// ============================================================
//                           MAIN
// ============================================================

int main() {

    Database database;

    if(!database.open()) {
        cout << "Database connection failed.\n";
        return 1;
    }

    MedicineSystem system(database);

    // ---------------- NIKITA ----------------

    system.addPatient(
        Patient(101,"Rahul","Critical","Insulin",2)
    );

    system.addPatient(
        Patient(102,"Ankit","Medium","Paracetamol",5)
    );

    system.addPatient(
        Patient(103,"Priya","High","Insulin",1)
    );

    system.addPatient(
        Patient(104,"Riya","Normal","Amoxicillin",20)
    );

    cout << "\n----- PATIENT MANAGEMENT -----\n";

    for(auto &p:system.patients)
        p.show();

    system.patients[0].increaseWait();

    database.patient(
        system.patients[0].id,
        system.patients[0].name,
        system.patients[0].urgency,
        system.patients[0].priority,
        system.patients[0].medicine,
        system.patients[0].quantity,
        system.patients[0].waitTime,
        system.patients[0].status
    );

    // ---------------- VANSH ----------------

    Medicine insulin(
        "Insulin","Diabetes",20,
        "2027-06-30",5
    );
    insulin.addAlternative("Glargine");

    Medicine paracetamol(
        "Paracetamol","Fever/Pain",50,
        "2027-08-15",10
    );
    paracetamol.addAlternative("Ibuprofen");

    Medicine amoxicillin(
        "Amoxicillin","Antibiotic",15,
        "2027-04-20",5
    );
    amoxicillin.addAlternative("Azithromycin");

    system.addMedicine(insulin);
    system.addMedicine(paracetamol);
    system.addMedicine(amoxicillin);

    cout << "\n----- INVENTORY -----\n";

    for(auto &x:system.inventory)
        x.second.show();

    // ---------------- KRISHNA ----------------

    Pharmacy p1(
        1,"City Pharmacy",
        "Main Road",1.5
    );

    p1.addMedicine("Insulin",5);
    p1.addMedicine("Paracetamol",20);

    Pharmacy p2(
        2,"Health Care Pharmacy",
        "Station Road",2.8
    );

    p2.addMedicine("Insulin",10);
    p2.addMedicine("Amoxicillin",5);

    system.addPharmacy(p1);
    system.addPharmacy(p2);

    findPharmacy(
        system.pharmacies,
        "Insulin"
    );

    findAlternative(
        system.inventory,
        "Insulin"
    );

    // ---------------- SHAURYA ----------------

    cout << "\n----- DSA OPERATIONS -----\n";

    int pos=searchPatient(
        system.patients,102
    );

    if(pos!=-1)
        cout << "Patient found: "
             << system.patients[pos].name
             << endl;

    if(searchMedicine(
        system.inventory,"Insulin"))
        cout << "Insulin found using Hash Map.\n";

    sortPatients(system.patients);

    demandReport(system.patients);
    priorityReport(system.patients);
    filterUrgency(system.patients,"Critical");

    // ---------------- ANKIT ----------------

    cout << "\n----- MEDICINE DISTRIBUTION -----\n";

    while(!system.pq.empty())
        system.distribute();

    cout << "\n----- TRANSACTIONS -----\n";

    for(auto &t:system.transactions)
        t.show();

    // ---------------- VANSH RESTOCK ----------------

    cout << "\n----- RESTOCK -----\n";

    system.restock(
        "Amoxicillin",20
    );

    // ---------------- SAKSHAM ----------------

    system.showWaiting();

    cout << "\n----- NOTIFICATION -----\n";

    system.checkWaiting(
        "Amoxicillin"
    );

    system.reorderCheck();
    system.expiryCheck();

    system.dashboard();

    database.summary();

    cout << "\n============================================\n";
    cout << "       MEDICINE DISTRIBUTION SYSTEM\n";
    cout << "              PHASE 2 COMPLETE\n";
    cout << "============================================\n";

    return 0;
}
