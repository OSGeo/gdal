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

typedef double double_6[6];
typedef double double_2[2];

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

%include "Dataset.i"

%include "Band.i"

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

%ignore GDALRasterBand;
%ignore GDALDataset;
%ignore GDALDriver;
%include "gcore/gdal_priv.h"
