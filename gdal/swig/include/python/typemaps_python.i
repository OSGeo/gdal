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

%typemap(in) GIntBig
{
    PY_LONG_LONG val;
    if ( !PyArg_Parse($input,"L",&val) ) {
        PyErr_SetString(PyExc_TypeError, "not an integer");
        SWIG_fail;
    }
    $1 = (GIntBig)val;
}

%typemap(out) GIntBig
{
    char szTmp[32];
    sprintf(szTmp, CPL_FRMT_GIB, $1);
%#if PY_VERSION_HEX>=0x03000000
    $result = PyLong_FromString(szTmp, NULL, 10);
%#else
    $result = PyInt_FromString(szTmp, NULL, 10);
%#endif
}

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


%typemap(in,numinputs=0) (double argout[6], int* isvalid) ( double argout[6], int isvalid )
{
  /* %typemap(in,numinputs=0) (double argout[6], int* isvalid) */
  $1 = argout;
  $2 = &isvalid;
}

%typemap(argout) (double argout[6], int* isvalid) 
{
   /* %typemap(argout) (double argout[6], int* isvalid)  */
  PyObject *r;
  if ( !*$2 ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }
  else {
    r = CreateTupleFromDoubleArray($1, 6);
  }
  $result = t_output_helper($result,r);
}


%typemap(in,numinputs=0) (double argout[4], int* isvalid) ( double argout[6], int isvalid )
{
  /* %typemap(in) (double argout[4], int* isvalid) */
  $1 = argout;
  $2 = &isvalid;
}

%typemap(argout) (double argout[4], int* isvalid)
{
   /* %typemap(argout) (double argout[4], int* isvalid)  */
  PyObject *r;
  if ( !*$2 ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }
  else {
    r = CreateTupleFromDoubleArray($1, 4);
  }
  $result = t_output_helper($result,r);
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
 *  Typemap for counted arrays of GIntBig <- PySequence
 */
%typemap(in,numinputs=1) (int nList, GIntBig* pList)
{
  /* %typemap(in,numinputs=1) (int nList, GIntBig* pList)*/
  /* check if is List */
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  $1 = PySequence_Size($input);
  $2 = (GIntBig*) malloc($1*sizeof(GIntBig));
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    PY_LONG_LONG val;
    if ( !PyArg_Parse(o,"L",&val) ) {
      PyErr_SetString(PyExc_TypeError, "not an integer");
      Py_DECREF(o);
      SWIG_fail;
    }
    $2[i] = (GIntBig)val;
    Py_DECREF(o);
  }
}

%typemap(freearg) (int nList, GIntBig* pList)
{
  /* %typemap(freearg) (int nList, GIntBig* pList) */
  if ($2) {
    free((void*) $2);
  }
}

/*
 *  Typemap for counted arrays of GUIntBig <- PySequence
 */
%typemap(in,numinputs=1) (int nList, GUIntBig* pList)
{
  /* %typemap(in,numinputs=1) (int nList, GUIntBig* pList)*/
  /* check if is List */
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  $1 = PySequence_Size($input);
  $2 = (GUIntBig*) malloc($1*sizeof(GUIntBig));
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    PY_LONG_LONG val;
    if ( !PyArg_Parse(o,"K",&val) ) {
      PyErr_SetString(PyExc_TypeError, "not an integer");
      Py_DECREF(o);
      SWIG_fail;
    }
    $2[i] = (GUIntBig)val;
    Py_DECREF(o);
  }
}

%typemap(freearg) (int nList, GUIntBig* pList)
{
  /* %typemap(freearg) (int nList, GUIntBig* pList) */
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
%typemap(in,numinputs=0) ( void **outPythonObject ) ( void *pyObject = NULL )
{
  /* %typemap(in,numinputs=0) ( void **outPythonObject ) ( void *pyObject = NULL ) */
  $1 = &pyObject;
}
%typemap(argout) ( void **outPythonObject )
{
  /* %typemap(argout) ( void **outPythonObject ) */
  Py_XDECREF($result);
  if (*$1)
  {
      $result = (PyObject*)*$1;
  }
  else
  {
      $result = Py_None;
      Py_INCREF($result);
  }
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


%typemap(in,numinputs=1) (GIntBig nLen, char *pBuf ) (int alloc = 0)
{
  /* %typemap(in,numinputs=1) (GIntBig nLen, char *pBuf ) */
%#if PY_VERSION_HEX>=0x03000000
  if (PyUnicode_Check($input))
  {
    size_t safeLen = 0;
    int ret = SWIG_AsCharPtrAndSize($input, (char**) &$2, &safeLen, &alloc);
    if (!SWIG_IsOK(ret)) {
      SWIG_exception( SWIG_RuntimeError, "invalid Unicode string" );
    }

    if (safeLen) safeLen--;
    $1 = (GIntBig) safeLen;
  }
  else if (PyBytes_Check($input))
  {
    Py_ssize_t safeLen = 0;
    PyBytes_AsStringAndSize($input, (char**) &$2, &safeLen);
    $1 = (GIntBig) safeLen;
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
    $1 = (GIntBig) safeLen;
  }
  else
  {
    PyErr_SetString(PyExc_TypeError, "not a string");
    SWIG_fail;
  }
%#endif
}

%typemap(freearg) (GIntBig nLen, char *pBuf )
{
  /* %typemap(freearg) (GIntBig *nLen, char *pBuf ) */
  if( alloc$argnum == SWIG_NEWOBJ ) {
    delete[] $2;
  }
}

/* required for GDALAsyncReader */

%typemap(in,numinputs=1) (int nLenKeepObject, char *pBufKeepObject, void* pyObject)
{
  /* %typemap(in,numinputs=1) (int nLenKeepObject, char *pBufKeepObject, void* pyObject) */
%#if PY_VERSION_HEX>=0x03000000
  if (PyBytes_Check($input))
  {
    Py_ssize_t safeLen = 0;
    PyBytes_AsStringAndSize($input, (char**) &$2, &safeLen);
    $1 = (int) safeLen;
    $3 = $input;
  }
  else
  {
    PyErr_SetString(PyExc_TypeError, "not a bytes");
    SWIG_fail;
  }
%#else
  if (PyString_Check($input))
  {
    Py_ssize_t safeLen = 0;
    PyString_AsStringAndSize($input, (char**) &$2, &safeLen);
    $1 = (int) safeLen;
    $3 = $input;
  }
  else
  {
    PyErr_SetString(PyExc_TypeError, "not a string");
    SWIG_fail;
  }
%#endif
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
 * Typemap argout used in Feature::GetFieldAsInteger64List()
 */
%typemap(in,numinputs=0) (int *nLen, const GIntBig **pList) (int nLen, GIntBig *pList)
{
  /* %typemap(in,numinputs=0) (int *nLen, const GIntBig **pList) (int nLen, GIntBig *pList) */
  $1 = &nLen;
  $2 = &pList;
}

%typemap(argout) (int *nLen, const GIntBig **pList )
{
  /* %typemap(argout) (int *nLen, const GIntBig **pList ) */
  Py_DECREF($result);
  PyObject *out = PyList_New( *$1 );
  for( int i=0; i<*$1; i++ ) {
    char szTmp[32];
    sprintf(szTmp, CPL_FRMT_GIB, (*$2)[i]);
    PyObject* val;
%#if PY_VERSION_HEX>=0x03000000
    val = PyLong_FromString(szTmp, NULL, 10);
%#else
    val = PyInt_FromString(szTmp, NULL, 10);
%#endif
    PyList_SetItem( out, i, val );
  }
  $result = out;
}

/*
 * Typemap argout used in Feature::GetFieldAsInteger64List()
 */
%typemap(in,numinputs=0) (int *nLen, const GIntBig **pList) (int nLen, GIntBig *pList)
{
  /* %typemap(in,numinputs=0) (int *nLen, const GIntBig **pList) (int nLen, GIntBig *pList) */
  $1 = &nLen;
  $2 = &pList;
}

%typemap(argout) (int *nLen, const GIntBig **pList )
{
  /* %typemap(argout) (int *nLen, const GIntBig **pList ) */
  Py_DECREF($result);
  PyObject *out = PyList_New( *$1 );
  for( int i=0; i<*$1; i++ ) {
    char szTmp[32];
    sprintf(szTmp, CPL_FRMT_GIB, (*$2)[i]);
    PyObject* val;
%#if PY_VERSION_HEX>=0x03000000
    val = PyLong_FromString(szTmp, NULL, 10);
%#else
    val = PyInt_FromString(szTmp, NULL, 10);
%#endif
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
  /* Note: we exclude explicitly strings, because they can be considered as a sequence of characters, */
  /* which is not desirable since it makes it impossible to define bindings such as SetMetadata(string) and SetMetadata(array_of_string) */
  /* (see #4816) */
  $1 = ((PyMapping_Check($input) || PySequence_Check($input) ) && !SWIG_CheckState(SWIG_AsCharPtrAndSize($input, 0, NULL, 0)) ) ? 1 : 0;
}
%typemap(in) char **dict
{
  /* %typemap(in) char **dict */
  $1 = NULL;
  if ( PySequence_Check( $input ) ) {
    int size = PySequence_Size($input);
    for (int i = 0; i < size; i++) {
      PyObject* pyObj = PySequence_GetItem($input,i);
      int bFreeStr;
      char* pszStr = GDALPythonObjectToCStr(pyObj, &bFreeStr);
      if ( pszStr == NULL ) {
          Py_DECREF(pyObj);
          PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
          SWIG_fail;
      }
      $1 = CSLAddString( $1, pszStr );
      GDALPythonFreeCStr(pszStr, bFreeStr);
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

        PyObject *k, *v;
        if ( ! PyArg_ParseTuple( it, "OO", &k, &v ) ) {
          Py_DECREF(it);
          PyErr_SetString(PyExc_TypeError,"dictionnaire must contain tuples of strings");
          SWIG_fail;
        }

        int bFreeK, bFreeV;
        char* pszK = GDALPythonObjectToCStr(k, &bFreeK);
        char* pszV = GDALPythonObjectToCStr(v, &bFreeV);
        if( pszK == NULL || pszV == NULL )
        {
            GDALPythonFreeCStr(pszK, bFreeK);
            GDALPythonFreeCStr(pszV, bFreeV);
            Py_DECREF(it);
            PyErr_SetString(PyExc_TypeError,"dictionnaire must contain tuples of strings");
            SWIG_fail;
        }
         $1 = CSLAddNameValue( $1, pszK, pszV );

        GDALPythonFreeCStr(pszK, bFreeK);
        GDALPythonFreeCStr(pszV, bFreeV);
        Py_DECREF(it);
      }
      Py_DECREF(item_list);
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
  /* Check if is a list (and reject strings, that are seen as sequence of characters)  */
  if ( ! PySequence_Check($input) || PyUnicode_Check($input)
%#if PY_VERSION_HEX < 0x03000000
    || PyString_Check($input)
%#endif
    ) {
    PyErr_SetString(PyExc_TypeError,"not a sequence");
    SWIG_fail;
  }

  int size = PySequence_Size($input);
  for (int i = 0; i < size; i++) {
    PyObject* pyObj = PySequence_GetItem($input,i);
    if (PyUnicode_Check(pyObj))
    {
      char *pszStr;
      Py_ssize_t nLen;
      PyObject* pyUTF8Str = PyUnicode_AsUTF8String(pyObj);
%#if PY_VERSION_HEX >= 0x03000000
      PyBytes_AsStringAndSize(pyUTF8Str, &pszStr, &nLen);
%#else
      PyString_AsStringAndSize(pyUTF8Str, &pszStr, &nLen);
%#endif
      $1 = CSLAddString( $1, pszStr );
      Py_XDECREF(pyUTF8Str);
    }
%#if PY_VERSION_HEX >= 0x03000000
    else if (PyBytes_Check(pyObj))
      $1 = CSLAddString( $1, PyBytes_AsString(pyObj) );
%#else
    else if (PyString_Check(pyObj))
      $1 = CSLAddString( $1, PyString_AsString(pyObj) );
%#endif
    else
    {
        Py_DECREF(pyObj);
        PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
        SWIG_fail;
    }
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
%typemap(out) char **CSL
{
  /* %typemap(out) char **CSL -> ( string ) */
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
  if( !PyArg_Parse( $input, "s", &val ) ) {
    PyErr_SetString( PyExc_TypeError, "not a string" );
    SWIG_fail;
  }
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
OPTIONAL_POD(GIntBig,L);

/*
 * Typedef const char * <- Any object.
 *
 * Formats the object using str and returns the string representation
 */


%typemap(in) (tostring argin) (PyObject * str=0, int bToFree = 0)
{
  /* %typemap(in) (tostring argin) */
  str = PyObject_Str( $input );
  if ( str == 0 ) {
    PyErr_SetString( PyExc_RuntimeError, "Unable to format argument as string");
    SWIG_fail;
  }
 
  $1 = GDALPythonObjectToCStr(str, &bToFree); 
}
%typemap(freearg)(tostring argin)
{
  /* %typemap(freearg) (tostring argin) */
  if ( str$argnum != NULL)
  {
    Py_DECREF(str$argnum);
  }
  GDALPythonFreeCStr($1, bToFree$argnum);
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

    /* Detect "pseudo" root */
    if (nType == CXT_Element && pszText != NULL && strlen(pszText) == 0 && nChildCount == 2)
    {
        PyObject *pyFirst = PyList_GET_ITEM(pyList, 2);
        if (PyList_Size(pyFirst) < 2)
        {
            PyErr_SetString(PyExc_TypeError,"Error in input XMLTree." );
            return NULL;
        }
        int nTypeFirst = 0;
        char* pszTextFirst = NULL;
        PyArg_Parse( PyList_GET_ITEM(pyFirst,0), "i", &nTypeFirst );
        PyArg_Parse( PyList_GET_ITEM(pyFirst,1), "s", &pszTextFirst );
        if (nTypeFirst == CXT_Element && pszTextFirst != NULL && pszTextFirst[0] == '?')
        {
            psThisNode = PyListToXMLTree( PyList_GET_ITEM(pyList,2) );
            psThisNode->psNext = PyListToXMLTree( PyList_GET_ITEM(pyList,3) );
            return psThisNode;
        }
    }

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

    if( psTree == NULL )
        return Py_None;

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
        $1 = psProgressInfo;
}

/*  This typemap takes the $input'ed  PyObject* and hangs it on the     */
/*  struct's callback data .                                            */
%typemap(in) (void* callback_data=NULL) 
{
    /* %typemap(in) ( void* callback_data=NULL)  */
        psProgressInfo->psPyCallbackData = $input ;
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


%typemap(in) ( CPLErrorHandler pfnErrorHandler = NULL, void* user_data = NULL ) 
{
    /* %typemap(in) (CPLErrorHandler pfnErrorHandler = NULL, void* user_data = NULL) */
    int alloc = 0;
    char* pszCallbackName = NULL;
    $2 = NULL;
    if( SWIG_IsOK(SWIG_AsCharPtrAndSize($input, &pszCallbackName, NULL, &alloc)) )
    {
        if( pszCallbackName == NULL || EQUAL(pszCallbackName,"CPLQuietErrorHandler") )
            $1 = CPLQuietErrorHandler;
        else if( EQUAL(pszCallbackName,"CPLDefaultErrorHandler") )
            $1 = CPLDefaultErrorHandler;
        else if( EQUAL(pszCallbackName,"CPLLoggingErrorHandler") )
            $1 = CPLLoggingErrorHandler;
        else
        {
            if (alloc == SWIG_NEWOBJ) delete[] pszCallbackName;
            PyErr_SetString( PyExc_RuntimeError, "Unhandled value for passed string" );
            SWIG_fail;
        }

        if (alloc == SWIG_NEWOBJ) delete[] pszCallbackName;
    }
    else if (!PyCallable_Check($input))
    {
        PyErr_SetString( PyExc_RuntimeError, 
                         "Object given is not a String or a Python function" );
        SWIG_fail;
    }
    else
    {
        Py_INCREF($input);
        $1 = PyCPLErrorHandler;
        $2 = $input;
    }
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

%typemap(arginit) (int buckets, GUIntBig* panHistogram)
{
  /* %typemap(in) int buckets, GUIntBig* panHistogram -> list */
  $2 = (GUIntBig *) VSICalloc(sizeof(GUIntBig),$1);
}

%typemap(in, numinputs=1) (int buckets, GUIntBig* panHistogram)
{
  /* %typemap(in) int buckets, GUIntBig* panHistogram -> list */
  int requested_buckets = 0;
  SWIG_AsVal_int($input, &requested_buckets);
  if( requested_buckets != $1 )
  {
    $1 = requested_buckets;
    if (requested_buckets <= 0 || requested_buckets > (int)(INT_MAX / sizeof(GUIntBig)))
    {
        PyErr_SetString( PyExc_RuntimeError, "Bad value for buckets" );
        SWIG_fail;
    }
    $2 = (GUIntBig *) VSIRealloc($2, sizeof(GUIntBig) * requested_buckets);
  }
  if ($2 == NULL)
  {
    PyErr_SetString( PyExc_RuntimeError, "Cannot allocate buckets" );
    SWIG_fail;
  }
}

%typemap(freearg)  (int buckets, GUIntBig* panHistogram)
{
  /* %typemap(freearg) (int buckets, GUIntBig* panHistogram)*/
  if ( $2 ) {
    VSIFree( $2 );
  }
}

%typemap(argout) (int buckets, GUIntBig* panHistogram)
{
  /* %typemap(out) int buckets, GUIntBig* panHistogram -> list */
  GUIntBig *integerarray = $2;
  if ( integerarray == NULL ) {
    $result = Py_None;
    Py_INCREF( $result );
  }
  else {
    $result = PyList_New( $1 );
    for ( int i = 0; i < $1; ++i ) {
      char szTmp[32];
      sprintf(szTmp, CPL_FRMT_GUIB, integerarray[i]);
%#if PY_VERSION_HEX>=0x03000000
      PyObject *o = PyLong_FromString(szTmp, NULL, 10);
%#else
      PyObject *o =  PyInt_FromString(szTmp, NULL, 10);
%#endif
      PyList_SetItem($result, i, o );
    }
  }
}

/* ***************************************************************************
 *                       GetDefaultHistogram()
 */

%typemap(arginit, noblock=1) (double *min_ret, double *max_ret, int *buckets_ret, GUIntBig **ppanHistogram)
{
   double min_val, max_val;
   int buckets_val;
   GUIntBig *panHistogram;

   $1 = &min_val;
   $2 = &max_val;
   $3 = &buckets_val;
   $4 = &panHistogram;
}

%typemap(argout) (double *min_ret, double *max_ret, int *buckets_ret, GUIntBig** ppanHistogram)
{
  int i;
  PyObject *psList = NULL;

  Py_XDECREF($result);
  
  if (panHistogram)
  {
      psList = PyList_New(buckets_val);
      for( i = 0; i < buckets_val; i++ )
        PyList_SetItem(psList, i, Py_BuildValue("K", panHistogram[i] ));

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


/***************************************************
 * Typemaps for Gemetry.GetPoints()
 ***************************************************/
%typemap(in,numinputs=0) (int* pnCount, double** ppadfXY, double** ppadfZ) ( int nPoints = 0, double* padfXY = NULL, double* padfZ = NULL )
{
  /* %typemap(in,numinputs=0) (int* pnCount, double** ppadfXY, double** ppadfZ) */
  $1 = &nPoints;
  $2 = &padfXY;
  $3 = &padfZ;
}

%typemap(argout)  (int* pnCount, double** ppadfXY, double** ppadfZ)
{
  /* %typemap(argout)  (int* pnCount, double** ppadfXY, double** ppadfZ) */
  Py_DECREF($result);
  int nPointCount = *($1);
  if (nPointCount == 0)
  {
    $result = Py_None;
  }
  else
  {
    PyObject *xyz = PyList_New( nPointCount );
    int nDimensions = (*$3 != NULL) ? 3 : 2;
    for( int i=0; i< nPointCount; i++ ) {
        PyObject *tuple = PyTuple_New( nDimensions );
        PyTuple_SetItem( tuple, 0, PyFloat_FromDouble( (*$2)[2*i] ) );
        PyTuple_SetItem( tuple, 1, PyFloat_FromDouble( (*$2)[2*i+1] ) );
        if (nDimensions == 3)
            PyTuple_SetItem( tuple, 2, PyFloat_FromDouble( (*$3)[i] ) );
        PyList_SetItem( xyz, i, tuple );
    }
    $result = xyz;
  }
}

%typemap(freearg)  (int* pnCount, double** ppadfXY, double** ppadfZ)
{
    /* %typemap(freearg)  (int* pnCount, double** ppadfXY, double** ppadfZ) */
    VSIFree(*$2);
    VSIFree(*$3);
}


%typemap(in) (const char *utf8_path) (int bToFree = 0)
{
    /* %typemap(in) (const char *utf8_path) */
    $1 = GDALPythonObjectToCStr( $input, &bToFree );
    if ($1 == NULL)
    {
        PyErr_SetString( PyExc_RuntimeError, "not a string" );
        SWIG_fail;
    }
}

%typemap(freearg)(const char *utf8_path)
{
    /* %typemap(freearg) (const char *utf8_path) */
    GDALPythonFreeCStr($1, bToFree$argnum);
}

/*
 * Typemap argout of StatBuf * used in VSIStatL( )
 */
%typemap(in,numinputs=0) (StatBuf *psStatBufOut) (StatBuf sStatBuf )
{
  /* %typemap(in,numinputs=0) (StatBuf *psStatBufOut) (StatBuf sStatBuf ) */
  $1 = &sStatBuf;
}
%typemap(argout) (StatBuf *psStatBufOut)
{
  /* %typemap(argout) (StatBuf *psStatBufOut)*/
  Py_DECREF($result);
  if (result == 0)
    $result = SWIG_NewPointerObj((void*)new_StatBuf( $1 ),SWIGTYPE_p_StatBuf,1);
  else
    $result = Py_None;
}

%typemap(in,numinputs=0) (void** pptr, size_t* pnsize, GDALDataType* pdatatype, int* preadonly) (void* ptr, size_t nsize, GDALDataType datatype, int readonly)
{
  /* %typemap(in,numinputs=0) (void** pptr, size_t* pnsize, GDALDataType* pdatatype, int* preadonly) */
  $1 = &ptr;
  $2 = &nsize;
  $3 = &datatype;
  $4 = &readonly;
}
%typemap(argout) (void** pptr, size_t* pnsize, GDALDataType* pdatatype, int* preadonly)
{
%#if PY_VERSION_HEX >= 0x02070000 
  /* %typemap(argout) (void** pptr, size_t* pnsize, GDALDataType* pdatatype, int* preadonly)*/
  Py_buffer *buf=(Py_buffer*)malloc(sizeof(Py_buffer));
  if (PyBuffer_FillInfo(buf,  obj0,  *($1), *($2), *($4), PyBUF_ND)) {
    // error, handle
  }
  if( *($3) == GDT_Byte )
  {
    buf->format = "B";
    buf->itemsize = 1;
  }
  else if( *($3) == GDT_Int16 )
  {
    buf->format = "h";
    buf->itemsize = 2;
  }
  else if( *($3) == GDT_UInt16 )
  {
    buf->format = "H";
    buf->itemsize = 2;
  }
  else if( *($3) == GDT_Int32 )
  {
    buf->format = "i";
    buf->itemsize = 4;
  }
  else if( *($3) == GDT_UInt32 )
  {
    buf->format = "I";
    buf->itemsize = 4;
  }
  else if( *($3) == GDT_Float32 )
  {
    buf->format = "f";
    buf->itemsize = 4;
  }
  else if( *($3) == GDT_Float64 )
  {
    buf->format = "F";
    buf->itemsize = 8;
  }
  else
  {
    buf->format = "B";
    buf->itemsize = 1;
  }
  $result = PyMemoryView_FromBuffer(buf);
%#else
  PyErr_SetString( PyExc_RuntimeError, "needs Python 2.7 or later" );
  SWIG_fail;
%#endif
}
