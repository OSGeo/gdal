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

#ifndef NETCDF_CF_CONSTANTS_H_INCLUDED_
#define NETCDF_CF_CONSTANTS_H_INCLUDED_

/* -------------------------------------------------------------------- */
/*       CF-1 or NUG (NetCDF User's Guide) defs                         */
/* -------------------------------------------------------------------- */

/* CF: http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html */
/* NUG: http://www.unidata.ucar.edu/software/netcdf/docs/netcdf.html#Variables */
#define CF_STD_NAME          "standard_name"
#define CF_LNG_NAME          "long_name"
#define CF_UNITS             "units"
#define CF_ADD_OFFSET        "add_offset"
#define CF_SCALE_FACTOR      "scale_factor"
/* should be SRS_UL_METER but use meter now for compat with gtiff files */
#define CF_UNITS_M           "metre"
#define CF_UNITS_D           SRS_UA_DEGREE
#define CF_PROJ_X_VAR_NAME   "x"
#define CF_PROJ_Y_VAR_NAME   "y"
#define CF_PROJ_X_COORD      "projection_x_coordinate"
#define CF_PROJ_Y_COORD      "projection_y_coordinate"
#define CF_PROJ_X_COORD_LONG_NAME "x coordinate of projection"
#define CF_PROJ_Y_COORD_LONG_NAME "y coordinate of projection"
#define CF_GRD_MAPPING_NAME  "grid_mapping_name"
#define CF_GRD_MAPPING       "grid_mapping"
#define CF_COORDINATES       "coordinates"

#define CF_LONGITUDE_VAR_NAME      "lon"
#define CF_LONGITUDE_STD_NAME      "longitude"
#define CF_LONGITUDE_LNG_NAME      "longitude"
#define CF_LATITUDE_VAR_NAME       "lat"
#define CF_LATITUDE_STD_NAME       "latitude"
#define CF_LATITUDE_LNG_NAME       "latitude"
#define CF_DEGREES_NORTH           "degrees_north" /* recommended */
#define CF_DEGREE_NORTH            "degree_north"  /* acceptable */
#define CF_DEGREES_N               "degrees_N"     /* acceptable */
#define CF_DEGREES_EAST            "degrees_east"  /* recommended */
#define CF_DEGREE_EAST             "degree_east"   /* acceptable */
#define CF_DEGREES_E               "degrees_E"     /* acceptable */

#define CF_AXIS            "axis"
/* #define CF_BOUNDS          "bounds" */
/* #define CF_ORIG_UNITS      "original_units" */

/* -------------------------------------------------------------------- */
/*      CF-1 convention standard variables related to                   */
/*      mapping & projection - see http://cf-pcmdi.llnl.gov/            */
/* -------------------------------------------------------------------- */

/* projection types */
#define CF_PT_AEA                    "albers_conical_equal_area"
#define CF_PT_AE                     "azimuthal_equidistant"
#define CF_PT_CEA                    "cylindrical_equal_area"
#define CF_PT_LAEA                   "lambert_azimuthal_equal_area"
#define CF_PT_LCEA                   "lambert_cylindrical_equal_area"
#define CF_PT_LCC                    "lambert_conformal_conic"
#define CF_PT_TM                     "transverse_mercator"
#define CF_PT_LATITUDE_LONGITUDE     "latitude_longitude"
#define CF_PT_MERCATOR               "mercator"
#define CF_PT_ORTHOGRAPHIC           "orthographic"
#define CF_PT_POLAR_STEREO           "polar_stereographic"
#define CF_PT_STEREO                 "stereographic"
#define CF_PT_GEOS                   "geostationary"

/* projection parameters */
#define CF_PP_STD_PARALLEL           "standard_parallel"
/* CF uses only "standard_parallel" */
#define CF_PP_STD_PARALLEL_1         "standard_parallel_1"
#define CF_PP_STD_PARALLEL_2         "standard_parallel_2"
#define CF_PP_CENTRAL_MERIDIAN       "central_meridian"
#define CF_PP_LONG_CENTRAL_MERIDIAN  "longitude_of_central_meridian"
#define CF_PP_LON_PROJ_ORIGIN        "longitude_of_projection_origin"
#define CF_PP_LAT_PROJ_ORIGIN        "latitude_of_projection_origin"
/* #define PROJ_X_ORIGIN             "projection_x_coordinate_origin" */
/* #define PROJ_Y_ORIGIN             "projection_y_coordinate_origin" */
#define CF_PP_EARTH_SHAPE            "GRIB_earth_shape"
#define CF_PP_EARTH_SHAPE_CODE       "GRIB_earth_shape_code"
/* scale_factor is not CF, there are two possible translations  */
/* for WKT scale_factor : SCALE_FACTOR_MERIDIAN and SCALE_FACTOR_ORIGIN */
#define CF_PP_SCALE_FACTOR_MERIDIAN  "scale_factor_at_central_meridian"
#define CF_PP_SCALE_FACTOR_ORIGIN    "scale_factor_at_projection_origin"
#define CF_PP_VERT_LONG_FROM_POLE    "straight_vertical_longitude_from_pole"
#define CF_PP_FALSE_EASTING          "false_easting"
#define CF_PP_FALSE_NORTHING         "false_northing"
#define CF_PP_EARTH_RADIUS           "earth_radius"
#define CF_PP_EARTH_RADIUS_OLD       "spherical_earth_radius_meters"
#define CF_PP_INVERSE_FLATTENING     "inverse_flattening"
#define CF_PP_LONG_PRIME_MERIDIAN    "longitude_of_prime_meridian"
#define CF_PP_SEMI_MAJOR_AXIS        "semi_major_axis"
#define CF_PP_SEMI_MINOR_AXIS        "semi_minor_axis"
#define CF_PP_VERT_PERSP             "vertical_perspective" /*not used yet */
#define CF_PP_PERSPECTIVE_POINT_HEIGHT "perspective_point_height"
#define CF_PP_SWEEP_ANGLE_AXIS        "sweep_angle_axis"
#define CF_PP_GRID_NORTH_POLE_LONGITUDE "grid_north_pole_longitude"
#define CF_PP_GRID_NORTH_POLE_LATITUDE  "grid_north_pole_latitude"
#define CF_PP_NORTH_POLE_GRID_LONGITUDE "north_pole_grid_longitude"

/* Simple Geometries Special Names from CF-1.8 Draft - Chapter 7 section Geometries */
#define CF_SG_GEOMETRY               "geometry"
#define CF_SG_GEOMETRY_DIMENSION     "geometry_dimension"
#define CF_SG_GEOMETRY_TYPE          "geometry_type"
#define CF_SG_INTERIOR_RING          "interior_ring"
#define CF_SG_NODES                  "nodes"
#define CF_SG_NODE_COORDINATES       "node_coordinates"
#define CF_SG_NODE_COUNT             "node_count"
#define CF_SG_PART_NODE_COUNT        "part_node_count"
#define CF_SG_TYPE_LINE              "line"
#define CF_SG_TYPE_POINT             "point"
#define CF_SG_TYPE_POLY              "polygon"
#define CF_SG_X_AXIS                 "X"
#define CF_SG_Y_AXIS                 "Y"
#define CF_SG_Z_AXIS                 "Z"

#endif // NETCDF_CF_CONSTANTS_H_INCLUDED_
