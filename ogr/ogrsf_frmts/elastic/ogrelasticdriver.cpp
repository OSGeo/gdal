/******************************************************************************
 *
 * Project:  Elasticsearch Translator
 * Purpose:
 * Author:
 *
 ******************************************************************************
 * Copyright (c) 2011, Adam Estrada
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
