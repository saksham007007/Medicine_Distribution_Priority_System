// Canonical database schema. Embedded in the binary so the database is
// created automatically on first launch. (database/schema.sql is generated
// from this file by scripts/export_schema.py - do not edit it by hand.)
#pragma once
static const char* SCHEMA_SQL = R"SQL(
PRAGMA foreign_keys=ON;

CREATE TABLE IF NOT EXISTS patients(
  patient_id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL CHECK(length(trim(name))>=2),
  age INTEGER CHECK(age IS NULL OR (age>=0 AND age<=120)),
  gender TEXT CHECK(gender IS NULL OR gender IN('Male','Female','Other')),
  contact TEXT, email TEXT, address TEXT, city TEXT,
  latitude REAL CHECK(latitude IS NULL OR (latitude>=-90 AND latitude<=90)),
  longitude REAL CHECK(longitude IS NULL OR (longitude>=-180 AND longitude<=180)),
  active INTEGER NOT NULL DEFAULT 1,
  created_at TEXT NOT NULL DEFAULT(datetime('now')));

CREATE TABLE IF NOT EXISTS pharmacies(
  pharmacy_id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE, address TEXT NOT NULL, city TEXT NOT NULL, state TEXT NOT NULL,
  pin_code TEXT NOT NULL CHECK(length(pin_code)=6),
  latitude REAL CHECK(latitude IS NULL OR (latitude>=-90 AND latitude<=90)),
  longitude REAL CHECK(longitude IS NULL OR (longitude>=-180 AND longitude<=180)),
  phone TEXT, email TEXT, opening_time TEXT DEFAULT '09:00', closing_time TEXT DEFAULT '21:00',
  status TEXT NOT NULL DEFAULT 'Open' CHECK(status IN('Open','Closed','Suspended')),
  delivery_available INTEGER NOT NULL DEFAULT 0,
  is_demo INTEGER NOT NULL DEFAULT 1,
  created_at TEXT NOT NULL DEFAULT(datetime('now')));

CREATE TABLE IF NOT EXISTS users(
  user_id INTEGER PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL UNIQUE COLLATE NOCASE CHECK(length(username)>=3),
  password_hash TEXT NOT NULL,
  role TEXT NOT NULL CHECK(role IN('Admin','Pharmacist','Patient')),
  full_name TEXT NOT NULL, email TEXT,
  patient_id INTEGER REFERENCES patients(patient_id) ON DELETE SET NULL,
  pharmacy_id INTEGER REFERENCES pharmacies(pharmacy_id) ON DELETE SET NULL,
  active INTEGER NOT NULL DEFAULT 1,
  created_at TEXT NOT NULL DEFAULT(datetime('now')));

CREATE TABLE IF NOT EXISTS sessions(
  token_hash TEXT PRIMARY KEY,
  user_id INTEGER NOT NULL REFERENCES users(user_id) ON DELETE CASCADE,
  created_at TEXT NOT NULL DEFAULT(datetime('now')),
  expires_at TEXT NOT NULL);

CREATE TABLE IF NOT EXISTS medicine_categories(
  category_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE);

CREATE TABLE IF NOT EXISTS manufacturers(
  manufacturer_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE, city TEXT);

CREATE TABLE IF NOT EXISTS medicines(
  medicine_id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL UNIQUE, generic_name TEXT NOT NULL, brand_name TEXT NOT NULL,
  category_id INTEGER NOT NULL REFERENCES medicine_categories(category_id),
  manufacturer_id INTEGER NOT NULL REFERENCES manufacturers(manufacturer_id),
  composition TEXT NOT NULL, strength TEXT NOT NULL, dosage_form TEXT NOT NULL,
  requires_prescription INTEGER NOT NULL DEFAULT 0 CHECK(requires_prescription IN(0,1)),
  description TEXT, unit_price REAL NOT NULL CHECK(unit_price>=0), storage_instructions TEXT,
  status TEXT NOT NULL DEFAULT 'Active' CHECK(status IN('Active','Discontinued')),
  min_stock INTEGER NOT NULL DEFAULT 20 CHECK(min_stock>=0),
  is_demo INTEGER NOT NULL DEFAULT 1,
  created_at TEXT NOT NULL DEFAULT(datetime('now')));
CREATE INDEX IF NOT EXISTS idx_med_generic ON medicines(generic_name);
CREATE INDEX IF NOT EXISTS idx_med_category ON medicines(category_id);
CREATE INDEX IF NOT EXISTS idx_med_manuf ON medicines(manufacturer_id);
CREATE INDEX IF NOT EXISTS idx_med_form ON medicines(dosage_form);

CREATE TABLE IF NOT EXISTS pharmacy_inventory(
  pharmacy_id INTEGER NOT NULL REFERENCES pharmacies(pharmacy_id) ON DELETE CASCADE,
  medicine_id INTEGER NOT NULL REFERENCES medicines(medicine_id),
  quantity INTEGER NOT NULL DEFAULT 0 CHECK(quantity>=0),
  reserved_quantity INTEGER NOT NULL DEFAULT 0 CHECK(reserved_quantity>=0),
  reorder_level INTEGER NOT NULL DEFAULT 20 CHECK(reorder_level>=0),
  last_updated TEXT NOT NULL DEFAULT(datetime('now')),
  PRIMARY KEY(pharmacy_id,medicine_id),
  CHECK(reserved_quantity<=quantity));
CREATE INDEX IF NOT EXISTS idx_inv_medicine ON pharmacy_inventory(medicine_id);

CREATE TABLE IF NOT EXISTS medicine_batches(
  batch_id INTEGER PRIMARY KEY AUTOINCREMENT,
  pharmacy_id INTEGER NOT NULL, medicine_id INTEGER NOT NULL,
  batch_number TEXT NOT NULL, manufacturing_date TEXT NOT NULL, expiry_date TEXT NOT NULL,
  quantity INTEGER NOT NULL CHECK(quantity>=0),
  received_at TEXT NOT NULL DEFAULT(datetime('now')),
  UNIQUE(pharmacy_id,medicine_id,batch_number),
  CHECK(expiry_date>manufacturing_date),
  FOREIGN KEY(pharmacy_id,medicine_id) REFERENCES pharmacy_inventory(pharmacy_id,medicine_id) ON DELETE CASCADE);
CREATE INDEX IF NOT EXISTS idx_batch_expiry ON medicine_batches(expiry_date);
CREATE INDEX IF NOT EXISTS idx_batch_pm ON medicine_batches(pharmacy_id,medicine_id);
CREATE INDEX IF NOT EXISTS idx_batch_med ON medicine_batches(medicine_id,expiry_date);

CREATE TABLE IF NOT EXISTS prescriptions(
  prescription_id INTEGER PRIMARY KEY AUTOINCREMENT,
  patient_id INTEGER NOT NULL REFERENCES patients(patient_id),
  medicine_id INTEGER NOT NULL REFERENCES medicines(medicine_id),
  quantity INTEGER NOT NULL CHECK(quantity>0),
  doctor_name TEXT NOT NULL, doctor_reg_no TEXT, issued_date TEXT NOT NULL, notes TEXT,
  verified INTEGER NOT NULL DEFAULT 0, verified_by INTEGER REFERENCES users(user_id), verified_at TEXT,
  created_at TEXT NOT NULL DEFAULT(datetime('now')));
CREATE INDEX IF NOT EXISTS idx_rx_patient ON prescriptions(patient_id);

CREATE TABLE IF NOT EXISTS medicine_requests(
  request_id INTEGER PRIMARY KEY AUTOINCREMENT,
  patient_id INTEGER NOT NULL REFERENCES patients(patient_id),
  medicine_id INTEGER NOT NULL REFERENCES medicines(medicine_id),
  quantity INTEGER NOT NULL CHECK(quantity>0 AND quantity<=1000),
  urgency TEXT NOT NULL CHECK(urgency IN('Critical','High','Medium','Normal')),
  priority INTEGER NOT NULL CHECK(priority BETWEEN 1 AND 4),
  status TEXT NOT NULL DEFAULT 'Pending' CHECK(status IN('Pending','Waiting','Distributed','Rejected','Cancelled')),
  prescription_id INTEGER REFERENCES prescriptions(prescription_id),
  substitute_medicine_id INTEGER REFERENCES medicines(medicine_id),
  substitution_status TEXT NOT NULL DEFAULT 'None' CHECK(substitution_status IN('None','Proposed','Approved','Rejected')),
  substitution_by INTEGER REFERENCES users(user_id),
  fulfilled_pharmacy_id INTEGER REFERENCES pharmacies(pharmacy_id),
  notes TEXT,
  requested_at TEXT NOT NULL DEFAULT(datetime('now')),
  updated_at TEXT NOT NULL DEFAULT(datetime('now')));
CREATE INDEX IF NOT EXISTS idx_req_queue ON medicine_requests(status,priority,requested_at);
CREATE INDEX IF NOT EXISTS idx_req_patient ON medicine_requests(patient_id);
CREATE INDEX IF NOT EXISTS idx_req_medicine ON medicine_requests(medicine_id);

CREATE TABLE IF NOT EXISTS patient_priority_queue(
  queue_id INTEGER PRIMARY KEY AUTOINCREMENT,
  request_id INTEGER NOT NULL UNIQUE REFERENCES medicine_requests(request_id) ON DELETE CASCADE,
  priority INTEGER NOT NULL,
  status TEXT NOT NULL DEFAULT 'Queued' CHECK(status IN('Queued','Waiting','Processed','Removed')),
  enqueued_at TEXT NOT NULL DEFAULT(datetime('now')),
  updated_at TEXT NOT NULL DEFAULT(datetime('now')));

CREATE TABLE IF NOT EXISTS priority_history(
  history_id INTEGER PRIMARY KEY AUTOINCREMENT,
  request_id INTEGER NOT NULL REFERENCES medicine_requests(request_id) ON DELETE CASCADE,
  old_urgency TEXT, new_urgency TEXT NOT NULL,
  changed_by INTEGER REFERENCES users(user_id), reason TEXT,
  changed_at TEXT NOT NULL DEFAULT(datetime('now')));

CREATE TABLE IF NOT EXISTS waiting_list(
  waiting_id INTEGER PRIMARY KEY AUTOINCREMENT,
  request_id INTEGER NOT NULL REFERENCES medicine_requests(request_id) ON DELETE CASCADE,
  patient_id INTEGER NOT NULL REFERENCES patients(patient_id),
  medicine_id INTEGER NOT NULL REFERENCES medicines(medicine_id),
  quantity INTEGER NOT NULL CHECK(quantity>0),
  status TEXT NOT NULL DEFAULT 'Waiting' CHECK(status IN('Waiting','Completed','Cancelled')),
  added_at TEXT NOT NULL DEFAULT(datetime('now')), completed_at TEXT);
CREATE UNIQUE INDEX IF NOT EXISTS idx_wait_active ON waiting_list(request_id) WHERE status='Waiting';

CREATE TABLE IF NOT EXISTS medicine_alternatives(
  medicine_id INTEGER NOT NULL REFERENCES medicines(medicine_id) ON DELETE CASCADE,
  alternative_id INTEGER NOT NULL REFERENCES medicines(medicine_id) ON DELETE CASCADE,
  match_type TEXT NOT NULL CHECK(match_type IN('Same composition','Same generic, different strength','Same generic, different form')),
  PRIMARY KEY(medicine_id,alternative_id), CHECK(medicine_id<>alternative_id));

CREATE TABLE IF NOT EXISTS distribution_transactions(
  transaction_id INTEGER PRIMARY KEY AUTOINCREMENT,
  request_id INTEGER REFERENCES medicine_requests(request_id),
  patient_id INTEGER NOT NULL REFERENCES patients(patient_id), patient_name TEXT NOT NULL,
  medicine_id INTEGER NOT NULL REFERENCES medicines(medicine_id), medicine_name TEXT NOT NULL,
  pharmacy_id INTEGER REFERENCES pharmacies(pharmacy_id),
  quantity INTEGER NOT NULL CHECK(quantity>0),
  status TEXT NOT NULL CHECK(status IN('Distributed','Waiting','Rejected')),
  performed_by INTEGER REFERENCES users(user_id), notes TEXT,
  created_at TEXT NOT NULL DEFAULT(datetime('now')));
CREATE INDEX IF NOT EXISTS idx_txn_patient ON distribution_transactions(patient_id);
CREATE INDEX IF NOT EXISTS idx_txn_pharmacy ON distribution_transactions(pharmacy_id);
CREATE INDEX IF NOT EXISTS idx_txn_date ON distribution_transactions(created_at);

CREATE TABLE IF NOT EXISTS transaction_batches(
  transaction_id INTEGER NOT NULL REFERENCES distribution_transactions(transaction_id) ON DELETE CASCADE,
  batch_id INTEGER NOT NULL REFERENCES medicine_batches(batch_id),
  quantity INTEGER NOT NULL CHECK(quantity>0),
  PRIMARY KEY(transaction_id,batch_id));

CREATE TABLE IF NOT EXISTS notifications(
  notification_id INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER REFERENCES users(user_id) ON DELETE CASCADE,
  patient_id INTEGER REFERENCES patients(patient_id) ON DELETE CASCADE,
  target_role TEXT CHECK(target_role IS NULL OR target_role IN('Admin','Pharmacist')),
  type TEXT NOT NULL DEFAULT 'Info', title TEXT NOT NULL, message TEXT NOT NULL,
  is_read INTEGER NOT NULL DEFAULT 0,
  created_at TEXT NOT NULL DEFAULT(datetime('now')));
CREATE INDEX IF NOT EXISTS idx_notif_user ON notifications(user_id,is_read);
CREATE INDEX IF NOT EXISTS idx_notif_patient ON notifications(patient_id);

CREATE TABLE IF NOT EXISTS restocking_history(
  restock_id INTEGER PRIMARY KEY AUTOINCREMENT,
  pharmacy_id INTEGER NOT NULL REFERENCES pharmacies(pharmacy_id),
  medicine_id INTEGER NOT NULL REFERENCES medicines(medicine_id),
  batch_number TEXT NOT NULL, quantity INTEGER NOT NULL CHECK(quantity>0),
  performed_by INTEGER REFERENCES users(user_id),
  created_at TEXT NOT NULL DEFAULT(datetime('now')));

CREATE TABLE IF NOT EXISTS stock_movements(
  movement_id INTEGER PRIMARY KEY AUTOINCREMENT,
  pharmacy_id INTEGER NOT NULL REFERENCES pharmacies(pharmacy_id),
  medicine_id INTEGER NOT NULL REFERENCES medicines(medicine_id),
  change_qty INTEGER NOT NULL CHECK(change_qty<>0),
  reason TEXT NOT NULL CHECK(reason IN('Restock','Distribution','Expired write-off','Adjustment')),
  reference TEXT, performed_by INTEGER REFERENCES users(user_id),
  created_at TEXT NOT NULL DEFAULT(datetime('now')));
CREATE INDEX IF NOT EXISTS idx_mov_pm ON stock_movements(pharmacy_id,medicine_id);

CREATE TABLE IF NOT EXISTS audit_logs(
  log_id INTEGER PRIMARY KEY AUTOINCREMENT,
  user_id INTEGER, username TEXT, action TEXT NOT NULL, entity TEXT, entity_id TEXT, details TEXT,
  created_at TEXT NOT NULL DEFAULT(datetime('now')));
CREATE INDEX IF NOT EXISTS idx_audit_date ON audit_logs(created_at);

CREATE TABLE IF NOT EXISTS system_settings(
  key TEXT PRIMARY KEY, value TEXT NOT NULL, description TEXT,
  updated_at TEXT NOT NULL DEFAULT(datetime('now')));
)SQL";
