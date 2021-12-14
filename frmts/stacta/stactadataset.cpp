/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  STACTA (Spatio-Temporal Asset Catalog Tiled Assets) driver
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_json.h"
#include "cpl_mem_cache.h"
#include "cpl_string.h"
#include "gdal_pam.h"
#include "gdal_utils.h"
#include "memdataset.h"
#include "tilematrixset.hpp"
#include "stactadataset.h"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <vector>

extern "C" void GDALRegister_STACTA();

CPL_CVSID("$Id$")

// Implements a driver for
// https://github.com/stac-extensions/tiled-assets

/************************************************************************/
/*                         STACTARasterBand()                           */
/************************************************************************/

STACTARasterBand::STACTARasterBand(STACTADataset* poDSIn, int nBandIn,
                                   GDALRasterBand* poProtoBand):
    m_eColorInterp(poProtoBand->GetColorInterpretation())
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = poProtoBand->GetRasterDataType();
    poProtoBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
    nRasterXSize = poDSIn->GetRasterXSize();
    nRasterYSize = poDSIn->GetRasterYSize();
    m_dfNoData = poProtoBand->GetNoDataValue(&m_bHasNoDataValue);
}

/************************************************************************/
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr STACTARasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage)
{
    auto poGDS = cpl::down_cast<STACTADataset*>(poDS);
    return poGDS->m_poDS->GetRasterBand(nBand)->ReadBlock(nBlockXOff, nBlockYOff, pImage);
}

/************************************************************************/
/*                           IRasterIO()                                */
/************************************************************************/

CPLErr STACTARasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void* pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    GSpacing nPixelSpace, GSpacing nLineSpace,
                                    GDALRasterIOExtraArg* psExtraArg )
{
    auto poGDS = cpl::down_cast<STACTADataset*>(poDS);
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && poGDS->m_apoOverviewDS.size() >= 1 && eRWFlag == GF_Read )
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                    nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nPixelSpace, nLineSpace,
                                    psExtraArg,
                                    &bTried );
        if( bTried )
            return eErr;
    }

    return poGDS->m_poDS->GetRasterBand(nBand)->RasterIO(eRWFlag,
                                                  nXOff, nYOff, nXSize, nYSize,
                                                  pData, nBufXSize, nBufYSize,
                                                  eBufType,
                                                  nPixelSpace, nLineSpace,
                                                  psExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr STACTADataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg)
{
    if( (nBufXSize < nXSize || nBufYSize < nYSize)
        && m_apoOverviewDS.size() >= 1 && eRWFlag == GF_Read )
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                    nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace,
                                    nBandSpace,
                                    psExtraArg,
                                    &bTried );
        if( bTried )
            return eErr;
    }

    return m_poDS->RasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                  pData, nBufXSize, nBufYSize,
                                  eBufType, nBandCount, panBandMap,
                                  nPixelSpace, nLineSpace, nBandSpace,
                                  psExtraArg );
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int STACTARasterBand::GetOverviewCount()
{
    STACTADataset* poGDS = cpl::down_cast<STACTADataset*>(poDS);
    return static_cast<int>(poGDS->m_apoOverviewDS.size());
}

/************************************************************************/
/*                             GetOverview()                            */
/************************************************************************/

GDALRasterBand* STACTARasterBand::GetOverview(int nIdx)
{
    STACTADataset* poGDS = cpl::down_cast<STACTADataset*>(poDS);
    if( nIdx < 0 || nIdx >= GetOverviewCount() )
        return nullptr;
    return poGDS->m_apoOverviewDS[nIdx]->GetRasterBand(nBand);
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double STACTARasterBand::GetNoDataValue(int* pbHasNoData)
{
    if( pbHasNoData )
        *pbHasNoData = m_bHasNoDataValue;
    return m_dfNoData;
}

/************************************************************************/
/*                        STACTARawRasterBand()                         */
/************************************************************************/

STACTARawRasterBand::STACTARawRasterBand(STACTARawDataset* poDSIn, int nBandIn,
                                         GDALRasterBand* poProtoBand):
    m_eColorInterp(poProtoBand->GetColorInterpretation())
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = poProtoBand->GetRasterDataType();
    nBlockXSize = 256;
    nBlockYSize = 256;
    int nProtoBlockXSize;
    int nProtoBlockYSize;
    // Use tile block size if it divides the metatile dimension.
    poProtoBand->GetBlockSize(&nProtoBlockXSize, &nProtoBlockYSize);
    if( (poDSIn->m_nMetaTileWidth % nProtoBlockXSize) == 0 &&
        (poDSIn->m_nMetaTileHeight % nProtoBlockYSize) == 0 )
    {
        nBlockXSize = nProtoBlockXSize;
        nBlockYSize = nProtoBlockYSize;
    }
    nRasterXSize = poDSIn->GetRasterXSize();
    nRasterYSize = poDSIn->GetRasterYSize();
    m_dfNoData = poProtoBand->GetNoDataValue(&m_bHasNoDataValue);
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double STACTARawRasterBand::GetNoDataValue(int* pbHasNoData)
{
    if( pbHasNoData )
        *pbHasNoData = m_bHasNoDataValue;
    return m_dfNoData;
}

/************************************************************************/
/*                           IReadBlock()                               */
/************************************************************************/

CPLErr STACTARawRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage)
{
    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nXSize = std::min(nBlockXSize, nRasterXSize - nXOff);
    const int nYSize = std::min(nBlockYSize, nRasterYSize - nYOff);
    GDALRasterIOExtraArg sExtraArgs;
    INIT_RASTERIO_EXTRA_ARG(sExtraArgs);
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    return IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                     pImage, nBlockXSize, nBlockYSize,
                     eDataType, nDTSize, nDTSize * nBlockXSize,
                     &sExtraArgs);
}

/************************************************************************/
/*                           IRasterIO()                                */
/************************************************************************/

CPLErr STACTARawRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void* pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    GSpacing nPixelSpace, GSpacing nLineSpace,
                                    GDALRasterIOExtraArg* psExtraArg )
{
    CPLDebugOnly("STACTA", "Band %d RasterIO: %d,%d,%d,%d->%d,%d",
                 nBand, nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize);
    auto poGDS = cpl::down_cast<STACTARawDataset*>(poDS);

    const int nKernelRadius = 3; // up to 3 for Lanczos
    const int nRadiusX = nKernelRadius * static_cast<int>(std::ceil(nXSize / nBufXSize));
    const int nRadiusY = nKernelRadius * static_cast<int>(std::ceil(nYSize / nBufYSize));
    const int nXOffMod = std::max(0, nXOff - nRadiusX);
    const int nYOffMod = std::max(0, nYOff - nRadiusY);
    const int nXSizeMod = static_cast<int>(std::min(
        nXOff + nXSize + static_cast<GIntBig>(nRadiusX),
        static_cast<GIntBig>(nRasterXSize))) - nXOffMod;
    const int nYSizeMod = static_cast<int>(std::min(
        nYOff + nYSize + static_cast<GIntBig>(nRadiusY),
        static_cast<GIntBig>(nRasterYSize))) - nYOffMod;

    const bool bRequestFitsInSingleMetaTile =
        nXOffMod / poGDS->m_nMetaTileWidth == (nXOffMod + nXSizeMod - 1) / poGDS->m_nMetaTileWidth &&
        nYOffMod / poGDS->m_nMetaTileHeight == (nYOffMod + nYSizeMod - 1) / poGDS->m_nMetaTileHeight;

    if( eRWFlag != GF_Read ||
        ((nXSize != nBufXSize || nYSize != nBufYSize) && !bRequestFitsInSingleMetaTile) )
    {
        if( !(eRWFlag == GF_Read && nXSizeMod <= 4096 && nYSizeMod <= 4096) )
        {
            // If not reading at nominal resolution, fallback to default block
            // reading
            return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                            pData, nBufXSize, nBufYSize,
                                            eBufType, nPixelSpace, nLineSpace,
                                            psExtraArg);
        }
    }

    // Use optimized dataset level RasterIO()
    return poGDS->IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                            nBufXSize, nBufYSize, eBufType, 1, &nBand,
                            nPixelSpace, nLineSpace, 0, psExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr STACTARawDataset::IRasterIO( GDALRWFlag eRWFlag,
                                    int nXOff, int nYOff, int nXSize, int nYSize,
                                    void * pData, int nBufXSize, int nBufYSize,
                                    GDALDataType eBufType,
                                    int nBandCount, int *panBandMap,
                                    GSpacing nPixelSpace, GSpacing nLineSpace,
                                    GSpacing nBandSpace,
                                    GDALRasterIOExtraArg* psExtraArg)
{
    CPLDebugOnly("STACTA", "Dataset RasterIO: %d,%d,%d,%d->%d,%d",
                 nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize);
    const int nMinBlockX = nXOff / m_nMetaTileWidth;
    const int nMaxBlockX = (nXOff + nXSize - 1) / m_nMetaTileWidth;
    const int nMinBlockY = nYOff / m_nMetaTileHeight;
    const int nMaxBlockY = (nYOff + nYSize - 1) / m_nMetaTileHeight;

    const int nKernelRadius = 3; // up to 3 for Lanczos
    const int nRadiusX = nKernelRadius * static_cast<int>(std::ceil(nXSize / nBufXSize));
    const int nRadiusY = nKernelRadius * static_cast<int>(std::ceil(nYSize / nBufYSize));
    const int nXOffMod = std::max(0, nXOff - nRadiusX);
    const int nYOffMod = std::max(0, nYOff - nRadiusY);
    const int nXSizeMod = static_cast<int>(std::min(
        nXOff + nXSize + static_cast<GIntBig>(nRadiusX),
        static_cast<GIntBig>(nRasterXSize))) - nXOffMod;
    const int nYSizeMod = static_cast<int>(std::min(
        nYOff + nYSize + static_cast<GIntBig>(nRadiusY),
        static_cast<GIntBig>(nRasterYSize))) - nYOffMod;

    const bool bRequestFitsInSingleMetaTile =
        nXOffMod / m_nMetaTileWidth == (nXOffMod + nXSizeMod - 1) / m_nMetaTileWidth &&
        nYOffMod / m_nMetaTileHeight == (nYOffMod + nYSizeMod - 1) / m_nMetaTileHeight;
    const auto eBandDT = GetRasterBand(1)->GetRasterDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eBandDT);

    if( eRWFlag != GF_Read ||
        ((nXSize != nBufXSize || nYSize != nBufYSize) && !bRequestFitsInSingleMetaTile) )
    {
        if( eRWFlag == GF_Read && nXSizeMod <= 4096 && nYSizeMod <= 4096 && nBandCount <= 10 )
        {
            // If extracting from a small enough window, do a RasterIO()
            // at full resolution into a MEM dataset, and then proceeding to
            // resampling on it. This will avoid  to fallback on block based
            // approach.
            GDALRasterIOExtraArg sExtraArgs;
            INIT_RASTERIO_EXTRA_ARG(sExtraArgs);
            std::vector<GByte> abyBuf(nXSizeMod * nYSizeMod * nBandCount * nDTSize);
            if( IRasterIO( GF_Read, nXOffMod, nYOffMod, nXSizeMod, nYSizeMod,
                        &abyBuf[0], nXSizeMod, nYSizeMod,
                        eBandDT,
                        nBandCount, panBandMap,
                        nDTSize,
                        nDTSize * nXSizeMod,
                        nDTSize * nXSizeMod * nYSizeMod,
                        &sExtraArgs ) != CE_None )
            {
                return CE_Failure;
            }


            auto poMEMDS = std::unique_ptr<GDALDataset>(
                MEMDataset::Create( "", nXSizeMod, nYSizeMod, 0,
                                    eBandDT, nullptr ));
            for( int i = 0; i < nBandCount; i++ )
            {
                char szBuffer[32] = { '\0' };
                int nRet = CPLPrintPointer(
                        szBuffer,
                        &abyBuf[0] + i * nDTSize * nXSizeMod * nYSizeMod,
                        sizeof(szBuffer));
                szBuffer[nRet] = '\0';

                char szBuffer0[64] = { '\0' };
                snprintf(szBuffer0, sizeof(szBuffer0), "DATAPOINTER=%s", szBuffer);
                char szBuffer1[64] = { '\0' };
                snprintf( szBuffer1, sizeof(szBuffer1),
                          "PIXELOFFSET=%d", nDTSize );
                char szBuffer2[64] = { '\0' };
                snprintf( szBuffer2, sizeof(szBuffer2),
                          "LINEOFFSET=%d", nDTSize * nXSizeMod );
                char* apszOptions[4] = { szBuffer0, szBuffer1, szBuffer2, nullptr };
                poMEMDS->AddBand(eBandDT, apszOptions);
            }

            sExtraArgs.eResampleAlg = psExtraArg->eResampleAlg;
            if( psExtraArg->bFloatingPointWindowValidity )
            {
                sExtraArgs.bFloatingPointWindowValidity = true;
                sExtraArgs.dfXOff = psExtraArg->dfXOff - nXOffMod;
                sExtraArgs.dfYOff = psExtraArg->dfYOff - nYOffMod;
                sExtraArgs.dfXSize = psExtraArg->dfXSize;
                sExtraArgs.dfYSize = psExtraArg->dfYSize;
            }
            return poMEMDS->RasterIO(GF_Read,
                                     nXOff - nXOffMod, nYOff - nYOffMod,
                                     nXSize, nYSize,
                                     pData,
                                     nBufXSize, nBufYSize,
                                     eBufType,
                                     nBandCount, nullptr,
                                     nPixelSpace, nLineSpace, nBandSpace,
                                     &sExtraArgs);
        }

        // If not reading at nominal resolution, fallback to default block
        // reading
        return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize,
                                      eBufType,
                                      nBandCount, panBandMap,
                                      nPixelSpace, nLineSpace, nBandSpace,
                                      psExtraArg);
    }

    int nBufYOff = 0;

    // If the (uncompressed) size of a metatile is small enough, then download
    // it entirely to minimize the number of network requests
    const bool bDownloadWholeMetaTile =
        m_poMasterDS->m_bDownloadWholeMetaTile ||
        (static_cast<GIntBig>(m_nMetaTileWidth) * m_nMetaTileHeight *
         nBands * nDTSize < 128 * 1024);

    // Avoid probing side car files
    std::unique_ptr<CPLConfigOptionSetter> poSetter;
    const CPLString osExt(CPLGetExtension(m_osURLTemplate));
    if( osExt.size() > 0 && osExt.size() <= 3 )
    {
        poSetter.reset(new CPLConfigOptionSetter(
            "CPL_VSIL_CURL_ALLOWED_EXTENSIONS", osExt, false));
    }

    // Split the request on each metatile that it intersects
    for( int iY = nMinBlockY; iY <= nMaxBlockY; iY ++ )
    {
        const int nTileYOff = std::max(0, nYOff - iY * m_nMetaTileHeight);
        const int nTileYSize =
            std::min((iY + 1) * m_nMetaTileHeight, nYOff + nYSize) -
            std::max(nYOff, iY * m_nMetaTileHeight);

        int nBufXOff = 0;
        for( int iX = nMinBlockX; iX <= nMaxBlockX; iX++ )
        {
            CPLString osURL(m_osURLTemplate);
            osURL.replaceAll("{TileRow}", CPLSPrintf("%d", iY + m_nMinMetaTileRow));
            osURL.replaceAll("{TileCol}", CPLSPrintf("%d", iX + m_nMinMetaTileCol));

            const int nTileXOff = std::max(0, nXOff - iX * m_nMetaTileWidth);
            const int nTileXSize =
                std::min((iX + 1) * m_nMetaTileWidth, nXOff + nXSize) -
                std::max(nXOff, iX * m_nMetaTileWidth);

            const int nBufXSizeEffective =
                bRequestFitsInSingleMetaTile ? nBufXSize : nTileXSize;
            const int nBufYSizeEffective =
                bRequestFitsInSingleMetaTile ? nBufYSize : nTileYSize;

            std::shared_ptr<GDALDataset> poTileDS;
            bool bMissingTile = false;
            do
            {
                if( !m_poMasterDS->m_oCacheTileDS.tryGet(osURL, poTileDS) )
                {
                    CPLStringList aosAllowedDrivers;
                    aosAllowedDrivers.AddString("GTiff");
                    aosAllowedDrivers.AddString("PNG");
                    aosAllowedDrivers.AddString("JPEG");
                    aosAllowedDrivers.AddString("JP2KAK");
                    aosAllowedDrivers.AddString("JP2ECW");
                    aosAllowedDrivers.AddString("JP2MrSID");
                    aosAllowedDrivers.AddString("JP2OpenJPEG");
                    if( bDownloadWholeMetaTile &&
                        (STARTS_WITH(osURL, "/vsis3/") ||
                        STARTS_WITH(osURL, "/vsicurl/")) )
                    {
                        if( m_poMasterDS->m_bSkipMissingMetaTile )
                            CPLPushErrorHandler(CPLQuietErrorHandler);
                        VSILFILE* fp = VSIFOpenL(osURL, "rb");
                        if( m_poMasterDS->m_bSkipMissingMetaTile )
                            CPLPopErrorHandler();
                        if( fp == nullptr )
                        {
                            if( m_poMasterDS->m_bSkipMissingMetaTile )
                            {
                                m_poMasterDS->m_oCacheTileDS.insert(osURL, nullptr);
                                bMissingTile = true;
                                break;
                            }
                            CPLError(CE_Failure, CPLE_OpenFailed,
                                    "Cannot open %s", osURL.c_str());
                            return CE_Failure;
                        }
                        GByte* pabyBuf = nullptr;
                        vsi_l_offset nSize = 0;
                        if( !VSIIngestFile(fp, nullptr, &pabyBuf, &nSize, -1) )
                        {
                            VSIFCloseL(fp);
                            return CE_Failure;
                        }
                        VSIFCloseL(fp);
                        const CPLString osMEMFilename("/vsimem/stacta/" + osURL);
                        VSIFCloseL(VSIFileFromMemBuffer(osMEMFilename, pabyBuf, nSize, TRUE));
                        poTileDS = std::shared_ptr<GDALDataset>(
                            GDALDataset::Open(osMEMFilename,
                                            GDAL_OF_INTERNAL | GDAL_OF_RASTER,
                                            aosAllowedDrivers.List()));
                        if( poTileDS )
                            poTileDS->MarkSuppressOnClose();
                        else
                            VSIUnlink(osMEMFilename);

                    }
                    else if( bDownloadWholeMetaTile ||
                        (!STARTS_WITH(osURL, "http://") &&
                        !STARTS_WITH(osURL, "https://")) )
                    {
                        aosAllowedDrivers.AddString("HTTP");
                        if( m_poMasterDS->m_bSkipMissingMetaTile )
                            CPLPushErrorHandler(CPLQuietErrorHandler);
                        poTileDS = std::shared_ptr<GDALDataset>(
                            GDALDataset::Open(osURL,
                                            GDAL_OF_INTERNAL | GDAL_OF_RASTER,
                                            aosAllowedDrivers.List()));
                        if( m_poMasterDS->m_bSkipMissingMetaTile )
                            CPLPopErrorHandler();
                    }
                    else
                    {
                        if( m_poMasterDS->m_bSkipMissingMetaTile )
                            CPLPushErrorHandler(CPLQuietErrorHandler);
                        poTileDS = std::shared_ptr<GDALDataset>(
                            GDALDataset::Open(("/vsicurl/" + osURL).c_str(),
                                            GDAL_OF_INTERNAL | GDAL_OF_RASTER,
                                            aosAllowedDrivers.List()));
                        if( m_poMasterDS->m_bSkipMissingMetaTile )
                            CPLPopErrorHandler();
                    }
                    if( poTileDS == nullptr )
                    {
                        if( m_poMasterDS->m_bSkipMissingMetaTile )
                        {
                            m_poMasterDS->m_oCacheTileDS.insert(osURL, nullptr);
                            bMissingTile = true;
                            break;
                        }
                        CPLError(CE_Failure, CPLE_OpenFailed,
                                "Cannot open %s", osURL.c_str());
                        return CE_Failure;
                    }
                    m_poMasterDS->m_oCacheTileDS.insert(osURL, poTileDS);
                }
                if( poTileDS == nullptr )
                {
                    bMissingTile = true;
                    break;
                }

                GDALRasterIOExtraArg sExtraArgs;
                INIT_RASTERIO_EXTRA_ARG(sExtraArgs);
                if( bRequestFitsInSingleMetaTile )
                {
                    sExtraArgs.eResampleAlg = psExtraArg->eResampleAlg;
                    if( psExtraArg->bFloatingPointWindowValidity )
                    {
                        sExtraArgs.bFloatingPointWindowValidity = true;
                        sExtraArgs.dfXOff = psExtraArg->dfXOff - iX * m_nMetaTileWidth;
                        sExtraArgs.dfYOff = psExtraArg->dfYOff - iY * m_nMetaTileHeight;
                        sExtraArgs.dfXSize = psExtraArg->dfXSize;
                        sExtraArgs.dfYSize = psExtraArg->dfYSize;
                    }
                }
                CPLDebugOnly("STACTA", "Reading %d,%d,%d,%d in %s",
                            nTileXOff, nTileYOff, nTileXSize, nTileYSize,
                            osURL.c_str());
                if( poTileDS->RasterIO(
                        GF_Read, nTileXOff, nTileYOff, nTileXSize, nTileYSize,
                        static_cast<GByte*>(pData) + nBufXOff * nPixelSpace +
                                                    nBufYOff * nLineSpace,
                        nBufXSizeEffective,
                        nBufYSizeEffective,
                        eBufType,
                        nBandCount, panBandMap,
                        nPixelSpace, nLineSpace, nBandSpace,
                        &sExtraArgs) != CE_None )
                {
                    return CE_Failure;
                }
            }
            while(false);

            if( bMissingTile )
            {
                CPLDebugOnly("STACTA", "Missing metatile %s", osURL.c_str());
                for( int iBand = 0; iBand < nBandCount; iBand++ )
                {
                    int bHasNoData = FALSE;
                    double dfNodata = GetRasterBand(panBandMap[iBand])->GetNoDataValue(&bHasNoData);
                    if( !bHasNoData )
                        dfNodata = 0;
                    for( int nYBufOff = 0; nYBufOff < nBufYSizeEffective; nYBufOff ++ )
                    {
                        GByte* pabyDest = static_cast<GByte*>(pData) +
                                                iBand * nBandSpace +
                                                nBufXOff * nPixelSpace +
                                                (nBufYOff + nYBufOff) * nLineSpace;
                        GDALCopyWords(&dfNodata, GDT_Float64, 0,
                                      pabyDest, eBufType,
                                      static_cast<int>(nPixelSpace),
                                      nBufXSizeEffective);
                    }
                }
            }

            if( iX == nMinBlockX )
            {
                nBufXOff = m_nMetaTileWidth - std::max(0, nXOff - nMinBlockX * m_nMetaTileWidth);
            }
            else
            {
                nBufXOff += m_nMetaTileWidth;
            }
        }

        if( iY == nMinBlockY )
        {
            nBufYOff = m_nMetaTileHeight - std::max(0, nYOff - nMinBlockY * m_nMetaTileHeight);
        }
        else
        {
            nBufYOff += m_nMetaTileHeight;
        }
    }

    return CE_None;
}

/************************************************************************/
/*                           GetGeoTransform()                          */
/************************************************************************/

CPLErr STACTARawDataset::GetGeoTransform(double* padfGeoTransform)
{
    memcpy(padfGeoTransform, &m_adfGeoTransform[0], 6 * sizeof(double));
    return CE_None;
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int STACTADataset::Identify(GDALOpenInfo* poOpenInfo)
{
    if( STARTS_WITH(poOpenInfo->pszFilename, "STACTA:") )
    {
        return true;
    }

    if(
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        !EQUAL(CPLGetExtension(poOpenInfo->pszFilename), "json") ||
#endif
        poOpenInfo->nHeaderBytes == 0 )
    {
        return false;
    }

    for( int i = 0; i < 2; i++ )
    {
        // TryToIngest() may reallocate pabyHeader, so do not move this
        // before the loop.
        const char* pszHeader =
            reinterpret_cast<const char*>(poOpenInfo->pabyHeader);
        if( strstr(pszHeader, "\"stac_extensions\"") != nullptr &&
            strstr(pszHeader, "\"tiled-assets\"") != nullptr )
        {
            return true;
        }

        if( i == 0 )
        {
            // Should be enough for a STACTA .json file
            poOpenInfo->TryToIngest(32768);
        }
    }

    return false;
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

bool STACTADataset::Open(GDALOpenInfo* poOpenInfo)
{
    CPLString osFilename(poOpenInfo->pszFilename);
    CPLString osAssetName;
    CPLString osTMS;
    if( STARTS_WITH(poOpenInfo->pszFilename, "STACTA:") )
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2(poOpenInfo->pszFilename, ":", CSLT_HONOURSTRINGS ));
        if( aosTokens.size() != 2 && aosTokens.size() != 3 && aosTokens.size() != 4 )
            return false;
        osFilename = aosTokens[1];
        if( aosTokens.size() >= 3 )
            osAssetName = aosTokens[2];
        if( aosTokens.size() == 4 )
            osTMS = aosTokens[3];
    }

    CPLJSONDocument oDoc;
    if( STARTS_WITH(osFilename, "http://") || STARTS_WITH(osFilename, "https://") )
    {
        if( !oDoc.LoadUrl(osFilename, nullptr) )
            return false;
    }
    else
    {
        if( !oDoc.Load(osFilename) )
            return false;
    }
    const auto oRoot = oDoc.GetRoot();
    const auto oProperties = oRoot["properties"];
    if( !oProperties.IsValid() || oProperties.GetType() != CPLJSONObject::Type::Object )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing properties");
        return false;
    }

    const auto oAssetTemplates = oRoot["asset_templates"];
    if( !oAssetTemplates.IsValid() || oAssetTemplates.GetType() != CPLJSONObject::Type::Object )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Missing asset_templates");
        return false;
    }

    const auto aoAssetTemplates = oAssetTemplates.GetChildren();
    if( aoAssetTemplates.size() == 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty asset_templates");
        return false;
    }

    const auto oTMSs = oProperties.GetObj("tiles:tile_matrix_sets");
    if( !oTMSs.IsValid() || oTMSs.GetType() != CPLJSONObject::Type::Object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Missing properties[\"tiles:tile_matrix_sets\"]");
        return false;
    }
    const auto aoTMSs = oTMSs.GetChildren();
    if( aoTMSs.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Empty properties[\"tiles:tile_matrix_sets\"]");
        return false;
    }

    if( (aoAssetTemplates.size() >= 2 || aoTMSs.size() >= 2) &&
        osAssetName.empty() && osTMS.empty() )
    {
        int nSDSCount = 0;
        for( const auto& oAssetTemplate: aoAssetTemplates )
        {
            const CPLString osAssetNameSubDS = oAssetTemplate.GetName();
            const char* pszAssetNameSubDS = osAssetNameSubDS.c_str();
            if( aoTMSs.size() >= 2 )
            {
                for( const auto& oTMS: aoTMSs )
                {
                    const CPLString osTMSSubDS = oTMS.GetName();
                    const char* pszTMSSubDS = osTMSSubDS.c_str();
                    GDALDataset::SetMetadataItem(
                        CPLSPrintf("SUBDATASET_%d_NAME", nSDSCount+1),
                        CPLSPrintf("STACTA:\"%s\":%s:%s", osFilename.c_str(), pszAssetNameSubDS, pszTMSSubDS),
                        "SUBDATASETS");
                    GDALDataset::SetMetadataItem(
                        CPLSPrintf("SUBDATASET_%d_DESC", nSDSCount+1),
                        CPLSPrintf("Asset %s, tile matrix set %s", pszAssetNameSubDS, pszTMSSubDS),
                        "SUBDATASETS");
                    nSDSCount ++;
                }
            }
            else
            {
                GDALDataset::SetMetadataItem(
                    CPLSPrintf("SUBDATASET_%d_NAME", nSDSCount+1),
                    CPLSPrintf("STACTA:\"%s\":%s", osFilename.c_str(), pszAssetNameSubDS),
                    "SUBDATASETS");
                GDALDataset::SetMetadataItem(
                    CPLSPrintf("SUBDATASET_%d_DESC", nSDSCount+1),
                    CPLSPrintf("Asset %s", pszAssetNameSubDS),
                    "SUBDATASETS");
                nSDSCount ++;
            }
        }
        return true;
    }

    if( osAssetName.empty() )
    {
        osAssetName = aoAssetTemplates[0].GetName();
    }
    const auto oAssetTemplate = oAssetTemplates.GetObj(osAssetName);
    if( !oAssetTemplate.IsValid() || oAssetTemplate.GetType() != CPLJSONObject::Type::Object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find asset_templates[\"%s\"]", osAssetName.c_str());
        return false;
    }

    if( osTMS.empty() )
    {
        osTMS = aoTMSs[0].GetName();
    }
    const auto oTMS = oTMSs.GetObj(osTMS);
    if( !oTMS.IsValid() || oTMS.GetType() != CPLJSONObject::Type::Object )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find properties[\"tiles:tile_matrix_sets\"][\"%s\"]", osTMS.c_str());
        return false;
    }

    auto poTMS = gdal::TileMatrixSet::parse(oTMS.Format(CPLJSONObject::PrettyFormat::Plain).c_str());
    if( poTMS == nullptr )
        return false;

    CPLString osURLTemplate = oAssetTemplate.GetString("href");
    if( osURLTemplate.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find asset_templates[\"%s\"][\"href\"]", osAssetName.c_str());
    }
    osURLTemplate.replaceAll("{TileMatrixSet}", osTMS);
    if( STARTS_WITH(osURLTemplate, "file://") )
    {
        osURLTemplate = osURLTemplate.substr(strlen("file://"));
    }
    else if( STARTS_WITH(osURLTemplate, "s3://") )
    {
        osURLTemplate = "/vsis3/" + osURLTemplate.substr(strlen("s3://"));
    }

    if( !STARTS_WITH(osURLTemplate, "http://") &&
        !STARTS_WITH(osURLTemplate, "https://") )
    {
        if( STARTS_WITH(osURLTemplate, "./") )
            osURLTemplate = osURLTemplate.substr(2);
        osURLTemplate = CPLProjectRelativeFilename(
            CPLGetDirname(osFilename), osURLTemplate);
    }

    // Parse optional tile matrix set limits
    std::map<CPLString, Limits> oMapLimits;
    const auto oTMLinks = oProperties.GetObj("tiles:tile_matrix_links");
    if( oTMLinks.IsValid() )
    {
        if( oTMLinks.GetType() != CPLJSONObject::Type::Object )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "Invalid type for properties[\"tiles:tile_matrix_links\"]");
            return false;
        }

        auto oLimits = oTMLinks[osTMS]["limits"];
        if( oLimits.IsValid() && oLimits.GetType() == CPLJSONObject::Type::Object )
        {
            for( const auto& oLimit: oLimits.GetChildren() )
            {
                Limits limits;
                limits.min_tile_col = oLimit.GetInteger("min_tile_col");
                limits.max_tile_col = oLimit.GetInteger("max_tile_col");
                limits.min_tile_row = oLimit.GetInteger("min_tile_row");
                limits.max_tile_row = oLimit.GetInteger("max_tile_row");
                oMapLimits[oLimit.GetName()] = limits;
            }
        }
    }
    const auto tmsList = poTMS->tileMatrixList();
    if( tmsList.empty() )
        return false;

    m_bSkipMissingMetaTile = CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "SKIP_MISSING_METATILE",
            CPLGetConfigOption("GDAL_STACTA_SKIP_MISSING_METATILE", "NO")));

    std::unique_ptr<GDALDataset> poProtoDS;
    for( int i = 0; i < static_cast<int>(tmsList.size()); i++ )
    {
        // Open a metatile to get mostly its band data type
        int nProtoTileCol = 0;
        int nProtoTileRow = 0;
        auto oIterLimit = oMapLimits.find(tmsList[i].mId);
        if( oIterLimit != oMapLimits.end() )
        {
            nProtoTileCol = oIterLimit->second.min_tile_col;
            nProtoTileRow = oIterLimit->second.min_tile_row;
        }
        const CPLString osURL =
            CPLString(osURLTemplate)
                .replaceAll("{TileMatrix}", tmsList[i].mId)
                .replaceAll("{TileRow}", CPLSPrintf("%d", nProtoTileRow))
                .replaceAll("{TileCol}", CPLSPrintf("%d", nProtoTileCol));
        CPLString osProtoDSName =
            (STARTS_WITH(osURL, "http://") || STARTS_WITH(osURL, "https://")) ?
                CPLString("/vsicurl/" + osURL) : osURL;
        if( m_bSkipMissingMetaTile )
            CPLPushErrorHandler(CPLQuietErrorHandler);
        poProtoDS.reset(
            GDALDataset::Open(osProtoDSName.c_str()));
        if( m_bSkipMissingMetaTile )
            CPLPopErrorHandler();
        if( poProtoDS != nullptr )
        {
            break;
        }
        if( !m_bSkipMissingMetaTile )
        {
            CPLError(CE_Failure, CPLE_OpenFailed, "Cannot open %s",
                    osURL.c_str());
            return false;
        }
    }
    if( poProtoDS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find prototype dataset");
        return false;
    }

    // Iterate over tile matrices to create corresponding STACTARawDataset
    // objects
    for( int i = static_cast<int>(tmsList.size() - 1); i >= 0; i-- )
    {
        const auto& oTM = tmsList[i];
        int nMatrixWidth = oTM.mMatrixWidth;
        int nMatrixHeight = oTM.mMatrixHeight;
        auto oIterLimit = oMapLimits.find(tmsList[i].mId);
        if( oIterLimit != oMapLimits.end() )
        {
            nMatrixWidth = oIterLimit->second.max_tile_col - oIterLimit->second.min_tile_col + 1;
            nMatrixHeight = oIterLimit->second.max_tile_row - oIterLimit->second.min_tile_row + 1;
        }
        if( nMatrixWidth <= 0 || oTM.mTileWidth > INT_MAX / nMatrixWidth ||
            nMatrixHeight <= 0 || oTM.mTileHeight > INT_MAX / nMatrixHeight )
        {
            continue;
        }
        auto poRawDS = cpl::make_unique<STACTARawDataset>();
        if( !poRawDS->InitRaster(poProtoDS.get(), poTMS.get(), tmsList[i].mId, oTM,
                                 oMapLimits) )
        {
            return false;
        }
        poRawDS->m_osURLTemplate = osURLTemplate;
        poRawDS->m_osURLTemplate.replaceAll("{TileMatrix}", tmsList[i].mId);
        poRawDS->m_poMasterDS = this;

        if( m_poDS == nullptr )
        {
            nRasterXSize = poRawDS->GetRasterXSize();
            nRasterYSize = poRawDS->GetRasterYSize();
            m_oSRS = poRawDS->m_oSRS;
            memcpy(&m_adfGeoTransform[0], &poRawDS->m_adfGeoTransform[0], 6 * sizeof(double));
            m_poDS = std::move(poRawDS);
        }
        else
        {
            const double dfMinX = m_adfGeoTransform[0];
            const double dfMaxX = m_adfGeoTransform[0] +
                GetRasterXSize() * m_adfGeoTransform[1];
            const double dfMaxY = m_adfGeoTransform[3];
            const double dfMinY = m_adfGeoTransform[3] +
                GetRasterYSize() * m_adfGeoTransform[5];

            const double dfOvrMinX = poRawDS->m_adfGeoTransform[0];
            const double dfOvrMaxX = poRawDS->m_adfGeoTransform[0] +
                poRawDS->GetRasterXSize() * poRawDS->m_adfGeoTransform[1];
            const double dfOvrMaxY = poRawDS->m_adfGeoTransform[3];
            const double dfOvrMinY = poRawDS->m_adfGeoTransform[3] +
                poRawDS->GetRasterYSize() * poRawDS->m_adfGeoTransform[5];

            if( fabs(dfMinX - dfOvrMinX) < 1e-10 * fabs(dfMinX) &&
                fabs(dfMinY - dfOvrMinY) < 1e-10 * fabs(dfMinY) &&
                fabs(dfMaxX - dfOvrMaxX) < 1e-10 * fabs(dfMaxX) &&
                fabs(dfMaxY - dfOvrMaxY) < 1e-10 * fabs(dfMaxY) )
            {
                m_apoOverviewDS.emplace_back(std::move(poRawDS));
            }
            else
            {
                // If this zoom level doesn't share the same origin and extent
                // as the most resoluted one, then subset it
                CPLStringList aosOptions;
                aosOptions.AddString("-of");
                aosOptions.AddString("VRT");
                aosOptions.AddString("-projwin");
                aosOptions.AddString(CPLSPrintf("%.18g", dfMinX));
                aosOptions.AddString(CPLSPrintf("%.18g", dfMaxY));
                aosOptions.AddString(CPLSPrintf("%.18g", dfMaxX));
                aosOptions.AddString(CPLSPrintf("%.18g", dfMinY));
                auto psOptions = GDALTranslateOptionsNew(aosOptions.List(), nullptr);
                auto hDS = GDALTranslate(
                    "", GDALDataset::ToHandle(poRawDS.get()), psOptions, nullptr);
                GDALTranslateOptionsFree(psOptions);
                if( hDS == nullptr )
                    continue;
                m_apoIntermediaryDS.emplace_back(std::move(poRawDS));
                m_apoOverviewDS.emplace_back(GDALDataset::FromHandle(hDS));
            }
        }
    }
    if( m_poDS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find valid tile matrix");
        return false;
    }

    // Create main bands
    const auto oEoBands = oAssetTemplate.GetArray("eo:bands");
    for( int i = 0; i < m_poDS->GetRasterCount(); i++ )
    {
        auto poSrcBand = m_poDS->GetRasterBand(i+1);
        auto poBand = new STACTARasterBand(this, i+1, poSrcBand);
        if( oEoBands.IsValid() && oEoBands.Size() == poProtoDS->GetRasterCount() )
        {
            // Set band metadata
            if( oEoBands[i].GetType() == CPLJSONObject::Type::Object )
            {
                for( const auto& oItem: oEoBands[i].GetChildren() )
                {
                    poBand->GDALRasterBand::SetMetadataItem(
                        oItem.GetName().c_str(), oItem.ToString().c_str());
                }
            }
        }
        SetBand(i + 1, poBand);
    }

    // Set dataset metadata
    for( const auto& oItem: oProperties.GetChildren() )
    {
        const auto osName = oItem.GetName();
        if( osName != "tiles:tile_matrix_links" &&
            osName != "tiles:tile_matrix_sets" )
        {
            GDALDataset::SetMetadataItem(osName.c_str(), oItem.ToString().c_str());
        }
    }

    const char* pszInterleave =
        poProtoDS->GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE");
    GDALDataset::SetMetadataItem(
        "INTERLEAVE", pszInterleave ? pszInterleave : "PIXEL", "IMAGE_STRUCTURE");

    m_bDownloadWholeMetaTile = CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "WHOLE_METATILE", "NO"));

    return true;
}

/************************************************************************/
/*                          ~STACTADataset()                            */
/************************************************************************/

STACTADataset::~STACTADataset()
{
    m_poDS.reset();
    m_apoOverviewDS.clear();
    m_apoIntermediaryDS.clear();
}

/************************************************************************/
/*                          FlushCache()                                */
/************************************************************************/

void STACTADataset::FlushCache(bool bAtClosing)
{
    m_oCacheTileDS.clear();
    GDALDataset::FlushCache(bAtClosing);
}

/************************************************************************/
/*                            InitRaster()                              */
/************************************************************************/

bool STACTARawDataset::InitRaster(GDALDataset* poProtoDS,
                                  const gdal::TileMatrixSet* poTMS,
                                  const std::string& osTMId,
                                  const gdal::TileMatrixSet::TileMatrix& oTM,
                                  const std::map<CPLString, Limits>& oMapLimits)
{
    int nMatrixWidth = oTM.mMatrixWidth;
    int nMatrixHeight = oTM.mMatrixHeight;
    auto oIterLimit = oMapLimits.find(osTMId);
    if( oIterLimit != oMapLimits.end() )
    {
        m_nMinMetaTileCol = oIterLimit->second.min_tile_col;
        m_nMinMetaTileRow = oIterLimit->second.min_tile_row;
        nMatrixWidth = oIterLimit->second.max_tile_col - m_nMinMetaTileCol + 1;
        nMatrixHeight = oIterLimit->second.max_tile_row - m_nMinMetaTileRow + 1;
    }
    m_nMetaTileWidth = oTM.mTileWidth;
    m_nMetaTileHeight = oTM.mTileHeight;
    nRasterXSize = nMatrixWidth * m_nMetaTileWidth;
    nRasterYSize = nMatrixHeight * m_nMetaTileHeight;

    for( int i = 0; i < poProtoDS->GetRasterCount(); i++)
    {
        auto poProtoBand = poProtoDS->GetRasterBand(i+1);
        auto poBand = new STACTARawRasterBand(
            this, i + 1, poProtoBand);
        SetBand(i + 1, poBand);
    }

    CPLString osCRS = poTMS->crs().c_str();
    if( osCRS == "http://www.opengis.net/def/crs/OGC/1.3/CRS84" )
        osCRS = "EPSG:4326";
    if( m_oSRS.SetFromUserInput(osCRS) != OGRERR_NONE )
    {
        return false;
    }
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    m_adfGeoTransform[0] = oTM.mTopLeftX + m_nMinMetaTileCol * m_nMetaTileWidth * oTM.mResX;
    m_adfGeoTransform[1] = oTM.mResX;
    m_adfGeoTransform[3] = oTM.mTopLeftY - m_nMinMetaTileRow * m_nMetaTileHeight * oTM.mResY;
    m_adfGeoTransform[5] = -oTM.mResY;
    SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");

    return true;
}

/************************************************************************/
/*                            GetSpatialRef ()                          */
/************************************************************************/

const OGRSpatialReference* STACTADataset::GetSpatialRef() const
{
    return nBands == 0 ? nullptr : &m_oSRS;
}

/************************************************************************/
/*                           GetGeoTransform()                          */
/************************************************************************/

CPLErr STACTADataset::GetGeoTransform(double* padfGeoTransform)
{
    memcpy(padfGeoTransform, &m_adfGeoTransform[0], 6 * sizeof(double));
    return nBands == 0 ? CE_Failure: CE_None;
}

/************************************************************************/
/*                            OpenStatic()                              */
/************************************************************************/

GDALDataset* STACTADataset::OpenStatic(GDALOpenInfo* poOpenInfo)
{
    if( !Identify(poOpenInfo) )
        return nullptr;
    auto poDS = cpl::make_unique<STACTADataset>();
    if( !poDS->Open(poOpenInfo) )
        return nullptr;
    return poDS.release();
}

/************************************************************************/
/*                       GDALRegister_STACTA()                          */
/************************************************************************/

void GDALRegister_STACTA()

{
    if( GDALGetDriverByName( "STACTA" ) != nullptr )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "STACTA" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Spatio-Temporal Asset Catalog Tiled Assets" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/stacta.html" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "json" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"   <Option name='WHOLE_METATILE' type='boolean' "
                        "description='Whether to download whole metatiles'/>"
"   <Option name='SKIP_MISSING_METATILE' type='boolean' "
                        "description='Whether to gracefully skip missing metatiles'/>"
"</OpenOptionList>" );

    poDriver->pfnOpen = STACTADataset::OpenStatic;
    poDriver->pfnIdentify = STACTADataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
