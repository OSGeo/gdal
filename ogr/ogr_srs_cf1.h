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

#ifndef OGR_SRS_CF1_INCLUDED
#define OGR_SRS_CF1_INCLUDED

#define NCDF_CRS_WKT "crs_wkt"
#define NCDF_SPATIAL_REF "spatial_ref"

/* -------------------------------------------------------------------- */
/*      CF-1 convention standard variables related to                   */
/*      mapping & projection - see http://cf-pcmdi.llnl.gov/            */
/* -------------------------------------------------------------------- */

#define CF_GRD_MAPPING_NAME "grid_mapping_name"

#define CF_PRIME_MERIDIAN_NAME "prime_meridian_name"
#define CF_REFERENCE_ELLIPSOID_NAME "reference_ellipsoid_name"
#define CF_HORIZONTAL_DATUM_NAME "horizontal_datum_name"
#define CF_GEOGRAPHIC_CRS_NAME "geographic_crs_name"
#define CF_PROJECTED_CRS_NAME "projected_crs_name"

/* projection types */
#define CF_PT_AEA "albers_conical_equal_area"
#define CF_PT_AE "azimuthal_equidistant"
#define CF_PT_CEA "cylindrical_equal_area"
#define CF_PT_LAEA "lambert_azimuthal_equal_area"
#define CF_PT_LCEA "lambert_cylindrical_equal_area"
#define CF_PT_LCC "lambert_conformal_conic"
#define CF_PT_TM "transverse_mercator"
#define CF_PT_LATITUDE_LONGITUDE "latitude_longitude"
#define CF_PT_MERCATOR "mercator"
#define CF_PT_ORTHOGRAPHIC "orthographic"
#define CF_PT_POLAR_STEREO "polar_stereographic"
#define CF_PT_STEREO "stereographic"
#define CF_PT_GEOS "geostationary"
#define CF_PT_ROTATED_LATITUDE_LONGITUDE "rotated_latitude_longitude"

/* projection parameters */
#define CF_PP_STD_PARALLEL "standard_parallel"
/* CF uses only "standard_parallel" */
#define CF_PP_STD_PARALLEL_1 "standard_parallel_1"
#define CF_PP_STD_PARALLEL_2 "standard_parallel_2"
#define CF_PP_CENTRAL_MERIDIAN "central_meridian"
#define CF_PP_LONG_CENTRAL_MERIDIAN "longitude_of_central_meridian"
#define CF_PP_LON_PROJ_ORIGIN "longitude_of_projection_origin"
#define CF_PP_LAT_PROJ_ORIGIN "latitude_of_projection_origin"
/* #define PROJ_X_ORIGIN             "projection_x_coordinate_origin" */
/* #define PROJ_Y_ORIGIN             "projection_y_coordinate_origin" */
#define CF_PP_EARTH_SHAPE "GRIB_earth_shape"
#define CF_PP_EARTH_SHAPE_CODE "GRIB_earth_shape_code"
/* scale_factor is not CF, there are two possible translations  */
/* for WKT scale_factor : SCALE_FACTOR_MERIDIAN and SCALE_FACTOR_ORIGIN */
#define CF_PP_SCALE_FACTOR_MERIDIAN "scale_factor_at_central_meridian"
#define CF_PP_SCALE_FACTOR_ORIGIN "scale_factor_at_projection_origin"
#define CF_PP_VERT_LONG_FROM_POLE "straight_vertical_longitude_from_pole"
#define CF_PP_FALSE_EASTING "false_easting"
#define CF_PP_FALSE_NORTHING "false_northing"
#define CF_PP_EARTH_RADIUS "earth_radius"
#define CF_PP_EARTH_RADIUS_OLD "spherical_earth_radius_meters"
#define CF_PP_INVERSE_FLATTENING "inverse_flattening"
#define CF_PP_LONG_PRIME_MERIDIAN "longitude_of_prime_meridian"
#define CF_PP_SEMI_MAJOR_AXIS "semi_major_axis"
#define CF_PP_SEMI_MINOR_AXIS "semi_minor_axis"
#define CF_PP_VERT_PERSP "vertical_perspective" /*not used yet */
#define CF_PP_PERSPECTIVE_POINT_HEIGHT "perspective_point_height"
#define CF_PP_SWEEP_ANGLE_AXIS "sweep_angle_axis"
#define CF_PP_GRID_NORTH_POLE_LONGITUDE "grid_north_pole_longitude"
#define CF_PP_GRID_NORTH_POLE_LATITUDE "grid_north_pole_latitude"
#define CF_PP_NORTH_POLE_GRID_LONGITUDE "north_pole_grid_longitude"

#endif /* OGR_SRS_CF1_INCLUDED */
