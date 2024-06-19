/******************************************************************************
 *
 * Project:  ODS Translator
 * Purpose:  Implements OGRODSDriver.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_conv.h"
#include "ogr_ods.h"
#include "ogrsf_frmts.h"

using namespace OGRODS;

// g++ -DHAVE_EXPAT -g -Wall -fPIC ogr/ogrsf_frmts/ods/*.cpp -shared
// -o ogr_ODS.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts
// -Iogr/ogrsf_frmts/mem -Iogr/ogrsf_frmts/ods -L. -lgdal

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int OGRODSDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->fpL == nullptr &&
        STARTS_WITH_CI(poOpenInfo->pszFilename, "ODS:"))
    {
        return TRUE;
    }

    if (EQUAL(CPLGetFilename(poOpenInfo->pszFilename), "content.xml"))
    {
        return poOpenInfo->nHeaderBytes != 0 &&
               strstr(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                      "<office:document-content") != nullptr;
    }

    const char *pszExt = CPLGetExtension(poOpenInfo->pszFilename);
    if (!EQUAL(pszExt, "ODS") && !EQUAL(pszExt, "ODS}"))
        return FALSE;

    if (STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") ||
        STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/"))
        return TRUE;

    return poOpenInfo->nHeaderBytes > 4 &&
           memcmp(poOpenInfo->pabyHeader, "PK\x03\x04", 4) == 0;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRODSDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRODSDriverIdentify(poOpenInfo))
        return nullptr;

    const char *pszFilename = poOpenInfo->pszFilename;
    const bool bIsODSPrefixed =
        poOpenInfo->fpL == nullptr && STARTS_WITH_CI(pszFilename, "ODS:");
    const bool bIsVsiZipOrTarPrefixed = STARTS_WITH(pszFilename, "/vsizip/") ||
                                        STARTS_WITH(pszFilename, "/vsitar/");
    if (bIsVsiZipOrTarPrefixed)
    {
        if (poOpenInfo->eAccess != GA_ReadOnly)
            return nullptr;
    }

    bool bIsZIP = false;
    if (bIsODSPrefixed)
    {
        pszFilename += strlen("ODS:");
        if (!bIsVsiZipOrTarPrefixed)
        {
            VSILFILE *fp = VSIFOpenL(pszFilename, "rb");
            if (fp == nullptr)
                return nullptr;
            GByte abyHeader[4] = {0};
            VSIFReadL(abyHeader, 1, 4, fp);
            VSIFCloseL(fp);
            bIsZIP = memcmp(abyHeader, "PK\x03\x04", 4) == 0;
        }
    }
    else
    {
        bIsZIP = true;
    }

    std::string osPrefixedFilename;
    if (bIsZIP)
    {
        if (!bIsVsiZipOrTarPrefixed)
        {
            osPrefixedFilename = "/vsizip/{";
            osPrefixedFilename += pszFilename;
            osPrefixedFilename += "}";
        }
        else
        {
            osPrefixedFilename = pszFilename;
        }
    }

    CPLString osContentFilename(pszFilename);
    if (bIsZIP)
    {
        osContentFilename.Printf("%s/content.xml", osPrefixedFilename.c_str());
    }
    else if (poOpenInfo->eAccess ==
             GA_Update) /* We cannot update the xml file, only the .ods */
    {
        return nullptr;
    }

    VSILFILE *fpContent = VSIFOpenL(osContentFilename, "rb");
    if (fpContent == nullptr)
        return nullptr;

    char szBuffer[1024];
    int nRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fpContent);
    szBuffer[nRead] = 0;

    if (strstr(szBuffer, "<office:document-content") == nullptr)
    {
        VSIFCloseL(fpContent);
        return nullptr;
    }

    /* We could also check that there's a <office:spreadsheet>, but it might be
     * further */
    /* in the XML due to styles, etc... */

    VSILFILE *fpSettings = nullptr;
    if (bIsZIP)
    {
        CPLString osTmpFilename(
            CPLSPrintf("%s/settings.xml", osPrefixedFilename.c_str()));
        fpSettings = VSIFOpenL(osTmpFilename, "rb");
    }

    OGRODSDataSource *poDS = new OGRODSDataSource(poOpenInfo->papszOpenOptions);

    if (!poDS->Open(pszFilename, fpContent, fpSettings,
                    poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        poDS = nullptr;
    }
    else
    {
        poDS->SetDescription(poOpenInfo->pszFilename);
    }

    return poDS;
}

/************************************************************************/
/*                         OGRODSDriverCreate()                         */
/************************************************************************/

static GDALDataset *OGRODSDriverCreate(const char *pszName, int /* nXSize */,
                                       int /* nYSize */, int /* nBands */,
                                       GDALDataType /* eDT */,
                                       char **papszOptions)

{
    if (!EQUAL(CPLGetExtension(pszName), "ODS"))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "File extension should be ODS");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      First, ensure there isn't any such file yet.                    */
    /* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if (VSIStatL(pszName, &sStatBuf) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "It seems a file system object called '%s' already exists.",
                 pszName);

        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to create datasource.                                       */
    /* -------------------------------------------------------------------- */
    OGRODSDataSource *poDS = new OGRODSDataSource(nullptr);

    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/************************************************************************/
/*                           RegisterOGRODS()                           */
/************************************************************************/

void RegisterOGRODS()

{
    if (GDALGetDriverByName("ODS") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("ODS");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Open Document/ LibreOffice / "
                                                 "OpenOffice Spreadsheet");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "ods");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/ods.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime "
                              "Time Binary");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");
    poDriver->SetMetadataItem(GDAL_DCAP_NONSPATIAL, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_REORDER_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS, "Name Type");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='FIELD_TYPES' type='string-select' "
        "description='If set to STRING, all fields will be of type String. "
        "Otherwise the driver autodetects the field type from field content.' "
        "default='AUTO'>"
        "    <Value>AUTO</Value>"
        "    <Value>STRING</Value>"
        "  </Option>"
        "  <Option name='HEADERS' type='string-select' "
        "description='Defines if the first line should be considered as "
        "containing the name of the fields.' "
        "default='AUTO'>"
        "    <Value>AUTO</Value>"
        "    <Value>FORCE</Value>"
        "    <Value>DISABLE</Value>"
        "  </Option>"
        "</OpenOptionList>");

    poDriver->pfnIdentify = OGRODSDriverIdentify;
    poDriver->pfnOpen = OGRODSDriverOpen;
    poDriver->pfnCreate = OGRODSDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
