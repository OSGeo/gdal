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
 * Revision 1.2  2005/03/10 17:12:55  hobu
 * dummy typemaps for csharp.  Nothing here yet, but the names
 * are there
 *
 * Revision 1.1  2005/02/24 17:42:03  kruland
 * C# typemap file started.  Code taken from gdal_typemaps.i
 *
 *
*/

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

%typemap(csharp, out,fragment="OGRErrMessages") THROW_OGR_ERROR
{
  /* %typemap(csharp, out) THROW_OGR_ERROR */
  
}
%typemap(csharp, ret) THROW_OGR_ERROR
{
  /* %typemap(csharp, ret) THROW_OGR_ERROR */

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