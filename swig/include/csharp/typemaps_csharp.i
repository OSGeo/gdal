/******************************************************************************
 * $Id$
 *
 * Name:     typemaps_cshar.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for C# bindings
 * Author:   Howard Butler
 *

 *
 * $Log$
 * Revision 1.1  2005/02/24 17:42:03  kruland
 * C# typemap file started.  Code taken from gdal_typemaps.i
 *
 *
*/

/* CSHARP TYPEMAPS */

%typemap(csharp,in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=0) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}

%typemap(csharp,argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */
  Py_XDECREF($result);
  $result = PyString_FromStringAndSize( *$2, *$1 );
}
%typemap(csharp,freearg) (int *nLen, char **pBuf )
{
  /* %typemap(python,freearg) (int *nLen, char **pBuf ) */
  if( $1 ) {
    free( *$2 );
  }
}
%typemap(csharp,in,numinputs=1) (int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
  PyString_AsStringAndSize($input, &$2, &$1 );
}


%typemap(csharp,in) (tostring argin) (PyObject *str)
{
  /* %typemap(csharp,in) (tostring argin) */
  str = PyObject_Str( $input );
  if ( str == 0 ) {
    PyErr_SetString( PyExc_RuntimeError, "Unable to format argument as string");
    SWIG_fail;
  }

  $1 = PyString_AsString(str); 
  Py_DECREF(str);
}

%typemap(csharp,in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
  PyArg_Parse( $input, "s", &val );
  $1 = &val;
}

