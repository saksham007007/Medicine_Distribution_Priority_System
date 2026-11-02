#include <iostream>
#include <vector>
#include <queue>
#include <unordered_map>
#include <algorithm>
using namespace std;


// ================= NIKITA - ROLE 1 =================
// Patient Management & Priority System

class Patient {
public:
    int id, priority, quantity;
    string name, urgency, medicine, status;

    Patient(int i, string n, string u, string m, int q) {

        id=i; 
        name=n;
        urgency=u;
        medicine=m;
        quantity=q;

        status="Waiting";

        if(u=="Critical") 
        {
            priority=1;
        }
        else if(u=="High")
        {
            priority=2;
        }
        else if(u=="Medium")
        {
             priority=3;
        }
        else 
        {
            priority=4;
        }
    }

    void show() {
        cout << id << " " << name << " " << urgency
             << " " << medicine << " " << status << endl;
    }
};

struct Compare {
    bool operator()(Patient a, Patient b) {
        return a.priority > b.priority;
    }
};


// ================= VANSH - ROLE 2 =================
// Medicine & Inventory Management

class Medicine {
public:
    string name;
    int stock;
    vector<string> alternatives;

    Medicine() {}
    Medicine(string n, int s) {
        name=n; stock=s;
    }

    bool available(int q) {
        return stock >= q;
    }

    void removeStock(int q) {
        stock -= q;
    }
};


// ================= KRISHNA - ROLE 3 =================
// Pharmacy & Alternative Medicine System

class Pharmacy {
public:
    string name, location;
    double distance;
    unordered_map<string,int> stock;

    Pharmacy(string n, string l, double d) {
        name=n; location=l; distance=d;
    }

    void addMedicine(string m, int q) {
        stock[m]=q;
    }

    bool hasMedicine(string m) {
        return stock.find(m)!=stock.end() && stock[m]>0;
    }
};

void findPharmacy(vector<Pharmacy>& p, string medicine) {
    for(auto &x:p)
        if(x.hasMedicine(medicine))
            cout << x.name << " - " << x.distance << " km\n";
}


// ================= ANKIT - ROLE 4 =================
// Medicine Distribution & Transaction System

void distribute(
    priority_queue<Patient,vector<Patient>,Compare>& pq,
    unordered_map<string,Medicine>& inventory) {

    if(pq.empty()) return;

    Patient p=pq.top();
    pq.pop();

    cout << "\nProcessing: " << p.name << endl;

    if(inventory.find(p.medicine)==inventory.end()) {
        cout << "Medicine unavailable\n";
        return;
    }

    if(inventory[p.medicine].available(p.quantity)) {
        inventory[p.medicine].removeStock(p.quantity);
        p.status="Distributed";
        cout << "Medicine distributed successfully\n";
    }
    else {
        cout << "Insufficient stock\n";
    }
}


// ================= SHAURYA - ROLE 5 =================
// DSA Optimization & Data Management

int searchPatient(vector<Patient>& patients, int id) {
    for(int i=0;i<patients.size();i++)
        if(patients[i].id==id) return i;
    return -1;
}

void sortPatients(vector<Patient>& patients) {
    sort(patients.begin(),patients.end(),
        [](Patient a,Patient b) {
            return a.priority < b.priority;
        });
}


// ================= SAKSHAM - ROLE 6 =================
// OOP Architecture & System Integration

class MedicineSystem {
public:
    vector<Patient> patients;
    vector<Pharmacy> pharmacies;
    priority_queue<Patient,vector<Patient>,Compare> pq;
    unordered_map<string,Medicine> inventory;

    void addPatient(Patient p) {
        patients.push_back(p);
        pq.push(p);
    }

    void addMedicine(string n,int s) {
        inventory[n]=Medicine(n,s);
    }
};


// ================= MAIN - SAKSHAM =================

int main() {

    MedicineSystem system;

    // medicines
    system.addMedicine("Insulin",20);
    system.addMedicine("Paracetamol",50);
    system.inventory["Insulin"].alternatives.push_back("Glargine");

    // patients
    system.addPatient(Patient(101,"Rahul","Critical","Insulin",2));
    system.addPatient(Patient(102,"Ankit","Medium","Paracetamol",5));
    system.addPatient(Patient(103,"Priya","High","Insulin",1));

    cout << "MEDICINE DISTRIBUTION PRIORITY SYSTEM\n";

    // show patients
    for(auto &p:system.patients)
        p.show();

    // searching
    int pos=searchPatient(system.patients,102);
    if(pos!=-1)
        cout << "\nPatient found: "
             << system.patients[pos].name << endl;

    // sorting
    sortPatients(system.patients);

    // distribution
    cout << "\nDistribution:\n";
    while(!system.pq.empty())
        distribute(system.pq,system.inventory);

    // pharmacy
    Pharmacy p1("City Pharmacy","Main Road",1.5);
    p1.addMedicine("Insulin",5);
    system.pharmacies.push_back(p1);

    cout << "\nNearby pharmacies:\n";
    findPharmacy(system.pharmacies,"Insulin");

    return 0;
}