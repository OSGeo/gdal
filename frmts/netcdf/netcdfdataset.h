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
/*			     netCDFDataset				*/
/* ==================================================================== */
/************************************************************************/
#define MAX_STR_LEN            8192
#define CEA                    "cylindrical_equal_area"
#define LCEA                   "lambert_cylindrical_equal_area"
#define L_C_CONIC              "lambert_conformal_conic"
#define TM                     "transverse_mercator"
#define LAEA                   "lambert_azimuthal_equal_area"
#define GRD_MAPPING_NAME       "grid_mapping_name"
#define GRD_MAPPING            "grid_mapping"
#define COORDINATES            "coordinates"
#define LONLAT                 "lon lat"
#define LATITUDE_LONGITUDE     "latitude_longitude"
#define MERCATOR               "mercator"
#define ORTHOGRAPHIC           "orthographic"

#define STD_PARALLEL           "standard_parallel"
#define STD_PARALLEL_1         "standard_parallel_1"
#define STD_PARALLEL_2         "standard_parallel_2"
#define LONG_CENTRAL_MERIDIAN  "central_meridian"
#define LON_PROJ_ORIGIN        "longitude_of_projection_origin"
#define LAT_PROJ_ORIGIN        "latitude_of_projection_origin"
#define SCALE_FACTOR_ORIGIN    "scale_factor_at_projection_origin"
#define PROJ_X_ORIGIN          "projection_x_coordinate_origin"
#define PROJ_Y_ORIGIN          "projection_y_coordinate_origin"
#define EARTH_SHAPE            "GRIB_earth_shape"
#define EARTH_SHAPE_CODE       "GRIB_earth_shape_code"
#define SCALE_FACTOR           "scale_factor_at_central_meridian"
#define FALSE_EASTING          "false_easting"
#define FALSE_NORTHING         "false_northing"
#define EARTH_RADIUS           "earth_radius"
#define INVERSE_FLATTENING     "inverse_flattening"
#define LONG_PRIME_MERIDIAN    "longitude_of_prime_meridian"
#define SEMI_MAJOR_AXIS        "semi_major_axis"
#define SEMI_MINOR_AXIS        "semi_minor_axis"

#define STD_NAME               "standard_name"
#define LNG_NAME               "long_name"
#define UNITS                  "units"
#define AXIS                   "axis"
#define BOUNDS                 "bounds"
#define ORIG_AXIS              "original_units"

#define GDALNBDIM  2


typedef struct {
    const char *netCDFSRS;
    const char *SRS; }
oNetcdfSRS;

static const oNetcdfSRS poNetcdfSRS[] = {
    {"albers_conical_equal_area", SRS_PT_ALBERS_CONIC_EQUAL_AREA },
    {"azimuthal_equidistant", SRS_PT_AZIMUTHAL_EQUIDISTANT },
    {"cassini_soldner", SRS_PT_CASSINI_SOLDNER },
    {"cylindrical_equal_area", SRS_PT_CYLINDRICAL_EQUAL_AREA },
    {"eckert_iv", SRS_PT_ECKERT_IV },      
    {"eckert_vi", SRS_PT_ECKERT_VI },  
    {"equidistant_conic", SRS_PT_EQUIDISTANT_CONIC },
    {"equirectangular", SRS_PT_EQUIRECTANGULAR },
    {"gall_stereographic", SRS_PT_GALL_STEREOGRAPHIC },
    {"geostationary_satellite", SRS_PT_GEOSTATIONARY_SATELLITE },
    {"goode_homolosine", SRS_PT_GOODE_HOMOLOSINE },
    {"gnomonic", SRS_PT_GNOMONIC },
    {"hotine_oblique_mercator", SRS_PT_HOTINE_OBLIQUE_MERCATOR},
    {"hotine_oblique_marcator_2P", 
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

    {LONG_CENTRAL_MERIDIAN, SRS_PP_CENTRAL_MERIDIAN },
    {SCALE_FACTOR, SRS_PP_SCALE_FACTOR },   
    {STD_PARALLEL_1, SRS_PP_STANDARD_PARALLEL_1 },
    {STD_PARALLEL_2, SRS_PP_STANDARD_PARALLEL_2 },
    {"longitude_of_central_meridian", SRS_PP_LONGITUDE_OF_CENTER },
    {"longitude_of_projection_origin", SRS_PP_LONGITUDE_OF_ORIGIN }, 
    {"latitude_of_projection_origin", SRS_PP_LATITUDE_OF_ORIGIN }, 
    {FALSE_EASTING, SRS_PP_FALSE_EASTING },  
    {FALSE_NORTHING, SRS_PP_FALSE_NORTHING },       
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

  public:
    int           cdfid;
    char         **papszMetadata;
    char          papszDimName[NC_MAX_NAME][1024];
    int          *paDimIds;
    size_t        xdim, ydim;
    int           nDimXid, nDimYid;
    bool          bBottomUp;

		netCDFDataset( );
		~netCDFDataset( );
    
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
