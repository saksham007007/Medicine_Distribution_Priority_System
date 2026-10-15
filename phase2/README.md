# Medicine Distribution Priority System — Phase 2

This is the Phase 2 version: C++ + DSA + SQLite only.

There is NO frontend, HTTP server, REST API, HTML interface, or JavaScript.

## Requirements

Ubuntu/Linux:

```bash
sudo apt update
sudo apt install g++ sqlite3 libsqlite3-dev
```

## Run

```bash
chmod +x run.sh
./run.sh
```

Or:

```bash
g++ Medicine_Distribution_Phase_2_SQLite.cpp -o medicine_phase2 -lsqlite3
./medicine_phase2
```

The program uses `medicine_system_phase2.db`.

## Database tables

- patients
- medicines
- pharmacies
- pharmacy_stock
- alternatives
- transactions
- waiting_list

## View the DB

```bash
sqlite3 medicine_system_phase2.db
```

Then:

```sql
.tables
SELECT * FROM patients;
SELECT * FROM medicines;
SELECT * FROM pharmacies;
SELECT * FROM pharmacy_stock;
SELECT * FROM alternatives;
SELECT * FROM transactions;
SELECT * FROM waiting_list;
.quit
```

The database is only for persistent storage. The actual DSA processing remains in C++ using priority_queue, vector, unordered_map, searching and sorting.
