/******************************************************************************
 *
 * Purpose:  Bulk loading of SQLite R*Tree tables
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2020- 2023 Joshua J Baker
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef SQLITE_RTREE_BULK_LOAD_H_INCLUDED
#define SQLITE_RTREE_BULK_LOAD_H_INCLUDED

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sqlite3.h"

/** Can be defined to mangle public symbols of this library, for the purpose
 * of vendorizing without conflicting with other code.
 * e.g you could set "#define SQLITE_RTREE_BL_SYMBOL(x) mylib_##x"
 */
#ifndef SQLITE_RTREE_BL_SYMBOL
#define SQLITE_RTREE_BL_SYMBOL(x) x
#endif

/** Opaque type for a RTree */
typedef struct sqlite_rtree_bl sqlite_rtree_bl;

/** Creates a new R*Tree.
 * @param sqlite_page_size The page size of the target SQLite database, as
 *                         typically determined by "PRAGMA page_size"
 * @return a RTree to free with sqlite_rtree_bl_free(), or NULL if the system
 *         is out of memory.
 */
sqlite_rtree_bl *SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_new)(int sqlite_page_size);

/** Insert a new row into the RTree.
 * The double values are rounded to float in an appropriate way by the
 * function.
 * @return true in case of success, false in case of error
 */
bool SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_insert)(
                            sqlite_rtree_bl *tr,
                            int64_t fid,
                            double minx, double miny, double maxx, double maxy);

/** Serialize the RTree into the database.
 *
 * This method issues a
 * CREATE VIRTUAL TABLE rtree_name USING rtree(rowid_colname, minx_colname, miny_colname, maxx_colname, maxy_colname)
 * and then iterates over the RTree content to populate the SQLite RTree
 * _node, _parent and _rowid implementation tables.
 *
 * It is the responsibility of the caller to issue BEGIN / COMMIT statements
 * around the call to that function for faster speed.
 *
 * @param tr RTree.
 * @param hDB Handle to SQLite database.
 * @param rtree_name Name of the R*Tree virtual table.
 * @param rowid_colname Name of the rowid column in the R*Tree virtual table.
 * @param minx_colname Name of the minx column in the R*Tree virtual table.
 * @param miny_colname Name of the miny column in the R*Tree virtual table.
 * @param maxx_colname Name of the maxx column in the R*Tree virtual table.
 * @param maxy_colname Name of the maxy column in the R*Tree virtual table.
 * @param p_error_msg NULL, or pointer to a string that will receive an error
 *                    message. *p_error_msg must be freed with sqlite3_free().
 * @return true in case of success, false in case of error
 */
bool SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_serialize)(
                               const sqlite_rtree_bl *tr,
                               sqlite3* hDB,
                               const char* rtree_name,
                               const char* rowid_colname,
                               const char* minx_colname,
                               const char* miny_colname,
                               const char* maxx_colname,
                               const char* maxy_colname,
                               char** p_error_msg);

/** Get an approximate value, in bytes, of the current RAM usage of the RTree.
 *
 * This is typically number_of_rows * 24 * 1.7
 */
size_t SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_ram_usage)(const sqlite_rtree_bl* tr);

/** Free the rtree.
 */
void SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_free)(sqlite_rtree_bl* tr);

/** Progress callback. */
typedef bool (*sqlite_rtree_progress_callback)(const char* message, void* user_data);

/** Creates a SQLite R*Tree from an existing feature table.
 *
 * This method issues a
 * CREATE VIRTUAL TABLE rtree_name USING rtree(rowid_colname, minx_colname, miny_colname, maxx_colname, maxy_colname)
 * and then iterates over the feature table to populate the SQLite RTree
 * _node, _parent and _rowid implementation tables.
 *
 * The ST_MinX, ST_MinY, ST_MaxX, ST_MaxY and ST_IsEmpty SQL functions must
 * be available.
 *
 * It is the responsibility of the caller to issue BEGIN / COMMIT statements
 * around the call to that function for faster speed.
 *
 * @param hDB Handle to SQLite database.
 * @param feature_table_name Feature table name
 * @param feature_table_fid_colname Name of the feature ID column in the feature table.
 * @param feature_table_geom_colname Name of the geometry column in the feature table.
 * @param rtree_name Name of the R*Tree virtual table.
 * @param rowid_colname Name of the rowid column in the R*Tree virtual table.
 * @param minx_colname Name of the minx column in the R*Tree virtual table.
 * @param miny_colname Name of the miny column in the R*Tree virtual table.
 * @param maxx_colname Name of the maxx column in the R*Tree virtual table.
 * @param maxy_colname Name of the maxy column in the R*Tree virtual table.
 * @param max_ram_usage Max RAM usage, in bytes, allowed for the in-memory RTree.
 *                      Once reached, slower insertion into the R*Tree virtual
 *                      table will be used. 0 means unlimited.
 * @param p_error_msg NULL, or pointer to a string that will receive an error
 *                    message. *p_error_msg must be freed with sqlite3_free().
 * @param progress_cbk Optional progress callback (that can return false to
 *                     stop processing), or NULL
 * @param progress_cbk_user_data User data to provide to the callback, or NULL
 * @return true in case of success, false in case of error
 */
bool SQLITE_RTREE_BL_SYMBOL(sqlite_rtree_bl_from_feature_table)(
                               sqlite3* hDB,
                               const char* feature_table_name,
                               const char* feature_table_fid_colname,
                               const char* feature_table_geom_colname,
                               const char* rtree_name,
                               const char* rowid_colname,
                               const char* minx_colname,
                               const char* miny_colname,
                               const char* maxx_colname,
                               const char* maxy_colname,
                               size_t max_ram_usage,
                               char** p_error_msg,
                               sqlite_rtree_progress_callback progress_cbk,
                               void* progress_cbk_user_data);

#ifdef __cplusplus
}
#endif

#endif // SQLITE_RTREE_BULK_LOAD_H_INCLUDED
