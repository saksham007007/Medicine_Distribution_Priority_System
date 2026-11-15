// TEST-ONLY STUB (not part of the product). The sandbox has libsqlite3.so but
// no sqlite3.h, so this declares the subset of the real SQLite C API that the
// project uses. A real build on a developer machine must use the real header
// (libsqlite3-dev / SQLite amalgamation) - see docs/BUILD.md.
#pragma once
extern "C" {
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef long long sqlite3_int64;
#define SQLITE_OK 0
#define SQLITE_ROW 100
#define SQLITE_DONE 101
#define SQLITE_CONSTRAINT 19
#define SQLITE_INTEGER 1
#define SQLITE_FLOAT 2
#define SQLITE_TEXT 3
#define SQLITE_BLOB 4
#define SQLITE_NULL 5
#define SQLITE_TRANSIENT ((void(*)(void*))-1)
int sqlite3_open(const char*, sqlite3**);
int sqlite3_close(sqlite3*);
int sqlite3_busy_timeout(sqlite3*, int);
int sqlite3_exec(sqlite3*, const char*, int(*)(void*,int,char**,char**), void*, char**);
void sqlite3_free(void*);
const char* sqlite3_errmsg(sqlite3*);
int sqlite3_changes(sqlite3*);
sqlite3_int64 sqlite3_last_insert_rowid(sqlite3*);
int sqlite3_prepare_v2(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
int sqlite3_step(sqlite3_stmt*);
int sqlite3_finalize(sqlite3_stmt*);
int sqlite3_bind_null(sqlite3_stmt*, int);
int sqlite3_bind_int64(sqlite3_stmt*, int, sqlite3_int64);
int sqlite3_bind_double(sqlite3_stmt*, int, double);
int sqlite3_bind_text(sqlite3_stmt*, int, const char*, int, void(*)(void*));
int sqlite3_column_count(sqlite3_stmt*);
const char* sqlite3_column_name(sqlite3_stmt*, int);
int sqlite3_column_type(sqlite3_stmt*, int);
int sqlite3_column_int(sqlite3_stmt*, int);
sqlite3_int64 sqlite3_column_int64(sqlite3_stmt*, int);
double sqlite3_column_double(sqlite3_stmt*, int);
const unsigned char* sqlite3_column_text(sqlite3_stmt*, int);
}
