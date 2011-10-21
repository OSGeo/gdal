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

/* GDAL or NETCDF driver defs */
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
#define NCDF_FILETYPE_NONE            0   /* Not a netCDF file */
#define NCDF_FILETYPE_NC              1   /* File type netCDF */
#define NCDF_FILETYPE_NC2             2   /* File type netCDF version 2 (64-bit)  */
#define NCDF_FILETYPE_NC4             3   /* File type netCDF version 4           */
#define NCDF_FILETYPE_NC4C            4   /* File type netCDF version 4 (classic) - not used yet */
/* File type HDF5, not supported here (lack of netCDF-4 support or extension is not .nc or .nc4 */
#define NCDF_FILETYPE_HDF5            5   
#define NCDF_FILETYPE_UNKNOWN         10  /* Filetype not determined (yet) */

/* compression parameters */
#define NCDF_COMPRESS_NONE            0   
/* TODO */
/* http://www.unidata.ucar.edu/software/netcdf/docs/BestPractices.html#Packed%20Data%20Values */
#define NCDF_COMPRESS_PACKED          1  
#define NCDF_COMPRESS_DEFLATE         2   
#define NCDF_DEFLATE_LEVEL            1  /* best time/size ratio */  
#define NCDF_COMPRESS_SZIP            3  /* no support for writting */ 

/* helper inline function for libnetcdf errors */
/* how can we make this a multi-line define ? */
//#define NCDF_ERR(status)  ( if ( status != NC_NOERR ) 
//{ CPLError( CE_Failure, CPLE_AppDefined, "netcdf error #%d : %s .\n", status, nc_strerror(status) ); } ) 
void NCDF_ERR(int status)  { if ( status != NC_NOERR ) { 
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "netcdf error #%d : %s .\n", 
                  status, nc_strerror(status) ); } } 

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
/* TODO add both in generic mapping, remove CF_PP_SCALE_FACTOR */
#define SCALE_FACTOR           "scale_factor" /* TODO: this has to go */
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


/************************************************************************/
/* ==================================================================== */
/*			     netCDFDataset		                             		*/
/* ==================================================================== */
/************************************************************************/

typedef struct {
    const char *netCDFSRS;
    const char *SRS; }
oNetcdfSRS;

static const oNetcdfSRS poNetcdfSRS[] = {
    {"albers_conical_equal_area", SRS_PT_ALBERS_CONIC_EQUAL_AREA },
    {"azimuthal_equidistant", SRS_PT_AZIMUTHAL_EQUIDISTANT },
    {"cassini_soldner", SRS_PT_CASSINI_SOLDNER },
    {"lambert_cylindrical_equal_area", SRS_PT_CYLINDRICAL_EQUAL_AREA },
    {"eckert_iv", SRS_PT_ECKERT_IV },      
    {"eckert_vi", SRS_PT_ECKERT_VI },  
    {"equidistant_conic", SRS_PT_EQUIDISTANT_CONIC },
    {"equirectangular", SRS_PT_EQUIRECTANGULAR },
    {"gall_stereographic", SRS_PT_GALL_STEREOGRAPHIC },
    {"geostationary_satellite", SRS_PT_GEOSTATIONARY_SATELLITE },
    {"goode_homolosine", SRS_PT_GOODE_HOMOLOSINE },
    {"gnomonic", SRS_PT_GNOMONIC },
    {"hotine_oblique_mercator", SRS_PT_HOTINE_OBLIQUE_MERCATOR},
    {"hotine_oblique_mercator_2P", 
     SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN},
    {"laborde_oblique_mercator", SRS_PT_LABORDE_OBLIQUE_MERCATOR },
    {"lambert_conformal_conic1", SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP },
    {"lambert_conformal_conic", SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP },
    {"lambert_azimuthal_equal_area", SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA },
    {"mercator_1sp", SRS_PT_MERCATOR_1SP },
    {"mercator_2sp", SRS_PT_MERCATOR_2SP },
    {"miller_cylindrical", SRS_PT_MILLER_CYLINDRICAL },
    {"mollweide", SRS_PT_MOLLWEIDE },
    {"new_zealand_map_grid", SRS_PT_NEW_ZEALAND_MAP_GRID },
    {"oblique_stereographic", SRS_PT_OBLIQUE_STEREOGRAPHIC }, 
    {"orthographic", SRS_PT_ORTHOGRAPHIC },
    {"polar_stereographic", SRS_PT_POLAR_STEREOGRAPHIC },
    {"polyconic", SRS_PT_POLYCONIC },
    {"robinson", SRS_PT_ROBINSON }, 
    {"sinusoidal", SRS_PT_SINUSOIDAL },  
    {"stereographic", SRS_PT_STEREOGRAPHIC },
    {"swiss_oblique_cylindrical", SRS_PT_SWISS_OBLIQUE_CYLINDRICAL},
    {"transverse_mercator", SRS_PT_TRANSVERSE_MERCATOR },
    {"TM_south_oriented", SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED },

    {CF_PP_LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN },
    {SCALE_FACTOR, SRS_PP_SCALE_FACTOR },   
    {CF_PP_STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1 },
    {CF_PP_STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2 },
    {"longitude_of_central_meridian", SRS_PP_LONGITUDE_OF_CENTER },
    {"longitude_of_projection_origin", SRS_PP_LONGITUDE_OF_ORIGIN }, 
    {"latitude_of_projection_origin", SRS_PP_LATITUDE_OF_ORIGIN }, 
    {CF_PP_FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {CF_PP_FALSE_NORTHING, SRS_PP_FALSE_NORTHING },       
    {NULL, NULL },
 };

class netCDFRasterBand;

class netCDFDataset : public GDALPamDataset
{
    CPLString    osSubdatasetName;
    int          bTreatAsSubdataset;

    double      adfGeoTransform[6];
    char        **papszSubDatasets;
    char        **papszGeolocation;
    CPLString    osFilename;
    int          *panBandDimPos;         // X, Y, Z postion in array
    int          *panBandZLev;
    char         *pszProjection;
    int          bGotGeoTransform;
    double       rint( double );

    double       FetchCopyParm( const char *pszGridMappingValue, 
                                const char *pszParm, double dfDefault );

    char **      FetchStandardParallels( const char *pszGridMappingValue );

    static int IdentifyFileType( GDALOpenInfo *, bool );

  public:
    int           cdfid;
    char         **papszMetadata;
    char          papszDimName[NC_MAX_NAME][1024];
    int          *paDimIds;
    size_t        xdim, ydim;
    int           nDimXid, nDimYid;
    bool          bBottomUp;
    int           nFileType;

    netCDFDataset( );
    ~netCDFDataset( );
    
    static int Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr      SafeStrcat(char**, char*, size_t*);
    CPLErr      ReadAttributes( int, int );

    CPLErr 	GetGeoTransform( double * );    

    const char * GetProjectionRef();

    char ** GetMetadata( const char * );

    void  CreateSubDatasetList( );

    void  SetProjection( int );

};

#endif
