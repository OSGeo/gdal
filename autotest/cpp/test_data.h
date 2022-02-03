///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Locate test data for test suite
// Author:   Hiroshi Miura
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Hiroshi Miura
/*
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

#ifndef GDAL_TEST_DATA_H
#define GDAL_TEST_DATA_H

// Use GDAL_TEST_ROOT_DIR for the root directory of test project's source
#ifdef GDAL_TEST_ROOT_DIR

#ifndef SEP
#if defined(WIN32)
#define SEP "\\"
#else
#define SEP "/"
#endif
#endif

#define GCORE_DATA_DIR GDAL_TEST_ROOT_DIR SEP "gcore" SEP "data" SEP
#define GDRIVERS_DATA_DIR GDAL_TEST_ROOT_DIR SEP "gdrivers" SEP "data" SEP
#define GDRIVERS_DIR GDAL_TEST_ROOT_DIR SEP "gdrivers" SEP

#define TUT_ROOT_DATA_DIR GDAL_TEST_ROOT_DIR SEP "cpp" SEP "data"
#define TUT_ROOT_TMP_DIR GDAL_TEST_ROOT_DIR  SEP "cpp" SEP "tmp"

#else

#define GCORE_DATA_DIR "../gcore/data/"
#define GDRIVERS_DATA_DIR "../gdrivers/data/"
#define GDRIVERS_DIR "../gdrivers/"

#define TUT_ROOT_DATA_DIR "data"
#define TUT_ROOT_TMP_DIR "tmp"

#endif

#endif //GDAL_TEST_DATA_H
