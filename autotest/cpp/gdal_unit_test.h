///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Common definitions used in C++ Test Suite for GDAL
// Author:   Mateusz Loskot <mateusz@loskot.net>
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2006, Mateusz Loskot <mateusz@loskot.net>
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA.
///////////////////////////////////////////////////////////////////////////////
#ifndef GDAL_COMMON_H_INCLUDED
#define GDAL_COMMON_H_INCLUDED

#include <tut_gdal.h>

#include <string>

namespace tut {
namespace common {

// Data directory path used by GDAL C++ Unit Tests subset
extern std::string const data_basedir;

// Temp directory path
extern std::string const tmp_basedir;

} // common
} // tut

#endif // GDAL_COMMON_H_INCLUDED
