# Building and running the backend (Phase 1)

## Requirements
- A C++17 compiler (g++ 9+, clang 10+, MinGW-w64, or MSVC 2019+).
- SQLite3 **development files** (`sqlite3.h` + library). No other third-party C++ library is needed.
  - Ubuntu/Debian: `sudo apt install build-essential libsqlite3-dev`
  - macOS: `brew install sqlite` (headers are also in the Xcode SDK)
  - Windows (MSYS2/MinGW): `pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sqlite3`

## Build (single translation unit: `backend/main.cpp` includes everything)
```
g++ -std=c++17 -O2 backend/main.cpp -pthread -lsqlite3 -o medicine_server            # Linux / macOS
g++ -std=c++17 -O2 backend/main.cpp -lsqlite3 -lws2_32 -o medicine_server.exe        # Windows (MinGW)
cmake -S . -B build && cmake --build build                                            # CMake alternative
```
Do **not** compile the member `.cpp` files separately; `saksham.cpp` includes them in a fixed order.

## Run
```
./medicine_server                      # API on http://127.0.0.1:8080, database file medicine_system.db
./medicine_server --port 9000 --db demo.db
./medicine_server --seed               # (re)run the demo-data seeder; safe to repeat
./medicine_server --no-seed            # start without auto-loading demo data on an empty database
./medicine_server --cli                # original console menu on top of the same service layer
```
On first launch the schema is created and, if the database is empty, 1,300+ medicines, 30 pharmacies, inventory, 40 patients,
demo users and a sample queue are loaded. All of it is **synthetic demonstration data** (brand names, manufacturers, phone numbers,
addresses and stock are invented; nothing is verified real-world availability).

## Demo accounts (change before any real use)
| Role | Username | Password |
|---|---|---|
| Admin | `admin` | `Admin@123` |
| Pharmacist (all pharmacies) | `pharmacist` | `Pharma@123` |
| Pharmacist (pharmacy #1 only) | `pharmacist.doon` | `Pharma@123` |
| Patient (patient #1) | `patient` | `Patient@123` |
| Patient (patient #2) | `patient2` | `Patient@123` |

## Frontend
```
cd frontend && npm install && npm run build     # produces frontend/dist
./medicine_server                               # serves frontend/dist and the API on one port
# development: ./medicine_server  +  (cd frontend && npm run dev)  -> http://localhost:5173 (proxy to :8080)
```
Use `--static <dir>` to serve a different build folder. See the top-level README.md and docs/TESTING.md (the frontend build itself has not been run in the authoring environment).

## Tests
```
./scripts/run_tests.sh                    # builds, then runs unit, database, API-workflow, CLI and seeding tests
SANITIZE=1 ./scripts/run_tests.sh         # same, with AddressSanitizer + UBSan
```
Python 3 and `curl` are used only as a test client. The system under test is the C++ server.

## Quick try
```
curl -s -X POST localhost:8080/api/auth/login -d '{"username":"pharmacist","password":"Pharma@123"}'
curl -s localhost:8080/api/queue -H "Authorization: Bearer <token>"
```
See `docs/API.md` for all 76 routes. Browser tests: `./scripts/run_ui_tests.sh` (Node + Playwright + esbuild; see docs/TESTING.md).
