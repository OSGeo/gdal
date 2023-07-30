/***************************************************************************
  gdal_subdatasetinfo.cpp - GDALSubdatasetInfo

 ---------------------
 begin                : 21.7.2023
 copyright            : (C) 2023 by ale
 email                : [your-email-here]
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "gdalsubdatasetinfo.h"
#include "gdal_priv.h"

/************************************************************************/
/*                     Subdataset informational functions               */
/************************************************************************/

GDALSubdatasetInfoH CPL_STDCALL GDALGetSubdatasetInfo(const char *pszFileName)
{
    // Iterate all drivers
    GDALDriverManager *poDM_ = GetGDALDriverManager();
    const int nDriverCount = poDM_->GetDriverCount();
    for (int iDriver = 0; iDriver < nDriverCount; ++iDriver)
    {
        GDALDriver *poDriver = poDM_->GetDriver(iDriver);
        char **papszMD = GDALGetMetadata(poDriver, nullptr);
        if (!CPLFetchBool(papszMD, GDAL_DMD_SUBDATASETS, false) ||
            !poDriver->pfnGetSubdatasetInfoFunc)
        {
            continue;
        }

        GDALSubdatasetInfo *poGetSubdatasetInfo =
            poDriver->pfnGetSubdatasetInfoFunc();

        if (!poGetSubdatasetInfo)
        {
            continue;
        }

        if (poGetSubdatasetInfo->IsSubdatasetSyntax(pszFileName))
        {
            return static_cast<GDALSubdatasetInfoH>(poGetSubdatasetInfo);
        }

        delete poGetSubdatasetInfo;
    }
    return nullptr;
}

/************************************************************************/
/*                       GDALDestroySubdatasetInfo()                    */
/************************************************************************/

/**
 * \brief Destroys subdataset info.
 *
 * This function is the same as the C++ method GDALSubdatasetInfo::~GDALSubdatasetInfo()
 */
void CPL_STDCALL GDALDestroySubdatasetInfo(GDALSubdatasetInfoH hInfo)

{
    delete hInfo;
}

const char *CPL_STDCALL GDALSubdatasetInfoGetFileName(GDALSubdatasetInfoH hInfo,
                                                      const char *pszFileName)
{
    return CPLStrdup(hInfo->GetFilenameFromSubdatasetName(pszFileName).c_str());
}

bool CPL_STDCALL GDALSubdatasetInfoIsSubdatasetSyntax(GDALSubdatasetInfoH hInfo,
                                                      const char *pszFileName)
{
    return hInfo->IsSubdatasetSyntax(pszFileName);
}

const char *CPL_STDCALL GDALSubdatasetInfoModifyFileName(
    GDALSubdatasetInfoH hInfo, const char *pszFileName,
    const char *pszNewFileName)
{
    return CPLStrdup(hInfo->ModifyFileName(pszFileName, pszNewFileName).c_str());
}
