/******************************************************************************
 * $Id$
 *
 * Name:     typemaps_php.i
 * Project:  GDAL PHP Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

*/

/*
 * The typemaps defined here use the code fragment called:
 * t_out_helper which is defined in the pytuplehlp.swg file.
 * The *.swg library files are considered "swig internal".
 * fortunately, pytuplehlp.swg is included by typemaps.i
 * which we need anyway.
 */

/*
 * Include the typemaps from swig library for returning of
 * standard types through arguments.
 */
%include "typemaps.i"

%apply (double *OUTPUT) { double *argout };

/*
 *
 * Define a simple return code typemap which checks if the return code from
 * the wrapped method is non-zero. If non-zero, return None.  Otherwise,
 * return any argout or None.
 *
 * Applied like this:
 * %apply (IF_ERR_RETURN_NONE) {CPLErr};
 * CPLErr function_to_wrap( );
 * %clear (CPLErr);
 */
%typemap(out) IF_ERR_RETURN_NONE
{
  /* %typemap(out) IF_ERR_RETURN_NONE */
  result = 0;
}
%typemap(ret) IF_ERR_RETURN_NONE
{
 /* %typemap(ret) IF_ERR_RETURN_NONE */
}
%typemap(out) IF_FALSE_RETURN_NONE
{
  /* %typemap(out) IF_FALSE_RETURN_NONE */
  $result = 0;
}
%typemap(ret) IF_FALSE_RETURN_NONE
{
 /* %typemap(ret) IF_FALSE_RETURN_NONE */
}

/*
 * Another output typemap which will raise an
 * exception on error.  If there is no error,
 * and no other argout typemaps create a return value,
 * then it will return 0.
 */
%fragment("OGRErrMessages","header") %{
static char const *
OGRErrMessages( int rc ) {
  switch( rc ) {
  case 0:
    return "OGR Error %d: None";
  case 1:
    return "OGR Error %d: Not enough data";
  case 2:
    return "OGR Error %d: Unsupported geometry type";
  case 3:
    return "OGR Error %d: Unsupported operation";
  case 4:
    return "OGR Error %d: Corrupt data";
  case 5:
    return "OGR Error %d: General Error";
  case 6:
    return "OGR Error %d: Unsupported SRS";
  default:
    return "OGR Error %d: Unknown";
  }
}
%}
%typemap(php, out,fragment="OGRErrMessages") OGRErr
{
  /* %typemap(out) OGRErr */
}
%typemap(php, ret) OGRErr
{
  /* %typemap(ret) OGRErr */
}

/*
 * SWIG macro to define fixed length array typemaps
 * defines three different typemaps.
 *
 * 1) For argument in.  The wrapped function's prototype is:
 *
 *    FunctionOfDouble3( double *vector );
 *
 *    The function assumes that vector points to three consecutive doubles.
 *    This can be wrapped using:
 * 
 *    %apply (double_3 argin) { (double *vector) };
 *    FunctionOfDouble3( double *vector );
 *    %clear (double *vector);
 *
 *    Example:  Dataset.SetGeoTransform().
 *
 * 2) Functions which modify a fixed length array passed as
 *    an argument or return data in an array allocated by the
 *    caller.
 *
 *    %apply (double_6 argout ) { (double *vector) };
 *    GetVector6( double *vector );
 *    %clear ( double *vector );
 *
 *    Example:  Dataset.GetGeoTransform().
 *
 * 3) Functions which take a double **.  Through this argument it
 *    returns a pointer to a fixed size array allocated with CPLMalloc.
 *
 *    %apply (double_17 *argoug) { (double **vector) };
 *    ReturnVector17( double **vector );
 *    %clear ( double **vector );
 *   
 *    Example:  SpatialReference.ExportToPCI().
 *
 */

%fragment("CreateTupleFromDoubleArray","header") %{
/*
 * static PyObject *
 * CreateTupleFromDoubleArray( double *first, unsigned int size ) {
 *  PyObject *out = PyTuple_New( size );
 *  for( unsigned int i=0; i<size; i++ ) {
 *    PyObject *val = PyFloat_FromDouble( *first );
 *    ++first;
 *    PyTuple_SetItem( out, i, val );
 *  }
 *  return out;
 * }
 */
%}

%define ARRAY_TYPEMAP(size)
%typemap(in,numinputs=0) ( double_ ## size argout) (double argout[size])
{
  /* %typemap(in,numinputs=0) (double_ ## size argout) */
  $1 = argout;
}
%typemap(argout,fragment="CreateTupleFromDoubleArray") ( double_ ## size argout)
{
  /* %typemap(argout) (double_ ## size argout) */
}
%typemap(in,numinputs=0) ( double_ ## size *argout) (double *argout)
{
  /* %typemap(in,numinputs=0) (double_ ## size *argout) */
}
%typemap(argout,fragment="CreateTupleFromDoubleArray") ( double_ ## size *argout)
{
  /* %typemap(argout) (double_ ## size *argout) */
}
%typemap(freearg) (double_ ## size *argout)
{
  /* %typemap(freearg) (double_ ## size *argout) */
  CPLFree(*$1);
}
%typemap(in) (double_ ## size argin) (double argin[size])
{
  /* %typemap(in) (double_ ## size argin) */
}
%enddef

/*
 * Typemap for double c_minmax[2]. 
 * Used in Band::ComputeMinMax()
 */
ARRAY_TYPEMAP(2);

/*
 * Typemap for double[4]
 * Used in OGR::Geometry::GetEnvelope
 */
ARRAY_TYPEMAP(4);

/*
 * Typemap for double c_transform[6]
 * Used in Dataset::GetGeoTransform(), Dataset::SetGeoTransform().
 */
ARRAY_TYPEMAP(6);

// Used by SpatialReference
ARRAY_TYPEMAP(7);
ARRAY_TYPEMAP(15);
ARRAY_TYPEMAP(17);

// Used by CoordinateTransformation::TransformPoint()
ARRAY_TYPEMAP(3);

/*
 *  Typemap for counted arrays of ints <- PySequence
 */
%typemap(in,numinputs=1) (int nList, int* pList)
{
  /* %typemap(in,numinputs=1) (int nList, int* pList)*/
  /* check if is List */
}
%typemap(freearg) (int nList, int* pList)
{
  /* %typemap(freearg) (int nList, int* pList) */
}

/*
 * Typemap for buffers with length <-> PyStrings
 * Used in Band::ReadRaster() and Band::WriteRaster()
 *
 * This typemap has a typecheck also since the WriteRaster()
 * methods are overloaded.
 */
%typemap(in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen = 0, char *pBuf = 0 )
{
  /* %typemap(in,numinputs=0) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}
%typemap(argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */
}
%typemap(freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
}
%typemap(in,numinputs=1) (int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER)
        (int nLen, char *pBuf)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (int nLen, char *pBuf) */
}

/*
 * Typemap argout of GDAL_GCP* used in Dataset::GetGCPs( )
 */
%typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) (int nGCPs=0, GDAL_GCP *pGCPs=0 )
{
  /* %typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  $1 = &nGCPs;
  $2 = &pGCPs;
}
%typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs )
{
  /* %typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs ) */
}
%typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) ( GDAL_GCP *tmpGCPList )
{
  /* %typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) */
}
%typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs )
{
  /* %typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs ) */
}

/*
 * Typemap for GDALColorEntry* <-> tuple
 */
%typemap(out) GDALColorEntry*
{
  /* %typemap(out) GDALColorEntry* */
}

%typemap(in) GDALColorEntry*
{
  /* %typemap(in) GDALColorEntry* */
}

/*
 * Typemap char ** -> dict
 */
%typemap(out) char **dict
{
  /* %typemap(out) char **dict */
}

/*
 * Typemap char **<- dict.  This typemap actually supports lists as well,
 * Then each entry in the list must be a string and have the form:
 * "name=value" so gdal can handle it.
 */
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (char **dict)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (char **dict) */
}
%typemap(in) char **dict
{
  /* %typemap(in) char **dict */
}
%typemap(freearg) char **dict
{
  /* %typemap(freearg) char **dict */
}

/*
 * Typemap maps char** arguments from Python Sequence Object
 */
%typemap(in) char **options
{
  /* %typemap(in) char **options */
}
%typemap(freearg) char **options
{
  /* %typemap(freearg) char **options */
}
%typemap(out) char **options
{
}

/*
 * Typemaps map mutable char ** arguments from PyStrings.  Does not
 * return the modified argument
 */
%typemap(in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
}

/*
 * Typemap for char **argout.
 */
%typemap(in,numinputs=0) (char **argout) ( char *argout=0 )
{
  /* %typemap(in,numinputs=0) (char **argout) */
}
%typemap(argout) (char **argout)
{
  /* %typemap(argout) (char **argout) */
}
%typemap(freearg) (char **argout)
{
}

/*
 * Typemap for an optional POD argument.
 * Declare function to take POD *.  If the parameter
 * is NULL then the function needs to define a default
 * value.
 */
%define OPTIONAL_POD(type,argstring)
%typemap(in) (type *optional_##type) ( type val )
{
  /* %typemap(in) (type *optional_##type) */
}
%typemap(typecheck,precedence=0) (type *optional_##type)
{
  /* %typemap(typecheck,precedence=0) (type *optionalInt) */
}
%enddef

OPTIONAL_POD(int,i);

/*
 * Typedef const char * <- Any object.
 *
 * Formats the object using str and returns the string representation
 */

%typemap(in) (tostring argin) ()
{
  /* %typemap(in) (tostring argin) */
}
%typemap(freearg)(tostring argin)
{
  /* %typemap(freearg) (tostring argin) */
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin)
{
  /* %typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin) */
}

/*
 * Typemap for CPLErr.
 * This typemap will use the wrapper C-variable
 * int UseExceptions to determine proper behavour for
 * CPLErr return codes.
 * If UseExceptions ==0, then return the rc.
 * If UseExceptions ==1, then if rc >= CE_Failure, raise an exception.
 */
%typemap(arginit) CPLErr
{
  /* %typemap(arginit) CPLErr */
}
%typemap(out) CPLErr
{
  /* %typemap(out) CPLErr */
}
%typemap(ret) CPLErr
{
  /* %typemap(ret) CPLErr */
}
