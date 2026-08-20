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
}
