/******************************************************************************
 * $Id$
 *
 * Project:  GDAL SWIG Interfaces.
 * Purpose:  OGRSpatialReference related declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *
 ******************************************************************************
 * Copyright (c) 2005, Kevin Ruland
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
 *****************************************************************************/

#ifdef PERL_CPAN_NAMESPACE
%module "Geo::OSR"
#elif defined(SWIGCSHARP)
%module Osr
#else
%module osr
#endif

#ifdef SWIGCSHARP
%include swig_csharp_extensions.i
%implement_class(SWIGTYPE)
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
  OGRErr rcode = OSRSetWellKnownGeogCS( srs, name );
  if( rcode == OGRERR_NONE )
      rcode = OSRExportToWkt ( srs, argout );  
  OSRDestroySpatialReference( srs );
  return rcode;
}
%}

/************************************************************************/
/*                           GetUserInputAsWKT()                        */
/************************************************************************/
%inline %{
OGRErr GetUserInputAsWKT( const char *name, char **argout ) {
  OGRSpatialReferenceH srs = OSRNewSpatialReference("");
  OGRErr rcode = OSRSetFromUserInput( srs, name );
  if( rcode == OGRERR_NONE )
      rcode = OSRExportToWkt ( srs, argout );  
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

#if !defined(SWIGPYTHON)
%rename (GetProjectionMethods) OPTGetProjectionMethods;
#if defined(SWIGPERL)
%apply (char **CSL) {(char **)};
#endif
char **OPTGetProjectionMethods();
#if defined(SWIGPERL)
%clear (char **);
#endif

%rename (GetProjectionMethodParameterList) OPTGetParameterList;
#if defined(SWIGPERL)
%apply (char **free) {(char **)};
%apply (char **argout) {(char **username)};
#endif
char **OPTGetParameterList( char *method, char **username );
#if defined(SWIGPERL)
%clear (char **);
%clear (char **username);
#endif

%rename (GetProjectionMethodParamInfo) OPTGetParameterInfo;
#if defined(SWIGPERL)
%apply (char **argout) {(char **usrname)};
%apply (char **argout) {(char **type)};
%apply (double *OUTPUT){(double *defaultval)};
#endif
void OPTGetParameterInfo( char *method, char *param, char **usrname,
                          char **type, double *defaultval );
#if defined(SWIGPERL)
%clear (double *defaultval);
%clear (char **type);
%clear (char **usrname);
#endif
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

  OGRErr SetAuthority( const char * pszTargetKey,
                       const char * pszAuthority,
                       int nCode ) {
    return OSRSetAuthority( self, pszTargetKey, pszAuthority, nCode );
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

  OGRErr SetACEA( double dfStdP1, double dfStdP2,
 		double dfCenterLat, double dfCenterLong,
                double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetACEA( self, dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, 
                       dfFalseEasting, dfFalseNorthing );
  }    

  OGRErr SetAE( double dfCenterLat, double dfCenterLong,
              double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetAE( self, dfCenterLat, dfCenterLong, 
                     dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetBonne( double dfStandardParallel, double dfCentralMeridian,
                 double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetBonne( self, dfStandardParallel, dfCentralMeridian, 
                        dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetCEA( double dfStdP1, double dfCentralMeridian,
               double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetCEA( self, dfStdP1, dfCentralMeridian, 
                      dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetCS( double dfCenterLat, double dfCenterLong,
              double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetCS( self, dfCenterLat, dfCenterLong, 
                     dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetEC( double dfStdP1, double dfStdP2,
              double dfCenterLat, double dfCenterLong,
              double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetEC( self, dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, 
                     dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetEckertIV( double dfCentralMeridian,
                    double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetEckertIV( self, dfCentralMeridian, dfFalseEasting, dfFalseNorthing);
  }

  OGRErr SetEckertVI( double dfCentralMeridian,
                    double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetEckertVI( self, dfCentralMeridian, dfFalseEasting, dfFalseNorthing);
  }

  OGRErr SetEquirectangular( double dfCenterLat, double dfCenterLong,
                           double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetEquirectangular( self, dfCenterLat, dfCenterLong, 
                                  dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetGS( double dfCentralMeridian,
              double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetGS( self, dfCentralMeridian, dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetGH( double dfCentralMeridian,
              double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetGH( self, dfCentralMeridian, dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetGEOS( double dfCentralMeridian, double dfSatelliteHeight,
                double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetGEOS( self, dfCentralMeridian, dfSatelliteHeight,
                       dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetGnomonic( double dfCenterLat, double dfCenterLong,
                    double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetGnomonic( self, dfCenterLat, dfCenterLong, 
                           dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetHOM( double dfCenterLat, double dfCenterLong,
               double dfAzimuth, double dfRectToSkew,
               double dfScale,
               double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetHOM( self, dfCenterLat, dfCenterLong, dfAzimuth, dfRectToSkew,
                      dfScale, dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetHOM2PNO( double dfCenterLat,
                   double dfLat1, double dfLong1,
                   double dfLat2, double dfLong2,
                   double dfScale,
                   double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetHOM2PNO( self, dfCenterLat, dfLat1, dfLong1, dfLat2, dfLong2, 
                          dfScale, dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetKrovak( double dfCenterLat, double dfCenterLong,
                  double dfAzimuth, double dfPseudoStdParallelLat,
                  double dfScale, 
                  double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetKrovak( self, dfCenterLat, dfCenterLong, 
                         dfAzimuth, dfPseudoStdParallelLat, 
                         dfScale, dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetLAEA( double dfCenterLat, double dfCenterLong,
                double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetLAEA( self, dfCenterLat, dfCenterLong, 
                       dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetLCC( double dfStdP1, double dfStdP2,
               double dfCenterLat, double dfCenterLong,
               double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetLCC( self, dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, 
                      dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetLCC1SP( double dfCenterLat, double dfCenterLong,
                  double dfScale,
                  double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetLCC1SP( self, dfCenterLat, dfCenterLong, dfScale, 
                         dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetLCCB( double dfStdP1, double dfStdP2,
                double dfCenterLat, double dfCenterLong,
                double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetLCCB( self, dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, 
                       dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetMC( double dfCenterLat, double dfCenterLong,
              double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetMC( self, dfCenterLat, dfCenterLong,    
                     dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetMercator( double dfCenterLat, double dfCenterLong,
                    double dfScale, 
                    double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetMercator( self, dfCenterLat, dfCenterLong, 
                           dfScale, dfFalseEasting, dfFalseNorthing );
  }

  OGRErr  SetMollweide( double dfCentralMeridian,
                      double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetMollweide( self, dfCentralMeridian, 
                            dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetNZMG( double dfCenterLat, double dfCenterLong,
                double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetNZMG( self, dfCenterLat, dfCenterLong, 
                       dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetOS( double dfOriginLat, double dfCMeridian,
              double dfScale,
              double dfFalseEasting,double dfFalseNorthing) {
    return OSRSetOS( self, dfOriginLat, dfCMeridian, dfScale, 
                     dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetOrthographic( double dfCenterLat, double dfCenterLong,
                        double dfFalseEasting,double dfFalseNorthing) {
    return OSRSetOrthographic( self, dfCenterLat, dfCenterLong, 
                               dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetPolyconic( double dfCenterLat, double dfCenterLong,
                     double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetPolyconic( self, dfCenterLat, dfCenterLong, 
                            dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetPS( double dfCenterLat, double dfCenterLong,
              double dfScale,
              double dfFalseEasting, double dfFalseNorthing) {
    return OSRSetPS( self, dfCenterLat, dfCenterLong, dfScale,
                     dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetRobinson( double dfCenterLong, 
                    double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetRobinson( self, dfCenterLong, dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetSinusoidal( double dfCenterLong, 
                      double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetSinusoidal( self, dfCenterLong, dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetStereographic( double dfCenterLat, double dfCenterLong,
                         double dfScale,
                         double dfFalseEasting,double dfFalseNorthing) {
    return OSRSetStereographic( self, dfCenterLat, dfCenterLong, dfScale, 
                                dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetSOC( double dfLatitudeOfOrigin, double dfCentralMeridian,
               double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetSOC( self, dfLatitudeOfOrigin, dfCentralMeridian,
	              dfFalseEasting, dfFalseNorthing );
  }
    
  OGRErr SetTM( double dfCenterLat, double dfCenterLong,
              double dfScale,
              double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetTM( self, dfCenterLat, dfCenterLong, dfScale, 
                     dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetTMVariant( const char *pszVariantName,
                     double dfCenterLat, double dfCenterLong,
                     double dfScale,
                     double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetTMVariant( self, pszVariantName, dfCenterLat, dfCenterLong,  
                            dfScale, dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetTMG( double dfCenterLat, double dfCenterLong, 
               double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetTMG( self, dfCenterLat, dfCenterLong, 
                      dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetTMSO( double dfCenterLat, double dfCenterLong,
                double dfScale,
                double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetTMSO( self, dfCenterLat, dfCenterLong, dfScale, 
                       dfFalseEasting, dfFalseNorthing );
  }

  OGRErr SetVDG( double dfCenterLong,
               double dfFalseEasting, double dfFalseNorthing ) {
    return OSRSetVDG( self, dfCenterLong, dfFalseEasting, dfFalseNorthing );
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

%apply (char **options) { (char **) };
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
