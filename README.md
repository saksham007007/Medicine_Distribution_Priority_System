# Medicine Distribution Priority System

**Team Bug Busters · BTech CSE Project-Based Learning (Graphic Era)**

A full-stack web application that distributes medicines to patients **by urgency rather than first-come-first-served**.
The business logic is the team's original **C++ OOP/DSA project** (priority queue, hash-map inventory, sorting, searching), now behind a REST API with a SQLite database and a React dashboard.

> **All medicines, brand names, manufacturers, pharmacies, phone numbers, addresses and stock levels are synthetic demonstration data.** Nothing here is verified real-world availability, and nothing is medical advice.

## Architecture
```
React (Vite, Tailwind, Router, Recharts, Lucide)  --REST/JSON-->  C++17 backend  -->  SQLite
frontend/                                                          backend/          created on first run
```
| Layer | What it does |
|---|---|
| `backend/*.cpp` (6 member files + `*_api.cpp`) | Original classes (`Patient`, `Medicine`, `Pharmacy`, `Transaction`, `Database`, `MedicineSystem`), priority queue (`std::priority_queue`), hash-map inventory, sorting/searching, REST routes |
| `backend/include/` | JSON, embedded HTTP server + router, PBKDF2-SHA256 crypto, schema, static file serving (no third-party C++ library besides SQLite) |
| `frontend/` | 15 pages with role-based navigation; every action calls the API |
| `database/schema.sql` | 22 tables with foreign keys, CHECK constraints and indexes (generated from `backend/include/schema.h`, which the server runs automatically) |

**Deviation from the brief:** the REST layer is a small embedded server (`http_server.h`) rather than Crow, so the backend needs only SQLite and could be tested offline. Routes are registered on a `Router`, so Crow could replace it without touching business logic.

## Key behaviour
- Four urgency levels (Critical, High, Medium, Normal); ties go to the longest-waiting request. Only staff can change urgency, with a mandatory recorded reason. Self-service patient requests always enter as Normal.
- Distribution is strictly in queue order; stock is deducted first-expiry-first-out from batches inside one database transaction (stock, transaction record, status, waiting list and notification succeed or fail together).
- Out-of-stock requests join the waiting list, return to the queue automatically after a restock, and patients are notified.
- Alternatives are suggestions only (same generic name and dosage form). **Only the Pharmacist role can approve a substitution**; a different strength needs a written clinical note; prescription rules still apply.
- Roles: Admin (everything), Pharmacist (inventory, distribution, prescription verification; can be restricted to one pharmacy), Patient (own data, medicine/pharmacy search, own requests).
- Passwords: salted PBKDF2-SHA256; sessions are random tokens stored hashed; login throttling; audit log.

## Run it
Requirements: a C++17 compiler, SQLite3 development files (`sqlite3.h`), Node.js 18+ (for the frontend).
```
# 1. backend
g++ -std=c++17 -O2 backend/main.cpp -pthread -lsqlite3 -o medicine_server        # Windows/MinGW: add -lws2_32
# 2. frontend (needs internet once for npm)
cd frontend && npm install && npm run build && cd ..
# 3. run everything on one port
./medicine_server            # http://127.0.0.1:8080  (serves frontend/dist and the API)
```
First launch creates `medicine_system.db` and loads the demo data (1,300+ medicines, 30 pharmacies, 40 patients, a sample queue).
Development mode: run `./medicine_server` and, in `frontend/`, `npm run dev` (http://localhost:5173, proxied to the backend).
Other options: `--port N`, `--db file`, `--seed` (re-run the seeder; safe to repeat), `--cli` (original console menu). Details: `docs/BUILD.md`.

### Demo accounts (change before any real use)
| Role | Username | Password |
|---|---|---|
| Admin | `admin` | `Admin@123` |
| Pharmacist | `pharmacist` | `Pharma@123` |
| Pharmacist (pharmacy #1 only) | `pharmacist.doon` | `Pharma@123` |
| Patient | `patient` | `Patient@123` |

## Suggested demo for the panel
1. Log in as **pharmacist** → *Priority Queue*: Critical patients first; raise someone's urgency (reason required).
2. *Distribution* → "Evaluate next request" → distribute from the recommended pharmacy; check *Inventory → Stock movement* and *Transactions → batches used*.
3. *Waiting List* → alternatives → propose, then **approve** (pharmacist) → the request returns to the queue.
4. *Inventory* → restock → waiting requests are promoted automatically.
5. Log in as **patient** → request a medicine (enters as Normal), watch the queue position; **admin** → Analytics.

## Documentation
`docs/BUILD.md` (build/run) · `docs/API.md` (76 routes with roles) · `docs/TESTING.md` (what was and was not verified) · `docs/CONTRIBUTIONS.md` (team report) · `docs/PHASE1_AUDIT.md` (bugs found in the original code).

## Honest status
See `docs/TESTING.md`. In short: the backend and the UI logic are tested end-to-end; the **real** SQLite header build, `npm install`/`vite build` with the real React Router/Recharts/Lucide, the Windows build and visual styling (Tailwind) were **not** verifiable in the environment that produced this project and must be checked on your machine.
