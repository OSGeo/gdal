/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMiraMonDriver class.
 * Author:   Abel Pau
 ******************************************************************************
 * Copyright (c) 2024, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrmiramon.h"

#include <cmath>

bool MM_IsNANDouble(double x)
{
    return std::isnan(x);
}

bool MM_IsDoubleInfinite(double x)
{
    return std::isinf(x);
}

/****************************************************************************/
/*                    OGRMMDriverIdentify()                                 */
/****************************************************************************/

static int OGRMiraMonDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 7)
        return FALSE;
    else if (poOpenInfo->IsExtensionEqualToCI("PNT") ||
             poOpenInfo->IsExtensionEqualToCI("ARC") ||
             poOpenInfo->IsExtensionEqualToCI("POL"))
    {
        // Format
        if ((poOpenInfo->pabyHeader[0] == 'P' &&
             poOpenInfo->pabyHeader[1] == 'N' &&
             poOpenInfo->pabyHeader[2] == 'T') ||
            (poOpenInfo->pabyHeader[0] == 'A' &&
             poOpenInfo->pabyHeader[1] == 'R' &&
             poOpenInfo->pabyHeader[2] == 'C') ||
            (poOpenInfo->pabyHeader[0] == 'P' &&
             poOpenInfo->pabyHeader[1] == 'O' &&
             poOpenInfo->pabyHeader[2] == 'L'))
        {
            // Version 1.1 or 2.0
            if ((poOpenInfo->pabyHeader[3] == ' ' &&
                 poOpenInfo->pabyHeader[4] == '1' &&
                 poOpenInfo->pabyHeader[5] == '.' &&
                 poOpenInfo->pabyHeader[6] == '1') ||
                (poOpenInfo->pabyHeader[3] == ' ' &&
                 poOpenInfo->pabyHeader[4] == '2' &&
                 poOpenInfo->pabyHeader[5] == '.' &&
                 poOpenInfo->pabyHeader[6] == '0'))
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

/****************************************************************************/
/*                           OGRMiraMonDriverOpen()                         */
/****************************************************************************/

static GDALDataset *OGRMiraMonDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (OGRMiraMonDriverIdentify(poOpenInfo) == FALSE)
        return nullptr;

    auto poDS = std::make_unique<OGRMiraMonDataSource>();
    if (!poDS->Open(poOpenInfo->pszFilename, nullptr, nullptr,
                    poOpenInfo->papszOpenOptions))
    {
        poDS.reset();
    }

    if (poDS && poOpenInfo->eAccess == GA_Update)
    {
        GDALDataset::ReportUpdateNotSupportedByDriver("MiraMonVector");
        return nullptr;
    }

    return poDS.release();
}

/****************************************************************************/
/*                         OGRMiraMonDriverCreate()                              */
/****************************************************************************/

static GDALDataset *
OGRMiraMonDriverCreate(const char *pszName, CPL_UNUSED int /*nBands*/,
                       CPL_UNUSED int /*nXSize*/, CPL_UNUSED int /*nYSize*/,
                       CPL_UNUSED GDALDataType /*eDT*/, char **papszOptions)
{
    auto poDS = std::make_unique<OGRMiraMonDataSource>();

    if (!poDS->Create(pszName, papszOptions))
    {
        poDS.reset();
    }

    return poDS.release();
}

/****************************************************************************/
/*                           RegisterOGRMM()                                */
/****************************************************************************/

void RegisterOGRMiraMon()

{
    if (GDALGetDriverByName("MiraMonVector") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("MiraMonVector");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "MiraMon Vectors (.pol, .arc, .pnt)");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "pol arc pnt");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/miramon.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='Height' scope='vector' type='string-select' "
        "   description='Sets which of the possible heights is chosen: "
        "the first, the highest or the lowest one.'>"
        "    <Value>First</Value>"
        "    <Value>Lowest</Value>"
        "    <Value>Highest</Value>"
        "  </Option>"
        "  <Option name='MultiRecordIndex' scope='vector' type='string' "
        "   description='Sets which of the possible records is chosen: "
        "0, 1, 2,... or the Last one. Use JSON when a serialized "
        "JSON is wanted'>"
        "  </Option>"
        "  <Option name='OpenLanguage' scope='vector' type='string-select' "
        "   description='If the layer to be opened is multilingual "
        "(in fact the *.rel* file), this parameter sets the language "
        "to be read.'>"
        "    <Value>ENG</Value>"
        "    <Value>CAT</Value>"
        "    <Value>SPA</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='Version' type='string-select' description='Version of "
        "the file. "
        "V1.1 is a limited 32 bits for FID and for internal offsets. "
        "V2.0 is the 64 bits version, with practically no limits for FID nor "
        "for internal offsets.' "
        "default='last_version'>"
        "<Value>V1.1</Value>"
        "<Value>V2.0</Value>"
        "<Value>last_version</Value>"
        "</Option>"
        "  <Option name='DBFEncoding' type='string-select' "
        "description='Encoding of "
        "the "
        ".dbf files."
        "MiraMon can write *.dbf* files in these two charsets.' "
        "default='ANSI'>"
        "<Value>UTF8</Value>"
        "<Value>ANSI</Value>"
        "</Option>"
        "  <Option name='CreationLanguage' scope='vector' type='string-select' "
        "   description='If the layer to be opened is multilingual "
        "(in fact the *.rel* file), this parameter sets the language "
        "to be read.'>"
        "    <Value>ENG</Value>"
        "    <Value>CAT</Value>"
        "    <Value>SPA</Value>"
        "  </Option>"
        "</LayerCreationOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date Time "
        "Binary IntegerList Integer64List RealList StringList");
    poDriver->pfnOpen = OGRMiraMonDriverOpen;
    poDriver->pfnIdentify = OGRMiraMonDriverIdentify;
    poDriver->pfnCreate = OGRMiraMonDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
