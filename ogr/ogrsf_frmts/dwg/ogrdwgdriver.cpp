/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements OGRDWGDriver.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2011, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dwg.h"
#include "cpl_conv.h"
#include "ogrteigha.h"
#include "ogrdwgdrivercore.h"

/************************************************************************/
/*                          OGRDWGDriverUnload()                        */
/************************************************************************/

static void OGRDWGDriverUnload(GDALDriver *)
{
    CPLDebug("DWG", "Driver cleanup");
    OGRTEIGHADeinitialize();
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRDWGDriverOpen(GDALOpenInfo *poOpenInfo)
{
    if (!OGRDWGDriverIdentify(poOpenInfo))
        return nullptr;

    // Check that this is a real file since the driver doesn't support
    // VSI*L API
    VSIStatBuf sStat;
    if (VSIStat(poOpenInfo->pszFilename, &sStat) != 0)
        return nullptr;

    if (!OGRTEIGHAInitialize())
        return nullptr;

    OGRDWGDataSource *poDS = new OGRDWGDataSource();

    if (!poDS->Open(OGRDWGGetServices(), poOpenInfo->pszFilename))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRDWG()                           */
/************************************************************************/

void RegisterOGRDWG()

{
    if (GDALGetDriverByName(DWG_DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRDWGDriverSetCommonMetadata(poDriver);

    poDriver->pfnOpen = OGRDWGDriverOpen;
    poDriver->pfnUnloadDriver = OGRDWGDriverUnload;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
