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
  if ( result != 0 && GetUseExceptions()) {
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
    $1 = (GIntBig)PyLong_AsLongLong($input);
}

%typemap(out) GIntBig
{
    $result = PyLong_FromLongLong($1);
}

%typemap(in) GUIntBig
{
    $1 = (GIntBig)PyLong_AsUnsignedLongLong($input);
}

%typemap(out) GUIntBig
{
    $result = PyLong_FromUnsignedLongLong($1);
}

%typemap(in) VoidPtrAsLong
{
    $1 = PyLong_AsVoidPtr($input);
}

%typemap(out) VoidPtrAsLong
{
    $result = PyLong_FromVoidPtr($1);
}

/*
 * double *val, int*hasval, is a special contrived typemap used for
 * the RasterBand GetNoDataValue, GetMinimum, GetMaximum, GetOffset, GetScale methods.
 * In the python bindings, the variable hasval is tested.  If it is 0 (is, the value
 * is not set in the raster band) then Py_None is returned.  If it is != 0, then
 * the value is coerced into a long and returned.
 */
%typemap(in,numinputs=0) (double *val, int *hasval) ( double tmpval, int tmphasval ) {
  /* %typemap(python,in,numinputs=0) (double *val, int *hasval) */
  $1 = &tmpval;
  $2 = &tmphasval;
}
%typemap(argout) (double *val, int *hasval) {
  /* %typemap(python,argout) (double *val, int *hasval) */
  PyObject *r;
  if ( !*$2 ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }
  else {
    r = PyFloat_FromDouble( *$1 );
  }
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,r);
%#endif
}


%typemap(in,numinputs=0) (GIntBig *val, int *hasval) ( GIntBig tmpval, int tmphasval ) {
  /* %typemap(python,in,numinputs=0) (GIntBig *val, int *hasval) */
  $1 = &tmpval;
  $2 = &tmphasval;
}
%typemap(argout) (GIntBig *val, int *hasval) {
  /* %typemap(python,argout) (GIntBig *val, int *hasval) */
  PyObject *r;
  if ( !*$2 ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }
  else {
    r = PyLong_FromLongLong( *$1 );
  }
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,r);
%#endif
}


%typemap(in,numinputs=0) (GUIntBig *val, int *hasval) ( GUIntBig tmpval, int tmphasval ) {
  /* %typemap(python,in,numinputs=0) (GUIntBig *val, int *hasval) */
  $1 = &tmpval;
  $2 = &tmphasval;
}
%typemap(argout) (GUIntBig *val, int *hasval) {
  /* %typemap(python,argout) (GUIntBig *val, int *hasval) */
  PyObject *r;
  if ( !*$2 ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }
  else {
    r = PyLong_FromUnsignedLongLong( *$1 );
  }
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,r);
%#endif
}


%define TYPEMAP_IN_ARGOUT_ARRAY_IS_VALID(num_values)
%typemap(in,numinputs=0) (double argout[num_values], int* isvalid) ( double argout[num_values], int isvalid )
{
  /* %typemap(in,numinputs=0) (double argout[num_values], int* isvalid) */
  $1 = argout;
  $2 = &isvalid;
}
%enddef

%define TYPEMAP_ARGOUT_ARGOUT_ARRAY_IS_VALID(num_values)
%typemap(argout, fragment="CreateTupleFromDoubleArray") (double argout[num_values], int* isvalid)
{
   /* %typemap(argout) (double argout[num_values], int* isvalid)  */
  PyObject *r;
  if ( !*$2 ) {
    Py_INCREF(Py_None);
    r = Py_None;
  }
  else {
    r = CreateTupleFromDoubleArray($1, num_values);
  }
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,r);
%#endif
}
%enddef

TYPEMAP_IN_ARGOUT_ARRAY_IS_VALID(2)
TYPEMAP_ARGOUT_ARGOUT_ARRAY_IS_VALID(2)

TYPEMAP_IN_ARGOUT_ARRAY_IS_VALID(4)
TYPEMAP_ARGOUT_ARGOUT_ARRAY_IS_VALID(4)

TYPEMAP_IN_ARGOUT_ARRAY_IS_VALID(6)
TYPEMAP_ARGOUT_ARGOUT_ARRAY_IS_VALID(6)


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
  if ( result != 0 && GetUseExceptions()) {
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
%typemap(argout,fragment="CreateTupleFromDoubleArray") ( double argout[ANY])
{
  /* %typemap(argout) (double argout[ANY]) */
  PyObject *out = CreateTupleFromDoubleArray( $1, $dim0 );
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,out,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,out);
%#endif
}

%typemap(in,numinputs=0) ( double *argout[ANY]) (double *argout)
{
  /* %typemap(in,numinputs=0) (double *argout[ANY]) */
  argout = NULL;
  $1 = &argout;
}
%typemap(argout,fragment="CreateTupleFromDoubleArray") ( double *argout[ANY])
{
  /* %typemap(argout) (double *argout[ANY]) */
  PyObject *out = CreateTupleFromDoubleArray( *$1, $dim0 );
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,out,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,out);
%#endif
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
  if( size > (Py_ssize_t)INT_MAX ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  if( (size_t)size > SIZE_MAX / sizeof(int) ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  *pnSize = (int)size;
  int* ret = (int*) malloc((*pnSize)*sizeof(int));
  if( !ret ) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    *pnSize = -1;
    return NULL;
  }
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

%fragment("CreateCGIntBigListFromSequence","header") %{
static GIntBig*
CreateCGIntBigListFromSequence( PyObject* pySeq, int* pnSize ) {
  /* check if is List */
  if ( !PySequence_Check(pySeq) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    *pnSize = -1;
    return NULL;
  }
  Py_ssize_t size = PySequence_Size(pySeq);
  if( size > (Py_ssize_t)INT_MAX ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  if( (size_t)size > SIZE_MAX / sizeof(GIntBig) ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  *pnSize = (int)size;
  GIntBig* ret = (GIntBig*) malloc((*pnSize)*sizeof(GIntBig));
  if( !ret ) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    *pnSize = -1;
    return NULL;
  }
  for( int i = 0; i<*pnSize; i++ ) {
    PyObject *o = PySequence_GetItem(pySeq,i);
    if ( !PyArg_Parse(o,"L",&ret[i]) ) {
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
 *  Typemap for counted arrays of GIntBig <- PySequence
 */
%typemap(in,numinputs=1,fragment="CreateCGIntBigListFromSequence") (int nList, GIntBig* pList)
{
  /* %typemap(in,numinputs=1) (int nList, GIntBig* pList)*/
  $2 = CreateCGIntBigListFromSequence($input, &$1);
  if( $1 < 0 ) {
    SWIG_fail;
  }
}

%typemap(freearg) (int nList, GIntBig* pList)
{
  /* %typemap(freearg) (int nList, GIntBig* pList) */
  free($2);
}

%fragment("CreateCGUIntBigListFromSequence","header") %{
static GUIntBig*
CreateCGUIntBigListFromSequence( PyObject* pySeq, int* pnSize ) {
  /* check if is List */
  if ( !PySequence_Check(pySeq) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    *pnSize = -1;
    return NULL;
  }
  Py_ssize_t size = PySequence_Size(pySeq);
  if( size > (Py_ssize_t)INT_MAX ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  if( (size_t)size > SIZE_MAX / sizeof(GUIntBig) ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  *pnSize = (int)size;
  GUIntBig* ret = (GUIntBig*) malloc((*pnSize)*sizeof(GUIntBig));
  if( !ret ) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    *pnSize = -1;
    return NULL;
  }
  for( int i = 0; i<*pnSize; i++ ) {
    PyObject *o = PySequence_GetItem(pySeq,i);
    if ( !PyArg_Parse(o,"K",&ret[i]) ) {
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
 *  Typemap for counted arrays of GUIntBig <- PySequence
 */
%typemap(in,numinputs=1,fragment="CreateCGUIntBigListFromSequence") (int nList, GUIntBig* pList)
{
  /* %typemap(in,numinputs=1) (int nList, GUIntBig* pList)*/
  $2 = CreateCGUIntBigListFromSequence($input, &$1);
  if( $1 < 0 ) {
    SWIG_fail;
  }
}

%typemap(freearg) (int nList, GUIntBig* pList)
{
  /* %typemap(freearg) (int nList, GUIntBig* pList) */
  free($2);
}

%fragment("CreateCDoubleListFromSequence","header") %{
static double*
CreateCDoubleListFromSequence( PyObject* pySeq, int* pnSize ) {
  /* check if is List */
  if ( !PySequence_Check(pySeq) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    *pnSize = -1;
    return NULL;
  }
  Py_ssize_t size = PySequence_Size(pySeq);
  if( size > (Py_ssize_t)INT_MAX ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  if( (size_t)size > SIZE_MAX / sizeof(double) ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    *pnSize = -1;
    return NULL;
  }
  *pnSize = (int)size;
  double* ret = (double*) malloc((*pnSize)*sizeof(double));
  if( !ret ) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    *pnSize = -1;
    return NULL;
  }
  for( int i = 0; i<*pnSize; i++ ) {
    PyObject *o = PySequence_GetItem(pySeq,i);
    if ( !PyArg_Parse(o,"d",&ret[i]) ) {
        PyErr_SetString(PyExc_TypeError, "not an number");
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
 *  Typemap for counted arrays of doubles <- PySequence
 */
%typemap(in,numinputs=1,fragment="CreateCDoubleListFromSequence") (int nList, double* pList)
{
  /* %typemap(in,numinputs=1) (int nList, double* pList)*/
  $2 = CreateCDoubleListFromSequence($input, &$1);
  if( $1 < 0 ) {
    SWIG_fail;
  }
}

%typemap(freearg) (int nList, double* pList)
{
  /* %typemap(freearg) (int nList, double* pList) */
  free($2);
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
  VSIFree( *$2 );
}

%typemap(in,numinputs=0) (size_t *nLen, char **pBuf ) ( size_t nLen = 0, char *pBuf = 0 )
{
  /* %typemap(in,numinputs=0) (size_t *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}
%typemap(argout) (size_t *nLen, char **pBuf )
{
  /* %typemap(argout) (size_t *nLen, char **pBuf ) */
  Py_XDECREF($result);
  if( *$2 ) {
      $result = PyByteArray_FromStringAndSize( *$2, *$1 );
  }
  else {
      $result = Py_None;
      Py_INCREF(Py_None);
  }
}
%typemap(freearg) (size_t *nLen, char **pBuf )
{
  /* %typemap(freearg) (size_t *nLen, char **pBuf ) */
  VSIFree( *$2 );
}


%fragment("GetBufferAsCharPtrIntSize","header") %{
static bool
GetBufferAsCharPtrIntSize( PyObject* input, int *nLen, char **pBuf, int *alloc, bool *viewIsValid, Py_buffer *view ) {
  {
    if (PyObject_GetBuffer(input, view, PyBUF_SIMPLE) == 0)
    {
      if( view->len > INT_MAX ) {
        PyBuffer_Release(view);
        PyErr_SetString(PyExc_RuntimeError, "too large buffer (>2GB)" );
        return false;
      }
      *viewIsValid = true;
      *nLen = (int) view->len;
      *pBuf = (char*) view->buf;
      return true;
    }
    else
    {
      PyErr_Clear();
    }
  }
  if (PyUnicode_Check(input))
  {
    size_t safeLen = 0;
    int ret;
    try {
      ret = SWIG_AsCharPtrAndSize(input, pBuf, &safeLen, alloc);
    }
    catch( const std::exception& )
    {
      PyErr_SetString(PyExc_MemoryError, "out of memory");
      return false;
    }
    if (!SWIG_IsOK(ret)) {
      PyErr_SetString(PyExc_RuntimeError, "invalid Unicode string" );
      return false;
    }

    if (safeLen) safeLen--;
    if( safeLen > INT_MAX ) {
      PyErr_SetString(PyExc_RuntimeError, "too large buffer (>2GB)" );
      return false;
    }
    *nLen = (int) safeLen;
    return true;
  }
  else
  {
    PyErr_SetString(PyExc_RuntimeError, "not a unicode string, bytes, bytearray or memoryview");
    return false;
  }
}
%}

%typemap(in,numinputs=1,fragment="GetBufferAsCharPtrIntSize") (int nLen, char *pBuf ) (int alloc = 0, bool viewIsValid = false, Py_buffer view)
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
  char* ptr = NULL;
  if( !GetBufferAsCharPtrIntSize($input, &$1, &ptr, &alloc, &viewIsValid, &view) ) {
      SWIG_fail;
  }
  $2 = ($2_ltype)ptr;
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


%fragment("GetBufferAsCharPtrSizetSize","header") %{
static bool
GetBufferAsCharPtrSizetSize( PyObject* input, size_t *nLen, char **pBuf, int *alloc, bool *viewIsValid, Py_buffer *view ) {
  {
    if (PyObject_GetBuffer(input, view, PyBUF_SIMPLE) == 0)
    {
      *viewIsValid = true;
      *nLen = view->len;
      *pBuf = (char*) view->buf;
      return true;
    }
    else
    {
      PyErr_Clear();
    }
  }
  if (PyUnicode_Check(input))
  {
    size_t safeLen = 0;
    int ret;
    try {
      ret = SWIG_AsCharPtrAndSize(input, pBuf, &safeLen, alloc);
    }
    catch( const std::exception& )
    {
      PyErr_SetString(PyExc_MemoryError, "out of memory");
      return false;
    }
    if (!SWIG_IsOK(ret)) {
      PyErr_SetString(PyExc_RuntimeError, "invalid Unicode string" );
      return false;
    }

    if (safeLen) safeLen--;
    *nLen = safeLen;
    return true;
  }
  else
  {
    PyErr_SetString(PyExc_RuntimeError, "not a unicode string, bytes, bytearray or memoryview");
    return false;
  }
}
%}

%typemap(in,numinputs=1,fragment="GetBufferAsCharPtrSizetSize") (size_t nLen, char *pBuf ) (int alloc = 0, bool viewIsValid = false, Py_buffer view)
{
  /* %typemap(in,numinputs=1) (size_t nLen, char *pBuf ) */
  char* ptr = NULL;
  if( !GetBufferAsCharPtrSizetSize($input, &$1, &ptr, &alloc, &viewIsValid, &view) ) {
      SWIG_fail;
  }
  $2 = ($2_ltype)ptr;
}

%typemap(freearg) (size_t nLen, char *pBuf )
{
  /* %typemap(freearg) (size_t *nLen, char *pBuf ) */
  if( viewIsValid$argnum ) {
    PyBuffer_Release(&view$argnum);
  }
  else if( alloc$argnum == SWIG_NEWOBJ ) {
    delete[] $2;
  }
}

%fragment("GetBufferAsCharPtrGIntBigSize","header") %{
static bool
GetBufferAsCharPtrGIntBigSize( PyObject* input, GIntBig *nLen, char **pBuf, int *alloc, bool *viewIsValid, Py_buffer *view ) {
  {
    if (PyObject_GetBuffer(input, view, PyBUF_SIMPLE) == 0)
    {
      *viewIsValid = true;
      *nLen = view->len;
      *pBuf = (char*) view->buf;
      return true;
    }
    else
    {
      PyErr_Clear();
    }
  }
  if (PyUnicode_Check(input))
  {
    size_t safeLen = 0;
    int ret;
    try {
      ret = SWIG_AsCharPtrAndSize(input, pBuf, &safeLen, alloc);
    }
    catch( const std::exception& )
    {
      PyErr_SetString(PyExc_MemoryError, "out of memory");
      return false;
    }
    if (!SWIG_IsOK(ret)) {
      PyErr_SetString(PyExc_RuntimeError, "invalid Unicode string" );
      return false;
    }

    if (safeLen) safeLen--;
    *nLen = (GIntBig)safeLen;
    return true;
  }
  else
  {
    PyErr_SetString(PyExc_RuntimeError, "not a unicode string, bytes, bytearray or memoryview");
    return false;
  }
}
%}

%typemap(in,numinputs=1,fragment="GetBufferAsCharPtrGIntBigSize") (GIntBig nLen, char *pBuf ) (int alloc = 0, bool viewIsValid = false, Py_buffer view)
{
  /* %typemap(in,numinputs=1) (GIntBig nLen, char *pBuf ) */
  char* ptr = NULL;
  if( !GetBufferAsCharPtrGIntBigSize($input, &$1, &ptr, &alloc, &viewIsValid, &view) ) {
      SWIG_fail;
  }
  $2 = ($2_ltype)ptr;
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

%typemap(in,numinputs=1) (size_t nLenKeepObject, char *pBufKeepObject, void* pyObject)
{
  /* %typemap(in,numinputs=1) (size_t nLenKeepObject, char *pBufKeepObject, void* pyObject) */
  if (PyBytes_Check($input))
  {
    Py_ssize_t safeLen = 0;
    PyBytes_AsStringAndSize($input, (char**) &$2, &safeLen);
    $1 = safeLen;
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
  if( !out ) {
      SWIG_fail;
  }
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
  if( !out ) {
      SWIG_fail;
  }
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
  if( !out ) {
      SWIG_fail;
  }
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
  if( size > (Py_ssize_t)INT_MAX ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  if( (size_t)size > SIZE_MAX / sizeof(GDAL_GCP) ) {
    PyErr_SetString(PyExc_RuntimeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
  tmpGCPList = (GDAL_GCP*) malloc($1*sizeof(GDAL_GCP));
  if( !tmpGCPList ) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    SWIG_fail;
  }
  $2 = tmpGCPList;
  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    GDAL_GCP *item = 0;
    CPL_IGNORE_RET_VAL(SWIG_ConvertPtr( o, (void**)&item, SWIGTYPE_p_GDAL_GCP, SWIG_POINTER_EXCEPTION | 0 ));
    if ( ! item ) {
      Py_DECREF(o);
      SWIG_fail;
    }
    memcpy( tmpGCPList + i, item, sizeof( GDAL_GCP ) );
    Py_DECREF(o);
  }
}
%typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs )
{
  /* %typemap(freearg) (int nGCPs, GDAL_GCP const *pGCPs ) */
  free( $2 );
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


%fragment("GDALPythonObjectFromCStrAndSize","header") %{
/* Return a PyObject* from a C String */
static PyObject* GDALPythonObjectFromCStrAndSize(const char *pszStr, size_t nLen)
{
  const unsigned char* pszIter = (const unsigned char*) pszStr;
  for( size_t i = 0; i < nLen; ++i)
  {
    if (pszIter[i] > 127)
    {
        PyObject* pyObj = PyUnicode_DecodeUTF8(pszStr, nLen, "strict");
        if (pyObj != NULL && !PyErr_Occurred())
            return pyObj;
        PyErr_Clear();
        return PyBytes_FromStringAndSize(pszStr, nLen);
    }
  }
  return PyUnicode_FromStringAndSize(pszStr, nLen);
}
%}

%fragment("GetCSLStringAsPyDict","header",fragment="GDALPythonObjectFromCStrAndSize") %{
static PyObject*
GetCSLStringAsPyDict( char **stringarray, bool bFreeCSL ) {
  PyObject* dict = PyDict_New();
  if ( stringarray != NULL ) {
    for (char** iter = stringarray; *iter; ++iter ) {
      const char* pszSep = strchr( *iter, '=' );
      if ( pszSep != NULL) {
        const char* keyptr = *iter;
        const char* valptr = pszSep + 1;
        PyObject *nm = GDALPythonObjectFromCStrAndSize( keyptr, (size_t)(pszSep - keyptr) );
        PyObject *val = GDALPythonObjectFromCStr( valptr );
        PyDict_SetItem(dict, nm, val );
        Py_DECREF(nm);
        Py_DECREF(val);
      }
    }
  }
  if( bFreeCSL )
    CSLDestroy(stringarray);
  return dict;
}
%}

/*
 * Typemap char ** -> dict
 */
%typemap(out,fragment="GetCSLStringAsPyDict") char **dict
{
  /* %typemap(out) char **dict */
  $result = GetCSLStringAsPyDict($1, false);
}

/*
 * Typemap char ** -> dict and CSLDestroy()
 */
%typemap(out,fragment="GetCSLStringAsPyDict") char **dictAndCSLDestroy
{
  /* %typemap(out) char **dict */
  $result = GetCSLStringAsPyDict($1, true);
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
  if( size > (Py_ssize_t)(INT_MAX - 1) ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    *pbErr = TRUE;
    return NULL;
  }
  if( size == 0 ) {
    return NULL;
  }
  char** papszRet = (char**) VSICalloc((int)size + 1, sizeof(char*));
  if( !papszRet ) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    *pbErr = TRUE;
    return NULL;
  }
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
      papszRet[i] = VSIStrdup(pszStr);
      Py_XDECREF(pyUTF8Str);
    }
    else if (PyBytes_Check(pyObj))
      papszRet[i] = VSIStrdup(PyBytes_AsString(pyObj));
    else
    {
        Py_DECREF(pyObj);
        PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
        CSLDestroy(papszRet);
        *pbErr = TRUE;
        return NULL;
    }
    Py_DECREF(pyObj);
    if( !papszRet[i] )
    {
        PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
        CSLDestroy(papszRet);
        *pbErr = TRUE;
        return NULL;
    }
  }
  return papszRet;
}
%}

%fragment("CSLFromPyMapping","header") %{
static char **CSLFromPyMapping( PyObject *pyObj, int *pbErr )

{
    char** retCSL = NULL;
    Py_ssize_t size = PyMapping_Length( pyObj );
    if ( size > 0 && size == (int)size) {
      PyObject *item_list = PyMapping_Items( pyObj );
      for( int i=0; i<(int)size; i++ ) {
        PyObject *it = PySequence_GetItem( item_list, i );

        PyObject *k, *v;
        if ( ! PyArg_ParseTuple( it, "OO", &k, &v ) ) {
          Py_DECREF(it);
          Py_DECREF(item_list);
          PyErr_SetString(PyExc_TypeError,"Cannot retrieve key/value");
          CSLDestroy(retCSL);
          *pbErr = TRUE;
          return NULL;
        }

        PyObject* kStr = PyObject_Str(k);
        if( PyErr_Occurred() )
        {
            Py_DECREF(it);
            Py_DECREF(item_list);
            CSLDestroy(retCSL);
            *pbErr = TRUE;
            return NULL;
        }

        PyObject* vStr;
        if( PyBytes_Check(v) )
        {
            vStr = v;
            Py_INCREF(vStr);
        }
        else
        {
            vStr = PyObject_Str(v);
            if( PyErr_Occurred() )
            {
                Py_DECREF(it);
                Py_DECREF(kStr);
                Py_DECREF(item_list);
                CSLDestroy(retCSL);
                *pbErr = TRUE;
                return NULL;
            }
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
            CSLDestroy(retCSL);
            *pbErr = TRUE;
            return NULL;
        }
        retCSL = CSLAddNameValue( retCSL, pszK, pszV );

        GDALPythonFreeCStr(pszK, bFreeK);
        GDALPythonFreeCStr(pszV, bFreeV);
        Py_DECREF(kStr);
        Py_DECREF(vStr);
        Py_DECREF(it);
      }
      Py_DECREF(item_list);
    }
    *pbErr = FALSE;
    return retCSL;
}
%}

%typemap(in, fragment="CSLFromPySequence,CSLFromPyMapping") char **dict
{
  /* %typemap(in) char **dict */
  $1 = NULL;
  if ( PySequence_Check( $input ) ) {
    int bErr = FALSE;
    $1 = CSLFromPySequence($input, &bErr);
    if ( bErr )
    {
        SWIG_fail;
    }
  }
  else if ( PyMapping_Check( $input ) ) {
    int bErr = FALSE;
    $1 = CSLFromPyMapping($input, &bErr);
    if ( bErr )
    {
        SWIG_fail;
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

%apply char **dict { char **options};


%fragment("CSLToList","header") %{
static PyObject* CSLToList( char** stringarray, bool *pbErr )
{
  PyObject* res;
  if ( stringarray == NULL ) {
    res = Py_None;
    Py_INCREF( res );
  }
  else {
    int len = CSLCount( stringarray );
    res = PyList_New( len );
    if( !res ) {
      *pbErr = true;
      return res;
    }
    for ( int i = 0; i < len; ++i ) {
      PyObject *o = GDALPythonObjectFromCStr( stringarray[i] );
      PyList_SetItem(res, i, o );
    }
  }
  *pbErr = false;
  return res;
}
%}

/*
 * Typemap converts an array of strings into a list of strings
 * with the assumption that the called object maintains ownership of the
 * array of strings.
 */
%typemap(out,fragment="CSLToList") char **options
{
  /* %typemap(out) char **options -> ( string ) */
  bool bErr = false;
  $result = CSLToList($1, &bErr);
  if( bErr ) {
    SWIG_fail;
  }
}

/* Almost same as %typemap(out) char **options */
/* but we CSLDestroy the char** pointer at the end */
%typemap(out,fragment="CSLToList") char **CSL
{
  /* %typemap(out) char **CSL -> ( string ) */
  bool bErr = false;
  $result = CSLToList($1, &bErr);
  CSLDestroy($1);
  if( bErr ) {
    SWIG_fail;
  }
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
%typemap(argout) (char **argout)
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
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,o,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,o);
%#endif
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
  if( size > (Py_ssize_t)INT_MAX ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  if( (size_t)size > SIZE_MAX / sizeof(type*) ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
  $2 = (type**) VSIMalloc($1*sizeof(type*));
  if( !$2) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    SWIG_fail;
  }

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
    if (requested_buckets <= 0 || (size_t)requested_buckets > SIZE_MAX / sizeof(GUIntBig))
    {
        PyErr_SetString( PyExc_RuntimeError, "Bad value for buckets" );
        SWIG_fail;
    }
    void* tmp = VSIRealloc($2, sizeof(GUIntBig) * requested_buckets);
    if( !tmp) {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
      SWIG_fail;
    }
    $2 = (GUIntBig *)tmp;
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
  VSIFree( $2 );
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
    if( !$result ) {
      SWIG_fail;
    }
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
      if( !psList ) {
          SWIG_fail;
      }
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
  if( !out ) {
      SWIG_fail;
  }
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
  if( !out ) {
    SWIG_fail;
  }
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
  if( !xyz ) {
      SWIG_fail;
  }
  PyObject *success = PyList_New( $1 );
  if( !success ) {
      Py_DECREF(xyz);
      SWIG_fail;
  }
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
    Py_INCREF(Py_None);
    $result = Py_None;
  }
  else
  {
    PyObject *xyz = PyList_New( nPointCount );
    if( !xyz ) {
        SWIG_fail;
    }
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
    if (PyUnicode_Check($input) || PyBytes_Check($input))
    {
        $1 = GDALPythonObjectToCStr( $input, &bToFree );
    }
    else
    {
        $1 = GDALPythonPathToCStr($input, &bToFree);

    }
    if ($1 == NULL)
    {
        PyErr_SetString( PyExc_RuntimeError, "not a string or os.PathLike" );
        SWIG_fail;
    }
}

%typemap(freearg)(const char *utf8_path)
{
    /* %typemap(freearg) (const char *utf8_path) */
    GDALPythonFreeCStr($1, bToFree$argnum);
}

%typemap(in) (const char *utf8_path_or_none) (int bToFree = 0)
{
    /* %typemap(in) (const char *utf8_path_or_none) */
    if( $input == Py_None )
    {
        $1 = NULL;
    }
    else
    {
        $1 = GDALPythonObjectToCStr( $input, &bToFree );
        if ($1 == NULL)
        {
            PyErr_SetString( PyExc_RuntimeError, "not a string" );
            SWIG_fail;
        }
        }
}

%typemap(freearg)(const char *utf8_path_or_none)
{
    /* %typemap(freearg) (const char *utf8_path_or_none) */
    if( $1 != NULL )
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
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,r);
%#endif
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

    PyObject* r;
    if ( !*$1 ) {
        r = Py_None;
        Py_INCREF(Py_None);
    }
    else {
        r = SWIG_NewPointerObj(SWIG_as_voidptr( *$1), SWIGTYPE_p_OGRLayerShadow, 0 );
    }
%#if SWIG_VERSION >= 0x040300
    $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
    $result = SWIG_Python_AppendOutput($result,r);
%#endif
  }

  if( arg3 )
  {
    if( $result == Py_None )
    {
        $result = PyList_New(1);
        PyList_SetItem($result, 0, Py_None);
    }
    PyObject* r = PyFloat_FromDouble( *$2);
%#if SWIG_VERSION >= 0x040300
    $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
    $result = SWIG_Python_AppendOutput($result,r);
%#endif
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
    if( !$result ) {
      SWIG_fail;
    }
    for( int i = 0; i < *($2); i++ )
    {
        PyObject *tuple = PyTuple_New( 2 );
        OSRReference((*($1))[i]);
        PyTuple_SetItem( tuple, 0,
            SWIG_NewPointerObj(SWIG_as_voidptr((*($1))[i]), SWIGTYPE_p_OSRSpatialReferenceShadow, 1 ) );
        PyTuple_SetItem( tuple, 1, PyInt_FromLong((*($3))[i]) );
        PyList_SetItem( $result, i, tuple );
    }
}

%typemap(freearg) (OSRSpatialReferenceShadow*** matches = NULL, int* nvalues = NULL, int** confidence_values = NULL)
{
    /* %typemap(freearg) (OOSRSpatialReferenceShadow***, int* nvalues, int** confidence_values)  */
    OSRFreeSRSArray( *($1) );
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
  Py_DECREF($result);
  $result = PyList_New( *$2 );
  if( !$result ) {
    SWIG_fail;
  }
  for( size_t i = 0; i < *$2; i++ ) {
    PyList_SetItem($result, i,
       SWIG_NewPointerObj((void*)(*$1)[i],SWIGTYPE_p_GDALAttributeHS,SWIG_POINTER_OWN) );
    /* We have borrowed the GDALAttributeHS */
    (*$1)[i] = NULL;
  }
}

%typemap(freearg) (GDALAttributeHS*** pattrs, size_t* pnCount)
{
  /* %typemap(freearg) (GDALAttributeHS*** pattrs, size_t* pnCount) */
  GDALReleaseAttributes(*$1, *$2);
}

OBJECT_LIST_INPUT(GDALDimensionHS);


%define OBJECT_LIST_INPUT_ITEM_MAY_BE_NULL(type)
%typemap(in, numinputs=1) (int object_list_count, type **poObjectsItemMaybeNull)
{
  /*  OBJECT_LIST_INPUT %typemap(in) (int itemcount, type *optional_##type)*/
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  Py_ssize_t size = PySequence_Size($input);
  if( size > (Py_ssize_t)INT_MAX ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  if( (size_t)size > SIZE_MAX / sizeof(type*) ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
  $2 = (type**) VSIMalloc($1*sizeof(type*));
  if( !$2) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    SWIG_fail;
  }

  for( int i = 0; i<$1; i++ ) {

      PyObject *o = PySequence_GetItem($input,i);
      type* rawobjectpointer = NULL;
      if( o != Py_None )
      {
          CPL_IGNORE_RET_VAL(SWIG_ConvertPtr( o, (void**)&rawobjectpointer, SWIGTYPE_p_##type, SWIG_POINTER_EXCEPTION | 0 ));
          if (!rawobjectpointer) {
              Py_DECREF(o);
              PyErr_SetString(PyExc_TypeError, "object of wrong type");
              SWIG_fail;
          }
      }
      $2[i] = rawobjectpointer;
      Py_DECREF(o);

  }
}

%typemap(freearg)  (int object_list_count, type **poObjectsItemMaybeNull)
{
  /* OBJECT_LIST_INPUT %typemap(freearg) (int object_list_count, type **poObjectsItemMaybeNull)*/
  CPLFree( $2 );
}
%enddef

OBJECT_LIST_INPUT_ITEM_MAY_BE_NULL(GDALDimensionHS);


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
  Py_DECREF($result);
  $result = PyList_New( *$2 );
  if( !$result ) {
    SWIG_fail;
  }
  for( size_t i = 0; i < *$2; i++ ) {
    PyList_SetItem($result, i,
       SWIG_NewPointerObj((void*)(*$1)[i],SWIGTYPE_p_GDALDimensionHS,SWIG_POINTER_OWN) );
    /* We have borrowed the GDALDimensionHS */
    (*$1)[i] = NULL;
  }
}

%typemap(freearg) (GDALDimensionHS*** pdims, size_t* pnCount)
{
  /* %typemap(freearg) (GDALDimensionHS*** pdims, size_t* pnCount) */
  GDALReleaseDimensions(*$1, *$2);
}


/*
 * Typemap argout for GDALMDArrayGetIndexingVariables()
 */
%typemap(in,numinputs=0) (GDALMDArrayHS*** parrays, size_t* pnCount) ( GDALMDArrayHS** arrays=0, size_t nCount = 0 )
{
  /* %typemap(in,numinputs=0) (GDALMDArrayHS*** parrays, size_t* pnCount) */
  $1 = &arrays;
  $2 = &nCount;
}
%typemap(argout) (GDALMDArrayHS*** parrays, size_t* pnCount)
{
  /* %typemap(argout) (GDALMDArrayHS*** parrays, size_t* pnCount) */
  Py_DECREF($result);
  $result = PyList_New( *$2 );
  if( !$result ) {
    SWIG_fail;
  }
  for( size_t i = 0; i < *$2; i++ ) {
    PyList_SetItem($result, i,
       SWIG_NewPointerObj((void*)(*$1)[i],SWIGTYPE_p_GDALMDArrayHS,SWIG_POINTER_OWN) );
    /* We have borrowed the GDALMDArrayHS */
    (*$1)[i] = NULL;
  }
}

%typemap(freearg) (GDALMDArrayHS*** parrays, size_t* pnCount)
{
  /* %typemap(freearg) (GDALMDArrayHS*** parrays, size_t* pnCount) */
  GDALReleaseArrays(*$1, *$2);
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
  Py_DECREF($result);
  $result = PyList_New( *$2 );
  if( !$result ) {
    SWIG_fail;
  }
  for( size_t i = 0; i < *$2; i++ ) {
      char szTmp[32];
      sprintf(szTmp, CPL_FRMT_GUIB, (*$1)[i]);
      PyObject *o = PyLong_FromString(szTmp, NULL, 10);
      PyList_SetItem($result, i, o );
  }
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
  Py_DECREF($result);
  $result = PyTuple_New( *$2 );
  if( !$result ) {
    SWIG_fail;
  }
  for( unsigned int i=0; i<*$2; i++ ) {
    PyObject *val = PyInt_FromLong( (*$1)[i] );
    PyTuple_SetItem( $result, i, val );
  }
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
%typemap(argout, fragment="CreateTupleFromDoubleArray") (double** pvals, size_t* pnCount)
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
  Py_DECREF($result);
  $result = PyList_New( *$2 );
  if( !$result ) {
    SWIG_fail;
  }
  for( size_t i = 0; i < *$2; i++ ) {
    PyList_SetItem($result, i,
       SWIG_NewPointerObj((void*)(*$1)[i],SWIGTYPE_p_GDALEDTComponentHS,SWIG_POINTER_OWN) );
    /* We have borrowed the GDALEDTComponentHS */
    (*$1)[i] = NULL;
  }
}

%typemap(freearg) (GDALEDTComponentHS*** pcomps, size_t* pnCount)
{
  /* %typemap(freearg) (GDALEDTComponentHS*** pcomps, size_t* pnCount) */
  GDALExtendedDataTypeFreeComponents(*$1, *$2);
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
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,r);
%#endif
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



/*
 * Typemap OGRCodedValue*<- dict.
 */

%typemap(in) OGRCodedValue* enumeration
{
    /* %typemap(in) OGRCodedValue* enumeration */
    $1 = NULL;

    if ($input == NULL || !PyMapping_Check($input)) {
        SWIG_exception_fail(SWIG_ValueError,"Expected dict.");
    }
    Py_ssize_t size = PyMapping_Length( $input );
    $1 = (OGRCodedValue*)VSICalloc(size+1, sizeof(OGRCodedValue) );
    if( !$1 ) {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
      SWIG_fail;
    }

    PyObject *item_list = PyMapping_Items( $input );
    if( item_list == NULL )
    {
      PyErr_SetString(PyExc_TypeError,"Cannot retrieve items");
      SWIG_fail;
    }

    for( Py_ssize_t i=0; i<size; i++ ) {
        PyObject *it = PySequence_GetItem( item_list, i );
        if( it == NULL )
        {
          Py_DECREF(item_list);
          PyErr_SetString(PyExc_TypeError,"Cannot retrieve key/value");
          SWIG_fail;
        }

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

        PyObject* vStr = v != Py_None ? PyObject_Str(v) : Py_None;
        if( v == Py_None )
            Py_INCREF(Py_None);
        if( PyErr_Occurred() )
        {
            Py_DECREF(it);
            Py_DECREF(kStr);
            Py_DECREF(item_list);
            SWIG_fail;
        }

        int bFreeK, bFreeV;
        char* pszK = GDALPythonObjectToCStr(kStr, &bFreeK);
        char* pszV = vStr != Py_None ? GDALPythonObjectToCStr(vStr, &bFreeV) : NULL;
        if( pszK == NULL || (pszV == NULL && vStr != Py_None) )
        {
            GDALPythonFreeCStr(pszK, bFreeK);
            if( pszV )
                GDALPythonFreeCStr(pszV, bFreeV);
            Py_DECREF(kStr);
            Py_DECREF(vStr);
            Py_DECREF(it);
            Py_DECREF(item_list);
            PyErr_SetString(PyExc_TypeError,"Cannot get key/value as string");
            SWIG_fail;
        }
        ($1)[i].pszCode = CPLStrdup(pszK);
        ($1)[i].pszValue = pszV ? CPLStrdup(pszV) : NULL;

        GDALPythonFreeCStr(pszK, bFreeK);
        if( pszV )
            GDALPythonFreeCStr(pszV, bFreeV);
        Py_DECREF(kStr);
        Py_DECREF(vStr);
        Py_DECREF(it);
    }
    Py_DECREF(item_list);
}

%typemap(freearg) OGRCodedValue*
{
  /* %typemap(freearg) OGRCodedValue* */
  if( $1 )
  {
      for( size_t i = 0; ($1)[i].pszCode != NULL; ++i )
      {
          CPLFree(($1)[i].pszCode);
          CPLFree(($1)[i].pszValue);
      }
  }
  CPLFree( $1 );
}

%typemap(out) OGRCodedValue*
{
  /* %typemap(out) OGRCodedValue* */
  if( $1 == NULL )
  {
      PyErr_SetString( PyExc_RuntimeError, CPLGetLastErrorMsg() );
      SWIG_fail;
  }
  PyObject *dict = PyDict_New();
  for( int i = 0; ($1)[i].pszCode != NULL; i++ )
  {
    if( ($1)[i].pszValue )
    {
        PyObject* val = GDALPythonObjectFromCStr(($1)[i].pszValue);
        PyDict_SetItemString(dict, ($1)[i].pszCode, val);
        Py_DECREF(val);
    }
    else
    {
        PyDict_SetItemString(dict, ($1)[i].pszCode, Py_None);
    }
  }
  $result = dict;
}

%typemap(in) (OSRSpatialReferenceShadow** optional_OSRSpatialReferenceShadow) ( OSRSpatialReferenceShadow* val )
{
  /* %typemap(in) (OSRSpatialReferenceShadow** optional_OSRSpatialReferenceShadow) */
  if ( $input == Py_None ) {
    $1 = NULL;
  }
  else {
    void* argp = NULL;
    int res = SWIG_ConvertPtr($input, &argp, SWIGTYPE_p_OSRSpatialReferenceShadow,  0  | 0);
    if (!SWIG_IsOK(res)) {
      SWIG_exception_fail(SWIG_ArgError(res), "argument of type != OSRSpatialReferenceShadow");
    }
    val = reinterpret_cast< OSRSpatialReferenceShadow * >(argp);
    $1 = &val;
  }
}

/***************************************************
 * Typemaps for Layer.GetGeometryTypes()
 ***************************************************/
%typemap(in,numinputs=0) (OGRGeometryTypeCounter** ppRet, int* pnEntryCount) ( OGRGeometryTypeCounter* pRet = NULL, int nEntryCount = 0 )
{
  /* %typemap(in,numinputs=0) (OGRGeometryTypeCounter** ppRet, int* pnEntryCount) */
  $1 = &pRet;
  $2 = &nEntryCount;
}

%typemap(argout)  (OGRGeometryTypeCounter** ppRet, int* pnEntryCount)
{
  /* %typemap(argout)  (OGRGeometryTypeCounter** ppRet, int* pnEntryCount) */
  Py_DECREF($result);
  int nEntryCount = *($2);
  OGRGeometryTypeCounter* pRet = *($1);
  if( pRet == NULL )
  {
      PyErr_SetString( PyExc_RuntimeError, CPLGetLastErrorMsg() );
      SWIG_fail;
  }
  $result = PyDict_New();
  for(int i = 0; i < nEntryCount; ++ i)
  {
      PyObject *key = PyInt_FromLong( (int)(pRet[i].eGeomType) );
      PyObject *val = PyLong_FromLongLong( pRet[i].nCount );
      PyDict_SetItem($result, key, val );
      Py_DECREF(key);
      Py_DECREF(val);
  }
}

%typemap(freearg)  (OGRGeometryTypeCounter** ppRet, int* pnEntryCount)
{
    /* %typemap(freearg)  (OGRGeometryTypeCounter** ppRet, int* pnEntryCount) */
    VSIFree(*$1);
}

/***************************************************
 * Typemaps for Layer.GetSupportedSRSList()
 ***************************************************/
%typemap(in,numinputs=0) (OGRSpatialReferenceH** ppRet, int* pnEntryCount) ( OGRSpatialReferenceH* pRet = NULL, int nEntryCount = 0 )
{
  /* %typemap(in,numinputs=0) (OGRSpatialReferenceH** ppRet, int* pnEntryCount) */
  $1 = &pRet;
  $2 = &nEntryCount;
}

%typemap(argout)  (OGRSpatialReferenceH** ppRet, int* pnEntryCount)
{
  /* %typemap(argout)  (OGRSpatialReferenceH** ppRet, int* pnEntryCount) */
  Py_DECREF($result);
  int nEntryCount = *($2);
  OGRSpatialReferenceH* pRet = *($1);
  if( nEntryCount == 0)
  {
    Py_INCREF(Py_None);
    $result = Py_None;
  }
  else
  {
    $result = PyList_New(nEntryCount);
    if( !$result ) {
      SWIG_fail;
    }
    for(int i = 0; i < nEntryCount; ++ i)
    {
      OSRReference(pRet[i]);
      PyList_SetItem($result, i, SWIG_NewPointerObj(
          SWIG_as_voidptr(pRet[i]),SWIGTYPE_p_OSRSpatialReferenceShadow, SWIG_POINTER_OWN) );
    }
  }
}

%typemap(freearg)  (OGRSpatialReferenceH** ppRet, int* pnEntryCount)
{
    /* %typemap(freearg)  (OGRSpatialReferenceH** ppRet, int* pnEntryCount) */
    OSRFreeSRSArray(*$1);
}


/***************************************************
 * Typemap for Layer.IsArrowSchemaSupported()
 ***************************************************/
%typemap(in,numinputs=0) (bool* pbRet, char **errorMsg) (bool ret, char* errorMsg)
{
  /* %typemap(in,numinputs=0) (bool* pbRet, char **errorMsg) */
  $1 = &ret;
  $2 = &errorMsg;
}
%typemap(argout) (bool* pbRet, char **errorMsg)
{
  /* %typemap(argout) (bool* pbRet, char **errorMsg)*/
  Py_DECREF($result);
  $result = PyTuple_New(2);
  PyTuple_SetItem($result, 0, PyBool_FromLong(*$1));
  if( *$2 )
  {
      PyTuple_SetItem($result, 1, PyUnicode_FromString(*$2));
      VSIFree(*$2);
  }
  else
  {
      PyTuple_SetItem($result, 1, Py_None);
      Py_INCREF(Py_None);
  }
}

/***************************************************
 * Typemap for RasterAttributeTable.ReadValuesIOAsString()
 ***************************************************/

%typemap(in,numinputs=1) (int iLength, char **ppszData) (int iLength)
{
  /* %typemap(in,numinputs=1) (int iLength, char **ppszData) (int iLength) */
  if ( !PyArg_Parse($input,"i",&iLength) )
  {
      PyErr_SetString(PyExc_TypeError, "not a integer");
      SWIG_fail;
  }
  if( iLength <= 0 || iLength > INT_MAX - 1 )
  {
      PyErr_SetString(PyExc_TypeError, "invalid length");
      SWIG_fail;
  }
  $1 = iLength;
  $2 = (char**)VSICalloc(iLength + 1, sizeof(char*));
  if( !$2 )
  {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
      SWIG_fail;
  }
}

%typemap(argout) (int iLength, char **ppszData)
{
  /* %typemap(argout) (int iLength, char **ppszData) */
  Py_DECREF($result);
  PyObject *out = PyList_New( $1 );
  if( !out ) {
      SWIG_fail;
  }
  for( int i=0; i<$1; i++ ) {
    if( $2[i] )
    {
        PyObject *val = GDALPythonObjectFromCStr( $2[i] );
        PyList_SetItem( out, i, val );
    }
    else
    {
        Py_INCREF(Py_None);
        PyList_SetItem( out, i, Py_None );
    }
  }
  $result = out;
}

%typemap(freearg) (int iLength, char **ppszData)
{
  /* %typemap(freearg) (int iLength, char **ppszData) */
  CSLDestroy($2);
}

/***************************************************
 * Typemap for RasterAttributeTable.ReadValuesIOAsInt()
 ***************************************************/

%typemap(in,numinputs=1) (int iLength, int *pnData) (int iLength)
{
  /* %typemap(in,numinputs=1) (int iLength, int *pnData) (int iLength) */
  if ( !PyArg_Parse($input,"i",&iLength) ) {
    PyErr_SetString(PyExc_TypeError, "not a integer");
    SWIG_fail;
  }
  if( iLength <= 0 )
  {
      PyErr_SetString(PyExc_TypeError, "invalid length");
      SWIG_fail;
  }
  $1 = iLength;
  $2 = (int*)VSICalloc(iLength, sizeof(int));
  if( !$2 )
  {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
      SWIG_fail;
  }
}

%typemap(argout) (int iLength, int *pnData)
{
  /* %typemap(argout) (int iLength, int *pnData) */
  Py_DECREF($result);
  PyObject *out = PyList_New( $1 );
  if( !out ) {
      SWIG_fail;
  }
  for( int i=0; i<$1; i++ ) {
    PyObject *val = PyLong_FromLong( ($2)[i] );
    PyList_SetItem( out, i, val );
  }
  $result = out;
}

%typemap(freearg) (int iLength, int *pnData)
{
  /* %typemap(freearg) (int iLength, int *pnData) */
  CPLFree($2);
}

/***************************************************
 * Typemap for RasterAttributeTable.ReadValuesIOAsDouble()
 ***************************************************/

%typemap(in,numinputs=1) (int iLength, double *pdfData) (int iLength)
{
  /* %typemap(in,numinputs=1) (int iLength, double *pdfData) (int iLength) */
  if ( !PyArg_Parse($input,"i",&iLength) ) {
    PyErr_SetString(PyExc_TypeError, "not a integer");
    SWIG_fail;
  }
  if( iLength <= 0 )
  {
      PyErr_SetString(PyExc_TypeError, "invalid length");
      SWIG_fail;
  }
  $1 = iLength;
  $2 = (double*)CPLCalloc(iLength, sizeof(double));
  if( !$2 )
  {
      PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
      SWIG_fail;
  }
}

%typemap(argout) (int iLength, double *pdfData)
{
  /* %typemap(argout) (int iLength, double *pdfData)  */
  Py_DECREF($result);
  PyObject *out = PyList_New( $1 );
  if( !out ) {
      SWIG_fail;
  }
  for( int i=0; i<$1; i++ ) {
    PyObject *val = PyFloat_FromDouble( ($2)[i] );
    PyList_SetItem( out, i, val );
  }
  $result = out;
}

%typemap(freearg) (int iLength, double *pdfData)
{
  /* %typemap(freearg) (int iLength, double *pdfData) */
  CPLFree($2);
}

/***************************************************
 * Typemap for gdal.CreateRasterAttributeTableFromMDArrays()
 ***************************************************/

OBJECT_LIST_INPUT(GDALMDArrayHS);

%typemap(in, numinputs=1) (int nUsages, GDALRATFieldUsage *paeUsages)
{
  /*  %typemap(in) (int nUsages, GDALRATFieldUsage *paeUsages)*/
  if ( !PySequence_Check($input) ) {
    PyErr_SetString(PyExc_TypeError, "not a sequence");
    SWIG_fail;
  }
  Py_ssize_t size = PySequence_Size($input);
  if( size > (Py_ssize_t)INT_MAX ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  if( (size_t)size > SIZE_MAX / sizeof(int) ) {
    PyErr_SetString(PyExc_TypeError, "too big sequence");
    SWIG_fail;
  }
  $1 = (int)size;
  $2 = (GDALRATFieldUsage*) VSIMalloc($1*sizeof(GDALRATFieldUsage));
  if( !$2) {
    PyErr_SetString(PyExc_MemoryError, "cannot allocate temporary buffer");
    SWIG_fail;
  }

  for( int i = 0; i<$1; i++ ) {
    PyObject *o = PySequence_GetItem($input,i);
    int nVal = 0;
    if ( !PyArg_Parse(o,"i",&nVal) ) {
        PyErr_SetString(PyExc_TypeError, "not a valid GDALRATFieldUsage");
        Py_DECREF(o);
        SWIG_fail;
    }
    Py_DECREF(o);
    if( nVal < 0 || nVal >= GFU_MaxCount )
    {
        PyErr_SetString(PyExc_TypeError, "not a valid GDALRATFieldUsage");
        SWIG_fail;
    }
    ($2)[i] = static_cast<GDALRATFieldUsage>(nVal);
  }
}

%typemap(freearg) (int nUsages, GDALRATFieldUsage *paeUsages)
{
  /* %typemap(freearg) (int nUsages, GDALRATFieldUsage *paeUsages)*/
  CPLFree( $2 );
}


%typemap(in,numinputs=0) (bool *pbVisible, int *pnXIntersection, int *pnYIntersection) ( bool visible = 0, int nxintersection = 0, int nyintersection = 0  )
{
  /* %typemap(in) (bool *pbVisible, int *pnXIntersection, int *pnYIntersection) */
  $1 = &visible;
  $2 = &nxintersection;
  $3 = &nyintersection;
}

%typemap(argout) (bool *pbVisible, int *pnXIntersection, int *pnYIntersection)
{
   /* %typemap(argout) (bool *pbVisible, int *pnXIntersection, int *pnYIntersection)  */
  PyObject *r = PyTuple_New( 3 );
  PyTuple_SetItem( r, 0, PyBool_FromLong(*$1) );
  PyTuple_SetItem( r, 1, PyLong_FromLong(*$2) );
  PyTuple_SetItem( r, 2, PyLong_FromLong(*$3) );
%#if SWIG_VERSION >= 0x040300
  $result = SWIG_Python_AppendOutput($result,r,$isvoid);
%#else
  $result = SWIG_Python_AppendOutput($result,r);
%#endif
}
