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
 * Revision 1.10  2005/02/24 16:45:17  kruland
 * Commented missing methods.
 * Added first cut at a proxy for ProjectionMethods.
 *
 * Revision 1.9  2005/02/23 17:44:37  kruland
 * Change SpatialReference constructor to have keyword argument "wkt".
 *
 * Revision 1.8  2005/02/20 19:46:04  kruland
 * Use the new fixed size typemap for (double **) arguments in ExportToPCI and
 * ExportToUSGS.
 *
 * Revision 1.7  2005/02/20 19:42:53  kruland
 * Rename the Swig shadow classes so the names do not give the impression that
 * they are any part of the GDAL/OSR apis.  There were no bugs with the old
 * names but they were confusing.
 *
 * Revision 1.6  2005/02/18 18:42:07  kruland
 * Added %feature("autodoc");
 *
 * Revision 1.5  2005/02/18 18:13:05  kruland
 * Now using the THROW_OGR_ERROR tyepmap for all methods.  Changed those
 * which should return OGRErr to return OGRErr since the typemap is friendly
 * to return arguments.
 *
 * Revision 1.4  2005/02/18 16:18:27  kruland
 * Added %feature("compactdefaultargs") to fix problem with ImportFromPCI,
 * ImportFromUSGS and the fact that %typemap (char**ignorechange) has no
 * %typecheck.
 *
 * Fixed bug in new_CoordinateTransformation().  It wasn't returning a value.
 *
 * Revision 1.3  2005/02/17 21:12:06  kruland
 * More complete implementation of SpatialReference and CoordinateTransformation.
 * Satisfies the osr gdalautotests with 1 exception.
 *
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

#include "cpl_string.h"
#include "cpl_conv.h"

#include "ogr_srs_api.h"

typedef void OSRSpatialReferenceShadow;
typedef void OSRCoordinateTransformationShadow;

typedef double * double_3;
typedef double * double_7;
typedef double * double_15;
typedef double * double_17;

%}

%feature("compactdefaultargs");
%feature("autodoc");

%import gdal_typemaps.i

typedef int OGRErr;

%apply (THROW_OGR_ERROR) { OGRErr };

/******************************************************************************
 *
 *  Global methods
 *
 */

// NEEDED GLOBALS 
// GetWellKnowGeogCSAsWKT 

/******************************************************************************
 *
 *  Projection Methods pseudo object.
 *
 */

/*
 * The previous gdal.i interface defined a list with the following structure:
 *
 * ProjectionMethod = ( MethodName (string),
                        UserMethodName (string),
                        ParameterList [
                          ( ParameterName (string),
                            UserParamName (string),
                            Type (string),
                            Default (double ) )
                          ....
                         ]
                       )
  *
  * MethodName is returned by OPTGetProjectionMethods().
  *
  * papszparameters = OPTGetParameterList( MethodName, &UserMethodName ).
  *
  * ParameterName is an entry in papszparameters (char **)
  * 
  * OPTGetParameterInfo ( MethodName, ParameterName, &UserParamName, &Type, &Default )
  */

%inline %{
struct ProjectionMethod {
  char *MethodName;
  char *UserMethodName;
  char **papszParameters;

  ProjectionMethod( char const *MethodName ) {
    this->MethodName = CPLStrdup( MethodName );
    this->papszParameters = OPTGetParameterList( MethodName, &this->UserMethodName );
  }

  ~ProjectionMethod() {
    CPLFree( this->MethodName );
    CPLFree( this->UserMethodName );
    CSLDestroy( this->papszParameters );
  }
}; /* struct ProjectionMethod */
%} /* %inline */

%extend ProjectionMethod {
%immutable;
  char *MethodName;
  char *UserMethodName;
%pythoncode {
  def __str__(self):
    return "%s(%s)" % (self.MethodName, self.UserMethodName)
}  /* %pythoncode */
}  /* %extend */

%rename (GetProjectionMethods) OPTGetProjectionMethods;
%apply (char **options) { (char **) };
char ** OPTGetProjectionMethods();
%clear (char **);

/******************************************************************************
 *
 *  Spatial Reference Object.
 *
 */

%rename (SpatialReference) OSRSpatialReferenceShadow;
class OSRSpatialReferenceShadow {
private:
public:
%extend {

// NEEDED
// Reference
// Dereference
// SetAuthority
// SetGH
// SetGnomonic
// SetHOM
//SetHOM2PNO
// SetKrovak
// SetLAEA
// SetLCC
// SetLCCB
// SetLCC1SP
// SetMC
// SetMercator
// SetMollweide
// SetNZMG
// SetOS
// SetOrthographic
// SetPolyconic
// SetPS
// SetRobinson
// SetSinusoidal
// SetStereographic
// SetSOC
// SetTM
// SetTMSO
// SetTMG
// SetVDG


  %feature("kwargs") OSRSpatialReferenceShadow;
  OSRSpatialReferenceShadow( char const * wkt = "" ) {
    OSRSpatialReferenceShadow *sr = (OSRSpatialReferenceShadow*) OSRNewSpatialReference(wkt);
    if (sr) {
      OSRReference( sr );
    }
    return sr;
  }

  ~OSRSpatialReferenceShadow() {
    if (OSRDereference( self ) == 0 ) {
      OSRDestroySpatialReference( self );
    }
  }

/* NEEDED */
// Reference ?  I don't think this are needed in script-land
// Dereference ?  I don't think this are needed in script-land

%newobject __str__;
  char *__str__() {
    char *buf = 0;
    OSRExportToPrettyWkt( self, &buf, 0 );
    return buf;
  }

  int IsSame( OSRSpatialReferenceShadow *rhs ) {
    return OSRIsSame( self, rhs );
  }

  int IsSameGeogCS( OSRSpatialReferenceShadow *rhs ) {
    return OSRIsSameGeogCS( self, rhs );
  }

  int IsGeographic() {
    return OSRIsGeographic(self);
  }

  int IsProjected() {
    return OSRIsProjected(self);
  }

  const char *GetAttrValue( const char *name, int child = 0 ) {
    return OSRGetAttrValue( self, name, child );
  }

/*
  const char *__getattr__( const char *name ) {
    return OSRGetAttrValue( self, name, 0 );
  }
*/

  OGRErr SetAttrValue( const char *name, const char *value ) {
    return OSRSetAttrValue( self, name, value ); 
  }

/*
  OGRErr __setattr__( const char *name, const char *value ) {
    return OSRSetAttrValue( self, name, value );
  }
*/

  OGRErr SetAngularUnits( const char*name, double to_radians ) {
    return OSRSetAngularUnits( self, name, to_radians );
  }

  double GetAngularUnits() {
    // Return code ignored.
    return OSRGetAngularUnits( self, 0 );
  }

  OGRErr SetLinearUnits( const char*name, double to_meters ) {
    return OSRSetAngularUnits( self, name, to_meters );
  }

  double GetLinearUnits() {
    // Return code ignored.
    return OSRGetLinearUnits( self, 0 );
  }

  const char *GetLinearUnitsName() {
    const char *name = 0;
    if ( OSRIsProjected( self ) ) {
      name = OSRGetAttrValue( self, "PROJCS|UNIT", 0 );
    }
    else if ( OSRIsLocal( self ) ) {
      name = OSRGetAttrValue( self, "LOCAL_CS|UNIT", 0 );
    }

    if (name != 0) 
      return name;

    return "Meter";
  }

  const char *GetAuthorityCode( const char *target_key ) {
    return OSRGetAuthorityCode( self, target_key );
  }

  const char *GetAuthorityName( const char *target_key ) {
    return OSRGetAuthorityName( self, target_key );
  }

  OGRErr SetUTM( int zone, int north =1 ) {
    return OSRSetUTM( self, zone, north );
  }

  OGRErr SetStatePlane( int zone, int is_nad83 = 1, char const *unitsname = "", double units = 0.0 ) {
    return OSRSetStatePlaneWithUnits( self, zone, is_nad83, unitsname, units );
  }

  OGRErr AutoIdentifyEPSG() {
    return OSRAutoIdentifyEPSG( self );
  }

  OGRErr SetProjection( char const *arg ) {
    return OSRSetProjection( self, arg );
  }

  OGRErr SetProjParm( const char *name, double val ) {
    return OSRSetProjParm( self, name, val ); 
  }

  double GetProjParm( const char *name, double default_val = 0.0 ) {
    // Return code ignored.
    return OSRGetProjParm( self, name, default_val, 0 );
  }

  OGRErr SetNormProjParm( const char *name, double val ) {
    return OSRSetNormProjParm( self, name, val );
  }

  double GetNormProjParm( const char *name, double default_val = 0.0 ) {
    // Return code ignored.
    return OSRGetNormProjParm( self, name, default_val, 0 );
  }

  OGRErr SetACEA( double stdp1, double stdp2, double clat, double clong, double fe, double fn ) {
    return OSRSetACEA( self, stdp1, stdp2, clat, clong, fe, fn );
  }

  OGRErr SetAE( double clat, double clon, double fe, double fn ) {
    return OSRSetAE( self, clat, clon, fe, fn );
  }

  OGRErr SetCS( double clat, double clong, double fe, double fn ) {
    return OSRSetCS( self, clat, clong, fe, fn );
  }

  OGRErr SetBonne( double clat, double clong, double fe, double fn ) {
    return OSRSetBonne( self, clat, clong, fe, fn );
  }

  OGRErr SetEC( double stdp1, double stdp2, double clat, double clong, double fe, double fn ) {
    return OSRSetEC( self, stdp1, stdp2, clat, clong, fe, fn );
  }

  OGRErr SetEckertIV( double cm, double fe, double fn ) {
    return OSRSetEckertIV( self, cm, fe, fn );
  }

  OGRErr SetEckertVI( double cm, double fe, double fn ) {
    return OSRSetEckertVI( self, cm, fe, fn );
  }

  OGRErr SetEquirectangular( double clat, double clong, double fe, double fn ) {
    return OSRSetEquirectangular( self, clat, clong, fe, fn );
  }

%feature( "kwargs" ) SetGS;
  OGRErr SetGS( double cm, double fe, double fn ) {
    return OSRSetGS( self, cm, fe, fn );
  }

  OGRErr SetWellKnownGeogCS( const char *name ) {
    return OSRSetWellKnownGeogCS( self, name );
  }

  OGRErr SetFromUserInput( const char *name ) {
    return OSRSetFromUserInput( self, name );
  }

  OGRErr CopyGeogCSFrom( OSRSpatialReferenceShadow *rhs ) {
    return OSRCopyGeogCSFrom( self, rhs );
  }

  OGRErr SetTOWGS84( double p1, double p2, double p3,
                     double p4 = 0.0, double p5 = 0.0,
                     double p6 = 0.0, double p7 = 0.0 ) {
    return OSRSetTOWGS84( self, p1, p2, p3, p4, p5, p6, p7 );
  }

  OGRErr GetTOWGS84( double_7 argout ) {
    return OSRGetTOWGS84( self, argout, 7 );
  }

  OGRErr SetGeogCS( const char * pszGeogName,
                    const char * pszDatumName,
                    const char * pszEllipsoidName,
                    double dfSemiMajor, double dfInvFlattening,
                    const char * pszPMName = "Greenwich",
                    double dfPMOffset = 0.0,
                    const char * pszUnits = "degree",
                    double dfConvertToRadians =  0.0174532925199433 ) {
    return OSRSetGeogCS( self, pszGeogName, pszDatumName, pszEllipsoidName,
                         dfSemiMajor, dfInvFlattening,
                         pszPMName, dfPMOffset, pszUnits, dfConvertToRadians );
  }

  OGRErr SetProjCS( const char *name = "unnamed" ) {
    return OSRSetProjCS( self, name );
  }

%apply (char **ignorechange) { (char **) };
  OGRErr ImportFromWkt( char **ppszInput ) {
    return OSRImportFromWkt( self, ppszInput );
  }
%clear (char **);

  OGRErr ImportFromProj4( char *ppszInput ) {
    return OSRImportFromProj4( self, ppszInput );
  }

%apply (char **ignorechange) { (char **) };
  OGRErr ImportFromESRI( char **ppszInput ) {
    return OSRImportFromESRI( self, ppszInput );
  }
%clear (char **);

  OGRErr ImportFromEPSG( int arg ) {
    return OSRImportFromEPSG(self, arg);
  }

  OGRErr ImportFromPCI( char const *proj, char const *units = "METRE",
                        double_17 argin = 0 ) {
    return OSRImportFromPCI( self, proj, units, argin );
  }

  OGRErr ImportFromUSGS( long proj_code, long zone = 0,
                         double_15 argin = 0,
                         long datum_code = 0 ) {
    return OSRImportFromUSGS( self, proj_code, zone, argin, datum_code );
  }

  OGRErr ImportFromXML( char const *xmlString ) {
    return OSRImportFromXML( self, xmlString );
  }

  OGRErr ExportToWkt( char **argout ) {
    return OSRExportToWkt( self, argout );
  }

  OGRErr ExportToPrettyWkt( char **argout, int simplify = 0 ) {
    return OSRExportToPrettyWkt( self, argout, simplify );
  }

  OGRErr ExportToProj4( char **argout ) {
    return OSRExportToProj4( self, argout );
  }

%apply (char **argout) { (char **) };
%apply (double_17 *argout) { (double_17 *parms ) };
  OGRErr ExportToPCI( char **proj, char **units, double_17 *parms ) {
    return OSRExportToPCI( self, proj, units, parms );
  }
%clear (char **);
%clear (double_17 *parms);

%apply (long *OUTPUT) { (long*) };
%apply (double_15 *argout) { (double_15 *parms) }
  OGRErr ExportToUSGS( long *code, long *zone, double_15 *parms, long *datum ) {
    return OSRExportToUSGS( self, code, zone, parms, datum );
  }
%clear (long*);
%clear (double_15 *parms);

  OGRErr ExportToXML( char **argout, const char *dialect = "" ) {
    return OSRExportToXML( self, argout, dialect );
  }

%newobject CloneGeogCS;
  OSRSpatialReferenceShadow *CloneGeogCS() {
    return (OSRSpatialReferenceShadow*) OSRCloneGeogCS(self);
  }

/*
 * Commented out until the headers have changed to make OSRClone visible.
%newobject Clone;
  OSRSpatialReferenceShadow *Clone() {
    return (OSRSpatialReferenceShadow*) OSRClone(self);
  }
*/

  OGRErr Validate() {
    return OSRValidate(self);
  }

  OGRErr StripCTParms() {
    return OSRStripCTParms(self);
  }

  OGRErr FixupOrdering() {
    return OSRFixupOrdering(self);
  }

  OGRErr Fixup() {
    return OSRFixup(self);
  }

  OGRErr MorphToESRI() {
    return OSRMorphToESRI(self);
  }

  OGRErr MorphFromESRI() {
    return OSRMorphFromESRI(self);
  }


} /* %extend */
};


/******************************************************************************
 *
 *  CoordinateTransformation Object
 *
 */

// NEEDED
// Custom python __init__ which takes a tuple.
// TransformPoints which takes list of 3-tuples

%rename (CoordinateTransformation) OSRCoordinateTransformationShadow;
class OSRCoordinateTransformationShadow {
private:
public:
%extend {

  OSRCoordinateTransformationShadow( OSRSpatialReferenceShadow *src, OSRSpatialReferenceShadow *dst ) {
    OSRCoordinateTransformationShadow *obj = (OSRCoordinateTransformationShadow*) OCTNewCoordinateTransformation( src, dst );
    if (obj == 0 ) {
      throw "Failed to create coordinate transformation";
    }
    return obj;
  }

  ~OSRCoordinateTransformationShadow() {
    OCTDestroyCoordinateTransformation( self );
  }

// Need to apply argin typemap second so the numinputs=1 version gets applied
// instead of the numinputs=0 version from argout.
%apply (double_3 argout) {(double_3 inout)};
%apply (double_3 argin) {(double_3 inout)};
  void TransformPoint( double_3 inout ) {
    OCTTransform( self, 1, &inout[0], &inout[1], &inout[2] );
  }
%clear (double_3 inout);

  void TransformPoint( double_3 argout, double x, double y, double z = 0.0 ) {
    argout[0] = x;
    argout[1] = y;
    argout[2] = z;
    OCTTransform( self, 1, &argout[0], &argout[1], &argout[2] );
  }

} /*extend */
};
