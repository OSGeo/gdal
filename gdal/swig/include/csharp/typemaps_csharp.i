/******************************************************************************
 * $Id$
 *
 * Name:     typemaps_csharp.i
 * Project:  GDAL CSharp Interface
 * Purpose:  Typemaps for C# bindings.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *****************************************************************************/
 

%include "typemaps.i"

/* CSHARP TYPEMAPS */

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

%typemap(out,fragment="OGRErrMessages",canthrow=1) OGRErr
{
  /* %typemap(out,fragment="OGRErrMessages",canthrow=1) OGRErr */
  $result = result;
}
%typemap(ret) OGRErr
{
  /* %typemap(ret) OGRErr */

}

%typemap(in) (tostring argin) (string str)
{
  /* %typemap(in) (tostring argin) */
  $1 = ($1_ltype)$input;
}

/* GDAL Typemaps */

%typemap(out) IF_FALSE_RETURN_NONE %{ $result = $1; %}
%typemap(ctype) IF_FALSE_RETURN_NONE "int"
%typemap(imtype) IF_FALSE_RETURN_NONE "int"
%typemap(cstype) IF_FALSE_RETURN_NONE "int"
%typemap(csout, excode=SWIGEXCODE) IF_FALSE_RETURN_NONE {
    int res = $imcall;$excode
    return res;
}

%typemap(out) IF_ERROR_RETURN_NONE %{ $result = $1; %}

%define OPTIONAL_POD(CTYPE, CSTYPE)
%typemap(imtype) (CTYPE *optional_##CTYPE) "IntPtr"
%typemap(cstype) (CTYPE *optional_##CTYPE) "ref CSTYPE"
%typemap(csin) (CTYPE *optional_##CTYPE) "(IntPtr)$csinput"
 
%typemap(in) (CTYPE *optional_##CTYPE)
{
  /* %typemap(in) (type *optional_##CTYPE) */
  $1 = ($1_type)$input;
}
%enddef

OPTIONAL_POD(int, int);


/***************************************************
 * Typemaps for  (retStringAndCPLFree*)
 ***************************************************/

%typemap(out) (retStringAndCPLFree*)
%{ 
    /* %typemap(out) (retStringAndCPLFree*) */
    if($1)
    {
        $result = SWIG_csharp_string_callback((const char *)$1);
        CPLFree($1);
    }
    else
    {
        $result = NULL;
    }
%}

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

/* FIXME: all those typemaps are not equivalent... out(char **CSL) should free */
/* the list with CSLDestroy() for example */

%typemap(imtype, out="IntPtr") char **options, char **dict, char **CSL "IntPtr[]"
%typemap(cstype) char **options, char **dict, char **CSL %{string[]%}
%typemap(in) char **options, char **dict, char **CSL %{ $1 = ($1_ltype)$input; %}
%typemap(out) char **options, char **dict, char **CSL %{ $result = $1; %}
%typemap(csin) char **options, char **dict, char **CSL "($csinput != null)? new $modulePINVOKE.StringListMarshal($csinput)._ar : null"
%typemap(csout, excode=SWIGEXCODE) char**options, char **dict {
        /* %typemap(csout) char**options */
        IntPtr cPtr = $imcall;
        IntPtr objPtr;
        int count = 0;
        if (cPtr != IntPtr.Zero) {
            while (Marshal.ReadIntPtr(cPtr, count*IntPtr.Size) != IntPtr.Zero)
                ++count;
        }
        string[] ret = new string[count];
        if (count > 0) {       
	        for(int cx = 0; cx < count; cx++) {
                objPtr = System.Runtime.InteropServices.Marshal.ReadIntPtr(cPtr, cx * System.Runtime.InteropServices.Marshal.SizeOf(typeof(IntPtr)));
                ret[cx]= (objPtr == IntPtr.Zero) ? null : System.Runtime.InteropServices.Marshal.PtrToStringAnsi(objPtr);
            }
        }
        $excode
        return ret;
}
 
%typemap(csout, excode=SWIGEXCODE) char** CSL {
        /* %typemap(csout) char** CSL */
        IntPtr cPtr = $imcall;
        IntPtr objPtr;
        int count = 0;
        if (cPtr != IntPtr.Zero) {
            while (Marshal.ReadIntPtr(cPtr, count*IntPtr.Size) != IntPtr.Zero)
                ++count;
        }
        string[] ret = new string[count];
        if (count > 0) {       
	        for(int cx = 0; cx < count; cx++) {
                objPtr = System.Runtime.InteropServices.Marshal.ReadIntPtr(cPtr, cx * System.Runtime.InteropServices.Marshal.SizeOf(typeof(IntPtr)));
                ret[cx]= (objPtr == IntPtr.Zero) ? null : System.Runtime.InteropServices.Marshal.PtrToStringAnsi(objPtr);
            }
        }
        if (cPtr != IntPtr.Zero)
            $modulePINVOKE.StringListDestroy(cPtr);
        $excode
        return ret;
}

%typemap(imtype, out="IntPtr") int *intList "int[]"
%typemap(cstype) int *intList %{int[]%}
%typemap(in) int *intList %{ $1 = ($1_ltype)$input; %}
%typemap(out) int *intList %{ $result = $1; %}
%typemap(csout, excode=SWIGEXCODE) int *intList {
        /* %typemap(csout) int *intList */
        IntPtr cPtr = $imcall;
        int[] ret = new int[count];
        if (count > 0) {       
	        System.Runtime.InteropServices.Marshal.Copy(cPtr, ret, 0, count);
        }
        $excode
        return ret;
}

%typemap(imtype, out="IntPtr") double *doubleList "double[]"
%typemap(cstype) double *doubleList %{double[]%}
%typemap(in) double *doubleList %{ $1 = ($1_ltype)$input; %}
%typemap(out) double *doubleList %{ $result = $1; %}
%typemap(csout, excode=SWIGEXCODE) double *doubleList {
        /* %typemap(csout) int *intList */
        IntPtr cPtr = $imcall;
        double[] ret = new double[count];
        if (count > 0) {       
	        System.Runtime.InteropServices.Marshal.Copy(cPtr, ret, 0, count);
        }
        $excode
        return ret;
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
%typemap(argout) (char **argout)
{
  /* %typemap(argout) (char **argout) */
  char* temp_string;
  temp_string = SWIG_csharp_string_callback(*$1);
  if (*$1)
		CPLFree(*$1);
  *$1 = temp_string;
}
%typemap(argout) (char **staticstring), (char **username), (char **usrname), (char **type)
{
  /* %typemap(argout) (char **staticstring) */
  *$1 = SWIG_csharp_string_callback(*$1);
}

/*
 * Typemap for char **ignorechange. 
 */
 
%typemap(imtype) (char **ignorechange) "ref string"
%typemap(cstype) (char **ignorechange) "ref string"
%typemap(csin) (char** ignorechange) "ref $csinput"
  
%typemap(in, noblock="1") (char **ignorechange)
{
  /* %typemap(in) (char **ignorechange) */
    $*1_type savearg = *(($1_type)$input); 
	$1 = ($1_ltype)$input;
}
%typemap(argout, noblock="1") (char **ignorechange)
{
  /* %typemap(argout) (char **ignorechange) */
  if ((*$1 - savearg) > 0)
     memmove(savearg, *$1, strlen(*$1)+1);
  *$1 = savearg;
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

%apply double argout[ANY] {double *inout}

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
 * Typemap for int argin[ANY]. 
 */

%typemap(imtype) (int argin[ANY])  "int[]"
%typemap(cstype) (int argin[ANY]) "int[]"
%typemap(csin) (int argin[ANY])  "$csinput"

%typemap(in) (int argin[ANY])
{
  /* %typemap(in) (int argin[ANY]) */
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

%apply (double inout[ANY]) {double *pList};

/*
 * Typemap for int inout[ANY]. 
 */

%typemap(imtype) (int inout[ANY])  "int[]"
%typemap(cstype) (int inout[ANY]) "int[]"
%typemap(csin) (int inout[ANY])  "$csinput"

%typemap(in) (int inout[ANY])
{
  /* %typemap(in) (int inout[ANY]) */
  $1 = ($1_ltype)$input;
}

%apply (int inout[ANY]) {int *pList};

/*
 * Typemap for const char *utf8_path. 
 */
%typemap(csin) (const char *utf8_path)  "System.Text.Encoding.Default.GetString(System.Text.Encoding.UTF8.GetBytes($csinput))" 

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

/*
 * Typemap for out double.
 */

%typemap(imtype) (double *OUTPUT), (double *val), (double *min), (double *max), (double *mean), (double *stddev) "out double"
%typemap(cstype) (double *OUTPUT), (double *val), (double *min), (double *max), (double *mean), (double *stddev) "out double"
%typemap(csin) (double *OUTPUT), (double *val), (double *min), (double *max), (double *mean), (double *stddev) "out $csinput"

%typemap(in) (double *OUTPUT), (double *val), (double *min), (double *max), (double *mean), (double *stddev)
{
  /* %typemap(in) (double *val) */
  $1 = ($1_ltype)$input;
}

/*
 * Typemap for 'out int'.
 */

%typemap(imtype) (int *hasval)  "out int"
%typemap(cstype) (int *hasval) "out int"
%typemap(csin) (int *hasval)  "out $csinput"

%typemap(in) (int *hasval)
{
  /* %typemap(in) (int *hasval) */
  $1 = ($1_ltype)$input;
}

%apply (int *hasval) {int *nLen};
%apply (int *hasval) {int *pnBytes};

/*
 * Typemap for int **array_argout. 
 */

%typemap(imtype) (int **array_argout)  "out int[]"
%typemap(cstype) (int **array_argout) "out int[]"
%typemap(csin) (int **array_argout)  "out $csinput"

%typemap(in) (int **array_argout)
{
  /* %typemap(in) (int **array_argout) */
  $1 = ($1_ltype)$input;
}

%apply (int **array_argout) {int **pList};

/*
 * Typemap for double **array_argout. 
 */

%typemap(imtype) (double **array_argout)  "out double[]"
%typemap(cstype) (double **array_argout) "out double[]"
%typemap(csin) (double **array_argout)  "out $csinput"

%typemap(in) (double **array_argout)
{
  /* %typemap(in) (double **array_argout) */
  $1 = ($1_ltype)$input;
}

%apply (double **array_argout) {double **pList};

/******************************************************************************
 * GDAL raster R/W support                                                    *
 *****************************************************************************/
 
%typemap(imtype, out="IntPtr") void *buffer_ptr "IntPtr"
%typemap(cstype) void *buffer_ptr %{IntPtr%}
%typemap(in) void *buffer_ptr %{ $1 = ($1_ltype)$input; %}
%typemap(out) void *buffer_ptr %{ $result = $1; %}
%typemap(csin) void *buffer_ptr "$csinput"
%typemap(csout, excode=SWIGEXCODE) void *buffer_ptr {
      IntPtr ret = $imcall;$excode
      return ret;
}

%apply (void *buffer_ptr) {GByte*};

%csmethodmodifiers StringListDestroy "internal";
%inline %{
    void StringListDestroy(void *buffer_ptr) {
       CSLDestroy((char**)buffer_ptr);
    }
%}

/******************************************************************************
 * ErrorHandler callback support                                              *
 *****************************************************************************/
%pragma(csharp) modulecode="public delegate void GDALErrorHandlerDelegate(int eclass, int code, IntPtr msg);"
%typemap(imtype) (CPLErrorHandler)  "$module.GDALErrorHandlerDelegate"
%typemap(cstype) (CPLErrorHandler) "$module.GDALErrorHandlerDelegate"
%typemap(csin) (CPLErrorHandler)  "$csinput"
%typemap(in) (CPLErrorHandler) %{ $1 = ($1_ltype)$input; %}

/******************************************************************************
 * GDALProgressFunc typemaps                                                  *
 *****************************************************************************/
%pragma(csharp) modulecode="public delegate int GDALProgressFuncDelegate(double Complete, IntPtr Message, IntPtr Data);"

%typemap(imtype) (GDALProgressFunc callback)  "$module.GDALProgressFuncDelegate"
%typemap(cstype) (GDALProgressFunc callback) "$module.GDALProgressFuncDelegate"
%typemap(csin) (GDALProgressFunc callback)  "$csinput"
%typemap(in) (GDALProgressFunc callback) %{ $1 = ($1_ltype)$input; %}
%typemap(imtype) (void* callback_data) "string"
%typemap(cstype) (void* callback_data) "string"
%typemap(csin) (void* callback_data) "$csinput"

