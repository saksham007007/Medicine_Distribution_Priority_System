CREATE TABLE patients(
 patient_id INTEGER PRIMARY KEY,name TEXT,urgency TEXT,priority INTEGER,
 medicine TEXT,quantity INTEGER,wait_time INTEGER,status TEXT);

CREATE TABLE medicines(
 medicine_id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT UNIQUE,
 category TEXT,stock INTEGER,expiry TEXT,min_stock INTEGER);

CREATE TABLE pharmacies(
 pharmacy_id INTEGER PRIMARY KEY,name TEXT,location TEXT,distance REAL);

CREATE TABLE pharmacy_stock(
 pharmacy_id INTEGER,medicine TEXT,stock INTEGER,
 PRIMARY KEY(pharmacy_id,medicine));

CREATE TABLE alternatives(
 medicine TEXT,alternative TEXT,
 PRIMARY KEY(medicine,alternative));

CREATE TABLE transactions(
 transaction_id INTEGER PRIMARY KEY,patient_id INTEGER,patient_name TEXT,
 medicine TEXT,quantity INTEGER,status TEXT);

CREATE TABLE waiting_list(
 waiting_id INTEGER PRIMARY KEY AUTOINCREMENT,patient_id INTEGER,
 medicine TEXT,quantity INTEGER,status TEXT);
