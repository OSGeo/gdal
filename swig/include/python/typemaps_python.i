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
 * SWIG macro to define fixed length array typemaps
 *
 * defines the following:
 *
 * typemap(in,numinputs=0) double_size *var_name
 * typemap(out) double_size *var_name
 *
 * which matches decls like:  Dataset::GetGeoTransform( double_6 *c_transform )
 * where c_transform is a returned argument.
 *
 * typemap(in) double_size var_name
 *
 * which matches decls like: Dataset::SetGeoTransform( double_6 c_transform )
 * where c_transform is an input variable.
 *
 * The actual typedef for these new types needs to be in gdal.i in the %{..%} block
 * like this:
 *
 * %{
 *   ....
 *   typedef double double_6[6];
 * %}
 */

%define ARRAY_TYPEMAP(var_name, size)
%typemap(in,numinputs=0) ( double_ ## size *var_name) (double_ ## size var_name)
{
  /* %typemap(in,numinputs=0) (double_size *var_name) */
  $1 = &var_name;
}
%typemap(argout) ( double_ ## size *var_name)
{
  /* %typemap(argout) (double_size *var_name) */
  Py_DECREF( $result );
  $result = PyTuple_New( size );
  for( unsigned int i=0; i<size; i++ ) {
    PyObject *val = PyFloat_FromDouble( (*$1)[i] );
    PyTuple_SetItem( $result, i, val );
  }
}
%typemap(in) (double_ ## size var_name)
{
  /* %typemap(in) (double_size var_name) */
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
ARRAY_TYPEMAP(c_minmax, 2);

/*
 * Typemap for double c_transform[6]
 * Used in Dataset::GetGeoTransform(), Dataset::SetGeoTransform().
 */
ARRAY_TYPEMAP(c_transform, 6);

/*
 *  Typemap for counted arrays of ints <- PySequence
 */
%typemap(in,numargs=1) (int nList, int* pList)
{
  /* %typemap(in,numargs=1) (int nList, int* pList)*/
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

/*
 * Typemap for buffers with length <-> PyStrings
 * Used in Band::ReadRaster() and Band::WriteRaster()
 *
 * This typemap has a typecheck also since the WriteRaster()
 * methods are overloaded.
 */
%typemap(in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=0) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}
%typemap(argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */
  Py_DECREF($result);
  $result = PyString_FromStringAndSize( *$2, *$1 );
}
%typemap(freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
  if( $1 ) {
    free( *$2 );
  }
}
%typemap(in,numinputs=1) (int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
  PyString_AsStringAndSize($input, &$2, &$1 );
}
%typecheck(SWIG_TYPECHECK_POINTER) (int nLen, char *pBuf)
{
  /* %typecheck(SWIG_TYPECHECK_POINTER) (int nLen, char *pBuf) */
  $1 = (PyString_Check($input)) ? 1 : 0;
}

/*
 * Typemap argout of GDAL_GCP* used in Dataset::GetGCPs( )
 */
%typemap(in,numinputs=0) (int *nGCPs, GDAL_GCP const **pGCPs ) (int nGCPs, GDAL_GCP *pGCPs )
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
  Py_DECREF($result);
  $result = dict;
}

/*
 * Typemap for GDALColorEntry* <-> tuple
 */
%typemap(out) GDALColorEntry*
{
  /*  %typemap(out) GDALColorEntry* */
   $result = Py_BuildValue( "(hhhh)", (*$1).c1,(*$1).c2,(*$1).c3,(*$1).c4);
}

%typemap(in) GDALColorEntry*
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
%typemap(out) char **dict
{
  /* %typemap(out) char ** -> to hash */
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
%typemap(in) char **dict
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
 * Typemaps map mutable char ** arguments from PyStrings.  Does not
 * return the modified argument
 */
%typemap(in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
  PyArg_Parse( $input, "s", &val );
  $1 = &val;
}
