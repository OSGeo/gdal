/******************************************************************************
 * $Id$
 *
 * Name:     typemaps_csharp.i
 * Project:  GDAL SWIG Interface
 * Purpose:  Typemaps for C# bindings
 * Author:   Howard Butler, Tamas Szekeres
 *

 *
 * $Log$
 * Revision 1.8  2006/09/08 16:31:42  tamas
 * Typemap for char **argout
 *
 * Revision 1.7  2006/09/07 10:25:45  tamas
 * Corrected default typemaps to eliminate warnings at the interface creation
 *
 * Revision 1.6  2005/08/19 13:42:39  kruland
 * Fix problem in a double[ANY] typemap which prevented compilation of wrapper.
 *
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

%typemap(in,numinputs=0) (int *nLen, char **pBuf ) ( int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=0) (int *nLen, char **pBuf ) */
  $1 = &nLen;
  $2 = &pBuf;
}

%typemap(argout) (int *nLen, char **pBuf )
{
  /* %typemap(argout) (int *nLen, char **pBuf ) */

}
%typemap(freearg) (int *nLen, char **pBuf )
{
  /* %typemap(freearg) (int *nLen, char **pBuf ) */
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

%fragment("t_output_helper","header") %{
  /* TODO */
%}

%typemap(in,numinputs=1) (int nLen, char *pBuf )
{
  /* %typemap(in,numinputs=1) (int nLen, char *pBuf ) */
  /*TODO*/
	$2 = $null;
    $1 = $null;
}


%typemap(in) (tostring argin) (string str)
{
  /* %typemap(in) (tostring argin) */
}

%typemap(in) (char **ignorechange) ( char *val )
{
  /* %typemap(in) (char **ignorechange) */
	/*TODO*/
	$1 = $null;
}

%typemap(out,fragment="OGRErrMessages",canthrow=1) OGRErr
{
  /* %typemap(out,fragment="OGRErrMessages",canthrow=1) OGRErr */
  $result = result;
}
%typemap(ret) OGRErr
{
  /* %typemap(ret) OGRErr */

}


/* GDAL Typemaps */

%typemap(out) IF_ERR_RETURN_NONE
{
  /* %typemap(out) IF_ERR_RETURN_NONE */

}
%typemap(ret) IF_ERR_RETURN_NONE
{
 /* %typemap(ret) IF_ERR_RETURN_NONE */

}
%typemap(out) IF_FALSE_RETURN_NONE
{
  /* %typemap(out) IF_FALSE_RETURN_NONE */

}
%typemap(ret) IF_FALSE_RETURN_NONE
{
 /* %typemap(ret) IF_FALSE_RETURN_NONE */

}

%typemap(in,numargs=1) (int nList, int* pList)
{
  /* %typemap(in,numargs=1) (int nList, int* pList)*/
  /* check if is List */

}
%typemap(freearg) (int nList, int* pList)
{
  /* %typemap(freearg) (int nList, int* pList) */
  if ($2) {
    free((void*) $2);
  }
}


/*
 * Typemap char ** -> dict
 */
%typemap(out) char **dict
{
  /* %typemap(out) char ** -> to hash */
  /*TODO*/
	$result = $null;
}

/*
 * Typemap char **<- dict
 */
%typemap(in) char **dict
{
  /* %typemap(in) char **dict */

}
%typemap(freearg) char **dict
{
  /* %typemap(freearg) char **dict */
  CSLDestroy( $1 );
}

%define OPTIONAL_POD(type,argstring)
%typemap(in) (type *optional_##type) ( type val )
{
  /* %typemap(in) (type *optional_##type) */

}
%typemap(typecheck,precedence=0) (type *optional_##type)
{
  /* %typemap(typecheck,precedence=0) (type *optionalInt) */

}
%enddef

OPTIONAL_POD(int,i);

/*
 * Typemap maps char** arguments from Python Sequence Object
 */
%typemap(in) char **options
{
  /* %typemap(in) char **options */
  /* Check if is a list */

}
%typemap(freearg) char **options
{
  /* %typemap(freearg) char **options */
  CSLDestroy( $1 );
}
%typemap(out) char **options
{
  /* %typemap(out) char ** -> ( string ) */
  /*TODO*/
  $result = $null;
}

/*
 * Typemap for char **argout. 
 */
%typemap(imtype) (char **argout) "out string"
%typemap(cstype) (char **argout) "out string"
%typemap(csin) (char** argout) "out $csinput"
  
%typemap(in) (char **argout)
{
  /* %typemap(in) (char **argout) */
	$1 = ($1_ltype)$input;
}
%typemap(argout) (char **argout)
{
  /* %typemap(argout) (char **argout) */
  char* temp_string;
  temp_string = SWIG_csharp_string_callback(*$1);
  if (*$1)
		free(*$1);
  *$1 = temp_string;
}
%typemap(freearg) (char **argout)
{
  /* %typemap(freearg) (char **argout) */
}

%fragment("CreateTupleFromDoubleArray","header") %{
static int
CreateTupleFromDoubleArray( double *first, unsigned int size ) {
  
  return 1;
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

}
%typemap(in,numinputs=0) ( double *argout[ANY]) (double *argout[$dim0])
{
  /* %typemap(in,numinputs=0) (double *argout[ANY]) */
  $1 = (double**)&argout;
}
%typemap(argout,fragment="t_output_helper,CreateTupleFromDoubleArray") ( double *argout[ANY])
{
  /* %typemap(argout) (double *argout[ANY]) */

}
%typemap(freearg) (double *argout[ANY])
{
  /* %typemap(freearg) (double *argout[ANY]) */

}
%typemap(in) (double argin[ANY]) (double argin[$dim0])
{
  /* %typemap(in) (double argin[ANY]) */

}
