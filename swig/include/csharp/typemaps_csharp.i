/******************************************************************************
 *
 * Name:     typemaps_csharp.i
 * Project:  GDAL CSharp Interface
 * Purpose:  Typemaps for C# bindings.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/


%include "typemaps.i"
%include "arrays_csharp.i"
%include "csharp_strings.i"

/* CSHARP TYPEMAPS */

%typemap(csdispose) SWIGTYPE %{
  ~$csclassname() {
    Dispose();
  }
%}

%typemap(csdispose_derived) SWIGTYPE %{
  ~$csclassname() {
    Dispose();
  }
%}

%typemap(csdisposing, methodname="Dispose", methodmodifiers="public", parameters="") SWIGTYPE {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          $imcall;
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
      global::System.GC.SuppressFinalize(this);
    }
  }

%typemap(csdisposing_derived, methodname="Dispose", methodmodifiers="public", parameters="") SWIGTYPE {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          $imcall;
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
      global::System.GC.SuppressFinalize(this);
      base.Dispose();
    }
  }

%apply (int) {VSI_RETVAL};

%fragment("OGRErrMessages","header") %{
static char const *
OGRErrMessages( int rc ) {
  switch( rc ) {
  case 0:
    return "OGR Error %d: None";
  case 1:
    return "OGR Error %d: Not enough data to deserialize";
  case 2:
    return "OGR Error %d: Not enough memory";
  case 3:
    return "OGR Error %d: Unsupported geometry type";
  case 4:
    return "OGR Error %d: Unsupported operation";
  case 5:
    return "OGR Error %d: Corrupt data";
  case 6:
    return "OGR Error %d: General Error";
  case 7:
    return "OGR Error %d: Unsupported SRS";
  case 8:
    return "OGR Error %d: Invalid handle";
  case 9:
    return "OGR Error %d: Non existing feature";
  default:
    return "OGR Error %d: Unknown";
  }
}
%}

/*
 * Helper to marshal utf8 strings.
 */

%pragma(csharp) modulecode=%{
  internal unsafe static string Utf8BytesToString(IntPtr pNativeData)
  {
    if (pNativeData == IntPtr.Zero)
        return null;

    byte* pStringUtf8 = (byte*) pNativeData;
    int len = 0;
    while (pStringUtf8[len] != 0) len++;
    return System.Text.Encoding.UTF8.GetString(pStringUtf8, len);
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

/*
 * Typemap for GIntBig (int64)
 */

%typemap(ctype, out="GIntBig") GIntBig  %{GIntBig%}
%typemap(imtype, out="long") GIntBig "long"
%typemap(cstype) GIntBig %{long%}
%typemap(in) GIntBig %{ $1 = $input; %}
%typemap(out) GIntBig %{ $result = $1; %}
%typemap(csin) GIntBig "$csinput"
%typemap(csout, excode=SWIGEXCODE) GIntBig {
    long res = $imcall;$excode
    return res;
}

/*
 * Typemap for GUIntBig (uint64)
 */

%typemap(ctype, out="GUIntBig") GUIntBig  %{GUIntBig%}
%typemap(imtype, out="ulong") GUIntBig "ulong"
%typemap(cstype) GUIntBig %{ulong%}
%typemap(in) GUIntBig %{ $1 = $input; %}
%typemap(out) GUIntBig %{ $result = $1; %}
%typemap(csin) GUIntBig "$csinput"
%typemap(csout, excode=SWIGEXCODE) GUIntBig {
    ulong res = $imcall;$excode
    return res;
}

/*
 * Typemap for PINNED arrays
 */

CSHARP_ARRAYS_PINNED(GUIntBig, uint)
CSHARP_ARRAYS_PINNED(int, int)
CSHARP_OBJECT_ARRAYS_PINNED(GDALRasterBandShadow, Band)

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

/*
 * Typemap for void* user_data for SetErrorHandler.
 * Note: The user should implement marshaling their own data to IntPtr.
 */

%typemap(imtype) (void* user_data) "IntPtr"
%typemap(cstype) (void* user_data) "IntPtr"
%typemap(csin) (void* user_data)  "$csinput"

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
%typemap(csvarout, excode=SWIGEXCODE2) (void *buffer_ptr)   %{
    get {
      IntPtr ret = $imcall;$excode
      return ret;
    } %}

%apply (void *buffer_ptr) {GByte*, VSILFILE*};

%csmethodmodifiers CPLMemDestroy "internal";
%inline %{
    void CPLMemDestroy(void *buffer_ptr) {
       if (buffer_ptr)
           CPLFree(buffer_ptr);
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
%typemap(csvarout, excode=SWIGEXCODE2) (GDALProgressFunc callback)   %{
    get {
      Gdal.GDALProgressFuncDelegate ret = $imcall;$excode
      return ret;
    } %}
%typemap(in) (GDALProgressFunc callback) %{ $1 = ($1_ltype)$input; %}
%apply (char*) {(void* callback_data)};

%ignore SWIGTYPE_p_GDALProgressFunc;

/******************************************************************************
 * GDALGetNextFeature typemaps                                                *
 *****************************************************************************/

%apply (double *defaultval) {double* pdfProgressPct};

%typemap(imtype) (OGRLayerShadow **ppoBelongingLayer)  "ref IntPtr"
%typemap(cstype) (OGRLayerShadow **ppoBelongingLayer) "ref IntPtr"
%typemap(csin) (OGRLayerShadow **ppoBelongingLayer)  "ref $csinput"

/******************************************************************************
 * Band.AdviseRead and Dataset.AdviseRead typemaps                            *
 *****************************************************************************/
%apply (int *INOUT) {int *buf_xsize, int *buf_ysize};

/******************************************************************************
 * SpatialReference.FindMatches                                               *
 *****************************************************************************/
%apply (int *hasval) {int *nvalues};
%typemap(imtype, out="IntPtr") OSRSpatialReferenceShadow** FindMatches "SpatialReference[]"
%typemap(cstype) OSRSpatialReferenceShadow** FindMatches %{SpatialReference[]%}
%typemap(imtype) int** confidence_values "out IntPtr"
%typemap(cstype) int** confidence_values %{out int[]%}
%typemap(csin) int** confidence_values "out confValPtr"
%typemap(in) (int** confidence_values)
{
  /* %typemap(in) (int** confidence_values) */
  $1 = ($1_ltype)$input;
}
%typemap(csout, excode=SWIGEXCODE) OSRSpatialReferenceShadow** FindMatches {
        /* %typemap(csout) OSRSpatialReferenceShadow** FindMatches */
        IntPtr confValPtr;
        IntPtr cPtr = $imcall;
        IntPtr objPtr;
        SpatialReference[] ret = new SpatialReference[nvalues];
        confidence_values = (confValPtr == IntPtr.Zero) ? null : new int[nvalues];
        if (nvalues > 0) {
	        for(int cx = 0; cx < nvalues; cx++) {
                objPtr = System.Runtime.InteropServices.Marshal.ReadIntPtr(cPtr, cx * System.Runtime.InteropServices.Marshal.SizeOf(typeof(IntPtr)));
                /* SpatialReference will take ownership of the unmanaged memory and will call OSRRelease() when the object is disposed.
                   Therefore, OSRFreeSRSArray() is not called; only CPLFree() is used to release the array itself. */
                ret[cx]= (objPtr == IntPtr.Zero) ? null : new SpatialReference(objPtr, true, null);
                if (confValPtr != IntPtr.Zero) {
                    confidence_values[cx] = System.Runtime.InteropServices.Marshal.ReadInt32(confValPtr, cx * System.Runtime.InteropServices.Marshal.SizeOf(typeof(Int32)));
                }

            }
        }
        if (cPtr != IntPtr.Zero) {
            $modulePINVOKE.CPLMemDestroy(cPtr);
        }
        if (confValPtr != IntPtr.Zero) {
            $modulePINVOKE.CPLMemDestroy(confValPtr);
        }
        $excode
        return ret;
}
