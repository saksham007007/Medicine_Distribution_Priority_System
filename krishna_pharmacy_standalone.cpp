// ============================================================
//                         KRISHNA
//                PHARMACY & ALTERNATIVES (STANDALONE)
// Team leader ke project se nikala gaya module.
// SQLite ki zaroorat nahi: data seed_database.sql se yahin hardcode hai.
// Compile: g++ -std=c++17 krishna_pharmacy_standalone.cpp -o pharmacy
// ============================================================

#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <limits>
using namespace std;

// ---------------- Pharmacy class (same as team project) ----------------
class Pharmacy {
public:
    int id;
    string name, location;
    double distance;
    unordered_map<string,int> stock;

    Pharmacy(int i, string n, string l, double d)
        : id(i), name(n), location(l), distance(d) {}

    void addMedicine(string m, int q) { stock[m] = q; }

    bool hasMedicine(string m) const {
        auto it = stock.find(m);
        return it != stock.end() && it->second > 0;
    }

    int getStock(string m) const {
        auto it = stock.find(m);
        return it != stock.end() ? it->second : 0;
    }

    void removeMedicine(string m, int q) {
        if (hasMedicine(m) && stock[m] >= q) stock[m] -= q;
    }

    void show() const {
        cout << id << " | " << name << " | " << location
             << " | " << distance << " km" << endl;
    }

    void showMedicineStock() const {
        cout << "\n" << name << " stock:\n";
        for (const auto& x : stock) cout << x.first << " : " << x.second << endl;
    }
};

// ---------------- Functions (same logic as team project) ----------------
void findPharmacy(vector<Pharmacy>& pharmacies, string medicine) {
    sort(pharmacies.begin(), pharmacies.end(),
         [](const Pharmacy& a, const Pharmacy& b) { return a.distance < b.distance; });
    bool found = false;
    cout << "\nPharmacies for " << medicine << ":\n";
    for (const auto& p : pharmacies) {
        if (p.hasMedicine(medicine)) {
            cout << p.name << " - " << p.distance << " km - Stock: "
                 << p.getStock(medicine) << endl;
            found = true;
        }
    }
    if (!found) cout << "No pharmacy found.\n";
}

// Team project mein alternatives Medicine class ke andar hain.
// Yahan simple unordered_map use kiya hai taaki module alag se chal sake.
void findAlternative(unordered_map<string, vector<string>>& alts, string medicine) {
    auto it = alts.find(medicine);
    cout << "\nAlternatives for " << medicine << ":\n";
    if (it == alts.end() || it->second.empty()) {
        cout << "Alternatives: None\n";
        return;
    }
    cout << "Alternatives: ";
    for (const auto& a : it->second) cout << a << " ";
    cout << endl;
}

// ---------------- Sample data (seed_database.sql se) ----------------
void loadSampleData(vector<Pharmacy>& ph, unordered_map<string, vector<string>>& alts) {
    ph.emplace_back(1, "City Pharmacy", "Main Road", 1.5);
    ph.emplace_back(2, "Health Care Pharmacy", "Station Road", 2.8);
    ph.emplace_back(3, "MediPlus Pharmacy", "Civil Lines", 3.2);
    ph.emplace_back(4, "Apollo Care Pharmacy", "University Road", 4.1);
    ph.emplace_back(5, "Wellness Pharmacy", "Market Road", 4.8);
    ph.emplace_back(6, "LifeLine Pharmacy", "Bus Stand Road", 5.4);
    ph.emplace_back(7, "Arogya Pharmacy", "Medical College Road", 6.1);
    ph.emplace_back(8, "Green Cross Pharmacy", "Shastri Nagar", 6.8);
    ph.emplace_back(9, "TrustMed Pharmacy", "Delhi Road", 7.5);
    ph.emplace_back(10, "Jan Aushadhi Pharmacy", "Hospital Road", 8.2);

    struct S { int pid; const char* med; int qty; };
    S stocks[] = {
        {1,"Insulin",20},{1,"Paracetamol",30},{1,"Ibuprofen",15},{1,"Metformin",20},{1,"ORS",40},
        {2,"Insulin",15},{2,"Amoxicillin",20},{2,"Azithromycin",15},{2,"Cefixime",12},{2,"Paracetamol",25},
        {3,"Amlodipine",20},{3,"Losartan",15},{3,"Atorvastatin",15},{3,"Metformin",25},{3,"Omeprazole",20},
        {4,"Insulin",10},{4,"Glargine",10},{4,"Budesonide",8},{4,"Salbutamol",15},{4,"Montelukast",20},
        {5,"Paracetamol",35},{5,"Cetirizine",20},{5,"Loratadine",15},{5,"ORS",50},{5,"Ondansetron",10},
        {6,"Amoxicillin",15},{6,"Doxycycline",10},{6,"Cefixime",15},{6,"Diclofenac",20},{6,"Naproxen",15},
        {7,"Insulin",12},{7,"Metformin",30},{7,"Amlodipine",20},{7,"Calcium",25},{7,"Vitamin D3",20},
        {8,"Pantoprazole",25},{8,"Omeprazole",15},{8,"Domperidone",15},{8,"Ondansetron",12},{8,"ORS",30},
        {9,"Iron",25},{9,"Folic Acid",20},{9,"Multivitamin",30},{9,"Calcium",20},{9,"Paracetamol",20},
        {10,"Insulin",8},{10,"Amoxicillin",10},{10,"Azithromycin",10},{10,"Salbutamol",10},{10,"Montelukast",10}
    };
    for (auto& s : stocks)
        for (auto& p : ph)
            if (p.id == s.pid) p.addMedicine(s.med, s.qty);

    const char* pairs[][2] = {
        {"Insulin","Glargine"},{"Glargine","Insulin"},{"Paracetamol","Ibuprofen"},{"Ibuprofen","Paracetamol"},
        {"Amoxicillin","Azithromycin"},{"Amoxicillin","Cefixime"},{"Azithromycin","Amoxicillin"},
        {"Cefixime","Amoxicillin"},{"Metformin","Insulin"},{"Amlodipine","Losartan"},{"Losartan","Amlodipine"},
        {"Atorvastatin","Fenofibrate"},{"Omeprazole","Pantoprazole"},{"Pantoprazole","Omeprazole"},
        {"Cetirizine","Loratadine"},{"Loratadine","Cetirizine"},{"Cefixime","Azithromycin"},
        {"Doxycycline","Azithromycin"},{"Salbutamol","Budesonide"},{"Budesonide","Salbutamol"},
        {"Diclofenac","Naproxen"},{"Naproxen","Diclofenac"},{"Domperidone","Ondansetron"},
        {"Ondansetron","Domperidone"},{"Calcium","Vitamin D3"},{"Vitamin D3","Calcium"},
        {"Iron","Folic Acid"},{"Folic Acid","Iron"}
    };
    for (auto& p : pairs) alts[p[0]].push_back(p[1]);
}

// ---------------- Input helpers ----------------
int readInt(const string& prompt) {
    int x;
    while (true) {
        cout << prompt;
        if (cin >> x) { cin.ignore(numeric_limits<streamsize>::max(), '\n'); return x; }
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid input. Enter a number.\n";
    }
}

string readLine(const string& prompt) {
    string s;
    cout << prompt;
    getline(cin, s);
    return s;
}

int main() {
    vector<Pharmacy> pharmacies;
    unordered_map<string, vector<string>> alternatives;
    loadSampleData(pharmacies, alternatives);

    cout << "Preloaded pharmacies: " << pharmacies.size() << endl;

    while (true) {
        cout << "\n=========== PHARMACY MODULE ===========\n"
             << "1. Show All Pharmacies\n"
             << "2. Find Pharmacy for Medicine\n"
             << "3. Show Medicine Alternatives\n"
             << "4. Show Stock of a Pharmacy\n"
             << "0. Exit\n"
             << "=======================================\n";
        int choice = readInt("Enter choice: ");
        if (choice == 0) break;

        switch (choice) {
        case 1:
            cout << "\n----- PHARMACIES -----\n";
            for (auto& p : pharmacies) p.show();
            break;
        case 2:
            findPharmacy(pharmacies, readLine("Enter Medicine Name: "));
            break;
        case 3:
            findAlternative(alternatives, readLine("Enter Medicine Name: "));
            break;
        case 4: {
            int id = readInt("Enter Pharmacy ID (1-10): ");
            bool ok = false;
            for (auto& p : pharmacies)
                if (p.id == id) { p.showMedicineStock(); ok = true; }
            if (!ok) cout << "Pharmacy not found.\n";
            break;
        }
        default:
            cout << "Invalid choice.\n";
        }
    }
    return 0;
}
