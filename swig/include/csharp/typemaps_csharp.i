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

%apply (char*) {(tostring argin)};
%apply (long) {(IF_FALSE_RETURN_NONE)};
%apply (long long) {GIntBig};
%apply (unsigned long long) {GUIntBig};

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
%enddef //OPTIONAL_POD

OPTIONAL_POD(int, int);

/*
 * Typemap for size_t native_size
 */
%typemap(ctype)  (size_t native_size), (const size_t &native_size) "size_t"
%typemap(imtype) (size_t native_size), (const size_t &native_size) "IntPtr"
%typemap(cstype) (size_t native_size), (const size_t &native_size) "IntPtr"
%typemap(in)     (size_t native_size), (const size_t &native_size) "$1 = $input;"
%typemap(out)    (size_t native_size), (const size_t &native_size) "$result = $1;"
%typemap(csout, excode=SWIGEXCODE) (size_t native_size), (const size_t &native_size) {
    IntPtr res = $imcall;$excode
    return res;
}

/*
 * Typemap for PINNED arrays
 */

PRIMITIVE_ARRAYS_INOUT(GIntBig, long)
PRIMITIVE_ARRAYS_INOUT(GUIntBig, ulong)
PRIMITIVE_ARRAYS_INOUT(char, byte)
PRIMITIVE_ARRAYS_INOUT(int, int)
PRIMITIVE_ARRAYS_INOUT(double, double)
OBJECT_LIST_INOUT(GDALDatasetShadow, Dataset)
OBJECT_LIST_INOUT(GDALRasterBandShadow, Band)
OBJECT_LIST_INOUT(GDALEDTComponentHS, EDTComponent)
OBJECT_LIST_INOUT(OGRLayerShadow, OSGeo.OGR.Layer)

%define %NUMBER_VALUE_LIST(CTYPE, CSTYPE)

%typemap(imtype, out="IntPtr")     CTYPE *CSTYPE##List "CSTYPE[]"
%typemap(cstype)                   CTYPE *CSTYPE##List %{CSTYPE[]%}
%typemap(in)                       CTYPE *CSTYPE##List %{ $1 = ($1_ltype)$input; %}
%typemap(out)                      CTYPE *CSTYPE##List %{ $result = $1; %}
%typemap(csout, excode=SWIGEXCODE) CTYPE *CSTYPE##List {
        /* %typemap(csout) CTYPE *CSTYPE##List */
        IntPtr cPtr = $imcall;
        CSTYPE[] ret = new CSTYPE[count];
        if (count > 0) {
            System.Runtime.InteropServices.Marshal.Copy(cPtr, ret, 0, count);
        }
        $excode
        return ret;
}
%enddef //%NUMBER_VALUE_LIST

%NUMBER_VALUE_LIST(GByte, byte);    // (GByte   *byteList)
%NUMBER_VALUE_LIST(int, int);       // (int     *intList)
%NUMBER_VALUE_LIST(GIntBig, long);  // (GIntBig *longList)
%NUMBER_VALUE_LIST(double, double); // (double  *doubleList)

/*
 * Macro for generating CTYPE typemaps for *argout[ANY], argout[ANY],
 * argin[ANY], and inout[ANY]
 */

%define %FIXED_SIZE_ARRAYS(CTYPE, CSTYPE)

%typemap(ctype)   (CTYPE *argout[ANY]) "CTYPE*"
%typemap(imtype)  (CTYPE *argout[ANY]) "CSTYPE[]"
%typemap(cstype)  (CTYPE *argout[ANY]) "out CSTYPE[]"
%typemap(csin, pre="$csinput = new CSTYPE[$1_dim0];") (CTYPE *argout[ANY]) "$csinput"
%typemap(in)      (CTYPE *argout[ANY]) {
  /* %typemap(in) (CTYPE *argout[ANY]) */
  $*1_ltype tmp$1_name = NULL;
  $1 = ($1_ltype)&tmp$1_name;
}
%typemap(argout)  (CTYPE *argout[ANY]) {
  /* %typemap(argout) (CTYPE *argout[ANY]) */
  memcpy($input, *$1, $1_dim0 * sizeof(CTYPE));
}
%typemap(freearg) (CTYPE *argout[ANY]) {
  /* %typemap(freearg) (CTYPE *argout[ANY]) */
  CPLFree(*$1);
}

%typemap(ctype)  CTYPE inout[ANY] "CTYPE*"
%typemap(imtype) CTYPE inout[ANY] "CSTYPE[]"
%typemap(cstype) CTYPE inout[ANY] "CSTYPE[]"
%typemap(in)     CTYPE inout[ANY] {
  /* %typemap(in) CTYPE inout[ANY] */
  $1 = ($1_ltype)$input;
}
%typemap(csin, pre="
    if($csinput is null) throw new ArgumentNullException(\"$csinput\");
    if($csinput.Length < $1_dim0) throw new ArgumentException(\"Array must be at least $1_dim0 elements long.\", \"$csinput\");"
) CTYPE inout[ANY] "$csinput"

%apply CTYPE inout [ANY] {CTYPE argin [ANY], CTYPE argout[ANY]};
%enddef // %FIXED_SIZE_ARRAYS

%FIXED_SIZE_ARRAYS(int, int);
%FIXED_SIZE_ARRAYS(double, double);

/*
 * Typemap for 'out double'.
 */
%apply (double *OUTPUT) {(double *val), (double *min), (double *max), (double *mean), (double *stddev)};

/*
 * Typemap for 'out int'.
 */
%apply (int *OUTPUT) {int *hasval, int *nLen, int *pnBytes};

/*
 * Typemap for void* user_data for SetErrorHandler.
 * Note: The user should implement marshaling their own data to IntPtr.
 */

%apply (void *VOID_INT_PTR) {void* user_data, void *buffer_ptr, GByte*, VSILFILE*};

%csmethodmodifiers CPLMemDestroy "internal";
%inline %{
    void CPLMemDestroy(void *buffer_ptr) {
       if (buffer_ptr)
           CPLFree(buffer_ptr);
    }
%}

%define %DELEGATE_TYPEMAP(CTYPE, CSTYPE)

%typemap(ctype)  (CTYPE) "CTYPE"
%typemap(imtype) (CTYPE) "CSTYPE"
%typemap(cstype) (CTYPE) "CSTYPE"
%typemap(csin)   (CTYPE) "$csinput"
%typemap(in)     (CTYPE) %{ $1 = $input; %}
%typemap(out)    (CTYPE) %{ $result = $1; %}
%typemap(csvarout, excode=SWIGEXCODE2) (CTYPE)   %{
    get {
      CSTYPE ret = $imcall;$excode
      return ret;
    } %}
%typemap(csout, excode=SWIGEXCODE) (CTYPE)   %{
    CSTYPE ret = $imcall;$excode
    return ret;
%}
%enddef //DELEGATE_TYPEMAP

/******************************************************************************
 * CPLErrorHandler callback support                                           *
 *****************************************************************************/
%pragma(csharp) modulecode="public delegate void GDALErrorHandlerDelegate(int eclass, int code, IntPtr msg);"
%DELEGATE_TYPEMAP(CPLErrorHandler, $module.GDALErrorHandlerDelegate);

/******************************************************************************
 * GDALTransformerFunc typemaps                                                  *
 *****************************************************************************/
%pragma(csharp) modulecode="public delegate bool GDALTransformerFuncDelegate(IntPtr pTransformerArg, int bDstToSrc, int nPointCount, IntPtr x, IntPtr y, IntPtr z, IntPtr panSuccess);"
%DELEGATE_TYPEMAP(GDALTransformerFunc, $module.GDALTransformerFuncDelegate);

/******************************************************************************
 * GDALProgressFunc typemaps                                                  *
 *****************************************************************************/
%pragma(csharp) modulecode="public delegate int GDALProgressFuncDelegate(double Complete, IntPtr Message, IntPtr Data);"
%DELEGATE_TYPEMAP(GDALProgressFunc, $module.GDALProgressFuncDelegate);

%apply (char*) {(void* callback_data)};

/******************************************************************************
 * GDALGetNextFeature typemaps                                                *
 *****************************************************************************/

%typemap(imtype) (OGRLayerShadow **ppoBelongingLayer) "ref IntPtr"
%typemap(cstype) (OGRLayerShadow **ppoBelongingLayer) "out OSGeo.OGR.Layer"
%typemap(csin,
  pre="    IntPtr p$csinput = IntPtr.Zero;",
  post="      $csinput = p$csinput == IntPtr.Zero ? null : new OSGeo.OGR.Layer(p$csinput, false, ThisOwn_false());"
) (OGRLayerShadow **ppoBelongingLayer) "ref p$csinput"

/******************************************************************************
 * SpatialReference.FindMatches                                               *
 *****************************************************************************/
%apply (int *nLen, int **pList_free) {(int* confidence_values, int** ppanMatchConfidence )};
%typemap(imtype, out="IntPtr") OSRSpatialReferenceShadow** FindMatches "SpatialReference[]"
%typemap(cstype) OSRSpatialReferenceShadow** FindMatches %{SpatialReference[]%}
%typemap(csout, excode=SWIGEXCODE) OSRSpatialReferenceShadow** FindMatches {
        /* %typemap(csout) OSRSpatialReferenceShadow** FindMatches */
        IntPtr cPtr = $imcall;
        $excode
        var srsArray = new $modulePINVOKE.ArrayWithSize(tempconfidence_values.Count, IntPtr.Size, cPtr);
        SpatialReference[] ret = srsArray.ToReferenceArray<SpatialReference>(p => new SpatialReference(p, true, ThisOwn_true()));
        if (cPtr != IntPtr.Zero) {
            $modulePINVOKE.CPLMemDestroy(cPtr);
        }
        return ret;
}

/***************************************************
 * Typemaps converts a OGRCodedValue to/from a     *
 * Dictionary<string, string>                      *
 ***************************************************/

%typemap(ctype)  const OGRCodedValue* "OGRCodedValue*"
%typemap(imtype, out="IntPtr") const OGRCodedValue* "IntPtr[]"
%typemap(in)     const OGRCodedValue* %{ $1 = $input; %}
%typemap(out)    const OGRCodedValue* %{ $result = $1; %}
%typemap(cstype) const OGRCodedValue* "System.Collections.Generic.Dictionary<string,string>"
%typemap(csin, cshin="$csinput",
  pre="    using (var temp$csinput = new $modulePINVOKE.StringListMarshal($csinput)) { ",
  terminator="    }")
  const OGRCodedValue*
  "temp$csinput._ar"

%typemap(csout, excode=SWIGEXCODE) const OGRCodedValue*
{
  /* %typemap(csout) const OGRCodedValue* */
  IntPtr pEnum = $imcall;
  $excode
  var kvps = $modulePINVOKE.StringListMarshal.DecodeKeyValuePairArray(pEnum);
  var dict = new System.Collections.Generic.Dictionary<string,string>(kvps.Length);
  foreach (var kvp in kvps)
    dict.Add(kvp.Key, kvp.Value);
  return dict;
}
