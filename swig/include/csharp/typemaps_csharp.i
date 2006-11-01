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
 * Revision 1.13  2006/11/01 00:04:47  tamas
 * More typemaps for 'out string'
 *
 * Revision 1.12  2006/10/31 23:23:08  tamas
 * Typemaps for GIntBig
 *
 * Revision 1.11  2006/10/28 20:40:55  tamas
 * Added typemaps for char **options
 *
 * Revision 1.10  2006/09/09 21:06:32  tamas
 * Added preliminary SWIGTYPE *DISOWN support.
 *
 * Revision 1.9  2006/09/09 17:54:37  tamas
 * Typemaps for double arrays
 *
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
  $1 = ($1_ltype)$input;
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
 * Typemap for GIntBig (int64)
 */

%typemap(ctype, out="GIntBig") GIntBig  %{GIntBig%}
%typemap(imtype, out="long") GIntBig "long"
%typemap(cstype) GIntBig %{long%}
%typemap(out) GIntBig %{ $result = $1; %}
%typemap(csout, excode=SWIGEXCODE) GIntBig {
    long res = $imcall;$excode
    return res;
}


/******************************************************************************
 * Marshaler for NULL terminated string arrays                                *
 *****************************************************************************/

%pragma(csharp) imclasscode=%{
  public class StringListMarshal : IDisposable {
    public readonly IntPtr[] _ar;
    public StringListMarshal(string[] ar) {
      _ar = new IntPtr[ar.Length+1];
      for (int cx = 0; cx < ar.Length; cx++) {
	      _ar[cx] = System.Runtime.InteropServices.Marshal.StringToHGlobalAnsi(ar[cx]);
      }
      _ar[ar.Length] = IntPtr.Zero;
    }
    public virtual void Dispose() {
	  for (int cx = 0; cx < _ar.Length-1; cx++) {
          System.Runtime.InteropServices.Marshal.FreeHGlobal(_ar[cx]);
      }
      GC.SuppressFinalize(this);
    }
  }
%}

/*
 * Typemap for char** options
 */

%typemap(imtype, out="IntPtr") char **options "IntPtr[]"
%typemap(cstype) char **options %{string[]%}
%typemap(in) char **options %{ $1 = ($1_ltype)$input; %}
%typemap(out) char **options %{ $result = $1; %}
%typemap(csin) char **options "new $modulePINVOKE.StringListMarshal($csinput)._ar"
%typemap(csout, excode=SWIGEXCODE) char**options {
    $excode
    throw new System.NotSupportedException("Returning string arrays is not implemented yet.");
}
 
%typemap(freearg) char **options
{
  /* %typemap(freearg) char **options */
  //CSLDestroy( $1 );
}

/*
 * Typemap for char **argout. 
 */
%typemap(imtype) (char **argout), (char **username), (char **usrname), (char **type) "out string"
%typemap(cstype) (char **argout), (char **username), (char **usrname), (char **type) "out string"
%typemap(csin) (char** argout), (char **username), (char **usrname), (char **type) "out $csinput"
  
%typemap(in) (char **argout), (char **username), (char **usrname), (char **type)
{
  /* %typemap(in) (char **argout) */
	$1 = ($1_ltype)$input;
}
%typemap(argout) (char **argout), (char **username), (char **usrname), (char **type)
{
  /* %typemap(argout) (char **argout) */
  char* temp_string;
  temp_string = SWIG_csharp_string_callback(*$1);
  if (*$1)
		free(*$1);
  *$1 = temp_string;
}
%typemap(freearg) (char **argout), (char **username), (char **usrname), (char **type)
{
  /* %typemap(freearg) (char **argout) */
}

/*
 * Typemap for double argout[ANY]. 
 */
%typemap(imtype) (double argout[ANY]) "double[]"
%typemap(cstype) (double argout[ANY]) "double[]"
%typemap(csin) (double argout[ANY]) "$csinput"

%typemap(in) (double argout[ANY])
{
  /* %typemap(in) (double argout[ANY]) */
  $1 = ($1_ltype)$input;
}

%typemap(in,numinputs=0) ( double *argout[ANY]) (double *argout[$dim0])
{
  /* %typemap(in,numinputs=0) (double *argout[ANY]) */
  $1 = (double**)&argout;
}
%typemap(argout) ( double *argout[ANY])
{
  /* %typemap(argout) (double *argout[ANY]) */

}
%typemap(freearg) (double *argout[ANY])
{
  /* %typemap(freearg) (double *argout[ANY]) */

}

/*
 * Typemap for double argin[ANY]. 
 */

%typemap(imtype) (double argin[ANY])  "double[]"
%typemap(cstype) (double argin[ANY]) "double[]"
%typemap(csin) (double argin[ANY])  "$csinput"

%typemap(in) (double argin[ANY])
{
  /* %typemap(in) (double argin[ANY]) */
  $1 = ($1_ltype)$input;
}

/*
 * Typemap for double inout[ANY]. 
 */

%typemap(imtype) (double inout[ANY])  "double[]"
%typemap(cstype) (double inout[ANY]) "double[]"
%typemap(csin) (double inout[ANY])  "$csinput"

%typemap(in) (double inout[ANY])
{
  /* %typemap(in) (double inout[ANY]) */
  $1 = ($1_ltype)$input;
}

%typemap(argout) (double inout[ANY])
{
  /* %typemap(argout) (double inout[ANY]) */
}

/*
 * Typemap for double *defaultval. 
 */

%typemap(imtype) (double *defaultval)  "ref double"
%typemap(cstype) (double *defaultval) "ref double"
%typemap(csin) (double *defaultval)  "ref $csinput"

%typemap(in) (double *defaultval)
{
  /* %typemap(in) (double inout[ANY]) */
  $1 = ($1_ltype)$input;
}

%typemap(cscode) SWIGTYPE %{
  internal static HandleRef getCPtrAndDisown($csclassname obj) {
    obj.swigCMemOwn = false;
    return getCPtr(obj);
  }
%}

%typemap(csin) SWIGTYPE *DISOWN "$csclassname.getCPtrAndDisown($csinput)"

