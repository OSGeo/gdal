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
 * Revision 1.2  2005/02/17 03:39:25  kruland
 * Use %constant decls for the constants.
 *
 * Revision 1.1  2005/02/15 20:50:09  kruland
 * Minimal SpatialReference Object to make gcore pretty much succeed.
 *
 *
*/

%module osr

%constant SRS_PT_ALBERS_CONIC_EQUAL_AREA	= "Albers_Conic_Equal_Area";
%constant SRS_PT_AZIMUTHAL_EQUIDISTANT	= "Azimuthal_Equidistant";
%constant SRS_PT_CASSINI_SOLDNER		= "Cassini_Soldner";
%constant SRS_PT_CYLINDRICAL_EQUAL_AREA	= "Cylindrical_Equal_Area";
%constant SRS_PT_ECKERT_IV		= "Eckert_IV";
%constant SRS_PT_ECKERT_VI		= "Eckert_VI";
%constant SRS_PT_EQUIDISTANT_CONIC	= "Equidistant_Conic";
%constant SRS_PT_EQUIRECTANGULAR		= "Equirectangular";
%constant SRS_PT_GALL_STEREOGRAPHIC	= "Gall_Stereographic";
%constant SRS_PT_GNOMONIC			= "Gnomonic";
%constant SRS_PT_GOODE_HOMOLOSINE         = "Goode_Homolosine";
%constant SRS_PT_HOTINE_OBLIQUE_MERCATOR	= "Hotine_Oblique_Mercator";
%constant SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN = "Hotine_Oblique_Mercator_Two_Point_Natural_Origin";
%constant SRS_PT_LABORDE_OBLIQUE_MERCATOR	= "Laborde_Oblique_Mercator";
%constant SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP = "Lambert_Conformal_Conic_1SP";
%constant SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP = "Lambert_Conformal_Conic_2SP";
%constant SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM = 
    "Lambert_Conformal_Conic_2SP_Belgium)";
%constant SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA = "Lambert_Azimuthal_Equal_Area";
%constant SRS_PT_MERCATOR_1SP		= "Mercator_1SP";
%constant SRS_PT_MERCATOR_2SP		= "Mercator_2SP";
%constant SRS_PT_MILLER_CYLINDRICAL	= "Miller_Cylindrical";
%constant SRS_PT_MOLLWEIDE		= "Mollweide";
%constant SRS_PT_NEW_ZEALAND_MAP_GRID     = "New_Zealand_Map_Grid";
%constant SRS_PT_OBLIQUE_STEREOGRAPHIC    = "Oblique_Stereographic";
%constant SRS_PT_ORTHOGRAPHIC		= "Orthographic";
%constant SRS_PT_POLAR_STEREOGRAPHIC      = "Polar_Stereographic";
%constant SRS_PT_POLYCONIC		= "Polyconic";
%constant SRS_PT_ROBINSON			= "Robinson";
%constant SRS_PT_SINUSOIDAL		= "Sinusoidal";
%constant SRS_PT_STEREOGRAPHIC		= "Stereographic";
%constant SRS_PT_SWISS_OBLIQUE_CYLINDRICAL= "Swiss_Oblique_Cylindrical";
%constant SRS_PT_TRANSVERSE_MERCATOR      = "Transverse_Mercator";
%constant SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED = "Transverse_Mercator_South_Orientated";
%constant SRS_PT_TRANSVERSE_MERCATOR_MI_22= "Transverse_Mercator_MapInfo_22";
%constant SRS_PT_TRANSVERSE_MERCATOR_MI_23= "Transverse_Mercator_MapInfo_23";
%constant SRS_PT_TRANSVERSE_MERCATOR_MI_24= "Transverse_Mercator_MapInfo_24";
%constant SRS_PT_TRANSVERSE_MERCATOR_MI_25= "Transverse_Mercator_MapInfo_25";
%constant SRS_PT_TUNISIA_MINING_GRID	= "Tunisia_Mining_Grid";
%constant SRS_PT_VANDERGRINTEN		= "VanDerGrinten";
%constant SRS_PT_KROVAK			= "Krovak";

%constant SRS_PP_CENTRAL_MERIDIAN         = "central_meridian";
%constant SRS_PP_SCALE_FACTOR             = "scale_factor";
%constant SRS_PP_STANDARD_PARALLEL_1      = "standard_parallel_1";
%constant SRS_PP_STANDARD_PARALLEL_2      = "standard_parallel_2";
%constant SRS_PP_PSEUDO_STD_PARALLEL_1    = "pseudo_standard_parallel_1";
%constant SRS_PP_LONGITUDE_OF_CENTER      = "longitude_of_center";
%constant SRS_PP_LATITUDE_OF_CENTER       = "latitude_of_center";
%constant SRS_PP_LONGITUDE_OF_ORIGIN      = "longitude_of_origin";
%constant SRS_PP_LATITUDE_OF_ORIGIN       = "latitude_of_origin";
%constant SRS_PP_FALSE_EASTING            = "false_easting";
%constant SRS_PP_FALSE_NORTHING           = "false_northing";
%constant SRS_PP_AZIMUTH                  = "azimuth";
%constant SRS_PP_LONGITUDE_OF_POINT_1     = "longitude_of_point_1";
%constant SRS_PP_LATITUDE_OF_POINT_1      = "latitude_of_point_1";
%constant SRS_PP_LONGITUDE_OF_POINT_2     = "longitude_of_point_2";
%constant SRS_PP_LATITUDE_OF_POINT_2      = "latitude_of_point_2";
%constant SRS_PP_LONGITUDE_OF_POINT_3     = "longitude_of_point_3";
%constant SRS_PP_LATITUDE_OF_POINT_3      = "latitude_of_point_3";
%constant SRS_PP_RECTIFIED_GRID_ANGLE     = "rectified_grid_angle";
%constant SRS_PP_LANDSAT_NUMBER           = "landsat_number";
%constant SRS_PP_PATH_NUMBER              = "path_number";
%constant SRS_PP_PERSPECTIVE_POINT_HEIGHT = "perspective_point_height";
%constant SRS_PP_FIPSZONE                 = "fipszone";
%constant SRS_PP_ZONE                     = "zone";

%constant SRS_UL_METER			= "Meter";
%constant SRS_UL_FOOT			= "Foot (International)";
%constant SRS_UL_FOOT_CONV                = "0.3048";
%constant SRS_UL_US_FOOT			= "U.S. Foot";
%constant SRS_UL_US_FOOT_CONV             = "0.3048006";
%constant SRS_UL_NAUTICAL_MILE		= "Nautical Mile";
%constant SRS_UL_NAUTICAL_MILE_CONV       = "1852.0";
%constant SRS_UL_LINK			= "Link";
%constant SRS_UL_LINK_CONV                = "0.20116684023368047";
%constant SRS_UL_CHAIN			= "Chain";
%constant SRS_UL_CHAIN_CONV               = "2.0116684023368047";
%constant SRS_UL_ROD			= "Rod";
%constant SRS_UL_ROD_CONV                 = "5.02921005842012";

%constant SRS_DN_NAD27			= "North_American_Datum_1927";
%constant SRS_DN_NAD83			= "North_American_Datum_1983";
%constant SRS_DN_WGS72			= "WGS_1972";
%constant SRS_DN_WGS84			= "WGS_1984";

%constant SRS_WGS84_SEMIMAJOR             = 6378137.0;
%constant SRS_WGS84_INVFLATTENING         = 298.257223563;

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
