/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMiraMonDriver class.
 * Author:   Abel Pau, a.pau@creaf.uab.cat
 *
 ******************************************************************************
 * Copyright (c) 2023,  MiraMon
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

#include "ogrmiramon.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                    OGRMMDriverIdentify()                            */
/************************************************************************/

static int OGRMMDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    const char *pszHeader =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if (poOpenInfo->nHeaderBytes && strstr(pszHeader, "@VGMT") != nullptr)
        return true;

    if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "PNT") ||
        EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "ARC") ||
        EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "POL"))
        return true;

    return false;
}

/************************************************************************/
/*                           OGRMMDriverOpen()                         */
/************************************************************************/

static GDALDataset *OGRMMDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRMMDriverIdentify(poOpenInfo))
        return nullptr;

    OGRMiraMonDataSource *poDS = new OGRMiraMonDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, nullptr, nullptr,
                    poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                         OGRMMDriverCreate()                         */
/************************************************************************/

static GDALDataset *
OGRMMDriverCreate(const char *pszName, CPL_UNUSED int nBands,
                   CPL_UNUSED int nXSize, CPL_UNUSED int nYSize,
                   CPL_UNUSED GDALDataType eDT, char **papszOptions)
{
    OGRMiraMonDataSource *poDS = new OGRMiraMonDataSource();

    if (poDS->Create(pszName, papszOptions))
        return poDS;

    delete poDS;
    return nullptr;
}

/************************************************************************/
/*                           RegisterOGRMM()                           */
/************************************************************************/

void RegisterOGRMiraMon()

{
    if (GDALGetDriverByName("OGR_MM") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("MM");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MiraMon Vectors (.pol, .arc, .pnt)");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "pol");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "pol arc pnt");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/miramon.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    //poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");
    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='Version' type='string' description='Version of the file."
        "V11 is a limited 32 bits for FID and internal offsets. "
        "V20 is the 64 bits version for FID and internal offsets.' default='last_version'>"
        "<Value>V11</Value>"
        "<Value>V20</Value>"
        "<Value>last_version</Value>"
        "<Value>NULL</Value>"
        "</Option>"
        "</LayerCreationOptionList>");

    poDriver->pfnOpen = OGRMMDriverOpen;
    poDriver->pfnIdentify = OGRMMDriverIdentify;
    poDriver->pfnCreate = OGRMMDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
