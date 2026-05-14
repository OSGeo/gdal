/******************************************************************************
 *
 * Project:  UK NTF Reader
 * Purpose:  Implements OGRNTFDriver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ntf.h"
#include "cpl_conv.h"

/************************************************************************/
/* ==================================================================== */
/*                            OGRNTFDriver                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRNTFDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!poOpenInfo->bStatOK)
        return nullptr;

    if (poOpenInfo->nHeaderBytes != 0)
    {
        if (poOpenInfo->nHeaderBytes < 80)
            return nullptr;
        const char *pszHeader = (const char *)poOpenInfo->pabyHeader;
        if (!STARTS_WITH_CI(pszHeader, "01"))
            return nullptr;

        int j = 0;  // Used after for.
        for (; j < 80; j++)
        {
            if (pszHeader[j] == 10 || pszHeader[j] == 13)
                break;
        }

        if (j == 80 || pszHeader[j - 1] != '%')
            return nullptr;
    }

    OGRNTFDataSource *poDS = new OGRNTFDataSource;
    if (!poDS->Open(poOpenInfo->pszFilename, TRUE))
    {
        delete poDS;
        poDS = nullptr;
    }

    if (poDS != nullptr && poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "NTF Driver doesn't support update.");
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRNTF()                           */
/************************************************************************/

void RegisterOGRNTF()

{
    if (GDALGetDriverByName("UK .NTF") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("UK .NTF");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "UK .NTF");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/ntf.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRNTFDriverOpen;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
