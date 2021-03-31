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

%typemap(out) VSI_RETVAL
{
  /* %typemap(out) VSI_RETVAL */
  if ( result != 0 && bUseExceptions) {
    const char* pszMessage = CPLGetLastErrorMsg();
    if( pszMessage[0] != '\0' )
        PyErr_SetString( PyExc_RuntimeError, pszMessage );
    else
        PyErr_SetString( PyExc_RuntimeError, "unknown error occurred" );
    SWIG_fail;
  }
}

%typemap(ret) VSI_RETVAL
{
  /* %typemap(ret) VSI_RETVAL */
  resultobj = PyInt_FromLong( $1 );
}


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
    $result = PyLong_FromString(szTmp, NULL, 10);
}

/*
 * double *val, int*hasval, is a special contrived typemap used for
 * the RasterBand GetNoDataValue, GetMinimum, GetMaximum, GetOffset, GetScale methods.
 * In the python bindings, the variable hasval is tested.  If it is 0 (is, the value
 * is not set in the raster band) then Py_None is returned.  If it is != 0, then
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
    const char* pszMessage = CPLGetLastErrorMsg();
    if( pszMessage[0] != '\0' )
        PyErr_SetString( PyExc_RuntimeError, pszMessage );
    else
        PyErr_SetString( PyExc_RuntimeError, OGRErrMessages(result) );
    SWIG_fail;
  }
}

%typemap(ret) OGRErr
{
  /* %typemap(ret) OGRErr */
  if ( ReturnSame(resultobj == Py_None || resultobj == 0) ) {
    resultobj = PyInt_FromLong( $1 );
  }
}

%fragment("CreateTupleFromDoubleArray","header") %{
static PyObject *
CreateTupleFromDoubleArray( const double *first, size_t size ) {
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
  memset(argout, 0, sizeof(argout));
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
  argout = NULL;
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
  Py_ssize_t seq_size = PySequence_Size($input);
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

%fragment("CreateCIntListFromSequence","header") %{
static int*
CreateCIntListFromSequence( PyObject* pySeq, int* pnSize ) {
  /* check if is List */
  if ( !PySequence_Check(pySeq) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    *pnSize = -1;
    return NULL;
  }
  Py_ssize_t size = PySequence_Size(pySeq);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  *pnSize = (int)size;
  int* ret = (int*) malloc(*pnSize*sizeof(int));
  for( int i = 0; i<*pnSize; i++ ) {
    PyObject *o = PySequence_GetItem(pySeq,i);
    if ( !PyArg_Parse(o,"i",&ret[i]) ) {
        PyErr_SetString(PyExc_TypeError, "not an integer");
        Py_DECREF(o);
        free(ret);
        *pnSize = -1;
        return NULL;
    }
    Py_DECREF(o);
  }
  return ret;
}
%}

/*
 *  Typemap for counted arrays of ints <- PySequence
 */
%typemap(in,numinputs=1,fragment="CreateCIntListFromSequence") (int nList, int* pList)
{
  /* %typemap(in,numinputs=1) (int nList, int* pList)*/
  $2 = CreateCIntListFromSequence($input, &$1);
  if( $1 < 0 ) {
    SWIG_fail;
  }
}

%typemap(freearg) (int nList, int* pList)
{
  /* %typemap(freearg) (int nList, int* pList) */
  free($2);
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
  Py_ssize_t size = PySequence_Size($input);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
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
  Py_ssize_t size = PySequence_Size($input);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
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
  Py_ssize_t size = PySequence_Size($input);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
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

%typemap(in) ( void *inPythonObject )
{
  /* %typemap(in) ( void *inPythonObject ) */
  $1 = $input;
}

/*
 * Typemap for methods such as GetFieldAsBinary()
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
  $result = PyByteArray_FromStringAndSize( *$2, *$1 );
}
%typemap(freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
  if( *$1 ) {
    free( *$2 );
  }
}


%typemap(in,numinputs=1) (int nLen, char *pBuf ) (int alloc = 0, bool viewIsValid = false, Py_buffer view)
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
  {
    if (PyObject_GetBuffer($input, &view, PyBUF_SIMPLE) == 0)
    {
      if( view.len > INT_MAX ) {
        PyBuffer_Release(&view);
        SWIG_exception( SWIG_RuntimeError, "too large buffer (>2GB)" );
      }
      viewIsValid = true;
      $1 = (int) view.len;
      $2 = ($2_ltype) view.buf;
      goto ok;
    }
    else
    {
        PyErr_Clear();
    }
  }
  if (PyUnicode_Check($input))
  {
    size_t safeLen = 0;
    int ret = SWIG_AsCharPtrAndSize($input, (char**) &$2, &safeLen, &alloc);
    if (!SWIG_IsOK(ret)) {
      SWIG_exception( SWIG_RuntimeError, "invalid Unicode string" );
    }

    if (safeLen) safeLen--;
    if( safeLen > INT_MAX ) {
      SWIG_exception( SWIG_RuntimeError, "too large buffer (>2GB)" );
    }
    $1 = (int) safeLen;
  }
  else
  {
    PyErr_SetString(PyExc_TypeError, "not a unicode string, bytes, bytearray or memoryview");
    SWIG_fail;
  }
  ok: ;
}

%typemap(freearg) (int nLen, char *pBuf )
{
  /* %typemap(freearg) (int *nLen, char *pBuf ) */
  if( viewIsValid$argnum ) {
    PyBuffer_Release(&view$argnum);
  }
  else if( alloc$argnum == SWIG_NEWOBJ ) {
    delete[] $2;
  }
}


%typemap(in,numinputs=1) (GIntBig nLen, char *pBuf ) (int alloc = 0, bool viewIsValid = false, Py_buffer view)
{
  /* %typemap(in,numinputs=1) (GIntBig nLen, char *pBuf ) */
  {
    if (PyObject_GetBuffer($input, &view, PyBUF_SIMPLE) == 0)
    {
      viewIsValid = true;
      $1 = view.len;
      $2 = ($2_ltype) view.buf;
      goto ok;
    }
    else
    {
        PyErr_Clear();
    }
  }
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
  else
  {
    PyErr_SetString(PyExc_TypeError, "not a unicode string, bytes, bytearray or memoryview");
    SWIG_fail;
  }
  ok: ;
}

%typemap(freearg) (GIntBig nLen, char *pBuf )
{
  /* %typemap(freearg) (GIntBig *nLen, char *pBuf ) */
  if( viewIsValid$argnum ) {
    PyBuffer_Release(&view$argnum);
  }
  else if( alloc$argnum == SWIG_NEWOBJ ) {
    delete[] $2;
  }
}

/* required for GDALAsyncReader */

%typemap(in,numinputs=1) (int nLenKeepObject, char *pBufKeepObject, void* pyObject)
{
  /* %typemap(in,numinputs=1) (int nLenKeepObject, char *pBufKeepObject, void* pyObject) */
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
}

/* end of required for GDALAsyncReader */

/*
 * Typemap argout used in Feature::GetFieldAsIntegerList()
 */
%typemap(in,numinputs=0) (int *nLen, const int **pList) (int nLen = 0, int *pList = NULL)
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
%typemap(in,numinputs=0) (int *nLen, const GIntBig **pList) (int nLen = 0, GIntBig *pList = NULL)
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
    val = PyLong_FromString(szTmp, NULL, 10);
    PyList_SetItem( out, i, val );
  }
  $result = out;
}

/*
 * Typemap argout used in Feature::GetFieldAsDoubleList()
 */
%typemap(in,numinputs=0) (int *nLen, const double **pList) (int nLen = 0, double *pList = NULL)
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
  Py_ssize_t size = PySequence_Size($input);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
  tmpGCPList = (GDAL_GCP*) malloc($1*sizeof(GDAL_GCP));
  $2 = tmpGCPList;
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    GDAL_GCP *item = 0;
    CPL_IGNORE_RET_VAL(SWIG_ConvertPtr( o, (void**)&item, SWIGTYPE_p_GDAL_GCP, SWIG_POINTER_EXCEPTION | 0 ));
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
   Py_ssize_t size = PySequence_Size($input);
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
    Py_ssize_t size = PySequence_Size($input);
    if( size != (int)size ) {
        PyErr_SetString(PyExc_TypeError, "too big sequence");
        SWIG_fail;
    }
    for (int i = 0; i < (int)size; i++) {
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
    Py_ssize_t size = PyMapping_Length( $input );
    if ( size > 0 && size == (int)size) {
      PyObject *item_list = PyMapping_Items( $input );
      for( int i=0; i<(int)size; i++ ) {
        PyObject *it = PySequence_GetItem( item_list, i );

        PyObject *k, *v;
        if ( ! PyArg_ParseTuple( it, "OO", &k, &v ) ) {
          Py_DECREF(it);
          Py_DECREF(item_list);
          PyErr_SetString(PyExc_TypeError,"Cannot retrieve key/value");
          SWIG_fail;
        }

        PyObject* kStr = PyObject_Str(k);
        if( PyErr_Occurred() )
        {
            Py_DECREF(it);
            Py_DECREF(item_list);
            SWIG_fail;
        }

        PyObject* vStr = PyObject_Str(v);
        if( PyErr_Occurred() )
        {
            Py_DECREF(it);
            Py_DECREF(kStr);
            Py_DECREF(item_list);
            SWIG_fail;
        }

        int bFreeK, bFreeV;
        char* pszK = GDALPythonObjectToCStr(kStr, &bFreeK);
        char* pszV = GDALPythonObjectToCStr(vStr, &bFreeV);
        if( pszK == NULL || pszV == NULL )
        {
            GDALPythonFreeCStr(pszK, bFreeK);
            GDALPythonFreeCStr(pszV, bFreeV);
            Py_DECREF(kStr);
            Py_DECREF(vStr);
            Py_DECREF(it);
            Py_DECREF(item_list);
            PyErr_SetString(PyExc_TypeError,"Cannot get key/value as string");
            SWIG_fail;
        }
        $1 = CSLAddNameValue( $1, pszK, pszV );

        GDALPythonFreeCStr(pszK, bFreeK);
        GDALPythonFreeCStr(pszV, bFreeV);
        Py_DECREF(kStr);
        Py_DECREF(vStr);
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

%fragment("CSLFromPySequence","header") %{
/************************************************************************/
/*                         CSLFromPySequence()                          */
/************************************************************************/
static char **CSLFromPySequence( PyObject *pySeq, int *pbErr )

{
  *pbErr = FALSE;
  /* Check if is a list (and reject strings, that are seen as sequence of characters)  */
  if ( ! PySequence_Check(pySeq) || PyUnicode_Check(pySeq) ) {
    PyErr_SetString(PyExc_TypeError,"not a sequence");
    *pbErr = TRUE;
    return NULL;
  }

  Py_ssize_t size = PySequence_Size(pySeq);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    *pbErr = TRUE;
    return NULL;
  }
  char** papszRet = NULL;
  for (int i = 0; i < (int)size; i++) {
    PyObject* pyObj = PySequence_GetItem(pySeq,i);
    if (PyUnicode_Check(pyObj))
    {
      char *pszStr;
      Py_ssize_t nLen;
      PyObject* pyUTF8Str = PyUnicode_AsUTF8String(pyObj);
      if( !pyUTF8Str )
      {
        Py_DECREF(pyObj);
        PyErr_SetString(PyExc_TypeError,"invalid Unicode sequence");
        CSLDestroy(papszRet);
        *pbErr = TRUE;
        return NULL;
      }
      PyBytes_AsStringAndSize(pyUTF8Str, &pszStr, &nLen);
      papszRet = CSLAddString( papszRet, pszStr );
      Py_XDECREF(pyUTF8Str);
    }
    else if (PyBytes_Check(pyObj))
      papszRet = CSLAddString( papszRet, PyBytes_AsString(pyObj) );
    else
    {
        Py_DECREF(pyObj);
        PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
        CSLDestroy(papszRet);
        *pbErr = TRUE;
        return NULL;
    }
    Py_DECREF(pyObj);
  }
  return papszRet;
}
%}

/*
 * Typemap maps char** arguments from Python Sequence Object
 */
%typemap(in,fragment="CSLFromPySequence") char **options
{
  /* %typemap(in) char **options */
  int bErr = FALSE;
  $1 = CSLFromPySequence($input, &bErr);
  if( bErr )
  {
    SWIG_fail;
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
  if ( ReturnSame($1) != NULL && *$1 != NULL ) {
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
 * Typemaps for minixml:  CPLXMLNode* input, CPLXMLNode *ret
 */

%fragment("PyListToXMLTree","header") %{
/************************************************************************/
/*                          PyListToXMLTree()                           */
/************************************************************************/
static CPLXMLNode *PyListToXMLTree( PyObject *pyList )

{
    int      nChildCount = 0, iChild, nType = 0;
    CPLXMLNode *psThisNode;
    CPLXMLNode *psChild;
    char       *pszText = NULL;

    if( PyList_Size(pyList) > INT_MAX )
    {
        PyErr_SetString(PyExc_TypeError,"Error in input XMLTree." );
        return NULL;
    }
    nChildCount = static_cast<int>(PyList_Size(pyList)) - 2;
    if( nChildCount < 0 )
    {
        PyErr_SetString(PyExc_TypeError,"Error in input XMLTree." );
        return NULL;
    }

    CPL_IGNORE_RET_VAL(PyArg_Parse( PyList_GET_ITEM(pyList,0), "i", &nType ));
    CPL_IGNORE_RET_VAL(PyArg_Parse( PyList_GET_ITEM(pyList,1), "s", &pszText ));

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
        CPL_IGNORE_RET_VAL(PyArg_Parse( PyList_GET_ITEM(pyFirst,0), "i", &nTypeFirst ));
        CPL_IGNORE_RET_VAL(PyArg_Parse( PyList_GET_ITEM(pyFirst,1), "s", &pszTextFirst ));
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
  CPLDestroyXMLNode( $1 );
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

    /* In some cases 0 is passed instead of None. */
    /* See https://github.com/OSGeo/gdal/pull/219 */
    if ( PyLong_Check($input) || PyInt_Check($input) )
    {
        if( PyLong_AsLong($input) == 0 )
        {
            $input = Py_None;
        }
    }

    if ($input && $input != Py_None ) {
        void* cbfunction = NULL;
        CPL_IGNORE_RET_VAL(SWIG_ConvertPtr( $input,
                         (void**)&cbfunction,
                         SWIGTYPE_p_f_double_p_q_const__char_p_void__int,
                         SWIG_POINTER_EXCEPTION | 0 ));

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
  Py_ssize_t size = PySequence_Size($input);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
  $2 = (type**) CPLMalloc($1*sizeof(type*));

  for( int i = 0; i<$1; i++ ) {

      PyObject *o = PySequence_GetItem($input,i);
      type* rawobjectpointer = NULL;
      CPL_IGNORE_RET_VAL(SWIG_ConvertPtr( o, (void**)&rawobjectpointer, SWIGTYPE_p_##type, SWIG_POINTER_EXCEPTION | 0 ));
      if (!rawobjectpointer) {
          Py_DECREF(o);
          PyErr_SetString(PyExc_TypeError, "object of wrong type");
          SWIG_fail;
      }
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
OBJECT_LIST_INPUT(GDALDatasetShadow);

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
  CPL_IGNORE_RET_VAL(SWIG_AsVal_int($input, &requested_buckets));
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
  Py_DECREF( $result );
  if ( integerarray == NULL ) {
    $result = Py_None;
    Py_INCREF( $result );
  }
  else {
    $result = PyList_New( $1 );
    for ( int i = 0; i < $1; ++i ) {
      char szTmp[32];
      sprintf(szTmp, CPL_FRMT_GUIB, integerarray[i]);
      PyObject *o = PyLong_FromString(szTmp, NULL, 10);
      PyList_SetItem($result, i, o );
    }
  }
}

/* ***************************************************************************
 *                       GetDefaultHistogram()
 */

%typemap(arginit, noblock=1) (double *min_ret, double *max_ret, int *buckets_ret, GUIntBig **ppanHistogram)
{
   double min_val = 0.0, max_val = 0.0;
   int buckets_val = 0;
   GUIntBig *panHistogram = NULL;

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
    Py_XDECREF($result);
    if(result)
    {
        $result = GDALPythonObjectFromCStr( (const char *)result);
        CPLFree(result);
    }
    else
    {
        $result = Py_None;
        Py_INCREF($result);
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

  Py_ssize_t size = PySequence_Size($input);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
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


%fragment("DecomposeSequenceOf4DCoordinates","header") %{
static int
DecomposeSequenceOf4DCoordinates( PyObject *seq, int nCount, double *x, double *y, double *z, double *t, int *pbFoundTime )
{
  *pbFoundTime = FALSE;
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

    if (len >= 2 && len <= 4)
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
        if (len >= 3)
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

        /* The 4th coordinate is optional, default 0.0 */
        if (len >= 4)
        {
            o1 = PySequence_GetItem(o, 3);
            if (!PyNumber_Check(o1))
            {
                Py_DECREF(o); Py_DECREF(o1);
                PyErr_SetString(PyExc_TypeError, "not a number");

                return FALSE;
            }
            *pbFoundTime = TRUE;
            t[i] = PyFloat_AsDouble(o1);
            Py_DECREF(o1);
        }
        else
        {
            t[i] = 0.0;
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

%typemap(in,numinputs=1,fragment="DecomposeSequenceOf4DCoordinates") (int nCount, double *x, double *y, double *z, double *t) (int foundTime = FALSE)
{
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }

  Py_ssize_t size = PySequence_Size($input);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
  $2 = (double*) VSIMalloc($1*sizeof(double));
  $3 = (double*) VSIMalloc($1*sizeof(double));
  $4 = (double*) VSIMalloc($1*sizeof(double));
  $5 = (double*) VSIMalloc($1*sizeof(double));

  if ($2 == NULL || $3 == NULL || $4 == NULL || $5 == NULL)
  {
      PyErr_SetString( PyExc_RuntimeError, "Out of memory" );
      SWIG_fail;
  }

  if (!DecomposeSequenceOf4DCoordinates($input,$1,$2,$3,$4,$5, &foundTime)) {
    SWIG_fail;
  }
}

%typemap(argout)  (int nCount, double *x, double *y, double *z, double *t)
{
  /* %typemap(argout)  (int nCount, double *x, double *y, double *z, double *t) */
  Py_DECREF($result);
  PyObject *out = PyList_New( $1 );
  int foundTime = foundTime$argnum;
  for( int i=0; i< $1; i++ ) {
    PyObject *tuple = PyTuple_New( foundTime ? 4 : 3 );
    PyTuple_SetItem( tuple, 0, PyFloat_FromDouble( ($2)[i] ) );
    PyTuple_SetItem( tuple, 1, PyFloat_FromDouble( ($3)[i] ) );
    PyTuple_SetItem( tuple, 2, PyFloat_FromDouble( ($4)[i] ) );
    if( foundTime )
        PyTuple_SetItem( tuple, 3, PyFloat_FromDouble( ($5)[i] ) );
    PyList_SetItem( out, i, tuple );
  }
  $result = out;
}

%typemap(freearg)  (int nCount, double *x, double *y, double *z, double *t)
{
    /* %typemap(freearg)  (int nCount, double *x, double *y, double *z, double *t) */
    VSIFree($2);
    VSIFree($3);
    VSIFree($4);
    VSIFree($5);
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

  Py_ssize_t size = PySequence_Size($input);
  if( size != (int)size ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
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
 * Typemaps for Geometry.GetPoints()
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
  {
    $result = Py_None;
    Py_INCREF($result);
  }
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
  /* %typemap(argout) (void** pptr, size_t* pnsize, GDALDataType* pdatatype, int* preadonly)*/
  Py_buffer *buf=(Py_buffer*)malloc(sizeof(Py_buffer));

  if (PyBuffer_FillInfo(buf, $self, *($1), *($2), *($4), PyBUF_ND)) {
    // error, handle
  }
  if( *($3) == GDT_Byte )
  {
    buf->format = (char*) "B";
    buf->itemsize = 1;
  }
  else if( *($3) == GDT_Int16 )
  {
    buf->format = (char*) "h";
    buf->itemsize = 2;
  }
  else if( *($3) == GDT_UInt16 )
  {
    buf->format = (char*) "H";
    buf->itemsize = 2;
  }
  else if( *($3) == GDT_Int32 )
  {
    buf->format = (char*) "i";
    buf->itemsize = 4;
  }
  else if( *($3) == GDT_UInt32 )
  {
    buf->format = (char*) "I";
    buf->itemsize = 4;
  }
  else if( *($3) == GDT_Float32 )
  {
    buf->format = (char*) "f";
    buf->itemsize = 4;
  }
  else if( *($3) == GDT_Float64 )
  {
    buf->format = (char*) "F";
    buf->itemsize = 8;
  }
  else
  {
    buf->format = (char*) "B";
    buf->itemsize = 1;
  }
  Py_DECREF($result);
  $result = PyMemoryView_FromBuffer(buf);
}



%typemap(in,numinputs=0) (int *pnxvalid, int *pnyvalid, int* pisvalid) ( int nxvalid = 0, int nyvalid = 0, int isvalid = 0  )
{
  /* %typemap(in) (int *pnxvalid, int *pnyvalid, int* pisvalid) */
  $1 = &nxvalid;
  $2 = &nyvalid;
  $3 = &isvalid;
}

%typemap(argout) (int *pnxvalid, int *pnyvalid, int* pisvalid)
{
   /* %typemap(argout) (int *pnxvalid, int *pnyvalid, int* pisvalid)  */
  PyObject *r;
  if ( !*$3 ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }
  else {
    r = PyTuple_New( 2 );
    PyTuple_SetItem( r, 0, PyLong_FromLong(*$1) );
    PyTuple_SetItem( r, 1, PyLong_FromLong(*$2) );
  }
  $result = t_output_helper($result,r);
}

%typemap(in,numinputs=0) (OGRLayerShadow** ppoBelongingLayer, double* pdfProgressPct) ( OGRLayerShadow* poBelongingLayer = NULL, double dfProgressPct = 0 )
{
  /* %typemap(in) (OGRLayerShadow** ppoBelongingLayer, double* pdfProgressPct)  */
  $1 = &poBelongingLayer;
  $2 = &dfProgressPct;
}

%typemap(check) (OGRLayerShadow** ppoBelongingLayer, double* pdfProgressPct)
{
   /* %typemap(check) (OGRLayerShadow** ppoBelongingLayer, double* pdfProgressPct)  */
  if( !arg3 )
    $2 = NULL;
}

%typemap(argout) (OGRLayerShadow** ppoBelongingLayer, double* pdfProgressPct)
{
   /* %typemap(argout) (OGRLayerShadow** ppoBelongingLayer, double* pdfProgressPct)  */

  if( arg2 )
  {
    if( $result == Py_None )
    {
        $result = PyList_New(1);
        PyList_SetItem($result, 0, Py_None);
    }

    if ( !*$1 ) {
        Py_INCREF(Py_None);
        $result = SWIG_Python_AppendOutput($result, Py_None);
    }
    else {
        $result = SWIG_Python_AppendOutput($result,
            SWIG_NewPointerObj(SWIG_as_voidptr( *$1), SWIGTYPE_p_OGRLayerShadow, 0 ));
    }
  }

  if( arg3 )
  {
    if( $result == Py_None )
    {
        $result = PyList_New(1);
        PyList_SetItem($result, 0, Py_None);
    }
    $result = SWIG_Python_AppendOutput($result, PyFloat_FromDouble( *$2));
  }

}


%typemap(in,numinputs=0) (OSRSpatialReferenceShadow*** matches = NULL, int* nvalues = NULL, int** confidence_values = NULL) ( OGRSpatialReferenceH* pahSRS = NULL, int nvalues = 0, int* confidence_values = NULL )
{
  /* %typemap(in) (OSRSpatialReferenceShadow***, int* nvalues, int** confidence_values)  */
  $1 = &pahSRS;
  $2 = &nvalues;
  $3 = &confidence_values;
}

%typemap(argout) (OSRSpatialReferenceShadow*** matches = NULL, int* nvalues = NULL, int** confidence_values = NULL)
{
    /* %typemap(argout) (OOSRSpatialReferenceShadow***, int* nvalues, int** confidence_values)  */

    Py_DECREF($result);

    $result = PyList_New( *($2));
    for( int i = 0; i < *($2); i++ )
    {
        PyObject *tuple = PyTuple_New( 2 );
        PyTuple_SetItem( tuple, 0,
            SWIG_NewPointerObj(SWIG_as_voidptr((*($1))[i]), SWIGTYPE_p_OSRSpatialReferenceShadow, 1 ) );
        PyTuple_SetItem( tuple, 1, PyInt_FromLong((*($3))[i]) );
        PyList_SetItem( $result, i, tuple );
    }
    CPLFree( *($1) );
    CPLFree( *($3) );
}


/*
 * Typemap argout for GetCRSInfoListFromDatabase()
 */
%typemap(in,numinputs=0) (OSRCRSInfo*** pList, int* pnListCount) ( OSRCRSInfo **list=0, int count=0 )
{
  /* %typemap(in,numinputs=0) (OSRCRSInfo*** pList, int* pnListCount) */
  $1 = &list;
  $2 = &count;
}
%typemap(argout) (OSRCRSInfo*** pList, int* pnListCount)
{
  /* %typemap(argout) (OSRCRSInfo*** pList, int* pnListCount) */
  PyObject *dict = PyTuple_New( *$2 );
  for( int i = 0; i < *$2; i++ ) {
    OSRCRSInfo *o = new_OSRCRSInfo( (*$1)[i]->pszAuthName,
                                    (*$1)[i]->pszCode,
                                    (*$1)[i]->pszName,
                                    (*$1)[i]->eType,
                                    (*$1)[i]->bDeprecated,
                                    (*$1)[i]->bBboxValid,
                                    (*$1)[i]->dfWestLongitudeDeg,
                                    (*$1)[i]->dfSouthLatitudeDeg,
                                    (*$1)[i]->dfEastLongitudeDeg,
                                    (*$1)[i]->dfNorthLatitudeDeg,
                                    (*$1)[i]->pszAreaName,
                                    (*$1)[i]->pszProjectionMethod );

    PyTuple_SetItem(dict, i,
       SWIG_NewPointerObj((void*)o,SWIGTYPE_p_OSRCRSInfo,1) );
  }
  Py_DECREF($result);
  $result = dict;
}

%typemap(freearg) (OSRCRSInfo*** pList, int* pnListCount)
{
  /* %typemap(freearg) (OSRCRSInfo*** pList, int* pnListCount) */
  OSRDestroyCRSInfoList( *($1) );
}


/*
 * Typemap argout for GDALGroupGetAttributes()
 */
%typemap(in,numinputs=0) (GDALAttributeHS*** pattrs, size_t* pnCount) ( GDALAttributeHS** attrs=0, size_t nCount = 0 )
{
  /* %typemap(in,numinputs=0) (GDALAttributeHS*** pattrs, size_t* pnCount) */
  $1 = &attrs;
  $2 = &nCount;
}
%typemap(argout) (GDALAttributeHS*** pattrs, size_t* pnCount)
{
  /* %typemap(argout) (GDALAttributeHS*** pattrs, size_t* pnCount) */
  PyObject *list = PyList_New( *$2 );
  for( size_t i = 0; i < *$2; i++ ) {
    PyList_SetItem(list, i,
       SWIG_NewPointerObj((void*)(*$1)[i],SWIGTYPE_p_GDALAttributeHS,SWIG_POINTER_OWN) );
  }
  Py_DECREF($result);
  $result = list;
}

%typemap(freearg) (GDALAttributeHS*** pattrs, size_t* pnCount)
{
  /* %typemap(freearg) (GDALAttributeHS*** pattrs, size_t* pnCount) */
  CPLFree(*$1);
}

OBJECT_LIST_INPUT(GDALDimensionHS);


/*
 * Typemap argout for GDALMDArrayGetDimensions()
 */
%typemap(in,numinputs=0) (GDALDimensionHS*** pdims, size_t* pnCount) ( GDALDimensionHS** dims=0, size_t nCount = 0 )
{
  /* %typemap(in,numinputs=0) (GDALDimensionHS*** pdims, size_t* pnCount) */
  $1 = &dims;
  $2 = &nCount;
}
%typemap(argout) (GDALDimensionHS*** pdims, size_t* pnCount)
{
  /* %typemap(argout) (GDALDimensionHS*** pdims, size_t* pnCount) */
  PyObject *list = PyList_New( *$2 );
  for( size_t i = 0; i < *$2; i++ ) {
    PyList_SetItem(list, i,
       SWIG_NewPointerObj((void*)(*$1)[i],SWIGTYPE_p_GDALDimensionHS,SWIG_POINTER_OWN) );
  }
  Py_DECREF($result);
  $result = list;
}

%typemap(freearg) (GDALDimensionHS*** pdims, size_t* pnCount)
{
  /* %typemap(freearg) (GDALDimensionHS*** pdims, size_t* pnCount) */
  CPLFree(*$1);
}

/*
 * Typemap argout for GDALAttributeGetDimensionsSize()
 */
%typemap(in,numinputs=0) (GUIntBig** pvals, size_t* pnCount) ( GUIntBig* vals=0, size_t nCount = 0 )
{
  /* %typemap(in,numinputs=0) (GUIntBig** pvals, size_t* pnCount) */
  $1 = &vals;
  $2 = &nCount;
}
%typemap(argout) (GUIntBig** pvals, size_t* pnCount)
{
  /* %typemap(argout) (GUIntBig** pvals, size_t* pnCount) */
  PyObject *list = PyList_New( *$2 );
  for( size_t i = 0; i < *$2; i++ ) {
      char szTmp[32];
      sprintf(szTmp, CPL_FRMT_GUIB, (*$1)[i]);
      PyObject *o = PyLong_FromString(szTmp, NULL, 10);
      PyList_SetItem(list, i, o );
  }
  Py_DECREF($result);
  $result = list;
}

%typemap(freearg) (GUIntBig** pvals, size_t* pnCount)
{
  /* %typemap(freearg) (GUIntBig** pvals, size_t* pnCount) */
  CPLFree(*$1);
}

/*
 * Typemap argout for GDALAttributeReadAsIntArray()
 */
%typemap(in,numinputs=0) (int** pvals, size_t* pnCount) ( int* vals=0, size_t nCount = 0 )
{
  /* %typemap(in,numinputs=0) (int** pvals, size_t* pnCount) */
  $1 = &vals;
  $2 = &nCount;
}
%typemap(argout) (int** pvals, size_t* pnCount)
{
  /* %typemap(argout) (int** pvals, size_t* pnCount) */
  PyObject *out = PyTuple_New( *$2 );
  for( unsigned int i=0; i<*$2; i++ ) {
    PyObject *val = PyInt_FromLong( (*$1)[i] );
    PyTuple_SetItem( out, i, val );
  }
  Py_DECREF($result);
  $result = out;
}

%typemap(freearg) (int** pvals, size_t* pnCount)
{
  /* %typemap(freearg) (int** pvals, size_t* pnCount) */
  CPLFree(*$1);
}

/*
 * Typemap argout for GDALAttributeReadAsDoubleArray()
 */
%typemap(in,numinputs=0) (double** pvals, size_t* pnCount) ( double* vals=0, size_t nCount = 0 )
{
  /* %typemap(in,numinputs=0) (double** pvals, size_t* pnCount) */
  $1 = &vals;
  $2 = &nCount;
}
%typemap(argout) (double** pvals, size_t* pnCount)
{
  /* %typemap(argout) (double** pvals, size_t* pnCount) */
  PyObject *list = CreateTupleFromDoubleArray(*$1, *$2);
  Py_DECREF($result);
  $result = list;
}

%typemap(freearg) (double** pvals, size_t* pnCount)
{
  /* %typemap(freearg) (double** pvals, size_t* pnCount) */
  CPLFree(*$1);
}

/*
 * Typemap argout for GDALExtendedDataTypeGetComponents()
 */
%typemap(in,numinputs=0) (GDALEDTComponentHS*** pcomps, size_t* pnCount) ( GDALEDTComponentHS** comps=0, size_t nCount = 0 )
{
  /* %typemap(in,numinputs=0) (GDALEDTComponentHS*** pcomps, size_t* pnCount) */
  $1 = &comps;
  $2 = &nCount;
}
%typemap(argout) (GDALEDTComponentHS*** pcomps, size_t* pnCount)
{
  /* %typemap(argout) (GDALEDTComponentHS*** pcomps, size_t* pnCount) */
  PyObject *list = PyList_New( *$2 );
  for( size_t i = 0; i < *$2; i++ ) {
    PyList_SetItem(list, i,
       SWIG_NewPointerObj((void*)(*$1)[i],SWIGTYPE_p_GDALEDTComponentHS,SWIG_POINTER_OWN) );
  }
  Py_DECREF($result);
  $result = list;
}

%typemap(freearg) (GDALEDTComponentHS*** pcomps, size_t* pnCount)
{
  /* %typemap(freearg) (GDALEDTComponentHS*** pcomps, size_t* pnCount) */
  CPLFree(*$1);
}

OBJECT_LIST_INPUT(GDALEDTComponentHS)




%typemap(in,numinputs=0) (double argout[4], int errorCode[1]) ( double argout[4], int errorCode[1] )
{
  /* %typemap(in) (double argout[4], int errorCode[1]) */
  $1 = argout;
  $2 = errorCode;
}

%typemap(argout) (double argout[4], int errorCode[1])
{
   /* %typemap(argout) (double argout[4], int errorCode[1])  */
  PyObject *r = PyTuple_New( 5 );
  PyTuple_SetItem( r, 0, PyFloat_FromDouble($1[0]));
  PyTuple_SetItem( r, 1, PyFloat_FromDouble($1[1]));
  PyTuple_SetItem( r, 2, PyFloat_FromDouble($1[2]));
  PyTuple_SetItem( r, 3, PyFloat_FromDouble($1[3]));
  PyTuple_SetItem( r, 4, PyLong_FromLong($2[0]));
  $result = t_output_helper($result,r);
}


%typemap(in) GDALRIOResampleAlg
{
    // %typemap(in) GDALRIOResampleAlg
    int val = 0;
    int ecode = SWIG_AsVal_int($input, &val);
    if (!SWIG_IsOK(ecode)) {
        SWIG_exception_fail(SWIG_ArgError(ecode), "invalid value for GDALRIOResampleAlg");
    }
    if( val < 0 ||
        ( val >= static_cast<int>(GRIORA_RESERVED_START) &&
          val <= static_cast<int>(GRIORA_RESERVED_END) ) ||
        val > static_cast<int>(GRIORA_LAST) )
    {
        SWIG_exception_fail(SWIG_ValueError, "Invalid value for resample_alg");
    }
    $1 = static_cast< GDALRIOResampleAlg >(val);
}


%typemap(in) GDALDataType
{
    // %typemap(in) GDALDataType
    int val = 0;
    int ecode = SWIG_AsVal_int($input, &val);
    if (!SWIG_IsOK(ecode)) {
        SWIG_exception_fail(SWIG_ArgError(ecode), "invalid value for GDALDataType");
    }
    if( val < GDT_Unknown || val >= GDT_TypeCount )
    {
        SWIG_exception_fail(SWIG_ValueError, "Invalid value for GDALDataType");
    }
    $1 = static_cast<GDALDataType>(val);
}

%typemap(in) (GDALDataType *optional_GDALDataType) ( GDALDataType val )
{
  /* %typemap(in) (GDALDataType *optional_GDALDataType) */
  int intval = 0;
  if ( $input == Py_None ) {
    $1 = NULL;
  }
  else if ( SWIG_IsOK(SWIG_AsVal_int($input, &intval)) ) {
    if( intval < GDT_Unknown || intval >= GDT_TypeCount )
    {
        SWIG_exception_fail(SWIG_ValueError, "Invalid value for GDALDataType");
    }
    val = static_cast<GDALDataType>(intval);
    $1 = &val;
  }
  else {
    PyErr_SetString( PyExc_TypeError, "Invalid Parameter" );
    SWIG_fail;
  }
}

%typemap(typecheck,precedence=0) (GDALDataType *optional_GDALDataType)
{
  /* %typemap(typecheck,precedence=0) (GDALDataType *optional_GDALDataType) */
  $1 = (($input==Py_None) || my_PyCheck_int($input)) ? 1 : 0;
}
