/******************************************************************************
 *
 * Project:  Carto Translator
 * Purpose:  Implements OGRCARTODriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_carto.h"
#include "ogrcartodrivercore.h"

// g++ -g -Wall -fPIC -shared -o ogr_CARTO.so -Iport -Igcore -Iogr
// -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/carto ogr/ogrsf_frmts/carto/*.c* -L.
// -lgdal -Iogr/ogrsf_frmts/geojson/libjson

extern "C" void RegisterOGRCarto();

/************************************************************************/
/*                           OGRCartoDriverOpen()                     */
/************************************************************************/

static GDALDataset *OGRCartoDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRCartoDriverIdentify(poOpenInfo))
        return nullptr;

    OGRCARTODataSource *poDS = new OGRCARTODataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, poOpenInfo->papszOpenOptions,
                    poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                      OGRCartoDriverCreate()                        */
/************************************************************************/

static GDALDataset *OGRCartoDriverCreate(const char *pszName,
                                         CPL_UNUSED int nBands,
                                         CPL_UNUSED int nXSize,
                                         CPL_UNUSED int nYSize,
                                         CPL_UNUSED GDALDataType eDT,
                                         CPL_UNUSED char **papszOptions)

{
    OGRCARTODataSource *poDS = new OGRCARTODataSource();

    if (!poDS->Open(pszName, nullptr, TRUE))
    {
        delete poDS;
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Carto driver doesn't support database creation.");
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         RegisterOGRCARTO()                         */
/************************************************************************/

void RegisterOGRCarto()

{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRCartoDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRCartoDriverOpen;
    poDriver->pfnCreate = OGRCartoDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
