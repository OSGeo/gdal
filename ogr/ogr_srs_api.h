/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API and constant declarations for OGR Spatial References.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.6  2000/05/30 22:45:44  warmerda
 * added OSRCloneGeogCS()
 *
 * Revision 1.5  2000/04/26 18:25:56  warmerda
 * added missing CPL_DLL attributes
 *
 * Revision 1.4  2000/03/22 01:09:43  warmerda
 * added SetProjCS and SetWellKnownTextCS
 *
 * Revision 1.3  2000/03/20 23:33:51  warmerda
 * updated docs a bit
 *
 * Revision 1.2  2000/03/20 22:39:31  warmerda
 * Added C API.
 *
 * Revision 1.1  2000/03/16 19:04:14  warmerda
 * New
 *
 */

#ifndef _OGR_SRS_API_H_INCLUDED
#define _OGR_SRS_API_H_INCLUDED

#include "ogr_core.h"

CPL_C_START

/**
 * \file ogr_srs_api.h
 * 
 * C spatial reference system services and defines.
 * 
 * See also: ogr_spatialref.h
 */

/* -------------------------------------------------------------------- */
/*      Axis orientations (corresponds to CS_AxisOrientationEnum).      */
/* -------------------------------------------------------------------- */
typedef enum {
    CS_AO_Other=0,
    CS_AO_North=1,
    CS_AO_South=2,
    CS_AO_East=3,
    CS_AO_West=4,
    CS_AO_Up=5,
    CS_AO_Down=6
} OGRAxisOrientation;
    
/* -------------------------------------------------------------------- */
/*      Datum types (corresponds to CS_DatumType).                      */
/* -------------------------------------------------------------------- */

typedef enum {
    CS_HD_Min=1000,
    CS_HD_Other=1000,
    CS_HD_Classic=1001,
    CS_HD_Geocentric=1002,
    CS_HD_Max=1999,
    CS_VD_Min=2000,
    CS_VD_Other=2000,
    CS_VD_Orthometric=2001,
    CS_VD_Ellipsoidal=2002,
    CS_VD_AltitudeBarometric=2003,
    CS_VD_Normal=2004,
    CS_VD_GeoidModelDerived=2005,
    CS_VD_Depth=2006,
    CS_VD_Max=2999,
    CS_LD_Min=10000,
    CS_LD_Max=32767
} OGRDatumType; 

/* ==================================================================== */
/*      Some "standard" strings.                                        */
/* ==================================================================== */

#define SRS_PT_ALBERS_CONIC_EQUAL_AREA                                  \
                                "Albers_Conic_Equal_Area"
#define SRS_PT_AZIMUTHAL_EQUIDISTANT "Azimuthal_Equidistant"
#define SRS_PT_CASSINI_SOLDNER  "Cassini_Soldner"
#define SRS_PT_CYLINDRICAL_EQUAL_AREA "Cylindrical_Equal_Area"
#define SRS_PT_ECKERT_IV        "Eckert_IV"
#define SRS_PT_ECKERT_VI        "Eckert_VI"
#define SRS_PT_EQUIDISTANT_CONIC "Equidistant_Conic"
#define SRS_PT_EQUIRECTANGULAR  "Equirectangular"
#define SRS_PT_GALL_STEREOGRAPHIC "Gall_Stereographic"
#define SRS_PT_GNOMONIC         "Gnomonic"
#define SRS_PT_HOTINE_OBLIQUE_MERCATOR                                  \
                                "Hotine_Oblique_Mercator"
#define SRS_PT_LABORDE_OBLIQUE_MERCATOR                                 \
                                "Laborde_Oblique_Mercator"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP                              \
                                "Lambert_Conformal_Conic_1SP"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP                              \
                                "Lambert_Conformal_Conic_2SP"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM                      \
                                "Lambert_Conformal_Conic_2SP_Belgium)"
#define SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA                     \
                                "Lambert_Azimuthal_Equal_Area"
#define SRS_PT_MERCATOR_1SP     "Mercator_1SP"
#define SRS_PT_MERCATOR_2SP     "Mercator_2SP"
#define SRS_PT_MILLER_CYLINDRICAL "Miller_Cylindrical"
#define SRS_PT_MOLLWEIDE        "Mollweide"
#define SRS_PT_NEW_ZEALAND_MAP_GRID                                     \
                                "New_Zealand_Map_Grid"
#define SRS_PT_OBLIQUE_STEREOGRAPHIC                                    \
                                "Oblique_Stereographic"
#define SRS_PT_ORTHOGRAPHIC     "Orthographic"
#define SRS_PT_POLAR_STEREOGRAPHIC                                      \
                                "Polar_Stereographic"
#define SRS_PT_POLYCONIC        "Polyconic"
#define SRS_PT_ROBINSON         "Robinson"
#define SRS_PT_SINUSOIDAL       "Sinusoidal"
#define SRS_PT_STEREOGRAPHIC    "Stereographic"
#define SRS_PT_SWISS_OBLIQUE_CYLINDRICAL                                \
                                "Swiss_Oblique_Cylindrical"
#define SRS_PT_TRANSVERSE_MERCATOR                                      \
                                "Transverse_Mercator"
#define SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED                       \
                                "Transverse_Mercator_South_Orientated"
#define SRS_PT_TUNISIA_MINING_GRID                                      \
                                "Tunisia_Mining_Grid"
#define SRS_PT_VANDERGRINTEN    "VanDerGrinten"

                                

#define SRS_PP_CENTRAL_MERIDIAN         "central_meridian"
#define SRS_PP_SCALE_FACTOR             "scale_factor"
#define SRS_PP_STANDARD_PARALLEL_1      "standard_parallel_1"
#define SRS_PP_STANDARD_PARALLEL_2      "standard_parallel_2"
#define SRS_PP_LONGITUDE_OF_CENTER      "longitude_of_center"
#define SRS_PP_LATITUDE_OF_CENTER       "latitude_of_center"
#define SRS_PP_LONGITUDE_OF_ORIGIN      "longitude_of_origin"
#define SRS_PP_LATITUDE_OF_ORIGIN       "latitude_of_origin"
#define SRS_PP_FALSE_EASTING            "false_easting"
#define SRS_PP_FALSE_NORTHING           "false_northing"
#define SRS_PP_AZIMUTH                  "azimuth"
#define SRS_PP_LONGITUDE_OF_POINT_1     "longitude_of_point_1"
#define SRS_PP_LATITUDE_OF_POINT_1      "latitude_of_point_1"
#define SRS_PP_LONGITUDE_OF_POINT_2     "longitude_of_point_2"
#define SRS_PP_LATITUDE_OF_POINT_2      "latitude_of_point_2"
#define SRS_PP_LONGITUDE_OF_POINT_3     "longitude_of_point_3"
#define SRS_PP_LATITUDE_OF_POINT_3      "latitude_of_point_3"
#define SRS_PP_RECTIFIED_GRID_ANGLE     "rectified_grid_angle"
#define SRS_PP_LANDSAT_NUMBER           "landsat_number"
#define SRS_PP_PATH_NUMBER              "path_number"
#define SRS_PP_PERSPECTIVE_POINT_HEIGHT "perspective_point_height"
#define SRS_PP_FIPSZONE                 "fipszone"
#define SRS_PP_ZONE                     "zone"

#define SRS_UL_METER            "Meter"
#define SRS_UL_FOOT             "Foot (International)" /* or just "FOOT"? */
#define SRS_UL_FOOT_CONV                    "0.3048"
#define SRS_UL_US_FOOT          "U.S. Foot" /* or "US survey foot" */
#define SRS_UL_US_FOOT_CONV                 "0.3048006"
#define SRS_UL_NAUTICAL_MILE    "Nautical Mile"
#define SRS_UL_NAUTICAL_MILE_CONV           "1852.0"
#define SRS_UL_LINK             "Link"          /* Based on US Foot */
#define SRS_UL_LINK_CONV                    "0.20116684023368047"
#define SRS_UL_CHAIN            "Chain"         /* based on US Foot */
#define SRS_UL_CHAIN_CONV                   "2.0116684023368047"
#define SRS_UL_ROD              "Rod"           /* based on US Foot */
#define SRS_UL_ROD_CONV                     "5.02921005842012"

#define SRS_UA_DEGREE           "degree"
#define SRS_UA_DEGREE_CONV                  "0.0174532925199433"
#define SRS_UA_RADIAN           "radian"

#define SRS_PM_GREENWICH        "Greenwich"

#define SRS_DN_NAD27            "North American Datum 1927"
#define SRS_DN_NAD83            "North American Datum 1983"
#define SRS_DN_WGS84            "World Geodetic System 1984"

#define SRS_WGS84_SEMIMAJOR     6378137.0                                
#define SRS_WGS84_INVFLATTENING 298.257223563

/* -------------------------------------------------------------------- */
/*      C Wrappers for C++ objects and methods.                         */
/* -------------------------------------------------------------------- */

typedef void *OGRSpatialReferenceH;                               
typedef void *OGRCoordinateTransformationH;

OGRSpatialReferenceH CPL_DLL
      OSRNewSpatialReference( const char * /* = NULL */);
OGRSpatialReferenceH CPL_DLL OSRCloneGeogCS( OGRSpatialReferenceH );
void CPL_DLL OSRDestroySpatialReference( OGRSpatialReferenceH );

int CPL_DLL OSRReference( OGRSpatialReferenceH );
int CPL_DLL OSRDereference( OGRSpatialReferenceH );

OGRErr CPL_DLL OSRImportFromEPSG( OGRSpatialReferenceH, int );
OGRErr CPL_DLL OSRImportFromWkt( OGRSpatialReferenceH, char ** );
OGRErr CPL_DLL OSRExportToWkt( OGRSpatialReferenceH, char ** );

OGRErr CPL_DLL OSRSetAttrValue( OGRSpatialReferenceH hSRS,
                                const char * pszNodePath,
                                const char * pszNewNodeValue );
const char CPL_DLL * OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                           const char * pszName, int iChild /* = 0 */ );

OGRErr CPL_DLL OSRSetLinearUnits( OGRSpatialReferenceH, const char *, double );
double CPL_DLL OSRGetLinearUnits( OGRSpatialReferenceH, char ** );

int CPL_DLL OSRIsGeographic( OGRSpatialReferenceH );
int CPL_DLL OSRIsProjected( OGRSpatialReferenceH );
int CPL_DLL OSRIsSameGeogCS( OGRSpatialReferenceH, OGRSpatialReferenceH );
int CPL_DLL OSRIsSame( OGRSpatialReferenceH, OGRSpatialReferenceH );

OGRErr CPL_DLL OSRSetProjCS( OGRSpatialReferenceH hSRS, const char * pszName );
OGRErr CPL_DLL OSRSetWellKnownGeogCS( OGRSpatialReferenceH hSRS,
                                      const char * pszName );

OGRErr CPL_DLL OSRSetGeogCS( OGRSpatialReferenceH hSRS,
                      const char * pszGeogName,
                      const char * pszDatumName,
                      const char * pszEllipsoidName,
                      double dfSemiMajor, double dfInvFlattening,
                      const char * pszPMName /* = NULL */,
                      double dfPMOffset /* = 0.0 */,
                      const char * pszUnits /* = NULL */,
                      double dfConvertToRadians /* = 0.0 */ );

double CPL_DLL OSRGetSemiMajor( OGRSpatialReferenceH, OGRErr * /* = NULL */ );
double CPL_DLL OSRGetSemiMinor( OGRSpatialReferenceH, OGRErr * /* = NULL */ );
double CPL_DLL OSRGetInvFlattening( OGRSpatialReferenceH, OGRErr * /*=NULL*/);

OGRErr CPL_DLL OSRSetAuthority( OGRSpatialReferenceH hSRS,
                         const char * pszTargetKey,
                         const char * pszAuthority,
                         int nCode );
OGRErr CPL_DLL OSRSetProjParm( OGRSpatialReferenceH, const char *, double );
double CPL_DLL OSRGetProjParm( OGRSpatialReferenceH hSRS,
                        const char * pszParmName, 
                        double dfDefault /* = 0.0 */,
                        OGRErr * /* = NULL */ );

OGRErr CPL_DLL OSRSetUTM( OGRSpatialReferenceH hSRS, int nZone, int bNorth );
int    CPL_DLL OSRGetUTMZone( OGRSpatialReferenceH hSRS, int *pbNorth );

/* -------------------------------------------------------------------- */
/*      OGRCoordinateTransform C API.                                   */
/* -------------------------------------------------------------------- */
OGRCoordinateTransformationH CPL_DLL
OCTNewCoordinateTransformation( OGRSpatialReferenceH hSourceSRS,
                                OGRSpatialReferenceH hTargetSRS );
void CPL_DLL
      OCTDestroyCoordinateTransformation( OGRCoordinateTransformationH );

int CPL_DLL OCTTransform( OGRCoordinateTransformationH hCT,
                  int nCount, double *x, double *y, double *z );

CPL_C_END

#endif /* ndef _OGR_SRS_API_H_INCLUDED */
