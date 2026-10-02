/******************************************************************************
 *
 * Name:     osr_csharp.i
 * Project:  GDAL CSharp Interface
 * Purpose:  OSR CSharp SWIG Interface declarations.
 * Author:   Tamas Szekeres, szekerest@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2007, Tamas Szekeres
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

%include cpl_exceptions.i

%include csharp_string_encoder.i
%include typemaps_csharp.i

%apply (int *hasval) {int* pnListCount};

%{
typedef OSRCRSInfo* OSRCRSInfoList;
%}

%typemap(cscode) OSRCRSInfoList %{  
  public CRSInfo this[int i]
  {
     get { return get(i); }
  }
%}


%rename (CRSInfoList) OSRCRSInfoList;

struct OSRCRSInfoList {
%extend {

  OSRCRSInfo* get(int index) {
     return self[index];
  }

  ~OSRCRSInfoList() {
    OSRDestroyCRSInfoList(self);
  }
} /* extend */
}; /* OSRCRSInfoList */

%newobject GetCRSInfoListFromDatabase;
%inline %{
OSRCRSInfoList* GetCRSInfoListFromDatabase( char* authName, int* pnListCount )
{
    return (OSRCRSInfoList*)OSRGetCRSInfoListFromDatabase(authName, NULL, pnListCount);
}
%}

/*
 * Overloads to maintain backwards compatibility with multi-argument typemaps
 */

%typemap(cscode, noblock="1") OSRSpatialReferenceShadow {
  [Obsolete("Use ExportToPCI(out string proj, out string units, out double[] params_) instead.")]
  public int ExportToPCI(out string proj, out string units)
    => ExportToPCI(out proj, out units, out _);
  [Obsolete("Use ExportToUSGS(out int code, out int zone, out double[] params_, out int datum) instead.")]
  public int ExportToUSGS(out int code, out int zone, out int datum)
    => ExportToUSGS(out code, out zone, out _, out datum);
  [Obsolete("Use SetDataAxisToSRSAxisMapping(int[] nList) instead.")]
  public int SetDataAxisToSRSAxisMapping(int nList, int[] pList)
    => SetDataAxisToSRSAxisMapping(pList);
  [Obsolete("Use FindMatches(string[] options, out int[] confidence_values) instead.")]
  public SpatialReference[] FindMatches(string[] options, out int nvalues, out int[] confidence_values) {
    var ret = FindMatches(options, out confidence_values);
	nvalues = confidence_values.Length;
	return ret;
  }
}
/*
 * Overloads to maintain backwards compatibility with multi-argument typemaps
 */

%typemap(cscode, noblock="1") OSRCoordinateTransformationShadow {
  [Obsolete("Use TransformPoints(double[] x, double[] y, double[] z, double[] t, int[] success) instead.")]
  public void TransformPoints(int nCount, double[] x, double[] y, double[] z)
    => TransformPoints(x, y, z);
}

/*****************************************************************************
 * Enable C# default arguments all OSR methods                               *
 * Apply fixes to specific methods to translate C++ default values to C#     *
 ****************************************************************************/
 
#if SWIG_VERSION >= 0x040200 && !defined(FROM_GDAL_I)
%feature("cs:defaultargs");

%feature("cs:defaultargs", options="null")    OSRSpatialReferenceShadow::IsSame;
%feature("cs:defaultargs", options="null")    OSRSpatialReferenceShadow::ExportToWkt;
%feature("cs:defaultargs", options="null")    OSRSpatialReferenceShadow::ExportToCF1;
%feature("cs:defaultargs", options="null")    OSRSpatialReferenceShadow::ExportToCF1Units;
%feature("cs:defaultargs", options="null")    OSRSpatialReferenceShadow::ExportToPROJJSON;
%feature("cs:defaultargs", options="null")    OSRSpatialReferenceShadow::ConvertToOtherProjection;
%feature("cs:defaultargs", target_key="null") OSRSpatialReferenceShadow::GetAuthorityName;
%feature("cs:defaultargs", target_key="null") OSRSpatialReferenceShadow::GetAuthorityCode;
%feature("cs:defaultargs", name="null")       OSRSpatialReferenceShadow::PromoteTo3D;
%feature("cs:defaultargs", name="null")       OSRSpatialReferenceShadow::DemoteTo2D;
%feature("cs:defaultargs", units="null")      OSRSpatialReferenceShadow::ImportFromCF1;
%feature("cs:defaultargs", argin="null")      OSRSpatialReferenceShadow::ImportFromPCI;
%feature("cs:defaultargs", argin="null")      OSRSpatialReferenceShadow::ImportFromUSGS;

%feature("cs:defaultargs", z="null", t="null", success="null")    OSRCoordinateTransformationShadow::TransformPoints;
%feature("cs:defaultargs", z="null", t="null", errorCodes="null") OSRCoordinateTransformationShadow::TransformPointsWithErrorCodes;

%feature("cs:defaultargs", options="null")    CreateCoordinateTransformation;

#endif //SWIG_VERSION >= 0x040200 && !defined(FROM_GDAL_I)
