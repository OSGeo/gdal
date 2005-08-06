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
 * Revision 1.5  2005/08/06 20:51:58  kruland
 * Instead of using double_## defines and SWIG macros, use typemaps with
 * [ANY] specified and use $dim0 to extract the dimension.  This makes the
 * code quite a bit more readable.
 *
 * Revision 1.4  2005/08/05 18:49:26  hobu
 * Add some more dummy typemaps to get us closer to where
 * Kevin is with python
 *
 * Revision 1.3  2005/06/22 18:41:30  kruland
 * Renamed type for OGRErr typemap to use OGRErr instead of the made up
 * THROW_OGR_ERROR.
 *
 * Revision 1.2  2005/03/10 17:12:55  hobu
 * dummy typemaps for csharp.  Nothing here yet, but the names
 * are there
 *
 * Revision 1.1  2005/02/24 17:42:03  kruland
 * C# typemap file started.  Code taken from gdal_typemaps.i
 *
 *
*/

%include "typemaps.i"

/* CSHARP TYPEMAPS */

%typemap(csharp,in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen, char *pBuf )
{
  /* %typemap(csharp, in,numinputs=0) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}

%typemap(csharp,argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */

}
%typemap(csharp,freearg) (int *nLen, char **pBuf )
{
  /* %typemap(csharp,freearg) (int *nLen, char **pBuf ) */
  if( $1 ) {
    free( *$2 );
  }
}

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
%typemap(csharp,in,numinputs=1) (int nLen, char *pBuf )
{
  /* %typemap(csharp, in,numinputs=1) (int nLen, char *pBuf ) */

}


%typemap(csharp,in) (tostring argin) (string str)
{
  /* %typemap(csharp,in) (tostring argin) */

}

%typemap(csharp,in) (char **ignorechange) ( char *val )
{
  /* %typemap(csharp, in) (char **ignorechange) */

}

%typemap(csharp, out,fragment="OGRErrMessages") OGRErr
{
  /* %typemap(csharp, out) OGRErr */
  
}
%typemap(csharp, ret) OGRErr
{
  /* %typemap(csharp, ret) OGRErr */

}


/* GDAL Typemaps */

%typemap(csharp,out) IF_ERR_RETURN_NONE
{
  /* %typemap(csharp,out) IF_ERR_RETURN_NONE */

}
%typemap(csharp,ret) IF_ERR_RETURN_NONE
{
 /* %typemap(csharp,ret) IF_ERR_RETURN_NONE */

}
%typemap(csharp,out) IF_FALSE_RETURN_NONE
{
  /* %typemap(csharp,out) IF_FALSE_RETURN_NONE */

}
%typemap(ret) IF_FALSE_RETURN_NONE
{
 /* %typemap(ret) IF_FALSE_RETURN_NONE */

}

%typemap(csharp,in,numargs=1) (int nList, int* pList)
{
  /* %typemap(in,numargs=1) (int nList, int* pList)*/
  /* check if is List */

}
%typemap(csharp,freearg) (int nList, int* pList)
{
  /* %typemap(python,freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
}


/*
 * Typemap char ** -> dict
 */
%typemap(csharp,out) char **dict
{
  /* %typemap(out) char ** -> to hash */

}

/*
 * Typemap char **<- dict
 */
%typemap(csharp,in) char **dict
{
  /* %typemap(in) char **dict */

}
%typemap(csharp,freearg) char **dict
{
  /* %typemap(csharp,freearg) char **dict */
  CSLDestroy( $1 );
}

%define OPTIONAL_POD(type,argstring)
%typemap(csharp,in) (type *optional_##type) ( type val )
{
  /* %typemap(csharp,in) (type *optional_##type) */

}
%typemap(csharp,typecheck,precedence=0) (type *optional_##type)
{
  /* %typemap(csharp,typecheck,precedence=0) (type *optionalInt) */

}
%enddef

OPTIONAL_POD(int,i);

/*
 * Typemap maps char** arguments from Python Sequence Object
 */
%typemap(csharp,in) char **options
{
  /* %typemap(in) char **options */
  /* Check if is a list */

}
%typemap(csharp,freearg) char **options
{
  /* %typemap(freearg) char **options */
  CSLDestroy( $1 );
}
%typemap(csharp,out) char **options
{
  /* %typemap(out) char ** -> ( string ) */

}

/*
 * Typemap for char **argout. 
 */
%typemap(csharp,in,numinputs=0) (char **argout) ( char *argout=0 )
{
  /* %typemap(in,numinputs=0) (char **argout) */

}
%typemap(csharp,argout,fragment="t_output_helper") (char **argout)
{
  /* %typemap(argout) (char **argout) */

}
%typemap(csharp,freearg) (char **argout)
{
  /* %typemap(freearg) (char **argout) */

}


%fragment("CreateTupleFromDoubleArray","header") %{
static int
CreateTupleFromDoubleArray( double *first, unsigned int size ) {
  
  return 1;
}
%}

%typemap(csharp,in,numinputs=0) ( double argout[ANY]) (double argout[$dim0])
{
  /* %typemap(in,numinputs=0) (double argout[ANY]) */

}
%typemap(csharp,argout,fragment="t_output_helper,CreateTupleFromDoubleArray") ( double argout[ANY])
{
  /* %typemap(argout) (double argout[ANY]) */

}
%typemap(csharp,in,numinputs=0) ( double *argout[ANY]) (double *argout[ANY])
{
  /* %typemap(in,numinputs=0) (double *argout[ANY]) */

}
%typemap(csharp,argout,fragment="t_output_helper,CreateTupleFromDoubleArray") ( double *argout[ANY])
{
  /* %typemap(argout) (double *argout[ANY]) */

}
%typemap(csharp,freearg) (double *argout[ANY])
{
  /* %typemap(freearg) (double *argout[ANY]) */

}
%typemap(csharp,in) (double argin[ANY]) (double argin[$dim0])
{
  /* %typemap(in) (double argin[ANY]) */

}
