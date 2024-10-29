/******************************************************************************
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements OGRTigerDriver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_tiger.h"
#include "cpl_conv.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRTigerDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!poOpenInfo->bStatOK)
        return nullptr;
    char **papszSiblingFiles = poOpenInfo->GetSiblingFiles();
    if (papszSiblingFiles != nullptr)
    {
        bool bFoundCompatibleFile = false;
        for (int i = 0; papszSiblingFiles[i] != nullptr; i++)
        {
            int nLen = (int)strlen(papszSiblingFiles[i]);
            if (nLen > 4 && papszSiblingFiles[i][nLen - 4] == '.' &&
                papszSiblingFiles[i][nLen - 1] == '1')
            {
                bFoundCompatibleFile = true;
                break;
            }
        }
        if (!bFoundCompatibleFile)
            return nullptr;
    }

    OGRTigerDataSource *poDS = new OGRTigerDataSource;

    if (!poDS->Open(poOpenInfo->pszFilename, TRUE))
    {
        delete poDS;
        poDS = nullptr;
    }

    if (poDS != nullptr && poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Tiger Driver doesn't support update.");
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRTiger()                         */
/************************************************************************/

void RegisterOGRTiger()

{
    if (GDALGetDriverByName("TIGER") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("TIGER");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "U.S. Census TIGER/Line");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/tiger.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRTigerDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
