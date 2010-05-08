/******************************************************************************
 * $Id$
 *
 * Name:     typemaps_python.i
 * Project:  GDAL Python Interface
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
 * double *val, int*hasval, is a special contrived typemap used for
 * the RasterBand GetNoDataValue, GetMinimum, GetMaximum, GetOffset, GetScale methods.
 * In the python bindings, the variable hasval is tested.  If it is 0 (is, the value
 * is not set in the raster band) then Py_None is returned.  If is is != 0, then
 * the value is coerced into a long and returned.
 */
%typemap(in,numinputs=0) (double *val, int*hasval) ( double tmpval, int tmphasval ) {
  /* %typemap(python,in,numinputs=0) (double *val, int*hasval) */
  $1 = &tmpval;
  $2 = &tmphasval;
}
%typemap(argout) (double *val, int*hasval) {
  /* %typemap(python,argout) (double *val, int*hasval) */
  PyObject *r;
  if ( !*$2 ) {
    Py_INCREF(Py_None);
    r = Py_None;
    $result = t_output_helper($result,r);
  }
  else {
    r = PyFloat_FromDouble( *$1 );
    $result = t_output_helper($result,r);
  }
}

/*
 *
 * Define a simple return code typemap which checks if the return code from
 * the wrapped method is non-zero. If zero, return None.  Otherwise,
 * return any argout or None.
 *
 * Applied like this:
 * %apply (IF_FALSE_RETURN_NONE) {int};
 * int function_to_wrap( );
 * %clear (int);
 */
/*
 * The out typemap prevents the default typemap for output integers from
 * applying.
 */
%typemap(out) IF_FALSE_RETURN_NONE "/*%typemap(out) IF_FALSE_RETURN_NONE */"
%typemap(ret) IF_FALSE_RETURN_NONE
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


%typemap(out) IF_ERROR_RETURN_NONE
{
  /* %typemap(out) IF_ERROR_RETURN_NONE */
}


%import "ogr_error_map.i"

%typemap(out,fragment="OGRErrMessages") OGRErr
{
  /* %typemap(out) OGRErr */
  if ( result != 0 && bUseExceptions) {
    PyErr_SetString( PyExc_RuntimeError, OGRErrMessages(result) );
    SWIG_fail;
  }
}

%typemap(ret) OGRErr
{
  /* %typemap(ret) OGRErr */
  if (resultobj == Py_None ) {
    Py_DECREF(resultobj);
    resultobj = 0;
  }
  if (resultobj == 0) {
    resultobj = PyInt_FromLong( $1 );
  }
}

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

%typemap(in,numinputs=0) ( double argout[ANY]) (double argout[$dim0])
{
  /* %typemap(in,numinputs=0) (double argout[ANY]) */
  $1 = argout;
}
%typemap(argout,fragment="t_output_helper,CreateTupleFromDoubleArray") ( double argout[ANY])
{
  /* %typemap(argout) (double argout[ANY]) */
  PyObject *out = CreateTupleFromDoubleArray( $1, $dim0 );
  $result = t_output_helper($result,out);
}

%typemap(in,numinputs=0) ( double *argout[ANY]) (double *argout)
{
  /* %typemap(in,numinputs=0) (double *argout[ANY]) */
  $1 = &argout;
}
%typemap(argout,fragment="t_output_helper,CreateTupleFromDoubleArray") ( double *argout[ANY])
{
  /* %typemap(argout) (double *argout[ANY]) */
  PyObject *out = CreateTupleFromDoubleArray( *$1, $dim0 );
  $result = t_output_helper($result,out);
}
%typemap(freearg) (double *argout[ANY])
{
  /* %typemap(freearg) (double *argout[ANY]) */
  CPLFree(*$1);
}
%typemap(in) (double argin[ANY]) (double argin[$dim0])
{
  /* %typemap(in) (double argin[ANY]) */
  $1 = argin;
  if (! PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  int seq_size = PySequence_Size($input);
  if ( seq_size != $dim0 ) {
    PyErr_SetString(PyExc_TypeError, "sequence must have length ##size");
    SWIG_fail;
  }
  for (unsigned int i=0; i<$dim0; i++) {
    PyObject *o = PySequence_GetItem($input,i);
    double val;
    if ( !PyArg_Parse(o, "d", &val ) ) {
      PyErr_SetString(PyExc_TypeError, "not a number");
      Py_DECREF(o);
      SWIG_fail;
    }
    $1[i] =  val;
    Py_DECREF(o);
  }
}

/*
 *  Typemap for counted arrays of ints <- PySequence
 */
%typemap(in,numinputs=1) (int nList, int* pList)
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
        PyErr_SetString(PyExc_TypeError, "not an integer");
      Py_DECREF(o);
      SWIG_fail;
    }
    Py_DECREF(o);
  }
}

%typemap(freearg) (int nList, int* pList)
{
  /* %typemap(freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
}

/*
 *  Typemap for counted arrays of doubles <- PySequence
 */
%typemap(in,numinputs=1) (int nList, double* pList)
{
  /* %typemap(in,numinputs=1) (int nList, double* pList)*/
  /* check if is List */
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  $1 = PySequence_Size($input);
  $2 = (double*) malloc($1*sizeof(double));
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    if ( !PyArg_Parse(o,"d",&$2[i]) ) {
      PyErr_SetString(PyExc_TypeError, "not a number");
      Py_DECREF(o);
      SWIG_fail;
    }
    Py_DECREF(o);
  }
}

%typemap(freearg) (int nList, double* pList)
{
  /* %typemap(freearg) (int nList, double* pList) */
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
 * Typemap Band::ReadRaster()
 */
%typemap(in,numinputs=0) ( void **outPythonObject ) ( void *pBuf = NULL )
{
  /* %typemap(in,numinputs=0) ( void **outPythonObject ) ( void *pBuf = NULL ) */
  $1 = &pBuf;
}
%typemap(argout) ( void **outPythonObject )
{
  /* %typemap(argout) ( void **outPythonObject ) */
  Py_XDECREF($result);
  $result = (PyObject *)*$1;
}

/*
 * Typemap for buffers with length <-> PyStrings
 * Used in Band::WriteRaster()
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
  Py_XDECREF($result);
%#if PY_VERSION_HEX >= 0x03000000
  $result = PyBytes_FromStringAndSize( *$2, *$1 );
%#else
  $result = PyString_FromStringAndSize( *$2, *$1 );
%#endif
}
%typemap(freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
  if( *$1 ) {
    free( *$2 );
  }
}


%typemap(in,numinputs=1) (int nLen, char *pBuf ) (int alloc = 0)
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
%#if PY_VERSION_HEX>=0x03000000
  if (PyUnicode_Check($input))
  {
    size_t safeLen = 0;
    int ret = SWIG_AsCharPtrAndSize($input, (char**) &$2, &safeLen, &alloc);
    if (!SWIG_IsOK(ret)) {
      SWIG_exception( SWIG_RuntimeError, "invalid Unicode string" );
    }

    if (safeLen) safeLen--;
    $1 = (int) safeLen;
  }
  else if (PyBytes_Check($input))
  {
    Py_ssize_t safeLen = 0;
    PyBytes_AsStringAndSize($input, (char**) &$2, &safeLen);
    $1 = (int) safeLen;
  }
  else
  {
    PyErr_SetString(PyExc_TypeError, "not a unicode string or a bytes");
    SWIG_fail;
  }
%#else
  if (PyString_Check($input))
  {
    Py_ssize_t safeLen = 0;
    PyString_AsStringAndSize($input, (char**) &$2, &safeLen);
    $1 = (int) safeLen;
  }
  else
  {
    PyErr_SetString(PyExc_TypeError, "not a string");
    SWIG_fail;
  }
%#endif
}

%typemap(freearg) (int nLen, char *pBuf )
{
  /* %typemap(freearg) (int *nLen, char *pBuf ) */
  if( alloc$argnum == SWIG_NEWOBJ ) {
    delete[] $2;
  }
}

/* required for GDALAsyncReader */
%typemap(in,numinputs=0) (int *nLength, char **pBuffer ) ( int nLen = 0, char *pBuf = 0 )
{
  /* %typemap(in,numinputs=0) (int *nLength, char **pBuffer ) */
  $1 = &nLen;
  $2 = &pBuf;
}
%typemap(freearg) (int *nLength, char **pBuffer )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
  if( *$1 ) {
    free( *$2 );
  }
}

/* end of required for GDALAsyncReader */

/*
 * Typemap argout used in Feature::GetFieldAsIntegerList()
 */
%typemap(in,numinputs=0) (int *nLen, const int **pList) (int nLen, int *pList)
{
  /* %typemap(in,numinputs=0) (int *nLen, const int **pList) (int nLen, int *pList) */
  $1 = &nLen;
  $2 = &pList;
}

%typemap(argout) (int *nLen, const int **pList )
{
  /* %typemap(argout) (int *nLen, const int **pList ) */
  Py_DECREF($result);
  PyObject *out = PyList_New( *$1 );
  for( int i=0; i<*$1; i++ ) {
    PyObject *val = PyInt_FromLong( (*$2)[i] );
    PyList_SetItem( out, i, val );
  }
  $result = out;
}

/*
 * Typemap argout used in Feature::GetFieldAsDoubleList()
 */
%typemap(in,numinputs=0) (int *nLen, const double **pList) (int nLen, double *pList)
{
  /* %typemap(in,numinputs=0) (int *nLen, const double **pList) (int nLen, double *pList) */
  $1 = &nLen;
  $2 = &pList;
}

%typemap(argout) (int *nLen, const double **pList )
{
  /* %typemap(argout) (int *nLen, const double **pList ) */
  Py_DECREF($result);
  PyObject *out = PyList_New( *$1 );
  for( int i=0; i<*$1; i++ ) {
    PyObject *val = PyFloat_FromDouble( (*$2)[i] );
    PyList_SetItem( out, i, val );
  }
  $result = out;
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
%typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) ( GDAL_GCP *tmpGCPList )
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
    SWIG_ConvertPtr( o, (void**)&item, SWIGTYPE_p_GDAL_GCP, SWIG_POINTER_EXCEPTION | 0 );
    if ( ! item ) {
      Py_DECREF(o);
      SWIG_fail;
    }
    memcpy( (void*) tmpGCPList, (void*) item, sizeof( GDAL_GCP ) );
    ++tmpGCPList;
    Py_DECREF(o);
  }
}
%typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs )
{
  /* %typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs ) */
  if ($2) {
    free( (void*) $2 );
  }
}

/*
 * Typemap for GDALColorEntry* <-> tuple
 */
%typemap(out) GDALColorEntry*
{
  /* %typemap(out) GDALColorEntry* */
   if ( $1 != NULL )
     $result = Py_BuildValue( "(hhhh)", (*$1).c1,(*$1).c2,(*$1).c3,(*$1).c4);
   else
     $result = NULL;
}

%typemap(in) GDALColorEntry* (GDALColorEntry ce)
{
  /* %typemap(in) GDALColorEntry* */
   ce.c4 = 255;
  if (! PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
   int size = PySequence_Size($input);
   if ( size > 4 ) {
     PyErr_SetString(PyExc_TypeError, "ColorEntry sequence too long");
     SWIG_fail;
   }
   if ( size < 3 ) {
     PyErr_SetString(PyExc_TypeError, "ColorEntry sequence too short");
     SWIG_fail;
   }
   if ( !PyArg_ParseTuple( $input,"hhh|h", &ce.c1, &ce.c2, &ce.c3, &ce.c4 ) ) {
     PyErr_SetString(PyExc_TypeError, "Invalid values in ColorEntry sequence ");
     SWIG_fail;
   }
   $1 = &ce;
}

/*
 * Typemap char ** -> dict
 */
%typemap(out) char **dict
{
  /* %typemap(out) char **dict */
  char **stringarray = $1;
  $result = PyDict_New();
  if ( stringarray != NULL ) {
    while (*stringarray != NULL ) {
      char const *valptr;
      char *keyptr;
      const char* pszSep = strchr( *stringarray, '=' );
      if ( pszSep != NULL) {
        keyptr = CPLStrdup(*stringarray);
        keyptr[pszSep - *stringarray] = '\0';
        valptr = pszSep + 1;
        PyObject *nm = GDALPythonObjectFromCStr( keyptr );
        PyObject *val = GDALPythonObjectFromCStr( valptr );
        PyDict_SetItem($result, nm, val );
        Py_DECREF(nm);
        Py_DECREF(val);
        CPLFree( keyptr );
      }
      stringarray++;
    }
  }
}

/*
 * Typemap char **<- dict.  This typemap actually supports lists as well,
 * Then each entry in the list must be a string and have the form:
 * "name=value" so gdal can handle it.
 */
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (char **dict)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (char **dict) */
  $1 = (PyMapping_Check($input) || PySequence_Check($input) ) ? 1 : 0;
}
%typemap(in) char **dict
{
  /* %typemap(in) char **dict */
  $1 = NULL;
  if ( PySequence_Check( $input ) ) {
    int size = PySequence_Size($input);
    for (int i = 0; i < size; i++) {
      char *pszItem = NULL;
      PyObject* pyObj = PySequence_GetItem($input,i);
      if ( ! PyArg_Parse( pyObj, "s", &pszItem ) ) {
          Py_DECREF(pyObj);
          PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
          SWIG_fail;
      }
      $1 = CSLAddString( $1, pszItem );
      Py_DECREF(pyObj);
    }
  }
  else if ( PyMapping_Check( $input ) ) {
    /* We need to use the dictionary form. */
    int size = PyMapping_Length( $input );
    if ( size > 0 ) {
      PyObject *item_list = PyMapping_Items( $input );
      for( int i=0; i<size; i++ ) {
        PyObject *it = PySequence_GetItem( item_list, i );
        char *nm;
        char *val;
        if ( ! PyArg_ParseTuple( it, "ss", &nm, &val ) ) {
          Py_DECREF(it);
          PyErr_SetString(PyExc_TypeError,"dictionnaire must contain tuples of strings");
          SWIG_fail;
        }
        $1 = CSLAddNameValue( $1, nm, val );
        Py_DECREF(it);
      }
    }
  }
  else {
    PyErr_SetString(PyExc_TypeError,"Argument must be dictionary or sequence of strings");
    SWIG_fail;
  }
}
%typemap(freearg) char **dict
{
  /* %typemap(freearg) char **dict */
  CSLDestroy( $1 );
}

/*
 * Typemap maps char** arguments from Python Sequence Object
 */
%typemap(in) char **options
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
    PyObject* pyObj = PySequence_GetItem($input,i);
    if ( ! PyArg_Parse( pyObj, "s", &pszItem ) ) {
        Py_DECREF(pyObj);
        PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
        SWIG_fail;
    }
    $1 = CSLAddString( $1, pszItem );
    Py_DECREF(pyObj);
  }
}
%typemap(freearg) char **options
{
  /* %typemap(freearg) char **options */
  CSLDestroy( $1 );
}


/*
 * Typemap converts an array of strings into a list of strings
 * with the assumption that the called object maintains ownership of the
 * array of strings.
 */
%typemap(out) char **options
{
  /* %typemap(out) char **options -> ( string ) */
  char **stringarray = $1;
  if ( stringarray == NULL ) {
    $result = Py_None;
    Py_INCREF( $result );
  }
  else {
    int len = CSLCount( stringarray );
    $result = PyList_New( len );
    for ( int i = 0; i < len; ++i ) {
      PyObject *o = GDALPythonObjectFromCStr( stringarray[i] );
      PyList_SetItem($result, i, o );
    }
  }
}

/* Almost same as %typemap(out) char **options */
/* but we CSLDestroy the char** pointer at the end */
%typemap(out) char **out_ppsz_and_free
{
  /* %typemap(out) char **options -> ( string ) */
  char **stringarray = $1;
  if ( stringarray == NULL ) {
    $result = Py_None;
    Py_INCREF( $result );
  }
  else {
    int len = CSLCount( stringarray );
    $result = PyList_New( len );
    for ( int i = 0; i < len; ++i ) {
      PyObject *o = GDALPythonObjectFromCStr( stringarray[i] );
      PyList_SetItem($result, i, o );
    }
  }
  CSLDestroy($1);
}


/*
 * Typemaps map mutable char ** arguments from PyStrings.  Does not
 * return the modified argument
 */
%typemap(in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
  PyArg_Parse( $input, "s", &val );
  $1 = &val;
}

/*
 * Typemap for char **argout.
 */
%typemap(in,numinputs=0) (char **argout) ( char *argout=0 )
{
  /* %typemap(in,numinputs=0) (char **argout) */
  $1 = &argout;
}
%typemap(argout,fragment="t_output_helper") (char **argout)
{
  /* %typemap(argout) (char **argout) */
  PyObject *o;
  if ( $1 != NULL && *$1 != NULL) {
    o = GDALPythonObjectFromCStr( *$1 );
  }
  else {
    o = Py_None;
    Py_INCREF( o );
  }
  $result = t_output_helper($result, o);
}
%typemap(freearg) (char **argout)
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
%typemap(in) (type *optional_##type) ( type val )
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
%typemap(typecheck,precedence=0) (type *optional_##type)
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


%typemap(in) (tostring argin) (PyObject * str=0)
{
  /* %typemap(in) (tostring argin) */
  str = PyObject_Str( $input );
  if ( str == 0 ) {
    PyErr_SetString( PyExc_RuntimeError, "Unable to format argument as string");
    SWIG_fail;
  }
 
  $1 = GDALPythonObjectToCStr(str); 
}
%typemap(freearg)(tostring argin)
{
  /* %typemap(freearg) (tostring argin) */
  if ( str$argnum != NULL)
  {
    Py_DECREF(str$argnum);
  }
  GDALPythonFreeCStr($1);
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin)
{
  /* %typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin) */
%#if PY_VERSION_HEX>=0x03000000
  $1 = (PyUnicode_Check($input) || PyBytes_Check($input)) ? 1 : 0;
%#else
  $1 = (PyString_Check($input)) ? 1 : 0;
%#endif
}

/*
 * Typemap for CPLErr.
 * This typemap will use the wrapper C-variable
 * int UseExceptions to determine proper behavour for
 * CPLErr return codes.
 * If UseExceptions ==0, then return the rc.
 * If UseExceptions ==1, then if rc >= CE_Failure, raise an exception.
 */
%typemap(ret) CPLErr
{
  /* %typemap(ret) CPLErr */
  if ( bUseExceptions == 0 ) {
    /* We're not using exceptions.  And no error has occurred */
    if ( $result == 0 ) {
      /* No other return values set so return ErrorCode */
      $result = PyInt_FromLong($1);
    }
  }
}

/*
 * Typemaps for minixml:  CPLXMLNode* input, CPLXMLNode *ret
 */

%fragment("PyListToXMLTree","header") %{
/************************************************************************/
/*                          PyListToXMLTree()                           */
/************************************************************************/
static CPLXMLNode *PyListToXMLTree( PyObject *pyList )

{
    int      nChildCount = 0, iChild, nType;
    CPLXMLNode *psThisNode;
    CPLXMLNode *psChild;
    char       *pszText = NULL;

    nChildCount = PyList_Size(pyList) - 2;
    if( nChildCount < 0 )
    {
        PyErr_SetString(PyExc_TypeError,"Error in input XMLTree." );
	return NULL;
    }

    PyArg_Parse( PyList_GET_ITEM(pyList,0), "i", &nType );
    PyArg_Parse( PyList_GET_ITEM(pyList,1), "s", &pszText );
    psThisNode = CPLCreateXMLNode( NULL, (CPLXMLNodeType) nType, pszText );

    for( iChild = 0; iChild < nChildCount; iChild++ )
    {
        psChild = PyListToXMLTree( PyList_GET_ITEM(pyList,iChild+2) );
        CPLAddXMLChild( psThisNode, psChild );
    }

    return psThisNode;
}
%}

%typemap(in,fragment="PyListToXMLTree") (CPLXMLNode* xmlnode )
{
  /* %typemap(python,in) (CPLXMLNode* xmlnode ) */
  $1 = PyListToXMLTree( $input );
  if ( !$1 ) SWIG_fail;
}
%typemap(freearg) (CPLXMLNode *xmlnode)
{
  /* %typemap(freearg) (CPLXMLNode *xmlnode) */
  if ( $1 ) CPLDestroyXMLNode( $1 );
}

%fragment("XMLTreeToPyList","header") %{
/************************************************************************/
/*                          XMLTreeToPyList()                           */
/************************************************************************/
static PyObject *XMLTreeToPyList( CPLXMLNode *psTree )
{
    PyObject *pyList;
    int      nChildCount = 0, iChild;
    CPLXMLNode *psChild;

    for( psChild = psTree->psChild; 
         psChild != NULL; 
         psChild = psChild->psNext )
        nChildCount++;

    pyList = PyList_New(nChildCount+2);

    PyList_SetItem( pyList, 0, Py_BuildValue( "i", (int) psTree->eType ) );
    PyList_SetItem( pyList, 1, Py_BuildValue( "s", psTree->pszValue ) );

    for( psChild = psTree->psChild, iChild = 2; 
         psChild != NULL; 
         psChild = psChild->psNext, iChild++ )
    {
        PyList_SetItem( pyList, iChild, XMLTreeToPyList( psChild ) );
    }

    return pyList; 
}
%}

%typemap(out,fragment="XMLTreeToPyList") (CPLXMLNode*)
{
  /* %typemap(out) (CPLXMLNode*) */

  CPLXMLNode *psXMLTree = $1;
  int         bFakeRoot = FALSE;

  if( psXMLTree != NULL && psXMLTree->psNext != NULL )
  {
	CPLXMLNode *psFirst = psXMLTree;

	/* create a "pseudo" root if we have multiple elements */
        psXMLTree = CPLCreateXMLNode( NULL, CXT_Element, "" );
	psXMLTree->psChild = psFirst;
        bFakeRoot = TRUE;
  }

  $result = XMLTreeToPyList( psXMLTree );

  if( bFakeRoot )
  {
        psXMLTree->psChild = NULL;
        CPLDestroyXMLNode( psXMLTree );
  }
}
%typemap(ret) (CPLXMLNode*)
{
  /* %typemap(ret) (CPLXMLNode*) */
  if ( $1 ) CPLDestroyXMLNode( $1 );
}

/* Check inputs to ensure they are not NULL but instead empty #1775 */
%define CHECK_NOT_UNDEF(type, param, msg)
%typemap(check) (const char *pszNewDesc)
{
    /* %typemap(check) (type *param) */
    if ( bUseExceptions && !$1) {
        PyErr_SetString( PyExc_RuntimeError, "Description cannot be None" );
        SWIG_fail;
    }
}
%enddef

//CHECK_NOT_UNDEF(char, method, method)
//CHECK_NOT_UNDEF(const char, name, name)
//CHECK_NOT_UNDEF(const char, request, request)
//CHECK_NOT_UNDEF(const char, cap, capability)
//CHECK_NOT_UNDEF(const char, statement, statement)
CHECK_NOT_UNDEF(const char, pszNewDesc, description)
CHECK_NOT_UNDEF(OSRCoordinateTransformationShadow, , coordinate transformation)
CHECK_NOT_UNDEF(OGRGeometryShadow, other, other geometry)
CHECK_NOT_UNDEF(OGRGeometryShadow, other_disown, other geometry)
CHECK_NOT_UNDEF(OGRGeometryShadow, geom, geometry)
CHECK_NOT_UNDEF(OGRFieldDefnShadow, defn, field definition)
CHECK_NOT_UNDEF(OGRFieldDefnShadow, field_defn, field definition)
CHECK_NOT_UNDEF(OGRFeatureShadow, feature, feature)


/* ==================================================================== */
/*	Support function for progress callbacks to python.                  */
/* ==================================================================== */

/*  The following scary, scary, voodoo -- hobu                          */
/*                                                                      */
/*  A number of things happen as part of callbacks in GDAL.  First,     */
/*  there is a generic callback function internal to GDAL called        */
/*  GDALTermProgress, which just outputs generic progress counts to the */
/*  terminal as you would expect.  This callback function is a special  */
/*  case.  Alternatively, a user can pass in a Python function that     */
/*  can be used as a callback, and it will be eval'd by GDAL during     */
/*  its update loop.  The typemaps here handle taking in                */
/*  GDALTermProgress and the Python function.                           */

/*  This arginit does some magic because it must create a               */
/*  psProgressInfo that is global to the wrapper function.  The noblock */
/*  option here allows it to end up being global and not being          */
/*  instantiated within a {} block.  Both the callback_data and the     */
/*  callback typemaps will then use this struct to hold pointers to the */
/*  callback and callback_data PyObject*'s.                             */

%typemap(arginit, noblock=1) ( void* callback_data=NULL)
{
    /* %typemap(arginit) ( const char* callback_data=NULL)  */
        PyProgressData *psProgressInfo;
        psProgressInfo = (PyProgressData *) CPLCalloc(1,sizeof(PyProgressData));
        psProgressInfo->nLastReported = -1;
        psProgressInfo->psPyCallback = NULL;
        psProgressInfo->psPyCallbackData = NULL;

}

/*  This is kind of silly, but this typemap takes the $input'ed         */
/*  PyObject* and hangs it on the struct's callback data *and* sets     */
/*  the argument to the psProgressInfo void* that will eventually be    */
/*  passed into the function as its callback data.  Confusing.  Sorry.  */
%typemap(in) (void* callback_data=NULL) 
{
    /* %typemap(in) ( void* callback_data=NULL)  */
  
        psProgressInfo->psPyCallbackData = $input ;
        $1 = psProgressInfo;

}

/*  Here is our actual callback function.  It could be a generic GDAL   */
/*  callback function like GDALTermProgress, or it might be a user-     */
/*  defined callback function that is actually a Python function.       */
/*  If we were the generic function, set our argument to that,          */
/*  otherwise, setup the psProgressInfo's callback to be our PyObject*  */
/*  and set our callback function to be PyProgressProxy, which is       */
/*  defined in gdal_python.i                                            */
%typemap(in) ( GDALProgressFunc callback = NULL) 
{
    /* %typemap(in) (GDALProgressFunc callback = NULL) */
    /* callback_func typemap */
    if ($input && $input != Py_None ) {
        void* cbfunction = NULL;
        SWIG_ConvertPtr( $input, 
                         (void**)&cbfunction, 
                         SWIGTYPE_p_f_double_p_q_const__char_p_void__int, 
                         SWIG_POINTER_EXCEPTION | 0 );

        if ( cbfunction == GDALTermProgress ) {
            $1 = GDALTermProgress;
        } else {
            if (!PyCallable_Check($input)) {
                PyErr_SetString( PyExc_RuntimeError, 
                                 "Object given is not a Python function" );
                SWIG_fail;
            }
            psProgressInfo->psPyCallback = $input;
            $1 = PyProgressProxy;
        }
        
    }

}

/*  clean up our global (to the wrapper function) psProgressInfo        */
/*  struct now that we're done with it.                                 */
%typemap(freearg) (void* callback_data=NULL) 
{
    /* %typemap(freearg) ( void* callback_data=NULL)  */
  
        CPLFree(psProgressInfo);

}


%typemap(arginit) ( GUInt32 ) 
{
    /* %typemap(out) ( GUInt32 )  */

	$1 = 0;

}

%typemap(out) ( GUInt32 ) 
{
    /* %typemap(out) ( GUInt32 )  */

	$result = PyLong_FromUnsignedLong($1);

}

%typemap(in) ( GUInt32 ) 
{
    /* %typemap(in) ( GUInt32 )  */

    if (PyLong_Check($input) || PyInt_Check($input)) {
		$1 = PyLong_AsUnsignedLong($input);
	}

}


%define OBJECT_LIST_INPUT(type)
%typemap(in, numinputs=1) (int object_list_count, type **poObjects)
{
  /*  OBJECT_LIST_INPUT %typemap(in) (int itemcount, type *optional_##type)*/
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  $1 = PySequence_Size($input);
  $2 = (type**) CPLMalloc($1*sizeof(type*));
  
  for( int i = 0; i<$1; i++ ) {

      PyObject *o = PySequence_GetItem($input,i);
%#if SWIG_VERSION <= 0x010337
      PySwigObject *sobj = SWIG_Python_GetSwigThis(o);
%#else
      SwigPyObject *sobj = SWIG_Python_GetSwigThis(o);
%#endif
      type* rawobjectpointer = NULL;
      if (!sobj) {
          Py_DECREF(o);
          SWIG_fail;
      }
      rawobjectpointer = (type*) sobj->ptr;
      $2[i] = rawobjectpointer;
      Py_DECREF(o);

  }
}

%typemap(freearg)  (int object_list_count, type **poObjects)
{
  /* OBJECT_LIST_INPUT %typemap(freearg) (int object_list_count, type **poObjects)*/
  CPLFree( $2 );
}
%enddef

OBJECT_LIST_INPUT(GDALRasterBandShadow);

/* ***************************************************************************
 *                       GetHistogram()
 * Python is somewhat special in that we don't want the caller
 * to pass in the histogram array to populate.  Instead we allocate
 * it internally, call the C level, and then turn the result into 
 * a list object. 
 */

%typemap(arginit) (int buckets, int* panHistogram)
{
  /* %typemap(in) int buckets, int* panHistogram -> list */
  $2 = (int *) CPLCalloc(sizeof(int),$1);
}

%typemap(in, numinputs=1) (int buckets, int* panHistogram)
{
  /* %typemap(in) int buckets, int* panHistogram -> list */
  int requested_buckets;
  SWIG_AsVal_int($input, &requested_buckets);
  if( requested_buckets != $1 )
  { 
    $1 = requested_buckets;
    $2 = (int *) CPLRealloc($2,sizeof(int) * requested_buckets);
  }
}

%typemap(freearg)  (int buckets, int* panHistogram)
{
  /* %typemap(freearg) (int buckets, int* panHistogram)*/
  if ( $2 ) {
    CPLFree( $2 );
  }
}

%typemap(argout) (int buckets, int* panHistogram)
{
  /* %typemap(out) int buckets, int* panHistogram -> list */
  int *integerarray = $2;
  if ( integerarray == NULL ) {
    $result = Py_None;
    Py_INCREF( $result );
  }
  else {
    $result = PyList_New( $1 );
    for ( int i = 0; i < $1; ++i ) {
      PyObject *o =  PyInt_FromLong( integerarray[i] );
      PyList_SetItem($result, i, o );
    }
  }
}

/* ***************************************************************************
 *                       GetDefaultHistogram()
 */

%typemap(arginit, noblock=1) (double *min_ret, double *max_ret, int *buckets_ret, int **ppanHistogram)
{
   double min_val, max_val;
   int buckets_val;
   int *panHistogram;

   $1 = &min_val;
   $2 = &max_val;
   $3 = &buckets_val;
   $4 = &panHistogram;
}

%typemap(argout) (double *min_ret, double *max_ret, int *buckets_ret, int** ppanHistogram)
{
  int i;
  PyObject *psList = NULL;

  Py_XDECREF($result);
  
  if (panHistogram)
  {
      psList = PyList_New(buckets_val);
      for( i = 0; i < buckets_val; i++ )
        PyList_SetItem(psList, i, Py_BuildValue("i", panHistogram[i] ));

      CPLFree( panHistogram );

      $result = Py_BuildValue( "(ddiO)", min_val, max_val, buckets_val, psList );
      Py_XDECREF(psList);
  }
  else
  {
      $result = Py_None;
      Py_INCREF($result);
  }
}

/***************************************************
 * Typemaps for  (retStringAndCPLFree*)
 ***************************************************/
%typemap(out) (retStringAndCPLFree*)
{
    /* %typemap(out) (retStringAndCPLFree*) */
    if(result)
    {
        $result = GDALPythonObjectFromCStr( (const char *)result);
        CPLFree(result);
    }
}

/***************************************************
 * Typemaps for CoordinateTransformation.TransformPoints()
 ***************************************************/
%fragment("DecomposeSequenceOfCoordinates","header") %{
static int
DecomposeSequenceOfCoordinates( PyObject *seq, int nCount, double *x, double *y, double *z )
{
  for( int i = 0; i<nCount; ++i )
  {

    PyObject *o = PySequence_GetItem(seq, i);
    if ( !PySequence_Check(o) )
    {
        Py_DECREF(o);
        PyErr_SetString(PyExc_TypeError, "not a sequence");

        return FALSE;
    }

    Py_ssize_t len = PySequence_Size(o);

    if (len == 2 || len == 3)
    {
        PyObject *o1 = PySequence_GetItem(o, 0);
        if (!PyNumber_Check(o1))
        {
            Py_DECREF(o); Py_DECREF(o1);
            PyErr_SetString(PyExc_TypeError, "not a number");

            return FALSE;
        }
        x[i] = PyFloat_AsDouble(o1);
        Py_DECREF(o1);

        o1 = PySequence_GetItem(o, 1);
        if (!PyNumber_Check(o1))
        {
            Py_DECREF(o); Py_DECREF(o1);
            PyErr_SetString(PyExc_TypeError, "not a number");

            return FALSE;
        }
        y[i] = PyFloat_AsDouble(o1);
        Py_DECREF(o1);

        /* The 3rd coordinate is optional, default 0.0 */
        if (len == 3)
        {
            o1 = PySequence_GetItem(o, 2);
            if (!PyNumber_Check(o1))
            {
                Py_DECREF(o); Py_DECREF(o1);
                PyErr_SetString(PyExc_TypeError, "not a number");

                return FALSE;
            }
            z[i] = PyFloat_AsDouble(o1);
            Py_DECREF(o1);
        }
        else
        {
            z[i] = 0.0;
        }
    }
    else
    {
        Py_DECREF(o);
        PyErr_SetString(PyExc_TypeError, "invalid coordinate");

        return FALSE;
    }

    Py_DECREF(o);
  }

  return TRUE;
}
%}

%typemap(in,numinputs=1,fragment="DecomposeSequenceOfCoordinates") (int nCount, double *x, double *y, double *z)
{
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }

  $1 = PySequence_Size($input);
  $2 = (double*) VSIMalloc($1*sizeof(double));
  $3 = (double*) VSIMalloc($1*sizeof(double));
  $4 = (double*) VSIMalloc($1*sizeof(double));

  if ($2 == NULL || $3 == NULL || $4 == NULL)
  {
      PyErr_SetString( PyExc_RuntimeError, "Out of memory" );
      SWIG_fail;
  }

  if (!DecomposeSequenceOfCoordinates($input,$1,$2,$3,$4)) {
    SWIG_fail;
  }
}

%typemap(argout)  (int nCount, double *x, double *y, double *z)
{
  /* %typemap(argout)  (int nCount, double *x, double *y, double *z) */
  Py_DECREF($result);
  PyObject *out = PyList_New( $1 );
  for( int i=0; i< $1; i++ ) {
    PyObject *tuple = PyTuple_New( 3 );
    PyTuple_SetItem( tuple, 0, PyFloat_FromDouble( ($2)[i] ) );
    PyTuple_SetItem( tuple, 1, PyFloat_FromDouble( ($3)[i] ) );
    PyTuple_SetItem( tuple, 2, PyFloat_FromDouble( ($4)[i] ) );
    PyList_SetItem( out, i, tuple );
  }
  $result = out;
}

%typemap(freearg)  (int nCount, double *x, double *y, double *z)
{
    /* %typemap(freearg)  (int nCount, double *x, double *y, double *z) */
    VSIFree($2);
    VSIFree($3);
    VSIFree($4);
}

/***************************************************
 * Typemaps for Transform.TransformPoints()
 ***************************************************/

%typemap(in,numinputs=1,fragment="DecomposeSequenceOfCoordinates") (int nCount, double *x, double *y, double *z, int* panSuccess)
{
  /*  typemap(in,numinputs=1) (int nCount, double *x, double *y, double *z, int* panSuccess) */
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }

  $1 = PySequence_Size($input);
  $2 = (double*) VSIMalloc($1*sizeof(double));
  $3 = (double*) VSIMalloc($1*sizeof(double));
  $4 = (double*) VSIMalloc($1*sizeof(double));
  $5 = (int*) VSIMalloc($1*sizeof(int));

  if ($2 == NULL || $3 == NULL || $4 == NULL || $5 == NULL)
  {
      PyErr_SetString( PyExc_RuntimeError, "Out of memory" );
      SWIG_fail;
  }

  if (!DecomposeSequenceOfCoordinates($input,$1,$2,$3,$4)) {
     SWIG_fail;
  }
}

%typemap(argout)  (int nCount, double *x, double *y, double *z, int* panSuccess)
{
  /* %typemap(argout)  (int nCount, double *x, double *y, double *z, int* panSuccess) */
  Py_DECREF($result);
  PyObject *xyz = PyList_New( $1 );
  PyObject *success = PyList_New( $1 );
  for( int i=0; i< $1; i++ ) {
    PyObject *tuple = PyTuple_New( 3 );
    PyTuple_SetItem( tuple, 0, PyFloat_FromDouble( ($2)[i] ) );
    PyTuple_SetItem( tuple, 1, PyFloat_FromDouble( ($3)[i] ) );
    PyTuple_SetItem( tuple, 2, PyFloat_FromDouble( ($4)[i] ) );
    PyList_SetItem( xyz, i, tuple );
    PyList_SetItem( success, i, Py_BuildValue( "i",  ($5)[i]) );
  }
  $result = PyTuple_New( 2 );
  PyTuple_SetItem( $result, 0, xyz );
  PyTuple_SetItem( $result, 1, success );
}

%typemap(freearg)  (int nCount, double *x, double *y, double *z, int* panSuccess)
{
    /* %typemap(freearg)  (int nCount, double *x, double *y, double *z, int* panSuccess) */
    VSIFree($2);
    VSIFree($3);
    VSIFree($4);
    VSIFree($5);
}
