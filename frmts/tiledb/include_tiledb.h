/******************************************************************************
 *
 * Project:  GDAL TileDB Driver
 * Purpose:  Include tiledb headers
 * Author:   TileDB, Inc
 *
 ******************************************************************************
 * Copyright (c) 2019, TileDB, Inc
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef INCLUDE_TILEDB_H
#define INCLUDE_TILEDB_H

#include "cpl_port.h"

#ifdef HAVE_GCC_SYSTEM_HEADER
#pragma GCC system_header
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) /* XXXX was deprecated */
#endif

#ifdef INCLUDE_ONLY_TILEDB_VERSION
#include "tiledb/tiledb_version.h"
#else
#include "tiledb/tiledb"
#include "tiledb/tiledb_experimental"
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if TILEDB_VERSION_MAJOR > 2 ||                                                \
    (TILEDB_VERSION_MAJOR == 2 && TILEDB_VERSION_MINOR >= 21)
#define HAS_TILEDB_GEOM_WKB_WKT
#endif

#endif  // INCLUDE_TILEDB_H
