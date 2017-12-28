///////////////////////////////////////////////////////////////////////////////
//
// Project:  C++ Test Suite for GDAL/OGR
// Purpose:  Locate test data for test suite
// Author:   Hiroshi Miura
//
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017, Hiroshi Miura
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

#ifndef GDAL_TEST_DATA_H
#define GDAL_TEST_DATA_H

// Use GDAL_TEST_ROOT_DIR for the root directory of test project's source
#ifdef GDAL_TEST_ROOT_DIR
#define GCORE_DATA_DIR GDAL_TEST_ROOT_DIR "/gcore/data/"
#define GDRIVERS_DIR GDAL_TEST_ROOT_DIR "/gdrivers/"
#else
#define GCORE_DATA_DIR "../gcore/data/"
#define GDRIVERS_DIR "../gdrivers/"
#endif

#endif //GDAL_TEST_DATA_H
