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
  /* %typemap(in,numinputs=0) (double *val, int*hasval) */
  $1 = &tmpval;
  $2 = &tmphasval;
}
%typemap(argout) (double *val, int*hasval) {
  /* %typemap(argout) (double *val, int*hasval) */
  if ( !*$2 ) {
    RETVAL_NULL();
  }
  else {
    RETVAL_DOUBLE( *$1 );
  }
}

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
%typemap(out) IF_FALSE_RETURN_NONE
{
 /* %typemap(out) IF_FALSE_RETURN_NONE */
 RETVAL_NULL();  
}
%typemap(ret) IF_FALSE_RETURN_NONE
{
 /* %typemap(ret) IF_FALSE_RETURN_NONE */
 RETVAL_NULL();
}

/*
 * Another output typemap which will raise an
 * exception on error.  If there is no error,
 * and no other argout typemaps create a return value,
 * then it will return 0.
 */
%fragment("OGRErrMessages","header") %{
static char *
OGRErrMessages( int rc ) {
  switch( rc ) {
  case 0:
    return "OGR Error 0: None";
  case 1:
    return "OGR Error 1: Not enough data";
  case 2:
    return "OGR Error 2: Unsupported geometry type";
  case 3:
    return "OGR Error 3: Unsupported operation";
  case 4:
    return "OGR Error 4: Corrupt data";
  case 5:
    return "OGR Error 5: General Error";
  case 6:
    return "OGR Error 6: Unsupported SRS";
  default:
    return "OGR Error: Unknown";
  }
}
%}
%typemap(out) OGRErr
{
  /* %typemap(out) OGRErr */
  if (result != 0 ) {
    SWIG_PHP_Error(E_ERROR,OGRErrMessages(result));
  }
}
%typemap(ret,fragment="OGRErrMessages") OGRErr
{
  /* %typemap(ret) OGRErr */
  RETVAL_LONG(0);
}

%fragment("CreateTupleFromDoubleArray","header") %{
  zval *
  CreateTupleFromDoubleArray( double *first, unsigned int size ) {
    zval *tmp;
    MAKE_STD_ZVAL(tmp);
    array_init(tmp);
    for( unsigned int i=0; i<size; i++ ) {
      add_next_index_double( tmp, *first );
      ++first;
    }
    return tmp;
 }
%}

%typemap(in,numinputs=0) ( double argout[ANY]) (double argout[$dim0])
{
  /* %typemap(in,numinputs=0) (double argout[ANY]) */
  $1 = argout;
}
%typemap(argout,fragment="CreateTupleFromDoubleArray,t_output_helper") ( double argout[ANY])
{
  /* %typemap(argout) (double argout[ANY]) */
  zval *t = CreateTupleFromDoubleArray( $1, $dim0 );
  t_output_helper( &$result, t );
}
%typemap(in,numinputs=0) ( double *argout[ANY]) (double *argout)
{
  /* %typemap(in,numinputs=0) (double *argout[ANY]) */
  $1 = &argout;
}
%typemap(argout,fragment="CreateTupleFromDoubleArray,t_output_helper") ( double *argout[ANY])
{
  /* %typemap(argout) (double *argout[ANY]) */
  zval *t = CreateTupleFromDoubleArray( *$1, $dim0 );
  t_output_helper( &$result, t);
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
  for (unsigned int i=0; i<$dim0; i++) {
    double val = 0.0; /* extract val from i-th position of $input */
    $1[i] =  val;
  }
}

/*
 *  Typemap for counted arrays of ints <- PySequence
 */
%typemap(in,numinputs=1) (int nList, int* pList)
{
  /* %typemap(in,numinputs=1) (int nList, int* pList)*/
  zend_error(E_ERROR,"Typemap (in,numinputs=1) (int nList, int*pList) not properly defined");
  /* check if is List */
//  if ( !PySequence_Check($input) ) {
//    PyErr_SetString(PyExc_TypeError, "not a sequence");
//    SWIG_fail;
//  }
//  $1 = PySequence_Size($input);
//  $2 = (int*) malloc($1*sizeof(int));
//  for( int i = 0; i<$1; i++ ) {
//    PyObject *o = PySequence_GetItem($input,i);
//    if ( !PyArg_Parse(o,"i",&$2[i]) ) {
//      SWIG_fail;
//    }
//  }
}
%typemap(freearg) (int nList, int* pList)
{
  /* %typemap(freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
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
  ZVAL_STRINGL( $result, *$2, *$1, 1 );
}
%typemap(freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
  if( *$1 ) {
    free( *$2 );
  }
}
%typemap(in,numinputs=1) (int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
  convert_to_string_ex($input);
  $2 = Z_STRVAL_PP($input);
  $1 = Z_STRLEN_PP($input);
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER)
        (int nLen, char *pBuf)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (int nLen, char *pBuf) */
  $1 = ($input)->type == IS_STRING;
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
  zval *out;
  MAKE_STD_ZVAL(out);
  array_init(out);
  for( int i = 0; i < *$1; i++ ) {
    GDAL_GCP *o = new_GDAL_GCP( (*$2)[i].dfGCPX,
                                (*$2)[i].dfGCPY,
                                (*$2)[i].dfGCPZ,
                                (*$2)[i].dfGCPPixel,
                                (*$2)[i].dfGCPLine,
                                (*$2)[i].pszInfo,
                                (*$2)[i].pszId );
    zval *t;
    MAKE_STD_ZVAL(t);
    SWIG_SetPointerZval(t,(void*)o,SWIGTYPE_p_GDAL_GCP,1);
    add_next_index_zval(out,t);
  }
  $result = out;
  zval_copy_ctor($result);
}
%typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) ( GDAL_GCP *tmpGCPList )
{
  /* %typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs ) */
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
  array_init($result);
  add_next_index_long($result,(*$1).c1);
  add_next_index_long($result,(*$1).c2);
  add_next_index_long($result,(*$1).c3);
  add_next_index_long($result,(*$1).c4);
}

%typemap(in) GDALColorEntry*
{
  /* %typemap(in) GDALColorEntry* */
  GDALColorEntry ce = {255,255,255,255};
  // Need to parse the array values from $input
  $1 = &ce;
}

/*
 * Typemap char ** -> dict
 */
%typemap(out) char **dict
{
  /* %typemap(out) char **dict */
  char **stringarray = $1;
  array_init($result);
  if ( stringarray != NULL ) {
    while (*stringarray != NULL ) {
      char const *valptr;
      char *keyptr;
      valptr = CPLParseNameValue( *stringarray, &keyptr );
      if ( valptr != 0 ) {
	add_assoc_string($result,keyptr,(char*)valptr,1);
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
  $1 = 0; //(PyMapping_Check($input) || PySequence_Check($input) ) ? 1 : 0;
}
%typemap(in) char **dict
{
  /* %typemap(in) char **dict */
  zend_error(E_ERROR,"Typemap (in) char **dict not properly defined");
/*  if ( PySequence_Check( $input ) ) {
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
*/
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
  zend_error(E_ERROR,"Typemap (in) char **options not properly defined");
  //  int size = PySequence_Size($input);
  //  for (int i = 0; i < size; i++) {
  //    char *pszItem = NULL;
  //    if ( ! PyArg_Parse( PySequence_GetItem($input,i), "s", &pszItem ) ) {
  //      PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
  //      SWIG_fail;
  //    }
  //    $1 = CSLAddString( $1, pszItem );
  //  }
}
%typemap(freearg) char **options
{
  /* %typemap(freearg) char **options */
  CSLDestroy( $1 );
}
%typemap(out) char **options
{
  /* %typemap(out) char ** -> ( string ) */
  char **stringarray = $1;
  if ( stringarray == NULL ) {
    RETVAL_NULL();
  }
  else {
    int len = CSLCount( stringarray );
    array_init($result);
    for ( int i = 0; i < len; ++i, ++stringarray ) {
      add_next_index_string( $result, *stringarray, 1 );
    }
  }
}

/*
 * Typemaps map mutable char ** arguments from PyStrings.  Does not
 * return the modified argument
 */
%typemap(in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
  convert_to_string_ex( $input );
  $1 = NULL;
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
  zval *t;
  MAKE_STD_ZVAL(t);
  if ( $1 ) {
    ZVAL_STRING(t,*$1,strlen(*$1));
  }
  else {
    ZVAL_NULL(t);
  }
  t_output_helper(&$result, t);
}
%typemap(freearg) (char **argout)
{
  /* %typemap(freearg) (char **argout) */
  if ( *$1 )
    CPLFree( *$1 );
}

%typemap(in) (int *optional_int) ( $*1_ltype val )
{
  /* %typemap(in) (int *optional_int) */
  if ( ZVAL_IS_NULL(*$input) ) {
    $1 = 0;
  }
  convert_to_long_ex($input);
  val = ($*1_ltype) Z_LVAL_PP( $input );
  $1 = &val;  
}
%typemap(typecheck,precedence=0) (int *optional_int)
{
  /* %typemap(typecheck,precedence=0) (int *optional_int) */
  $1 = (($input->type == IS_NONE) || $input->type == IS_LONG ) ? 1 : 0;
}

/*
 * Typedef const char * <- Any object.
 *
 * Formats the object using str and returns the string representation
 */

%typemap(in) (tostring argin)
{
  /* %typemap(in) (tostring argin) */
  convert_to_string_ex($input);
  $1 = Z_STRVAL_PP( $input );
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin)
{
  /* %typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin) */
  $1 = 1;
}

