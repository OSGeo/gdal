/******************************************************************************
 *
 * Project:  Parquet Translator
 * Purpose:  Implements OGRParquetDriver.
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Planet Labs
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

#include "ogrsf_frmts.h"
#include "gdal_priv.h"

#include "ogrparquetdrivercore.h"

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

template <size_t N> constexpr int constexpr_length(const char (&)[N])
{
    return static_cast<int>(N - 1);
}

int OGRParquetDriverIdentify(GDALOpenInfo *poOpenInfo)
{
#if defined(GDAL_USE_ARROWDATASET) || defined(PLUGIN_FILENAME)
    if (poOpenInfo->bIsDirectory)
    {
        // Might be a ParquetDataset
        return -1;
    }
#endif
    if (STARTS_WITH(poOpenInfo->pszFilename, "PARQUET:"))
        return TRUE;

    // See https://github.com/apache/parquet-format#file-format
    bool bRet = false;
    constexpr const char SIGNATURE[] = "PAR1";
    constexpr int SIGNATURE_SIZE = constexpr_length(SIGNATURE);
    static_assert(SIGNATURE_SIZE == 4, "SIGNATURE_SIZE == 4");
    constexpr int METADATASIZE_SIZE = 4;
    if (poOpenInfo->fpL != nullptr &&
        poOpenInfo->nHeaderBytes >=
            SIGNATURE_SIZE + METADATASIZE_SIZE + SIGNATURE_SIZE &&
        memcmp(poOpenInfo->pabyHeader, SIGNATURE, SIGNATURE_SIZE) == 0)
    {
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
        const auto nFileSize = VSIFTellL(poOpenInfo->fpL);
        VSIFSeekL(poOpenInfo->fpL,
                  nFileSize - (METADATASIZE_SIZE + SIGNATURE_SIZE), SEEK_SET);
        uint32_t nMetadataSize = 0;
        static_assert(sizeof(nMetadataSize) == METADATASIZE_SIZE,
                      "sizeof(nMetadataSize) == METADATASIZE_SIZE");
        VSIFReadL(&nMetadataSize, 1, sizeof(nMetadataSize), poOpenInfo->fpL);
        CPL_LSBPTR32(&nMetadataSize);
        unsigned char abyTrailingBytes[SIGNATURE_SIZE] = {0};
        VSIFReadL(&abyTrailingBytes[0], 1, SIGNATURE_SIZE, poOpenInfo->fpL);
        bRet = memcmp(abyTrailingBytes, SIGNATURE, SIGNATURE_SIZE) == 0 &&
               nMetadataSize < nFileSize;
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
    }
    return bRet;
}

/************************************************************************/
/*                OGRParquetDriverSetCommonMetadata()                   */
/************************************************************************/

void OGRParquetDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "(Geo)Parquet");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "parquet");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/parquet.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");

    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date Time DateTime "
        "Binary IntegerList Integer64List RealList StringList");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean Int16 Float32 JSON UUID");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision Nullable Comment "
                              "AlternativeName Domain");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='GEOM_POSSIBLE_NAMES' type='string' description='Comma "
        "separated list of possible names for geometry column(s).' "
        "default='geometry,wkb_geometry,wkt_geometry'/>"
        "  <Option name='CRS' type='string' "
        "description='Set/override CRS, typically defined as AUTH:CODE "
        "(e.g EPSG:4326), of geometry column(s)'/>"
        "</OpenOptionList>");

    poDriver->pfnIdentify = OGRParquetDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                  DeclareDeferredOGRParquetPlugin()                   */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRParquetPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    OGRParquetDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
