/******************************************************************************
 * $Id$
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef NETCDF_CF_CONSTANTS_H_INCLUDED_
#define NETCDF_CF_CONSTANTS_H_INCLUDED_

#include "ogr_srs_cf1.h"

/* -------------------------------------------------------------------- */
/*       CF-1 or NUG (NetCDF User's Guide) defs                         */
/* -------------------------------------------------------------------- */

/* CF: http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html
 */
/* NUG: http://www.unidata.ucar.edu/software/netcdf/docs/netcdf.html#Variables
 */
#define CF_STD_NAME "standard_name"
#define CF_LNG_NAME "long_name"
#define CF_UNITS "units"
#define CF_ADD_OFFSET "add_offset"
#define CF_SCALE_FACTOR "scale_factor"
/* should be SRS_UL_METER but use meter now for compat with gtiff files */
#define CF_UNITS_M "metre"
#define CF_UNITS_D SRS_UA_DEGREE
#define CF_PROJ_X_VAR_NAME "x"
#define CF_PROJ_Y_VAR_NAME "y"
#define CF_PROJ_X_COORD "projection_x_coordinate"
#define CF_PROJ_Y_COORD "projection_y_coordinate"
#define CF_PROJ_X_COORD_LONG_NAME "x coordinate of projection"
#define CF_PROJ_Y_COORD_LONG_NAME "y coordinate of projection"
#define CF_GRD_MAPPING "grid_mapping"
#define CF_COORDINATES "coordinates"

#define CF_LONGITUDE_VAR_NAME "lon"
#define CF_LONGITUDE_STD_NAME "longitude"
#define CF_LONGITUDE_LNG_NAME "longitude"
#define CF_LATITUDE_VAR_NAME "lat"
#define CF_LATITUDE_STD_NAME "latitude"
#define CF_LATITUDE_LNG_NAME "latitude"
#define CF_DEGREES_NORTH "degrees_north" /* recommended */
#define CF_DEGREE_NORTH "degree_north"   /* acceptable */
#define CF_DEGREES_N "degrees_N"         /* acceptable */
#define CF_DEGREES_EAST "degrees_east"   /* recommended */
#define CF_DEGREE_EAST "degree_east"     /* acceptable */
#define CF_DEGREES_E "degrees_E"         /* acceptable */

#define CF_AXIS "axis"
/* #define CF_BOUNDS          "bounds" */
/* #define CF_ORIG_UNITS      "original_units" */

/* Simple Geometries Special Names from CF-1.8 Draft - Chapter 7 section
 * Geometries */
#define CF_SG_GEOMETRY "geometry"
#define CF_SG_GEOMETRY_DIMENSION "geometry_dimension"
#define CF_SG_GEOMETRY_TYPE "geometry_type"
#define CF_SG_INTERIOR_RING "interior_ring"
#define CF_SG_NODES "nodes"
#define CF_SG_NODE_COORDINATES "node_coordinates"
#define CF_SG_NODE_COUNT "node_count"
#define CF_SG_PART_NODE_COUNT "part_node_count"
#define CF_SG_TYPE_LINE "line"
#define CF_SG_TYPE_POINT "point"
#define CF_SG_TYPE_POLY "polygon"
#define CF_SG_X_AXIS "X"
#define CF_SG_Y_AXIS "Y"
#define CF_SG_Z_AXIS "Z"

#endif  // NETCDF_CF_CONSTANTS_H_INCLUDED_
