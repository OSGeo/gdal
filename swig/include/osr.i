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

#ifdef SWIGPYTHON
%nothread;
#endif

%include constraints.i

#if defined(SWIGCSHARP)
%module Osr
#elif defined(SWIGPYTHON)
%module (package="osgeo") osr
#else
%module osr
#endif

#ifdef SWIGCSHARP
%include swig_csharp_extensions.i
#endif

#ifndef SWIGJAVA
%feature("compactdefaultargs");
#endif

#ifdef SWIGCSHARP
%csconst(1);
#elif defined(SWIGJAVA)
%javaconst(1);
#endif

%include "../../ogr/ogr_srs_api.h"

#ifdef SWIGCSHARP
%csconst(0);
#elif defined(SWIGJAVA)
%javaconst(0);
#endif

#ifndef SWIGCSHARP
typedef int OGRAxisOrientation;
typedef int OSRAxisMappingStrategy;
#ifdef SWIGJAVA
%javaconst(1);
#endif
%constant OAO_Other=0;
%constant OAO_North=1;
%constant OAO_South=2;
%constant OAO_East=3;
%constant OAO_West=4;
%constant OAO_Up=5;
%constant OAO_Down=6;

%constant OAMS_TRADITIONAL_GIS_ORDER=0;
%constant OAMS_AUTHORITY_COMPLIANT=1;
%constant OAMS_CUSTOM=2;

#ifdef SWIGJAVA
%javaconst(0);
#endif
#else
%rename (AxisOrientation) OGRAxisOrientation;
typedef enum OGRAxisOrientation
{
    OAO_Other=0,
    OAO_North=1,
    OAO_South=2,
    OAO_East=3,
    OAO_West=4,
    OAO_Up=5,
    OAO_Down=6
};

%rename (AxisMappingStrategy) OSRAxisMappingStrategy;
typedef enum
{
    OAMS_TRADITIONAL_GIS_ORDER,
    OAMS_AUTHORITY_COMPLIANT,
    OAMS_CUSTOM
} OSRAxisMappingStrategy;

#endif

#ifndef SWIGCSHARP
#ifdef SWIGJAVA
%javaconst(1);
#endif
%constant PROJ_ERR_INVALID_OP                           =1024;
%constant PROJ_ERR_INVALID_OP_WRONG_SYNTAX              =1025;
%constant PROJ_ERR_INVALID_OP_MISSING_ARG               =1026;
%constant PROJ_ERR_INVALID_OP_ILLEGAL_ARG_VALUE         =1027;
%constant PROJ_ERR_INVALID_OP_MUTUALLY_EXCLUSIVE_ARGS   =1028;
%constant PROJ_ERR_INVALID_OP_FILE_NOT_FOUND_OR_INVALID =1029;
%constant PROJ_ERR_COORD_TRANSFM                           =2048;
%constant PROJ_ERR_COORD_TRANSFM_INVALID_COORD             =2049;
%constant PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN =2050;
%constant PROJ_ERR_COORD_TRANSFM_NO_OPERATION              =2051;
%constant PROJ_ERR_COORD_TRANSFM_OUTSIDE_GRID              =2052;
%constant PROJ_ERR_COORD_TRANSFM_GRID_AT_NODATA            =2053;
%constant PROJ_ERR_OTHER                                   =4096;
%constant PROJ_ERR_OTHER_API_MISUSE                        =4097;
%constant PROJ_ERR_OTHER_NO_INVERSE_OP                     =4098;
%constant PROJ_ERR_OTHER_NETWORK_ERROR                     =4099;
#ifdef SWIGJAVA
%javaconst(0);
#endif
#endif

#if !defined(FROM_GDAL_I) && !defined(FROM_OGR_I)
%inline %{
typedef char retStringAndCPLFree;
%}
#endif

%{
#include <iostream>
using namespace std;

#define CPL_SUPRESS_CPLUSPLUS

#include "cpl_string.h"
#include "cpl_conv.h"

#include "ogr_srs_api.h"

#ifdef DEBUG
typedef struct OGRSpatialReferenceHS OSRSpatialReferenceShadow;
typedef struct OGRCoordinateTransformationHS OSRCoordinateTransformationShadow;
typedef struct OGRCoordinateTransformationHS OGRCoordinateTransformationShadow;
#else
typedef void OSRSpatialReferenceShadow;
typedef void OSRCoordinateTransformationShadow;
#endif
%}

typedef int OGRErr;

#if defined(SWIGPYTHON)
%include osr_python.i
#elif defined(SWIGCSHARP)
%include osr_csharp.i
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
%apply Pointer NONNULL {const char* name};
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

#ifdef SWIGJAVA
%{
typedef int* retIntArray;
%}
#endif


/************************************************************************/
/*                             AreaOfUse()                              */
/************************************************************************/

%{
typedef struct
{
  double west_lon_degree;
  double south_lat_degree;
  double east_lon_degree;
  double north_lat_degree;
  char* name;
} OSRAreaOfUse;
%}

%rename (AreaOfUse) OSRAreaOfUse;

struct OSRAreaOfUse {
%extend {
%immutable;
  double west_lon_degree;
  double south_lat_degree;
  double east_lon_degree;
  double north_lat_degree;
  char* name;

public:
  OSRAreaOfUse( double west_lon_degree,
             double south_lat_degree,
             double east_lon_degree,
             double north_lat_degree,
             char* name )
  {
    OSRAreaOfUse *self = (OSRAreaOfUse*) CPLMalloc( sizeof( OSRAreaOfUse ) );
    self->west_lon_degree = west_lon_degree;
    self->south_lat_degree = south_lat_degree;
    self->east_lon_degree = east_lon_degree;
    self->north_lat_degree = north_lat_degree;
    self->name = name ? CPLStrdup(name) : NULL;
    return self;
  }

  ~OSRAreaOfUse()
  {
    CPLFree( self->name );
    CPLFree( self );
  }
} /* extend */
}; /* OSRAreaOfUse */

%apply Pointer NONNULL {OSRAreaOfUse *area};
%inline %{

double OSRAreaOfUse_west_lon_degree_get( OSRAreaOfUse *area ) {
  return area->west_lon_degree;
}

double OSRAreaOfUse_south_lat_degree_get( OSRAreaOfUse *area ) {
  return area->south_lat_degree;
}

double OSRAreaOfUse_east_lon_degree_get( OSRAreaOfUse *area ) {
  return area->east_lon_degree;
}

double OSRAreaOfUse_north_lat_degree_get( OSRAreaOfUse *area ) {
  return area->north_lat_degree;
}

const char* OSRAreaOfUse_name_get( OSRAreaOfUse *area ) {
  return area->name;
}

%}

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


#ifndef SWIGJAVA
  %feature("kwargs") OSRSpatialReferenceShadow;
#endif
  OSRSpatialReferenceShadow( char const * wkt = "" ) {
    return (OSRSpatialReferenceShadow*) OSRNewSpatialReference(wkt);
  }

  ~OSRSpatialReferenceShadow() {
    if (OSRDereference( self ) == 0 ) {
      OSRDestroySpatialReference( self );
    }
  }

/* FIXME : all bindings should avoid using the #else case */
/* as the deallocator for the char* is delete[] where as */
/* OSRExportToPrettyWkt uses CPL/VSIMalloc() */
#if defined(SWIGCSHARP)||defined(SWIGPYTHON)||defined(SWIGJAVA)
  retStringAndCPLFree *__str__() {
    char *buf = 0;
    OSRExportToPrettyWkt( self, &buf, 0 );
    return buf;
  }
#else
%newobject __str__;
  char *__str__() {
    char *buf = 0;
    OSRExportToPrettyWkt( self, &buf, 0 );
    return buf;
  }
#endif

  const char* GetName() {
    return OSRGetName( self );
  }

%apply Pointer NONNULL {OSRSpatialReferenceShadow* rhs};
#ifndef SWIGJAVA
  %feature("kwargs") IsSame;
#endif
  int IsSame( OSRSpatialReferenceShadow *rhs, char **options = NULL ) {
    return OSRIsSameEx( self, rhs, options );
  }

  int IsSameGeogCS( OSRSpatialReferenceShadow *rhs ) {
    return OSRIsSameGeogCS( self, rhs );
  }

  int IsSameVertCS( OSRSpatialReferenceShadow *rhs ) {
    return OSRIsSameVertCS( self, rhs );
  }

  int IsGeographic() {
    return OSRIsGeographic(self);
  }

  int IsDerivedGeographic() {
    return OSRIsDerivedGeographic(self);
  }

  int IsProjected() {
    return OSRIsProjected(self);
  }

  int IsCompound() {
    return OSRIsCompound(self);
  }

  int IsGeocentric() {
    return OSRIsGeocentric(self);
  }

  int IsLocal() {
    return OSRIsLocal(self);
  }

  int IsVertical() {
    return OSRIsVertical(self);
  }

  bool IsDynamic() {
    return OSRIsDynamic(self);
  }

  double GetCoordinateEpoch() {
    return OSRGetCoordinateEpoch(self);
  }

  void SetCoordinateEpoch(double coordinateEpoch) {
    OSRSetCoordinateEpoch(self, coordinateEpoch);
  }

  int EPSGTreatsAsLatLong() {
    return OSREPSGTreatsAsLatLong(self);
  }

  int EPSGTreatsAsNorthingEasting() {
    return OSREPSGTreatsAsNorthingEasting(self);
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

  // Added in GDAL 2.1
  const char* GetAngularUnitsName()
  {
    char *name = 0;
    OSRGetAngularUnits( self, &name );
    // This is really a const char* that is returned and shouldn't be freed
    return (const char*)name;
  }

  OGRErr SetTargetLinearUnits( const char *target, const char*name, double to_meters ) {
    return OSRSetTargetLinearUnits( self, target, name, to_meters );
  }

  OGRErr SetLinearUnits( const char*name, double to_meters ) {
    return OSRSetLinearUnits( self, name, to_meters );
  }

  OGRErr SetLinearUnitsAndUpdateParameters( const char*name, double to_meters) {
    return OSRSetLinearUnitsAndUpdateParameters( self, name, to_meters );
  }

  double GetTargetLinearUnits( const char *target_key ) {
    // Return code ignored.
    return OSRGetTargetLinearUnits( self, target_key, 0 );
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

  %newobject GetAreaOfUse;
  OSRAreaOfUse* GetAreaOfUse() {
    OSRAreaOfUse* pArea = new_OSRAreaOfUse(0,0,0,0,NULL);
    const char* name = NULL;
    if( !OSRGetAreaOfUse(self,
                    &pArea->west_lon_degree,
                    &pArea->south_lat_degree,
                    &pArea->east_lon_degree,
                    &pArea->north_lat_degree,
                    &name) )
    {
        delete_OSRAreaOfUse(pArea);
        return NULL;
    }
    pArea->name = name ? CPLStrdup(name) : NULL;
    return pArea;
  }

  /* Added in GDAL 2.1 */
  const char *GetAxisName( const char *target_key, int iAxis ) {
    return OSRGetAxis( self, target_key, iAxis, NULL );
  }

  /* Added in GDAL 3.1 */
  int GetAxesCount() {
    return OSRGetAxesCount(self);
  }

  /* Added in GDAL 2.1 */
  OGRAxisOrientation GetAxisOrientation( const char *target_key, int iAxis ) {
    OGRAxisOrientation orientation = OAO_Other;
    OSRGetAxis( self, target_key, iAxis, &orientation );
    return orientation;
  }

  OSRAxisMappingStrategy GetAxisMappingStrategy() {
    return OSRGetAxisMappingStrategy(self);
  }

  void SetAxisMappingStrategy(OSRAxisMappingStrategy strategy) {
    OSRSetAxisMappingStrategy(self, strategy);
  }

#if defined(SWIGJAVA)
  retIntArray GetDataAxisToSRSAxisMapping(int *nLen, const int **pList) {
      *pList = OSRGetDataAxisToSRSAxisMapping(self, nLen);
      return (retIntArray)*pList;
  }
#elif defined(SWIGCSHARP)
  %apply (int *intList) {const int *};
  %apply (int *hasval) {int *count};
  const int *GetDataAxisToSRSAxisMapping(int *count) {
      return OSRGetDataAxisToSRSAxisMapping(self, count);
  }
  %clear (const int *);
  %clear (int *count);
#else
  void GetDataAxisToSRSAxisMapping(int *nLen, const int **pList) {
      *pList = OSRGetDataAxisToSRSAxisMapping(self, nLen);
  }
#endif

  OGRErr SetDataAxisToSRSAxisMapping(int nList, int* pList) {
    return OSRSetDataAxisToSRSAxisMapping(self, nList, pList);
  }

  OGRErr SetUTM( int zone, int north =1 ) {
    return OSRSetUTM( self, zone, north );
  }

  int GetUTMZone() {
    // Note: we will return south zones as negative since it is
    // hard to return two values as the C API does.
    int bNorth = FALSE;
    int nZone = OSRGetUTMZone( self, &bNorth );
    if( !bNorth )
        nZone = -1 * ABS(nZone);
    return nZone;
  }

  OGRErr SetStatePlane( int zone, int is_nad83 = 1, char const *unitsname = "", double units = 0.0 ) {
    return OSRSetStatePlaneWithUnits( self, zone, is_nad83, unitsname, units );
  }

  OGRErr AutoIdentifyEPSG() {
    return OSRAutoIdentifyEPSG( self );
  }

#ifdef SWIGPYTHON
  void FindMatches( char** options = NULL, OSRSpatialReferenceShadow*** matches = NULL, int* nvalues = NULL, int** confidence_values = NULL )
  {
        *matches = OSRFindMatches(self, options, nvalues, confidence_values);
  }
#endif

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

  double GetSemiMajor( ) {
    // Return code ignored.
    return OSRGetSemiMajor( self, 0 );
  }

  double GetSemiMinor( ) {
    // Return code ignored.
    return OSRGetSemiMinor( self, 0 );
  }

  double GetInvFlattening( ) {
    // Return code ignored.
    return OSRGetInvFlattening( self, 0 );
  }

%feature( "kwargs" ) SetACEA;
  OGRErr SetACEA( double stdp1, double stdp2,
 		double clat, double clong,
                double fe, double fn ) {
    return OSRSetACEA( self, stdp1, stdp2, clat, clong,
                       fe, fn );
  }

%feature( "kwargs" ) SetAE;
  OGRErr SetAE( double clat, double clong,
              double fe, double fn ) {
    return OSRSetAE( self, clat, clong,
                     fe, fn );
  }

%feature( "kwargs" ) SetBonne;
  OGRErr SetBonne( double stdp, double cm, double fe, double fn ) {
    return OSRSetBonne( self, stdp, cm, fe, fn );
  }

%feature( "kwargs" ) SetCEA;
  OGRErr SetCEA( double stdp1, double cm,
               double fe, double fn ) {
    return OSRSetCEA( self, stdp1, cm,
                      fe, fn );
  }

%feature( "kwargs" ) SetCS;
  OGRErr SetCS( double clat, double clong,
              double fe, double fn ) {
    return OSRSetCS( self, clat, clong,
                     fe, fn );
  }

%feature( "kwargs" ) SetEC;
  OGRErr SetEC( double stdp1, double stdp2,
              double clat, double clong,
              double fe, double fn ) {
    return OSRSetEC( self, stdp1, stdp2, clat, clong,
                     fe, fn );
  }

%feature( "kwargs" ) SetEckertIV;
  OGRErr SetEckertIV( double cm,
                    double fe, double fn ) {
    return OSRSetEckertIV( self, cm, fe, fn);
  }

%feature( "kwargs" ) SetEckertVI;
  OGRErr SetEckertVI( double cm,
                    double fe, double fn ) {
    return OSRSetEckertVI( self, cm, fe, fn);
  }

%feature( "kwargs" ) SetEquirectangular;
  OGRErr SetEquirectangular( double clat, double clong,
                           double fe, double fn ) {
    return OSRSetEquirectangular( self, clat, clong,
                                  fe, fn );
  }

%feature( "kwargs" ) SetEquirectangular2;
  OGRErr SetEquirectangular2( double clat, double clong,
                              double pseudostdparallellat,
                              double fe, double fn ) {
    return OSRSetEquirectangular2( self, clat, clong,
                                   pseudostdparallellat,
                                   fe, fn );
  }

%feature( "kwargs" ) SetGaussSchreiberTMercator;
  OGRErr SetGaussSchreiberTMercator( double clat, double clong, double sc, double fe, double fn ) {
    return OSRSetGaussSchreiberTMercator( self, clat, clong, sc, fe, fn );
  }

%feature( "kwargs" ) SetGS;
  OGRErr SetGS( double cm,
              double fe, double fn ) {
    return OSRSetGS( self, cm, fe, fn );
  }

%feature( "kwargs" ) SetGH;
  OGRErr SetGH( double cm,
              double fe, double fn ) {
    return OSRSetGH( self, cm, fe, fn );
  }

  OGRErr SetIGH() {
    return OSRSetIGH( self );
  }

%feature( "kwargs" ) SetGEOS;
  OGRErr SetGEOS( double cm, double satelliteheight,
                double fe, double fn ) {
    return OSRSetGEOS( self, cm, satelliteheight,
                       fe, fn );
  }

%feature( "kwargs" ) SetGnomonic;
  OGRErr SetGnomonic( double clat, double clong,
                    double fe, double fn ) {
    return OSRSetGnomonic( self, clat, clong,
                           fe, fn );
  }

%feature( "kwargs" ) SetHOM;
  OGRErr SetHOM( double clat, double clong,
               double azimuth, double recttoskew,
               double scale,
               double fe, double fn ) {
    return OSRSetHOM( self, clat, clong, azimuth, recttoskew,
                      scale, fe, fn );
  }

%feature( "kwargs" ) SetHOM2PNO;
  OGRErr SetHOM2PNO( double clat,
                   double dfLat1, double dfLong1,
                   double dfLat2, double dfLong2,
                   double scale,
                   double fe, double fn ) {
    return OSRSetHOM2PNO( self, clat, dfLat1, dfLong1, dfLat2, dfLong2,
                          scale, fe, fn );
  }

%feature( "kwargs" ) SetKrovak;
  OGRErr SetKrovak( double clat, double clong,
                  double azimuth, double pseudostdparallellat,
                  double scale,
                  double fe, double fn ) {
    return OSRSetKrovak( self, clat, clong,
                         azimuth, pseudostdparallellat,
                         scale, fe, fn );
  }

%feature( "kwargs" ) SetLAEA;
  OGRErr SetLAEA( double clat, double clong,
                double fe, double fn ) {
    return OSRSetLAEA( self, clat, clong,
                       fe, fn );
  }

%feature( "kwargs" ) SetLCC;
  OGRErr SetLCC( double stdp1, double stdp2,
               double clat, double clong,
               double fe, double fn ) {
    return OSRSetLCC( self, stdp1, stdp2, clat, clong,
                      fe, fn );
  }

%feature( "kwargs" ) SetLCC1SP;
  OGRErr SetLCC1SP( double clat, double clong,
                  double scale,
                  double fe, double fn ) {
    return OSRSetLCC1SP( self, clat, clong, scale,
                         fe, fn );
  }

%feature( "kwargs" ) SetLCCB;
  OGRErr SetLCCB( double stdp1, double stdp2,
                double clat, double clong,
                double fe, double fn ) {
    return OSRSetLCCB( self, stdp1, stdp2, clat, clong,
                       fe, fn );
  }

%feature( "kwargs" ) SetMC;
  OGRErr SetMC( double clat, double clong,
              double fe, double fn ) {
    return OSRSetMC( self, clat, clong,
                     fe, fn );
  }

%feature( "kwargs" ) SetMercator;
  OGRErr SetMercator( double clat, double clong,
                    double scale,
                    double fe, double fn ) {
    return OSRSetMercator( self, clat, clong,
                           scale, fe, fn );
  }

%feature( "kwargs" ) SetMercator2SP;
  OGRErr SetMercator2SP( double stdp1,
                         double clat, double clong,
                         double fe, double fn ) {
    return OSRSetMercator2SP( self, stdp1, clat, clong, fe, fn );
  }

%feature( "kwargs" ) SetMollweide;
  OGRErr  SetMollweide( double cm,
                      double fe, double fn ) {
    return OSRSetMollweide( self, cm,
                            fe, fn );
  }

%feature( "kwargs" ) SetNZMG;
  OGRErr SetNZMG( double clat, double clong,
                double fe, double fn ) {
    return OSRSetNZMG( self, clat, clong,
                       fe, fn );
  }

%feature( "kwargs" ) SetOS;
  OGRErr SetOS( double dfOriginLat, double dfCMeridian,
              double scale,
              double fe,double fn) {
    return OSRSetOS( self, dfOriginLat, dfCMeridian, scale,
                     fe, fn );
  }

%feature( "kwargs" ) SetOrthographic;
  OGRErr SetOrthographic( double clat, double clong,
                        double fe,double fn) {
    return OSRSetOrthographic( self, clat, clong,
                               fe, fn );
  }

%feature( "kwargs" ) SetPolyconic;
  OGRErr SetPolyconic( double clat, double clong,
                     double fe, double fn ) {
    return OSRSetPolyconic( self, clat, clong,
                            fe, fn );
  }

%feature( "kwargs" ) SetPS;
  OGRErr SetPS( double clat, double clong,
              double scale,
              double fe, double fn) {
    return OSRSetPS( self, clat, clong, scale,
                     fe, fn );
  }

%feature( "kwargs" ) SetRobinson;
  OGRErr SetRobinson( double clong,
                    double fe, double fn ) {
    return OSRSetRobinson( self, clong, fe, fn );
  }

%feature( "kwargs" ) SetSinusoidal;
  OGRErr SetSinusoidal( double clong,
                      double fe, double fn ) {
    return OSRSetSinusoidal( self, clong, fe, fn );
  }

%feature( "kwargs" ) SetStereographic;
  OGRErr SetStereographic( double clat, double clong,
                         double scale,
                         double fe,double fn) {
    return OSRSetStereographic( self, clat, clong, scale,
                                fe, fn );
  }

%feature( "kwargs" ) SetSOC;
  OGRErr SetSOC( double latitudeoforigin, double cm,
               double fe, double fn ) {
    return OSRSetSOC( self, latitudeoforigin, cm,
	              fe, fn );
  }

%feature( "kwargs" ) SetTM;
  OGRErr SetTM( double clat, double clong,
              double scale,
              double fe, double fn ) {
    return OSRSetTM( self, clat, clong, scale,
                     fe, fn );
  }

%feature( "kwargs" ) SetTMVariant;
  OGRErr SetTMVariant( const char *pszVariantName,
                     double clat, double clong,
                     double scale,
                     double fe, double fn ) {
    return OSRSetTMVariant( self, pszVariantName, clat, clong,
                            scale, fe, fn );
  }

%feature( "kwargs" ) SetTMG;
  OGRErr SetTMG( double clat, double clong,
               double fe, double fn ) {
    return OSRSetTMG( self, clat, clong,
                      fe, fn );
  }

%feature( "kwargs" ) SetTMSO;
  OGRErr SetTMSO( double clat, double clong,
                double scale,
                double fe, double fn ) {
    return OSRSetTMSO( self, clat, clong, scale,
                       fe, fn );
  }

%feature( "kwargs" ) SetVDG;
  OGRErr SetVDG( double clong,
               double fe, double fn ) {
    return OSRSetVDG( self, clong, fe, fn );
  }

%feature( "kwargs" ) SetVerticalPerspective;
  OGRErr SetVerticalPerspective( double topoOriginLat,
                                 double topoOriginLon,
                                 double topoOriginHeight,
                                 double viewPointHeight,
                                 double fe, double fn )
  {
    return OSRSetVerticalPerspective( self,
        topoOriginLat, topoOriginLon, topoOriginHeight, viewPointHeight, fe, fn );
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

#ifdef SWIGJAVA
  OGRErr SetTOWGS84( double p1, double p2, double p3,
                     double p4, double p5,
                     double p6, double p7 ) {
#else
  OGRErr SetTOWGS84( double p1, double p2, double p3,
                     double p4 = 0.0, double p5 = 0.0,
                     double p6 = 0.0, double p7 = 0.0 ) {
#endif
    return OSRSetTOWGS84( self, p1, p2, p3, p4, p5, p6, p7 );
  }

  bool HasTOWGS84() {
    double ignored[7];
    return OSRGetTOWGS84( self, ignored, 7 ) == OGRERR_NONE;
  }

  OGRErr GetTOWGS84( double argout[7] ) {
    return OSRGetTOWGS84( self, argout, 7 );
  }

  OGRErr AddGuessedTOWGS84() {
    return OSRAddGuessedTOWGS84( self );
  }

  OGRErr SetLocalCS( const char *pszName ) {
    return OSRSetLocalCS( self, pszName );
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

  OGRErr SetGeocCS( const char *name = "unnamed" ) {
    return OSRSetGeocCS( self, name );
  }

  OGRErr SetVertCS( const char *VertCSName = "unnamed",
                    const char *VertDatumName = "unnamed",
                    int VertDatumType = 0) {
    return OSRSetVertCS( self, VertCSName, VertDatumName, VertDatumType );
  }

%apply Pointer NONNULL {OSRSpatialReferenceShadow* horizcs};
%apply Pointer NONNULL {OSRSpatialReferenceShadow* vertcs};
  OGRErr SetCompoundCS( const char *name,
                        OSRSpatialReferenceShadow *horizcs,
                        OSRSpatialReferenceShadow *vertcs ) {
    return OSRSetCompoundCS( self, name, horizcs, vertcs );
  }

%apply (char **ignorechange) { (char **) };
  OGRErr ImportFromWkt( char **ppszInput ) {
    return OSRImportFromWkt( self, ppszInput );
  }
%clear (char **);

  OGRErr ImportFromProj4( char *ppszInput ) {
    return OSRImportFromProj4( self, ppszInput );
  }

%apply Pointer NONNULL {char* url};
  OGRErr ImportFromUrl( char *url ) {
    return OSRImportFromUrl( self, url );
  }

%apply (char **options) { (char **) };
  OGRErr ImportFromESRI( char **ppszInput ) {
    return OSRImportFromESRI( self, ppszInput );
  }
%clear (char **);

  OGRErr ImportFromEPSG( int arg ) {
    return OSRImportFromEPSG(self, arg);
  }

  OGRErr ImportFromEPSGA( int arg ) {
    return OSRImportFromEPSGA(self, arg);
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

%apply Pointer NONNULL {char const *proj};
%apply Pointer NONNULL {char const *datum};
  OGRErr ImportFromERM( char const *proj, char const *datum,
                        char const *units ) {
    return OSRImportFromERM( self, proj, datum, units );
  }

  OGRErr ImportFromMICoordSys( char const *pszCoordSys ) {
    return OSRImportFromMICoordSys( self, pszCoordSys );
  }

%apply Pointer NONNULL {const char* const *papszLines};
%apply (char **options) { (const char* const *papszLines) };
  OGRErr ImportFromOzi( const char* const *papszLines ) {
    return OSRImportFromOzi( self, papszLines );
  }

  OGRErr ExportToWkt( char **argout, char **options = NULL ) {
    return OSRExportToWktEx( self, argout, options );
  }

  OGRErr ExportToPrettyWkt( char **argout, int simplify = 0 ) {
    return OSRExportToPrettyWkt( self, argout, simplify );
  }

  OGRErr ExportToPROJJSON( char **argout, char **options = NULL ) {
    return OSRExportToPROJJSON( self, argout, options );
  }

  OGRErr ExportToProj4( char **argout ) {
    return OSRExportToProj4( self, argout );
  }

%apply (char **argout) { (char **) };
%apply (double *argout[ANY]) { (double *params[17] ) };
  OGRErr ExportToPCI( char **proj, char **units, double *params[17] ) {
    return OSRExportToPCI( self, proj, units, params );
  }
%clear (char **);
%clear (double *params[17]);

%apply (long *OUTPUT) { (long*) };
%apply (double *argout[ANY]) { (double *params[15]) }
  OGRErr ExportToUSGS( long *code, long *zone, double *params[15], long *datum ) {
    return OSRExportToUSGS( self, code, zone, params, datum );
  }
%clear (long*);
%clear (double *params[15]);

  OGRErr ExportToXML( char **argout, const char *dialect = "" ) {
    return OSRExportToXML( self, argout, dialect );
  }

  OGRErr ExportToMICoordSys( char **argout ) {
    return OSRExportToMICoordSys( self, argout );
  }

%newobject CloneGeogCS;
  OSRSpatialReferenceShadow *CloneGeogCS() {
    return (OSRSpatialReferenceShadow*) OSRCloneGeogCS(self);
  }

%newobject Clone;
  OSRSpatialReferenceShadow *Clone() {
    return (OSRSpatialReferenceShadow*) OSRClone(self);
  }

  OGRErr Validate() {
    return OSRValidate(self);
  }

  OGRErr MorphToESRI() {
    return OSRMorphToESRI(self);
  }

  OGRErr MorphFromESRI() {
    return OSRMorphFromESRI(self);
  }

%newobject ConvertToOtherProjection;
  OSRSpatialReferenceShadow* ConvertToOtherProjection(const char* other_projection, char **options = NULL) {
    return OSRConvertToOtherProjection(self, other_projection, options);
  }

%clear const char* name;
  OGRErr PromoteTo3D( const char* name = NULL ) {
    return OSRPromoteTo3D(self, name);
  }

  OGRErr DemoteTo2D( const char* name = NULL ) {
    return OSRDemoteTo2D(self, name);
  }
%apply Pointer NONNULL {const char* name};

} /* %extend */
};


/******************************************************************************
 *
 *  CoordinateTransformation Object
 *
 */

%rename (CoordinateTransformationOptions) OGRCoordinateTransformationOptions;
class OGRCoordinateTransformationOptions {
private:
  OGRCoordinateTransformationOptions();
public:
%extend {

  OGRCoordinateTransformationOptions() {
    return OCTNewCoordinateTransformationOptions();
  }

  ~OGRCoordinateTransformationOptions() {
    OCTDestroyCoordinateTransformationOptions( self );
  }

  bool SetAreaOfInterest( double westLongitudeDeg,
                          double southLatitudeDeg,
                          double eastLongitudeDeg,
                          double northLatitudeDeg ) {
    return OCTCoordinateTransformationOptionsSetAreaOfInterest(self,
        westLongitudeDeg, southLatitudeDeg,
        eastLongitudeDeg, northLatitudeDeg);
  }

  bool SetOperation(const char* operation) {
    return OCTCoordinateTransformationOptionsSetOperation(self, operation, false);
  }

  bool SetDesiredAccuracy(double accuracy) {
    return OCTCoordinateTransformationOptionsSetDesiredAccuracy(self, accuracy);
  }

  bool SetBallparkAllowed(bool allowBallpark) {
    return OCTCoordinateTransformationOptionsSetBallparkAllowed(self, allowBallpark);
  }
} /*extend */
};

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
    return (OSRCoordinateTransformationShadow*) OCTNewCoordinateTransformation(src, dst);
  }

  OSRCoordinateTransformationShadow( OSRSpatialReferenceShadow *src, OSRSpatialReferenceShadow *dst, OGRCoordinateTransformationOptions* options ) {
    return (OSRCoordinateTransformationShadow*)
        options ? OCTNewCoordinateTransformationEx( src, dst, options ) : OCTNewCoordinateTransformation(src, dst);
  }

  ~OSRCoordinateTransformationShadow() {
    OCTDestroyCoordinateTransformation( self );
  }

// Need to apply argin typemap second so the numinputs=1 version gets applied
// instead of the numinputs=0 version from argout.
#ifdef SWIGJAVA
%apply (double argout[ANY]) {(double inout[3])};
#else
%apply (double argout[ANY]) {(double inout[3])};
%apply (double argin[ANY]) {(double inout[3])};
#endif
  void TransformPoint( double inout[3] ) {
    if (self == NULL)
        return;
    OCTTransform( self, 1, &inout[0], &inout[1], &inout[2] );
  }
%clear (double inout[3]);

#ifdef SWIGJAVA
%apply (double argout[ANY]) {(double inout[4])};
#else
%apply (double argout[ANY]) {(double inout[4])};
%apply (double argin[ANY]) {(double inout[4])};
#endif
  void TransformPoint( double inout[4] ) {
    if (self == NULL)
        return;
    OCTTransform4D( self, 1, &inout[0], &inout[1], &inout[2], &inout[3], NULL );
  }
%clear (double inout[4]);

  void TransformPoint( double argout[3], double x, double y, double z = 0.0 ) {
    if (self == NULL)
        return;
    argout[0] = x;
    argout[1] = y;
    argout[2] = z;
    OCTTransform( self, 1, &argout[0], &argout[1], &argout[2] );
  }

  void TransformPoint( double argout[4], double x, double y, double z, double t ) {
    if (self == NULL)
        return;
    argout[0] = x;
    argout[1] = y;
    argout[2] = z;
    argout[3] = t;
    OCTTransform4D( self, 1, &argout[0], &argout[1], &argout[2], &argout[3], NULL );
  }

#if defined(SWIGPYTHON)
  void TransformPointWithErrorCode( double argout[4], int errorCode[1], double x, double y, double z, double t ) {
    if (self == NULL)
        return;
    argout[0] = x;
    argout[1] = y;
    argout[2] = z;
    argout[3] = t;
    OCTTransform4DWithErrorCodes( self, 1, &argout[0], &argout[1], &argout[2], &argout[3], errorCode );
  }
#else
  int TransformPointWithErrorCode( double argout[4], double x, double y, double z, double t ) {
    if (self == NULL)
        return -1;
    argout[0] = x;
    argout[1] = y;
    argout[2] = z;
    argout[3] = t;
    int errorCode = 0;
    OCTTransform4DWithErrorCodes( self, 1, &argout[0], &argout[1], &argout[2], &argout[3], &errorCode );
    return errorCode;
  }
#endif

#ifdef SWIGCSHARP
  %apply (double *inout) {(double*)};
#endif

#ifndef SWIGPYTHON
  void TransformPoints( int nCount, double *x, double *y, double *z ) {
    if (self == NULL)
        return;
    OCTTransform( self, nCount, x, y, z );
  }
#else
  void TransformPoints( int nCount, double *x, double *y, double *z, double *t ) {
    if (self == NULL)
        return;
    OCTTransform4D( self, nCount, x, y, z, t, NULL );
  }
#endif

#ifdef SWIGJAVA
  %apply (int* outIntArray) {int*};
  int* TransformPointsWithErrorCodes( int nCount, double *x, double *y, double *z, double *t, int* pnCountOut, int** outErrorCodes ) {
    *pnCountOut = 0;
    *outErrorCodes = NULL;
    if (self == NULL)
        return NULL;
    *pnCountOut = nCount;
    *outErrorCodes = (int*)CPLMalloc(sizeof(int) * nCount);
    OCTTransform4DWithErrorCodes( self, nCount, x, y, z, t, *outErrorCodes );
    return NULL;
  }
  %clear int*;
#endif

#ifdef SWIGCSHARP
  %clear (double*);
#endif

void TransformBounds(
    double argout[4], double minx, double miny, double maxx, double maxy, int densify_pts
) {
    argout[0] = HUGE_VAL;
    argout[1] = HUGE_VAL;
    argout[2] = HUGE_VAL;
    argout[3] = HUGE_VAL;
    if (self == NULL)
        return;
    OCTTransformBounds(
        self,
        minx, miny, maxx, maxy,
        &argout[0], &argout[1], &argout[2], &argout[3],
        densify_pts
    );
}

} /*extend */
};

/* New in GDAL 1.10 */
%newobject CreateCoordinateTransformation;
%inline %{
  OSRCoordinateTransformationShadow *CreateCoordinateTransformation( OSRSpatialReferenceShadow *src, OSRSpatialReferenceShadow *dst, OGRCoordinateTransformationOptions* options = NULL ) {
    return (OSRCoordinateTransformationShadow*)
        options ? OCTNewCoordinateTransformationEx( src, dst, options ) : OCTNewCoordinateTransformation(src, dst);
}
%}

/************************************************************************/
/*                   GetCRSInfoListFromDatabase()                       */
/************************************************************************/

#if defined(SWIGPYTHON) || defined(SWIGCSHARP)

%rename (CRSType) OSRCRSType;
typedef enum OSRCRSType
{
    /** Geographic 2D CRS */
    OSR_CRS_TYPE_GEOGRAPHIC_2D = 0,
    /** Geographic 3D CRS */
    OSR_CRS_TYPE_GEOGRAPHIC_3D = 1,
    /** Geocentric CRS */
    OSR_CRS_TYPE_GEOCENTRIC = 2,
    /** Projected CRS */
    OSR_CRS_TYPE_PROJECTED = 3,
    /** Vertical CRS */
    OSR_CRS_TYPE_VERTICAL = 4,
    /** Compound CRS */
    OSR_CRS_TYPE_COMPOUND = 5,
    /** Other */
    OSR_CRS_TYPE_OTHER = 6,
};

%rename (CRSInfo) OSRCRSInfo;

struct OSRCRSInfo {
%extend {
%immutable;
    /** Authority name. */
    char* auth_name;
    /** Object code. */
    char* code;
    /** Object name. */
    char* name;
    /** Object type. */
    OSRCRSType type;
    /** Whether the object is deprecated */
    bool deprecated;
    /** Whereas the west_lon_degree, south_lat_degree, east_lon_degree and
     * north_lat_degree fields are valid. */
    bool  bbox_valid;
    /** Western-most longitude of the area of use, in degrees. */
    double west_lon_degree;
    /** Southern-most latitude of the area of use, in degrees. */
    double south_lat_degree;
    /** Eastern-most longitude of the area of use, in degrees. */
    double east_lon_degree;
    /** Northern-most latitude of the area of use, in degrees. */
    double north_lat_degree;
    /** Name of the area of use. */
    char* area_name;
    /** Name of the projection method for a projected CRS. Might be NULL even
     *for projected CRS in some cases. */
    char* projection_method;

  OSRCRSInfo( const char* auth_name,
              const char* code,
              const char* name,
              OSRCRSType type,
              bool deprecated,
              bool bbox_valid,
              double west_lon_degree,
              double south_lat_degree,
              double east_lon_degree,
              double north_lat_degree,
              const char* area_name,
              const char* projection_method)
    {
    OSRCRSInfo *self = (OSRCRSInfo*) CPLMalloc( sizeof( OSRCRSInfo ) );
    self->pszAuthName = auth_name ? CPLStrdup(auth_name) : NULL;
    self->pszCode = code ? CPLStrdup(code) : NULL;
    self->pszName = name ? CPLStrdup(name) : NULL;
    self->eType = type;
    self->bDeprecated = deprecated;
    self->bBboxValid = bbox_valid;
    self->dfWestLongitudeDeg = west_lon_degree;
    self->dfSouthLatitudeDeg = south_lat_degree;
    self->dfEastLongitudeDeg = east_lon_degree;
    self->dfNorthLatitudeDeg = north_lat_degree;
    self->pszAreaName = area_name ? CPLStrdup(area_name) : NULL;
    self->pszProjectionMethod = projection_method ? CPLStrdup(projection_method) : NULL;
    return self;
  }

  ~OSRCRSInfo() {
    CPLFree( self->pszAuthName );
    CPLFree( self->pszCode );
    CPLFree( self->pszName );
    CPLFree( self->pszAreaName );
    CPLFree( self->pszProjectionMethod );
    CPLFree( self );
  }
} /* extend */
}; /* OSRCRSInfo */

%apply Pointer NONNULL {OSRCRSInfo *crsInfo};
%inline %{

const char* OSRCRSInfo_auth_name_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->pszAuthName;
}

const char* OSRCRSInfo_code_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->pszCode;
}

const char* OSRCRSInfo_name_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->pszName;
}

OSRCRSType OSRCRSInfo_type_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->eType;
}

bool OSRCRSInfo_deprecated_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->bDeprecated;
}

bool OSRCRSInfo_bbox_valid_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->bBboxValid;
}

double OSRCRSInfo_west_lon_degree_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->dfWestLongitudeDeg;
}

double OSRCRSInfo_south_lat_degree_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->dfSouthLatitudeDeg;
}

double OSRCRSInfo_east_lon_degree_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->dfEastLongitudeDeg;
}

double OSRCRSInfo_north_lat_degree_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->dfNorthLatitudeDeg;
}

const char* OSRCRSInfo_area_name_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->pszAreaName;
}

const char* OSRCRSInfo_projection_method_get( OSRCRSInfo *crsInfo ) {
  return crsInfo->pszProjectionMethod;
}

%}

#endif

#ifdef SWIGPYTHON
%inline %{
void GetCRSInfoListFromDatabase( const char *authName,
                                 OSRCRSInfo*** pList,
                                 int* pnListCount)
{
    *pList = OSRGetCRSInfoListFromDatabase(authName, NULL, pnListCount);
}
%}

#endif // SWIGPYTHON

%inline %{
void SetPROJSearchPath( const char *utf8_path )
{
    const char* const apszPaths[2] = { utf8_path, NULL };
    OSRSetPROJSearchPaths(apszPaths);
}
%}

%apply (char **options) { (char **) };
%inline %{
void SetPROJSearchPaths( char** paths )
{
    OSRSetPROJSearchPaths(paths);
}
%}
%clear (char **);

%apply (char **CSL) {(char **)};
%inline %{
char** GetPROJSearchPaths()
{
    return OSRGetPROJSearchPaths();
}
%}
%clear (char **);

%inline %{
int GetPROJVersionMajor()
{
    int num;
    OSRGetPROJVersion(&num, NULL, NULL);
    return num;
}

int GetPROJVersionMinor()
{
    int num;
    OSRGetPROJVersion(NULL, &num, NULL);
    return num;
}

int GetPROJVersionMicro()
{
    int num;
    OSRGetPROJVersion(NULL, NULL, &num);
    return num;
}

bool GetPROJEnableNetwork()
{
    return OSRGetPROJEnableNetwork();
}

void SetPROJEnableNetwork(bool enabled)
{
    OSRSetPROJEnableNetwork(enabled);
}
%}

%inline %{
void SetPROJAuxDbPath( const char *utf8_path )
{
    const char* const apszPaths[2] = { utf8_path, NULL };
    OSRSetPROJAuxDbPaths(apszPaths);
}
%}

%apply (char **options) { (char **) };
%inline %{
void SetPROJAuxDbPaths( char** paths )
{
    OSRSetPROJAuxDbPaths(paths);
}
%}
%clear (char **);

%apply (char **CSL) {(char **)};
%inline %{
char** GetPROJAuxDbPaths()
{
    return OSRGetPROJAuxDbPaths();
}
%}
%clear (char **);

#ifdef SWIGPYTHON
%thread;
#endif
