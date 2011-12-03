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

#ifndef _NETCDFDATASET_H_INCLUDED_
#define _NETCDFATASET_H_INCLUDED_

#include <float.h>
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "gdal_frmts.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "netcdf.h"


/************************************************************************/
/* ==================================================================== */
/*			     defines    		                             		*/
/* ==================================================================== */
/************************************************************************/

/* -------------------------------------------------------------------- */
/*      Driver-specific defines                                         */
/* -------------------------------------------------------------------- */

/* NETCDF driver defs */
#define NCDF_MAX_STR_LEN     8192
#define NCDF_CONVENTIONS_CF  "CF-1.5"
#define NCDF_GDAL             GDALVersionInfo("--version")
#define NCDF_NBDIM           2
#define NCDF_SPATIAL_REF     "spatial_ref"
#define NCDF_GEOTRANSFORM    "GeoTransform"
#define NCDF_DIMNAME_X       "x"
#define NCDF_DIMNAME_Y       "y"
#define NCDF_DIMNAME_LON     "lon"
#define NCDF_DIMNAME_LAT     "lat"
#define NCDF_LONLAT          "lon lat"

/* netcdf file types, as in libcdi/cdo and compat w/netcdf.h */
#define NCDF_FORMAT_NONE            0   /* Not a netCDF file */
#define NCDF_FORMAT_NC              1   /* netCDF classic format */
#define NCDF_FORMAT_NC2             2   /* netCDF version 2 (64-bit)  */
#define NCDF_FORMAT_NC4             3   /* netCDF version 4 */
#define NCDF_FORMAT_NC4C            4   /* netCDF version 4 (classic) */
#define NCDF_FORMAT_UNKNOWN         10  /* Format not determined (yet) */
/* HDF files (HDF5 or HDF4) not supported because of lack of support */
/* in libnetcdf installation or conflict with other drivers */
#define NCDF_FORMAT_HDF5            5   /* HDF4 file, not supported */
#define NCDF_FORMAT_HDF4            6   /* HDF4 file, not supported */

/* compression parameters */
#define NCDF_COMPRESS_NONE            0   
/* TODO */
/* http://www.unidata.ucar.edu/software/netcdf/docs/BestPractices.html#Packed%20Data%20Values */
#define NCDF_COMPRESS_PACKED          1  
#define NCDF_COMPRESS_DEFLATE         2   
#define NCDF_DEFLATE_LEVEL            1  /* best time/size ratio */  
#define NCDF_COMPRESS_SZIP            3  /* no support for writting */ 

/* helper for libnetcdf errors */
#define NCDF_ERR(status) if ( status != NC_NOERR ){ \
CPLError( CE_Failure,CPLE_AppDefined, \
"netcdf error #%d : %s .\nat (%s,%s,%d)\n",status, nc_strerror(status), \
__FILE__, __FUNCTION__, __LINE__ ); }

/* check for NC2 support in case it wasn't enabled at compile time */
/* NC4 has to be detected at compile as it requires a special build of netcdf-4 */
#ifndef NETCDF_HAS_NC2
#ifdef NC_64BIT_OFFSET
#define NETCDF_HAS_NC2 1
#endif
#endif

/* -------------------------------------------------------------------- */
/*       CF or NUG (NetCDF User's Guide) defs                           */
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
#define CF_PROJ_X_COORD      "projection_x_coordinate"
#define CF_PROJ_Y_COORD      "projection_y_coordinate"
#define CF_GRD_MAPPING_NAME  "grid_mapping_name"
#define CF_GRD_MAPPING       "grid_mapping"
#define CF_COORDINATES       "coordinates"
/* #define CF_AXIS            "axis" */
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


/* -------------------------------------------------------------------- */
/*         CF-1 to GDAL mappings                                        */
/* -------------------------------------------------------------------- */

/* Following are a series of mappings from CF-1 convention parameters
 * for each projection, to the equivalent in OGC WKT used internally by GDAL.
 * See: http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/apf.html
 */

/* A struct allowing us to map between GDAL(OGC WKT) and CF-1 attributes */
typedef struct {
    const char *CF_ATT;
    const char *WKT_ATT; 
    // TODO: mappings may need default values, like scale factor?
    //double defval;
} oNetcdfSRS_PP;

// default mappings, for the generic case
/* These 'generic' mappings are based on what was previously in the  
   poNetCDFSRS struct. They will be used as a fallback in case none 
   of the others match (ie you are exporting a projection that has 
   no CF-1 equivalent). 
   They are not used for known CF-1 projections since there is not a 
   unique 2-way projection-independent 
   mapping between OGC WKT params and CF-1 ones: it varies per-projection. 
*/ 

static const oNetcdfSRS_PP poGenericMappings[] = {
    /* scale_factor is handled as a special case, write 2 values */
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1 }, 
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2 }, 
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN }, 
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_LONGITUDE_OF_CENTER }, 
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_LONGITUDE_OF_ORIGIN },  
    //Multiple mappings to LAT_PROJ_ORIGIN 
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN },  
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER },  
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },   
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },        
    {NULL, NULL },
};

// Albers equal area 
//
// grid_mapping_name = albers_conical_equal_area
// WKT: Albers_Conic_Equal_Area
// ESPG:9822 
//
// Map parameters:
//
//    * standard_parallel - There may be 1 or 2 values.
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poAEAMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_LONGITUDE_OF_CENTER},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 };

// Azimuthal equidistant
//
// grid_mapping_name = azimuthal_equidistant
// WKT: Azimuthal_Equidistant
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poAEMappings[] = {
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_LONGITUDE_OF_CENTER},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 };

// Lambert azimuthal equal area
//
// grid_mapping_name = lambert_azimuthal_equal_area
// WKT: Lambert_Azimuthal_Equal_Area
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poLAEAMappings[] = {
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_LONGITUDE_OF_CENTER},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 };

// Lambert conformal
//
// grid_mapping_name = lambert_conformal_conic
// WKT: Lambert_Conformal_Conic_1SP / Lambert_Conformal_Conic_2SP
//
// Map parameters:
//
//    * standard_parallel - There may be 1 or 2 values.
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
// See http://www.remotesensing.org/geotiff/proj_list/lambert_conic_conformal_1sp.html 

// Lambert conformal conic - 1SP
/* NOTE: exporting SCALE_FACTOR_ORIGIN is not a CF-1 standard for LCC,
   but until CF-1 projection params clarified, feel this is safest behaviour so as to
   not lose projection information.
*/
/* ET to PDS: are you sure it should be SCALE_FACTOR_ORIGIN and not SCALE_FACTOR_MERIDIAN? */
static const oNetcdfSRS_PP poLCC1SPMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 };

// Lambert conformal conic - 2SP
static const oNetcdfSRS_PP poLCC2SPMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 };

// Lambert cylindrical equal area
//
// grid_mapping_name = lambert_cylindrical_equal_area
// WKT: Cylindrical_Equal_Area
// EPSG:9834 (Spherical) and EPSG:9835 
//
// Map parameters:
//
//    * longitude_of_central_meridian
//    * either standard_parallel or scale_factor_at_projection_origin
//    * false_easting
//    * false_northing
//
// NB: CF-1 specifies a 'scale_factor_at_projection' alternative  
//  to std_parallel ... but no reference to this in EPSG/remotesensing.org 
//  ignore for now. 
//
static const oNetcdfSRS_PP poLCEAMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 };

// Latitude-Longitude
//
// grid_mapping_name = latitude_longitude
//
// Map parameters:
//
//    * None
//
// NB: handled as a special case - !isProjected()


// Mercator
//
// grid_mapping_name = mercator
// WKT: Mercator_1SP / Mercator_2SP
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * either standard_parallel or scale_factor_at_projection_origin
//    * false_easting
//    * false_northing

// Mercator 1 Standard Parallel (EPSG:9804) 
static const oNetcdfSRS_PP poM1SPMappings[] = {
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    //LAT_PROJ_ORIGIN is always equator (0) in CF-1 
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 };

// Mercator 2 Standard Parallel
static const oNetcdfSRS_PP poM2SPMappings[] = {
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    //From best understanding of this projection, only  
 	// actually specify one SP - it is the same N/S of equator. 
    //{CF_PP_STD_PARALLEL_2, SRS_PP_LATITUDE_OF_ORIGIN}, 
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 };

// Orthographic
// grid_mapping_name = orthographic
// WKT: Orthographic
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poOrthoMappings[] = {
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
 }; 

// Polar stereographic
//
// grid_mapping_name = polar_stereographic
// WKT: Polar_Stereographic
//
// Map parameters:
//
//    * straight_vertical_longitude_from_pole
//    * latitude_of_projection_origin - Either +90. or -90.
//    * Either standard_parallel or scale_factor_at_projection_origin
//    * false_easting
//    * false_northing

/* 
   (http://www.remotesensing.org/geotiff/proj_list/polar_stereographic.html)

   Note: Projection parameters for this projection are quite different in CF-1 from
     OGC WKT/GeoTiff (for the latter, see above).
   From our best understanding, this projection requires more than a straight mapping:
     - As defined below, 'latitude_of_origin' (WKT) -> 'standard_parallel' (CF-1)
       and 'central_meridian' (WKT) -> 'straight_vertical_longitude_from_pole' (CF-1)
     - Then the 'latitude_of_projection_origin' in CF-1 must be set to either +90 or -90,
       depending on the sign of 'latitude_of_origin' in WKT.
   Current support for this approach is provided by one existing NetCDF user of this projection.
   TODO: On import from CF-1, not sure how to handle a version with
     'scale_factor_at_projection_origin' defined, but not 'standard_parallel'.
*/
static const oNetcdfSRS_PP poPSmappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},  
    {CF_PP_VERT_LONG_FROM_POLE, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
};

// Rotated Pole
//
// grid_mapping_name = rotated_latitude_longitude
// WKT: N/A
//
// Map parameters:
//
//    * grid_north_pole_latitude
//    * grid_north_pole_longitude
//    * north_pole_grid_longitude - This parameter is optional (default is 0.).

/* TODO: No GDAL equivalent of rotated pole? Doesn't seem to have an EPSG
   code or WKT ... so unless some advanced proj4 features can be used 
   seems to rule out.
   see GDAL bug #4285 for a possible fix or workaround
*/

// Stereographic
//
// grid_mapping_name = stereographic
// WKT: Stereographic (and/or Oblique_Stereographic??)
//
// Map parameters:
//
//    * longitude_of_projection_origin
//    * latitude_of_projection_origin
//    * scale_factor_at_projection_origin
//    * false_easting
//    * false_northing
//
// NB: see bug#4267 Stereographic vs. Oblique_Stereographic
//
static const oNetcdfSRS_PP poStMappings[] = {
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},  
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
  };

// Transverse Mercator
//
// grid_mapping_name = transverse_mercator
// WKT: Transverse_Mercator
//
// Map parameters:
//
//    * scale_factor_at_central_meridian
//    * longitude_of_central_meridian
//    * latitude_of_projection_origin
//    * false_easting
//    * false_northing
//
static const oNetcdfSRS_PP poTMMappings[] = {
    {CF_PP_SCALE_FACTOR_MERIDIAN, SRS_PP_SCALE_FACTOR},  
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },
    {NULL, NULL}
  };

// Vertical perspective
//
// grid_mapping_name = vertical_perspective
// WKT: ???
//
// Map parameters:
//
//    * latitude_of_projection_origin
//    * longitude_of_projection_origin
//    * perspective_point_height
//    * false_easting
//    * false_northing
//
// TODO: see how to map this to OGR


/* Mappings for various projections, including netcdf and GDAL projection names 
   and corresponding oNetcdfSRS_PP mapping struct. 
   A NULL mappings value means that the projection is not included in the CF
   standard and the generic mapping (poGenericMappings) will be used. */
typedef struct {
    const char *CF_SRS;
    const char *WKT_SRS; 
    const oNetcdfSRS_PP* mappings;
} oNetcdfSRS_PT;

static const oNetcdfSRS_PT poNetcdfSRS_PT[] = {
    {CF_PT_AEA, SRS_PT_ALBERS_CONIC_EQUAL_AREA, poAEAMappings },
    {CF_PT_AE, SRS_PT_AZIMUTHAL_EQUIDISTANT, poAEMappings },
    {"cassini_soldner", SRS_PT_CASSINI_SOLDNER, NULL },
    {CF_PT_LCEA, SRS_PT_CYLINDRICAL_EQUAL_AREA, poLCEAMappings },
    {"eckert_iv", SRS_PT_ECKERT_IV, NULL },      
    {"eckert_vi", SRS_PT_ECKERT_VI, NULL },  
    {"equidistant_conic", SRS_PT_EQUIDISTANT_CONIC, NULL },
    {"equirectangular", SRS_PT_EQUIRECTANGULAR, NULL },
    {"gall_stereographic", SRS_PT_GALL_STEREOGRAPHIC, NULL },
    {"geostationary_satellite", SRS_PT_GEOSTATIONARY_SATELLITE, NULL },
    {"goode_homolosine", SRS_PT_GOODE_HOMOLOSINE, NULL },
    {"gnomonic", SRS_PT_GNOMONIC, NULL },
    {"hotine_oblique_mercator", SRS_PT_HOTINE_OBLIQUE_MERCATOR, NULL },
    {"hotine_oblique_mercator_2P", 
     SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN, NULL },
    {"laborde_oblique_mercator", SRS_PT_LABORDE_OBLIQUE_MERCATOR, NULL },
    {CF_PT_LCC, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP, poLCC1SPMappings },
    {CF_PT_LCC, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP, poLCC2SPMappings },
    {CF_PT_LAEA, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA, poLAEAMappings },
    {CF_PT_MERCATOR, SRS_PT_MERCATOR_1SP, poM1SPMappings },
    {CF_PT_MERCATOR, SRS_PT_MERCATOR_2SP, poM2SPMappings },
    {"miller_cylindrical", SRS_PT_MILLER_CYLINDRICAL, NULL },
    {"mollweide", SRS_PT_MOLLWEIDE, NULL },
    {"new_zealand_map_grid", SRS_PT_NEW_ZEALAND_MAP_GRID, NULL },
    /* for now map to STEREO, see bug #4267 */
    {"oblique_stereographic", SRS_PT_OBLIQUE_STEREOGRAPHIC, NULL }, 
    /* {STEREO, SRS_PT_OBLIQUE_STEREOGRAPHIC, poStMappings },  */
    {CF_PT_ORTHOGRAPHIC, SRS_PT_ORTHOGRAPHIC, poOrthoMappings },
    {CF_PT_POLAR_STEREO, SRS_PT_POLAR_STEREOGRAPHIC, poPSmappings },
    {"polyconic", SRS_PT_POLYCONIC, NULL },
    {"robinson", SRS_PT_ROBINSON, NULL }, 
    {"sinusoidal", SRS_PT_SINUSOIDAL, NULL },  
    {CF_PT_STEREO, SRS_PT_STEREOGRAPHIC, poStMappings },
    {"swiss_oblique_cylindrical", SRS_PT_SWISS_OBLIQUE_CYLINDRICAL, NULL },
    {CF_PT_TM, SRS_PT_TRANSVERSE_MERCATOR, poTMMappings },
    {"TM_south_oriented", SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED, NULL },
    {NULL, NULL, NULL },
};

/************************************************************************/
/* ==================================================================== */
/*			     netCDFDataset		                             		*/
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand;

class netCDFDataset : public GDALPamDataset
{
    friend class netCDFRasterBand; //TMP

    /* basic dataset vars */
    CPLString     osFilename;
    int           cdfid;
    char          **papszSubDatasets;
    char          **papszMetadata;
    CPLStringList papszDimName;
    bool          bBottomUp;
    int           nFormat;
    int           bIsGdalFile; /* was this file created by GDAL? */
    int           bIsGdalCfFile; /* was this file created by the (new) CF-compliant driver? */

    /* projection/GT */
    double       adfGeoTransform[6];
    char         *pszProjection;
    int          nXDimID;
    int          nYDimID;
    int          bIsProjected;
    int          bIsGeographic;

    /* state vars */
    int          status;
    int          bDefineMode;
    int          bSetProjection; 
    int          bSetGeoTransform;
    int          bAddedProjectionVars;

    /* create vars */
    char         **papszCreationOptions;
    int          nCompress;
    int          nZLevel;
    int          nCreateMode;
    int          bSignedData;

    double       rint( double );

    double       FetchCopyParm( const char *pszGridMappingValue, 
                                const char *pszParm, double dfDefault );

    char **      FetchStandardParallels( const char *pszGridMappingValue );

    
    /* new */
    void ProcessCreationOptions( );
    int DefVarDeflate( int nVarId, int bChunking=TRUE );
    CPLErr AddProjectionVars( GDALProgressFunc pfnProgress=GDALDummyProgress, 
                              void * pProgressData=NULL );

    int GetDefineMode() { return bDefineMode; }
    int SetDefineMode( int bNewDefineMode );

    CPLErr      ReadAttributes( int, int );

    void  CreateSubDatasetList( );

    void  SetProjectionFromVar( int );

  public:

    netCDFDataset( );
    ~netCDFDataset( );
    
    /* Projection/GT */
    CPLErr 	GetGeoTransform( double * );    
    CPLErr 	SetGeoTransform (double *);
    const char * GetProjectionRef();
    CPLErr 	SetProjection (const char *);

    char ** GetMetadata( const char * );

    int GetCDFID() { return cdfid; }

    /* static functions */
    static int Identify( GDALOpenInfo * );
    static int IdentifyFormat( GDALOpenInfo *, bool );
    static GDALDataset *Open( GDALOpenInfo * );

    static netCDFDataset *CreateLL( const char * pszFilename,
                                    int nXSize, int nYSize, int nBands,
                                    char ** papszOptions );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType,
                                char ** papszOptions );
    static GDALDataset* CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                                    int bStrict, char ** papszOptions, 
                                    GDALProgressFunc pfnProgress, void * pProgressData );
        
};

#endif
