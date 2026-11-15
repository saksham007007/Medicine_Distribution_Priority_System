// ============================================================
//                         NIKITA
//              PATIENT MANAGEMENT & PRIORITY
// ============================================================
// One Patient object represents one medicine request in the priority queue
// (id = patient id, requestId = request id), so a patient can now have many
// requests over time.

class Patient {
public:
    int id = 0, priority = 4, quantity = 0, waitTime = 0, medicineId = 0;   // waitTime in minutes
    long long requestId = 0, requestedEpoch = 0;
    string name, urgency, medicine, status, requestedAt;

    Patient(int i,string n,string u,string m,int q) {
        id=i; name=n; urgency=u; medicine=m; quantity=q;
        waitTime=0; status="Waiting";
        setPriority();
    }

    Patient() : id(0),priority(4),quantity(0),waitTime(0),status("Waiting") {}

    void setPriority() {
        if(urgency=="Critical") priority=1;
        else if(urgency=="High") priority=2;
        else if(urgency=="Medium") priority=3;
        else priority=4;
    }

    static int priorityOf(const string& u) {
        if(u=="Critical") return 1;
        if(u=="High") return 2;
        if(u=="Medium") return 3;
        if(u=="Normal") return 4;
        return 0;
    }

    void show() const {
        cout << id << " | " << name << " | " << urgency << " | "
             << medicine << " | " << quantity << " | " << priority
             << " | " << status << endl;
    }

    void increaseWait() { waitTime++; }

    void updateUrgency(string u) { urgency=u; setPriority(); }
    void updateMedicine(string m,int q) { medicine=m; quantity=q; }
    void changeStatus(string s) { status=s; }

    void showDetails() const {
        cout << "\nPatient ID   : " << id
             << "\nName         : " << name
             << "\nUrgency      : " << urgency
             << "\nPriority     : " << priority
             << "\nMedicine     : " << medicine
             << "\nQuantity     : " << quantity
             << "\nWaiting Time : " << waitTime
             << "\nStatus       : " << status << endl;
    }

    Json toJson() const {
        Json j = Json::object();
        j["request_id"] = requestId; j["patient_id"] = id; j["patient_name"] = name;
        j["medicine_id"] = medicineId; j["medicine"] = medicine; j["quantity"] = quantity;
        j["urgency"] = urgency; j["priority"] = priority; j["status"] = status;
        j["waiting_minutes"] = waitTime; j["requested_at"] = requestedAt;
        return j;
    }
};

// Priority queue order: lower priority number first (Critical=1 ... Normal=4);
// within the same urgency the longer-waiting (older) request goes first, then
// the lower request id, so the order is always deterministic.
struct Compare {
    bool operator()(const Patient& a,const Patient& b) const {
        if(a.priority!=b.priority) return a.priority>b.priority;
        if(a.requestedEpoch!=b.requestedEpoch) return a.requestedEpoch>b.requestedEpoch;
        return a.requestId>b.requestId;
    }
};

// ---------------- Database: patients, requests, priority queue ----------------

static void fillPatients(Database& db, const string& status, vector<Patient>& out) {
    Json rows = db.query(
        "SELECT r.request_id,r.patient_id,pt.name pname,r.urgency,r.priority,r.quantity,r.status,r.requested_at,"
        "CAST(strftime('%s',r.requested_at) AS INTEGER) epoch,"
        "CAST((strftime('%s','now')-strftime('%s',r.requested_at))/60 AS INTEGER) wait,"
        "eff.medicine_id eid,eff.name ename "
        "FROM medicine_requests r JOIN patients pt ON pt.patient_id=r.patient_id "
        "JOIN medicines eff ON eff.medicine_id=CASE WHEN r.substitution_status='Approved' AND r.substitute_medicine_id IS NOT NULL "
        "THEN r.substitute_medicine_id ELSE r.medicine_id END "
        "WHERE r.status=? ORDER BY r.request_id", {status});
    for(const auto& r : rows.items()) {
        Patient p;
        p.requestId=r.at("request_id").integer(); p.id=(int)r.at("patient_id").integer();
        p.name=r.at("pname").str(); p.urgency=r.at("urgency").str();
        p.priority=(int)r.at("priority").integer(); p.quantity=(int)r.at("quantity").integer();
        p.status=r.at("status").str(); p.requestedAt=r.at("requested_at").str();
        p.requestedEpoch=r.at("epoch").integer(); p.waitTime=(int)max(0LL,r.at("wait").integer());
        p.medicineId=(int)r.at("eid").integer(); p.medicine=r.at("ename").str();
        out.push_back(p);
    }
}

long long Database::patient(const Patient& p,const string& contact,const string& address) {
    run("INSERT INTO patients(name,contact,address) VALUES(?,?,?)",{p.name,contact,address});
    return lastId();
}

void Database::loadPatients(vector<Patient>& pending) { fillPatients(*this,"Pending",pending); }

void Database::loadWaitingList(vector<Patient>& waiting) { fillPatients(*this,"Waiting",waiting); }

// Single place that changes a request's status and keeps the queue table in sync.
void Database::setRequestStatus(long long requestId,const string& status,const string& queueStatus) {
    run("UPDATE medicine_requests SET status=?,updated_at=datetime('now') WHERE request_id=?",{status,requestId});
    run("INSERT INTO patient_priority_queue(request_id,priority) SELECT request_id,priority FROM medicine_requests "
        "WHERE request_id=? ON CONFLICT(request_id) DO NOTHING",{requestId});
    run("UPDATE patient_priority_queue SET status=?,priority=(SELECT priority FROM medicine_requests WHERE request_id=?),"
        "updated_at=datetime('now') WHERE request_id=?",{queueStatus,requestId,requestId});
}
