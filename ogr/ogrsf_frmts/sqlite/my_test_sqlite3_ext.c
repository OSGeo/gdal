#include "cpl_port.h"

#include <sqlite3ext.h>

SQLITE_EXTENSION_INIT1

static void myext(sqlite3_context* pContext, int argc, sqlite3_value** argv)
{
    (void)argc;
    (void)argv;
    sqlite3_result_text( pContext, "this works!", -1, SQLITE_TRANSIENT );
}

int CPL_DLL sqlite3_extension_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);

int sqlite3_extension_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi)
{
    SQLITE_EXTENSION_INIT2(pApi);
    (void)pzErrMsg;
    return sqlite3_create_function(db, "myext", 0, SQLITE_ANY, 0, myext, 0, 0);
}
