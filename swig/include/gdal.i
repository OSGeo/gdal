
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

//
// Typemap for counted arrays of ints <- PySequence
//
%typemap(in,numargs=1) (int nList, int* pList)
{
  // %typemap(in,numargs=1) (int nList, int* pList)
  /* check if is List */
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  $1 = PySequence_Size($input);
  $2 = (int*) malloc($1*sizeof(int));
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    if ( !PyArg_Parse(o,"i",&$2[i]) ) {
      SWIG_fail;
    }
  }
}
%typemap(freearg) (int nList, int* pList)
{
  // %typemap(freearg) (int nList, int* pList)
  if ($2) {
    free((void*) $2);
  }
}
//
// Typemap for vector<double> <-> PyTuple.
//
%typemap(out) std::vector<double>
{
   // %typemap(out) std::vector<double>
   $result = PyTuple_New($1.size());
   for (unsigned int i=0; i<$1.size(); i++) {
      PyTuple_SetItem($result,i, PyFloat_FromDouble((($1_type &)$1)[i]));
   }
}

%typemap(in) std::vector<double>
{
   // %typemap(in) std::vector<double>
   if (! PySequence_Check($input) ) {
     PyErr_SetString(PyExc_TypeError, "not a sequence");
     SWIG_fail;
   }
   int size = PySequence_Size($input);
   for (unsigned int i=0; i<size; i++) {
     PyObject *o = PySequence_GetItem($input,i);
     double val;
     PyArg_ParseTuple(o, "d", &val );
     $1.push_back( val );
   }
}

//
// Typemap argout of GDAL_GCP* used in Dataset::GetGCPs( )
//
%typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) (int nGCPs, GDAL_GCP *pGCPs )
{
  // %typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs )
  $1 = &nGCPs;
  $2 = &pGCPs;
}
%typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs )
{
  // %typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs )
  PyObject *dict = PyTuple_New( *$1 );
  for( int i = 0; i < *$1; i++ ) {
    PyTuple_SetItem(dict, i, 
      Py_BuildValue("(ssddddd)", 
                    (*$2)[i].pszId,
                    (*$2)[i].pszInfo,
                    (*$2)[i].dfGCPPixel,
                    (*$2)[i].dfGCPLine,
                    (*$2)[i].dfGCPX,
                    (*$2)[i].dfGCPY,
                    (*$2)[i].dfGCPZ ) );
  }
cout << "leaving" << endl;
  Py_DECREF($result);
  $result = dict;
}

//
// Typemap for GDALColorEntry* <-> tuple
//
%typemap(out) GDALColorEntry*
{
  // %typemap(out) GDALColorEntry*
   $result = Py_BuildValue( "(hhhh)", (*$1).c1,(*$1).c2,(*$1).c3,(*$1).c4);
}

%typemap(in) GDALColorEntry*
{
  // %typemap(in) GDALColorEntry*
   
   GDALColorEntry ce = {255,255,255,255};
   int size = PySequence_Size($input);
   if ( size > 4 ) {
     PyErr_SetString(PyExc_TypeError, "sequence too long");
     SWIG_fail;
   }
   PyArg_ParseTuple( $input,"hhh|h", &ce.c1, &ce.c2, &ce.c3, &ce.c4 );
   $1 = &ce;
}

//
// Typemap char ** -> dict
//
%typemap(out) char **
{
  // %typemap(out) char ** -> to hash
  char **valptr = $1;
  $result = PyDict_New();
  if ( valptr != NULL ) {
    while (*valptr != NULL ) {
      char *equals = index( *valptr, '=' );
      PyObject *nm = PyString_FromStringAndSize( *valptr, equals-*valptr );
      PyObject *val = PyString_FromString( equals+1 );
      PyDict_SetItem($result, nm, val );
      valptr++;
    }
  }
}

//
// Typemap char **<- dict
//
%typemap(in) char **dict
{
  // %typemap(in) char **dict
  if ( ! PyMapping_Check( $input ) ) {
    PyErr_SetString(PyExc_TypeError,"not supports mapping (dict) protocol");
    SWIG_fail;
  }
  $1 = NULL;
  int size = PyMapping_Length( $input );
  if ( size > 0 ) {
    PyObject *item_list = PyMapping_Items( $input );
    for( int i=0; i<size; i++ ) {
      PyObject *it = PySequence_GetItem( item_list, i );
      char *nm;
      char *val;
      PyArg_ParseTuple( it, "ss", &nm, &val );
      $1 = CSLAddNameValue( $1, nm, val );
    }
  }
}
%typemap(freearg) char **dict
{
  // %typemap(freearg) char **dict
  CSLDestroy( $1 );
}

//
// Typemap maps char** arguments from Python Sequence Object
//
%typemap(in) char **options
{
  // %typemap(in) char **options
  /* Check if is a list */
  if ( ! PySequence_Check($input)) {
    PyErr_SetString(PyExc_TypeError,"not a sequence");
    SWIG_fail;
  }

  int size = PySequence_Size($input);
  for (int i = 0; i < size; i++) {
    char *pszItem = NULL;
    if ( ! PyArg_Parse( PySequence_GetItem($input,i), "s", &pszItem ) ) {
      PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
      SWIG_fail;
    }
    $1 = CSLAddString( $1, pszItem );
  }
}
%typemap(freearg) char **options
{
  // %typemap(freearg) char **options
  CSLDestroy( $1 );
}

//
//  Include gdal.h
//

#define CPL_DLL
#define CPL_C_START extern "C" {
#define CPL_C_END }

// Structures which need to be ignored.
%ignore GDALDriverManager;

// Must be ignored to prevent undefined symbol problems.
%ignore GDALGetRasterMetadata;
%ignore GDALDefaultBuildOverviews;

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

//************************************************************************
//
// Define the extensions for Driver (nee GDALDriver)
//
//************************************************************************
%rename (Driver) GDALDriver;
%newobject GDALDriver::Create;
%feature( "kwargs" ) GDALDriver::Create;
%newobject GDALDriver::CreateCopy;
%feature( "kwargs" ) GDALDriver::CreateCopy;
%extend GDALDriver {

  GDALDataset *Create( const char *name, int xsize, int ysize, int bands =1,
                       GDALDataType eType=GDT_Byte, char **options = 0 ) {
    GDALDataset* ds = self->Create( name, xsize, ysize, bands, eType, options );
    if ( ds != 0 )
      GDALDereferenceDataset( ds );
    return ds;
  }

  GDALDataset *CreateCopy( const char *name, GDALDataset* src, int strict =1, char **options = 0 ) {
    GDALDataset *ds = self->CreateCopy( name, src, strict, 0, 0, 0 );
    if ( ds != 0 )
      GDALDereferenceDataset( ds );
    return ds;
  }

};
%ignore GDALDriver::Create;
%ignore GDALDriver::CreateCopy;

//************************************************************************
//
// Define the extensions for Dataset (nee GDALDataset)
//
//************************************************************************
%rename (Dataset) GDALDataset;

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

%include "gcore/gdal_priv.h"
