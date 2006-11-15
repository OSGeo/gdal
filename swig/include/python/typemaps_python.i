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
 * Revision 1.47  2006/11/15 23:40:01  hobu
 * dump the tostring arginit typemap
 *
 * Revision 1.46  2006/11/15 23:09:29  hobu
 * try testing for a given string for the tostring arginit
 *
 * Revision 1.45  2006/11/07 05:37:30  hobu
 * an arginit typemap for the tostring typemap
 *
 * Revision 1.44  2006/11/05 20:30:10  hobu
 * swig no longer puts language-specific stuff in the first argument of
 * typemaps.  We already have everything conditionalized in gdal_typemaps.i
 * for each language anyway.  These changes just silence the warnings.
 *
 * Revision 1.43  2005/10/11 15:20:28  kruland
 * Removed the unused IF_ERROR_RETURN_NONE typemap.
 * Removed the unnecessary $result=0 from typemaps.  These lines were to correct
 * a bug in SWIG versions <= 1.3.24.
 *
 * Revision 1.42  2005/10/11 14:11:43  kruland
 * Fix memory bug in typemap(out) char **options.  The returned array of strings
 * is owned by the dataset.
 *
 * Revision 1.41  2005/10/03 20:13:33  kruland
 * Clean up the CPLErr typemap to account for the new-er %exception code.
 *
 * Revision 1.40  2005/09/29 14:02:23  kruland
 * Fixed memcpy in %typemap(in,numinputs=1) (int nGCPs, GDAL_GCP const *pGCPs)
 *
 * Revision 1.39  2005/09/13 02:58:41  kruland
 * Use the ogr_error_map.i include file.
 *
 * Revision 1.38  2005/09/13 02:10:19  kruland
 * Make fixes to the in ColorMap typemap.  Possible problems by using
 * references to temporaries.
 *
 * Revision 1.37  2005/09/02 15:24:38  kruland
 * Remove old and confusing comment which is no longer required.
 *
 * Revision 1.36  2005/08/08 17:07:16  kruland
 * Added python typemaps for CPLXMLNode* in and return for the ParseXMLString
 * and SerializeXMLTree methods.
 *
 * Revision 1.35  2005/08/06 20:51:58  kruland
 * Instead of using double_## defines and SWIG macros, use typemaps with
 * [ANY] specified and use $dim0 to extract the dimension.  This makes the
 * code quite a bit more readable.
 *
 * Revision 1.34  2005/08/04 20:47:24  kruland
 * Added a new binding to support RasterBand::GetNoDataValue(), GetMaximum(), GetMinimum(),
 * GetOffset(), GetScale() returning None when the attribute is not set.
 *
 * Revision 1.33  2005/07/15 20:28:16  kruland
 * It seems that in Python 2.3 (maybe 2.4 as well), sequence objects (lists) are also
 * mapping objects (dicts).  Fortunately, dicts are not lists.  So, in the in char **dict
 * typemap, we need to first test if the arg is a sequence then test for mapping.
 *
 * Revision 1.32  2005/07/15 19:01:45  kruland
 * Typemap out char **options needs to return a list instead of tuple to satisfy
 * the gdalautotests.
 *
 * Revision 1.31  2005/07/15 18:34:59  kruland
 * Revised the char **<-dict in mapping.  It now allows either dictionaries,
 * or sequences of strings.  Added a typecheck so overloading works.  This
 * typemap is only used in SetMetadata.
 *
 * Revision 1.30  2005/07/15 17:08:00  kruland
 * - Initialize the local pointer variables in
 *   (in,numinputs=0)(int *nLen,char**pBuf) argout typemap.
 * - Fix the freearg typemap for int *nLen,char **pBuf.
 * - Initialize the local pointer variables in
 *   (in,numinputs=0)(int*nGCPs, GDAL_GCP*pGCPs) argout typemap.
 * - Fix the ret CPLErr typemap for the not using exceptions case.
 *   The use exception case is probably still broken.
 *
 * Revision 1.29  2005/07/15 15:08:19  kruland
 * - Use the method SWIG_ConvertPtr instead of the #define'd name
 * SWIG_Python_ConvertPtr.  The other name is available in older versions of swig.
 * - Use PyInt_FromLong instead of SWIG_From_int.
 * - In all the typemaps which use t_output_helper, implement code which forces
 * initialization of $result to 0 at the beginning of the wrapper.
 *
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

%import "ogr_error_map.i"

%typemap(out,fragment="OGRErrMessages") OGRErr
{
  /* %typemap(out) OGRErr */
  if ( result != 0) {
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
    resultobj = PyInt_FromLong( 0 );
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
    PyArg_Parse(o, "d", &val );
    $1[i] =  val;
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
      SWIG_fail;
    }
  }
}
%typemap(freearg) (int nList, int* pList)
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
  $result = PyString_FromStringAndSize( *$2, *$1 );
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
  PyString_AsStringAndSize($input, &$2, &$1 );
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER)
        (int nLen, char *pBuf)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (int nLen, char *pBuf) */
  $1 = (PyString_Check($input)) ? 1 : 0;
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
      SWIG_fail;
    }
    memcpy( (void*) tmpGCPList, (void*) item, sizeof( GDAL_GCP ) );
    ++tmpGCPList;
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
   $result = Py_BuildValue( "(hhhh)", (*$1).c1,(*$1).c2,(*$1).c3,(*$1).c4);
}

%typemap(in) GDALColorEntry* (GDALColorEntry ce)
{
  /* %typemap(in) GDALColorEntry* */
   ce.c4 = 255;
   int size = PySequence_Size($input);
   if ( size > 4 ) {
     PyErr_SetString(PyExc_TypeError, "ColorEntry sequence too long");
     SWIG_fail;
   }
   if ( size < 3 ) {
     PyErr_SetString(PyExc_TypeError, "ColorEntry sequence too short");
     SWIG_fail;
   }
   PyArg_ParseTuple( $input,"hhh|h", &ce.c1, &ce.c2, &ce.c3, &ce.c4 );
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
      if ( ! PyArg_Parse( PySequence_GetItem($input,i), "s", &pszItem ) ) {
        PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
        SWIG_fail;
      }
      $1 = CSLAddString( $1, pszItem );
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
        PyArg_ParseTuple( it, "ss", &nm, &val );
        $1 = CSLAddNameValue( $1, nm, val );
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
    if ( ! PyArg_Parse( PySequence_GetItem($input,i), "s", &pszItem ) ) {
      PyErr_SetString(PyExc_TypeError,"sequence must contain strings");
      SWIG_fail;
    }
    $1 = CSLAddString( $1, pszItem );
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
  /* %typemap(out) char ** -> ( string ) */
  char **stringarray = $1;
  if ( stringarray == NULL ) {
    $result = Py_None;
    Py_INCREF( $result );
  }
  else {
    int len = CSLCount( stringarray );
    $result = PyList_New( len );
    for ( int i = 0; i < len; ++i, ++stringarray ) {
      PyObject *o = PyString_FromString( *stringarray );
      PyList_SetItem($result, i, o );
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
  if ( $1 ) {
    o = PyString_FromString( *$1 );
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


%typemap(in) (tostring argin) (PyObject *str)
{
  /* %typemap(in) (tostring argin) */
  str = PyObject_Str( $input );
  if ( str == 0 ) {
    PyErr_SetString( PyExc_RuntimeError, "Unable to format argument as string");
    SWIG_fail;
  }
 
  $1 = PyString_AsString(str); 
}
%typemap(freearg)(tostring argin)
{
  /* %typemap(freearg) (tostring argin) */
  Py_DECREF(str$argnum);
}
%typemap(typecheck,precedence=SWIG_TYPECHECK_POINTER) (tostring argin)
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
  $result = XMLTreeToPyList( $1 );
}
%typemap(ret) (CPLXMLNode*)
{
  /* %typemap(ret) (CPLXMLNode*) */
  if ( $1 ) CPLDestroyXMLNode( $1 );
}
