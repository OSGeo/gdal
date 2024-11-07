/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Static registration of sqlite3 entry points
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"

#include <stddef.h>

#if !defined(_WIN32) && defined(HAVE_SQLITE3EXT_H)

#ifndef SQLITE_CORE
#define SQLITE_CORE
#endif
#include "sqlite3ext.h"

#define CONCAT(a, b) a##b
#define MEMBER(x) .x = CONCAT(sqlite3_, x)

/* clang-format off */

const struct sqlite3_api_routines OGRSQLITE_static_routines = {
    MEMBER(bind_blob),
    MEMBER(bind_double),
    MEMBER(bind_int),
    MEMBER(bind_int64),
    MEMBER(bind_null),
    MEMBER(bind_text),
    MEMBER(changes),
    MEMBER(close),
    MEMBER(column_blob),
    MEMBER(column_bytes),
    MEMBER(column_count),
    MEMBER(column_decltype),
    MEMBER(column_double),
    MEMBER(column_int),
    MEMBER(column_int64),
    MEMBER(column_name),
#ifdef SQLITE_HAS_COLUMN_METADATA
    MEMBER(column_table_name),
#endif
    MEMBER(column_text),
    MEMBER(column_type),
    MEMBER(create_function),
    MEMBER(create_module),
    MEMBER(declare_vtab),
    MEMBER(errcode),
    MEMBER(errmsg),
    MEMBER(exec),
    MEMBER(finalize),
    MEMBER(free),
    MEMBER(free_table),
    MEMBER(get_table),
    MEMBER(last_insert_rowid),
    MEMBER(libversion_number),
    MEMBER(malloc),
    MEMBER(mprintf),
    MEMBER(open),
    MEMBER(prepare),
    MEMBER(realloc),
    MEMBER(reset),
    MEMBER(result_blob),
    MEMBER(result_double),
    MEMBER(result_error),
    MEMBER(result_int),
    MEMBER(result_int64),
    MEMBER(result_null),
    MEMBER(result_value),
    .xsnprintf = sqlite3_snprintf,
    MEMBER(step),
    MEMBER(total_changes),
    MEMBER(user_data),
    MEMBER(value_blob),
    MEMBER(value_bytes),
    MEMBER(value_double),
    MEMBER(value_int),
    MEMBER(value_int64),
    MEMBER(value_numeric_type),
    MEMBER(value_text),
    MEMBER(value_type),
    MEMBER(vmprintf),
    MEMBER(prepare_v2),
    MEMBER(create_module_v2),
    MEMBER(open_v2),
    MEMBER(vfs_find),
    MEMBER(vfs_register),
    MEMBER(vfs_unregister),
};

/* clang-format on */

#endif  // !defined(_WIN32) && defined(HAVE_SQLITE3EXT_H)
