// ============================================================
//                         VANSH
//                  MEDICINE & INVENTORY
// ============================================================

class Medicine {
public:
    string name,category,expiry;     // expiry = earliest unexpired batch expiry in stock
    int stock,minStock;              // stock = unexpired units across all pharmacies
    int id = 0, requiresPrescription = 0;
    double price = 0;
    string genericName,brand,manufacturer,composition,strength,dosageForm,status,description,storage;
    vector<string> alternatives;

    Medicine() : stock(0),minStock(20) {}
    Medicine(string n,string c,int s,string e,int minS)
        : name(n),category(c),expiry(e),stock(s),minStock(minS) {}

    bool available(int q) const { return stock>=q; }
    void removeStock(int q) { if(available(q)) stock-=q; }
    void addStock(int q) { stock+=q; }

    void show() const {
        cout << name << " | " << category << " | Stock: " << stock
             << " | Expiry: " << expiry << endl;
    }

    void checkStock() const {
        if(stock<=minStock) cout << "LOW STOCK: " << name << endl;
    }

    void addAlternative(string m) { alternatives.push_back(m); }

    void showAlternatives() const {
        cout << "Alternatives: ";
        if(alternatives.empty()) cout << "None";
        else for(const auto& x:alternatives) cout << x << " ";
        cout << endl;
    }

    bool isLowStock() const { return stock<=minStock; }

    void showFullDetails() const {
        cout << "\nMedicine Name : " << name
             << "\nCategory      : " << category
             << "\nStock         : " << stock
             << "\nMinimum Stock : " << minStock
             << "\nExpiry        : " << expiry << endl;
        showAlternatives();
    }

    Json toJson() const {
        Json j = Json::object();
        j["medicine_id"]=id; j["name"]=name; j["generic_name"]=genericName; j["brand_name"]=brand;
        j["category"]=category; j["manufacturer"]=manufacturer; j["composition"]=composition;
        j["strength"]=strength; j["dosage_form"]=dosageForm; j["requires_prescription"]=requiresPrescription==1;
        j["unit_price"]=price; j["status"]=status; j["total_stock"]=stock; j["min_stock"]=minStock;
        j["low_stock"]=isLowStock(); j["earliest_expiry"]=expiry.empty()?Json():Json(expiry);
        return j;
    }
};

// ---------------- Database: medicines & stock ----------------

// Loads medicine master data plus unexpired stock totals into the hash map.
void Database::loadMedicines(unordered_map<string,Medicine>& inventory) {
    Json rows = query(
        "SELECT m.medicine_id,m.name,m.generic_name,m.brand_name,c.name cat,mf.name manuf,m.composition,m.strength,"
        "m.dosage_form,m.requires_prescription rx,m.unit_price,m.status,m.min_stock,m.description,m.storage_instructions,"
        "COALESCE(SUM(CASE WHEN b.expiry_date>=date('now') THEN b.quantity END),0) stock,"
        "MIN(CASE WHEN b.expiry_date>=date('now') AND b.quantity>0 THEN b.expiry_date END) earliest "
        "FROM medicines m JOIN medicine_categories c ON c.category_id=m.category_id "
        "JOIN manufacturers mf ON mf.manufacturer_id=m.manufacturer_id "
        "LEFT JOIN medicine_batches b ON b.medicine_id=m.medicine_id GROUP BY m.medicine_id");
    for(const auto& r : rows.items()) {
        Medicine m;
        m.id=(int)r.at("medicine_id").integer(); m.name=r.at("name").str(); m.genericName=r.at("generic_name").str();
        m.brand=r.at("brand_name").str(); m.category=r.at("cat").str(); m.manufacturer=r.at("manuf").str();
        m.composition=r.at("composition").str(); m.strength=r.at("strength").str(); m.dosageForm=r.at("dosage_form").str();
        m.requiresPrescription=(int)r.at("rx").integer(); m.price=r.at("unit_price").num(); m.status=r.at("status").str();
        m.minStock=(int)r.at("min_stock").integer(); m.description=r.at("description").str();
        m.storage=r.at("storage_instructions").str(); m.stock=(int)r.at("stock").integer();
        m.expiry=r.at("earliest").str();
        inventory[m.name]=m;
    }
}
