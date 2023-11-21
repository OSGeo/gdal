/******************************************************************************
 *
 * Project:  Feather Translator
 * Purpose:  Implements OGRFeatherDriver.
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

#include "ogrfeatherdrivercore.h"

/************************************************************************/
/*              OGRFeatherDriverIsArrowIPCStreamBasic()                 */
/************************************************************************/

static int OGRFeatherDriverIsArrowIPCStreamBasic(GDALOpenInfo *poOpenInfo)
{
    // WARNING: if making changes in that method, reflect them in
    // IsArrowIPCStream() in ogrfeatherdriver.cpp

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "ARROW_IPC_STREAM:"))
        return true;

    constexpr int CONTINUATION_SIZE = 4;  // 0xFFFFFFFF
    constexpr int METADATA_SIZE_SIZE = 4;

    // See
    // https://arrow.apache.org/docs/format/Columnar.html#encapsulated-message-format
    if (poOpenInfo->fpL != nullptr &&
        poOpenInfo->nHeaderBytes >= CONTINUATION_SIZE + METADATA_SIZE_SIZE &&
        memcmp(poOpenInfo->pabyHeader, "\xFF\xFF\xFF\xFF", CONTINUATION_SIZE) ==
            0)
    {
        const char *pszExt = CPLGetExtension(poOpenInfo->pszFilename);
        if (EQUAL(pszExt, "arrows") || EQUAL(pszExt, "ipc"))
            return true;

        const uint32_t nMetadataSize =
            CPL_LSBUINT32PTR(poOpenInfo->pabyHeader + CONTINUATION_SIZE);
        if (strcmp(poOpenInfo->pszFilename, "/vsistdin/") == 0)
        {
            // Padding after metadata and before body is not necessarily present
            // but the body must be at least 4 bytes
            constexpr int PADDING_MAX_SIZE = 4;

            // /vsistdin/ cannot seek back beyond first MB
            if (nMetadataSize >
                1024 * 1024 -
                    (CONTINUATION_SIZE + METADATA_SIZE_SIZE + PADDING_MAX_SIZE))
            {
                return false;
            }
            const int nSizeToRead = CONTINUATION_SIZE + METADATA_SIZE_SIZE +
                                    nMetadataSize + PADDING_MAX_SIZE;
            if (!poOpenInfo->TryToIngest(nSizeToRead))
            {
                return false;
            }

            return GDAL_IDENTIFY_UNKNOWN;
        }

        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
        const auto nFileSize = VSIFTellL(poOpenInfo->fpL);
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
        if (nMetadataSize >
            nFileSize - (CONTINUATION_SIZE + METADATA_SIZE_SIZE))
            return false;

        return GDAL_IDENTIFY_UNKNOWN;
    }
    return false;
}

/************************************************************************/
/*                    OGRFeatherDriverIsArrowFileFormat()               */
/************************************************************************/

template <size_t N> constexpr int constexpr_length(const char (&)[N])
{
    return static_cast<int>(N - 1);
}

bool OGRFeatherDriverIsArrowFileFormat(GDALOpenInfo *poOpenInfo)
{
    // See https://arrow.apache.org/docs/format/Columnar.html#ipc-file-format
    bool bRet = false;
    constexpr const char SIGNATURE[] = "ARROW1";
    constexpr int SIGNATURE_SIZE = constexpr_length(SIGNATURE);
    static_assert(SIGNATURE_SIZE == 6, "SIGNATURE_SIZE == 6");
    constexpr int SIGNATURE_PLUS_PADDING = SIGNATURE_SIZE + 2;
    constexpr int FOOTERSIZE_SIZE = 4;
    if (poOpenInfo->fpL != nullptr &&
        poOpenInfo->nHeaderBytes >=
            SIGNATURE_PLUS_PADDING + FOOTERSIZE_SIZE + SIGNATURE_SIZE &&
        memcmp(poOpenInfo->pabyHeader, SIGNATURE, SIGNATURE_SIZE) == 0)
    {
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_END);
        const auto nFileSize = VSIFTellL(poOpenInfo->fpL);
        VSIFSeekL(poOpenInfo->fpL,
                  nFileSize - (FOOTERSIZE_SIZE + SIGNATURE_SIZE), SEEK_SET);
        uint32_t nFooterSize = 0;
        static_assert(sizeof(nFooterSize) == FOOTERSIZE_SIZE,
                      "sizeof(nFooterSize) == FOOTERSIZE_SIZE");
        VSIFReadL(&nFooterSize, 1, sizeof(nFooterSize), poOpenInfo->fpL);
        CPL_LSBPTR32(&nFooterSize);
        unsigned char abyTrailingBytes[SIGNATURE_SIZE] = {0};
        VSIFReadL(&abyTrailingBytes[0], 1, SIGNATURE_SIZE, poOpenInfo->fpL);
        bRet = memcmp(abyTrailingBytes, SIGNATURE, SIGNATURE_SIZE) == 0 &&
               nFooterSize < nFileSize;
        VSIFSeekL(poOpenInfo->fpL, 0, SEEK_SET);
    }
    return bRet;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int OGRFeatherDriverIdentify(GDALOpenInfo *poOpenInfo)
{
    int ret = OGRFeatherDriverIsArrowIPCStreamBasic(poOpenInfo);
    if (ret == GDAL_IDENTIFY_TRUE || ret == GDAL_IDENTIFY_UNKNOWN)
        return ret;
    return OGRFeatherDriverIsArrowFileFormat(poOpenInfo);
}

/************************************************************************/
/*                OGRFeatherDriverSetCommonMetadata()                   */
/************************************************************************/

void OGRFeatherDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "(Geo)Arrow IPC File Format / Stream");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "arrow feather arrows ipc");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/vector/feather.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MEASURED_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date Time DateTime "
        "Binary IntegerList Integer64List RealList StringList");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONFIELDDATASUBTYPES,
                              "Boolean Int16 Float32 JSON UUID");
    poDriver->SetMetadataItem(GDAL_DMD_CREATION_FIELD_DEFN_FLAGS,
                              "WidthPrecision Nullable "
                              "Comment AlternativeName Domain");

    poDriver->pfnIdentify = OGRFeatherDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
}

/************************************************************************/
/*                  DeclareDeferredOGRArrowPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRArrowPlugin()
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
    OGRFeatherDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
