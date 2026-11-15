#!/usr/bin/env python3
"""Validates a freshly seeded database. Usage: python3 tests/test_database.py path/to/medicine_system.db"""
import sys, sqlite3
c = sqlite3.connect(sys.argv[1]); passed = failed = 0
def check(cond, name, detail=""):
    global passed, failed
    if cond: passed += 1
    else: failed += 1; print("  FAIL:", name, detail)
q = lambda s: c.execute(s).fetchone()[0]
tables = {r[0] for r in c.execute("select name from sqlite_master where type='table'")}
need = {"users","patients","medicines","medicine_categories","manufacturers","pharmacies","pharmacy_inventory","medicine_batches","prescriptions","medicine_requests",
        "patient_priority_queue","waiting_list","medicine_alternatives","distribution_transactions","notifications","restocking_history","audit_logs","system_settings"}
check(need <= tables, "all 18 required tables exist", need - tables)
check(q("select count(*) from medicines") >= 1000, "at least 1000 medicines", q("select count(*) from medicines"))
check(q("select count(distinct name) from medicines") == q("select count(*) from medicines"), "medicine names are unique")
check(q("select count(*) from (select 1 from medicines group by generic_name,strength,dosage_form,brand_name having count(*)>1)") == 0, "no duplicate (brand,generic,strength,form)")
forms = q("select count(*) from (select 1 from medicines group by generic_name,strength,dosage_form)")
check(forms >= 500, "at least 500 distinct formulations (not copies of a few medicines)", forms)
check(q("select count(distinct generic_name) from medicines") >= 200, "200+ distinct generic ingredients", q("select count(distinct generic_name) from medicines"))
check(q("select count(*) from medicines where trim(name)='' or trim(generic_name)='' or trim(composition)='' or trim(strength)='' or description is null or storage_instructions is null or unit_price<=0") == 0, "no empty medicine fields")
check(q("select count(*) from manufacturers") >= 20 and q("select count(*) from medicine_categories") >= 12, "manufacturers and categories populated")
check(q("select count(*) from pharmacies") >= 30, "at least 30 pharmacies")
check(q("select count(distinct city) from pharmacies") >= 10, "pharmacies across 10+ cities")
check(q("select count(*) from pharmacies where latitude is null or longitude is null or phone is null or email is null or pin_code is null") == 0, "pharmacy contact/location fields complete")
check(q("select count(*) from pharmacies where phone not like '+91-00000-%'") == 0, "pharmacy phone numbers are clearly synthetic")
check(q("select count(*) from pharmacy_inventory") > 3000, "substantial pharmacy inventory")
check(q("select count(*) from (select pharmacy_id from pharmacy_inventory group by pharmacy_id having count(*) > (select count(*) from medicines)*0.6)") == 0, "no pharmacy stocks (almost) every medicine")
check(q("select count(*) from medicines m where not exists(select 1 from pharmacy_inventory i where i.medicine_id=m.medicine_id)") > 20, "some medicines are stocked nowhere (realistic shortages)")
check(q("select count(*) from medicine_batches where expiry_date<date('now')") > 0, "sample includes expired batches")
check(q("select count(*) from medicine_batches where expiry_date between date('now') and date('now','+60 days')") > 0, "sample includes near-expiry batches")
check(q("select count(*) from pharmacy_inventory i where (select coalesce(sum(quantity),0) from medicine_batches b where b.pharmacy_id=i.pharmacy_id and b.medicine_id=i.medicine_id and b.expiry_date>=date('now'))<=i.reorder_level") > 0, "sample includes low-stock rows")
check(q("select count(*) from pharmacy_inventory i where quantity<>(select coalesce(sum(quantity),0) from medicine_batches b where b.pharmacy_id=i.pharmacy_id and b.medicine_id=i.medicine_id)") == 0, "inventory totals equal batch sums")
check(q("select count(*) from medicine_batches where expiry_date<=manufacturing_date or manufacturing_date>date('now')") == 0, "batch dates are valid")
check(q("select count(*) from medicine_alternatives") > 1000, "alternatives table populated")
check(q("select count(*) from medicine_alternatives a join medicines x on x.medicine_id=a.medicine_id join medicines y on y.medicine_id=a.alternative_id where x.generic_name<>y.generic_name or x.dosage_form<>y.dosage_form") == 0, "alternatives never cross generic or dosage form (no name-only matches)")
check(q("select count(*) from users") >= 3 and q("select count(distinct role) from users")==3, "demo users for all three roles")
check(q("select count(*) from users where password_hash not like 'pbkdf2$%'") == 0, "all passwords are salted PBKDF2 hashes")
check(q("select count(distinct password_hash) from users where username in ('pharmacist','pharmacist.doon','patient','patient2')") == 4, "same password -> different hashes (per-user salt)")
check(q("select count(*) from medicine_requests where status='Pending'") >= 10 and q("select count(*) from waiting_list where status='Waiting'") >= 1, "demo queue and waiting list present")
check(q("select count(*) from distribution_transactions") >= 30, "transaction history for analytics")
check(q("pragma foreign_keys") in (0, 1), "pragma readable")
check(c.execute("pragma foreign_key_check").fetchall() == [], "no foreign key violations")
check(c.execute("pragma integrity_check").fetchone()[0] == "ok", "integrity_check ok")
check(q("select count(*) from sqlite_master where type='index' and name like 'idx_%'") >= 15, "indexes created", q("select count(*) from sqlite_master where type='index' and name like 'idx_%'"))
print(f"{passed} passed, {failed} failed"); sys.exit(1 if failed else 0)
