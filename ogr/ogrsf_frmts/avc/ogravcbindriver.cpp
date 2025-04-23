/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  OGRAVCBinDriver implementation (Arc/Info Binary Coverages)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_avc.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRAVCBinDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->eAccess == GA_Update)
        return nullptr;
    if (!poOpenInfo->bStatOK)
        return nullptr;
    if (poOpenInfo->fpL != nullptr)
    {
        char **papszSiblingFiles = poOpenInfo->GetSiblingFiles();
        if (papszSiblingFiles != nullptr)
        {
            bool bFoundCandidateFile = false;
            for (int i = 0; papszSiblingFiles[i] != nullptr; i++)
            {
                if (EQUAL(CPLGetExtensionSafe(papszSiblingFiles[i]).c_str(),
                          "ADF"))
                {
                    bFoundCandidateFile = true;
                    break;
                }
            }
            if (!bFoundCandidateFile)
                return nullptr;
        }
    }

    OGRAVCBinDataSource *poDS = new OGRAVCBinDataSource();

    if (poDS->Open(poOpenInfo->pszFilename, TRUE) && poDS->GetLayerCount() > 0)
    {
        return poDS;
    }
    delete poDS;

    return nullptr;
}

/************************************************************************/
/*                           RegisterOGRAVC()                           */
/************************************************************************/

void RegisterOGRAVCBin()

{
    if (GDALGetDriverByName("AVCBin") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("AVCBin");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Arc/Info Binary Coverage");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/avcbin.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRAVCBinDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
