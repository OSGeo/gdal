/******************************************************************************
 *
 * Name:     SubdatasetInfo.i
 * Project:  GDAL Python Interface
 * Purpose:  SWIG Interface for GDALSubdatasetInfo class.
 * Author:   Alessandro Pasotti
 *
 ******************************************************************************
 * Copyright (c) 2023, Alessandro Pasotti <elpaso@itopen.it>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

//************************************************************************
//
// Define the extensions for GDALSubdatasetInfo
//
//************************************************************************

%{
#include "gdalsubdatasetinfo.h"
%}

typedef struct GDALSubdatasetInfo GDALSubdatasetInfoShadow;

%rename (SubdatasetInfo) GDALSubdatasetInfoShadow;

struct GDALSubdatasetInfoShadow {

    private:

        GDALSubdatasetInfoShadow();

    public:

%extend {


        ~GDALSubdatasetInfoShadow() {
            GDALDestroySubdatasetInfo(reinterpret_cast<GDALSubdatasetInfoH>(self));
        }

        retStringAndCPLFree* GetPathComponent()
        {
            return GDALSubdatasetInfoGetPathComponent(reinterpret_cast<GDALSubdatasetInfoH>(self) );
        }

        retStringAndCPLFree* GetSubdatasetComponent()
        {
        return GDALSubdatasetInfoGetSubdatasetComponent(reinterpret_cast<GDALSubdatasetInfoH>(self) );
        }

        retStringAndCPLFree* ModifyPathComponent(const char *pszNewFileName)
        {
        return GDALSubdatasetInfoModifyPathComponent(reinterpret_cast<GDALSubdatasetInfoH>(self), pszNewFileName );
        }

}
};

%newobject GetSubdatasetInfo;

%inline %{
GDALSubdatasetInfoShadow* GetSubdatasetInfo(const char *pszFileName)
{
    GDALSubdatasetInfoH info { GDALGetSubdatasetInfo(pszFileName) };

    if( ! info )
    {
      return nullptr;
    }

    return (GDALSubdatasetInfoShadow*)( info );
};
%}
