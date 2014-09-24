///////////////////////////////////////////////////////////////////////////////
// $Id: gdal_common.h,v 1.4 2007/01/04 18:15:54 mloskot Exp $
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
//
//  $Log: gdal_common.h,v $
//  Revision 1.4  2007/01/04 18:15:54  mloskot
//  Updated C++ Unit Test package for Windows CE
//
//  Revision 1.3  2006/12/06 15:39:13  mloskot
//  Added file header comment and copyright note.
//
//
///////////////////////////////////////////////////////////////////////////////
#ifndef GDAL_COMMON_H_INCLUDED
#define GDAL_COMMON_H_INCLUDED

#include <string>

namespace tut {
namespace common {

#ifdef _WIN32_WCE

// GDAL test and shared data location
extern std::string const gdal_dir;

// GDAL and PROJ.4 dictionaries location
extern std::string const gdal_dictdir;

#endif // _WIN32_WCE

// Data directory path used by GDAL C++ Unit Tests subset
extern std::string const data_basedir;

// Temp directory path
extern std::string const tmp_basedir;

} // common
} // tut

#endif // GDAL_COMMON_H_INCLUDED
