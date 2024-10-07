///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Common definitions used in C++ Test Suite for GDAL
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
/*
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_COMMON_H_INCLUDED
#define GDAL_COMMON_H_INCLUDED

#include "cpl_port.h"
#include "ogr_api.h"

#include <string>

#include "gtest_include.h"

#if defined(WIN32)
#define SEP "\\"
#else
#define SEP "/"
#endif

namespace tut
{

::testing::AssertionResult
CheckEqualGeometries(OGRGeometryH lhs, OGRGeometryH rhs, double tolerance);

namespace common
{

// Data directory path used by GDAL C++ Unit Tests subset
extern std::string const data_basedir;

// Temp directory path
extern std::string const tmp_basedir;

}  // namespace common
}  // namespace tut

#endif  // GDAL_COMMON_H_INCLUDED
