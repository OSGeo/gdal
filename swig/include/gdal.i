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

//
// We register all the drivers upon module initialization
//
%init %{
  if ( GDALGetDriverCount() == 0 ) {
    GDALAllRegister();
  }
%}

%pythoncode %{
  from gdalconst import *
%}

%{
#include <iostream>
using namespace std;

#include "cpl_port.h"
#include "cpl_string.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_alg.h"
#include "gdalwarper.h"

typedef double *double_2;
typedef double *double_4;
typedef double *double_6;

typedef void GDALDriverShadow;
typedef void GDALDatasetShadow;
typedef void GDALRasterBandShadow;

typedef int FALSE_IS_ERR;

%}

%feature("compactdefaultargs");
%feature("autodoc");

%import gdal_typemaps.i

typedef int GDALColorInterp;
typedef int GDALAccess;
typedef int GDALDataType;
typedef int CPLErr;
typedef int GDALResampleAlg;

//************************************************************************
//
// Define the exposed CPL functions.
//
//************************************************************************
%include "cpl.i"

//************************************************************************
//
// Define the Driver object.
//
//************************************************************************
%include "Driver.i"

//************************************************************************
//
// Define the Ground Control Point structure.
//
//************************************************************************
%rename (GCP) GDAL_GCP;
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

%feature("kwargs") GDAL_GCP;
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

%pythoncode {
  def __str__(self):
    str = '%s (%.2fP,%.2fL) -> (%.7fE,%.7fN,%.2f) %s '\
          % (self.Id, self.GCPPixel, self.GCPLine,
             self.GCPX, self.GCPY, self.GCPZ, self.Info )
    return str
    def serialize(self,with_Z=0):
        base = [CXT_Element,'GCP']
        base.append([CXT_Attribute,'Id',[CXT_Text,self.Id]])
        pixval = '%0.15E' % self.GCPPixel       
        lineval = '%0.15E' % self.GCPLine
        xval = '%0.15E' % self.GCPX
        yval = '%0.15E' % self.GCPY
        zval = '%0.15E' % self.GCPZ
        base.append([CXT_Attribute,'Pixel',[CXT_Text,pixval]])
        base.append([CXT_Attribute,'Line',[CXT_Text,lineval]])
        base.append([CXT_Attribute,'X',[CXT_Text,xval]])
        base.append([CXT_Attribute,'Y',[CXT_Text,yval]])
        if with_Z:
            base.append([CXT_Attribute,'Z',[CXT_Text,zval]])        
        return base
} /* pythoncode */
} /* extend */
}; /* GDAL_GCP */
%inline {
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
}

%rename (GCPsToGeoTransform) GDALGCPsToGeoTransform;
%apply (IF_FALSE_RETURN_NONE) { (FALSE_IS_ERR) };
FALSE_IS_ERR GDALGCPsToGeoTransform( int nGCPs, GDAL_GCP const * pGCPs, 
    	                             double_6 argout, int bApproxOK = 1 ); 
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
// Majorobject - GetDescription
// Majorobject - SetDescription
//
// GCP - class?  serialize() method missing.

%rename (AllRegister) GDALAllRegister;
void GDALAllRegister();

%rename (GetCacheMax) GDALGetCacheMax;
int GDALGetCacheMax();

%rename (SetCacheMax) GDALSetCacheMax;
void GDALSetCacheMax( int nBytes );
    
%rename (GetCacheUsed) GDALGetCacheUsed;
int GDALGetCacheUsed();
    
%rename (GetDataTypeSize) GDALGetDataTypeSize;
int GDALGetDataTypeSize( GDALDataType );

%rename (DataTypeIsComplex) GDALDataTypeIsComplex;
int GDALDataTypeIsComplex( GDALDataType );

%rename (GetDataTypeName) GDALGetDataTypeName;
const char *GDALGetDataTypeName( GDALDataType );

%rename (GetDataTypeByName) GDALGetDataTypeByName;
GDALDataType GDALGetDataTypeByName( const char * );

%rename (GetColorInterpretationName) GDALGetColorInterpretationName;
const char *GDALGetColorInterpretationName( GDALColorInterp );

%rename (GetPaletteInterpretationName) GDALGetPaletteInterpretationName;
const char *GDALGetPaletteInterpretationName( GDALPaletteInterp );

%rename (DecToDMS) GDALDecToDMS;
const char *GDALDecToDMS( double, const char *, int = 2 );

%rename (PackedDMSToDec) GDALPackedDMSToDec;
double GDALPackedDMSToDec( double );

%rename (DecToPackedDMS) GDALDecToPackedDMS;
double GDALDecToPackedDMS( double );


//************************************************************************
//
// Define the factory functions for Drivers and Datasets
//
//************************************************************************

// Missing
// GetDriverList

%inline %{
GDALDriverShadow* GetDriverByName( char const *name ) {
  return (GDALDriverShadow*) GDALGetDriverByName( name );
}
%}

%newobject Open;
%inline %{
GDALDatasetShadow* Open( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  GDALDatasetShadow *ds = GDALOpen( name, eAccess );
  if ( ds ) 
    GDALReferenceDataset( ds );
  return (GDALDatasetShadow*) ds;
}
%}

%newobject OpenShared;
%inline %{
GDALDatasetShadow* OpenShared( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  GDALDatasetShadow *ds = GDALOpenShared( name, eAccess );
  if ( ds ) 
    GDALReferenceDataset( ds );
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