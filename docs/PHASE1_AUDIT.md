# Phase 1 audit of the original split project

## Findings in the original code (fixed during the migration)
1. **Waiting-time tie-break never worked.** `Patient::waitTime` was initialised to 0 and `increaseWait()` was never called, so equal-urgency patients were ordered arbitrarily. Wait time is now derived from the request timestamp; ties go to the older request, then the lower request id.
2. **Duplicate queue entries.** `restock()` pushed eligible waiting patients into the queue every time it ran. The queue is now rebuilt from the database, so a request can be queued only once.
3. **Stale copies.** The queue held copies of `Patient` objects that diverged from the vector/database. Status changes now go through one function (`Database::setRequestStatus`) and the queue is rebuilt after each change.
4. **One request per patient** (`patient_id` was the primary key). Requests are now a separate table.
5. **No stock-level integrity.** Stock was a single integer per medicine with no batches or expiry per pharmacy. Stock is now per pharmacy and per batch, with First-Expiry-First-Out deduction inside a database transaction.
6. **Unreachable code.** `sortPatients`, `sortByWaitingTime`, `filterUrgency`, `checkWaiting`, `addMedicine`, `addPharmacy`, `increaseWait` were never called from the menu. `sort*`/report functions are kept and now feed the REST analytics; the rest are preserved as class members.
7. `readUrgency()` looped forever at end of input; `Medicine`'s constructor initialised members out of declaration order. Both removed/fixed.
8. Original build never used the real `sqlite3.h`: it was only verified against a stand-in header in the sandbox (still true for this phase, see README of the test stub).

## Where the original pieces live now
| Original | New location | Change |
|---|---|---|
| `Database` class (Saksham declaration) | `saksham.cpp` | Prepared-statement helpers (`query/one/scalar/run`), transactions (`Tx`), constraint errors mapped to HTTP 409 |
| `Patient`, `Compare`, patient/waiting DB functions | `nikita.cpp` | Request-centric; queue order uses request time |
| `Medicine`, `loadMedicines` | `vansh.cpp` | Adds generic/brand/manufacturer/strength/form/price/Rx; stock aggregated from batches |
| `Pharmacy`, `findPharmacy`, `findAlternative`, `loadPharmacies`, `loadAlternatives` | `krishna.cpp` | Adds address/coordinates/hours; same algorithms |
| `Transaction`, `makeTransaction`, transaction DB functions | `ankit.cpp` | Adds pharmacy, batches, performer |
| `searchPatient`, `searchMedicine`, sort/report functions, DB core | `shaurya.cpp` | Plus JSON report variants, settings, audit log, seeding |
| `MedicineSystem` | `saksham.cpp` (class) + member `*_api.cpp` (service methods) | Priority queue + hash-map inventory kept; DB is source of truth |
| Console menu | `main.cpp --cli` | Preserved on the new service layer |
