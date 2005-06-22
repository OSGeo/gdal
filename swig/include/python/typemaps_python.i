/******************************************************************************
 * $Id$
 *
 * Name:     typemaps_python.i
 * Project:  GDAL Python Interface
 * Purpose:  GDAL Core SWIG Interface declarations.
 * Author:   Kevin Ruland, kruland@ku.edu
 *

 *
 * $Log$
 * Revision 1.28  2005/06/22 18:46:22  kruland
 * Be consistant about using 'python' in %typemap decls.
 * Removed references to 'python' in the %typemap comment strings to improve greps.
 * Use $result instead of resultobj.
 * Renamed type for OGRErr out typemap to OGRErr instead of THROW_OGR_ERROR.
 * First cut at CPLErr out typemap which uses the bUseExceptions flag.
 *
 * Revision 1.27  2005/02/24 18:37:20  kruland
 * Moved the c# typemaps to its own file.
 *
 * Revision 1.26  2005/02/24 17:35:15  hobu
 * add the python name to the THROW_OGR_ERROR typemap
 *
 * Revision 1.25  2005/02/24 16:36:31  kruland
 * Added IF_FALSE_RETURN_NONE typemap for GCPsToGeoTransform method.
 * Changed GCPs typemaps to use new object rather than raw tuple.
 * Added out typemap for char **s.  Currently used in osr.GetProjectionMethods.
 *
 * Revision 1.24  2005/02/24 16:13:57  hobu
 * freearg patch for tostring argin typemap
 * Added dummy typemaps (just copied the python ones)
 * for the typemaps used in OGR for C#
 *
 * Revision 1.23  2005/02/23 21:01:10  hobu
 * swap the decref with the asstring in the
 * tostring argin typemap
 *
 * Revision 1.22  2005/02/23 17:45:35  kruland
 * Change the optional_int macro to perform a cast to support the integer
 * typedefs such as GDALDataType.
 *
 * Revision 1.21  2005/02/22 15:36:17  kruland
 * Added ARRAY_TYPEMAP(4) for ogr.Geometry.GetEnvelope().
 * Added a char* typemap (tostring argin), which calls str() on its argument
 * to coerce into a string representation.
 *
 * Revision 1.20  2005/02/21 19:03:17  kruland
 * Use Py_XDECREF() in the argout buffer typemap.
 * Added a convienence fragment for constructing python sequences from integer
 * arrays.
 *
 * Revision 1.19  2005/02/20 19:43:33  kruland
 * Implement another argout typemap for fixed length double arrays.
 *
 * Revision 1.18  2005/02/18 19:34:08  hobu
 * typo in OGRErrMessages
 *
 * Revision 1.17  2005/02/18 17:59:20  kruland
 * Added nicely worded OGR exception mapping mechanism.
 *
 * Revision 1.16  2005/02/18 17:28:07  kruland
 * Fixed bugs in THROW_OGR_ERROR typemap.  When no error is found, and no
 * argouts, return None.
 * Fixed bug in IF_ERR_RETURN_NONE typemap to return None if there are no
 * argouts to return.
 *
 * Revision 1.15  2005/02/18 16:54:35  kruland
 * Removed IGNORE_RC exception macro.
 * Removed fragments.i %include because it's not included in swig 1.3.24.
 * Defined out typemap IF_ERR_RETURN_NONE.
 * Defined out typemap THROW_OGR_ERROR.  (untested).
 *
 * Revision 1.14  2005/02/17 21:14:48  kruland
 * Use swig library's typemaps.i and fragments.i to support returning
 * multiple argument values as a tuple.  Use this in all the custom
 * argout typemaps.
 *
 * Revision 1.13  2005/02/17 17:27:13  kruland
 * Changed the handling of fixed size double arrays to make it fit more
 * naturally with GDAL/OSR usage.  Declare as typedef double * double_17;
 * If used as return argument use:  function ( ... double_17 argout ... );
 * If used as value argument use: function (... double_17 argin ... );
 *
 * Revision 1.12  2005/02/17 03:42:10  kruland
 * Added macro to define typemaps for optional arguments to functions.
 * The optional argument must be coded as a pointer in the function decl.
 * The function must properly interpret a null pointer as default.
 *
 * Revision 1.11  2005/02/16 17:49:40  kruland
 * Added in typemap for Lists of GCPs.
 * Added 'python' to all the freearg typemaps too.
 *
 * Revision 1.10  2005/02/16 17:18:03  hobu
 * put "python" name on the typemaps that are specific
 * to python
 *
 * Revision 1.9  2005/02/16 16:53:45  kruland
 * Minor comment change.
 *
 * Revision 1.8  2005/02/15 20:53:11  kruland
 * Added typemap(in) char ** from PyString which allows the pointer (to char*)
 * to change but assumes the contents do not change.
 *
 * Revision 1.7  2005/02/15 19:49:42  kruland
 * Added typemaps for arbitrary char* buffers with length.
 *
 * Revision 1.6  2005/02/15 17:05:13  kruland
 * Use CPLParseNameValue instead of strchr() in typemap(out) char **dict.
 *
 * Revision 1.5  2005/02/15 16:52:41  kruland
 * Added a swig macro for handling fixed length double array arguments.  Used
 * for Band::ComputeMinMax( double[2] ), and Dataset::?etGeoTransform() methods.
 *
 * Revision 1.4  2005/02/15 06:00:28  kruland
 * Fixed critical bug in %typemap(in) std::vector<double>.  Used incorrect python
 * parse call.
 * Removed some stray cout's.
 * Gave the "argument" to the %typemap(out) char** mapping.  This makes it easier
 * to control its application.
 *
 * Revision 1.3  2005/02/14 23:56:02  hobu
 * Added log info and C99-style comments
 *
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
%typemap(python,out) IF_ERR_RETURN_NONE
{
  /* %typemap(out) IF_ERR_RETURN_NONE */
  result = 0;
}
%typemap(python,ret) IF_ERR_RETURN_NONE
{
 /* %typemap(ret) IF_ERR_RETURN_NONE */
  if (result != 0 ) {
    Py_XDECREF( resultobj );
    resultobj = Py_None;
    Py_INCREF(resultobj);
  }
  if (resultobj == 0) {
    resultobj = Py_None;
    Py_INCREF(resultobj);
  }
}
%typemap(python,out) IF_FALSE_RETURN_NONE
{
  /* %typemap(out) IF_FALSE_RETURN_NONE */
  $result = 0;
}
%typemap(python,ret) IF_FALSE_RETURN_NONE
{
 /* %typemap(ret) IF_FALSE_RETURN_NONE */
  if ($1 == 0 ) {
    Py_XDECREF( $result );
    $result = Py_None;
    Py_INCREF($result);
  }
  if ($result == 0) {
    $result = Py_None;
    Py_INCREF($result);
  }
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
%typemap(python, out,fragment="OGRErrMessages") OGRErr
{
  /* %typemap(out) OGRErr */
  resultobj = 0;
  if ( result != 0) {
    PyErr_Format( PyExc_RuntimeError, OGRErrMessages(result), result );
    SWIG_fail;
  }
}
%typemap(python, ret) OGRErr
{
  /* %typemap(ret) OGRErr */
  if (resultobj == Py_None ) {
    Py_DECREF(resultobj);
    resultobj = 0;
  }
  if (resultobj == 0) {
    resultobj = PyInt_FromLong( 0 );
  }
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
static PyObject *
CreateTupleFromDoubleArray( double *first, unsigned int size ) {
  PyObject *out = PyTuple_New( size );
  for( unsigned int i=0; i<size; i++ ) {
    PyObject *val = PyFloat_FromDouble( *first );
    ++first;
    PyTuple_SetItem( out, i, val );
  }
  return out;
}
%}

%define ARRAY_TYPEMAP(size)
%typemap(python,in,numinputs=0) ( double_ ## size argout) (double argout[size])
{
  /* %typemap(in,numinputs=0) (double_ ## size argout) */
  $1 = argout;
}
%typemap(python,argout,fragment="t_output_helper,CreateTupleFromDoubleArray") ( double_ ## size argout)
{
  /* %typemap(argout) (double_ ## size argout) */
  PyObject *out = CreateTupleFromDoubleArray( $1, size );
  $result = t_output_helper($result,out);
}
%typemap(python,in,numinputs=0) ( double_ ## size *argout) (double *argout)
{
  /* %typemap(in,numinputs=0) (double_ ## size *argout) */
  $1 = &argout;
}
%typemap(python,argout,fragment="t_output_helper,CreateTupleFromDoubleArray") ( double_ ## size *argout)
{
  /* %typemap(argout) (double_ ## size *argout) */
  PyObject *out = CreateTupleFromDoubleArray( *$1, size );
  $result = t_output_helper($result,out);
}
%typemap(python,freearg) (double_ ## size *argout)
{
  /* %typemap(freearg) (double_ ## size *argout) */
  CPLFree(*$1);
}
%typemap(python,in) (double_ ## size argin) (double argin[size])
{
  /* %typemap(in) (double_ ## size argin) */
  $1 = argin;
  if (! PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  int seq_size = PySequence_Size($input);
  if ( seq_size != size ) {
    PyErr_SetString(PyExc_TypeError, "sequence must have length ##size");
    SWIG_fail;
  }
  for (unsigned int i=0; i<size; i++) {
    PyObject *o = PySequence_GetItem($input,i);
    double val;
    PyArg_Parse(o, "d", &val );
    $1[i] =  val;
  }
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
%typemap(python,in,numinputs=1) (int nList, int* pList)
{
  /* %typemap(in,numinputs=1) (int nList, int* pList)*/
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
%typemap(python,freearg) (int nList, int* pList)
{
  /* %typemap(freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
}
%fragment("CreateTupleFromIntegerArray","header") %{
static PyObject *
CreateTupleFromDoubleArray( int *first, unsigned int size ) {
  PyObject *out = PyTuple_New( size );
  for( unsigned int i=0; i<size; i++ ) {
    PyObject *val = PyInt_FromInt( *first );
    ++first;
    PyTuple_SetItem( out, i, val );
  }
  return out;
}
%}

/*
 * Typemap for buffers with length <-> PyStrings
 * Used in Band::ReadRaster() and Band::WriteRaster()
 *
 * This typemap has a typecheck also since the WriteRaster()
 * methods are overloaded.
 */
%typemap(python,in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=0) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}
%typemap(python,argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */
  Py_XDECREF($result);
  $result = PyString_FromStringAndSize( *$2, *$1 );
}
%typemap(python,freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
  if( $1 ) {
    free( *$2 );
  }
}
%typemap(python,in,numinputs=1) (int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
  PyString_AsStringAndSize($input, &$2, &$1 );
}
%typemap(python,typecheck,precedence=SWIG_TYPECHECK_POINTER)
        (int nLen, char *pBuf)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (int nLen, char *pBuf) */
  $1 = (PyString_Check($input)) ? 1 : 0;
}

/*
 * Typemap argout of GDAL_GCP* used in Dataset::GetGCPs( )
 */
%typemap(python,in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) (int nGCPs, GDAL_GCP *pGCPs )
{
  /* %typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  $1 = &nGCPs;
  $2 = &pGCPs;
}
%typemap(python,argout) (int *nGCPs, GDAL_GCP const **pGCPs )
{
  /* %typemap(argout) (int *nGCPs, GDAL_GCP const **pGCPs ) */
  PyObject *dict = PyTuple_New( *$1 );
  for( int i = 0; i < *$1; i++ ) {
    GDAL_GCP *o = new_GDAL_GCP( (*$2)[i].dfGCPX,
                                (*$2)[i].dfGCPY,
                                (*$2)[i].dfGCPZ,
                                (*$2)[i].dfGCPPixel,
                                (*$2)[i].dfGCPLine,
                                (*$2)[i].pszInfo,
                                (*$2)[i].pszId );
	
    PyTuple_SetItem(dict, i, 
       SWIG_NewPointerObj((void*)o,SWIGTYPE_p_GDAL_GCP,1) );
  }
  Py_DECREF($result);
  $result = dict;
}
%typemap(python,in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) ( GDAL_GCP *tmpGCPList )
{
  /* %typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) */
  /* check if is List */
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  $1 = PySequence_Size($input);
  tmpGCPList = (GDAL_GCP*) malloc($1*sizeof(GDAL_GCP));
  $2 = tmpGCPList;
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    GDAL_GCP *item = 0;
    SWIG_Python_ConvertPtr( o, (void**)&item, SWIGTYPE_p_GDAL_GCP, SWIG_POINTER_EXCEPTION | 0 );
    if ( ! item ) {
      SWIG_fail;
    }
    memcpy( (void*) item, (void*) tmpGCPList, sizeof( GDAL_GCP ) );
    ++tmpGCPList;
  }
}
%typemap(python,freearg) (int nGCPs, GDAL_GCP const *pGCPs )
{
  /* %typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs ) */
  if ($2) {
    free( (void*) $2 );
  }
}

/*
 * Typemap for GDALColorEntry* <-> tuple
 */
%typemap(python,out) GDALColorEntry*
{
  /* %typemap(out) GDALColorEntry* */
   $result = Py_BuildValue( "(hhhh)", (*$1).c1,(*$1).c2,(*$1).c3,(*$1).c4);
}

%typemap(python,in) GDALColorEntry*
{
  /* %typemap(in) GDALColorEntry* */
   
   GDALColorEntry ce = {255,255,255,255};
   int size = PySequence_Size($input);
   if ( size > 4 ) {
     PyErr_SetString(PyExc_TypeError, "sequence too long");
     SWIG_fail;
   }
   PyArg_ParseTuple( $input,"hhh|h", &ce.c1, &ce.c2, &ce.c3, &ce.c4 );
   $1 = &ce;
}

/*
 * Typemap char ** -> dict
 */
%typemap(python,out) char **dict
{
  /* %typemap(out) char **dict */
  char **stringarray = $1;
  $result = PyDict_New();
  if ( stringarray != NULL ) {
    while (*stringarray != NULL ) {
      char const *valptr;
      char *keyptr;
      valptr = CPLParseNameValue( *stringarray, &keyptr );
      if ( valptr != 0 ) {
        PyObject *nm = PyString_FromString( keyptr );
        PyObject *val = PyString_FromString( valptr );
        PyDict_SetItem($result, nm, val );
        CPLFree( keyptr );
      }
      stringarray++;
    }
  }
}

/*
 * Typemap char **<- dict
 */
%typemap(python,in) char **dict
{
  /* %typemap(in) char **dict */
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
%typemap(python,freearg) char **dict
{
  /* %typemap(freearg) char **dict */
  CSLDestroy( $1 );
}

/*
 * Typemap maps char** arguments from Python Sequence Object
 */
%typemap(python,in) char **options
{
  /* %typemap(in) char **options */
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
%typemap(python,freearg) char **options
{
  /* %typemap(freearg) char **options */
  CSLDestroy( $1 );
}
%typemap(python,out) char **options
{
  /* %typemap(out) char ** -> ( string ) */
  char **stringarray = $1;
  if ( stringarray == NULL ) {
    $result = Py_None;
    Py_INCREF( $result );
  }
  else {
    int len = CSLCount( stringarray );
    $result = PyTuple_New( len );
    for ( int i = 0; i < len; ++i, ++stringarray ) {
      PyObject *o = PyString_FromString( *stringarray );
      PyTuple_SET_ITEM($result, i, o );
    }
    CSLDestroy( $1 );
  }
}

/*
 * Typemaps map mutable char ** arguments from PyStrings.  Does not
 * return the modified argument
 */
%typemap(python,in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
  PyArg_Parse( $input, "s", &val );
  $1 = &val;
}

/*
 * Typemap for char **argout.
 */
%typemap(python,in,numinputs=0) (char **argout) ( char *argout=0 )
{
  /* %typemap(in,numinputs=0) (char **argout) */
  $1 = &argout;
}
%typemap(python,argout,fragment="t_output_helper") (char **argout)
{
  /* %typemap(argout) (char **argout) */
  PyObject *o;
  if ( $1 ) {
    o = PyString_FromString( *$1 );
  }
  else {
    o = Py_None;
    Py_INCREF( o );
  }
  $result = t_output_helper($result, o);
}
%typemap(python,freearg) (char **argout)
{
  /* %typemap(freearg) (char **argout) */
  if ( *$1 )
    CPLFree( *$1 );
}

/*
 * Typemap for an optional POD argument.
 * Declare function to take POD *.  If the parameter
 * is NULL then the function needs to define a default
 * value.
 */
%define OPTIONAL_POD(type,argstring)
%typemap(python,in) (type *optional_##type) ( type val )
{
  /* %typemap(in) (type *optional_##type) */
  if ( $input == Py_None ) {
    $1 = 0;
  }
  else if ( PyArg_Parse( $input, #argstring ,&val ) ) {
    $1 = ($1_type) &val;
  }
  else {
    PyErr_SetString( PyExc_TypeError, "Invalid Parameter" );
    SWIG_fail;
  }
}
%typemap(python,typecheck,precedence=0) (type *optional_##type)
{
  /* %typemap(typecheck,precedence=0) (type *optionalInt) */
  $1 = (($input==Py_None) || my_PyCheck_##type($input)) ? 1 : 0;
}
%enddef

OPTIONAL_POD(int,i);

/*
 * Typedef const char * <- Any object.
 *
 * Formats the object using str and returns the string representation
 */

%typemap(python,in) (tostring argin) (PyObject *str)
{
  /* %typemap(in) (tostring argin) */
  str = PyObject_Str( $input );
  if ( str == 0 ) {
    PyErr_SetString( PyExc_RuntimeError, "Unable to format argument as string");
    SWIG_fail;
  }
 
  $1 = PyString_AsString(str); 
}
%typemap(python,freearg)(tostring argin)
{
  /* %typemap(freearg) (tostring argin) */
  Py_DECREF(str$argnum);
}
%typemap(python,typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin)
{
  /* %typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin) */
  $1 = 1;
}

/*
 * Typemap for CPLErr.
 * This typemap will use the wrapper C-variable
 * int UseExceptions to determine proper behavour for
 * CPLErr return codes.
 * If UseExceptions ==0, then return the rc.
 * If UseExceptions ==1, then if rc >= CE_Failure, raise an exception.
 */
%typemap(python,out) CPLErr
{
  /* %typemap(out) CPLErr */
  if ( bUseExceptions == 1 && $1 >= CE_Failure ) {
    int errcode = CPLGetLastErrorNo();
    const char *errmsg = CPLGetLastErrorMsg();
    PyErr_Format( PyExc_RuntimeError, "CPLErr %d: %s", errcode, (char*) errmsg );
    SWIG_fail;
  }
}
%typemap(python,ret,fragment="t_output_helper") CPLErr
{
  /* %typemap(ret) CPLErr */
  if ( bUseExceptions == 0 ) {
    $result = t_output_helper($result,SWIG_From_int( $1 ) );
  }
  else {
    /* We're using exceptions.  The test in the out typemap means that
       we know we have a valid return value.  Test if there are any return
       values set by argout typemaps.
    */
    if ( $result == 0 ) {
      /* No other return values set so return None */
      $result = Py_None;
      Py_INCREF($result);
    }
  }
}
