# Testing: what was verified, and what was not

Run everything with `./scripts/run_tests.sh` (backend) and `./scripts/run_ui_tests.sh` (browser). `SANITIZE=1` adds AddressSanitizer + UBSan.

## Results from the final run (the environment that produced this project)
| Suite | Result | What it covers |
|---|---|---|
| Unit tests (`tests/unit_tests.cpp`) | **36 / 36** | SHA-256, HMAC-SHA256 and PBKDF2 against published test vectors; JSON parse/serialise and malformed input; validation helpers; router matching |
| Database tests (`tests/test_database.py`) | **31 / 31** | 18 required tables; 1,000+ distinct medicines (1,322; ~600 distinct formulations, 250 generics); 30 pharmacies in 17 cities; inventory = sum of batches; expired / near-expiry / low-stock / out-of-stock samples; alternatives never cross generic or dosage form; hashed per-user-salted passwords; FK and integrity checks; indexes |
| API workflow tests (`tests/test_api.py`) | **271 / 271** | Authentication, throttling, role permissions, validation, CRUD; priority ordering and tie-break; controlled/recorded priority changes; prescription rules; draining the queue in priority order with exact stock deduction; failed distribution leaves no partial state; waiting list, alternatives, pharmacist-only substitution approval, restock promotion; expiry/low-stock; pharmacy search by distance; analytics equal database counts; final database invariants |
| Same suite under AddressSanitizer + UBSan | **no findings** | Memory errors and undefined behaviour in the server during the full API run |
| CLI regression | **pass** | Original console menu distributes the head request on the new service layer |
| Seeding idempotence | **pass** | Running `--seed` twice changes no table counts |
| Browser UI (`tests/ui/ui_test.cjs`, headless Chromium) | **56 / 56** | Login redirect/errors; patient vs staff navigation and access denial; all 14 admin pages load real data with no error banners; dashboard/analytics numbers and chart mounting, CSV download; medicine search/filter/sort/pagination, detail dialog, admin add/remove, server validation errors; patient/pharmacy/user CRUD; settings + audit log; queue order, urgency change with reason, request form, 4 distributions in order incl. prescription verification, waiting-list alternative proposal and approval, restock + stock movement/history, reused-batch error, notifications, alternatives compare, pharmacy search by distance; patient self-registration, Normal-priority request, prescription flow, cancel, profile edit, password change; mobile menu open/close logic; no console or page errors during the run |

Bugs found *by* these tests and fixed: a segfault from handlers capturing a destroyed local lambda; a foreign-key failure when the CLI (no database user) performed a distribution; a dangling-temporary bug in the seeder; the original queue's missing wait-time tie-break (see `docs/PHASE1_AUDIT.md`).

## NOT verified: please check these on a normal development machine
1. **Real SQLite header.** The sandbox had `libsqlite3.so` but no `sqlite3.h`, so every backend build and test here used `tests/sqlite_stub/sqlite3.h`, a hand-written declaration of the SQLite functions the code uses, linked against the real library. This exercises real SQLite behaviour but is **not proof that the code compiles against the official header**. Build with `libsqlite3-dev` installed and run `./scripts/run_tests.sh` without the stub variables.
2. **Real frontend dependencies.** `react-router-dom`, `recharts` and `lucide-react` (and Vite, Tailwind, PostCSS) could not be installed offline. The browser tests bundled the app with esbuild and **test-only stand-ins** for those three libraries (`tests/ui/shims/`; routing + chart containers + icons). They prove the application's own logic, data flow and API integration, **not** that the real libraries behave identically, that `npm install` resolves, or that `vite build` succeeds. Only their standard APIs are used. The sandbox also used React 19; `package.json` pins React 18.3.1.
3. **Styling and responsiveness.** Tailwind CSS was not compiled in the test build, so the pages were exercised unstyled. Visual layout, colours, the blue/teal palette and mobile layouts are **unverified**; the mobile test checks only the menu open/close logic.
4. **Windows and macOS builds** (Winsock code path, MinGW/MSVC) and the **CMake** file: not built.
5. **Browsers:** headless Chromium only (no Firefox/Safari, no real touch devices). The "Use my location" button was not exercised.
6. **Performance and concurrency:** the embedded server handles one request at a time behind a global lock, with no keep-alive and no TLS; no load or concurrent-user testing was done. Fine for a demonstration, not for production.
7. **Security review:** no external audit or penetration test. Tokens are bearer tokens in browser `localStorage`; CORS allows any origin (needed for the Vite dev server). Demo passwords are published in the README.
8. **Accessibility** was not audited (labels/roles are used, but no screen-reader or contrast testing).
9. **Data realism:** generic names, strengths and categories follow common formulations but were not clinically verified; all brands, manufacturers, contacts and stock are invented. Dates in the sample stock are generated relative to the first launch.

## How the stand-ins are kept out of the product
`tests/sqlite_stub/` and `tests/ui/shims/` are used only by the scripts in `scripts/`. The shipped build uses `-lsqlite3` and `npm install` + `vite build`.
