/******************************************************************************
 *
 * Project:  XLSX Translator
 * Purpose:  Implements OGRXLSXDriver.
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

#include "ogr_xlsx.h"
#include "cpl_conv.h"

extern "C" void RegisterOGRXLSX();

using namespace OGRXLSX;

// g++ -DHAVE_EXPAT -g -Wall -fPIC ogr/ogrsf_frmts/xlsx/*.cpp -shared -o
// ogr_XLSX.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mem
// -Iogr/ogrsf_frmts/xlsx -L. -lgdal

static const char XLSX_MIMETYPE[] =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml";

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

static int OGRXLSXDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->fpL == nullptr &&
        STARTS_WITH_CI(poOpenInfo->pszFilename, "XLSX:"))
    {
        return TRUE;
    }

    if (STARTS_WITH(poOpenInfo->pszFilename, "/vsizip/") ||
        STARTS_WITH(poOpenInfo->pszFilename, "/vsitar/"))
    {
        const char *pszExt = CPLGetExtension(poOpenInfo->pszFilename);
        return EQUAL(pszExt, "XLSX") || EQUAL(pszExt, "XLSM") ||
               EQUAL(pszExt, "XLSX}") || EQUAL(pszExt, "XLSM}");
    }

    if (poOpenInfo->nHeaderBytes > 30 &&
        memcmp(poOpenInfo->pabyHeader, "PK\x03\x04", 4) == 0)
    {
        // Fetch the first filename in the zip
        const int nFilenameLength =
            CPL_LSBUINT16PTR(poOpenInfo->pabyHeader + 26);
        if (30 + nFilenameLength > poOpenInfo->nHeaderBytes)
            return FALSE;
        const std::string osFilename(
            reinterpret_cast<const char *>(poOpenInfo->pabyHeader) + 30,
            nFilenameLength);
        if (STARTS_WITH(osFilename.c_str(), "xl/") ||
            STARTS_WITH(osFilename.c_str(), "_rels/") ||
            STARTS_WITH(osFilename.c_str(), "docProps/") ||
            osFilename == "[Content_Types].xml")
        {
            return TRUE;
        }
        const char *pszExt = CPLGetExtension(poOpenInfo->pszFilename);
        if (EQUAL(pszExt, "XLSX") || EQUAL(pszExt, "XLSM"))
        {
            CPLDebug(
                "XLSX",
                "Identify() failed to recognize first filename in zip (%s), "
                "but fallback to extension matching",
                osFilename.c_str());
            return TRUE;
        }
    }
    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRXLSXDriverOpen(GDALOpenInfo *poOpenInfo)

{
    if (!OGRXLSXDriverIdentify(poOpenInfo))
        return nullptr;

    const char *pszFilename = poOpenInfo->pszFilename;
    if (poOpenInfo->fpL == nullptr && STARTS_WITH_CI(pszFilename, "XLSX:"))
    {
        pszFilename += strlen("XLSX:");
    }
    const bool bIsVsiZipOrTarPrefixed = STARTS_WITH(pszFilename, "/vsizip/") ||
                                        STARTS_WITH(pszFilename, "/vsitar/");
    if (bIsVsiZipOrTarPrefixed)
    {
        if (poOpenInfo->eAccess != GA_ReadOnly)
            return nullptr;
    }

    std::string osPrefixedFilename;
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

    CPLString osTmpFilename;
    osTmpFilename =
        CPLSPrintf("%s/[Content_Types].xml", osPrefixedFilename.c_str());
    VSILFILE *fpContent = VSIFOpenL(osTmpFilename, "rb");
    if (fpContent == nullptr)
        return nullptr;

    char szBuffer[2048];
    int nRead = (int)VSIFReadL(szBuffer, 1, sizeof(szBuffer) - 1, fpContent);
    szBuffer[nRead] = 0;

    VSIFCloseL(fpContent);

    if (strstr(szBuffer, XLSX_MIMETYPE) == nullptr)
        return nullptr;

    osTmpFilename =
        CPLSPrintf("%s/xl/workbook.xml", osPrefixedFilename.c_str());
    VSILFILE *fpWorkbook = VSIFOpenL(osTmpFilename, "rb");
    if (fpWorkbook == nullptr)
        return nullptr;

    osTmpFilename =
        CPLSPrintf("%s/xl/_rels/workbook.xml.rels", osPrefixedFilename.c_str());
    VSILFILE *fpWorkbookRels = VSIFOpenL(osTmpFilename, "rb");
    if (fpWorkbookRels == nullptr)
    {
        VSIFCloseL(fpWorkbook);
        return nullptr;
    }

    osTmpFilename =
        CPLSPrintf("%s/xl/sharedStrings.xml", osPrefixedFilename.c_str());
    VSILFILE *fpSharedStrings = VSIFOpenL(osTmpFilename, "rb");
    osTmpFilename = CPLSPrintf("%s/xl/styles.xml", osPrefixedFilename.c_str());
    VSILFILE *fpStyles = VSIFOpenL(osTmpFilename, "rb");

    OGRXLSXDataSource *poDS =
        new OGRXLSXDataSource(poOpenInfo->papszOpenOptions);

    if (!poDS->Open(pszFilename, osPrefixedFilename.c_str(), fpWorkbook,
                    fpWorkbookRels, fpSharedStrings, fpStyles,
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
/*                       OGRXLSXDriverCreate()                          */
/************************************************************************/

static GDALDataset *OGRXLSXDriverCreate(const char *pszName, int /* nXSize */,
                                        int /* nYSize */, int /* nBands */,
                                        GDALDataType /* eDT */,
                                        char **papszOptions)

{
    if (!EQUAL(CPLGetExtension(pszName), "XLSX"))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "File extension should be XLSX");
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
    OGRXLSXDataSource *poDS = new OGRXLSXDataSource(nullptr);

    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterOGRXLSX()                           */
/************************************************************************/

void RegisterOGRXLSX()

{
    if (GDALGetDriverByName("XLSX") != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription("XLSX");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_DELETE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_REORDER_FIELDS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_ALTER_FIELD_DEFN_FLAGS, "Name Type");

    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "MS Office Open XML spreadsheet");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "xlsx xlsm");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/vector/xlsx.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES,
                              "Integer Integer64 Real String Date DateTime "
                              "Time");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES, "Boolean");
    poDriver->SetMetadataItem(GDAL_DCAP_NONSPATIAL, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CURVE_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
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

    poDriver->pfnIdentify = OGRXLSXDriverIdentify;
    poDriver->pfnOpen = OGRXLSXDriverOpen;
    poDriver->pfnCreate = OGRXLSXDriverCreate;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
