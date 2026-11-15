// ============================================================
//                         SHAURYA
//                   DSA & DATA MANAGEMENT
// ============================================================

int searchPatient(vector<Patient>& patients,int id) {
    for(int i=0;i<(int)patients.size();i++) if(patients[i].id==id) return i;
    return -1;
}

bool searchMedicine(unordered_map<string,Medicine>& inventory,string name) {
    return inventory.find(name)!=inventory.end();
}

void sortPatients(vector<Patient>& patients) {
    sort(patients.begin(),patients.end(),[](const Patient& a,const Patient& b){ return a.priority<b.priority; });
}

void sortByWaitingTime(vector<Patient>& patients) {
    sort(patients.begin(),patients.end(),[](const Patient& a,const Patient& b){ return a.waitTime>b.waitTime; });
}

void demandReport(vector<Patient>& patients) {
    unordered_map<string,int> demand;
    for(auto &p:patients) demand[p.medicine]++;
    cout << "\n----- MEDICINE DEMAND -----\n";
    for(auto &x:demand) cout << x.first << " : " << x.second << " requests\n";
}

void priorityReport(vector<Patient>& patients) {
    int critical=0,high=0,medium=0,normal=0;
    for(auto &p:patients) {
        if(p.urgency=="Critical") critical++;
        else if(p.urgency=="High") high++;
        else if(p.urgency=="Medium") medium++;
        else normal++;
    }
    cout << "\n----- PRIORITY REPORT -----\n"
         << "Critical : " << critical << "\n"
         << "High     : " << high << "\n"
         << "Medium   : " << medium << "\n"
         << "Normal   : " << normal << endl;
}

void filterUrgency(vector<Patient>& patients,string urgency) {
    cout << "\nPatients with " << urgency << " urgency:\n";
    for(auto &p:patients) if(p.urgency==urgency) p.show();
}

// JSON-producing versions used by the REST analytics endpoints.
Json demandJson(const vector<Patient>& patients) {
    unordered_map<string,int> demand;
    for(auto &p:patients) demand[p.medicine]++;
    vector<pair<string,int>> v(demand.begin(),demand.end());
    sort(v.begin(),v.end(),[](const pair<string,int>& a,const pair<string,int>& b){
        return a.second!=b.second ? a.second>b.second : a.first<b.first; });
    Json out=Json::array();
    for(auto &x:v) { Json j=Json::object(); j["medicine"]=x.first; j["requests"]=x.second; out.push_back(j); }
    return out;
}

Json priorityCounts(const vector<Patient>& patients) {
    Json j=Json::object(); j["Critical"]=0; j["High"]=0; j["Medium"]=0; j["Normal"]=0;
    for(auto &p:patients) j[p.urgency]=j.at(p.urgency).integer()+1;
    return j;
}

// ---------------- Database: core connection & data management ----------------

Database::~Database() { if(db) sqlite3_close(db); }

bool Database::open(const string& path) {
    if(sqlite3_open(path.c_str(),&db)!=SQLITE_OK) return false;
    sqlite3_busy_timeout(db,5000);
    exec("PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL;");
    return true;
}

void Database::exec(const string& sql) {
    char* err=nullptr;
    if(sqlite3_exec(db,sql.c_str(),nullptr,nullptr,&err)!=SQLITE_OK) {
        string m=err?err:"unknown error"; sqlite3_free(err);
        throw runtime_error("SQLite: "+m);
    }
}

void Database::raise(int rc,const string& sql) {
    string msg=sqlite3_errmsg(db);
    if((rc&0xff)==SQLITE_CONSTRAINT) {
        cerr << "[db] constraint: " << msg << " | " << sql.substr(0,80) << endl;
        throw ApiError(409,"Operation conflicts with existing data (duplicate, invalid reference or rule violation)");
    }
    throw runtime_error("SQLite: "+msg+" | "+sql.substr(0,120));
}

void Database::bindAll(sqlite3_stmt* st,const vector<Val>& p) {
    for(size_t i=0;i<p.size();i++) {
        int idx=(int)i+1, rc=SQLITE_OK;
        switch(p[i].t) {
        case Val::Null: rc=sqlite3_bind_null(st,idx); break;
        case Val::Int: rc=sqlite3_bind_int64(st,idx,p[i].i); break;
        case Val::Real: rc=sqlite3_bind_double(st,idx,p[i].d); break;
        case Val::Text: rc=sqlite3_bind_text(st,idx,p[i].s.c_str(),(int)p[i].s.size(),SQLITE_TRANSIENT); break;
        }
        if(rc!=SQLITE_OK) { sqlite3_finalize(st); raise(rc,"bind"); }
    }
}

Json Database::query(const string& sql,const vector<Val>& p) {
    sqlite3_stmt* st=nullptr;
    int rc=sqlite3_prepare_v2(db,sql.c_str(),-1,&st,nullptr);
    if(rc!=SQLITE_OK) raise(rc,sql);
    bindAll(st,p);
    Json rows=Json::array(); int cols=sqlite3_column_count(st);
    while((rc=sqlite3_step(st))==SQLITE_ROW) {
        Json r=Json::object();
        for(int c=0;c<cols;c++) {
            const char* name=sqlite3_column_name(st,c);
            switch(sqlite3_column_type(st,c)) {
            case SQLITE_INTEGER: r[name]=(long long)sqlite3_column_int64(st,c); break;
            case SQLITE_FLOAT: r[name]=sqlite3_column_double(st,c); break;
            case SQLITE_NULL: r[name]=Json(); break;
            default: { const unsigned char* t=sqlite3_column_text(st,c); r[name]=string(t?(const char*)t:""); }
            }
        }
        rows.push_back(r);
    }
    sqlite3_finalize(st);
    if(rc!=SQLITE_DONE) raise(rc,sql);
    return rows;
}

Json Database::one(const string& sql,const vector<Val>& p) {
    Json rows=query(sql,p);
    return rows.size()?rows.items()[0]:Json();
}

long long Database::scalar(const string& sql,const vector<Val>& p,long long def) {
    sqlite3_stmt* st=nullptr;
    int rc=sqlite3_prepare_v2(db,sql.c_str(),-1,&st,nullptr);
    if(rc!=SQLITE_OK) raise(rc,sql);
    bindAll(st,p);
    long long v=def;
    rc=sqlite3_step(st);
    if(rc==SQLITE_ROW && sqlite3_column_type(st,0)!=SQLITE_NULL) v=sqlite3_column_int64(st,0);
    sqlite3_finalize(st);
    if(rc!=SQLITE_ROW && rc!=SQLITE_DONE) raise(rc,sql);
    return v;
}

long long Database::run(const string& sql,const vector<Val>& p) {
    sqlite3_stmt* st=nullptr;
    int rc=sqlite3_prepare_v2(db,sql.c_str(),-1,&st,nullptr);
    if(rc!=SQLITE_OK) raise(rc,sql);
    bindAll(st,p);
    while((rc=sqlite3_step(st))==SQLITE_ROW) {}
    sqlite3_finalize(st);
    if(rc!=SQLITE_DONE) raise(rc,sql);
    return sqlite3_changes(db);
}

void Database::begin() { exec("BEGIN IMMEDIATE"); }
void Database::commit() { exec("COMMIT"); }
void Database::rollback() { exec("ROLLBACK"); }

void Database::initSchema() {
    exec(SCHEMA_SQL);
    struct S { const char* k; const char* v; const char* d; };
    const S defaults[]={
        {"near_expiry_days","60","Batches expiring within this many days are flagged as nearing expiry"},
        {"session_hours","12","Login session lifetime in hours"},
        {"enforce_priority","1","1 = only the head of the priority queue may be distributed"},
        {"default_reorder_level","20","Reorder level used for new inventory rows"},
        {"data_notice","Demonstration data: medicines, brands, manufacturers, pharmacies, contacts and stock are synthetic and not verified real-world availability.","Banner shown in the application"}};
    for(const auto& s:defaults)
        run("INSERT OR IGNORE INTO system_settings(key,value,description) VALUES(?,?,?)",{s.k,s.v,s.d});
}

string Database::setting(const string& key,const string& def) {
    Json r=one("SELECT value FROM system_settings WHERE key=?",{key});
    return r.isNull()?def:r.at("value").str();
}

void Database::setSetting(const string& key,const string& value,const string& desc) {
    run("INSERT INTO system_settings(key,value,description) VALUES(?,?,?) ON CONFLICT(key) DO UPDATE SET "
        "value=excluded.value,updated_at=datetime('now')",{key,value,desc});
}

void Database::audit(const Auth& by,const string& action,const string& entity,const string& id,const string& details) {
    run("INSERT INTO audit_logs(user_id,username,action,entity,entity_id,details) VALUES(?,?,?,?,?,?)",
        {by.userId>0?Val(by.userId):Val(),by.username.empty()?Val():Val(by.username),action,entity,id,details});
}

#include "shaurya_seed.cpp"
