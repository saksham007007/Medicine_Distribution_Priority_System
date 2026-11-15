#!/usr/bin/env python3
"""Generate database/schema.sql from the embedded C++ schema (single source of truth)."""
import re, pathlib
root = pathlib.Path(__file__).resolve().parent.parent
src = (root / "backend/include/schema.h").read_text()
sql = re.search(r'R"SQL\((.*)\)SQL"', src, re.S).group(1).strip() + "\n"
(root / "database/schema.sql").write_text("-- Generated from backend/include/schema.h by scripts/export_schema.py\n" + sql)
print("wrote database/schema.sql,", len(sql.splitlines()), "lines")
