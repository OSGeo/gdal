/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Static registration of sqlite3 entry points
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "cpl_port.h"

#ifndef WIN32

#ifdef HAVE_SPATIALITE
  #ifdef SPATIALITE_AMALGAMATION
    /*
    / using an AMALGAMATED version of SpatiaLite
    / a private internal copy of SQLite is included:
    / so we are required including the SpatiaLite's 
    / own header 
    /
    / IMPORTANT NOTICE: using AMALAGATION is only
    / useful on Windows (to skip DLL hell related oddities)
    */
    #include <spatialite/sqlite3.h>
  #else
    /*
    / You MUST NOT use AMALGAMATION on Linux or any
    / other "sane" operating system !!!!
    */
    #include "sqlite3.h"
  #endif
#else
#include "sqlite3.h"
#endif

#if SQLITE_VERSION_NUMBER >= 3006000
#define HAVE_SQLITE_VFS
#define HAVE_SQLITE3_PREPARE_V2
#endif

#define DONT_UNDEF_SQLITE3_MACROS
#ifndef SQLITE_CORE
#define SQLITE_CORE
#endif
#include "ogrsqlite3ext.h"

const struct sqlite3_api_routines OGRSQLITE_static_routines =
{
  NULL, /*sqlite3_aggregate_context, */
  NULL, /*sqlite3_aggregate_count, */
  sqlite3_bind_blob, /* YES */
  sqlite3_bind_double, /* YES */
  sqlite3_bind_int, /* YES */
  sqlite3_bind_int64, /* YES */
  sqlite3_bind_null, /* YES */
  NULL, /*sqlite3_bind_parameter_count,*/
  NULL, /*sqlite3_bind_parameter_index,*/
  NULL, /*sqlite3_bind_parameter_name,*/
  sqlite3_bind_text, /* YES */
  NULL, /*sqlite3_bind_text16,*/
  NULL, /*sqlite3_bind_value,*/
  NULL, /*sqlite3_busy_handler,*/
  NULL, /*sqlite3_busy_timeout,*/
  sqlite3_changes, /* YES */
  sqlite3_close, /* YES */
  NULL, /*sqlite3_collation_needed,*/
  NULL, /*sqlite3_collation_needed16,*/
  sqlite3_column_blob, /* YES */
  sqlite3_column_bytes, /* YES */
  NULL, /*sqlite3_column_bytes16,*/
  sqlite3_column_count, /* YES */
  NULL, /*sqlite3_column_database_name,*/
  NULL, /*sqlite3_column_database_name16,*/
  sqlite3_column_decltype, /* YES */
  NULL, /*sqlite3_column_decltype16,*/
  sqlite3_column_double, /* YES */
  sqlite3_column_int, /* YES */
  sqlite3_column_int64, /* YES */
  sqlite3_column_name, /* YES */
  NULL, /*sqlite3_column_name16,*/
  NULL, /*sqlite3_column_origin_name,*/
  NULL, /*sqlite3_column_origin_name16,*/
#ifdef SQLITE_HAS_COLUMN_METADATA
  sqlite3_column_table_name, /* YES */
#else
  NULL,
#endif
  NULL, /*sqlite3_column_table_name16,*/
  sqlite3_column_text, /* YES */
  NULL, /*sqlite3_column_text16,*/
  sqlite3_column_type, /* YES */
  NULL, /*sqlite3_column_value,*/
  NULL, /*sqlite3_commit_hook,*/
  NULL, /*sqlite3_complete,*/
  NULL, /*sqlite3_complete16,*/
  NULL, /*sqlite3_create_collation,*/
  NULL, /*sqlite3_create_collation16,*/
  sqlite3_create_function,
  NULL, /*sqlite3_create_function16,*/
  sqlite3_create_module,
  NULL, /*sqlite3_data_count,*/
  NULL, /*sqlite3_db_handle,*/
  sqlite3_declare_vtab,
  NULL, /*sqlite3_enable_shared_cache,*/
  sqlite3_errcode,
  sqlite3_errmsg, /* YES */
  NULL, /*sqlite3_errmsg16,*/
  sqlite3_exec, /* YES */
  NULL, /*sqlite3_expired,*/
  sqlite3_finalize, /* YES */
  sqlite3_free, /* YES */
  sqlite3_free_table, /* YES */
  NULL, /*sqlite3_get_autocommit,*/
  NULL, /*sqlite3_get_auxdata,*/
  sqlite3_get_table, /* YES */
  NULL, /*sqlite3_global_recover,*/
  NULL, /*sqlite3_interrupt,*/
  sqlite3_last_insert_rowid, /* YES */
  NULL, /*sqlite3_libversion,*/
  sqlite3_libversion_number, /* YES */
  sqlite3_malloc,
  sqlite3_mprintf,
  sqlite3_open, /* YES */
  NULL, /*sqlite3_open16,*/
  sqlite3_prepare, /* YES */
  NULL, /*sqlite3_prepare16,*/
  NULL, /*sqlite3_profile,*/
  NULL, /*sqlite3_progress_handler,*/
  sqlite3_realloc,
  sqlite3_reset, /* YES */
  sqlite3_result_blob,
  sqlite3_result_double,
  sqlite3_result_error,
  NULL, /*sqlite3_result_error16,*/
  sqlite3_result_int,
  sqlite3_result_int64,
  sqlite3_result_null,
  sqlite3_result_text,
  NULL, /*sqlite3_result_text16,*/
  NULL, /*sqlite3_result_text16be,*/
  NULL, /*sqlite3_result_text16le,*/
  sqlite3_result_value,
  NULL, /*sqlite3_rollback_hook,*/
  NULL, /*sqlite3_set_authorizer,*/
  NULL, /*sqlite3_set_auxdata,*/
  sqlite3_snprintf,
  sqlite3_step, /* YES */
  sqlite3_table_column_metadata,
  NULL, /*sqlite3_thread_cleanup,*/
  sqlite3_total_changes,
  NULL, /*sqlite3_trace,*/
  NULL, /*sqlite3_transfer_bindings,*/
  NULL, /*sqlite3_update_hook,*/
  sqlite3_user_data,
  sqlite3_value_blob,
  sqlite3_value_bytes,
  NULL, /*sqlite3_value_bytes16,*/
  sqlite3_value_double,
  sqlite3_value_int,
  sqlite3_value_int64,
  sqlite3_value_numeric_type,
  sqlite3_value_text,
  NULL, /*sqlite3_value_text16,*/
  NULL, /*sqlite3_value_text16be,*/
  NULL, /*sqlite3_value_text16le,*/
  sqlite3_value_type,
  sqlite3_vmprintf,
  /* Added ??? */
 NULL, /*sqlite3_overload_function,*/
  /* Added by 3.3.13 */
#ifdef HAVE_SQLITE3_PREPARE_V2
 sqlite3_prepare_v2, /* YES */
#else
 NULL,
#endif
 NULL, /*sqlite3_prepare16_v2,*/
 NULL, /*sqlite3_clear_bindings,*/
  /* Added by 3.4.1 */
#ifdef HAVE_SQLITE_VFS
 sqlite3_create_module_v2,
#endif
  /* Added by 3.5.0 */
 NULL, /*sqlite3_bind_zeroblob,*/
 NULL, /*sqlite3_blob_bytes,*/
 NULL, /*sqlite3_blob_close,*/
 NULL, /*sqlite3_blob_open,*/
 NULL, /*sqlite3_blob_read,*/
 NULL, /*sqlite3_blob_write,*/
 NULL, /*sqlite3_create_collation_v2,*/
 NULL, /*sqlite3_file_control,*/
  NULL, /*sqlite3_memory_highwater,*/
  NULL, /*sqlite3_memory_used,*/
  NULL, /*sqlite3_mutex_alloc,*/
  NULL, /*sqlite3_mutex_enter,*/
  NULL, /*sqlite3_mutex_free,*/
  NULL, /*sqlite3_mutex_leave,*/
 NULL, /*sqlite3_mutex_try,*/
#ifdef HAVE_SQLITE_VFS
 sqlite3_open_v2, /* YES */
#else
 NULL,
#endif
 NULL, /*sqlite3_release_memory,*/
  NULL, /*sqlite3_result_error_nomem,*/
  NULL, /*sqlite3_result_error_toobig,*/
 NULL, /*sqlite3_sleep,*/
  NULL, /*sqlite3_soft_heap_limit,*/
#ifdef HAVE_SQLITE_VFS
  sqlite3_vfs_find, /* YES */
 sqlite3_vfs_register, /* YES */
 sqlite3_vfs_unregister, /* YES */
#else
 NULL,
 NULL,
 NULL,
#endif
 NULL, /*sqlite3_threadsafe,*/
  NULL, /*sqlite3_result_zeroblob,*/
  NULL, /*sqlite3_result_error_code,*/
 NULL, /*sqlite3_test_control,*/
  NULL, /*sqlite3_randomness,*/
  NULL, /*sqlite3_context_db_handle,*/
 NULL, /*sqlite3_extended_result_codes,*/
 NULL, /*sqlite3_limit,*/
  NULL, /*sqlite3_next_stmt,*/
  NULL, /*sqlite3_sql,*/
 NULL, /*sqlite3_status,*/
 NULL, /*sqlite3_backup_finish,*/
  NULL, /*sqlite3_backup_init,*/
 NULL, /*sqlite3_backup_pagecount,*/
 NULL, /*sqlite3_backup_remaining,*/
 NULL, /*sqlite3_backup_step,*/
  NULL,/*sqlite3_compileoption_get,*/
 NULL,/*sqlite3_compileoption_used,*/
 NULL,/*sqlite3_create_function_v2,*/
 NULL, /*sqlite3_db_config,*/
  NULL, /*sqlite3_db_mutex,*/
 NULL, /*sqlite3_db_status,*/
 NULL, /*sqlite3_extended_errcode,*/
  NULL, /*sqlite3_log,*/
  NULL, /*sqlite3_soft_heap_limit64,*/
  NULL, /*sqlite3_sourceid,*/
 NULL, /*sqlite3_stmt_status,*/
 NULL, /*sqlite3_strnicmp,*/
 NULL, /*sqlite3_unlock_notify,*/
 NULL, /*sqlite3_wal_autocheckpoint,*/
 NULL, /*sqlite3_wal_checkpoint,*/
  NULL, /*sqlite3_wal_hook,*/
 NULL, /*sqlite3_blob_reopen,*/
 NULL, /*sqlite3_vtab_config,*/
 NULL, /*sqlite3_vtab_on_conflict,*/
};

#endif // WIN32
