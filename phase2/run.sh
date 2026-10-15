#!/bin/bash
set -e
g++ Medicine_Distribution_Phase_2_SQLite.cpp -o medicine_phase2 -lsqlite3
./medicine_phase2
