/******************************************************************************
 * $Id$
 *
 * Name:     gdal.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.40  2006/02/02 20:52:40  collinsb
 * Added SWIG JAVA bindings
 *
 * Revision 1.39  2006/01/17 04:38:44  cfis
 * Grouped all renames together and added section for Ruby.
 *
 * Revision 1.38  2005/09/13 18:37:25  kruland
 * Added binding for GDALGetDriver.
 *
 * Revision 1.37  2005/09/13 16:09:12  kruland
 * Import gdal_perl.i for SWIGPERL.
 *
 * Revision 1.36  2005/09/06 01:43:06  kruland
 * Include gdal_typemaps.i if no other file is specified.
 *
 * Revision 1.35  2005/09/02 21:42:42  kruland
 * The compactdefaultargs feature should be turned on for all bindings not just
 * python.
 *
 * Revision 1.34  2005/09/02 16:19:23  kruland
 * Major reorganization to accomodate multiple language bindings.
 * Each language binding can define renames and supplemental code without
 * having to have a lot of conditionals in the main interface definition files.
 *
 * Revision 1.33  2005/08/09 17:40:09  kruland
 * Added support for ruby.
 *
 * Revision 1.32  2005/08/08 17:06:40  kruland
 * Added bindings for ParseXMLString and SerializeXMLTree.
 *
 * Revision 1.31  2005/08/06 20:51:58  kruland
 * Instead of using double_## defines and SWIG macros, use typemaps with
 * [ANY] specified and use $dim0 to extract the dimension.  This makes the
 * code quite a bit more readable.
 *
 * Revision 1.30  2005/08/05 18:48:59  hobu
 * gross hack of duplicate function names for the
 * GCP stuff because C# module of swig is stupid
 *
 * Revision 1.29  2005/08/04 19:18:01  kruland
 * The Open() and OpenShared() methods were incrementing the gdal internal
 * reference count by mistake.
 *
 * Revision 1.28  2005/07/20 16:33:52  kruland
 * Added wrapper for GDALGetDriverCount.
 * Added %init for PHP.
 *
 * Revision 1.27  2005/07/18 16:13:32  kruland
 * Added MajorObject.i an interface specification to the MajorObject baseclass.
 * Used inheritance in Band.i, Driver.i, and Dataset.i to access MajorObject
 * functionality.
 * Adjusted Makefile to have PYTHON be a variable, gdal wrapper depend on
 * MajorObject.i, use rm (instead of libtool's wrapped RM) for removal because
 * the libtool didn't accept -r.
 *
 * Revision 1.26  2005/07/15 16:58:04  kruland
 * In the %exception spec, if an error is detected while UseExceptions(),
 * SWIG_fail immediately.
 *
 * Revision 1.25  2005/07/15 15:10:03  kruland
 * Move the #ifdef SWIGPYTHON to include the exception flags.
 * Correct some %inline to use %{ %}.
 *
 * Revision 1.24  2005/06/23 14:46:39  kruland
 * Switch from using the poor-form exception in the custom CPLErrorHandler to
 * using a global variable flag.
 *
 * Revision 1.23  2005/06/22 18:48:23  kruland
 * Added bUseExceptions flag and supporting methods UseExceptions(),
 * DontUseExceptions() to the python binding.  This allows the user
 * to determine if method invocations will throw exceptions in scripts or
 * use the old return value method.
 * Added PythonErrorHandler, a special CPLErrorHandler which will throw.
 *
 * Revision 1.22  2005/03/10 17:18:15  hobu
 * #ifdefs for csharp
 *
 * Revision 1.21  2005/02/24 17:20:02  hobu
 * return the dataset in AutoCreateWarpedVRT
 *
 * Revision 1.20  2005/02/24 16:34:14  kruland
 * Defined GCP as an object.  Manipulate as an object.  Defined __str__
 * and serialize as python only methods.
 *
 * Revision 1.19  2005/02/23 21:38:28  kruland
 * Added AutoCreateWarpedVRT() global algorithm method.  Commented missing methods.
 *
 * Revision 1.18  2005/02/22 23:30:14  kruland
 * Increment the reference count in the Dataset factory methods: Open, OpenShared.
 *
 * Revision 1.17  2005/02/20 19:42:53  kruland
 * Rename the Swig shadow classes so the names do not give the impression that
 * they are any part of the GDAL/OSR apis.  There were no bugs with the old
 * names but they were confusing.
 *
 * Revision 1.16  2005/02/18 18:41:37  kruland
 * Added %feature("autodoc");
 *
 * Revision 1.15  2005/02/18 16:09:53  kruland
 * Added %feature("compactdefaultargs") which in python (and perhaps others)
 * allows SWIG to code default arguments for C functions (like GDALDecToDMS).
 * This also fixes a problem with Dataset::SetMetadata and there not being
 * a %typecheck for char** <- dict.
 *
 * Revision 1.14  2005/02/17 21:12:48  kruland
 * Added some more module level functions.
 *
 * Revision 1.13  2005/02/17 17:27:13  kruland
 * Changed the handling of fixed size double arrays to make it fit more
 * naturally with GDAL/OSR usage.  Declare as typedef double * double_17;
 * If used as return argument use:  function ( ... double_17 argout ... );
 * If used as value argument use: function (... double_17 argin ... );
 *
 * Revision 1.12  2005/02/16 18:40:34  kruland
 * Added typedef for GDALColorInterp.
 *
 * Revision 1.11  2005/02/16 16:55:49  kruland
 * Added typedef for CPLErr to prevent wrapping of the enum.
 * Moved the AllRegister method definition.
 *
 * Revision 1.10  2005/02/15 22:31:52  kruland
 * Moved CPL wrapping to cpl.i
 *
 * Revision 1.9  2005/02/15 22:02:16  kruland
 * Previous revision introduced a problem.  Put the #defines CPL_* back in
 * until the cpl specific code is cleaned up.
 *
 * Revision 1.8  2005/02/15 21:40:00  kruland
 * Stripped out all the extras by no longer including gdal.h or gdal_priv.h.
 * Added CreateShared() method.
 *
 * Revision 1.7  2005/02/15 16:51:20  kruland
 * Removed use of <vector> and <algorith> stdlib includes.  Added typedefs for
 * the fixed size array types which are needed for new mapping mechanism.
 *
 * Revision 1.6  2005/02/15 06:25:42  kruland
 * Moved the Band definition to Band.i.
 *
 * Revision 1.5  2005/02/15 06:01:15  kruland
 * Moved the Dataset definition to Dataset.i.
 *
 * Revision 1.4  2005/02/14 23:50:16  hobu
 * Added log info
 *
*/

%module gdal

%feature ("compactdefaultargs");

//
// We register all the drivers upon module initialization
//

%{
#include <iostream>
using namespace std;

#include "cpl_port.h"
#include "cpl_string.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdalwarper.h"

typedef void GDALMajorObjectShadow;
typedef void GDALDriverShadow;
typedef void GDALDatasetShadow;
typedef void GDALRasterBandShadow;

typedef int FALSE_IS_ERR;

%}

typedef int GDALColorInterp;
typedef int GDALAccess;
typedef int GDALDataType;
typedef int CPLErr;
typedef int GDALResampleAlg;

#if defined(SWIGPYTHON)
%include "gdal_python.i"
#elif defined(SWIGRUBY)
%include "gdal_ruby.i"
#elif defined(SWIGPHP4)
%include "gdal_php.i"
#elif defined(SWIGCSHARP)
%include "gdal_csharp.i"
#elif defined(SWIGPERL)
%include "gdal_perl.i"
#elif defined(SWIGJAVA)
%include "gdal_java.i"
#else
%include "gdal_typemaps.i"
#endif

//************************************************************************
//
// Define the exposed CPL functions.
//
//************************************************************************
%include "cpl.i"

//************************************************************************
//
// Define the MajorObject object
//
//************************************************************************
%include "MajorObject.i"

//************************************************************************
//
// Define the Driver object.
//
//************************************************************************
%include "Driver.i"


//************************************************************************
//
// Define renames.
//
//************************************************************************
%rename (GCP) GDAL_GCP;

#ifdef SWIGRUBY
%rename (all_register) GDALAllRegister;
%rename (get_cache_max) GDALGetCacheMax;
%rename (set_cache_max) GDALSetCacheMax;
%rename (set_cache_used) GDALGetCacheUsed;
%rename (get_data_type_size) GDALGetDataTypeSize;
%rename (data_type_is_complex) GDALDataTypeIsComplex;
%rename (gcps_to_geo_transform) GDALGCPsToGeoTransform;
%rename (get_data_type_name) GDALGetDataTypeName;
%rename (get_data_type_by_name) GDALGetDataTypeByName;
%rename (get_color_interpretation_name) GDALGetColorInterpretationName;
%rename (get_palette_interpretation_name) GDALGetPaletteInterpretationName;
%rename (dec_to_dms) GDALDecToDMS;
%rename (packed_dms_to_dec) GDALPackedDMSToDec;
%rename (dec_to_packed_dms) GDALDecToPackedDMS;
%rename (parse_xml_string) CPLParseXMLString;
%rename (serialize_xml_tree) CPLSerializeXMLTree;
#else
%rename (GCP) GDAL_GCP;
%rename (GCPsToGeoTransform) GDALGCPsToGeoTransform;
%rename (AllRegister) GDALAllRegister;
%rename (GetCacheMax) GDALGetCacheMax;
%rename (SetCacheMax) GDALSetCacheMax;
%rename (GetCacheUsed) GDALGetCacheUsed;
%rename (GetDataTypeSize) GDALGetDataTypeSize;
%rename (DataTypeIsComplex) GDALDataTypeIsComplex;
%rename (GCPsToGeoTransform) GDALGCPsToGeoTransform;
%rename (GetDataTypeName) GDALGetDataTypeName;
%rename (GetDataTypeByName) GDALGetDataTypeByName;
%rename (GetColorInterpretationName) GDALGetColorInterpretationName;
%rename (GetPaletteInterpretationName) GDALGetPaletteInterpretationName;
%rename (DecToDMS) GDALDecToDMS;
%rename (PackedDMSToDec) GDALPackedDMSToDec;
%rename (DecToPackedDMS) GDALDecToPackedDMS;
%rename (ParseXMLString) CPLParseXMLString;
%rename (SerializeXMLTree) CPLSerializeXMLTree;
#endif


//************************************************************************
//
// Define the Ground Control Point structure.
//
//************************************************************************
// GCP - class?  serialize() method missing.
struct GDAL_GCP {
%extend {
%mutable;
  double GCPX;
  double GCPY;
  double GCPZ;
  double GCPPixel;
  double GCPLine;
  char *Info;
  char *Id;
%immutable;

  GDAL_GCP( double x = 0.0, double y = 0.0, double z = 0.0,
            double pixel = 0.0, double line = 0.0,
            const char *info = "", const char *id = "" ) {
    GDAL_GCP *self = (GDAL_GCP*) CPLMalloc( sizeof( GDAL_GCP ) );
    self->dfGCPX = x;
    self->dfGCPY = y;
    self->dfGCPZ = z;
    self->dfGCPPixel = pixel;
    self->dfGCPLine = line;
    self->pszInfo =  CPLStrdup( (info == 0) ? "" : info );
    self->pszId = CPLStrdup( (id==0)? "" : id );
    return self;
  }

  ~GDAL_GCP() {
    if ( self->pszInfo )
      CPLFree( self->pszInfo );
    if ( self->pszId )
      CPLFree( self->pszId );
    CPLFree( self );
  }


} /* extend */
}; /* GDAL_GCP */
%inline %{

double GDAL_GCP_GCPX_get( GDAL_GCP *h ) {
  return h->dfGCPX;
}
void GDAL_GCP_GCPX_set( GDAL_GCP *h, double val ) {
  h->dfGCPX = val;
}
double GDAL_GCP_GCPY_get( GDAL_GCP *h ) {
  return h->dfGCPY;
}
void GDAL_GCP_GCPY_set( GDAL_GCP *h, double val ) {
  h->dfGCPY = val;
}
double GDAL_GCP_GCPZ_get( GDAL_GCP *h ) {
  return h->dfGCPZ;
}
void GDAL_GCP_GCPZ_set( GDAL_GCP *h, double val ) {
  h->dfGCPZ = val;
}
double GDAL_GCP_GCPPixel_get( GDAL_GCP *h ) {
  return h->dfGCPPixel;
}
void GDAL_GCP_GCPPixel_set( GDAL_GCP *h, double val ) {
  h->dfGCPPixel = val;
}
double GDAL_GCP_GCPLine_get( GDAL_GCP *h ) {
  return h->dfGCPLine;
}
void GDAL_GCP_GCPLine_set( GDAL_GCP *h, double val ) {
  h->dfGCPLine = val;
}
const char * GDAL_GCP_Info_get( GDAL_GCP *h ) {
  return h->pszInfo;
}
void GDAL_GCP_Info_set( GDAL_GCP *h, const char * val ) {
  if ( h->pszInfo ) 
    CPLFree( h->pszInfo );
  h->pszInfo = CPLStrdup(val);
}
const char * GDAL_GCP_Id_get( GDAL_GCP *h ) {
  return h->pszId;
}
void GDAL_GCP_Id_set( GDAL_GCP *h, const char * val ) {
  if ( h->pszId ) 
    CPLFree( h->pszId );
  h->pszId = CPLStrdup(val);
}



/* Duplicate, but transposed names for C# because 
*  the C# module outputs backwards names
*/
double GDAL_GCP_get_GCPX( GDAL_GCP *h ) {
  return h->dfGCPX;
}
void GDAL_GCP_set_GCPX( GDAL_GCP *h, double val ) {
  h->dfGCPX = val;
}
double GDAL_GCP_get_GCPY( GDAL_GCP *h ) {
  return h->dfGCPY;
}
void GDAL_GCP_set_GCPY( GDAL_GCP *h, double val ) {
  h->dfGCPY = val;
}
double GDAL_GCP_get_GCPZ( GDAL_GCP *h ) {
  return h->dfGCPZ;
}
void GDAL_GCP_set_GCPZ( GDAL_GCP *h, double val ) {
  h->dfGCPZ = val;
}
double GDAL_GCP_get_GCPPixel( GDAL_GCP *h ) {
  return h->dfGCPPixel;
}
void GDAL_GCP_set_GCPPixel( GDAL_GCP *h, double val ) {
  h->dfGCPPixel = val;
}
double GDAL_GCP_get_GCPLine( GDAL_GCP *h ) {
  return h->dfGCPLine;
}
void GDAL_GCP_set_GCPLine( GDAL_GCP *h, double val ) {
  h->dfGCPLine = val;
}
const char * GDAL_GCP_get_Info( GDAL_GCP *h ) {
  return h->pszInfo;
}
void GDAL_GCP_set_Info( GDAL_GCP *h, const char * val ) {
  if ( h->pszInfo ) 
    CPLFree( h->pszInfo );
  h->pszInfo = CPLStrdup(val);
}
const char * GDAL_GCP_get_Id( GDAL_GCP *h ) {
  return h->pszId;
}
void GDAL_GCP_set_Id( GDAL_GCP *h, const char * val ) {
  if ( h->pszId ) 
    CPLFree( h->pszId );
  h->pszId = CPLStrdup(val);
}

%} //%inline 

%apply (IF_FALSE_RETURN_NONE) { (FALSE_IS_ERR) };
FALSE_IS_ERR GDALGCPsToGeoTransform( int nGCPs, GDAL_GCP const * pGCPs, 
    	                             double argout[6], int bApproxOK = 1 ); 
%clear (FALSE_IS_ERR);


//************************************************************************
//
// Define the Dataset object.
//
//************************************************************************
%include "Dataset.i"

//************************************************************************
//
// Define the Band object.
//
//************************************************************************
%include "Band.i"

//************************************************************************
//
// Define the ColorTable object.
//
//************************************************************************
%include "ColorTable.i"

//************************************************************************
//
// Define the global methods
//
//************************************************************************
//
// Missing
//
// GeneralCmdLineProcessor
// TermProgress
//

void GDALAllRegister();

int GDALGetCacheMax();

void GDALSetCacheMax( int nBytes );
    
int GDALGetCacheUsed();
    
int GDALGetDataTypeSize( GDALDataType );

int GDALDataTypeIsComplex( GDALDataType );

const char *GDALGetDataTypeName( GDALDataType );

GDALDataType GDALGetDataTypeByName( const char * );

const char *GDALGetColorInterpretationName( GDALColorInterp );

const char *GDALGetPaletteInterpretationName( GDALPaletteInterp );

const char *GDALDecToDMS( double, const char *, int = 2 );

double GDALPackedDMSToDec( double );

double GDALDecToPackedDMS( double );

CPLXMLNode *CPLParseXMLString( char * );

char *CPLSerializeXMLTree( CPLXMLNode *xmlnode );

//************************************************************************
//
// Define the factory functions for Drivers and Datasets
//
//************************************************************************

// Missing
// GetDriverList

%inline %{
int GetDriverCount() {
  return GDALGetDriverCount();
}
%}

%inline %{
GDALDriverShadow* GetDriverByName( char const *name ) {
  return (GDALDriverShadow*) GDALGetDriverByName( name );
}
%}

%inline %{
GDALDriverShadow* GetDriver( int i ) {
  return (GDALDriverShadow*) GDALGetDriver( i );
}
%}

%newobject Open;
%inline %{
GDALDatasetShadow* Open( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  GDALDatasetShadow *ds = GDALOpen( name, eAccess );
  return (GDALDatasetShadow*) ds;
}
%}

%newobject OpenShared;
%inline %{
GDALDatasetShadow* OpenShared( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  GDALDatasetShadow *ds = GDALOpenShared( name, eAccess );
  return (GDALDatasetShadow*) ds;
}
%}

//************************************************************************
//
// Define Algorithms
//
//************************************************************************

// Missing
// ComputeMedianCutPCT
// DitherRGB2PCT
// RGBFile2PCTFile
// AutoCreateWarpedVRT
// ReprojectImage
// CreateAndReprojectImage
// GCPsToGeoTransform

%newobject AutoCreateWarpedVRT;
%inline %{
GDALDatasetShadow *AutoCreateWarpedVRT( GDALDatasetShadow *src_ds,
                                        const char *src_wkt = 0,
                                        const char *dst_wkt = 0,
                                        GDALResampleAlg eResampleAlg = GRA_NearestNeighbour,
                                        double maxerror = 0.0 ) {
  GDALDatasetShadow *ds = GDALAutoCreateWarpedVRT( src_ds, src_wkt,
                                                   dst_wkt,
                                                   eResampleAlg,
                                                   maxerror,
                                                   0 );
  if (ds == 0) {
    throw CPLGetLastErrorMsg();
  }
  return ds;
  
}
%}
