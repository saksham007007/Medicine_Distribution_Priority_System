// ENTRY POINT. saksham.cpp includes every member file (single translation unit).
#include "saksham.cpp"
#include <cstdlib>

// Original console menu, kept as `--cli` mode on top of the new service layer.
static void printMenu() {
    cout << "\n============================================\n"
         << "     MEDICINE DISTRIBUTION SYSTEM (CLI)\n"
         << "============================================\n"
         << "1. Show Priority Queue\n"
         << "2. Evaluate Next Request (head of queue)\n"
         << "3. Search Patient Requests\n"
         << "4. Search Medicine\n"
         << "5. Show Medicine Alternatives\n"
         << "6. Find Pharmacy for Medicine\n"
         << "7. Show Waiting List\n"
         << "8. Show Transactions\n"
         << "9. Show Reports\n"
         << "10. Database Summary\n"
         << "0. Exit\n"
         << "============================================\n";
}

static int readInt(const string& prompt) {
    string line; cout << prompt;
    if (!getline(cin, line)) return 0;
    try { return stoi(line); } catch (...) { return -1; }
}
static string readLine(const string& prompt) { cout << prompt; string s; getline(cin, s); return s; }

static void runCli(MedicineSystem& system, const Auth& cliUser) {
    while (true) {
        printMenu();
        if (!cin) return;
        int choice = readInt("Enter choice: ");
        if (choice == 0) { cout << "Exiting... Data is saved in SQLite.\n"; return; }
        try {
            switch (choice) {
            case 1: system.reloadQueue(); for (auto& p : system.orderedQueue()) p.show(); if (system.pq.empty()) cout << "Queue is empty.\n"; break;
            case 2: { Json r = system.evaluateNext(cliUser); cout << r.at("action").str() << ": " << r.at("message").str() << endl;
                      if (r.at("action").str() == "ready") {
                          const Json& recs = r.at("details").at("recommendations");
                          Json pick = recs.items()[0];
                          cout << "Nearest/best pharmacy: " << pick.at("name").str() << " (" << pick.at("available").integer() << " units). Distribute? (y/n): ";
                          string a; getline(cin, a);
                          if (a == "y" || a == "Y") { Json d = system.distribute(r.at("request").at("request_id").integer(), pick.at("pharmacy_id").integer(), cliUser); cout << d.at("message").str() << endl; }
                      } break; }
            case 3: { int id = readInt("Enter Patient ID: "); system.reloadQueue(); vector<Patient> all = system.orderedQueue();
                      for (auto& w : system.waitingList) all.push_back(w);
                      bool any = false; for (auto& p : all) if (p.id == id) { p.showDetails(); any = true; }
                      if (!any) cout << "No open requests for that patient.\n";
                      break; }
            case 4: {
                string name = readLine("Enter Medicine Name: ");
                if (searchMedicine(system.inventory, name)) system.inventory[name].showFullDetails();
                else cout << "Medicine not found.\n";
                break; }
            case 5: findAlternative(system.inventory, readLine("Enter Medicine Name: ")); break;
            case 6: findPharmacy(system.pharmacies, readLine("Enter Medicine Name: ")); break;
            case 7: system.reloadQueue(); for (auto& p : system.waitingList) p.show(); if (system.waitingList.empty()) cout << "Waiting list is empty.\n"; break;
            case 8: system.database.showTransactions(); break;
            case 9: { system.reloadQueue(); vector<Patient> all = system.orderedQueue(); for (auto& w : system.waitingList) all.push_back(w);
                      demandReport(all); priorityReport(all); break; }
            case 10: system.database.summary(); break;
            default: cout << "Invalid choice.\n";
            }
        } catch (const exception& e) { cout << "Error: " << e.what() << endl; }
    }
}

int main(int argc, char** argv) {
    string dbPath = "medicine_system.db", host = "127.0.0.1", staticDir = "frontend/dist"; int port = 8080;
    bool cli = false, seedOnly = false, noSeed = false;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--cli") cli = true;
        else if (a == "--seed") seedOnly = true;
        else if (a == "--no-seed") noSeed = true;
        else if (a == "--db" && i + 1 < argc) dbPath = argv[++i];
        else if (a == "--port" && i + 1 < argc) port = atoi(argv[++i]);
        else if (a == "--host" && i + 1 < argc) host = argv[++i];
        else if (a == "--static" && i + 1 < argc) staticDir = argv[++i];
        else { cout << "Usage: medicine_server [--db file] [--port N] [--host IP] [--static dir] [--seed] [--no-seed] [--cli]\n"; return a == "--help" ? 0 : 1; }
    }
    Database database;
    if (!database.open(dbPath)) { cerr << "Cannot open database " << dbPath << endl; return 1; }
    try {
        database.initSchema();                                   // tables are created automatically on first launch
        if (seedOnly) { database.seedDemoData(); cout << "Seeding complete (safe to repeat).\n"; database.summary(); return 0; }
        if (!noSeed && database.seedIfEmpty()) cout << "Empty database detected: demonstration data was loaded.\n";
        MedicineSystem system(database);
        system.loadMasterData();
        cout << "Database: " << dbPath << " | medicines: " << system.inventory.size() << " | pharmacies: " << system.pharmacies.size()
             << " | queue: " << system.pq.size() << " | waiting: " << system.waitingList.size() << endl;
        if (cli) { Auth u; u.username = "cli"; u.role = "Admin"; runCli(system, u); return 0; }
        Router router; buildRoutes(router, system);
        std::error_code fsErr;
        if (std::filesystem::is_directory(staticDir, fsErr)) { router.fallback = makeStaticHandler(staticDir); cout << "Serving web app from " << staticDir << endl; }
        else cout << "Web app not found at '" << staticDir << "' (API only). Build it with: cd frontend && npm install && npm run build\n";
        cout << "Medicine Distribution Priority System API listening on http://" << host << ":" << port << "  (" << router.count() << " routes)\n";
        HttpServer server;
        if (!server.listen(host, port, router)) { cerr << "Could not listen on " << host << ":" << port << endl; return 1; }
    } catch (const exception& e) { cerr << "Fatal: " << e.what() << endl; return 1; }
    return 0;
}
