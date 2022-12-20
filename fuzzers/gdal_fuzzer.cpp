/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Fuzzer
 * Author:   Even Rouault, even.rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "gdal.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal_frmts.h"

#ifndef REGISTER_FUNC
#define REGISTER_FUNC GDALAllRegister
#endif

#ifndef GDAL_SKIP
#define GDAL_SKIP "CAD"
#endif

#ifndef EXTENSION
#define EXTENSION "bin"
#endif

#ifndef MEM_FILENAME
#define MEM_FILENAME "/vsimem/test"
#endif

#ifndef GDAL_FILENAME
#define GDAL_FILENAME MEM_FILENAME
#endif

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len);

int LLVMFuzzerInitialize(int * /*argc*/, char ***argv)
{
    const char *exe_path = (*argv)[0];
    if (CPLGetConfigOption("GDAL_DATA", nullptr) == nullptr)
    {
        CPLSetConfigOption("GDAL_DATA", CPLGetPath(exe_path));
    }
    CPLSetConfigOption("CPL_TMPDIR", "/tmp");
    CPLSetConfigOption("DISABLE_OPEN_REAL_NETCDF_FILES", "YES");
    // Disable PDF text rendering as fontconfig cannot access its config files
    CPLSetConfigOption("GDAL_PDF_RENDERING_OPTIONS", "RASTER,VECTOR");
    // to avoid timeout in WMS driver
    CPLSetConfigOption("GDAL_WMS_ABORT_CURL_REQUEST", "YES");
    CPLSetConfigOption("GDAL_HTTP_TIMEOUT", "1");
    CPLSetConfigOption("GDAL_HTTP_CONNECTTIMEOUT", "1");
    CPLSetConfigOption("GDAL_CACHEMAX", "1000");  // Limit to 1 GB
#ifdef GTIFF_USE_MMAP
    CPLSetConfigOption("GTIFF_USE_MMAP", "YES");
#endif

#ifdef GDAL_SKIP
    CPLSetConfigOption("GDAL_SKIP", GDAL_SKIP);
#endif
    REGISTER_FUNC();

    return 0;
}

static void ExploreAttributes(const GDALIHasAttribute *attributeHolder)
{
    const auto attributes = attributeHolder->GetAttributes();
    for (const auto &attribute : attributes)
    {
        attribute->ReadAsRaw();
    }

    attributeHolder->GetAttribute("i_do_not_exist");
}

static void ExploreArray(const std::shared_ptr<GDALMDArray> &poArray,
                         const char *pszDriverName)
{
    ExploreAttributes(poArray.get());

    poArray->GetFilename();
    poArray->GetStructuralInfo();
    poArray->GetUnit();
    poArray->GetSpatialRef();
    poArray->GetRawNoDataValue();
    poArray->GetOffset();
    poArray->GetScale();
    poArray->GetCoordinateVariables();

    const auto nDimCount = poArray->GetDimensionCount();
    bool bRead = true;
    constexpr size_t MAX_ALLOC = 1000 * 1000 * 1000U;
    if (pszDriverName && EQUAL(pszDriverName, "GRIB"))
    {
        const auto poDims = poArray->GetDimensions();
        if (nDimCount >= 2 &&
            poDims[nDimCount - 2]->GetSize() >
                MAX_ALLOC / sizeof(double) / poDims[nDimCount - 1]->GetSize())
        {
            bRead = false;
        }
    }
    else
    {
        const auto anBlockSize = poArray->GetBlockSize();
        size_t nBlockSize = poArray->GetDataType().GetSize();
        for (const auto nDimBlockSize : anBlockSize)
        {
            if (nDimBlockSize == 0)
            {
                break;
            }
            if (nBlockSize > MAX_ALLOC / nDimBlockSize)
            {
                bRead = false;
                break;
            }
            nBlockSize *= static_cast<size_t>(nDimBlockSize);
        }
    }

    if (bRead && poArray->GetDataType().GetClass() == GEDTC_NUMERIC)
    {
        std::vector<GUInt64> anArrayStartIdx(nDimCount);
        std::vector<size_t> anCount(nDimCount, 1);
        std::vector<GInt64> anArrayStep(nDimCount);
        std::vector<GPtrDiff_t> anBufferStride(nDimCount);
        std::vector<GByte> abyData(poArray->GetDataType().GetSize());
        poArray->Read(anArrayStartIdx.data(), anCount.data(),
                      anArrayStep.data(), anBufferStride.data(),
                      poArray->GetDataType(), &abyData[0]);
    }
}

static void ExploreGroup(const std::shared_ptr<GDALGroup> &poGroup,
                         const char *pszDriverName)
{
    ExploreAttributes(poGroup.get());

    const auto groupNames = poGroup->GetGroupNames();
    poGroup->OpenGroup("i_do_not_exist");
    for (const auto &name : groupNames)
    {
        auto poSubGroup = poGroup->OpenGroup(name);
        if (poSubGroup)
            ExploreGroup(poSubGroup, pszDriverName);
    }

    const auto arrayNames = poGroup->GetMDArrayNames();
    poGroup->OpenMDArray("i_do_not_exist");
    for (const auto &name : arrayNames)
    {
        auto poArray = poGroup->OpenMDArray(name);
        if (poArray)
        {
            ExploreArray(poArray, pszDriverName);
        }
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *buf, size_t len)
{
#ifdef USE_FILESYSTEM
    char szTempFilename[64];
    snprintf(szTempFilename, sizeof(szTempFilename), "/tmp/gdal_fuzzer_%d.%s",
             (int)getpid(), EXTENSION);
    VSILFILE *fp = VSIFOpenL(szTempFilename, "wb");
    if (!fp)
    {
        fprintf(stderr, "Cannot create %s\n", szTempFilename);
        return 1;
    }
    VSIFWriteL(buf, 1, len, fp);
#else
    VSILFILE *fp = VSIFileFromMemBuffer(
        MEM_FILENAME, reinterpret_cast<GByte *>(const_cast<uint8_t *>(buf)),
        len, FALSE);
#endif
    VSIFCloseL(fp);

    CPLPushErrorHandler(CPLQuietErrorHandler);
#ifdef USE_FILESYSTEM
    const char *pszGDALFilename = szTempFilename;
#else
    const char *pszGDALFilename = GDAL_FILENAME;
#endif
    GDALDatasetH hDS = GDALOpen(pszGDALFilename, GA_ReadOnly);
    if (hDS)
    {
        const int nTotalBands = GDALGetRasterCount(hDS);
        const int nBands = std::min(10, nTotalBands);
        bool bDoCheckSum = true;
        int nXSizeToRead = std::min(1024, GDALGetRasterXSize(hDS));
        int nYSizeToRead = std::min(1024, GDALGetRasterYSize(hDS));
        if (nBands > 0)
        {
            const char *pszInterleave =
                GDALGetMetadataItem(hDS, "INTERLEAVE", "IMAGE_STRUCTURE");
            int nSimultaneousBands =
                (pszInterleave && EQUAL(pszInterleave, "PIXEL")) ? nTotalBands
                                                                 : 1;

            // When using the RGBA interface in pixel-interleaved mode, take
            // into account the raw number of bands to compute memory
            // requirements
            if (nBands == 4 && nSimultaneousBands != 1 &&
                GDALGetDatasetDriver(hDS) == GDALGetDriverByName("GTiff"))
            {
                GDALDatasetH hRawDS = GDALOpen(
                    (CPLString("GTIFF_RAW:") + pszGDALFilename).c_str(),
                    GA_ReadOnly);
                if (hRawDS)
                {
                    nSimultaneousBands = GDALGetRasterCount(hRawDS);
                    GDALClose(hRawDS);
                }
            }

            // If we know that we will need to allocate a lot of memory
            // given the block size and interleaving mode, do not read
            // pixels to avoid out of memory conditions by ASAN
            GIntBig nPixels = 0;
            for (int i = 0; i < nBands; i++)
            {
                int nBXSize = 0, nBYSize = 0;
                GDALGetBlockSize(GDALGetRasterBand(hDS, i + 1), &nBXSize,
                                 &nBYSize);
                if (nBXSize == 0 || nBYSize == 0 || nBXSize > INT_MAX / nBYSize)
                {
                    bDoCheckSum = false;
                    break;
                }

                // Limit to 1000 blocks read for each band.
                while ((nXSizeToRead > 1 || nYSizeToRead > 1) &&
                       (DIV_ROUND_UP(nXSizeToRead, nBXSize) *
                            DIV_ROUND_UP(nYSizeToRead, nBYSize) >
                        1000))
                {
                    if (nXSizeToRead > 1 &&
                        DIV_ROUND_UP(nXSizeToRead, nBXSize) >
                            DIV_ROUND_UP(nYSizeToRead, nBYSize))
                        nXSizeToRead /= 2;
                    else if (nYSizeToRead > 1)
                        nYSizeToRead /= 2;
                    else
                        nXSizeToRead /= 2;
                }

                // Currently decoding of PIXARLOG compressed TIFF requires
                // a temporary buffer for the whole strip (if stripped) or
                // image (if tiled), so be careful for a
                // GTiffSplitBand
                // Could probably be fixed for the CHUNKY_STRIP_READ_SUPPORT
                // mode.
                // Workaround
                // https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=2606
                const char *pszCompress =
                    GDALGetMetadataItem(hDS, "COMPRESSION", "IMAGE_STRUCTURE");
                if (pszCompress != nullptr &&
                    ((nBYSize == 1 && GDALGetRasterYSize(hDS) > 1 &&
                      GDALGetMetadataItem(GDALGetRasterBand(hDS, 1),
                                          "BLOCK_OFFSET_0_1",
                                          "TIFF") == nullptr) ||
                     nBXSize != GDALGetRasterXSize(hDS)) &&
                    GDALGetDatasetDriver(hDS) == GDALGetDriverByName("GTiff"))
                {
                    if (EQUAL(pszCompress, "PIXARLOG") &&
                        GDALGetRasterYSize(hDS) >
                            (INT_MAX / 2) / static_cast<int>(sizeof(GUInt16)) /
                                nSimultaneousBands / GDALGetRasterXSize(hDS))
                    {
                        bDoCheckSum = false;
                    }
                    // https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=2874
                    else if (EQUAL(pszCompress, "SGILOG24") &&
                             GDALGetRasterYSize(hDS) >
                                 (INT_MAX / 2) /
                                     static_cast<int>(sizeof(GUInt32)) /
                                     nSimultaneousBands /
                                     GDALGetRasterXSize(hDS))
                    {
                        bDoCheckSum = false;
                    }
                    // https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=38051
                    else if (STARTS_WITH_CI(pszCompress, "LERC") &&
                             (GDALGetRasterYSize(hDS) >
                                  (INT_MAX / 2) / nSimultaneousBands /
                                      GDALGetRasterXSize(hDS) ||
                              static_cast<int64_t>(GDALGetRasterYSize(hDS)) *
                                          nSimultaneousBands *
                                          GDALGetRasterXSize(hDS) * 4 / 3 +
                                      100 >
                                  (INT_MAX / 2)))
                    {
                        bDoCheckSum = false;
                    }
                }

                GIntBig nNewPixels = static_cast<GIntBig>(nBXSize) * nBYSize;
                nNewPixels *= DIV_ROUND_UP(nXSizeToRead, nBXSize);
                nNewPixels *= DIV_ROUND_UP(nYSizeToRead, nBYSize);
                if (nNewPixels > nPixels)
                    nPixels = nNewPixels;
            }
            if (bDoCheckSum)
            {
                const GDALDataType eDT =
                    GDALGetRasterDataType(GDALGetRasterBand(hDS, 1));
                const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
                if (nPixels > 10 * 1024 * 1024 / nDTSize / nSimultaneousBands)
                {
                    bDoCheckSum = false;
                }
            }
        }
        if (bDoCheckSum)
        {
            for (int i = 0; i < nBands; i++)
            {
                GDALRasterBandH hBand = GDALGetRasterBand(hDS, i + 1);
                CPLDebug("FUZZER", "Checksum band %d: %d,%d,%d,%d", i + 1, 0, 0,
                         nXSizeToRead, nYSizeToRead);
                GDALChecksumImage(hBand, 0, 0, nXSizeToRead, nYSizeToRead);
            }
        }

        // Test other API
        GDALGetProjectionRef(hDS);
        double adfGeoTransform[6];
        GDALGetGeoTransform(hDS, adfGeoTransform);
        CSLDestroy(GDALGetFileList(hDS));
        GDALGetGCPCount(hDS);
        GDALGetGCPs(hDS);
        GDALGetGCPProjection(hDS);
        GDALGetMetadata(hDS, nullptr);
        GDALGetMetadataItem(hDS, "foo", nullptr);
        CSLDestroy(GDALGetFileList(hDS));
        if (nBands > 0)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hDS, 1);

            int bFound = FALSE;
            GDALGetRasterNoDataValue(hBand, &bFound);
            GDALGetRasterOffset(hBand, &bFound);
            GDALGetRasterScale(hBand, &bFound);
            GDALGetRasterUnitType(hBand);
            GDALGetMetadata(hBand, nullptr);
            GDALGetMetadataItem(hBand, "foo", nullptr);

            int nFlags = GDALGetMaskFlags(hBand);
            GDALRasterBandH hMaskBand = GDALGetMaskBand(hBand);
            GDALGetRasterBandXSize(hMaskBand);
            if (bDoCheckSum && nFlags == GMF_PER_DATASET)
            {
                int nBXSize = 0, nBYSize = 0;
                GDALGetBlockSize(hMaskBand, &nBXSize, &nBYSize);
                if (nBXSize == 0 || nBYSize == 0 ||
                    nBXSize > INT_MAX / 2 / nBYSize)
                {
                    // do nothing
                }
                else
                {
                    GDALChecksumImage(hMaskBand, 0, 0, nXSizeToRead,
                                      nYSizeToRead);
                }
            }

            int nOverviewCount = GDALGetOverviewCount(hBand);
            for (int i = 0; i < nOverviewCount; i++)
            {
                GDALGetOverview(hBand, i);
            }
        }

        GDALClose(hDS);
    }

    auto poDS = std::unique_ptr<GDALDataset>(
        GDALDataset::Open(pszGDALFilename, GDAL_OF_MULTIDIM_RASTER));
    if (poDS)
    {
        auto poDriver = poDS->GetDriver();
        const char *pszDriverName = nullptr;
        if (poDriver)
            pszDriverName = poDriver->GetDescription();
        auto poRootGroup = poDS->GetRootGroup();
        poDS.reset();
        if (poRootGroup)
            ExploreGroup(poRootGroup, pszDriverName);
    }

    CPLPopErrorHandler();
#ifdef USE_FILESYSTEM
    VSIUnlink(szTempFilename);
#else
    VSIUnlink(MEM_FILENAME);
#endif
    return 0;
}
