/******************************************************************************
 * $Id$
 *
 * Name:     osr.i
 * Project:  GDAL Python Interface
 * Purpose:  OSR Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.1  2005/02/15 20:50:09  kruland
 * Minimal SpatialReference Object to make gcore pretty much succeed.
 *
 *
*/

%module osr

%pythoncode %{
SRS_PT_ALBERS_CONIC_EQUAL_AREA	= "Albers_Conic_Equal_Area"
SRS_PT_AZIMUTHAL_EQUIDISTANT	= "Azimuthal_Equidistant"
SRS_PT_CASSINI_SOLDNER		= "Cassini_Soldner"
SRS_PT_CYLINDRICAL_EQUAL_AREA	= "Cylindrical_Equal_Area"
SRS_PT_ECKERT_IV		= "Eckert_IV"
SRS_PT_ECKERT_VI		= "Eckert_VI"
SRS_PT_EQUIDISTANT_CONIC	= "Equidistant_Conic"
SRS_PT_EQUIRECTANGULAR		= "Equirectangular"
SRS_PT_GALL_STEREOGRAPHIC	= "Gall_Stereographic"
SRS_PT_GNOMONIC			= "Gnomonic"
SRS_PT_GOODE_HOMOLOSINE         = "Goode_Homolosine"
SRS_PT_HOTINE_OBLIQUE_MERCATOR	= "Hotine_Oblique_Mercator"
SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN = \
    "Hotine_Oblique_Mercator_Two_Point_Natural_Origin"
SRS_PT_LABORDE_OBLIQUE_MERCATOR	= "Laborde_Oblique_Mercator"
SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP = "Lambert_Conformal_Conic_1SP"
SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP = "Lambert_Conformal_Conic_2SP"
SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM = \
    "Lambert_Conformal_Conic_2SP_Belgium)"
SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA = \
    "Lambert_Azimuthal_Equal_Area"
SRS_PT_MERCATOR_1SP		= "Mercator_1SP"
SRS_PT_MERCATOR_2SP		= "Mercator_2SP"
SRS_PT_MILLER_CYLINDRICAL	= "Miller_Cylindrical"
SRS_PT_MOLLWEIDE		= "Mollweide"
SRS_PT_NEW_ZEALAND_MAP_GRID     = "New_Zealand_Map_Grid"
SRS_PT_OBLIQUE_STEREOGRAPHIC    = "Oblique_Stereographic"
SRS_PT_ORTHOGRAPHIC		= "Orthographic"
SRS_PT_POLAR_STEREOGRAPHIC      = "Polar_Stereographic"
SRS_PT_POLYCONIC		= "Polyconic"
SRS_PT_ROBINSON			= "Robinson"
SRS_PT_SINUSOIDAL		= "Sinusoidal"
SRS_PT_STEREOGRAPHIC		= "Stereographic"
SRS_PT_SWISS_OBLIQUE_CYLINDRICAL= "Swiss_Oblique_Cylindrical"
SRS_PT_TRANSVERSE_MERCATOR      = "Transverse_Mercator"
SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED = \
    "Transverse_Mercator_South_Orientated"
SRS_PT_TRANSVERSE_MERCATOR_MI_22= "Transverse_Mercator_MapInfo_22"
SRS_PT_TRANSVERSE_MERCATOR_MI_23= "Transverse_Mercator_MapInfo_23"
SRS_PT_TRANSVERSE_MERCATOR_MI_24= "Transverse_Mercator_MapInfo_24"
SRS_PT_TRANSVERSE_MERCATOR_MI_25= "Transverse_Mercator_MapInfo_25"
SRS_PT_TUNISIA_MINING_GRID	= "Tunisia_Mining_Grid"
SRS_PT_VANDERGRINTEN		= "VanDerGrinten"
SRS_PT_KROVAK			= "Krovak"

SRS_PP_CENTRAL_MERIDIAN         = "central_meridian"
SRS_PP_SCALE_FACTOR             = "scale_factor"
SRS_PP_STANDARD_PARALLEL_1      = "standard_parallel_1"
SRS_PP_STANDARD_PARALLEL_2      = "standard_parallel_2"
SRS_PP_PSEUDO_STD_PARALLEL_1    = "pseudo_standard_parallel_1"
SRS_PP_LONGITUDE_OF_CENTER      = "longitude_of_center"
SRS_PP_LATITUDE_OF_CENTER       = "latitude_of_center"
SRS_PP_LONGITUDE_OF_ORIGIN      = "longitude_of_origin"
SRS_PP_LATITUDE_OF_ORIGIN       = "latitude_of_origin"
SRS_PP_FALSE_EASTING            = "false_easting"
SRS_PP_FALSE_NORTHING           = "false_northing"
SRS_PP_AZIMUTH                  = "azimuth"
SRS_PP_LONGITUDE_OF_POINT_1     = "longitude_of_point_1"
SRS_PP_LATITUDE_OF_POINT_1      = "latitude_of_point_1"
SRS_PP_LONGITUDE_OF_POINT_2     = "longitude_of_point_2"
SRS_PP_LATITUDE_OF_POINT_2      = "latitude_of_point_2"
SRS_PP_LONGITUDE_OF_POINT_3     = "longitude_of_point_3"
SRS_PP_LATITUDE_OF_POINT_3      = "latitude_of_point_3"
SRS_PP_RECTIFIED_GRID_ANGLE     = "rectified_grid_angle"
SRS_PP_LANDSAT_NUMBER           = "landsat_number"
SRS_PP_PATH_NUMBER              = "path_number"
SRS_PP_PERSPECTIVE_POINT_HEIGHT = "perspective_point_height"
SRS_PP_FIPSZONE                 = "fipszone"
SRS_PP_ZONE                     = "zone"

SRS_UL_METER			= "Meter"
SRS_UL_FOOT			= "Foot (International)"
SRS_UL_FOOT_CONV                = "0.3048"
SRS_UL_US_FOOT			= "U.S. Foot"
SRS_UL_US_FOOT_CONV             = "0.3048006"
SRS_UL_NAUTICAL_MILE		= "Nautical Mile"
SRS_UL_NAUTICAL_MILE_CONV       = "1852.0"
SRS_UL_LINK			= "Link"
SRS_UL_LINK_CONV                = "0.20116684023368047"
SRS_UL_CHAIN			= "Chain"
SRS_UL_CHAIN_CONV               = "2.0116684023368047"
SRS_UL_ROD			= "Rod"
SRS_UL_ROD_CONV                 = "5.02921005842012"

SRS_DN_NAD27			= "North_American_Datum_1927"
SRS_DN_NAD83			= "North_American_Datum_1983"
SRS_DN_WGS72			= "WGS_1972"
SRS_DN_WGS84			= "WGS_1984"

SRS_WGS84_SEMIMAJOR             = 6378137.0
SRS_WGS84_INVFLATTENING         = 298.257223563
%}

%{
#include <iostream>
using namespace std;

#include "ogr_srs_api.h"

typedef void SpatialReference;
%}

%import gdal_typemaps.i

class SpatialReference {
private:
  ~SpatialReference();
public:
%extend {
  SpatialReference( char const * arg = "" ) {
    return (SpatialReference*) OSRNewSpatialReference(arg);
  }

%apply (char **ignorechange) { char **arg };
  OGRErr ImportFromWkt( char **arg ) {
    return OSRImportFromWkt( self, arg );
  }
%clear (char **arg);

  int IsSame( SpatialReference *rhs ) {
    return OSRIsSame( self, rhs );
  }

}
};
