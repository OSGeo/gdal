/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  C API and constant declarations for OGR Spatial References.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _OGR_SRS_API_H_INCLUDED
#define _OGR_SRS_API_H_INCLUDED

#ifndef SWIG
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
    OAO_Other=0,
    OAO_North=1,
    OAO_South=2,
    OAO_East=3,
    OAO_West=4,
    OAO_Up=5,
    OAO_Down=6
} OGRAxisOrientation;
    
const char CPL_DLL *OSRAxisEnumToName( OGRAxisOrientation eOrientation );

/* -------------------------------------------------------------------- */
/*      Datum types (corresponds to CS_DatumType).                      */
/* -------------------------------------------------------------------- */

typedef enum {
    ODT_HD_Min=1000,
    ODT_HD_Other=1000,
    ODT_HD_Classic=1001,
    ODT_HD_Geocentric=1002,
    ODT_HD_Max=1999,
    ODT_VD_Min=2000,
    ODT_VD_Other=2000,
    ODT_VD_Orthometric=2001,
    ODT_VD_Ellipsoidal=2002,
    ODT_VD_AltitudeBarometric=2003,
    ODT_VD_Normal=2004,
    ODT_VD_GeoidModelDerived=2005,
    ODT_VD_Depth=2006,
    ODT_VD_Max=2999,
    ODT_LD_Min=10000,
    ODT_LD_Max=32767
} OGRDatumType; 

#endif // ndef SWIG

/* ==================================================================== */
/*      Some standard WKT geographic coordinate systems.                */
/* ==================================================================== */

#define SRS_WKT_WGS84 "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",\"7030\"]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",\"6326\"]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],UNIT[\"degree\",0.0174532925199433,AUTHORITY[\"EPSG\",\"9108\"]],AUTHORITY[\"EPSG\",\"4326\"]]"

/* ==================================================================== */
/*      Some "standard" strings.                                        */
/* ==================================================================== */

#define SRS_PT_ALBERS_CONIC_EQUAL_AREA                                  \
                                "Albers_Conic_Equal_Area"
#define SRS_PT_AZIMUTHAL_EQUIDISTANT "Azimuthal_Equidistant"
#define SRS_PT_CASSINI_SOLDNER  "Cassini_Soldner"
#define SRS_PT_CYLINDRICAL_EQUAL_AREA "Cylindrical_Equal_Area"
#define SRS_PT_BONNE            "Bonne"
#define SRS_PT_ECKERT_I         "Eckert_I"
#define SRS_PT_ECKERT_II        "Eckert_II"
#define SRS_PT_ECKERT_III       "Eckert_III"
#define SRS_PT_ECKERT_IV        "Eckert_IV"
#define SRS_PT_ECKERT_V         "Eckert_V"
#define SRS_PT_ECKERT_VI        "Eckert_VI"
#define SRS_PT_EQUIDISTANT_CONIC                                        \
                                "Equidistant_Conic"
#define SRS_PT_EQUIRECTANGULAR  "Equirectangular"
#define SRS_PT_GALL_STEREOGRAPHIC                                       \
                                "Gall_Stereographic"
#define SRS_PT_GAUSSSCHREIBERTMERCATOR                                      \
                                "Gauss_Schreiber_Transverse_Mercator"
#define SRS_PT_GEOSTATIONARY_SATELLITE                                  \
                                "Geostationary_Satellite"
#define SRS_PT_GOODE_HOMOLOSINE "Goode_Homolosine"
#define SRS_PT_IGH              "Interrupted_Goode_Homolosine"
#define SRS_PT_GNOMONIC         "Gnomonic"
#define SRS_PT_HOTINE_OBLIQUE_MERCATOR_AZIMUTH_CENTER                   \
                                "Hotine_Oblique_Mercator_Azimuth_Center"
#define SRS_PT_HOTINE_OBLIQUE_MERCATOR                                  \
                                "Hotine_Oblique_Mercator"
#define SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN         \
                            "Hotine_Oblique_Mercator_Two_Point_Natural_Origin"
#define SRS_PT_LABORDE_OBLIQUE_MERCATOR                                 \
                                "Laborde_Oblique_Mercator"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP                              \
                                "Lambert_Conformal_Conic_1SP"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP                              \
                                "Lambert_Conformal_Conic_2SP"
#define SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM                      \
                                "Lambert_Conformal_Conic_2SP_Belgium"
#define SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA                     \
                                "Lambert_Azimuthal_Equal_Area"
#define SRS_PT_MERCATOR_1SP     "Mercator_1SP"
#define SRS_PT_MERCATOR_2SP     "Mercator_2SP"
// Mercator_Auxiliary_Sphere is used used by ESRI to mean EPSG:3875
#define SRS_PT_MERCATOR_AUXILIARY_SPHERE                                 \
                                "Mercator_Auxiliary_Sphere"
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

/* special mapinfo variants on Transverse Mercator */
#define SRS_PT_TRANSVERSE_MERCATOR_MI_21 \
                                "Transverse_Mercator_MapInfo_21"
#define SRS_PT_TRANSVERSE_MERCATOR_MI_22 \
                                "Transverse_Mercator_MapInfo_22"
#define SRS_PT_TRANSVERSE_MERCATOR_MI_23 \
                                "Transverse_Mercator_MapInfo_23"
#define SRS_PT_TRANSVERSE_MERCATOR_MI_24 \
                                "Transverse_Mercator_MapInfo_24"
#define SRS_PT_TRANSVERSE_MERCATOR_MI_25 \
                                "Transverse_Mercator_MapInfo_25"

#define SRS_PT_TUNISIA_MINING_GRID                                      \
                                "Tunisia_Mining_Grid"
#define SRS_PT_TWO_POINT_EQUIDISTANT                                    \
                                "Two_Point_Equidistant"
#define SRS_PT_VANDERGRINTEN    "VanDerGrinten"
#define SRS_PT_KROVAK           "Krovak"
#define SRS_PT_IMW_POLYCONIC    "International_Map_of_the_World_Polyconic"
#define SRS_PT_WAGNER_I         "Wagner_I"
#define SRS_PT_WAGNER_II        "Wagner_II"
#define SRS_PT_WAGNER_III       "Wagner_III"
#define SRS_PT_WAGNER_IV        "Wagner_IV"
#define SRS_PT_WAGNER_V         "Wagner_V"
#define SRS_PT_WAGNER_VI        "Wagner_VI"
#define SRS_PT_WAGNER_VII       "Wagner_VII"
#define SRS_PT_QSC              "Quadrilateralized_Spherical_Cube"
#define SRS_PT_AITOFF           "Aitoff" 
#define SRS_PT_WINKEL_I         "Winkel_I" 
#define SRS_PT_WINKEL_II        "Winkel_II" 
#define SRS_PT_WINKEL_TRIPEL    "Winkel_Tripel" 
#define SRS_PT_CRASTER_PARABOLIC    "Craster_Parabolic" 
#define SRS_PT_LOXIMUTHAL        "Loximuthal"
#define SRS_PT_QUARTIC_AUTHALIC  "Quartic_Authalic"

#define SRS_PP_CENTRAL_MERIDIAN         "central_meridian"
#define SRS_PP_SCALE_FACTOR             "scale_factor"
#define SRS_PP_STANDARD_PARALLEL_1      "standard_parallel_1"
#define SRS_PP_STANDARD_PARALLEL_2      "standard_parallel_2"
#define SRS_PP_PSEUDO_STD_PARALLEL_1    "pseudo_standard_parallel_1"
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
#define SRS_PP_SATELLITE_HEIGHT         "satellite_height"
#define SRS_PP_FIPSZONE                 "fipszone"
#define SRS_PP_ZONE                     "zone"
#define SRS_PP_LATITUDE_OF_1ST_POINT    "Latitude_Of_1st_Point"
#define SRS_PP_LONGITUDE_OF_1ST_POINT   "Longitude_Of_1st_Point"
#define SRS_PP_LATITUDE_OF_2ND_POINT    "Latitude_Of_2nd_Point"
#define SRS_PP_LONGITUDE_OF_2ND_POINT   "Longitude_Of_2nd_Point"

#define SRS_UL_METER            "Meter"
#define SRS_UL_FOOT             "Foot (International)" /* or just "FOOT"? */
#define SRS_UL_FOOT_CONV                    "0.3048"
#define SRS_UL_US_FOOT          "Foot_US" /* or "US survey foot" from EPSG */
#define SRS_UL_US_FOOT_CONV                 "0.3048006096012192"
#define SRS_UL_NAUTICAL_MILE    "Nautical Mile"
#define SRS_UL_NAUTICAL_MILE_CONV           "1852.0"
#define SRS_UL_LINK             "Link"          /* Based on US Foot */
#define SRS_UL_LINK_CONV                    "0.20116684023368047"
#define SRS_UL_CHAIN            "Chain"         /* based on US Foot */
#define SRS_UL_CHAIN_CONV                   "20.116684023368047"
#define SRS_UL_ROD              "Rod"           /* based on US Foot */
#define SRS_UL_ROD_CONV                     "5.02921005842012"
#define SRS_UL_LINK_Clarke      "Link_Clarke"          
#define SRS_UL_LINK_Clarke_CONV              "0.2011661949"

#define SRS_UL_KILOMETER        "Kilometer"
#define SRS_UL_KILOMETER_CONV                "1000."
#define SRS_UL_DECIMETER        "Decimeter"
#define SRS_UL_DECIMETER_CONV                "0.1"
#define SRS_UL_CENTIMETER       "Centimeter"
#define SRS_UL_CENTIMETER_CONV               "0.01"
#define SRS_UL_MILLIMETER       "Millimeter"
#define SRS_UL_MILLIMETER_CONV               "0.001"
#define SRS_UL_INTL_NAUT_MILE   "Nautical_Mile_International"
#define SRS_UL_INTL_NAUT_MILE_CONV           "1852.0"
#define SRS_UL_INTL_INCH        "Inch_International"
#define SRS_UL_INTL_INCH_CONV                "0.0254"
#define SRS_UL_INTL_FOOT        "Foot_International"
#define SRS_UL_INTL_FOOT_CONV                    "0.3048"
#define SRS_UL_INTL_YARD        "Yard_International"
#define SRS_UL_INTL_YARD_CONV                "0.9144"
#define SRS_UL_INTL_STAT_MILE   "Statute_Mile_International"
#define SRS_UL_INTL_STAT_MILE_CONV           "1609.344"
#define SRS_UL_INTL_FATHOM      "Fathom_International"
#define SRS_UL_INTL_FATHOM_CONV              "1.8288"
#define SRS_UL_INTL_CHAIN       "Chain_International"
#define SRS_UL_INTL_CHAIN_CONV               "20.1168"
#define SRS_UL_INTL_LINK        "Link_International"
#define SRS_UL_INTL_LINK_CONV                "0.201168"
#define SRS_UL_US_INCH          "Inch_US_Surveyor"
#define SRS_UL_US_INCH_CONV                  "0.025400050800101603"
#define SRS_UL_US_YARD          "Yard_US_Surveyor"
#define SRS_UL_US_YARD_CONV                  "0.914401828803658"
#define SRS_UL_US_CHAIN         "Chain_US_Surveyor"
#define SRS_UL_US_CHAIN_CONV                 "20.11684023368047"
#define SRS_UL_US_STAT_MILE     "Statute_Mile_US_Surveyor"
#define SRS_UL_US_STAT_MILE_CONV             "1609.347218694437"
#define SRS_UL_INDIAN_YARD      "Yard_Indian"
#define SRS_UL_INDIAN_YARD_CONV              "0.91439523"
#define SRS_UL_INDIAN_FOOT      "Foot_Indian"
#define SRS_UL_INDIAN_FOOT_CONV              "0.30479841"
#define SRS_UL_INDIAN_CHAIN     "Chain_Indian"
#define SRS_UL_INDIAN_CHAIN_CONV             "20.11669506"

#define SRS_UA_DEGREE           "degree"
#define SRS_UA_DEGREE_CONV                  "0.0174532925199433"
#define SRS_UA_RADIAN           "radian"

#define SRS_PM_GREENWICH        "Greenwich"

#define SRS_DN_NAD27            "North_American_Datum_1927"
#define SRS_DN_NAD83            "North_American_Datum_1983"
#define SRS_DN_WGS72            "WGS_1972"
#define SRS_DN_WGS84            "WGS_1984"

#define SRS_WGS84_SEMIMAJOR     6378137.0                                
#define SRS_WGS84_INVFLATTENING 298.257223563

#ifndef SWIG
/* -------------------------------------------------------------------- */
/*      C Wrappers for C++ objects and methods.                         */
/* -------------------------------------------------------------------- */
#ifndef _DEFINED_OGRSpatialReferenceH
#define _DEFINED_OGRSpatialReferenceH

#ifdef DEBUG
typedef struct OGRSpatialReferenceHS *OGRSpatialReferenceH;
typedef struct OGRCoordinateTransformationHS *OGRCoordinateTransformationH;
#else
typedef void *OGRSpatialReferenceH;                               
typedef void *OGRCoordinateTransformationH;
#endif

#endif


OGRSpatialReferenceH CPL_DLL CPL_STDCALL
      OSRNewSpatialReference( const char * /* = NULL */);
OGRSpatialReferenceH CPL_DLL CPL_STDCALL OSRCloneGeogCS( OGRSpatialReferenceH );
OGRSpatialReferenceH CPL_DLL CPL_STDCALL OSRClone( OGRSpatialReferenceH );
void CPL_DLL CPL_STDCALL OSRDestroySpatialReference( OGRSpatialReferenceH );

int CPL_DLL OSRReference( OGRSpatialReferenceH );
int CPL_DLL OSRDereference( OGRSpatialReferenceH );
void CPL_DLL OSRRelease( OGRSpatialReferenceH );

OGRErr CPL_DLL OSRValidate( OGRSpatialReferenceH );
OGRErr CPL_DLL OSRFixupOrdering( OGRSpatialReferenceH );
OGRErr CPL_DLL OSRFixup( OGRSpatialReferenceH );
OGRErr CPL_DLL OSRStripCTParms( OGRSpatialReferenceH );

OGRErr CPL_DLL CPL_STDCALL OSRImportFromEPSG( OGRSpatialReferenceH, int );
OGRErr CPL_DLL CPL_STDCALL OSRImportFromEPSGA( OGRSpatialReferenceH, int );
OGRErr CPL_DLL OSRImportFromWkt( OGRSpatialReferenceH, char ** );
OGRErr CPL_DLL OSRImportFromProj4( OGRSpatialReferenceH, const char *);
OGRErr CPL_DLL OSRImportFromESRI( OGRSpatialReferenceH, char **);
OGRErr CPL_DLL OSRImportFromPCI( OGRSpatialReferenceH hSRS, const char *,
                                 const char *, double * );
OGRErr CPL_DLL OSRImportFromUSGS( OGRSpatialReferenceH,
                                  long, long, double *, long);
OGRErr CPL_DLL OSRImportFromXML( OGRSpatialReferenceH, const char * );
OGRErr CPL_DLL OSRImportFromDict( OGRSpatialReferenceH, const char *, 
                                  const char * );
OGRErr CPL_DLL OSRImportFromPanorama( OGRSpatialReferenceH, long, long, long,
                                      double * );
OGRErr CPL_DLL OSRImportFromOzi( OGRSpatialReferenceH , const char *, const char *,
                                 const char * );
OGRErr CPL_DLL OSRImportFromMICoordSys( OGRSpatialReferenceH, const char *);
OGRErr CPL_DLL OSRImportFromERM( OGRSpatialReferenceH,
                                 const char *, const char *, const char * );
OGRErr CPL_DLL OSRImportFromUrl( OGRSpatialReferenceH, const char * );

OGRErr CPL_DLL CPL_STDCALL OSRExportToWkt( OGRSpatialReferenceH, char ** );
OGRErr CPL_DLL CPL_STDCALL OSRExportToPrettyWkt( OGRSpatialReferenceH, char **, int);
OGRErr CPL_DLL CPL_STDCALL OSRExportToProj4( OGRSpatialReferenceH, char **);
OGRErr CPL_DLL OSRExportToPCI( OGRSpatialReferenceH, char **, char **,
                               double ** );
OGRErr CPL_DLL OSRExportToUSGS( OGRSpatialReferenceH, long *, long *,
                                double **, long * );
OGRErr CPL_DLL OSRExportToXML( OGRSpatialReferenceH, char **, const char * );
OGRErr CPL_DLL OSRExportToPanorama( OGRSpatialReferenceH, long *, long *,
                                    long *, long *, double * );
OGRErr CPL_DLL OSRExportToMICoordSys( OGRSpatialReferenceH, char ** );
OGRErr CPL_DLL OSRExportToERM( OGRSpatialReferenceH, char *, char *, char * );

OGRErr CPL_DLL OSRMorphToESRI( OGRSpatialReferenceH );
OGRErr CPL_DLL OSRMorphFromESRI( OGRSpatialReferenceH );

OGRErr CPL_DLL CPL_STDCALL OSRSetAttrValue( OGRSpatialReferenceH hSRS,
                                const char * pszNodePath,
                                const char * pszNewNodeValue );
const char CPL_DLL * CPL_STDCALL OSRGetAttrValue( OGRSpatialReferenceH hSRS,
                           const char * pszName, int iChild /* = 0 */ );

OGRErr CPL_DLL OSRSetAngularUnits( OGRSpatialReferenceH, const char *, double );
double CPL_DLL OSRGetAngularUnits( OGRSpatialReferenceH, char ** );
OGRErr CPL_DLL OSRSetLinearUnits( OGRSpatialReferenceH, const char *, double );
OGRErr CPL_DLL OSRSetTargetLinearUnits( OGRSpatialReferenceH, const char *, const char *, double );
OGRErr CPL_DLL OSRSetLinearUnitsAndUpdateParameters( 
    OGRSpatialReferenceH, const char *, double );
double CPL_DLL OSRGetLinearUnits( OGRSpatialReferenceH, char ** );
double CPL_DLL OSRGetTargetLinearUnits( OGRSpatialReferenceH, const char *, char ** );

double CPL_DLL OSRGetPrimeMeridian( OGRSpatialReferenceH, char ** );

int CPL_DLL OSRIsGeographic( OGRSpatialReferenceH );
int CPL_DLL OSRIsLocal( OGRSpatialReferenceH );
int CPL_DLL OSRIsProjected( OGRSpatialReferenceH );
int CPL_DLL OSRIsCompound( OGRSpatialReferenceH );
int CPL_DLL OSRIsGeocentric( OGRSpatialReferenceH );
int CPL_DLL OSRIsVertical( OGRSpatialReferenceH );
int CPL_DLL OSRIsSameGeogCS( OGRSpatialReferenceH, OGRSpatialReferenceH );
int CPL_DLL OSRIsSameVertCS( OGRSpatialReferenceH, OGRSpatialReferenceH );
int CPL_DLL OSRIsSame( OGRSpatialReferenceH, OGRSpatialReferenceH );

OGRErr CPL_DLL OSRSetLocalCS( OGRSpatialReferenceH hSRS, const char *pszName );
OGRErr CPL_DLL OSRSetProjCS( OGRSpatialReferenceH hSRS, const char * pszName );
OGRErr CPL_DLL OSRSetGeocCS( OGRSpatialReferenceH hSRS, const char * pszName );
OGRErr CPL_DLL OSRSetWellKnownGeogCS( OGRSpatialReferenceH hSRS,
                                      const char * pszName );
OGRErr CPL_DLL CPL_STDCALL OSRSetFromUserInput( OGRSpatialReferenceH hSRS, 
                                    const char * );
OGRErr CPL_DLL OSRCopyGeogCSFrom( OGRSpatialReferenceH hSRS, 
                                  OGRSpatialReferenceH hSrcSRS );
OGRErr CPL_DLL OSRSetTOWGS84( OGRSpatialReferenceH hSRS, 
                              double, double, double, 
                              double, double, double, double );
OGRErr CPL_DLL OSRGetTOWGS84( OGRSpatialReferenceH hSRS, double *, int );
                        

OGRErr CPL_DLL OSRSetCompoundCS( OGRSpatialReferenceH hSRS,
                                 const char *pszName,
                                 OGRSpatialReferenceH hHorizSRS,
                                 OGRSpatialReferenceH hVertSRS );
OGRErr CPL_DLL OSRSetGeogCS( OGRSpatialReferenceH hSRS,
                      const char * pszGeogName,
                      const char * pszDatumName,
                      const char * pszEllipsoidName,
                      double dfSemiMajor, double dfInvFlattening,
                      const char * pszPMName /* = NULL */,
                      double dfPMOffset /* = 0.0 */,
                      const char * pszUnits /* = NULL */,
                      double dfConvertToRadians /* = 0.0 */ );

OGRErr CPL_DLL OSRSetVertCS( OGRSpatialReferenceH hSRS,
                      const char * pszVertCSName,
                      const char * pszVertDatumName,
                      int nVertDatumType );

double CPL_DLL OSRGetSemiMajor( OGRSpatialReferenceH, OGRErr * /* = NULL */ );
double CPL_DLL OSRGetSemiMinor( OGRSpatialReferenceH, OGRErr * /* = NULL */ );
double CPL_DLL OSRGetInvFlattening( OGRSpatialReferenceH, OGRErr * /*=NULL*/);

OGRErr CPL_DLL OSRSetAuthority( OGRSpatialReferenceH hSRS,
                         const char * pszTargetKey,
                         const char * pszAuthority,
                         int nCode );
const char CPL_DLL *OSRGetAuthorityCode( OGRSpatialReferenceH hSRS,
                                         const char * pszTargetKey );
const char CPL_DLL *OSRGetAuthorityName( OGRSpatialReferenceH hSRS,
                                         const char * pszTargetKey );
OGRErr CPL_DLL OSRSetProjection( OGRSpatialReferenceH, const char * );
OGRErr CPL_DLL OSRSetProjParm( OGRSpatialReferenceH, const char *, double );
double CPL_DLL OSRGetProjParm( OGRSpatialReferenceH hSRS,
                        const char * pszParmName, 
                        double dfDefault /* = 0.0 */,
                        OGRErr * /* = NULL */ );
OGRErr CPL_DLL OSRSetNormProjParm( OGRSpatialReferenceH, const char *, double);
double CPL_DLL OSRGetNormProjParm( OGRSpatialReferenceH hSRS,
                                   const char * pszParmName, 
                                   double dfDefault /* = 0.0 */,
                                   OGRErr * /* = NULL */ );

OGRErr CPL_DLL OSRSetUTM( OGRSpatialReferenceH hSRS, int nZone, int bNorth );
int    CPL_DLL OSRGetUTMZone( OGRSpatialReferenceH hSRS, int *pbNorth );
OGRErr CPL_DLL OSRSetStatePlane( OGRSpatialReferenceH hSRS, 
                                 int nZone, int bNAD83 );
OGRErr CPL_DLL OSRSetStatePlaneWithUnits( OGRSpatialReferenceH hSRS, 
                                          int nZone, int bNAD83,
                                          const char *pszOverrideUnitName,
                                          double dfOverrideUnit );
OGRErr CPL_DLL OSRAutoIdentifyEPSG( OGRSpatialReferenceH hSRS );

int    CPL_DLL OSREPSGTreatsAsLatLong( OGRSpatialReferenceH hSRS );
int    CPL_DLL OSREPSGTreatsAsNorthingEasting( OGRSpatialReferenceH hSRS );
const char CPL_DLL *OSRGetAxis( OGRSpatialReferenceH hSRS,
                                const char *pszTargetKey, int iAxis, 
                                OGRAxisOrientation *peOrientation );
OGRErr CPL_DLL OSRSetAxes( const char *pszTargetKey,
                           const char *pszXAxisName,
                           OGRAxisOrientation eXAxisOrientation,
                           const char *pszYAxisName, 
                           OGRAxisOrientation eYAxisOrientation );
/** Albers Conic Equal Area */
OGRErr CPL_DLL OSRSetACEA( OGRSpatialReferenceH hSRS, double dfStdP1, double dfStdP2,
                         double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );
    
/** Azimuthal Equidistant */
OGRErr CPL_DLL  OSRSetAE( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

/** Bonne */
OGRErr CPL_DLL OSRSetBonne(OGRSpatialReferenceH hSRS, 
                           double dfStandardParallel, double dfCentralMeridian,
                           double dfFalseEasting, double dfFalseNorthing );

/** Cylindrical Equal Area */
OGRErr CPL_DLL OSRSetCEA( OGRSpatialReferenceH hSRS, double dfStdP1, double dfCentralMeridian,
                        double dfFalseEasting, double dfFalseNorthing );

/** Cassini-Soldner */
OGRErr CPL_DLL OSRSetCS( OGRSpatialReferenceH hSRS, double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

/** Equidistant Conic */
OGRErr CPL_DLL OSRSetEC( OGRSpatialReferenceH hSRS, double dfStdP1, double dfStdP2,
                       double dfCenterLat, double dfCenterLong,
                       double dfFalseEasting, double dfFalseNorthing );

/** Eckert I-VI */
OGRErr CPL_DLL OSRSetEckert( OGRSpatialReferenceH hSRS,  int nVariation,
                             double dfCentralMeridian,
                             double dfFalseEasting, double dfFalseNorthing );

/** Eckert IV */
OGRErr CPL_DLL OSRSetEckertIV( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
                             double dfFalseEasting, double dfFalseNorthing );

/** Eckert VI */
OGRErr CPL_DLL OSRSetEckertVI( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
                             double dfFalseEasting, double dfFalseNorthing );

/** Equirectangular */
OGRErr CPL_DLL OSRSetEquirectangular(OGRSpatialReferenceH hSRS,
                              double dfCenterLat, double dfCenterLong,
                              double dfFalseEasting, double dfFalseNorthing );

/** Equirectangular generalized form */
OGRErr CPL_DLL OSRSetEquirectangular2( OGRSpatialReferenceH hSRS,
                              double dfCenterLat, double dfCenterLong,
                              double dfPseudoStdParallel1,
                              double dfFalseEasting,
                              double dfFalseNorthing );

/** Gall Stereograpic */
OGRErr CPL_DLL OSRSetGS( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
                       double dfFalseEasting, double dfFalseNorthing );
    
/** Goode Homolosine */
OGRErr CPL_DLL OSRSetGH( OGRSpatialReferenceH hSRS, double dfCentralMeridian,
                         double dfFalseEasting, double dfFalseNorthing );

/** Interrupted Goode Homolosine */
OGRErr CPL_DLL OSRSetIGH( OGRSpatialReferenceH hSRS );
    
/** GEOS - Geostationary Satellite View */
OGRErr CPL_DLL OSRSetGEOS( OGRSpatialReferenceH hSRS, 
                           double dfCentralMeridian, double dfSatelliteHeight,
                           double dfFalseEasting, double dfFalseNorthing );

/** Gauss Schreiber Transverse Mercator */    
OGRErr CPL_DLL OSRSetGaussSchreiberTMercator( OGRSpatialReferenceH hSRS,
                                  double dfCenterLat, double dfCenterLong,
                                  double dfScale,
                                  double dfFalseEasting,
                                  double dfFalseNorthing );
/** Gnomonic */
OGRErr CPL_DLL OSRSetGnomonic(OGRSpatialReferenceH hSRS,
                              double dfCenterLat, double dfCenterLong,
                            double dfFalseEasting, double dfFalseNorthing );

/** Oblique Mercator (aka HOM (variant B) */
OGRErr CPL_DLL OSRSetOM( OGRSpatialReferenceH hSRS,
                         double dfCenterLat, double dfCenterLong,
                         double dfAzimuth, double dfRectToSkew,
                         double dfScale,
                         double dfFalseEasting, double dfFalseNorthing );

/** Hotine Oblique Mercator using azimuth angle */
OGRErr CPL_DLL OSRSetHOM( OGRSpatialReferenceH hSRS,
                          double dfCenterLat, double dfCenterLong,
                          double dfAzimuth, double dfRectToSkew,
                          double dfScale,
                          double dfFalseEasting, double dfFalseNorthing );

/** Hotine Oblique Mercator using two points on centerline */
OGRErr CPL_DLL OSRSetHOM2PNO( OGRSpatialReferenceH hSRS, double dfCenterLat,
                              double dfLat1, double dfLong1,
                              double dfLat2, double dfLong2,
                              double dfScale,
                              double dfFalseEasting, double dfFalseNorthing );

/** International Map of the World Polyconic */
OGRErr CPL_DLL OSRSetIWMPolyconic( OGRSpatialReferenceH hSRS,
                                   double dfLat1, double dfLat2,
                                   double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing );

/** Krovak Oblique Conic Conformal */
OGRErr CPL_DLL OSRSetKrovak( OGRSpatialReferenceH hSRS,
                             double dfCenterLat, double dfCenterLong,
                             double dfAzimuth, double dfPseudoStdParallelLat,
                             double dfScale, 
                             double dfFalseEasting, double dfFalseNorthing );

/** Lambert Azimuthal Equal-Area */
OGRErr CPL_DLL OSRSetLAEA( OGRSpatialReferenceH hSRS,
                           double dfCenterLat, double dfCenterLong,
                           double dfFalseEasting, double dfFalseNorthing );

/** Lambert Conformal Conic */
OGRErr CPL_DLL OSRSetLCC( OGRSpatialReferenceH hSRS,
                          double dfStdP1, double dfStdP2,
                          double dfCenterLat, double dfCenterLong,
                          double dfFalseEasting, double dfFalseNorthing );

/** Lambert Conformal Conic 1SP */
OGRErr CPL_DLL OSRSetLCC1SP( OGRSpatialReferenceH hSRS,
                             double dfCenterLat, double dfCenterLong,
                             double dfScale,
                             double dfFalseEasting, double dfFalseNorthing );

/** Lambert Conformal Conic (Belgium) */
OGRErr CPL_DLL OSRSetLCCB( OGRSpatialReferenceH hSRS,
                           double dfStdP1, double dfStdP2,
                           double dfCenterLat, double dfCenterLong,
                           double dfFalseEasting, double dfFalseNorthing );
    
/** Miller Cylindrical */
OGRErr CPL_DLL OSRSetMC( OGRSpatialReferenceH hSRS,
                         double dfCenterLat, double dfCenterLong,
                         double dfFalseEasting, double dfFalseNorthing );

/** Mercator */
OGRErr CPL_DLL OSRSetMercator( OGRSpatialReferenceH hSRS,
                               double dfCenterLat, double dfCenterLong,
                               double dfScale, 
                               double dfFalseEasting, double dfFalseNorthing );

/** Mollweide */
OGRErr CPL_DLL  OSRSetMollweide( OGRSpatialReferenceH hSRS,
                                 double dfCentralMeridian,
                                 double dfFalseEasting,
                                 double dfFalseNorthing );

/** New Zealand Map Grid */
OGRErr CPL_DLL OSRSetNZMG( OGRSpatialReferenceH hSRS,
                           double dfCenterLat, double dfCenterLong,
                           double dfFalseEasting, double dfFalseNorthing );

/** Oblique Stereographic */
OGRErr CPL_DLL OSRSetOS( OGRSpatialReferenceH hSRS,
                         double dfOriginLat, double dfCMeridian,
                         double dfScale,
                         double dfFalseEasting,double dfFalseNorthing);
    
/** Orthographic */
OGRErr CPL_DLL OSRSetOrthographic( OGRSpatialReferenceH hSRS,
                                   double dfCenterLat, double dfCenterLong,
                                   double dfFalseEasting,
                                   double dfFalseNorthing);

/** Polyconic */
OGRErr CPL_DLL OSRSetPolyconic( OGRSpatialReferenceH hSRS,
                                double dfCenterLat, double dfCenterLong,
                                double dfFalseEasting, double dfFalseNorthing );

/** Polar Stereographic */
OGRErr CPL_DLL OSRSetPS( OGRSpatialReferenceH hSRS,
                         double dfCenterLat, double dfCenterLong,
                         double dfScale,
                         double dfFalseEasting, double dfFalseNorthing);
    
/** Robinson */
OGRErr CPL_DLL OSRSetRobinson( OGRSpatialReferenceH hSRS,
                               double dfCenterLong, 
                               double dfFalseEasting, double dfFalseNorthing );
    
/** Sinusoidal */
OGRErr CPL_DLL OSRSetSinusoidal( OGRSpatialReferenceH hSRS,
                                 double dfCenterLong, 
                                 double dfFalseEasting,
                                 double dfFalseNorthing );
    
/** Stereographic */
OGRErr CPL_DLL OSRSetStereographic( OGRSpatialReferenceH hSRS,
                                    double dfCenterLat, double dfCenterLong,
                                    double dfScale,
                                    double dfFalseEasting,
                                    double dfFalseNorthing);
    
/** Swiss Oblique Cylindrical */
OGRErr CPL_DLL OSRSetSOC( OGRSpatialReferenceH hSRS,
                          double dfLatitudeOfOrigin, double dfCentralMeridian,
                          double dfFalseEasting, double dfFalseNorthing );
    
/** Transverse Mercator
 *
 * Special processing available for Transverse Mercator with GDAL &gt;= 1.10 and PROJ &gt;= 4.8 :
 * see OGRSpatialReference::exportToProj4().
 */

OGRErr CPL_DLL OSRSetTM( OGRSpatialReferenceH hSRS,
                         double dfCenterLat, double dfCenterLong,
                         double dfScale,
                         double dfFalseEasting, double dfFalseNorthing );

/** Transverse Mercator variant */
OGRErr CPL_DLL OSRSetTMVariant( 
    OGRSpatialReferenceH hSRS, const char *pszVariantName,
    double dfCenterLat, double dfCenterLong,
    double dfScale,
    double dfFalseEasting, double dfFalseNorthing );

/** Tunesia Mining Grid  */
OGRErr CPL_DLL OSRSetTMG( OGRSpatialReferenceH hSRS,
                          double dfCenterLat, double dfCenterLong, 
                         double dfFalseEasting, double dfFalseNorthing );

/** Transverse Mercator (South Oriented) */
OGRErr CPL_DLL OSRSetTMSO( OGRSpatialReferenceH hSRS,
                           double dfCenterLat, double dfCenterLong,
                           double dfScale,
                           double dfFalseEasting, double dfFalseNorthing );

/** VanDerGrinten */
OGRErr CPL_DLL OSRSetVDG( OGRSpatialReferenceH hSRS,
                          double dfCenterLong,
                          double dfFalseEasting, double dfFalseNorthing );

/** Wagner I -- VII */
OGRErr CPL_DLL OSRSetWagner( OGRSpatialReferenceH hSRS, int nVariation,
                             double dfFalseEasting,
                             double dfFalseNorthing );

/** Quadrilateralized Spherical Cube */
OGRErr CPL_DLL OSRSetQSC( OGRSpatialReferenceH hSRS,
                              double dfCenterLat, double dfCenterLong );


void CPL_DLL OSRCleanup( void );

/* -------------------------------------------------------------------- */
/*      OGRCoordinateTransform C API.                                   */
/* -------------------------------------------------------------------- */
OGRCoordinateTransformationH CPL_DLL CPL_STDCALL
OCTNewCoordinateTransformation( OGRSpatialReferenceH hSourceSRS,
                                OGRSpatialReferenceH hTargetSRS );
void CPL_DLL CPL_STDCALL 
      OCTDestroyCoordinateTransformation( OGRCoordinateTransformationH );

int CPL_DLL CPL_STDCALL
OCTTransform( OGRCoordinateTransformationH hCT,
              int nCount, double *x, double *y, double *z );

int CPL_DLL CPL_STDCALL
OCTTransformEx( OGRCoordinateTransformationH hCT,
                int nCount, double *x, double *y, double *z,
                int *pabSuccess );

/* this is really private to OGR. */
char *OCTProj4Normalize( const char *pszProj4Src );

void OCTCleanupProjMutex( void );

/* -------------------------------------------------------------------- */
/*      Projection transform dictionary query.                          */
/* -------------------------------------------------------------------- */

char CPL_DLL ** OPTGetProjectionMethods( void );
char CPL_DLL ** OPTGetParameterList( const char * pszProjectionMethod,
                             char ** ppszUserName );
int CPL_DLL OPTGetParameterInfo( const char * pszProjectionMethod,
                                 const char * pszParameterName,
                                 char ** ppszUserName,
                                 char ** ppszType,
                                 double *pdfDefaultValue );

CPL_C_END

#endif /* ndef SWIG */

#endif /* ndef _OGR_SRS_API_H_INCLUDED */
