
#include <sqlite3.h>
#include "../include/sera.h"

http_response_t *handle_sqlite(http_request_t *req) {
    sqlite3 *db;
    sqlite3_open(req->path + 1, &db);
    const char *sql = "SELECT 'SQLite connected successfully';";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_step(stmt);
    const unsigned char *txt = sqlite3_column_text(stmt, 0);
    http_response_t *res = malloc(sizeof(http_response_t));
    res->status = 200;
    res->content_type = "text/plain";
    res->body = strdup((const char*)txt);
    res->body_len = strlen(res->body);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return res;
}
