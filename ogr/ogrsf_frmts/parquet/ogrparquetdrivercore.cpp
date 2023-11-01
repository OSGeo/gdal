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
/*                  DeclareDeferredOGRParquetPlugin()                   */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredOGRParquetPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    GDALPluginDriverFeatures oFeatures;
    oFeatures.pfnIdentify = OGRParquetDriverIdentify;
    oFeatures.pszLongName = LONG_NAME;
    oFeatures.pszExtensions = EXTENSIONS;
    oFeatures.pszOpenOptionList = OPENOPTIONLIST;
    oFeatures.bHasVectorCapabilities = true;
    oFeatures.bHasCreate = true;
    GetGDALDriverManager()->DeclareDeferredPluginDriver(
        DRIVER_NAME, PLUGIN_FILENAME, oFeatures);
}
#endif
