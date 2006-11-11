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
 * Revision 1.29  2006/11/11 19:33:48  tamas
 * Controlling the owner of the objects returned by the static/non static members for the csharp binding
 *
 * Revision 1.28  2006/10/15 20:58:07  fwarmerdam
 * Added IsLocal() method.
 *
 * Revision 1.27  2006/06/27 13:14:49  ajolma
 * removed throw from OSRCoordinateTransformationShadow
 *
 * Revision 1.26  2006/06/27 12:48:44  ajolma
 * Geo::GDAL namespace had creeped in too early
 *
 * Revision 1.25  2006/06/20 07:33:17  ajolma
 * SetLinearUnits should call OSRSetLinearUnits
 *
 * Revision 1.24  2006/02/02 21:09:24  collinsb
 * Corrected wrong Java typemap filename
 *
 * Revision 1.23  2006/02/02 20:52:40  collinsb
 * Added SWIG JAVA bindings
 *
 * Revision 1.22  2006/01/14 01:47:22  cfis
 * Added private default constructors since SWIG 1.3.28 HEAD was incorrectly generating them.
 *
 * Revision 1.21  2005/09/06 01:43:06  kruland
 * Include gdal_typemaps.i if no other file is specified.
 *
 * Revision 1.20  2005/09/02 21:42:42  kruland
 * The compactdefaultargs feature should be turned on for all bindings not just
 * python.
 *
 * Revision 1.19  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 * Revision 1.18  2005/08/06 20:51:58  kruland
 * Instead of using double_## defines and SWIG macros, use typemaps with
 * [ANY] specified and use $dim0 to extract the dimension.  This makes the
 * code quite a bit more readable.
 *
 * Revision 1.17  2005/08/05 19:19:53  hobu
 * OPTGetParameterInfo uses default as an argument.
 * C# uses default as a keyword.
 * C# wins.  Rename the parameter name.
 *
 * Revision 1.16  2005/07/20 16:08:49  kruland
 * Declare the double and char * consts correctly.  And change all the decls
 * to defer the the #defined values from the headers.
 *
 * Revision 1.15  2005/07/20 14:44:13  kruland
 * Use correct name for OPTGetProjectionMethods().
 *
 * Revision 1.14  2005/06/22 18:42:33  kruland
 * Don't apply a typemap for returning OGRErr.
 *
 * Revision 1.13  2005/02/24 20:33:58  kruland
 * Back to import gdal_typemaps.i to prevent confusion.
 *
 * Revision 1.12  2005/02/24 18:39:14  kruland
 * Using the GetProjectionMethods() custom code from old interface for the
 * python binding.  Defined access to plain C-api for other bindings.
 * Added GetWellKnownGeogCSAsWKT() global method.
 *
 * Revision 1.11  2005/02/24 17:53:37  kruland
 * import the generic typemap.i file.
 *
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

#ifdef SWIGPERL_USE_THIS_LATER
%module "Geo::GDAL::osr"
#else
%module osr
#endif

%feature("compactdefaultargs");

%constant char *SRS_PT_ALBERS_CONIC_EQUAL_AREA	= SRS_PT_ALBERS_CONIC_EQUAL_AREA;
%constant char *SRS_PT_AZIMUTHAL_EQUIDISTANT	= SRS_PT_AZIMUTHAL_EQUIDISTANT;
%constant char *SRS_PT_CASSINI_SOLDNER		= SRS_PT_CASSINI_SOLDNER;
%constant char *SRS_PT_CYLINDRICAL_EQUAL_AREA	= SRS_PT_CYLINDRICAL_EQUAL_AREA;
%constant char *SRS_PT_ECKERT_IV		= SRS_PT_ECKERT_IV;
%constant char *SRS_PT_ECKERT_VI		= SRS_PT_ECKERT_VI;
%constant char *SRS_PT_EQUIDISTANT_CONIC	= SRS_PT_EQUIDISTANT_CONIC;
%constant char *SRS_PT_EQUIRECTANGULAR		= SRS_PT_EQUIRECTANGULAR;
%constant char *SRS_PT_GALL_STEREOGRAPHIC	= SRS_PT_GALL_STEREOGRAPHIC;
%constant char *SRS_PT_GNOMONIC			= SRS_PT_GNOMONIC;
%constant char *SRS_PT_GOODE_HOMOLOSINE         = SRS_PT_GOODE_HOMOLOSINE;
%constant char *SRS_PT_HOTINE_OBLIQUE_MERCATOR	= SRS_PT_HOTINE_OBLIQUE_MERCATOR;
%constant char *SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN = SRS_PT_HOTINE_OBLIQUE_MERCATOR_TWO_POINT_NATURAL_ORIGIN;
%constant char *SRS_PT_LABORDE_OBLIQUE_MERCATOR	= SRS_PT_LABORDE_OBLIQUE_MERCATOR;
%constant char *SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP = SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP;
%constant char *SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP = SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP;
%constant char *SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM = SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM;
%constant char *SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA = SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA;
%constant char *SRS_PT_MERCATOR_1SP		= SRS_PT_MERCATOR_1SP;
%constant char *SRS_PT_MERCATOR_2SP		= SRS_PT_MERCATOR_2SP;
%constant char *SRS_PT_MILLER_CYLINDRICAL	= SRS_PT_MILLER_CYLINDRICAL;
%constant char *SRS_PT_MOLLWEIDE		= SRS_PT_MOLLWEIDE;
%constant char *SRS_PT_NEW_ZEALAND_MAP_GRID     = SRS_PT_NEW_ZEALAND_MAP_GRID;
%constant char *SRS_PT_OBLIQUE_STEREOGRAPHIC    = SRS_PT_OBLIQUE_STEREOGRAPHIC;
%constant char *SRS_PT_ORTHOGRAPHIC		= SRS_PT_ORTHOGRAPHIC;
%constant char *SRS_PT_POLAR_STEREOGRAPHIC      = SRS_PT_POLAR_STEREOGRAPHIC;
%constant char *SRS_PT_POLYCONIC		= SRS_PT_POLYCONIC;
%constant char *SRS_PT_ROBINSON			= SRS_PT_ROBINSON;
%constant char *SRS_PT_SINUSOIDAL		= SRS_PT_SINUSOIDAL;
%constant char *SRS_PT_STEREOGRAPHIC		= SRS_PT_STEREOGRAPHIC;
%constant char *SRS_PT_SWISS_OBLIQUE_CYLINDRICAL= SRS_PT_SWISS_OBLIQUE_CYLINDRICAL;
%constant char *SRS_PT_TRANSVERSE_MERCATOR      = SRS_PT_TRANSVERSE_MERCATOR;
%constant char *SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED = SRS_PT_TRANSVERSE_MERCATOR_SOUTH_ORIENTED;
%constant char *SRS_PT_TRANSVERSE_MERCATOR_MI_22= SRS_PT_TRANSVERSE_MERCATOR_MI_22;
%constant char *SRS_PT_TRANSVERSE_MERCATOR_MI_23= SRS_PT_TRANSVERSE_MERCATOR_MI_23;
%constant char *SRS_PT_TRANSVERSE_MERCATOR_MI_24= SRS_PT_TRANSVERSE_MERCATOR_MI_24;
%constant char *SRS_PT_TRANSVERSE_MERCATOR_MI_25= SRS_PT_TRANSVERSE_MERCATOR_MI_25;
%constant char *SRS_PT_TUNISIA_MINING_GRID	= SRS_PT_TUNISIA_MINING_GRID;
%constant char *SRS_PT_VANDERGRINTEN		= SRS_PT_VANDERGRINTEN;
%constant char *SRS_PT_KROVAK			= SRS_PT_KROVAK;

%constant char *SRS_PP_CENTRAL_MERIDIAN         = SRS_PP_CENTRAL_MERIDIAN;
%constant char *SRS_PP_SCALE_FACTOR             = SRS_PP_SCALE_FACTOR;
%constant char *SRS_PP_STANDARD_PARALLEL_1      = SRS_PP_STANDARD_PARALLEL_1;
%constant char *SRS_PP_STANDARD_PARALLEL_2      = SRS_PP_STANDARD_PARALLEL_2;
%constant char *SRS_PP_PSEUDO_STD_PARALLEL_1    = SRS_PP_PSEUDO_STD_PARALLEL_1;
%constant char *SRS_PP_LONGITUDE_OF_CENTER      = SRS_PP_LONGITUDE_OF_CENTER;
%constant char *SRS_PP_LATITUDE_OF_CENTER       = SRS_PP_LATITUDE_OF_CENTER;
%constant char *SRS_PP_LONGITUDE_OF_ORIGIN      = SRS_PP_LONGITUDE_OF_ORIGIN;
%constant char *SRS_PP_LATITUDE_OF_ORIGIN       = SRS_PP_LATITUDE_OF_ORIGIN;
%constant char *SRS_PP_FALSE_EASTING            = SRS_PP_FALSE_EASTING;
%constant char *SRS_PP_FALSE_NORTHING           = SRS_PP_FALSE_NORTHING;
%constant char *SRS_PP_AZIMUTH                  = SRS_PP_AZIMUTH;
%constant char *SRS_PP_LONGITUDE_OF_POINT_1     = SRS_PP_LONGITUDE_OF_POINT_1;
%constant char *SRS_PP_LATITUDE_OF_POINT_1      = SRS_PP_LATITUDE_OF_POINT_1;
%constant char *SRS_PP_LONGITUDE_OF_POINT_2     = SRS_PP_LONGITUDE_OF_POINT_2;
%constant char *SRS_PP_LATITUDE_OF_POINT_2      = SRS_PP_LATITUDE_OF_POINT_2;
%constant char *SRS_PP_LONGITUDE_OF_POINT_3     = SRS_PP_LONGITUDE_OF_POINT_3;
%constant char *SRS_PP_LATITUDE_OF_POINT_3      = SRS_PP_LATITUDE_OF_POINT_3;
%constant char *SRS_PP_RECTIFIED_GRID_ANGLE     = SRS_PP_RECTIFIED_GRID_ANGLE;
%constant char *SRS_PP_LANDSAT_NUMBER           = SRS_PP_LANDSAT_NUMBER;
%constant char *SRS_PP_PATH_NUMBER              = SRS_PP_PATH_NUMBER;
%constant char *SRS_PP_PERSPECTIVE_POINT_HEIGHT = SRS_PP_PERSPECTIVE_POINT_HEIGHT;
%constant char *SRS_PP_FIPSZONE                 = SRS_PP_FIPSZONE;
%constant char *SRS_PP_ZONE                     = SRS_PP_ZONE;

%constant char *SRS_UL_METER			= SRS_UL_METER;
%constant char *SRS_UL_FOOT			= SRS_UL_FOOT;
%constant char *SRS_UL_FOOT_CONV                = SRS_UL_FOOT_CONV;
%constant char *SRS_UL_US_FOOT			= SRS_UL_US_FOOT;
%constant char *SRS_UL_US_FOOT_CONV             = SRS_UL_US_FOOT_CONV;
%constant char *SRS_UL_NAUTICAL_MILE		= SRS_UL_NAUTICAL_MILE;
%constant char *SRS_UL_NAUTICAL_MILE_CONV       = SRS_UL_NAUTICAL_MILE_CONV;
%constant char *SRS_UL_LINK			= SRS_UL_LINK;
%constant char *SRS_UL_LINK_CONV                = SRS_UL_LINK_CONV;
%constant char *SRS_UL_CHAIN			= SRS_UL_CHAIN;
%constant char *SRS_UL_CHAIN_CONV               = SRS_UL_CHAIN_CONV;
%constant char *SRS_UL_ROD			= SRS_UL_ROD;
%constant char *SRS_UL_ROD_CONV                 = SRS_UL_ROD_CONV;

%constant char *SRS_DN_NAD27			= SRS_DN_NAD27;
%constant char *SRS_DN_NAD83			= SRS_DN_NAD83;
%constant char *SRS_DN_WGS72			= SRS_DN_WGS72;
%constant char *SRS_DN_WGS84			= SRS_DN_WGS84;

%constant double SRS_WGS84_SEMIMAJOR             = SRS_WGS84_SEMIMAJOR;
%constant double SRS_WGS84_INVFLATTENING         = SRS_WGS84_INVFLATTENING;

%{
#include <iostream>
using namespace std;

#include "cpl_string.h"
#include "cpl_conv.h"

#include "ogr_srs_api.h"

typedef void OSRSpatialReferenceShadow;
typedef void OSRCoordinateTransformationShadow;

%}

typedef int OGRErr;

#if defined(SWIGPYTHON)
%include osr_python.i
#elif defined(SWIGRUBY)
%include typemaps_ruby.i
#elif defined(SWIGPHP4)
%include typemaps_php.i
#elif defined(SWIGCSHARP)
%include typemaps_csharp.i
#elif defined(SWIGJAVA)
%include osr_java.i
#else
%include gdal_typemaps.i
#endif

/******************************************************************************
 *
 *  Global methods
 *
 */

/************************************************************************/
/*                        GetWellKnownGeogCSAsWKT()                     */
/************************************************************************/
%inline %{
OGRErr GetWellKnownGeogCSAsWKT( const char *name, char **argout ) {
  OGRSpatialReferenceH srs = OSRNewSpatialReference("");
  OSRSetWellKnownGeogCS( srs, name );
  OGRErr rcode = OSRExportToWkt ( srs, argout );  
  OSRDestroySpatialReference( srs );
  return rcode;
}
%}

/************************************************************************/
/*                        GetProjectionMethods()                        */
/************************************************************************/
/*
 * Python has it's own custom interface to GetProjectionMethods().which returns
 * fairly complex strucutre.
 *
 * All other languages will have a more simplistic interface which is
 * exactly the same as the C api.
 * 
 */
#if defined(SWIGCSHARP)
%static_owner
#endif 
#if !defined(SWIGPYTHON)
%rename (GetProjectionMethods) OPTGetProjectionMethods;
char **OPTGetProjectionMethods();

%rename (GetProjectionMethodParameterList) OPTGetParameterList;
char **OPTGetParameterList( char *method, char **username );

%rename (GetProjectionMethodParamInfo) OPTGetParameterInfo;
void OPTGetParameterInfo( char *method, char *param, char **usrname,
                          char **type, double *defaultval );
#endif
#if defined(SWIGCSHARP)
%object_owner
#endif

/******************************************************************************
 *
 *  Spatial Reference Object.
 *
 */

%rename (SpatialReference) OSRSpatialReferenceShadow;
class OSRSpatialReferenceShadow {
private:
  OSRSpatialReferenceShadow();
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

  int IsLocal() {
    return OSRIsLocal(self);
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
    return OSRSetLinearUnits( self, name, to_meters );
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

  OGRErr GetTOWGS84( double argout[7] ) {
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
                        double argin[17] = 0 ) {
    return OSRImportFromPCI( self, proj, units, argin );
  }

  OGRErr ImportFromUSGS( long proj_code, long zone = 0,
                         double argin[15] = 0,
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
%apply (double *argout[ANY]) { (double *parms[17] ) };
  OGRErr ExportToPCI( char **proj, char **units, double *parms[17] ) {
    return OSRExportToPCI( self, proj, units, parms );
  }
%clear (char **);
%clear (double *parms[17]);

%apply (long *OUTPUT) { (long*) };
%apply (double *argout[ANY]) { (double *parms[15]) }
  OGRErr ExportToUSGS( long *code, long *zone, double *parms[15], long *datum ) {
    return OSRExportToUSGS( self, code, zone, parms, datum );
  }
%clear (long*);
%clear (double *parms[15]);

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
  OSRCoordinateTransformationShadow();
public:
%extend {

  OSRCoordinateTransformationShadow( OSRSpatialReferenceShadow *src, OSRSpatialReferenceShadow *dst ) {
    OSRCoordinateTransformationShadow *obj = (OSRCoordinateTransformationShadow*) OCTNewCoordinateTransformation( src, dst );
    if (obj == 0 ) {
      CPLError(CE_Failure, 1, "Failed to create coordinate transformation");
      return NULL;
    }
    return obj;
  }

  ~OSRCoordinateTransformationShadow() {
    OCTDestroyCoordinateTransformation( self );
  }

// Need to apply argin typemap second so the numinputs=1 version gets applied
// instead of the numinputs=0 version from argout.
%apply (double argout[ANY]) {(double inout[3])};
%apply (double argin[ANY]) {(double inout[3])};
  void TransformPoint( double inout[3] ) {
    OCTTransform( self, 1, &inout[0], &inout[1], &inout[2] );
  }
%clear (double inout[3]);

  void TransformPoint( double argout[3], double x, double y, double z = 0.0 ) {
    argout[0] = x;
    argout[1] = y;
    argout[2] = z;
    OCTTransform( self, 1, &argout[0], &argout[1], &argout[2] );
  }

} /*extend */
};
