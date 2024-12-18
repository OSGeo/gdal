/******************************************************************************
 *
 * Project:  GDAL SWIG Interfaces.
 * Purpose:  OGRErr handling typemap.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

%fragment("OGRErrMessages","header")

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
