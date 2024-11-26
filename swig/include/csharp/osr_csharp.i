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

