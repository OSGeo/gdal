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
 * Revision 1.3  2005/02/14 23:56:02  hobu
 * Added log info and C99-style comments
 *
 *
*/

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
* Typemap for vector<double> <-> PyTuple.
*/
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
   /* %typemap(in) std::vector<double> */
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
cout << "leaving" << endl;
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
%typemap(out) char **
{
  /* %typemap(out) char ** -> to hash */
  char **valptr = $1;
  $result = PyDict_New();
  if ( valptr != NULL ) {
    while (*valptr != NULL ) {
      char *equals = strchr( *valptr, '=' );
      PyObject *nm = PyString_FromStringAndSize( *valptr, equals-*valptr );
      PyObject *val = PyString_FromString( equals+1 );
      PyDict_SetItem($result, nm, val );
      valptr++;
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

