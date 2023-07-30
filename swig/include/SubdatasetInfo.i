/******************************************************************************
 * $Id$
 *
 * Name:     SubdatasetInfo.i
 * Project:  GDAL Python Interface
 * Purpose:  SWIG Interface for GDALSubdatasetInfo class.
 * Author:   Alessandro Pasotti
 *
 ******************************************************************************
 * Copyright (c) 2023, Alessandro Pasotti <elpaso@itopen.it>
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
class GDALSubdatasetInfoShadow {

    public:

%extend {

        ~GDALSubdatasetInfoShadow() {
            GDALDestroySubdatasetInfo(reinterpret_cast<GDALSubdatasetInfoH>(self));
        }

        retStringAndCPLFree* GetFilenameFromSubdatasetName(const char *pszFileName)
        {
            return GDALSubdatasetInfoGetFileName(reinterpret_cast<GDALSubdatasetInfoH>(self), pszFileName );
        }

        retStringAndCPLFree* ModifyFileName(const char *pszFileName, const char *pszNewFileName)
        {
        return GDALSubdatasetInfoModifyFileName(reinterpret_cast<GDALSubdatasetInfoH>(self), pszFileName, pszNewFileName );
        }

        bool IsSubdatasetSyntax(const char *pszFileName)
        {
            return GDALSubdatasetInfoIsSubdatasetSyntax(reinterpret_cast<GDALSubdatasetInfoH>(self), pszFileName );
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
