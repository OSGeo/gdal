/******************************************************************************
 * $Id$
 *
 * Project:  GDAL SWIG Interfaces.
 * Purpose:  OGRErr handling typemap.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
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
 *****************************************************************************/

#ifdef SWIGRUBY
%header 
#else
%fragment("OGRErrMessages","header") 
#endif
%{

#include "ogr_core.h"
static char const *
OGRErrMessages( int rc ) {
  switch( rc ) {
  case OGRERR_NONE:
    return "OGR Error: None";
  case OGRERR_NOT_ENOUGH_DATA:
    return "OGR Error: Not enough data to deserialize";
  case OGRERR_NOT_ENOUGH_MEMORY:
    return "OGR Error: Not enough memory";
  case OGRERR_UNSUPPORTED_GEOMETRY_TYPE:
    return "OGR Error: Unsupported geometry type";
  case OGRERR_UNSUPPORTED_OPERATION:
    return "OGR Error: Unsupported operation";
  case OGRERR_CORRUPT_DATA:
    return "OGR Error: Corrupt data";
  case OGRERR_FAILURE:
    return "OGR Error: General Error";
  case OGRERR_UNSUPPORTED_SRS:
    return "OGR Error: Unsupported SRS";
  case OGRERR_INVALID_HANDLE:
    return "OGR Error: Invalid handle";
  case OGRERR_NON_EXISTING_FEATURE:
    return "OGR Error: Non existing feature";
  default:
    return "OGR Error: Unknown";
  }
}
%}
