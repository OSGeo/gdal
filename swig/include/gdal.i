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

typedef double *double_2;
typedef double *double_4;
typedef double *double_6;

typedef void GDALDriverShadow;
typedef void GDALDatasetShadow;
typedef void GDALRasterBandShadow;

%}

%feature("compactdefaultargs");
%feature("autodoc");

%import gdal_typemaps.i

typedef int GDALColorInterp;
typedef int GDALAccess;
typedef int GDALDataType;
typedef int CPLErr;

%include "cpl.i"

%include "Driver.i"

%include "Dataset.i"

%include "Band.i"

%include "ColorTable.i"

//************************************************************************
//
// Define the global methods
//
//************************************************************************
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

