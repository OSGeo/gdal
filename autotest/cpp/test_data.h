///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Locate test data for test suite
// Author:   Hiroshi Miura
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Hiroshi Miura
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_TEST_DATA_H
#define GDAL_TEST_DATA_H

// Use GDAL_TEST_ROOT_DIR for the root directory of test project's source
#ifdef GDAL_TEST_ROOT_DIR

#ifndef SEP
#if defined(_WIN32)
#define SEP "\\"
#else
#define SEP "/"
#endif
#endif

#define GCORE_DATA_DIR GDAL_TEST_ROOT_DIR SEP "gcore" SEP "data" SEP
#define GDRIVERS_DATA_DIR GDAL_TEST_ROOT_DIR SEP "gdrivers" SEP "data" SEP
#define GDRIVERS_DIR GDAL_TEST_ROOT_DIR SEP "gdrivers" SEP
#define UTILITIES_DATA_DIR GDAL_TEST_ROOT_DIR SEP "utilities" SEP "data" SEP

#define TUT_ROOT_DATA_DIR GDAL_TEST_ROOT_DIR SEP "cpp" SEP "data"
#define TUT_ROOT_TMP_DIR GDAL_TEST_ROOT_DIR SEP "cpp" SEP "tmp"

#else

#define GCORE_DATA_DIR "../gcore/data/"
#define GDRIVERS_DATA_DIR "../gdrivers/data/"
#define GDRIVERS_DIR "../gdrivers/"
#define UTILITIES_DATA_DIR "../utilities/data/"

#define TUT_ROOT_DATA_DIR "data"
#define TUT_ROOT_TMP_DIR "tmp"

#endif

#endif  // GDAL_TEST_DATA_H
