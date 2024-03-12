/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMiraMonDriver class.
 * Author:   Abel Pau
 ******************************************************************************
 * Copyright (c) 2024, Xavier Pons
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

/****************************************************************************/
/*                    OGRMMDriverIdentify()                                 */
/****************************************************************************/

static int OGRMiraMonDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->fpL == nullptr || poOpenInfo->nHeaderBytes < 7)
        return FALSE;
    else if (EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "PNT") ||
             EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "ARC") ||
             EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "POL"))
    {
        if (!poOpenInfo->TryToIngest(7))
            return FALSE;

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
    if (!poOpenInfo->bStatOK)
        return nullptr;

    OGRMiraMonDataSource *poDS = new OGRMiraMonDataSource();

    if (!poDS->Open(poOpenInfo->pszFilename, nullptr, nullptr,
                    poOpenInfo->eAccess == GA_Update,
                    poOpenInfo->papszOpenOptions))
    {
        delete poDS;
        return nullptr;
    }

    /* Not sure when use that
    if (poDS != nullptr && poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "MiraMonVector driver does not support update.");
        delete poDS;
        poDS = nullptr;
    }*/

    return poDS;
}

/****************************************************************************/
/*                         OGRMiraMonDriverCreate()                              */
/****************************************************************************/

static GDALDataset *
OGRMiraMonDriverCreate(const char *pszName, CPL_UNUSED int /*nBands*/,
                       CPL_UNUSED int /*nXSize*/, CPL_UNUSED int /*nYSize*/,
                       CPL_UNUSED GDALDataType /*eDT*/, char **papszOptions)
{
    OGRMiraMonDataSource *poDS = new OGRMiraMonDataSource();

    if (poDS->Create(pszName, papszOptions))
        return poDS;

    delete poDS;
    return nullptr;
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
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "pol");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "pol arc pnt");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/miramon.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='Height' scope='vector' type='string' "
        "   description='Sets which of the possible heights is chosen: "
        "   the first, the highest or the lowest one.'>"
        "    <Value>First</Value>"
        "    <Value>Lower</Value>"
        "    <Value>Highest</Value>"
        "  </Option>"
        "  <Option name='iMultiRecord' scope='vector' type='string' "
        "   description='Sets which of the possible records is chosen: "
        "   0, 1, 2,... or the Last one. Use JSON when a serialized "
        "   JSON is wanted'>"
        "    <Value>0</Value>"
        "    <Value>1,...</Value>"
        "    <Value>Last</Value>"
        "    <Value>JSON</Value>"
        "  </Option>"
        "  <Option name='OpenMemoryRatio' scope='vector' type='float' "
        "   description='Ratio used to enhance certain aspects of memory"
        "   In some memory allocations, a block of 256 bytes is used."
        "   This parameter can be adjusted to achieve"
        "   OpenMemoryRatio*256."
        "   For example, OpenMemoryRatio=2 in powerful computers and"
        "   OpenMemoryRatio=0.5 in less powerful computers."
        "   By increasing this parameter, more memory will be required,"
        "   but there will be fewer read/write operations to the disk.'>"
        "    <Value>0.5</Value>"
        "    <Value>1</Value>"
        "    <Value>2</Value>"
        "  </Option>"
        "  <Option name='OpenLanguage' scope='vector' type='string' "
        "   description='If the layer to be opened is multilingual "
        "   (in fact the *.rel* file), this parameter sets the language "
        "   to be read.'>"
        "    <Value>ENG</Value>"
        "    <Value>CAT</Value>"
        "    <Value>SPA</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(
        GDAL_DS_LAYER_CREATIONOPTIONLIST,
        "<LayerCreationOptionList>"
        "  <Option name='Version' type='string' description='Version of the "
        "file."
        "V1.1 is a limited 32 bits for FID and for internal offsets. "
        "V2.0 is the 64 bits version, with pratically no limits for FID nor for"
        " internal offsets.' "
        "default='last_version'>"
        "<Value>V1.1</Value>"
        "<Value>V2.0</Value>"
        "<Value>last_version</Value>"
        "<Value>nullptr</Value>"
        "</Option>"
        "  <Option name='DBFEncoding' type='string' description='Encoding of "
        "the "
        ".dbf files."
        "MiraMon can write *.dbf* files in these two charsets.' "
        "default='ANSI'>"
        "<Value>UTF8</Value>"
        "<Value>ANSI</Value>"
        "<Value>last_version</Value>"
        "</Option>"
        "  <Option name='CreationMemoryRatio' scope='vector' type='float' "
        "   description='It is a ratio used to enhance certain aspects of "
        "memory. "
        "   In some memory allocations a block of 256 bytes is used. "
        "   This parameter can be adjusted to achieve nMemoryRatio*256. "
        "   By way of example, please use nMemoryRatio=2 in powerful computers "
        "and "
        "   nMemoryRatio=0.5 in less powerful computers. "
        "   By increasing this parameter, more memory will be required, "
        "   but there will be fewer read/write operations to the (network and) "
        "disk.'>"
        "    <Value>0.5</Value>"
        "    <Value>1</Value>"
        "    <Value>2</Value>"
        "  </Option>"
        "  <Option name='CreationLanguage' scope='vector' type='string' "
        "   description='If the layer to be opened is multilingual "
        "   (in fact the *.rel* file), this parameter sets the language "
        "    to be read.'>"
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
