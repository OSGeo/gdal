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
#include <vector>

#include <algorithm>

#include "cpl_port.h"
#include "cpl_string.h"

#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_alg.h"

char *py_ReadRaster( GDALRasterBand *obj,
                     int xoff, int yoff, int xsize, int ysize,
                     int buf_xsize, int buf_ysize, GDALDataType buf_type );
%}

%import gdal_typemaps.i

//
//  Include gdal.h
//

#define CPL_DLL
#define CPL_C_START extern "C" {
#define CPL_C_END }

// Structures which need to be ignored.
%ignore GDALDriverManager;
%ignore GDALColorInterp;

// Must be ignored to prevent undefined symbol problems.
%ignore GDALGetRasterMetadata;
%ignore GDALDefaultBuildOverviews;
%ignore GDALDatasetAdviseRead;
%ignore GDALRasterAdviseRead;

// Provide definitions for these below.
%ignore GDALGetDriverByName;
%ignore GDALOpen;

%include "gcore/gdal.h"

%pythoncode%{
def AllRegister():
    _gdal.GDALAllRegister()
%}


//
// CPL
/* ==================================================================== */
/*      Support function for error reporting callbacks to python.       */
/* ==================================================================== */

%{

typedef struct _PyErrorHandlerData {
    PyObject *psPyErrorHandler;
    struct _PyErrorHandlerData *psPrevious;
} PyErrorHandlerData;

static PyErrorHandlerData *psPyHandlerStack = NULL;

/************************************************************************/
/*                        PyErrorHandlerProxy()                         */
/************************************************************************/

void PyErrorHandlerProxy( CPLErr eErrType, int nErrorCode, const char *pszMsg )

{
    PyObject *psArgs;
    PyObject *psResult;

    CPLAssert( psPyHandlerStack != NULL );
    if( psPyHandlerStack == NULL )
        return;

    psArgs = Py_BuildValue("(iis)", (int) eErrType, nErrorCode, pszMsg );

    psResult = PyEval_CallObject( psPyHandlerStack->psPyErrorHandler, psArgs);
    Py_XDECREF(psArgs);

    if( psResult != NULL )
    {
        Py_XDECREF( psResult );
    }
}

/************************************************************************/
/*                        CPLPushErrorHandler()                         */
/************************************************************************/
static PyObject *
py_CPLPushErrorHandler(PyObject *self, PyObject *args) {

    PyObject *psPyCallback = NULL;
    PyErrorHandlerData *psCBData = NULL;
    char *pszCallbackName = NULL;
    CPLErrorHandler pfnHandler = NULL;

    self = self;

    if(!PyArg_ParseTuple(args,"O:CPLPushErrorHandler",	&psPyCallback ) )
        return NULL;

    psCBData = (PyErrorHandlerData *) CPLCalloc(sizeof(PyErrorHandlerData),1);
    psCBData->psPrevious = psPyHandlerStack;
    psPyHandlerStack = psCBData;

    if( PyArg_Parse( psPyCallback, "s", &pszCallbackName ) )
    {
        if( EQUAL(pszCallbackName,"CPLQuietErrorHandler") )
	    pfnHandler = CPLQuietErrorHandler;
        else if( EQUAL(pszCallbackName,"CPLDefaultErrorHandler") )
	    pfnHandler = CPLDefaultErrorHandler;
        else if( EQUAL(pszCallbackName,"CPLLoggingErrorHandler") )
            pfnHandler = CPLLoggingErrorHandler;
        else
        {
	    PyErr_SetString(PyExc_ValueError,
   	            "Unsupported callback name in CPLPushErrorHandler");
            return NULL;
        }
    }
    else
    {
	PyErr_Clear();
	pfnHandler = PyErrorHandlerProxy;
        psCBData->psPyErrorHandler = psPyCallback;
        Py_INCREF( psPyCallback );
    }

    CPLPushErrorHandler( pfnHandler );

    Py_INCREF(Py_None);
    return Py_None;
}

%}

%native(CPLPushErrorHandler) py_CPLPushErrorHandler;

%ignore CPLPushErrorHandler;
%include "port/cpl_error.h"
%include "port/cpl_conv.h"

%pythoncode %{
def Debug(msg_class, message):
    _gdal.CPLDebug( msg_class, message )

def Error(err_class = CE_Failure, err_code = CPLE_AppDefined, msg = 'error' ):
    _gdal.CPLError( err_class, err_code, msg )

def ErrorReset():
    _gdal.CPLErrorReset()

def GetLastErrorNo():
    return _gdal.CPLGetLastErrorNo()
    
def GetLastErrorType():
    return _gdal.CPLGetLastErrorType()
    
def GetLastErrorMsg():
    return _gdal.CPLGetLastErrorMsg()

def PushErrorHandler( handler = "CPLQuietErrorHandler" ):
    _gdal.CPLPushErrorHandler( handler )

def PopErrorHandler():
    _gdal.CPLPopErrorHandler()

def PushFinderLocation( x ):
    _gdal.CPLPushFinderLocation( x )

def PopFinderLocation():
    _gdal.CPLPopFinderLocation()

def FinderClean():
    _gdal.CPLFinderClean()

def FindFile( classname, basename ):
    return _gdal.CPLFindFile( classname, basename )

def SetConfigOption( name, value ):
    _gdal.CPLSetConfigOption( name, value )

def GetConfigOption( name, default ):
    return _gdal.CPLGetConfigOption( name, default )

def ParseXMLString( text ):
    return _gdal.CPLParseXMLString( text )
    
def SerializeXMLTree( tree ):
    return _gdal.CPLSerializeXMLTree( tree )


%}

%include "Driver.i"

//************************************************************************
//
// Define the extensions for Dataset (nee GDALDataset)
//
//************************************************************************
%rename (Dataset) GDALDataset;

%ignore GDALDataset::AdviseRead;

%ignore GDALDataset::GetGeoTransform( double * );
%ignore GDALDataset::SetGeoTransform( double * );

%ignore GDALDataset::GetGCPs();

%ignore GDALDataset::BuildOverviews( const char *, int, int*, int, int*, GDALProgressFunc, void *);
%feature("kwargs") GDALDataset::BuildOverviews;


%apply (char **dict) { char **papszMetadata };
%apply (int nList, int* pList) { (int overviewlist, int *pOverviews) };

%extend GDALDataset {

%immutable;
  int RasterXSize;
  int RasterYSize;
%mutable;

  char const *GetProjection() {
    return self->GetProjectionRef();
  }

  std::vector<double>
  GetGeoTransform() {
    double c_transform[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    std::vector<double> retval(6);
    if ( GDALGetGeoTransform( self, c_transform ) == 0 ) {
      retval[1] = 1.0;
      retval[5] = 1.0;
    }
    else {
      std::copy( c_transform, c_transform+6, retval.begin() );
    }
    return retval;
  }

  int SetGeoTransform( std::vector<double> trans ) {
    double c_transform[6];
    std::copy( trans.begin(), trans.begin()+6, c_transform );
    return GDALSetGeoTransform( self, c_transform );
  }

  // The int,int* arguments are typemapped.  The name of the first argument
  // becomes the kwarg name for it.
  int BuildOverviews( const char *resampling = "NEAREST", int overviewlist = 0 , int *pOverviews = 0 ) {
    return GDALBuildOverviews( self, resampling, overviewlist, pOverviews, 0, 0, 0, 0);
  }

  void GetGCPs( int *nGCPs, GDAL_GCP const **pGCPs ) {
    *nGCPs = self->GetGCPCount();
    *pGCPs = self->GetGCPs();
  }

};

%{
int GDALDataset_RasterXSize_get( GDALDataset *h ) {
  return GDALGetRasterXSize( h );
}
int GDALDataset_RasterYSize_get( GDALDataset *h ) {
  return GDALGetRasterYSize( h );
}
%}

//************************************************************************
//
// Define the extensions for Band (nee GDALRasterBand)
//
//************************************************************************
%{
char *py_ReadRaster( GDALRasterBand *obj,
                     int xoff, int yoff, int xsize, int ysize,
                     int buf_xsize, int buf_ysize, GDALDataType buf_type )
{

  int result_size = buf_xsize * buf_ysize * GDALGetDataTypeSize( buf_type ) / 8;
  void * result = malloc( result_size );
  if ( GDALRasterIO( obj, GF_Read, xoff, yoff, xsize, ysize,
                result, buf_xsize, buf_ysize, buf_type, 0, 0 ) != CE_None ) {
    free( result );
    result = 0;
  }
  return (char*)result;
}
%}

%rename (Band) GDALRasterBand;

%ignore GDALRasterBand::SetOffset;
%ignore GDALRasterBand::SetScale;
%ignore GDALRasterBand::AdviseRead;

%newobj GDALRasterBand::ReadRaster;

%extend GDALRasterBand {

%immutable;
  int XSize;
  int YSize;
  GDALDataType DataType;
%mutable;

  int Checksum( int xoff, int yoff, int xsize, int ysize) {
    return GDALChecksumImage( self, xoff, yoff, xsize, ysize );
  }

  int Checksum( int xoff = 0, int yoff = 0 ) {
    int xsize = GDALGetRasterBandXSize( self );
    int ysize = GDALGetRasterBandYSize( self );
    return GDALChecksumImage( self, xoff, yoff, xsize, ysize );
  }

  std::vector<double> ComputeRasterMinMax( int approx_ok = 0 ) {
    double c_minmax[2] = {0.0, 0.0};
    GDALComputeRasterMinMax( self, approx_ok, c_minmax );
    std::vector<double> retval(2);
    retval[0] = c_minmax[0];
    retval[1] = c_minmax[1];
    return retval;
  }

  char *ReadRaster( int xoff, int yoff, int xsize, int ysize,
                    int buf_xsize, int buf_ysize, GDALDataType buf_type ) {
    return py_ReadRaster( self, xoff, yoff, xsize, ysize,
                           buf_xsize, buf_ysize, buf_type );
  }

  char *ReadRaster( int xoff, int yoff, int xsize, int ysize,
                    int buf_xsize, int buf_ysize ) {
    return py_ReadRaster( self, xoff, yoff, xsize, ysize,
                           buf_xsize, buf_ysize, GDALGetRasterDataType(self) );
  }

  char *ReadRaster( int xoff, int yoff, int xsize, int ysize ) {
    return py_ReadRaster( self, xoff, yoff, xsize, ysize,
                           xsize, ysize, GDALGetRasterDataType(self) );
  }

};

%{
GDALDataType GDALRasterBand_DataType_get( GDALRasterBand *h ) {
  return GDALGetRasterDataType( h );
}
int GDALRasterBand_XSize_get( GDALRasterBand *h ) {
  return GDALGetRasterBandXSize( h );
}
int GDALRasterBand_YSize_get( GDALRasterBand *h ) {
  return GDALGetRasterBandYSize( h );
}
%}

//************************************************************************
//
// Define the extensions for ColorTable (nee GDALColorTable)
//
//************************************************************************
%rename (ColorTable) GDALColorTable;

//************************************************************************
//
// Define the factory functions for Drivers and Datasets
//
//************************************************************************

%inline %{
GDALDriver* GetDriverByName( char const *name ) {
  return (GDALDriver*) GDALGetDriverByName( name );
}
%}

%newobject Open;
%inline %{
GDALDataset* Open( char const* name, GDALAccess eAccess = GA_ReadOnly ) {
  return (GDALDataset*) GDALOpen( name, eAccess );
}
%}

%ignore GDALDriver;
%include "gcore/gdal_priv.h"
