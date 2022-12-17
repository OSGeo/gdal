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

#ifndef NETCDFDATASET_H_INCLUDED_
#define NETCDFDATASET_H_INCLUDED_

#include <array>
#include <ctime>
#include <cfloat>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "cpl_mem_cache.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"
#include "netcdf.h"
#include "netcdfsg.h"
#include "netcdfsgwriterutil.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"
#include "netcdfuffd.h"
#include "netcdf_cf_constants.h"

#if defined(DEBUG) || defined(FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION) ||     \
    defined(ALLOW_FORMAT_DUMPS)
// Whether to support opening a ncdump file as a file dataset
// Useful for fuzzing purposes
#define ENABLE_NCDUMP
#endif

#if CPL_IS_LSB
#define PLATFORM_HEADER 1
#else
#define PLATFORM_HEADER 0
#endif

/************************************************************************/
/* ==================================================================== */
/*                           defines                                    */
/* ==================================================================== */
/************************************************************************/

/* -------------------------------------------------------------------- */
/*      Creation and Configuration Options                              */
/* -------------------------------------------------------------------- */

/* Creation options

   FORMAT=NC/NC2/NC4/NC4C (COMPRESS=DEFLATE sets FORMAT=NC4C)
   COMPRESS=NONE/DEFLATE (default: NONE)
   ZLEVEL=[1-9] (default: 1)
   WRITE_BOTTOMUP=YES/NO (default: YES)
   WRITE_GDAL_TAGS=YES/NO (default: YES)
   WRITE_LONLAT=YES/NO/IF_NEEDED (default: YES for geographic, NO for projected)
   TYPE_LONLAT=float/double (default: double for geographic, float for
   projected) PIXELTYPE=DEFAULT/SIGNEDBYTE (use SIGNEDBYTE to get a signed Byte
   Band)
*/

/* Config Options

   GDAL_NETCDF_BOTTOMUP=YES/NO overrides bottom-up value on import
   GDAL_NETCDF_CONVERT_LAT_180=YES/NO convert longitude values from ]180,360] to
   [-180,180]
*/

/* -------------------------------------------------------------------- */
/*      Driver-specific defines                                         */
/* -------------------------------------------------------------------- */

/* NETCDF driver defs */
static const size_t NCDF_MAX_STR_LEN = 8192;
#define NCDF_CONVENTIONS "Conventions"
#define NCDF_CONVENTIONS_CF_V1_5 "CF-1.5"
#define GDAL_DEFAULT_NCDF_CONVENTIONS NCDF_CONVENTIONS_CF_V1_5
#define NCDF_CONVENTIONS_CF_V1_6 "CF-1.6"
#define NCDF_CONVENTIONS_CF_V1_8 "CF-1.8"
#define NCDF_CRS_WKT "crs_wkt"
#define NCDF_SPATIAL_REF "spatial_ref"
#define NCDF_GEOTRANSFORM "GeoTransform"
#define NCDF_DIMNAME_X "x"
#define NCDF_DIMNAME_Y "y"
#define NCDF_DIMNAME_LON "lon"
#define NCDF_DIMNAME_LAT "lat"
#define NCDF_LONLAT "lon lat"
#define NCDF_DIMNAME_RLON "rlon"  // rotated longitude
#define NCDF_DIMNAME_RLAT "rlat"  // rotated latitude

/* netcdf file types, as in libcdi/cdo and compat w/netcdf.h */
typedef enum
{
    NCDF_FORMAT_NONE = 0, /* Not a netCDF file */
    NCDF_FORMAT_NC = 1,   /* netCDF classic format */
    NCDF_FORMAT_NC2 = 2,  /* netCDF version 2 (64-bit)  */
    NCDF_FORMAT_NC4 = 3,  /* netCDF version 4 */
    NCDF_FORMAT_NC4C = 4, /* netCDF version 4 (classic) */
    /* HDF files (HDF5 or HDF4) not supported because of lack of support */
    /* in libnetcdf installation or conflict with other drivers */
    NCDF_FORMAT_HDF5 = 5,    /* HDF5 file, not supported */
    NCDF_FORMAT_HDF4 = 6,    /* HDF4 file, not supported */
    NCDF_FORMAT_UNKNOWN = 10 /* Format not determined (yet) */
} NetCDFFormatEnum;

/* compression parameters */
typedef enum
{
    NCDF_COMPRESS_NONE = 0,
    /* TODO */
    /* http://www.unidata.ucar.edu/software/netcdf/docs/BestPractices.html#Packed%20Data%20Values
     */
    NCDF_COMPRESS_PACKED = 1,
    NCDF_COMPRESS_DEFLATE = 2,
    NCDF_COMPRESS_SZIP = 3 /* no support for writing */
} NetCDFCompressEnum;

static const int NCDF_DEFLATE_LEVEL = 1; /* best time/size ratio */

/* helper for libnetcdf errors */
#define NCDF_ERR(status)                                                       \
    do                                                                         \
    {                                                                          \
        int NCDF_ERR_status_ = (status);                                       \
        if (NCDF_ERR_status_ != NC_NOERR)                                      \
        {                                                                      \
            CPLError(CE_Failure, CPLE_AppDefined,                              \
                     "netcdf error #%d : %s .\nat (%s,%s,%d)\n", status,       \
                     nc_strerror(NCDF_ERR_status_), __FILE__, __FUNCTION__,    \
                     __LINE__);                                                \
        }                                                                      \
    } while (0)

#define NCDF_ERR_RET(status)                                                   \
    do                                                                         \
    {                                                                          \
        int NCDF_ERR_RET_status_ = (status);                                   \
        if (NCDF_ERR_RET_status_ != NC_NOERR)                                  \
        {                                                                      \
            NCDF_ERR(NCDF_ERR_RET_status_);                                    \
            return CE_Failure;                                                 \
        }                                                                      \
    } while (0)

#define ERR_RET(eErr)                                                          \
    do                                                                         \
    {                                                                          \
        CPLErr ERR_RET_eErr_ = (eErr);                                         \
        if (ERR_RET_eErr_ != CE_None)                                          \
            return ERR_RET_eErr_;                                              \
    } while (0)

/* Check for NC2 support in case it was not enabled at compile time. */
/* NC4 has to be detected at compile as it requires a special build of netcdf-4.
 */
#ifndef NETCDF_HAS_NC2
#ifdef NC_64BIT_OFFSET
#define NETCDF_HAS_NC2 1
#endif
#endif

/* Some additional metadata */
#define OGR_SG_ORIGINAL_LAYERNAME "ogr_layer_name"

/* -------------------------------------------------------------------- */
/*         CF-1 Coordinate Type Naming (Chapter 4.  Coordinate Types )  */
/* -------------------------------------------------------------------- */
static const char *const papszCFLongitudeVarNames[] = {CF_LONGITUDE_VAR_NAME,
                                                       "longitude", nullptr};
static const char *const papszCFLongitudeAttribNames[] = {
    CF_UNITS, CF_UNITS, CF_UNITS, CF_STD_NAME, CF_AXIS, CF_LNG_NAME, nullptr};
static const char *const papszCFLongitudeAttribValues[] = {
    CF_DEGREES_EAST,
    CF_DEGREE_EAST,
    CF_DEGREES_E,
    CF_LONGITUDE_STD_NAME,
    "X",
    CF_LONGITUDE_LNG_NAME,
    nullptr};
static const char *const papszCFLatitudeVarNames[] = {CF_LATITUDE_VAR_NAME,
                                                      "latitude", nullptr};
static const char *const papszCFLatitudeAttribNames[] = {
    CF_UNITS, CF_UNITS, CF_UNITS, CF_STD_NAME, CF_AXIS, CF_LNG_NAME, nullptr};
static const char *const papszCFLatitudeAttribValues[] = {CF_DEGREES_NORTH,
                                                          CF_DEGREE_NORTH,
                                                          CF_DEGREES_N,
                                                          CF_LATITUDE_STD_NAME,
                                                          "Y",
                                                          CF_LATITUDE_LNG_NAME,
                                                          nullptr};

static const char *const papszCFProjectionXVarNames[] = {CF_PROJ_X_VAR_NAME,
                                                         "xc", nullptr};
static const char *const papszCFProjectionXAttribNames[] = {CF_STD_NAME,
                                                            CF_AXIS, nullptr};
static const char *const papszCFProjectionXAttribValues[] = {CF_PROJ_X_COORD,
                                                             "X", nullptr};
static const char *const papszCFProjectionYVarNames[] = {CF_PROJ_Y_VAR_NAME,
                                                         "yc", nullptr};
static const char *const papszCFProjectionYAttribNames[] = {CF_STD_NAME,
                                                            CF_AXIS, nullptr};
static const char *const papszCFProjectionYAttribValues[] = {CF_PROJ_Y_COORD,
                                                             "Y", nullptr};

static const char *const papszCFVerticalAttribNames[] = {CF_AXIS, "positive",
                                                         "positive", nullptr};
static const char *const papszCFVerticalAttribValues[] = {"Z", "up", "down",
                                                          nullptr};
static const char *const papszCFVerticalUnitsValues[] = {
    /* units of pressure */
    "bar", "bars", "millibar", "millibars", "decibar", "decibars", "atmosphere",
    "atmospheres", "atm", "pascal", "pascals", "Pa", "hPa",
    /* units of length */
    "meter", "meters", "m", "kilometer", "kilometers", "km",
    /* dimensionless vertical coordinates */
    "level", "layer", "sigma_level", nullptr};
/* dimensionless vertical coordinates */
static const char *const papszCFVerticalStandardNameValues[] = {
    "atmosphere_ln_pressure_coordinate",
    "atmosphere_sigma_coordinate",
    "atmosphere_hybrid_sigma_pressure_coordinate",
    "atmosphere_hybrid_height_coordinate",
    "atmosphere_sleve_coordinate",
    "ocean_sigma_coordinate",
    "ocean_s_coordinate",
    "ocean_sigma_z_coordinate",
    "ocean_double_sigma_coordinate",
    "atmosphere_ln_pressure_coordinate",
    "atmosphere_sigma_coordinate",
    "atmosphere_hybrid_sigma_pressure_coordinate",
    "atmosphere_hybrid_height_coordinate",
    "atmosphere_sleve_coordinate",
    "ocean_sigma_coordinate",
    "ocean_s_coordinate",
    "ocean_sigma_z_coordinate",
    "ocean_double_sigma_coordinate",
    nullptr};

static const char *const papszCFTimeAttribNames[] = {CF_AXIS, CF_STD_NAME,
                                                     nullptr};
static const char *const papszCFTimeAttribValues[] = {"T", "time", nullptr};
static const char *const papszCFTimeUnitsValues[] = {
    "days since",   "day since", "d since",       "hours since",
    "hour since",   "h since",   "hr since",      "minutes since",
    "minute since", "min since", "seconds since", "second since",
    "sec since",    "s since",   nullptr};

/* -------------------------------------------------------------------- */
/*         CF-1 to GDAL mappings                                        */
/* -------------------------------------------------------------------- */

/* Following are a series of mappings from CF-1 convention parameters
 * for each projection, to the equivalent in OGC WKT used internally by GDAL.
 * See: http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/apf.html
 */

/* A struct allowing us to map between GDAL(OGC WKT) and CF-1 attributes */
typedef struct
{
    const char *CF_ATT;
    const char *WKT_ATT;
    // TODO: mappings may need default values, like scale factor?
    // double defval;
} oNetcdfSRS_PP;

// default mappings, for the generic case
/* These 'generic' mappings are based on what was previously in the
   poNetCDFSRS struct. They will be used as a fallback in case none
   of the others match (i.e. you are exporting a projection that has
   no CF-1 equivalent).
   They are not used for known CF-1 projections since there is not a
   unique 2-way projection-independent
   mapping between OGC WKT params and CF-1 ones: it varies per-projection.
*/

static const oNetcdfSRS_PP poGenericMappings[] = {
    /* scale_factor is handled as a special case, write 2 values */
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_LONGITUDE_OF_CENTER},
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_LONGITUDE_OF_ORIGIN},
    // Multiple mappings to LAT_PROJ_ORIGIN
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_CENTER},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr},
};

// Albers equal area
//
// grid_mapping_name = albers_conical_equal_area
// WKT: Albers_Conic_Equal_Area
// EPSG:9822
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
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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
// See
// http://www.remotesensing.org/geotiff/proj_list/lambert_conic_conformal_1sp.html

// Lambert conformal conic - 1SP
/* See bug # 3324
   It seems that the missing scale factor can be computed from
   standard_parallel1 and latitude_of_projection_origin. If both are equal (the
   common case) then scale factor=1, else use Snyder eq. 15-4. We save in the
   WKT standard_parallel1 for export to CF, but do not export scale factor. If a
   WKT has a scale factor != 1 and no standard_parallel1 then export is not CF,
   but we output scale factor for compat. is there a formula for that?
*/
static const oNetcdfSRS_PP poLCC1SPMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR}, /* special case */
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Lambert conformal conic - 2SP
static const oNetcdfSRS_PP poLCC2SPMappings[] = {
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2},
    {CF_PP_LAT_PROJ_ORIGIN, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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
    // LAT_PROJ_ORIGIN is always equator (0) in CF-1
    {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

// Mercator 2 Standard Parallel
static const oNetcdfSRS_PP poM2SPMappings[] = {
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1},
    // From best understanding of this projection, only
    // actually specify one SP - it is the same N/S of equator.
    // {CF_PP_STD_PARALLEL_2, SRS_PP_LATITUDE_OF_ORIGIN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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

static const oNetcdfSRS_PP poPSmappings[] = {
    /* {CF_PP_STD_PARALLEL_1, SRS_PP_LATITUDE_OF_ORIGIN}, */
    /* {CF_PP_SCALE_FACTOR_ORIGIN, SRS_PP_SCALE_FACTOR},   */
    {CF_PP_VERT_LONG_FROM_POLE, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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

// No WKT equivalent

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
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    {nullptr, nullptr}};

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

static const oNetcdfSRS_PP poGEOSMappings[] = {
    {CF_PP_LON_PROJ_ORIGIN, SRS_PP_CENTRAL_MERIDIAN},
    {CF_PP_PERSPECTIVE_POINT_HEIGHT, SRS_PP_SATELLITE_HEIGHT},
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING},
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING},
    /* { CF_PP_SWEEP_ANGLE_AXIS, .... } handled as a proj.4 extension */
    {nullptr, nullptr}};

/* Mappings for various projections, including netcdf and GDAL projection names
   and corresponding oNetcdfSRS_PP mapping struct.
   A NULL mappings value means that the projection is not included in the CF
   standard and the generic mapping (poGenericMappings) will be used. */
typedef struct
{
    const char *CF_SRS;
    const char *WKT_SRS;
    const oNetcdfSRS_PP *mappings;
} oNetcdfSRS_PT;

static const oNetcdfSRS_PT poNetcdfSRS_PT[] = {
    {CF_PT_AEA, SRS_PT_ALBERS_CONIC_EQUAL_AREA, poAEAMappings},
    {CF_PT_AE, SRS_PT_AZIMUTHAL_EQUIDISTANT, poAEMappings},
    {"cassini_soldner", SRS_PT_CASSINI_SOLDNER, nullptr},
    {CF_PT_LCEA, SRS_PT_CYLINDRICAL_EQUAL_AREA, poLCEAMappings},
    {"eckert_iv", SRS_PT_ECKERT_IV, nullptr},
    {"eckert_vi", SRS_PT_ECKERT_VI, nullptr},
    {"equidistant_conic", SRS_PT_EQUIDISTANT_CONIC, nullptr},
    {"equirectangular", SRS_PT_EQUIRECTANGULAR, nullptr},
    {"gall_stereographic", SRS_PT_GALL_STEREOGRAPHIC, nullptr},
    {CF_PT_GEOS, SRS_PT_GEOSTATIONARY_SATELLITE, poGEOSMappings},
    {"goode_homolosine", SRS_PT_GOODE_HOMOLOSINE, nullptr},
    {"gnomonic", SRS_PT_GNOMONIC, nullptr},
    {"hotine_oblique_mercator", SRS_PT_HOTINE_OBLIQUE_MERCATOR, nullptr},
    {"hotine_oblique_mercator_2P",
     SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN, nullptr},
    {"laborde_oblique_mercator", SRS_PT_LABORDE_OBLIQUE_MERCATOR, nullptr},
    {CF_PT_LCC, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP, poLCC1SPMappings},
    {CF_PT_LCC, SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP, poLCC2SPMappings},
    {CF_PT_LAEA, SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA, poLAEAMappings},
    {CF_PT_MERCATOR, SRS_PT_MERCATOR_1SP, poM1SPMappings},
    {CF_PT_MERCATOR, SRS_PT_MERCATOR_2SP, poM2SPMappings},
    {"miller_cylindrical", SRS_PT_MILLER_CYLINDRICAL, nullptr},
    {"mollweide", SRS_PT_MOLLWEIDE, nullptr},
    {"new_zealand_map_grid", SRS_PT_NEW_ZEALAND_MAP_GRID, nullptr},
    /* for now map to STEREO, see bug #4267 */
    {"oblique_stereographic", SRS_PT_OBLIQUE_STEREOGRAPHIC, nullptr},
    /* {STEREO, SRS_PT_OBLIQUE_STEREOGRAPHIC, poStMappings },  */
    {CF_PT_ORTHOGRAPHIC, SRS_PT_ORTHOGRAPHIC, poOrthoMappings},
    {CF_PT_POLAR_STEREO, SRS_PT_POLAR_STEREOGRAPHIC, poPSmappings},
    {"polyconic", SRS_PT_POLYCONIC, nullptr},
    {"robinson", SRS_PT_ROBINSON, nullptr},
    {"sinusoidal", SRS_PT_SINUSOIDAL, nullptr},
    {CF_PT_STEREO, SRS_PT_STEREOGRAPHIC, poStMappings},
    {"swiss_oblique_cylindrical", SRS_PT_SWISS_OBLIQUE_CYLINDRICAL, nullptr},
    {CF_PT_TM, SRS_PT_TRANSVERSE_MERCATOR, poTMMappings},
    {"TM_south_oriented", SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED, nullptr},
    {nullptr, nullptr, nullptr},
};

/************************************************************************/
/* ==================================================================== */
/*                        netCDFWriterConfig classes                    */
/* ==================================================================== */
/************************************************************************/

class netCDFWriterConfigAttribute
{
  public:
    CPLString m_osName;
    CPLString m_osType;
    CPLString m_osValue;

    bool Parse(CPLXMLNode *psNode);
};

class netCDFWriterConfigField
{
  public:
    CPLString m_osName;
    CPLString m_osNetCDFName;
    CPLString m_osMainDim;
    std::vector<netCDFWriterConfigAttribute> m_aoAttributes;

    bool Parse(CPLXMLNode *psNode);
};

class netCDFWriterConfigLayer
{
  public:
    CPLString m_osName;
    CPLString m_osNetCDFName;
    std::map<CPLString, CPLString> m_oLayerCreationOptions;
    std::vector<netCDFWriterConfigAttribute> m_aoAttributes;
    std::map<CPLString, netCDFWriterConfigField> m_oFields;

    bool Parse(CPLXMLNode *psNode);
};

class netCDFWriterConfiguration
{
  public:
    bool m_bIsValid;
    std::map<CPLString, CPLString> m_oDatasetCreationOptions;
    std::map<CPLString, CPLString> m_oLayerCreationOptions;
    std::vector<netCDFWriterConfigAttribute> m_aoAttributes;
    std::map<CPLString, netCDFWriterConfigField> m_oFields;
    std::map<CPLString, netCDFWriterConfigLayer> m_oLayers;

    netCDFWriterConfiguration() : m_bIsValid(false)
    {
    }

    bool Parse(const char *pszFilename);
    static bool SetNameValue(CPLXMLNode *psNode,
                             std::map<CPLString, CPLString> &oMap);
};

/************************************************************************/
/* ==================================================================== */
/*                           netCDFDataset                              */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand;
class netCDFLayer;

class netCDFDataset final : public GDALPamDataset
{
    friend class netCDFRasterBand;  // TMP
    friend class netCDFLayer;
    friend class netCDFVariable;

    typedef enum
    {
        SINGLE_LAYER,
        SEPARATE_FILES,
        SEPARATE_GROUPS
    } MultipleLayerBehavior;

    /* basic dataset vars */
    CPLString osFilename;
#ifdef ENABLE_NCDUMP
    bool bFileToDestroyAtClosing;
#endif
    int cdfid;
#ifdef ENABLE_UFFD
    cpl_uffd_context *pCtx = nullptr;
#endif
    VSILFILE *fpVSIMEM = nullptr;
    int nSubDatasets;
    char **papszSubDatasets;
    char **papszMetadata;

    // Used to report metadata found in Sentinel 5
    std::map<std::string, CPLStringList> m_oMapDomainToJSon{};

    CPLStringList papszDimName;
    bool bBottomUp;
    NetCDFFormatEnum eFormat;
    bool bIsGdalFile;   /* was this file created by GDAL? */
    bool bIsGdalCfFile; /* was this file created by the (new) CF-compliant
                           driver? */
    char *pszCFProjection;
    const char *pszCFCoordinates;
    double nCFVersion;
    bool bSGSupport;
    MultipleLayerBehavior eMultipleLayerBehavior;
    std::vector<netCDFDataset *> apoVectorDatasets;
    std::string logHeader;
    int logCount;
    nccfdriver::netCDFVID vcdf;
    nccfdriver::OGR_NCScribe GeometryScribe;
    nccfdriver::OGR_NCScribe FieldScribe;
    nccfdriver::WBufferManager bufManager;

    bool bWriteGDALVersion = true;
    bool bWriteGDALHistory = true;

    /* projection/GT */
    double m_adfGeoTransform[6];
    OGRSpatialReference m_oSRS{};
    int nXDimID;
    int nYDimID;
    bool bIsProjected;
    bool bIsGeographic;
    bool bSwitchedXY = false;

    /* state vars */
    bool bDefineMode;
    bool m_bHasProjection = false;
    bool m_bHasGeoTransform = false;
    bool m_bAddedProjectionVarsDefs = false;
    bool m_bAddedProjectionVarsData = false;
    bool bAddedGridMappingRef;

    /* create vars */
    char **papszCreationOptions;
    NetCDFCompressEnum eCompress;
    int nZLevel;
#ifdef NETCDF_HAS_NC4
    bool bChunking;
#endif
    int nCreateMode;
    bool bSignedData;

    std::vector<std::shared_ptr<OGRLayer>> papoLayers;

    netCDFWriterConfiguration oWriterConfig;

    struct ChunkKey
    {
        size_t xChunk;  // netCDF chunk number along X axis
        size_t yChunk;  // netCDF chunk number along Y axis
        int nBand;

        ChunkKey(size_t xChunkIn, size_t yChunkIn, int nBandIn)
            : xChunk(xChunkIn), yChunk(yChunkIn), nBand(nBandIn)
        {
        }

        bool operator==(const ChunkKey &other) const
        {
            return xChunk == other.xChunk && yChunk == other.yChunk &&
                   nBand == other.nBand;
        }

        bool operator!=(const ChunkKey &other) const
        {
            return !(operator==(other));
        }
    };

    struct KeyHasher
    {
        std::size_t operator()(const ChunkKey &k) const
        {
            return std::hash<size_t>{}(k.xChunk) ^
                   (std::hash<size_t>{}(k.yChunk) << 1) ^
                   (std::hash<size_t>{}(k.nBand) << 2);
        }
    };

    typedef lru11::Cache<
        ChunkKey, std::shared_ptr<std::vector<GByte>>, lru11::NullLock,
        std::unordered_map<
            ChunkKey,
            typename std::list<lru11::KeyValuePair<
                ChunkKey, std::shared_ptr<std::vector<GByte>>>>::iterator,
            KeyHasher>>
        ChunkCacheType;

    std::unique_ptr<ChunkCacheType> poChunkCache;

    static double rint(double);

    double FetchCopyParam(const char *pszGridMappingValue, const char *pszParam,
                          double dfDefault, bool *pbFound = nullptr);

    std::vector<std::string>
    FetchStandardParallels(const char *pszGridMappingValue);

    const char *FetchAttr(const char *pszVarFullName, const char *pszAttr);
    const char *FetchAttr(int nGroupId, int nVarId, const char *pszAttr);

    void ProcessCreationOptions();
    int DefVarDeflate(int nVarId, bool bChunkingArg = true);
    CPLErr AddProjectionVars(bool bDefsOnly, GDALProgressFunc pfnProgress,
                             void *pProgressData);
    void AddGridMappingRef();

    bool GetDefineMode() const
    {
        return bDefineMode;
    }
    bool SetDefineMode(bool bNewDefineMode);

    CPLErr ReadAttributes(int, int);

    void CreateSubDatasetList(int nGroupId);

    void SetProjectionFromVar(int nGroupId, int nVarId, bool bReadSRSOnly,
                              const char *pszGivenGM, std::string *,
                              nccfdriver::SGeometry_Reader *);
    void SetProjectionFromVar(int nGroupId, int nVarId, bool bReadSRSOnly);

    int ProcessCFGeolocation(int nGroupId, int nVarId,
                             std::string &osGeolocXNameOut,
                             std::string &osGeolocYNameOut);
    CPLErr Set1DGeolocation(int nGroupId, int nVarId, const char *szDimName);
    double *Get1DGeolocation(const char *szDimName, int &nVarLen);

    static bool CloneAttributes(int old_cdfid, int new_cdfid, int nSrcVarId,
                                int nDstVarId);
    static bool CloneVariableContent(int old_cdfid, int new_cdfid,
                                     int nSrcVarId, int nDstVarId);
    static bool CloneGrp(int nOldGrpId, int nNewGrpId, bool bIsNC4,
                         int nLayerId, int nDimIdToGrow, size_t nNewSize);
    bool GrowDim(int nLayerId, int nDimIdToGrow, size_t nNewSize);

#ifdef NETCDF_HAS_NC4
    void ProcessSentinel3_SRAL_MWR();
#endif

    CPLErr
    FilterVars(int nCdfId, bool bKeepRasters, bool bKeepVectors,
               char **papszIgnoreVars, int *pnRasterVars, int *pnGroupId,
               int *pnVarId, int *pnIgnoredVars,
               // key is (dim1Id, dim2Id, nc_type varType)
               // value is (groupId, varId)
               std::map<std::array<int, 3>, std::vector<std::pair<int, int>>>
                   &oMap2DDimsToGroupAndVar);
    CPLErr CreateGrpVectorLayers(int nCdfId, CPLString osFeatureType,
                                 std::vector<int> anPotentialVectorVarID,
                                 std::map<int, int> oMapDimIdToCount,
                                 int nVarXId, int nVarYId, int nVarZId,
                                 int nProfileDimId, int nParentIndexVarID,
                                 bool bKeepRasters);

    CPLErr DetectAndFillSGLayers(int ncid);
    CPLErr LoadSGVarIntoLayer(int ncid, int nc_basevarId);

#ifdef NETCDF_HAS_NC4
    static GDALDataset *OpenMultiDim(GDALOpenInfo *);
    std::shared_ptr<GDALGroup> m_poRootGroup{};
#endif

    void SetGeoTransformNoUpdate(double *);
    void SetSpatialRefNoUpdate(const OGRSpatialReference *);

  protected:
    CPLXMLNode *SerializeToXML(const char *pszVRTPath) override;

    virtual OGRLayer *ICreateLayer(const char *pszName,
                                   OGRSpatialReference *poSpatialRef,
                                   OGRwkbGeometryType eGType,
                                   char **papszOptions) override;

  public:
    netCDFDataset();
    virtual ~netCDFDataset();
    void SGCommitPendingTransaction();
    void SGLogPendingTransaction();
    static std::string generateLogName();

    /* Projection/GT */
    CPLErr GetGeoTransform(double *) override;
    CPLErr SetGeoTransform(double *) override;
    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual char **GetMetadataDomainList() override;
    char **GetMetadata(const char *) override;

    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;
    virtual CPLErr SetMetadata(char **papszMD,
                               const char *pszDomain = "") override;

    virtual int TestCapability(const char *pszCap) override;

    virtual int GetLayerCount() override
    {
        return static_cast<int>(this->papoLayers.size());
    }
    virtual OGRLayer *GetLayer(int nIdx) override;

#ifdef NETCDF_HAS_NC4
    std::shared_ptr<GDALGroup> GetRootGroup() const override;
#endif

    int GetCDFID() const
    {
        return cdfid;
    }

    inline bool HasInfiniteRecordDim()
    {
        return !bSGSupport;
    }

    /* static functions */
    static int Identify(GDALOpenInfo *);
    static NetCDFFormatEnum IdentifyFormat(GDALOpenInfo *, bool);
    static GDALDataset *Open(GDALOpenInfo *);

    static netCDFDataset *CreateLL(const char *pszFilename, int nXSize,
                                   int nYSize, int nBands, char **papszOptions);
    static GDALDataset *Create(const char *pszFilename, int nXSize, int nYSize,
                               int nBands, GDALDataType eType,
                               char **papszOptions);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

#ifdef NETCDF_HAS_NC4
    static GDALDataset *
    CreateMultiDimensional(const char *pszFilename,
                           CSLConstList papszRootGroupOptions,
                           CSLConstList papzOptions);
#endif
};

class netCDFLayer final : public OGRLayer
{
    typedef union
    {
        signed char chVal;
        unsigned char uchVal;
        short sVal;
        unsigned short usVal;
        int nVal;
        unsigned int unVal;
        GIntBig nVal64;
        GUIntBig unVal64;
        float fVal;
        double dfVal;
    } NCDFNoDataUnion;

    typedef struct
    {
        NCDFNoDataUnion uNoData;
        nc_type nType;
        int nVarId;
        int nDimCount;
        bool bHasWarnedAboutTruncation;
        int nMainDimId;
        int nSecDimId;
        bool bIsDays;
    } FieldDesc;

    netCDFDataset *m_poDS;
    int m_nLayerCDFId;
    OGRFeatureDefn *m_poFeatureDefn;
    CPLString m_osRecordDimName;
    int m_nRecordDimID;
    int m_nDefaultWidth;
    bool m_bAutoGrowStrings;
    int m_nDefaultMaxWidthDimId;
    int m_nXVarID;
    int m_nYVarID;
    int m_nZVarID;
    nc_type m_nXVarNCDFType;
    nc_type m_nYVarNCDFType;
    nc_type m_nZVarNCDFType;
    NCDFNoDataUnion m_uXVarNoData;
    NCDFNoDataUnion m_uYVarNoData;
    NCDFNoDataUnion m_uZVarNoData;
    CPLString m_osWKTVarName;
    int m_nWKTMaxWidth;
    int m_nWKTMaxWidthDimId;
    int m_nWKTVarID;
    nc_type m_nWKTNCDFType;
    CPLString m_osCoordinatesValue;
    std::vector<FieldDesc> m_aoFieldDesc;
    bool m_bLegacyCreateMode;
    int m_nCurFeatureId;
    CPLString m_osGridMapping;
    bool m_bWriteGDALTags;
    bool m_bUseStringInNC4;
    bool m_bNCDumpCompat;

    CPLString m_osProfileDimName;
    int m_nProfileDimID;
    CPLString m_osProfileVariables;
    int m_nProfileVarID;
    bool m_bProfileVarUnlimited;
    int m_nParentIndexVarID;
    std::shared_ptr<nccfdriver::SGeometry_Reader> m_simpleGeometryReader;
    std::unique_ptr<nccfdriver::netCDFVID>
        layerVID_alloc;  // Allocation wrapper for group specific netCDFVID
    nccfdriver::netCDFVID &layerVID;  // refers to the "correct" VID
    std::string m_sgCRSname;
    size_t m_SGeometryFeatInd;

    const netCDFWriterConfigLayer *m_poLayerConfig;

    nccfdriver::ncLayer_SG_Metadata m_layerSGDefn;

    OGRFeature *GetNextRawFeature();
    double Get1DVarAsDouble(int nVarId, nc_type nVarType, size_t nIndex,
                            NCDFNoDataUnion noDataVal, bool *pbIsNoData);
    CPLErr GetFillValue(int nVarID, char **ppszValue);
    CPLErr GetFillValue(int nVarID, double *pdfValue);
    void GetNoDataValueForFloat(int nVarId, NCDFNoDataUnion *puNoData);
    void GetNoDataValueForDouble(int nVarId, NCDFNoDataUnion *puNoData);
    void GetNoDataValue(int nVarId, nc_type nVarType,
                        NCDFNoDataUnion *puNoData);
    bool FillVarFromFeature(OGRFeature *poFeature, int nMainDimId,
                            size_t nIndex);
    OGRFeature *buildSGeometryFeature(size_t featureInd);
    void netCDFWriteAttributesFromConf(
        int cdfid, int varid,
        const std::vector<netCDFWriterConfigAttribute> &aoAttributes);

  protected:
    bool FillFeatureFromVar(OGRFeature *poFeature, int nMainDimId,
                            size_t nIndex);

  public:
    netCDFLayer(netCDFDataset *poDS, int nLayerCDFId, const char *pszName,
                OGRwkbGeometryType eGeomType, OGRSpatialReference *poSRS);
    virtual ~netCDFLayer();

    bool Create(char **papszOptions,
                const netCDFWriterConfigLayer *poLayerConfig);
    void SetRecordDimID(int nRecordDimID);
    void SetXYZVars(int nXVarId, int nYVarId, int nZVarId);
    void SetWKTGeometryField(const char *pszWKTVarName);
    void SetGridMapping(const char *pszGridMapping);
    void SetProfile(int nProfileDimID, int nParentIndexVarID);
    void EnableSGBypass()
    {
        this->m_bLegacyCreateMode = false;
    }
    bool AddField(int nVarId);

    int GetCDFID() const
    {
        return m_nLayerCDFId;
    }
    void SetCDFID(int nId)
    {
        m_nLayerCDFId = nId;
    }
    void SetSGeometryRepresentation(
        const std::shared_ptr<nccfdriver::SGeometry_Reader> &sg)
    {
        m_simpleGeometryReader = sg;
    }
    nccfdriver::ncLayer_SG_Metadata &getLayerSGMetadata()
    {
        return m_layerSGDefn;
    }

    virtual void ResetReading() override;
    virtual OGRFeature *GetNextFeature() override;

    virtual GIntBig GetFeatureCount(int bForce) override;

    virtual int TestCapability(const char *pszCap) override;

    virtual OGRFeatureDefn *GetLayerDefn() override;

    virtual OGRErr ICreateFeature(OGRFeature *poFeature) override;
    virtual OGRErr CreateField(OGRFieldDefn *poFieldDefn,
                               int bApproxOK) override;
};

const char *NCDFGetProjectedCFUnit(const OGRSpatialReference *poSRS);
void NCDFWriteLonLatVarsAttributes(nccfdriver::netCDFVID &vcdf, int nVarLonID,
                                   int nVarLatID);
void NCDFWriteRLonRLatVarsAttributes(nccfdriver::netCDFVID &vcdf,
                                     int nVarRLonID, int nVarRLatID);
void NCDFWriteXYVarsAttributes(nccfdriver::netCDFVID &vcdf, int nVarXID,
                               int nVarYID, OGRSpatialReference *poSRS);
int NCDFWriteSRSVariable(int cdfid, const OGRSpatialReference *poSRS,
                         char **ppszCFProjection, bool bWriteGDALTags,
                         const std::string & = std::string());

double NCDFGetDefaultNoDataValue(int nCdfId, int nVarId, int nVarType,
                                 bool &bGotNoData);
#ifdef NETCDF_HAS_NC4
int64_t NCDFGetDefaultNoDataValueAsInt64(int nCdfId, int nVarId,
                                         bool &bGotNoData);
uint64_t NCDFGetDefaultNoDataValueAsUInt64(int nCdfId, int nVarId,
                                           bool &bGotNoData);
#endif

CPLErr NCDFGetAttr(int nCdfId, int nVarId, const char *pszAttrName,
                   double *pdfValue);
CPLErr NCDFGetAttr(int nCdfId, int nVarId, const char *pszAttrName,
                   char **pszValue);
bool NCDFIsUnlimitedDim(bool bIsNC4, int cdfid, int nDimId);
bool NCDFIsUserDefinedType(int ncid, int type);

CPLString NCDFGetGroupFullName(int nGroupId);

CPLErr NCDFResolveVar(int nStartGroupId, const char *pszVar, int *pnGroupId,
                      int *pnVarId, bool bMandatory = false);

// Dimension check functions.
bool NCDFIsVarLongitude(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarLatitude(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarProjectionX(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarProjectionY(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarVerticalCoord(int nCdfId, int nVarId, const char *pszVarName);
bool NCDFIsVarTimeCoord(int nCdfId, int nVarId, const char *pszVarName);

std::string NCDFReadMetadataAsJson(int cdfid);

extern CPLMutex *hNCMutex;

#ifdef ENABLE_NCDUMP
bool netCDFDatasetCreateTempFile(NetCDFFormatEnum eFormat,
                                 const char *pszTmpFilename, VSILFILE *fpSrc);
#endif

int GDAL_nc_open(const char *pszFilename, int nMode, int *pID);
int GDAL_nc_close(int cdfid);

#endif
