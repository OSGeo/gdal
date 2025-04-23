/******************************************************************************
 *
 * Project:  Elasticsearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_elastic.h"
#include "cpl_conv.h"
#include "ogrelasticdrivercore.h"

/************************************************************************/
/*                  OGRElasticsearchDriverOpen()                        */
/************************************************************************/

static GDALDataset *OGRElasticsearchDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRElasticsearchDriverIdentify(poOpenInfo))
        return nullptr;

    OGRElasticDataSource *poDS = new OGRElasticDataSource();
    if (!poDS->Open(poOpenInfo))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                     OGRElasticsearchDriverCreate()                   */
/************************************************************************/
static GDALDataset *
OGRElasticsearchDriverCreate(const char *pszName, CPL_UNUSED int nXSize,
                             CPL_UNUSED int nYSize, CPL_UNUSED int nBands,
                             CPL_UNUSED GDALDataType eDT, char **papszOptions)
{
    OGRElasticDataSource *poDS = new OGRElasticDataSource();

    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        poDS = nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                          RegisterOGRElastic()                        */
/************************************************************************/

void RegisterOGRElastic()
{
    if (!GDAL_CHECK_VERSION("OGR/Elastic Search driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    OGRElasticsearchDriverSetCommonMetadata(poDriver);
    poDriver->pfnOpen = OGRElasticsearchDriverOpen;
    poDriver->pfnCreate = OGRElasticsearchDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
