/******************************************************************************
 *
 * Project:  JPEG2000 driver based on OpenJPEG or Grok library
 * Purpose:  JPEG2000 driver based on OpenJPEG or Grok library
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *           Aaron Boxer, <boxerab at protonmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, European Union (European Environment Agency)
 * Copyright (c) 2023, Grok Image Compression Inc.
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

#include <cassert>
#include <vector>

#include "cpl_atomic_ops.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_worker_thread_pool.h"
#include "gdal_frmts.h"
#include "gdaljp2abstractdataset.h"
#include "gdaljp2metadata.h"
#include "vrt/vrtdataset.h"

#include <algorithm>

#include "jp2opjlikedataset.h"

/************************************************************************/
/*                        JP2OPJLikeRasterBand()                        */
/************************************************************************/

template <typename CODEC, typename BASE>
JP2OPJLikeRasterBand<CODEC, BASE>::JP2OPJLikeRasterBand(
    JP2OPJLikeDataset<CODEC, BASE> *poDSIn, int nBandIn,
    GDALDataType eDataTypeIn, int nBits, int bPromoteTo8BitIn,
    int nBlockXSizeIn, int nBlockYSizeIn)

{
    this->eDataType = eDataTypeIn;
    this->nBlockXSize = nBlockXSizeIn;
    this->nBlockYSize = nBlockYSizeIn;
    this->bPromoteTo8Bit = bPromoteTo8BitIn;
    poCT = nullptr;

    if ((nBits % 8) != 0)
        GDALRasterBand::SetMetadataItem(
            "NBITS", CPLString().Printf("%d", nBits), "IMAGE_STRUCTURE");
    GDALRasterBand::SetMetadataItem("COMPRESSION", "JPEG2000",
                                    "IMAGE_STRUCTURE");
    this->poDS = poDSIn;
    this->nBand = nBandIn;
}

template <typename CODEC, typename BASE>
GDALColorTable *JP2OPJLikeRasterBand<CODEC, BASE>::GetColorTable()
{
    return poCT;
}

template <typename CODEC, typename BASE>
int JP2OPJLikeRasterBand<CODEC, BASE>::HasArbitraryOverviews()
{
    return poCT == nullptr;
}

/************************************************************************/
/*                      ~JP2OPJLikeRasterBand()                         */
/************************************************************************/

template <typename CODEC, typename BASE>
JP2OPJLikeRasterBand<CODEC, BASE>::~JP2OPJLikeRasterBand()
{
    delete poCT;
}

/************************************************************************/
/*                            CLAMP_0_255()                             */
/************************************************************************/

static CPL_INLINE GByte CLAMP_0_255(int val)
{
    if (val < 0)
        return 0;
    else if (val > 255)
        return 255;
    else
        return (GByte)val;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeRasterBand<CODEC, BASE>::IReadBlock(int nBlockXOff,
                                                     int nBlockYOff,
                                                     void *pImage)
{
    auto poGDS = (JP2OPJLikeDataset<CODEC, BASE> *)poDS;

#ifdef DEBUG_VERBOSE
    int nXOff = nBlockXOff * nBlockXSize;
    int nYOff = nBlockYOff * nBlockYSize;
    int nXSize = std::min(nBlockXSize, nRasterXSize - nXOff);
    int nYSize = std::min(nBlockYSize, nRasterYSize - nYOff);
    if (poGDS->iLevel == 0)
    {
        CPLDebug(CODEC::debugId(),
                 "ds.GetRasterBand(%d).ReadRaster(%d,%d,%d,%d)", nBand, nXOff,
                 nYOff, nXSize, nYSize);
    }
    else
    {
        CPLDebug(CODEC::debugId(),
                 "ds.GetRasterBand(%d).GetOverview(%d).ReadRaster(%d,%d,%d,%d)",
                 nBand, poGDS->iLevel - 1, nXOff, nYOff, nXSize, nYSize);
    }
#endif

    if (poGDS->bEnoughMemoryToLoadOtherBands)
        return poGDS->ReadBlock(nBand, poGDS->fp_, nBlockXOff, nBlockYOff,
                                pImage, poGDS->nBands, nullptr);
    else
        return poGDS->ReadBlock(nBand, poGDS->fp_, nBlockXOff, nBlockYOff,
                                pImage, 1, &nBand);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeRasterBand<CODEC, BASE>::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)
{
    auto poGDS = (JP2OPJLikeDataset<CODEC, BASE> *)poDS;

    if (eRWFlag != GF_Read)
        return CE_Failure;

    /* ==================================================================== */
    /*      Do we have overviews that would be appropriate to satisfy       */
    /*      this request?                                                   */
    /* ==================================================================== */
    if ((nBufXSize < nXSize || nBufYSize < nYSize) && GetOverviewCount() > 0)
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    int nRet =
        poGDS->PreloadBlocks(this, nXOff, nYOff, nXSize, nYSize, 0, nullptr);
    if (nRet < 0)
        return CE_Failure;
    poGDS->bEnoughMemoryToLoadOtherBands = nRet;

    CPLErr eErr = GDALPamRasterBand::IRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nPixelSpace, nLineSpace, psExtraArg);

    // cppcheck-suppress redundantAssignment
    poGDS->bEnoughMemoryToLoadOtherBands = TRUE;
    return eErr;
}

template <typename CODEC, typename BASE> class JP2JobStruct
{
  public:
    JP2OPJLikeDataset<CODEC, BASE> *poGDS_;
    int nBand;
    std::vector<std::pair<int, int>> oPairs;
    volatile int nCurPair;
    int nBandCount;
    int *panBandMap;
    volatile bool bSuccess;
};

/************************************************************************/
/*                   GetFileHandle()                                    */
/************************************************************************/

template <typename CODEC, typename BASE>
VSILFILE *JP2OPJLikeDataset<CODEC, BASE>::GetFileHandle()
{
    return this->fp_;
}

/************************************************************************/
/*                   ReadBlockInThread()                                */
/************************************************************************/

template <typename CODEC, typename BASE>
void JP2OPJLikeDataset<CODEC, BASE>::ReadBlockInThread(void *userdata)
{
    int nPair;
    auto poJob = (JP2JobStruct<CODEC, BASE> *)userdata;

    JP2OPJLikeDataset *poGDS = poJob->poGDS_;
    int nBand = poJob->nBand;
    int nPairs = (int)poJob->oPairs.size();
    int nBandCount = poJob->nBandCount;
    int *panBandMap = poJob->panBandMap;
    VSILFILE *fp = VSIFOpenL(poGDS->m_osFilename.c_str(), "rb");
    if (fp == nullptr)
    {
        CPLDebug(CODEC::debugId(), "Cannot open %s",
                 poGDS->m_osFilename.c_str());
        poJob->bSuccess = false;
        // VSIFree(pDummy);
        return;
    }

    while ((nPair = CPLAtomicInc(&(poJob->nCurPair))) < nPairs &&
           poJob->bSuccess)
    {
        int nBlockXOff = poJob->oPairs[nPair].first;
        int nBlockYOff = poJob->oPairs[nPair].second;
        poGDS->AcquireMutex();
        GDALRasterBlock *poBlock =
            poGDS->GetRasterBand(nBand)->GetLockedBlockRef(nBlockXOff,
                                                           nBlockYOff, TRUE);
        poGDS->ReleaseMutex();
        if (poBlock == nullptr)
        {
            poJob->bSuccess = false;
            break;
        }

        void *pDstBuffer = poBlock->GetDataRef();
        if (poGDS->ReadBlock(nBand, fp, nBlockXOff, nBlockYOff, pDstBuffer,
                             nBandCount, panBandMap) != CE_None)
        {
            poJob->bSuccess = false;
        }

        poBlock->DropLock();
    }

    VSIFCloseL(fp);
    // VSIFree(pDummy);
}

/************************************************************************/
/*                           PreloadBlocks()                            */
/************************************************************************/

template <typename CODEC, typename BASE>
int JP2OPJLikeDataset<CODEC, BASE>::PreloadBlocks(
    JP2OPJLikeRasterBand<CODEC, BASE> *poBand, int nXOff, int nYOff, int nXSize,
    int nYSize, int nBandCount, int *panBandMap)
{
    int bRet = TRUE;
    int nXStart = nXOff / poBand->nBlockXSize;
    int nXEnd = (nXOff + nXSize - 1) / poBand->nBlockXSize;
    int nYStart = nYOff / poBand->nBlockYSize;
    int nYEnd = (nYOff + nYSize - 1) / poBand->nBlockYSize;
    GIntBig nReqMem = (GIntBig)(nXEnd - nXStart + 1) * (nYEnd - nYStart + 1) *
                      poBand->nBlockXSize * poBand->nBlockYSize *
                      (GDALGetDataTypeSize(poBand->eDataType) / 8);

    int nMaxThreads = this->GetNumThreads();
    if (!this->bUseSetDecodeArea && nMaxThreads > 1)
    {
        if (nReqMem > GDALGetCacheMax64() / (nBandCount == 0 ? 1 : nBandCount))
            return FALSE;

        JP2JobStruct<CODEC, BASE> oJob;
        this->m_nBlocksToLoad = 0;
        try
        {
            for (int nBlockXOff = nXStart; nBlockXOff <= nXEnd; ++nBlockXOff)
            {
                for (int nBlockYOff = nYStart; nBlockYOff <= nYEnd;
                     ++nBlockYOff)
                {
                    GDALRasterBlock *poBlock =
                        poBand->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
                    if (poBlock != nullptr)
                    {
                        poBlock->DropLock();
                        continue;
                    }
                    oJob.oPairs.push_back(
                        std::pair<int, int>(nBlockXOff, nBlockYOff));
                    this->m_nBlocksToLoad++;
                }
            }
        }
        catch (const std::bad_alloc &)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory error");
            this->m_nBlocksToLoad = 0;
            return -1;
        }

        if (this->m_nBlocksToLoad > 1)
        {
            const int l_nThreads = std::min(this->m_nBlocksToLoad, nMaxThreads);
            CPLJoinableThread **pahThreads =
                (CPLJoinableThread **)VSI_CALLOC_VERBOSE(
                    sizeof(CPLJoinableThread *), l_nThreads);
            if (pahThreads == nullptr)
            {
                this->m_nBlocksToLoad = 0;
                return -1;
            }
            int i;

            CPLDebug(CODEC::debugId(), "%d blocks to load (%d threads)",
                     this->m_nBlocksToLoad, l_nThreads);

            oJob.poGDS_ = this;
            oJob.nBand = poBand->GetBand();
            oJob.nCurPair = -1;
            if (nBandCount > 0)
            {
                oJob.nBandCount = nBandCount;
                oJob.panBandMap = panBandMap;
            }
            else
            {
                if (nReqMem <= GDALGetCacheMax64() / nBands)
                {
                    oJob.nBandCount = nBands;
                    oJob.panBandMap = nullptr;
                }
                else
                {
                    bRet = FALSE;
                    oJob.nBandCount = 1;
                    oJob.panBandMap = &oJob.nBand;
                }
            }
            oJob.bSuccess = true;

            /* Flushes all dirty blocks from cache to disk to avoid them */
            /* to be flushed randomly, and simultaneously, from our worker
             * threads, */
            /* which might cause races in the output driver. */
            /* This is a workaround to a design defect of the block cache */
            GDALRasterBlock::FlushDirtyBlocks();

            for (i = 0; i < l_nThreads; i++)
            {
                pahThreads[i] =
                    CPLCreateJoinableThread(ReadBlockInThread, &oJob);
                if (pahThreads[i] == nullptr)
                    oJob.bSuccess = false;
            }
            TemporarilyDropReadWriteLock();
            for (i = 0; i < l_nThreads; i++)
                CPLJoinThread(pahThreads[i]);
            ReacquireReadWriteLock();
            CPLFree(pahThreads);
            if (!oJob.bSuccess)
            {
                this->m_nBlocksToLoad = 0;
                return -1;
            }
            this->m_nBlocksToLoad = 0;
        }
    }

    return bRet;
}

/************************************************************************/
/*                      GetEstimatedRAMUsage()                          */
/************************************************************************/

template <typename CODEC, typename BASE>
GIntBig JP2OPJLikeDataset<CODEC, BASE>::GetEstimatedRAMUsage()
{
    // libopenjp2 holds the code block values in a uint32_t array.
    GIntBig nVal = static_cast<GIntBig>(this->m_nTileWidth) *
                   this->m_nTileHeight * this->nBands * sizeof(uint32_t);
    if (this->bSingleTiled)
    {
        // libopenjp2 ingests the codestream for a whole tile. So for a
        // single-tiled image, this is roughly the size of the file.
        const auto nCurPos = VSIFTellL(this->fp_);
        VSIFSeekL(this->fp_, 0, SEEK_END);
        nVal += VSIFTellL(this->fp_);
        VSIFSeekL(this->fp_, nCurPos, SEEK_SET);
    }
    CPLDebug(CODEC::debugId(), "Estimated RAM usage for %s: %.2f GB",
             GetDescription(), static_cast<double>(nVal * 1e-9));
    return nVal;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeDataset<CODEC, BASE>::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, int *panBandMap, GSpacing nPixelSpace, GSpacing nLineSpace,
    GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg)
{
    if (eRWFlag != GF_Read)
        return CE_Failure;

    if (nBandCount < 1)
        return CE_Failure;

    auto poBand =
        (JP2OPJLikeRasterBand<CODEC, BASE> *)GetRasterBand(panBandMap[0]);

    /* ==================================================================== */
    /*      Do we have overviews that would be appropriate to satisfy       */
    /*      this request?                                                   */
    /* ==================================================================== */

    if ((nBufXSize < nXSize || nBufYSize < nYSize) &&
        poBand->GetOverviewCount() > 0)
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    this->bEnoughMemoryToLoadOtherBands = PreloadBlocks(
        poBand, nXOff, nYOff, nXSize, nYSize, nBandCount, panBandMap);

    CPLErr eErr = GDALPamDataset::IRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace,
        psExtraArg);

    this->bEnoughMemoryToLoadOtherBands = TRUE;
    return eErr;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeDataset<CODEC, BASE>::IBuildOverviews(
    const char *pszResampling, int nOverviews, const int *panOverviewList,
    int nListBands, const int *panBandList, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList papszOptions)

{
    // In order for building external overviews to work properly, we
    // discard any concept of internal overviews when the user
    // first requests to build external overviews.
    for (int i = 0; i < this->nOverviewCount; i++)
    {
        delete papoOverviewDS[i];
    }
    CPLFree(papoOverviewDS);
    papoOverviewDS = nullptr;
    this->nOverviewCount = 0;

    return GDALPamDataset::IBuildOverviews(
        pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
        pfnProgress, pProgressData, papszOptions);
}

/************************************************************************/
/*                             ReadBlock()                              */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeDataset<CODEC, BASE>::ReadBlock(int nBand, VSILFILE *fpIn,
                                                 int nBlockXOff, int nBlockYOff,
                                                 void *pImage, int nBandCount,
                                                 int *panBandMap)
{
    CPLErr eErr = CE_None;
    CODEC localctx;

    auto poBand = (JP2OPJLikeRasterBand<CODEC, BASE> *)GetRasterBand(nBand);
    int nBlockXSize = poBand->nBlockXSize;
    int nBlockYSize = poBand->nBlockYSize;
    GDALDataType eDataType = poBand->eDataType;

    int nDataTypeSize = (GDALGetDataTypeSize(eDataType) / 8);

    int nTileNumber = nBlockXOff + nBlockYOff * poBand->nBlocksPerRow;
    const int nWidthToRead =
        std::min(nBlockXSize, nRasterXSize - nBlockXOff * nBlockXSize);
    const int nHeightToRead =
        std::min(nBlockYSize, nRasterYSize - nBlockYOff * nBlockYSize);

    eErr = this->readBlockInit(fpIn, &localctx, nBlockXOff, nBlockYOff,
                               this->nRasterXSize, this->nRasterYSize,
                               nBlockXSize, nBlockYSize, nTileNumber);
    if (eErr != CE_None)
        goto end;

    for (unsigned int iBand = 0; iBand < localctx.psImage->numcomps; iBand++)
    {
        if (localctx.psImage->comps[iBand].data == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "localctx.psImage->comps[%d].data == nullptr", iBand);
            eErr = CE_Failure;
            goto end;
        }
    }

    for (int xBand = 0; xBand < nBandCount; xBand++)
    {
        GDALRasterBlock *poBlock = nullptr;
        int iBand = (panBandMap) ? panBandMap[xBand] : xBand + 1;
        int bPromoteTo8Bit =
            ((JP2OPJLikeRasterBand<CODEC, BASE> *)GetRasterBand(iBand))
                ->bPromoteTo8Bit;

        void *pDstBuffer = nullptr;
        if (iBand == nBand)
            pDstBuffer = pImage;
        else
        {
            AcquireMutex();
            poBlock =
                ((JP2OPJLikeRasterBand<CODEC, BASE> *)GetRasterBand(iBand))
                    ->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
            if (poBlock != nullptr)
            {
                ReleaseMutex();
                poBlock->DropLock();
                continue;
            }

            poBlock = GetRasterBand(iBand)->GetLockedBlockRef(nBlockXOff,
                                                              nBlockYOff, TRUE);
            ReleaseMutex();
            if (poBlock == nullptr)
            {
                continue;
            }

            pDstBuffer = poBlock->GetDataRef();
        }

        if (this->bIs420)
        {
            if ((int)localctx.psImage->comps[0].w < nWidthToRead ||
                (int)localctx.psImage->comps[0].h < nHeightToRead ||
                localctx.psImage->comps[1].w !=
                    (localctx.psImage->comps[0].w + 1) / 2 ||
                localctx.psImage->comps[1].h !=
                    (localctx.psImage->comps[0].h + 1) / 2 ||
                localctx.psImage->comps[2].w !=
                    (localctx.psImage->comps[0].w + 1) / 2 ||
                localctx.psImage->comps[2].h !=
                    (localctx.psImage->comps[0].h + 1) / 2 ||
                (nBands == 4 &&
                 ((int)localctx.psImage->comps[3].w < nWidthToRead ||
                  (int)localctx.psImage->comps[3].h < nHeightToRead)))
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "Assertion at line %d of %s failed", __LINE__,
                         __FILE__);
                if (poBlock != nullptr)
                    poBlock->DropLock();
                eErr = CE_Failure;
                goto end;
            }

            GByte *pDst = (GByte *)pDstBuffer;
            if (iBand == 4)
            {
                const auto pSrcA = localctx.psImage->comps[3].data;
                for (GPtrDiff_t j = 0; j < nHeightToRead; j++)
                {
                    memcpy(pDst + j * nBlockXSize,
                           pSrcA + j * localctx.stride(localctx.psImage->comps),
                           nWidthToRead);
                }
            }
            else
            {
                const auto pSrcY = localctx.psImage->comps[0].data;
                const auto pSrcCb = localctx.psImage->comps[1].data;
                const auto pSrcCr = localctx.psImage->comps[2].data;
                for (GPtrDiff_t j = 0; j < nHeightToRead; j++)
                {
                    for (int i = 0; i < nWidthToRead; i++)
                    {
                        int Y =
                            pSrcY[j * localctx.stride(localctx.psImage->comps) +
                                  i];
                        int Cb =
                            pSrcCb[(j / 2) * localctx.stride(
                                                 localctx.psImage->comps + 1) +
                                   (i / 2)];
                        int Cr =
                            pSrcCr[(j / 2) * localctx.stride(
                                                 localctx.psImage->comps + 2) +
                                   (i / 2)];
                        if (iBand == 1)
                            pDst[j * nBlockXSize + i] =
                                CLAMP_0_255((int)(Y + 1.402 * (Cr - 128)));
                        else if (iBand == 2)
                            pDst[j * nBlockXSize + i] =
                                CLAMP_0_255((int)(Y - 0.34414 * (Cb - 128) -
                                                  0.71414 * (Cr - 128)));
                        else if (iBand == 3)
                            pDst[j * nBlockXSize + i] =
                                CLAMP_0_255((int)(Y + 1.772 * (Cb - 128)));
                    }
                }
            }

            if (bPromoteTo8Bit)
            {
                for (GPtrDiff_t j = 0; j < nHeightToRead; j++)
                {
                    for (int i = 0; i < nWidthToRead; i++)
                    {
                        pDst[j * nBlockXSize + i] *= 255;
                    }
                }
            }
        }
        else
        {
            if ((int)localctx.psImage->comps[iBand - 1].w < nWidthToRead ||
                (int)localctx.psImage->comps[iBand - 1].h < nHeightToRead)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "Assertion at line %d of %s failed", __LINE__,
                         __FILE__);
                if (poBlock != nullptr)
                    poBlock->DropLock();
                eErr = CE_Failure;
                goto end;
            }

            if (bPromoteTo8Bit)
            {
                for (GPtrDiff_t j = 0; j < nHeightToRead; j++)
                {
                    for (int i = 0; i < nWidthToRead; i++)
                    {
                        localctx.psImage->comps[iBand - 1]
                            .data[j * localctx.stride(localctx.psImage->comps +
                                                      iBand - 1) +
                                  i] *= 255;
                    }
                }
            }

            if ((int)localctx.stride(localctx.psImage->comps + iBand - 1) ==
                    nBlockXSize &&
                (int)localctx.psImage->comps[iBand - 1].h == nBlockYSize)
            {
                GDALCopyWords64(
                    localctx.psImage->comps[iBand - 1].data, GDT_Int32, 4,
                    pDstBuffer, eDataType, nDataTypeSize,
                    static_cast<GPtrDiff_t>(nBlockXSize) * nBlockYSize);
            }
            else
            {
                for (GPtrDiff_t j = 0; j < nHeightToRead; j++)
                {
                    GDALCopyWords(
                        localctx.psImage->comps[iBand - 1].data +
                            j * localctx.stride(localctx.psImage->comps +
                                                iBand - 1),
                        GDT_Int32, 4,
                        (GByte *)pDstBuffer + j * nBlockXSize * nDataTypeSize,
                        eDataType, nDataTypeSize, nWidthToRead);
                }
            }
        }

        if (poBlock != nullptr)
            poBlock->DropLock();
    }

end:
    this->cache(&localctx);

    return eErr;
}

/************************************************************************/
/*                         GetOverviewCount()                           */
/************************************************************************/

template <typename CODEC, typename BASE>
int JP2OPJLikeRasterBand<CODEC, BASE>::GetOverviewCount()
{
    auto poGDS = cpl::down_cast<JP2OPJLikeDataset<CODEC, BASE> *>(poDS);
    if (!poGDS->AreOverviewsEnabled())
        return 0;

    if (GDALPamRasterBand::GetOverviewCount() > 0)
        return GDALPamRasterBand::GetOverviewCount();

    return poGDS->nOverviewCount;
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

template <typename CODEC, typename BASE>
GDALRasterBand *JP2OPJLikeRasterBand<CODEC, BASE>::GetOverview(int iOvrLevel)
{
    if (GDALPamRasterBand::GetOverviewCount() > 0)
        return GDALPamRasterBand::GetOverview(iOvrLevel);

    auto poGDS = (JP2OPJLikeDataset<CODEC, BASE> *)poDS;
    if (iOvrLevel < 0 || iOvrLevel >= poGDS->nOverviewCount)
        return nullptr;

    return poGDS->papoOverviewDS[iOvrLevel]->GetRasterBand(nBand);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

template <typename CODEC, typename BASE>
GDALColorInterp JP2OPJLikeRasterBand<CODEC, BASE>::GetColorInterpretation()
{
    auto poGDS = (JP2OPJLikeDataset<CODEC, BASE> *)poDS;

    if (poCT)
        return GCI_PaletteIndex;

    if (nBand == poGDS->nAlphaIndex + 1)
        return GCI_AlphaBand;

    if (poGDS->nBands <= 2 &&
        poGDS->eColorSpace == CODEC::cvtenum(JP2_CLRSPC_GRAY))
        return GCI_GrayIndex;
    else if (poGDS->eColorSpace == CODEC::cvtenum(JP2_CLRSPC_SRGB) ||
             poGDS->eColorSpace == CODEC::cvtenum(JP2_CLRSPC_SYCC))
    {
        if (nBand == poGDS->nRedIndex + 1)
            return GCI_RedBand;
        if (nBand == poGDS->nGreenIndex + 1)
            return GCI_GreenBand;
        if (nBand == poGDS->nBlueIndex + 1)
            return GCI_BlueBand;
    }

    return GCI_Undefined;
}

/************************************************************************/
/* ==================================================================== */
/*                           JP2OPJLikeDataset                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        JP2OPJLikeDataset()                           */
/************************************************************************/

template <typename CODEC, typename BASE>
JP2OPJLikeDataset<CODEC, BASE>::JP2OPJLikeDataset()
{
    this->init();
}

/************************************************************************/
/*                         ~JP2OPJLikeDataset()                         */
/************************************************************************/

template <typename CODEC, typename BASE>
JP2OPJLikeDataset<CODEC, BASE>::~JP2OPJLikeDataset()

{
    JP2OPJLikeDataset::Close();
    this->deinit();
}

/************************************************************************/
/*                              Close()                                 */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeDataset<CODEC, BASE>::Close()
{
    CPLErr eErr = CE_None;
    if (nOpenFlags != OPEN_FLAGS_CLOSED)
    {
        if (JP2OPJLikeDataset::FlushCache(true) != CE_None)
            eErr = CE_Failure;

        this->closeJP2();
        if (this->iLevel == 0 && this->fp_ != nullptr)
        {
            if (this->bRewrite)
            {
                GDALJP2Box oBox(this->fp_);
                vsi_l_offset nOffsetJP2C = 0;
                vsi_l_offset nLengthJP2C = 0;
                vsi_l_offset nOffsetXML = 0;
                vsi_l_offset nOffsetASOC = 0;
                vsi_l_offset nOffsetUUID = 0;
                vsi_l_offset nOffsetIHDR = 0;
                vsi_l_offset nLengthIHDR = 0;
                int bMSIBox = FALSE;
                int bGMLData = FALSE;
                int bUnsupportedConfiguration = FALSE;
                if (oBox.ReadFirst())
                {
                    while (strlen(oBox.GetType()) > 0)
                    {
                        if (EQUAL(oBox.GetType(), "jp2c"))
                        {
                            if (nOffsetJP2C == 0)
                            {
                                nOffsetJP2C = VSIFTellL(this->fp_);
                                nLengthJP2C = oBox.GetDataLength();
                            }
                            else
                                bUnsupportedConfiguration = TRUE;
                        }
                        else if (EQUAL(oBox.GetType(), "jp2h"))
                        {
                            GDALJP2Box oSubBox(this->fp_);
                            if (oSubBox.ReadFirstChild(&oBox) &&
                                EQUAL(oSubBox.GetType(), "ihdr"))
                            {
                                nOffsetIHDR = VSIFTellL(this->fp_);
                                nLengthIHDR = oSubBox.GetDataLength();
                            }
                        }
                        else if (EQUAL(oBox.GetType(), "xml "))
                        {
                            if (nOffsetXML == 0)
                                nOffsetXML = VSIFTellL(this->fp_);
                        }
                        else if (EQUAL(oBox.GetType(), "asoc"))
                        {
                            if (nOffsetASOC == 0)
                                nOffsetASOC = VSIFTellL(this->fp_);

                            GDALJP2Box oSubBox(this->fp_);
                            if (oSubBox.ReadFirstChild(&oBox) &&
                                EQUAL(oSubBox.GetType(), "lbl "))
                            {
                                char *pszLabel = (char *)oSubBox.ReadBoxData();
                                if (pszLabel != nullptr &&
                                    EQUAL(pszLabel, "gml.data"))
                                {
                                    bGMLData = TRUE;
                                }
                                else
                                    bUnsupportedConfiguration = TRUE;
                                CPLFree(pszLabel);
                            }
                            else
                                bUnsupportedConfiguration = TRUE;
                        }
                        else if (EQUAL(oBox.GetType(), "uuid"))
                        {
                            if (nOffsetUUID == 0)
                                nOffsetUUID = VSIFTellL(this->fp_);
                            if (GDALJP2Metadata::IsUUID_MSI(oBox.GetUUID()))
                                bMSIBox = TRUE;
                            else if (!GDALJP2Metadata::IsUUID_XMP(
                                         oBox.GetUUID()))
                                bUnsupportedConfiguration = TRUE;
                        }
                        else if (!EQUAL(oBox.GetType(), "jP  ") &&
                                 !EQUAL(oBox.GetType(), "ftyp") &&
                                 !EQUAL(oBox.GetType(), "rreq") &&
                                 !EQUAL(oBox.GetType(), "jp2h") &&
                                 !EQUAL(oBox.GetType(), "jp2i"))
                        {
                            bUnsupportedConfiguration = TRUE;
                        }

                        if (bUnsupportedConfiguration || !oBox.ReadNext())
                            break;
                    }
                }

                const char *pszGMLJP2;
                int bGeoreferencingCompatOfGMLJP2 =
                    (!m_oSRS.IsEmpty() && bGeoTransformValid && nGCPCount == 0);
                if (bGeoreferencingCompatOfGMLJP2 &&
                    ((this->bHasGeoreferencingAtOpening && bGMLData) ||
                     (!this->bHasGeoreferencingAtOpening)))
                    pszGMLJP2 = "GMLJP2=YES";
                else
                    pszGMLJP2 = "GMLJP2=NO";

                const char *pszGeoJP2;
                int bGeoreferencingCompatOfGeoJP2 =
                    (!m_oSRS.IsEmpty() || nGCPCount != 0 || bGeoTransformValid);
                if (bGeoreferencingCompatOfGeoJP2 &&
                    ((this->bHasGeoreferencingAtOpening && bMSIBox) ||
                     (!this->bHasGeoreferencingAtOpening) ||
                     this->nGCPCount > 0))
                    pszGeoJP2 = "GeoJP2=YES";
                else
                    pszGeoJP2 = "GeoJP2=NO";

                /* Test that the length of the JP2C box is not 0 */
                int bJP2CBoxOKForRewriteInPlace = TRUE;
                if (nOffsetJP2C > 16 && !bUnsupportedConfiguration)
                {
                    VSIFSeekL(this->fp_, nOffsetJP2C - 8, SEEK_SET);
                    GByte abyBuffer[8];
                    VSIFReadL(abyBuffer, 1, 8, this->fp_);
                    if (STARTS_WITH_CI((const char *)abyBuffer + 4, "jp2c") &&
                        abyBuffer[0] == 0 && abyBuffer[1] == 0 &&
                        abyBuffer[2] == 0 && abyBuffer[3] == 0)
                    {
                        if ((vsi_l_offset)(GUInt32)(nLengthJP2C + 8) ==
                            (nLengthJP2C + 8))
                        {
                            CPLDebug(
                                CODEC::debugId(),
                                "Patching length of JP2C box with real length");
                            VSIFSeekL(this->fp_, nOffsetJP2C - 8, SEEK_SET);
                            GUInt32 nLength = (GUInt32)nLengthJP2C + 8;
                            CPL_MSBPTR32(&nLength);
                            if (VSIFWriteL(&nLength, 1, 4, this->fp_) != 1)
                                eErr = CE_Failure;
                        }
                        else
                            bJP2CBoxOKForRewriteInPlace = FALSE;
                    }
                }

                if (nOffsetJP2C == 0 || bUnsupportedConfiguration)
                {
                    eErr = CE_Failure;
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot rewrite file due to unsupported JP2 box "
                             "configuration");
                    VSIFCloseL(this->fp_);
                }
                else if (bJP2CBoxOKForRewriteInPlace &&
                         (nOffsetXML == 0 || nOffsetXML > nOffsetJP2C) &&
                         (nOffsetASOC == 0 || nOffsetASOC > nOffsetJP2C) &&
                         (nOffsetUUID == 0 || nOffsetUUID > nOffsetJP2C))
                {
                    CPLDebug(CODEC::debugId(),
                             "Rewriting boxes after codestream");

                    /* Update IPR flag */
                    if (nLengthIHDR == 14)
                    {
                        VSIFSeekL(this->fp_, nOffsetIHDR + nLengthIHDR - 1,
                                  SEEK_SET);
                        GByte bIPR = GetMetadata("xml:IPR") != nullptr;
                        if (VSIFWriteL(&bIPR, 1, 1, this->fp_) != 1)
                            eErr = CE_Failure;
                    }

                    VSIFSeekL(this->fp_, nOffsetJP2C + nLengthJP2C, SEEK_SET);

                    GDALJP2Metadata oJP2MD;
                    if (GetGCPCount() > 0)
                    {
                        oJP2MD.SetGCPs(GetGCPCount(), GetGCPs());
                        oJP2MD.SetSpatialRef(GetGCPSpatialRef());
                    }
                    else
                    {
                        const OGRSpatialReference *poSRS = GetSpatialRef();
                        if (poSRS != nullptr)
                        {
                            oJP2MD.SetSpatialRef(poSRS);
                        }
                        if (bGeoTransformValid)
                        {
                            oJP2MD.SetGeoTransform(adfGeoTransform);
                        }
                    }

                    const char *pszAreaOrPoint =
                        GetMetadataItem(GDALMD_AREA_OR_POINT);
                    oJP2MD.bPixelIsPoint =
                        pszAreaOrPoint != nullptr &&
                        EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);

                    if (!WriteIPRBox(this->fp_, this))
                        eErr = CE_Failure;

                    if (bGeoreferencingCompatOfGMLJP2 &&
                        EQUAL(pszGMLJP2, "GMLJP2=YES"))
                    {
                        GDALJP2Box *poBox =
                            oJP2MD.CreateGMLJP2(nRasterXSize, nRasterYSize);
                        if (!WriteBox(this->fp_, poBox))
                            eErr = CE_Failure;
                        delete poBox;
                    }

                    if (!WriteXMLBoxes(this->fp_, this) ||
                        !WriteGDALMetadataBox(this->fp_, this, nullptr))
                        eErr = CE_Failure;

                    if (bGeoreferencingCompatOfGeoJP2 &&
                        EQUAL(pszGeoJP2, "GeoJP2=YES"))
                    {
                        GDALJP2Box *poBox = oJP2MD.CreateJP2GeoTIFF();
                        if (!WriteBox(this->fp_, poBox))
                            eErr = CE_Failure;
                        delete poBox;
                    }

                    if (!WriteXMPBox(this->fp_, this))
                        eErr = CE_Failure;

                    if (VSIFTruncateL(this->fp_, VSIFTellL(this->fp_)) != 0)
                        eErr = CE_Failure;

                    if (VSIFCloseL(this->fp_) != 0)
                        eErr = CE_Failure;
                }
                else
                {
                    VSIFCloseL(this->fp_);

                    CPLDebug(CODEC::debugId(), "Rewriting whole file");

                    const char *const apszOptions[] = {"USE_SRC_CODESTREAM=YES",
                                                       "CODEC=JP2",
                                                       "WRITE_METADATA=YES",
                                                       pszGMLJP2,
                                                       pszGeoJP2,
                                                       nullptr};
                    CPLString osTmpFilename(
                        CPLSPrintf("%s.tmp", GetDescription()));
                    GDALDataset *poOutDS = CreateCopy(
                        osTmpFilename, this, FALSE, (char **)apszOptions,
                        GDALDummyProgress, nullptr);
                    if (poOutDS)
                    {
                        if (GDALClose(poOutDS) != CE_None)
                            eErr = CE_Failure;
                        if (VSIRename(osTmpFilename, GetDescription()) != 0)
                            eErr = CE_Failure;
                    }
                    else
                    {
                        eErr = CE_Failure;
                        VSIUnlink(osTmpFilename);
                    }
                    VSIUnlink(CPLSPrintf("%s.tmp.aux.xml", GetDescription()));
                }
            }
            else
                VSIFCloseL(this->fp_);
        }

        JP2OPJLikeDataset::CloseDependentDatasets();

        if (GDALPamDataset::Close() != CE_None)
            eErr = CE_Failure;
    }
    return eErr;
}

/************************************************************************/
/*                      CloseDependentDatasets()                        */
/************************************************************************/

template <typename CODEC, typename BASE>
int JP2OPJLikeDataset<CODEC, BASE>::CloseDependentDatasets()
{
    int bRet = GDALJP2AbstractDataset::CloseDependentDatasets();
    if (papoOverviewDS)
    {
        for (int i = 0; i < this->nOverviewCount; i++)
            delete papoOverviewDS[i];
        CPLFree(papoOverviewDS);
        papoOverviewDS = nullptr;
        bRet = TRUE;
    }
    return bRet;
}

/************************************************************************/
/*                           SetSpatialRef()                            */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr
JP2OPJLikeDataset<CODEC, BASE>::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    if (eAccess == GA_Update)
    {
        this->bRewrite = TRUE;
        m_oSRS.Clear();
        if (poSRS)
            m_oSRS = *poSRS;
        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetSpatialRef(poSRS);
}

/************************************************************************/
/*                           SetGeoTransform()                          */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeDataset<CODEC, BASE>::SetGeoTransform(double *padfGeoTransform)
{
    if (eAccess == GA_Update)
    {
        this->bRewrite = TRUE;
        memcpy(adfGeoTransform, padfGeoTransform, 6 * sizeof(double));
        bGeoTransformValid =
            !(adfGeoTransform[0] == 0.0 && adfGeoTransform[1] == 1.0 &&
              adfGeoTransform[2] == 0.0 && adfGeoTransform[3] == 0.0 &&
              adfGeoTransform[4] == 0.0 && adfGeoTransform[5] == 1.0);
        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetGeoTransform(padfGeoTransform);
}

/************************************************************************/
/*                           SetGCPs()                                  */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeDataset<CODEC, BASE>::SetGCPs(int nGCPCountIn,
                                               const GDAL_GCP *pasGCPListIn,
                                               const OGRSpatialReference *poSRS)
{
    if (eAccess == GA_Update)
    {
        this->bRewrite = TRUE;
        if (nGCPCount > 0)
        {
            GDALDeinitGCPs(nGCPCount, pasGCPList);
            CPLFree(pasGCPList);
        }

        m_oSRS.Clear();
        if (poSRS)
            m_oSRS = *poSRS;

        nGCPCount = nGCPCountIn;
        pasGCPList = GDALDuplicateGCPs(nGCPCount, pasGCPListIn);

        return CE_None;
    }
    else
        return GDALJP2AbstractDataset::SetGCPs(nGCPCountIn, pasGCPListIn,
                                               poSRS);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeDataset<CODEC, BASE>::SetMetadata(char **papszMetadata,
                                                   const char *pszDomain)
{
    if (eAccess == GA_Update)
    {
        this->bRewrite = TRUE;
        if (pszDomain == nullptr || EQUAL(pszDomain, ""))
        {
            CSLDestroy(m_papszMainMD);
            m_papszMainMD = CSLDuplicate(papszMetadata);
        }
        return GDALDataset::SetMetadata(papszMetadata, pszDomain);
    }
    return GDALJP2AbstractDataset::SetMetadata(papszMetadata, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

template <typename CODEC, typename BASE>
CPLErr JP2OPJLikeDataset<CODEC, BASE>::SetMetadataItem(const char *pszName,
                                                       const char *pszValue,
                                                       const char *pszDomain)
{
    if (eAccess == GA_Update)
    {
        this->bRewrite = TRUE;
        if (pszDomain == nullptr || EQUAL(pszDomain, ""))
        {
            m_papszMainMD = CSLSetNameValue(GetMetadata(), pszName, pszValue);
        }
        return GDALDataset::SetMetadataItem(pszName, pszValue, pszDomain);
    }
    return GDALJP2AbstractDataset::SetMetadataItem(pszName, pszValue,
                                                   pszDomain);
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

#ifndef jpc_header_defined
#define jpc_header_defined
static const unsigned char jpc_header[] = {0xff, 0x4f, 0xff,
                                           0x51};  // SOC + RSIZ markers
static const unsigned char jp2_box_jp[] = {0x6a, 0x50, 0x20, 0x20}; /* 'jP  ' */
#endif

template <typename CODEC, typename BASE>
int JP2OPJLikeDataset<CODEC, BASE>::Identify(GDALOpenInfo *poOpenInfo)

{
    if (poOpenInfo->nHeaderBytes >= 16 &&
        (memcmp(poOpenInfo->pabyHeader, jpc_header, sizeof(jpc_header)) == 0 ||
         memcmp(poOpenInfo->pabyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp)) ==
             0))
        return TRUE;

    else
        return FALSE;
}

/************************************************************************/
/*                        JP2FindCodeStream()                           */
/************************************************************************/

static vsi_l_offset JP2FindCodeStream(VSILFILE *fp, vsi_l_offset *pnLength)
{
    vsi_l_offset nCodeStreamStart = 0;
    vsi_l_offset nCodeStreamLength = 0;

    VSIFSeekL(fp, 0, SEEK_SET);
    GByte abyHeader[16];
    VSIFReadL(abyHeader, 1, 16, fp);

    if (memcmp(abyHeader, jpc_header, sizeof(jpc_header)) == 0)
    {
        VSIFSeekL(fp, 0, SEEK_END);
        nCodeStreamLength = VSIFTellL(fp);
    }
    else if (memcmp(abyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp)) == 0)
    {
        /* Find offset of first jp2c box */
        GDALJP2Box oBox(fp);
        if (oBox.ReadFirst())
        {
            while (strlen(oBox.GetType()) > 0)
            {
                if (EQUAL(oBox.GetType(), "jp2c"))
                {
                    nCodeStreamStart = VSIFTellL(fp);
                    nCodeStreamLength = oBox.GetDataLength();
                    break;
                }

                if (!oBox.ReadNext())
                    break;
            }
        }
    }
    *pnLength = nCodeStreamLength;
    return nCodeStreamStart;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

template <typename CODEC, typename BASE>
GDALDataset *JP2OPJLikeDataset<CODEC, BASE>::Open(GDALOpenInfo *poOpenInfo)

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr)
        return nullptr;

    /* Detect which codec to use : J2K or JP2 ? */
    vsi_l_offset nCodeStreamLength = 0;
    vsi_l_offset nCodeStreamStart =
        JP2FindCodeStream(poOpenInfo->fpL, &nCodeStreamLength);

    if (nCodeStreamStart == 0 && nCodeStreamLength == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No code-stream in JP2 file");
        return nullptr;
    }
    JP2OPJLikeDataset oTmpDS;
    int numThreads = oTmpDS.GetNumThreads();
    auto eCodecFormat = (nCodeStreamStart == 0) ? CODEC::cvtenum(JP2_CODEC_J2K)
                                                : CODEC::cvtenum(JP2_CODEC_JP2);

    uint32_t nTileW = 0, nTileH = 0;
    int numResolutions = 0;
    CODEC localctx;
    localctx.open(poOpenInfo->fpL, nCodeStreamStart);
    if (!localctx.setUpDecompress(numThreads, nCodeStreamLength, &nTileW,
                                  &nTileH, &numResolutions))
        return nullptr;

    GDALDataType eDataType = GDT_Byte;
    if (localctx.psImage->comps[0].prec > 16)
    {
        if (localctx.psImage->comps[0].sgnd)
            eDataType = GDT_Int32;
        else
            eDataType = GDT_UInt32;
    }
    else if (localctx.psImage->comps[0].prec > 8)
    {
        if (localctx.psImage->comps[0].sgnd)
            eDataType = GDT_Int16;
        else
            eDataType = GDT_UInt16;
    }

    int bIs420 =
        (localctx.psImage->color_space != CODEC::cvtenum(JP2_CLRSPC_SRGB) &&
         eDataType == GDT_Byte &&
         (localctx.psImage->numcomps == 3 || localctx.psImage->numcomps == 4) &&
         localctx.psImage->comps[1].w == localctx.psImage->comps[0].w / 2 &&
         localctx.psImage->comps[1].h == localctx.psImage->comps[0].h / 2 &&
         localctx.psImage->comps[2].w == localctx.psImage->comps[0].w / 2 &&
         localctx.psImage->comps[2].h == localctx.psImage->comps[0].h / 2) &&
        (localctx.psImage->numcomps == 3 ||
         (localctx.psImage->numcomps == 4 &&
          localctx.psImage->comps[3].w == localctx.psImage->comps[0].w &&
          localctx.psImage->comps[3].h == localctx.psImage->comps[0].h));

    if (bIs420)
    {
        CPLDebug(CODEC::debugId(), "420 format");
    }
    else
    {
        int iBand;
        for (iBand = 2; iBand <= (int)localctx.psImage->numcomps; iBand++)
        {
            if (localctx.psImage->comps[iBand - 1].w !=
                    localctx.psImage->comps[0].w ||
                localctx.psImage->comps[iBand - 1].h !=
                    localctx.psImage->comps[0].h)
            {
                CPLDebug(CODEC::debugId(), "Unable to handle that image (2)");
                localctx.free();
                return nullptr;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    JP2OPJLikeDataset *poDS;
    int iBand;

    poDS = new JP2OPJLikeDataset();
    poDS->m_osFilename = poOpenInfo->pszFilename;
    if (eCodecFormat == CODEC::cvtenum(JP2_CODEC_JP2))
        poDS->eAccess = poOpenInfo->eAccess;
    poDS->eColorSpace = localctx.psImage->color_space;
    poDS->nRasterXSize = localctx.psImage->x1 - localctx.psImage->x0;
    poDS->nRasterYSize = localctx.psImage->y1 - localctx.psImage->y0;
    poDS->nBands = localctx.psImage->numcomps;
    poDS->fp_ = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;
    poDS->nCodeStreamStart = nCodeStreamStart;
    poDS->nCodeStreamLength = nCodeStreamLength;
    poDS->bIs420 = bIs420;
    poDS->bSingleTiled = (poDS->nRasterXSize == (int)nTileW &&
                          poDS->nRasterYSize == (int)nTileH);
    poDS->m_nX0 = localctx.psImage->x0;
    poDS->m_nY0 = localctx.psImage->y0;
    poDS->m_nTileWidth = nTileW;
    poDS->m_nTileHeight = nTileH;

    int nBlockXSize = (int)nTileW;
    int nBlockYSize = (int)nTileH;

    if (CPLFetchBool(poOpenInfo->papszOpenOptions, "USE_TILE_AS_BLOCK", false))
    {
        poDS->bUseSetDecodeArea = false;
    }

    poDS->m_bStrict = CPLTestBool(
        CSLFetchNameValueDef(poOpenInfo->papszOpenOptions, "STRICT", "YES"));
    localctx.updateStrict(poDS->m_bStrict);

    if (localctx.preferPerBlockDeCompress())
    {
        /* Some Sentinel2 preview datasets are 343x343 large, but with 8x8 blocks */
        /* Using the tile API for that is super slow, so expose a single block */
        if (poDS->nRasterXSize <= 1024 && poDS->nRasterYSize <= 1024 &&
            nTileW < 32 && nTileH < 32)
        {
            poDS->bUseSetDecodeArea = true;
            nBlockXSize = poDS->nRasterXSize;
            nBlockYSize = poDS->nRasterYSize;
        }
        else
        {
            poDS->bUseSetDecodeArea =
                poDS->bSingleTiled &&
                (poDS->nRasterXSize > 1024 || poDS->nRasterYSize > 1024);

            /* Other Sentinel2 preview datasets are 343x343 and 60m are 1830x1830,
             * but they */
            /* are tiled with tile dimensions 2048x2048. It would be a waste of */
            /* memory to allocate such big blocks */
            if (poDS->nRasterXSize < (int)nTileW &&
                poDS->nRasterYSize < (int)nTileH)
            {
                poDS->bUseSetDecodeArea = TRUE;
                nBlockXSize = poDS->nRasterXSize;
                nBlockYSize = poDS->nRasterYSize;
                if (nBlockXSize > 2048)
                    nBlockXSize = 2048;
                if (nBlockYSize > 2048)
                    nBlockYSize = 2048;
            }
            else if (poDS->bUseSetDecodeArea)
            {
                // Arbitrary threshold... ~4 million at least needed for the GRIB2
                // images mentioned below.
                if (nTileH == 1 && nTileW < 20 * 1024 * 1024)
                {
                    // Some GRIB2 JPEG2000 compressed images are a 2D image
                    // organized as a single line image...
                }
                else
                {
                    if (nBlockXSize > 1024)
                        nBlockXSize = 1024;
                    if (nBlockYSize > 1024)
                        nBlockYSize = 1024;
                }
            }
        }
    }

    GDALColorTable *poCT = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Look for color table or cdef box                                */
    /* -------------------------------------------------------------------- */
    if (eCodecFormat == CODEC::cvtenum(JP2_CODEC_JP2))
    {
        vsi_l_offset nCurOffset = VSIFTellL(poDS->fp_);

        GDALJP2Box oBox(poDS->fp_);
        if (oBox.ReadFirst())
        {
            while (strlen(oBox.GetType()) > 0)
            {
                if (EQUAL(oBox.GetType(), "jp2h"))
                {
                    GDALJP2Box oSubBox(poDS->fp_);

                    for (oSubBox.ReadFirstChild(&oBox);
                         strlen(oSubBox.GetType()) > 0;
                         oSubBox.ReadNextChild(&oBox))
                    {
                        GIntBig nDataLength = oSubBox.GetDataLength();
                        if (poCT == nullptr &&
                            EQUAL(oSubBox.GetType(), "pclr") &&
                            nDataLength >= 3 &&
                            nDataLength <= 2 + 1 + 4 + 4 * 256)
                        {
                            GByte *pabyCT = oSubBox.ReadBoxData();
                            if (pabyCT != nullptr)
                            {
                                int nEntries = (pabyCT[0] << 8) | pabyCT[1];
                                int nComponents = pabyCT[2];
                                /* CPLDebug(CODEC::debugId(), "Color table found"); */
                                if (nEntries <= 256 && nComponents == 3)
                                {
                                    /*CPLDebug(CODEC::debugId(), "resol[0] = %d",
                                    pabyCT[3]); CPLDebug(CODEC::debugId(), "resol[1] =
                                    %d", pabyCT[4]); CPLDebug(CODEC::debugId(),
                                    "resol[2] = %d", pabyCT[5]);*/
                                    if (pabyCT[3] == 7 && pabyCT[4] == 7 &&
                                        pabyCT[5] == 7 &&
                                        nDataLength == 2 + 1 + 3 + 3 * nEntries)
                                    {
                                        poCT = new GDALColorTable();
                                        for (int i = 0; i < nEntries; i++)
                                        {
                                            GDALColorEntry sEntry;
                                            sEntry.c1 = pabyCT[6 + 3 * i];
                                            sEntry.c2 = pabyCT[6 + 3 * i + 1];
                                            sEntry.c3 = pabyCT[6 + 3 * i + 2];
                                            sEntry.c4 = 255;
                                            poCT->SetColorEntry(i, &sEntry);
                                        }
                                    }
                                }
                                else if (nEntries <= 256 && nComponents == 4)
                                {
                                    if (pabyCT[3] == 7 && pabyCT[4] == 7 &&
                                        pabyCT[5] == 7 && pabyCT[6] == 7 &&
                                        nDataLength == 2 + 1 + 4 + 4 * nEntries)
                                    {
                                        poCT = new GDALColorTable();
                                        for (int i = 0; i < nEntries; i++)
                                        {
                                            GDALColorEntry sEntry;
                                            sEntry.c1 = pabyCT[7 + 4 * i];
                                            sEntry.c2 = pabyCT[7 + 4 * i + 1];
                                            sEntry.c3 = pabyCT[7 + 4 * i + 2];
                                            sEntry.c4 = pabyCT[7 + 4 * i + 3];
                                            poCT->SetColorEntry(i, &sEntry);
                                        }
                                    }
                                }
                                CPLFree(pabyCT);
                            }
                        }
                        /* There's a bug/misfeature in openjpeg: the color_space
                           only gets set at read tile time */
                        else if (EQUAL(oSubBox.GetType(), "colr") &&
                                 nDataLength == 7)
                        {
                            GByte *pabyContent = oSubBox.ReadBoxData();
                            if (pabyContent != nullptr)
                            {
                                if (pabyContent[0] ==
                                    1 /* enumerated colourspace */)
                                {
                                    GUInt32 enumcs = (pabyContent[3] << 24) |
                                                     (pabyContent[4] << 16) |
                                                     (pabyContent[5] << 8) |
                                                     (pabyContent[6]);
                                    if (enumcs == 16)
                                    {
                                        poDS->eColorSpace =
                                            CODEC::cvtenum(JP2_CLRSPC_SRGB);
                                        CPLDebug(CODEC::debugId(),
                                                 "SRGB color space");
                                    }
                                    else if (enumcs == 17)
                                    {
                                        poDS->eColorSpace =
                                            CODEC::cvtenum(JP2_CLRSPC_GRAY);
                                        CPLDebug(CODEC::debugId(),
                                                 "Grayscale color space");
                                    }
                                    else if (enumcs == 18)
                                    {
                                        poDS->eColorSpace =
                                            CODEC::cvtenum(JP2_CLRSPC_SYCC);
                                        CPLDebug(CODEC::debugId(),
                                                 "SYCC color space");
                                    }
                                    else if (enumcs == 20)
                                    {
                                        /* Used by
                                         * J2KP4files/testfiles_jp2/file7.jp2 */
                                        poDS->eColorSpace =
                                            CODEC::cvtenum(JP2_CLRSPC_SRGB);
                                        CPLDebug(CODEC::debugId(),
                                                 "e-sRGB color space");
                                    }
                                    else if (enumcs == 21)
                                    {
                                        /* Used by
                                         * J2KP4files/testfiles_jp2/file5.jp2 */
                                        poDS->eColorSpace =
                                            CODEC::cvtenum(JP2_CLRSPC_SRGB);
                                        CPLDebug(CODEC::debugId(),
                                                 "ROMM-RGB color space");
                                    }
                                    else
                                    {
                                        poDS->eColorSpace =
                                            CODEC::cvtenum(JP2_CLRSPC_UNKNOWN);
                                        CPLDebug(CODEC::debugId(),
                                                 "Unknown color space");
                                    }
                                }
                                CPLFree(pabyContent);
                            }
                        }
                        /* Check if there's an alpha channel or odd channel
                         * attribution */
                        else if (EQUAL(oSubBox.GetType(), "cdef") &&
                                 nDataLength == 2 + poDS->nBands * 6)
                        {
                            GByte *pabyContent = oSubBox.ReadBoxData();
                            if (pabyContent != nullptr)
                            {
                                int nEntries =
                                    (pabyContent[0] << 8) | pabyContent[1];
                                if (nEntries == poDS->nBands)
                                {
                                    poDS->nRedIndex = -1;
                                    poDS->nGreenIndex = -1;
                                    poDS->nBlueIndex = -1;
                                    for (int i = 0; i < poDS->nBands; i++)
                                    {
                                        int CNi =
                                            (pabyContent[2 + 6 * i] << 8) |
                                            pabyContent[2 + 6 * i + 1];
                                        int Typi =
                                            (pabyContent[2 + 6 * i + 2] << 8) |
                                            pabyContent[2 + 6 * i + 3];
                                        int Asoci =
                                            (pabyContent[2 + 6 * i + 4] << 8) |
                                            pabyContent[2 + 6 * i + 5];
                                        if (CNi < 0 || CNi >= poDS->nBands)
                                        {
                                            CPLError(CE_Failure,
                                                     CPLE_AppDefined,
                                                     "Wrong value of CN%d=%d",
                                                     i, CNi);
                                            break;
                                        }
                                        if (Typi == 0)
                                        {
                                            if (Asoci == 1)
                                                poDS->nRedIndex = CNi;
                                            else if (Asoci == 2)
                                                poDS->nGreenIndex = CNi;
                                            else if (Asoci == 3)
                                                poDS->nBlueIndex = CNi;
                                            else if (Asoci < 0 ||
                                                     (Asoci > poDS->nBands &&
                                                      Asoci != 65535))
                                            {
                                                CPLError(
                                                    CE_Failure, CPLE_AppDefined,
                                                    "Wrong value of Asoc%d=%d",
                                                    i, Asoci);
                                                break;
                                            }
                                        }
                                        else if (Typi == 1)
                                        {
                                            poDS->nAlphaIndex = CNi;
                                        }
                                    }
                                }
                                else
                                {
                                    CPLDebug(CODEC::debugId(),
                                             "Unsupported cdef content");
                                }
                                CPLFree(pabyContent);
                            }
                        }
                    }
                }

                if (!oBox.ReadNext())
                    break;
            }
        }

        VSIFSeekL(poDS->fp_, nCurOffset, SEEK_SET);

        if (poDS->eColorSpace == CODEC::cvtenum(JP2_CLRSPC_GRAY) &&
            poDS->nBands == 4 && poDS->nRedIndex == 0 &&
            poDS->nGreenIndex == 1 && poDS->nBlueIndex == 2 &&
            poDS->m_osFilename.find("dop10rgbi") != std::string::npos)
        {
            CPLDebug(CODEC::debugId(),
                     "Autofix wrong colorspace from Greyscale to sRGB");
            // Workaround https://github.com/uclouvain/openjpeg/issues/1464
            // dop10rgbi products from https://www.opengeodata.nrw.de/produkte/geobasis/lusat/dop/dop_jp2_f10/
            // have a wrong color space.
            poDS->eColorSpace = CODEC::cvtenum(JP2_CLRSPC_SRGB);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    for (iBand = 1; iBand <= poDS->nBands; iBand++)
    {
        const bool bPromoteTo8Bit =
            iBand == poDS->nAlphaIndex + 1 &&
            localctx.psImage
                    ->comps[(poDS->nAlphaIndex == 0 && poDS->nBands > 1) ? 1
                                                                         : 0]
                    .prec == 8 &&
            localctx.psImage->comps[poDS->nAlphaIndex].prec == 1 &&
            CPLFetchBool(poOpenInfo->papszOpenOptions, "1BIT_ALPHA_PROMOTION",
                         CPLTestBool(CPLGetConfigOption(
                             "JP2OPENJPEG_PROMOTE_1BIT_ALPHA_AS_8BIT", "YES")));
        if (bPromoteTo8Bit)
            CPLDebug(CODEC::debugId(),
                     "Alpha band is promoted from 1 bit to 8 bit");

        auto poBand = new JP2OPJLikeRasterBand<CODEC, BASE>(
            poDS, iBand, eDataType,
            bPromoteTo8Bit ? 8 : localctx.psImage->comps[iBand - 1].prec,
            bPromoteTo8Bit, nBlockXSize, nBlockYSize);
        if (iBand == 1 && poCT != nullptr)
            poBand->poCT = poCT;
        poDS->SetBand(iBand, poBand);
    }

    /* -------------------------------------------------------------------- */
    /*      Create overview datasets.                                       */
    /* -------------------------------------------------------------------- */
    int nW = poDS->nRasterXSize;
    int nH = poDS->nRasterYSize;
    poDS->nParentXSize = poDS->nRasterXSize;
    poDS->nParentYSize = poDS->nRasterYSize;

    /* Lower resolutions are not compatible with a color-table */
    if (poCT != nullptr)
        numResolutions = 0;

    if (poDS->bSingleTiled && poDS->bUseSetDecodeArea)
    {
        poDS->cacheNew(&localctx);
    }
    poDS->m_pnLastLevel = new int(-1);

    while (
        poDS->nOverviewCount + 1 < numResolutions && (nW > 128 || nH > 128) &&
        (poDS->bUseSetDecodeArea || ((nTileW % 2) == 0 && (nTileH % 2) == 0)))
    {
        // This must be this exact formula per the JPEG2000 standard
        nW = (nW + 1) / 2;
        nH = (nH + 1) / 2;

        poDS->papoOverviewDS = (JP2OPJLikeDataset<CODEC, BASE> **)CPLRealloc(
            poDS->papoOverviewDS, (poDS->nOverviewCount + 1) *
                                      sizeof(JP2OPJLikeDataset<CODEC, BASE> *));
        JP2OPJLikeDataset *poODS = new JP2OPJLikeDataset();
        poODS->m_osFilename = poDS->m_osFilename;
        poODS->nParentXSize = poDS->nRasterXSize;
        poODS->nParentYSize = poDS->nRasterYSize;
        poODS->SetDescription(poOpenInfo->pszFilename);
        poODS->iLevel = poDS->nOverviewCount + 1;
        poODS->bSingleTiled = poDS->bSingleTiled;
        poODS->bUseSetDecodeArea = poDS->bUseSetDecodeArea;
        poODS->nRedIndex = poDS->nRedIndex;
        poODS->nGreenIndex = poDS->nGreenIndex;
        poODS->nBlueIndex = poDS->nBlueIndex;
        poODS->nAlphaIndex = poDS->nAlphaIndex;
        if (!poDS->bUseSetDecodeArea)
        {
            nTileW /= 2;
            nTileH /= 2;
            nBlockXSize = (int)nTileW;
            nBlockYSize = (int)nTileH;
        }
        else
        {
            nBlockXSize = std::min(nW, (int)nTileW);
            nBlockYSize = std::min(nH, (int)nTileH);
        }

        poODS->eColorSpace = poDS->eColorSpace;
        poODS->nRasterXSize = nW;
        poODS->nRasterYSize = nH;
        poODS->nBands = poDS->nBands;
        poODS->fp_ = poDS->fp_;
        poODS->nCodeStreamStart = nCodeStreamStart;
        poODS->nCodeStreamLength = nCodeStreamLength;
        poODS->bIs420 = bIs420;

        if (poODS->bSingleTiled && poODS->bUseSetDecodeArea)
        {
            poODS->cache(poDS);
        }
        poODS->m_pnLastLevel = poDS->m_pnLastLevel;
        poODS->m_bStrict = poDS->m_bStrict;

        poODS->m_nX0 = poDS->m_nX0;
        poODS->m_nY0 = poDS->m_nY0;

        for (iBand = 1; iBand <= poDS->nBands; iBand++)
        {
            const bool bPromoteTo8Bit =
                iBand == poDS->nAlphaIndex + 1 &&
                localctx.psImage
                        ->comps[(poDS->nAlphaIndex == 0 && poDS->nBands > 1)
                                    ? 1
                                    : 0]
                        .prec == 8 &&
                localctx.psImage->comps[poDS->nAlphaIndex].prec == 1 &&
                CPLFetchBool(
                    poOpenInfo->papszOpenOptions, "1BIT_ALPHA_PROMOTION",
                    CPLTestBool(CPLGetConfigOption(
                        "JP2OPENJPEG_PROMOTE_1BIT_ALPHA_AS_8BIT", "YES")));

            poODS->SetBand(iBand,
                           new JP2OPJLikeRasterBand<CODEC, BASE>(
                               poODS, iBand, eDataType,
                               bPromoteTo8Bit
                                   ? 8
                                   : localctx.psImage->comps[iBand - 1].prec,
                               bPromoteTo8Bit, nBlockXSize, nBlockYSize));
        }

        poDS->papoOverviewDS[poDS->nOverviewCount++] = poODS;
    }

    poDS->openCompleteJP2(&localctx);

    /* -------------------------------------------------------------------- */
    /*      More metadata.                                                  */
    /* -------------------------------------------------------------------- */
    if (poDS->nBands > 1)
    {
        poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL",
                                           "IMAGE_STRUCTURE");
    }

    poOpenInfo->fpL = poDS->fp_;
    vsi_l_offset nCurOffset = VSIFTellL(poDS->fp_);
    poDS->LoadJP2Metadata(poOpenInfo);
    VSIFSeekL(poDS->fp_, nCurOffset, SEEK_SET);
    poOpenInfo->fpL = nullptr;

    poDS->bHasGeoreferencingAtOpening =
        (!poDS->m_oSRS.IsEmpty() || poDS->nGCPCount != 0 ||
         poDS->bGeoTransformValid);

    /* -------------------------------------------------------------------- */
    /*      Vector layers                                                   */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR)
    {
        poDS->LoadVectorLayers(CPLFetchBool(poOpenInfo->papszOpenOptions,
                                            "OPEN_REMOTE_GML", false));

        // If file opened in vector-only mode and there's no vector,
        // return
        if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
            poDS->GetLayerCount() == 0)
        {
            delete poDS;
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for overviews.                                            */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    return poDS;
}

/************************************************************************/
/*                           WriteBox()                                 */
/************************************************************************/

template <typename CODEC, typename BASE>
bool JP2OPJLikeDataset<CODEC, BASE>::WriteBox(VSILFILE *fp, GDALJP2Box *poBox)
{
    GUInt32 nLBox;
    GUInt32 nTBox;

    if (poBox == nullptr)
        return true;

    nLBox = (int)poBox->GetDataLength() + 8;
    nLBox = CPL_MSBWORD32(nLBox);

    memcpy(&nTBox, poBox->GetType(), 4);

    return VSIFWriteL(&nLBox, 4, 1, fp) == 1 &&
           VSIFWriteL(&nTBox, 4, 1, fp) == 1 &&
           VSIFWriteL(poBox->GetWritableData(), (int)poBox->GetDataLength(), 1,
                      fp) == 1;
}

/************************************************************************/
/*                         WriteGDALMetadataBox()                       */
/************************************************************************/

template <typename CODEC, typename BASE>
bool JP2OPJLikeDataset<CODEC, BASE>::WriteGDALMetadataBox(VSILFILE *fp,
                                                          GDALDataset *poSrcDS,
                                                          char **papszOptions)
{
    bool bRet = true;
    GDALJP2Box *poBox = GDALJP2Metadata::CreateGDALMultiDomainMetadataXMLBox(
        poSrcDS, CPLFetchBool(papszOptions, "MAIN_MD_DOMAIN_ONLY", false));
    if (poBox)
        bRet = WriteBox(fp, poBox);
    delete poBox;
    return bRet;
}

/************************************************************************/
/*                         WriteXMLBoxes()                              */
/************************************************************************/

template <typename CODEC, typename BASE>
bool JP2OPJLikeDataset<CODEC, BASE>::WriteXMLBoxes(VSILFILE *fp,
                                                   GDALDataset *poSrcDS)
{
    bool bRet = true;
    int nBoxes = 0;
    GDALJP2Box **papoBoxes = GDALJP2Metadata::CreateXMLBoxes(poSrcDS, &nBoxes);
    for (int i = 0; i < nBoxes; i++)
    {
        if (!WriteBox(fp, papoBoxes[i]))
            bRet = false;
        delete papoBoxes[i];
    }
    CPLFree(papoBoxes);
    return bRet;
}

/************************************************************************/
/*                           WriteXMPBox()                              */
/************************************************************************/

template <typename CODEC, typename BASE>
bool JP2OPJLikeDataset<CODEC, BASE>::WriteXMPBox(VSILFILE *fp,
                                                 GDALDataset *poSrcDS)
{
    bool bRet = true;
    GDALJP2Box *poBox = GDALJP2Metadata::CreateXMPBox(poSrcDS);
    if (poBox)
        bRet = WriteBox(fp, poBox);
    delete poBox;
    return bRet;
}

/************************************************************************/
/*                           WriteIPRBox()                              */
/************************************************************************/

template <typename CODEC, typename BASE>
bool JP2OPJLikeDataset<CODEC, BASE>::WriteIPRBox(VSILFILE *fp,
                                                 GDALDataset *poSrcDS)
{
    bool bRet = true;
    GDALJP2Box *poBox = GDALJP2Metadata::CreateIPRBox(poSrcDS);
    if (poBox)
        bRet = WriteBox(fp, poBox);
    delete poBox;
    return bRet;
}

/************************************************************************/
/*                         FloorPowerOfTwo()                            */
/************************************************************************/

static int FloorPowerOfTwo(int nVal)
{
    int nBits = 0;
    while (nVal > 1)
    {
        nBits++;
        nVal >>= 1;
    }
    return 1 << nBits;
}

/************************************************************************/
/*                          CreateCopy()                                */
/************************************************************************/

template <typename CODEC, typename BASE>
GDALDataset *JP2OPJLikeDataset<CODEC, BASE>::CreateCopy(
    const char *pszFilename, GDALDataset *poSrcDS, CPL_UNUSED int bStrict,
    char **papszOptions, GDALProgressFunc pfnProgress, void *pProgressData)

{
    int nBands = poSrcDS->GetRasterCount();
    int nXSize = poSrcDS->GetRasterXSize();
    int nYSize = poSrcDS->GetRasterYSize();

    if (nBands == 0 || nBands > 16384)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Unable to export files with %d bands. Must be >= 1 and <= 16384",
            nBands);
        return nullptr;
    }

    GDALColorTable *poCT = poSrcDS->GetRasterBand(1)->GetColorTable();
    if (poCT != nullptr && nBands != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "JP2 driver only supports a color table for a "
                 "single-band dataset");
        return nullptr;
    }

    GDALDataType eDataType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    int nDataTypeSize = (GDALGetDataTypeSize(eDataType) / 8);
    if (eDataType != GDT_Byte && eDataType != GDT_Int16 &&
        eDataType != GDT_UInt16 && eDataType != GDT_Int32 &&
        eDataType != GDT_UInt32)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "JP2 driver only supports creating Byte, GDT_Int16, "
                 "GDT_UInt16, GDT_Int32, GDT_UInt32");
        return nullptr;
    }

    const bool bInspireTG = CPLFetchBool(papszOptions, "INSPIRE_TG", false);

    /* -------------------------------------------------------------------- */
    /*      Analyze creation options.                                       */
    /* -------------------------------------------------------------------- */
    auto eCodecFormat = CODEC::cvtenum(JP2_CODEC_J2K);
    const char *pszCodec = CSLFetchNameValueDef(papszOptions, "CODEC", nullptr);
    if (pszCodec)
    {
        if (EQUAL(pszCodec, "JP2"))
            eCodecFormat = CODEC::cvtenum(JP2_CODEC_JP2);
        else if (EQUAL(pszCodec, "J2K"))
            eCodecFormat = CODEC::cvtenum(JP2_CODEC_J2K);
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for CODEC : %s. Defaulting to J2K",
                     pszCodec);
        }
    }
    else
    {
        if (strlen(pszFilename) > 4 &&
            EQUAL(pszFilename + strlen(pszFilename) - 4, ".JP2"))
        {
            eCodecFormat = CODEC::cvtenum(JP2_CODEC_JP2);
        }
    }
    if (eCodecFormat != CODEC::cvtenum(JP2_CODEC_JP2) && bInspireTG)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "INSPIRE_TG=YES mandates CODEC=JP2 (TG requirement 21)");
        return nullptr;
    }

    // NOTE: if changing the default block size, the logic in nitfdataset.cpp
    // CreateCopy() will have to be changed as well.
    int nBlockXSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKXSIZE", "1024"));
    int nBlockYSize =
        atoi(CSLFetchNameValueDef(papszOptions, "BLOCKYSIZE", "1024"));
    if (nBlockXSize <= 0 || nBlockYSize <= 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid block size");
        return nullptr;
    }

    // By default do not generate tile sizes larger than the dataset
    // dimensions
    if (!CPLFetchBool(papszOptions, "BLOCKSIZE_STRICT", false) &&
        !CPLFetchBool(papszOptions, "@BLOCKSIZE_STRICT", false))
    {
        if (nBlockXSize < 32 || nBlockYSize < 32)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Invalid block size");
            return nullptr;
        }

        if (nXSize < nBlockXSize)
        {
            CPLDebug(CODEC::debugId(), "Adjusting block width from %d to %d",
                     nBlockXSize, nXSize);
            nBlockXSize = nXSize;
        }
        if (nYSize < nBlockYSize)
        {
            CPLDebug(CODEC::debugId(), "Adjusting block width from %d to %d",
                     nBlockYSize, nYSize);
            nBlockYSize = nYSize;
        }
    }

    JP2_PROG_ORDER eProgOrder = JP2_LRCP;
    const char *pszPROGORDER =
        CSLFetchNameValueDef(papszOptions, "PROGRESSION", "LRCP");
    if (EQUAL(pszPROGORDER, "LRCP"))
        eProgOrder = JP2_LRCP;
    else if (EQUAL(pszPROGORDER, "RLCP"))
        eProgOrder = JP2_RLCP;
    else if (EQUAL(pszPROGORDER, "RPCL"))
        eProgOrder = JP2_RPCL;
    else if (EQUAL(pszPROGORDER, "PCRL"))
        eProgOrder = JP2_PCRL;
    else if (EQUAL(pszPROGORDER, "CPRL"))
        eProgOrder = JP2_CPRL;
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unsupported value for PROGRESSION : %s. Defaulting to LRCP",
                 pszPROGORDER);
    }

    const bool bIsIrreversible =
        !CPLFetchBool(papszOptions, "REVERSIBLE", poCT != nullptr);

    std::vector<double> adfRates;
    const char *pszQuality =
        CSLFetchNameValueDef(papszOptions, "QUALITY", nullptr);
    double dfDefaultQuality = (poCT != nullptr) ? 100.0 : 25.0;
    if (pszQuality)
    {
        char **papszTokens =
            CSLTokenizeStringComplex(pszQuality, ",", FALSE, FALSE);
        for (int i = 0; papszTokens[i] != nullptr; i++)
        {
            double dfQuality = CPLAtof(papszTokens[i]);
            if (dfQuality > 0 && dfQuality <= 100)
            {
                double dfRate = 100 / dfQuality;
                adfRates.push_back(dfRate);
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Unsupported value for QUALITY: %s. Defaulting to "
                         "single-layer, with quality=%.0f",
                         papszTokens[i], dfDefaultQuality);
                adfRates.resize(0);
                break;
            }
        }
        if (papszTokens[0] == nullptr)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for QUALITY: %s. Defaulting to "
                     "single-layer, with quality=%.0f",
                     pszQuality, dfDefaultQuality);
        }
        CSLDestroy(papszTokens);
    }
    if (adfRates.empty())
    {
        adfRates.push_back(100. / dfDefaultQuality);
        assert(!adfRates.empty());
    }

    if (poCT != nullptr && (bIsIrreversible || adfRates.back() != 1.0))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Encoding a dataset with a color table with REVERSIBLE != YES "
                 "or QUALITY != 100 will likely lead to bad visual results");
    }

    const int nMaxTileDim = std::max(nBlockXSize, nBlockYSize);
    const int nMinTileDim = std::min(nBlockXSize, nBlockYSize);
    int nNumResolutions = 1;
    /* Pickup a reasonable value compatible with PROFILE_1 requirements */
    while ((nMaxTileDim >> (nNumResolutions - 1)) > 128 &&
           (nMinTileDim >> nNumResolutions) > 0)
        nNumResolutions++;
    int nMinProfile1Resolutions = nNumResolutions;
    const char *pszResolutions =
        CSLFetchNameValueDef(papszOptions, "RESOLUTIONS", nullptr);
    if (pszResolutions)
    {
        nNumResolutions = atoi(pszResolutions);
        if (nNumResolutions <= 0 || nNumResolutions >= 32 ||
            (nMinTileDim >> nNumResolutions) == 0 ||
            (nMaxTileDim >> nNumResolutions) == 0)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for RESOLUTIONS : %s. Defaulting to %d",
                     pszResolutions, nMinProfile1Resolutions);
            nNumResolutions = nMinProfile1Resolutions;
        }
    }
    int nRedBandIndex = -1;
    int nGreenBandIndex = -1;
    int nBlueBandIndex = -1;
    int nAlphaBandIndex = -1;
    for (int i = 0; i < nBands; i++)
    {
        GDALColorInterp eInterp =
            poSrcDS->GetRasterBand(i + 1)->GetColorInterpretation();
        if (eInterp == GCI_RedBand)
            nRedBandIndex = i;
        else if (eInterp == GCI_GreenBand)
            nGreenBandIndex = i;
        else if (eInterp == GCI_BlueBand)
            nBlueBandIndex = i;
        else if (eInterp == GCI_AlphaBand)
            nAlphaBandIndex = i;
    }
    const char *pszAlpha = CSLFetchNameValue(papszOptions, "ALPHA");
    if (nAlphaBandIndex < 0 && nBands > 1 && pszAlpha != nullptr &&
        CPLTestBool(pszAlpha))
    {
        nAlphaBandIndex = nBands - 1;
    }

    const char *pszYCBCR420 = CSLFetchNameValue(papszOptions, "YCBCR420");
    int bYCBCR420 = FALSE;
    if (pszYCBCR420 && CPLTestBool(pszYCBCR420))
    {
        if ((nBands == 3 || nBands == 4) && eDataType == GDT_Byte &&
            nRedBandIndex == 0 && nGreenBandIndex == 1 && nBlueBandIndex == 2)
        {
            if (((nXSize % 2) == 0 && (nYSize % 2) == 0 &&
                 (nBlockXSize % 2) == 0 && (nBlockYSize % 2) == 0))
            {
                bYCBCR420 = TRUE;
            }
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "YCBCR420 unsupported when image size and/or tile "
                         "size are not multiple of 2");
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "YCBCR420 unsupported with this image band count and/or "
                     "data byte");
        }
    }

    const char *pszYCC = CSLFetchNameValue(papszOptions, "YCC");
    int bYCC = ((nBands == 3 || nBands == 4) &&
                CPLTestBool(CSLFetchNameValueDef(papszOptions, "YCC", "TRUE")));

    if (bYCBCR420 && bYCC)
    {
        if (pszYCC != nullptr)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "YCC unsupported when YCbCr requesting");
        }
        bYCC = FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Deal with codeblocks size                                       */
    /* -------------------------------------------------------------------- */

    int nCblockW =
        atoi(CSLFetchNameValueDef(papszOptions, "CODEBLOCK_WIDTH", "64"));
    int nCblockH =
        atoi(CSLFetchNameValueDef(papszOptions, "CODEBLOCK_HEIGHT", "64"));
    if (nCblockW < 4 || nCblockW > 1024 || nCblockH < 4 || nCblockH > 1024)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Invalid values for codeblock size. Defaulting to 64x64");
        nCblockW = 64;
        nCblockH = 64;
    }
    else if (nCblockW * nCblockH > 4096)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Invalid values for codeblock size. "
                 "CODEBLOCK_WIDTH * CODEBLOCK_HEIGHT should be <= 4096. "
                 "Defaulting to 64x64");
        nCblockW = 64;
        nCblockH = 64;
    }
    int nCblockW_po2 = FloorPowerOfTwo(nCblockW);
    int nCblockH_po2 = FloorPowerOfTwo(nCblockH);
    if (nCblockW_po2 != nCblockW || nCblockH_po2 != nCblockH)
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Non power of two values used for codeblock size. "
                 "Using to %dx%d",
                 nCblockW_po2, nCblockH_po2);
    }
    nCblockW = nCblockW_po2;
    nCblockH = nCblockH_po2;

    /* -------------------------------------------------------------------- */
    /*      Deal with codestream PROFILE                                    */
    /* -------------------------------------------------------------------- */
    const char *pszProfile =
        CSLFetchNameValueDef(papszOptions, "PROFILE", "AUTO");
    int bProfile1 = FALSE;
    if (EQUAL(pszProfile, "UNRESTRICTED"))
    {
        bProfile1 = FALSE;
        if (bInspireTG)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "INSPIRE_TG=YES mandates PROFILE=PROFILE_1 (TG "
                     "requirement 21)");
            return nullptr;
        }
    }
    else if (EQUAL(pszProfile, "UNRESTRICTED_FORCED"))
    {
        bProfile1 = FALSE;
    }
    else if (EQUAL(pszProfile,
                   "PROFILE_1_FORCED")) /* For debug only: can produce
                                           inconsistent codestream */
    {
        bProfile1 = TRUE;
    }
    else
    {
        if (!(EQUAL(pszProfile, "PROFILE_1") || EQUAL(pszProfile, "AUTO")))
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported value for PROFILE : %s. Defaulting to AUTO",
                     pszProfile);
            pszProfile = "AUTO";
        }

        bProfile1 = TRUE;
        const char *pszReq21OrEmpty = bInspireTG ? " (TG requirement 21)" : "";
        if ((nBlockXSize != nXSize || nBlockYSize != nYSize) &&
            (nBlockXSize != nBlockYSize || nBlockXSize > 1024 ||
             nBlockYSize > 1024))
        {
            bProfile1 = FALSE;
            if (bInspireTG || EQUAL(pszProfile, "PROFILE_1"))
            {
                CPLError(
                    CE_Failure, CPLE_NotSupported,
                    "Tile dimensions incompatible with PROFILE_1%s. "
                    "Should be whole image or square with dimension <= 1024.",
                    pszReq21OrEmpty);
                return nullptr;
            }
        }
        if ((nMaxTileDim >> (nNumResolutions - 1)) > 128)
        {
            bProfile1 = FALSE;
            if (bInspireTG || EQUAL(pszProfile, "PROFILE_1"))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Number of resolutions incompatible with PROFILE_1%s. "
                         "Should be at least %d.",
                         pszReq21OrEmpty, nMinProfile1Resolutions);
                return nullptr;
            }
        }
        if (nCblockW > 64 || nCblockH > 64)
        {
            bProfile1 = FALSE;
            if (bInspireTG || EQUAL(pszProfile, "PROFILE_1"))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Codeblock width incompatible with PROFILE_1%s. "
                         "Codeblock width or height should be <= 64.",
                         pszReq21OrEmpty);
                return nullptr;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Work out the precision.                                         */
    /* -------------------------------------------------------------------- */
    int nBits;
    if (CSLFetchNameValue(papszOptions, "NBITS") != nullptr)
    {
        nBits = atoi(CSLFetchNameValue(papszOptions, "NBITS"));
        if (bInspireTG &&
            !(nBits == 1 || nBits == 8 || nBits == 16 || nBits == 32))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "INSPIRE_TG=YES mandates NBITS=1,8,16 or 32 (TG "
                     "requirement 24)");
            return nullptr;
        }
    }
    else if (poSrcDS->GetRasterBand(1)->GetMetadataItem(
                 "NBITS", "IMAGE_STRUCTURE") != nullptr)
    {
        nBits = atoi(poSrcDS->GetRasterBand(1)->GetMetadataItem(
            "NBITS", "IMAGE_STRUCTURE"));
        if (bInspireTG &&
            !(nBits == 1 || nBits == 8 || nBits == 16 || nBits == 32))
        {
            /* Implements "NOTE If the original data do not satisfy this "
               "requirement, they will be converted in a representation using "
               "the next higher power of 2" */
            nBits = GDALGetDataTypeSize(eDataType);
        }
    }
    else
    {
        nBits = GDALGetDataTypeSize(eDataType);
    }

    if ((GDALGetDataTypeSize(eDataType) == 8 && nBits > 8) ||
        (GDALGetDataTypeSize(eDataType) == 16 && (nBits <= 8 || nBits > 16)) ||
        (GDALGetDataTypeSize(eDataType) == 32 && (nBits <= 16 || nBits > 32)))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Inconsistent NBITS value with data type. Using %d",
                 GDALGetDataTypeSize(eDataType));
    }

    /* -------------------------------------------------------------------- */
    /*      Georeferencing options                                          */
    /* -------------------------------------------------------------------- */

    bool bGMLJP2Option = CPLFetchBool(papszOptions, "GMLJP2", true);
    int nGMLJP2Version = 1;
    const char *pszGMLJP2V2Def =
        CSLFetchNameValue(papszOptions, "GMLJP2V2_DEF");
    if (pszGMLJP2V2Def != nullptr)
    {
        bGMLJP2Option = true;
        nGMLJP2Version = 2;
        if (bInspireTG)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "INSPIRE_TG=YES is only compatible with GMLJP2 v1");
            return nullptr;
        }
    }
    const bool bGeoJP2Option = CPLFetchBool(papszOptions, "GeoJP2", true);

    GDALJP2Metadata oJP2MD;

    int bGeoreferencingCompatOfGeoJP2 = FALSE;
    int bGeoreferencingCompatOfGMLJP2 = FALSE;
    if (eCodecFormat == CODEC::cvtenum(JP2_CODEC_JP2) &&
        (bGMLJP2Option || bGeoJP2Option))
    {
        if (poSrcDS->GetGCPCount() > 0)
        {
            bGeoreferencingCompatOfGeoJP2 = TRUE;
            oJP2MD.SetGCPs(poSrcDS->GetGCPCount(), poSrcDS->GetGCPs());
            oJP2MD.SetSpatialRef(poSrcDS->GetGCPSpatialRef());
        }
        else
        {
            const OGRSpatialReference *poSRS = poSrcDS->GetSpatialRef();
            if (poSRS)
            {
                bGeoreferencingCompatOfGeoJP2 = TRUE;
                oJP2MD.SetSpatialRef(poSRS);
            }
            double adfGeoTransform[6];
            if (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None)
            {
                bGeoreferencingCompatOfGeoJP2 = TRUE;
                oJP2MD.SetGeoTransform(adfGeoTransform);
                if (poSRS && !poSRS->IsEmpty())
                {
                    bGeoreferencingCompatOfGMLJP2 =
                        GDALJP2Metadata::IsSRSCompatible(poSRS);
                    if (!bGeoreferencingCompatOfGMLJP2)
                    {
                        CPLDebug(
                            CODEC::debugId(),
                            "Cannot write GMLJP2 box due to unsupported SRS");
                    }
                }
            }
        }
        if (poSrcDS->GetMetadata("RPC") != nullptr)
        {
            oJP2MD.SetRPCMD(poSrcDS->GetMetadata("RPC"));
            bGeoreferencingCompatOfGeoJP2 = TRUE;
        }

        const char *pszAreaOrPoint =
            poSrcDS->GetMetadataItem(GDALMD_AREA_OR_POINT);
        oJP2MD.bPixelIsPoint = pszAreaOrPoint != nullptr &&
                               EQUAL(pszAreaOrPoint, GDALMD_AOP_POINT);

        if (bGMLJP2Option &&
            CPLGetConfigOption("GMLJP2OVERRIDE", nullptr) != nullptr)
        {
            // Force V1 since this is the branch in which the hack is
            // implemented
            nGMLJP2Version = 1;
            bGeoreferencingCompatOfGMLJP2 = TRUE;
        }
    }

    if (CSLFetchNameValue(papszOptions, "GMLJP2") != nullptr && bGMLJP2Option &&
        !bGeoreferencingCompatOfGMLJP2)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GMLJP2 box was explicitly required but cannot be written due "
                 "to lack of georeferencing and/or unsupported georeferencing "
                 "for GMLJP2");
    }

    if (CSLFetchNameValue(papszOptions, "GeoJP2") != nullptr && bGeoJP2Option &&
        !bGeoreferencingCompatOfGeoJP2)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "GeoJP2 box was explicitly required but cannot be written due "
                 "to lack of georeferencing");
    }
    const bool bGeoBoxesAfter =
        CPLFetchBool(papszOptions, "GEOBOXES_AFTER_JP2C", bInspireTG);
    GDALJP2Box *poGMLJP2Box = nullptr;
    if (eCodecFormat == CODEC::cvtenum(JP2_CODEC_JP2) && bGMLJP2Option &&
        bGeoreferencingCompatOfGMLJP2)
    {
        if (nGMLJP2Version == 1)
            poGMLJP2Box = oJP2MD.CreateGMLJP2(nXSize, nYSize);
        else
            poGMLJP2Box =
                oJP2MD.CreateGMLJP2V2(nXSize, nYSize, pszGMLJP2V2Def, poSrcDS);
        if (poGMLJP2Box == nullptr)
            return nullptr;
    }

    /* ---------------------------------------------------------------- */
    /* If the input driver is identifed as "GEORASTER" the following    */
    /* section will try to dump a ORACLE GeoRaster JP2 BLOB into a file */
    /* ---------------------------------------------------------------- */

    if (EQUAL(poSrcDS->GetDriverName(), "GEORASTER"))
    {
        const char *pszGEOR_compress =
            poSrcDS->GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE");

        if (pszGEOR_compress == nullptr)
        {
            pszGEOR_compress = "NONE";
        }

        /* Check if the JP2 BLOB needs re-shaping */

        bool bGEOR_reshape = false;

        const char *apszIgnoredOptions[] = {"BLOCKXSIZE",
                                            "BLOCKYSIZE",
                                            "QUALITY",
                                            "REVERSIBLE",
                                            "RESOLUTIONS",
                                            "PROGRESSION",
                                            "SOP",
                                            "EPH",
                                            "YCBCR420",
                                            "YCC",
                                            "NBITS",
                                            "1BIT_ALPHA",
                                            "PRECINCTS",
                                            "TILEPARTS",
                                            "CODEBLOCK_WIDTH",
                                            "CODEBLOCK_HEIGHT",
                                            "PLT",
                                            "TLM",
                                            nullptr};

        for (int i = 0; apszIgnoredOptions[i]; i++)
        {
            if (CSLFetchNameValue(papszOptions, apszIgnoredOptions[i]))
            {
                bGEOR_reshape = true;
            }
        }

        if (CSLFetchNameValue(papszOptions, "USE_SRC_CODESTREAM"))
        {
            bGEOR_reshape = false;
        }

        char **papszGEOR_files = poSrcDS->GetFileList();

        if (EQUAL(pszGEOR_compress, "JP2-F") && CSLCount(papszGEOR_files) > 0 &&
            bGEOR_reshape == false)
        {

            const char *pszVsiOciLob = papszGEOR_files[0];

            VSILFILE *fpBlob = VSIFOpenL(pszVsiOciLob, "r");
            if (fpBlob == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s",
                         pszVsiOciLob);
                delete poGMLJP2Box;
                return nullptr;
            }
            VSILFILE *fp = VSIFOpenL(pszFilename, "w+b");
            if (fp == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s",
                         pszFilename);
                delete poGMLJP2Box;
                VSIFCloseL(fpBlob);
                return nullptr;
            }

            VSIFSeekL(fpBlob, 0, SEEK_END);

            size_t nBlobSize = static_cast<size_t>(VSIFTellL(fpBlob));
            size_t nChunk = (size_t)(GDALGetCacheMax() * 0.25);
            size_t nSize = 0;
            size_t nCount = 0;

            void *pBuffer = (GByte *)VSI_MALLOC_VERBOSE(nChunk);
            if (pBuffer == nullptr)
            {
                delete poGMLJP2Box;
                VSIFCloseL(fpBlob);
                VSIFCloseL(fp);
                return nullptr;
            }

            VSIFSeekL(fpBlob, 0, SEEK_SET);

            while ((nSize = VSIFReadL(pBuffer, 1, nChunk, fpBlob)) > 0)
            {
                VSIFWriteL(pBuffer, 1, nSize, fp);
                nCount += nSize;
                pfnProgress((float)nCount / (float)nBlobSize, nullptr,
                            pProgressData);
            }

            CPLFree(pBuffer);
            VSIFCloseL(fpBlob);

            VSIFCloseL(fp);

            /* Return the GDALDaset object */

            GDALOpenInfo oOpenInfo(pszFilename, GA_Update);
            GDALDataset *poDS = JP2OPJLikeDataset::Open(&oOpenInfo);

            /* Copy essential metadata */

            double adfGeoTransform[6];

            if (poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None)
            {
                poDS->SetGeoTransform(adfGeoTransform);
            }

            const OGRSpatialReference *poSRS = poSrcDS->GetSpatialRef();
            if (poSRS)
            {
                poDS->SetSpatialRef(poSRS);
            }

            delete poGMLJP2Box;
            return poDS;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Setup encoder                                                   */
    /* -------------------------------------------------------------------- */

    JP2OPJLikeDataset oTmpDS;
    int numThreads = oTmpDS.GetNumThreads();

    CODEC localctx;
    localctx.allocComponentParams(nBands);
    int iBand;
    int bSamePrecision = TRUE;
    int b1BitAlpha = FALSE;
    for (iBand = 0; iBand < nBands; iBand++)
    {
        localctx.pasBandParams[iBand].x0 = 0;
        localctx.pasBandParams[iBand].y0 = 0;
        if (bYCBCR420 && (iBand == 1 || iBand == 2))
        {
            localctx.pasBandParams[iBand].dx = 2;
            localctx.pasBandParams[iBand].dy = 2;
            localctx.pasBandParams[iBand].w = nXSize / 2;
            localctx.pasBandParams[iBand].h = nYSize / 2;
        }
        else
        {
            localctx.pasBandParams[iBand].dx = 1;
            localctx.pasBandParams[iBand].dy = 1;
            localctx.pasBandParams[iBand].w = nXSize;
            localctx.pasBandParams[iBand].h = nYSize;
        }

        localctx.pasBandParams[iBand].sgnd =
            (eDataType == GDT_Int16 || eDataType == GDT_Int32);
        localctx.pasBandParams[iBand].prec = nBits;

        const char *pszNBits =
            poSrcDS->GetRasterBand(iBand + 1)->GetMetadataItem(
                "NBITS", "IMAGE_STRUCTURE");
        /* Recommendation 38 In the case of an opacity channel, the bit depth
         * should be 1-bit. */
        if (iBand == nAlphaBandIndex &&
            ((pszNBits != nullptr && EQUAL(pszNBits, "1")) ||
             CPLFetchBool(papszOptions, "1BIT_ALPHA", bInspireTG)))
        {
            if (iBand != nBands - 1 && nBits != 1)
            {
                /* Might be a bug in openjpeg, but it seems that if the alpha */
                /* band is the first one, it would select 1-bit for all
                 * channels... */
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Cannot output 1-bit alpha channel if it is not the "
                         "last one");
            }
            else
            {
                CPLDebug(CODEC::debugId(), "Using 1-bit alpha channel");
                localctx.pasBandParams[iBand].sgnd = 0;
                localctx.pasBandParams[iBand].prec = 1;
                bSamePrecision = FALSE;
                b1BitAlpha = TRUE;
            }
        }
    }

    if (bInspireTG && nAlphaBandIndex >= 0 && !b1BitAlpha)
    {
        CPLError(
            CE_Warning, CPLE_NotSupported,
            "INSPIRE_TG=YES recommends 1BIT_ALPHA=YES (Recommendation 38)");
    }
    auto eColorSpace = CODEC::cvtenum(JP2_CLRSPC_GRAY);

    if (bYCBCR420)
    {
        eColorSpace = CODEC::cvtenum(JP2_CLRSPC_SYCC);
    }
    else if ((nBands == 3 || nBands == 4) && nRedBandIndex >= 0 &&
             nGreenBandIndex >= 0 && nBlueBandIndex >= 0)
    {
        eColorSpace = CODEC::cvtenum(JP2_CLRSPC_SRGB);
    }
    else if (poCT != nullptr)
    {
        eColorSpace = CODEC::cvtenum(JP2_CLRSPC_SRGB);
    }

    /* -------------------------------------------------------------------- */
    /*      Create the dataset.                                             */
    /* -------------------------------------------------------------------- */

    const char *pszAccess =
        STARTS_WITH_CI(pszFilename, "/vsisubfile/") ? "r+b" : "w+b";
    VSILFILE *fp = VSIFOpenL(pszFilename, pszAccess);
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create file");
        CPLFree(localctx.pasBandParams);
        localctx.pasBandParams = nullptr;
        delete poGMLJP2Box;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Add JP2 boxes.                                                  */
    /* -------------------------------------------------------------------- */
    vsi_l_offset nStartJP2C = 0;
    int bUseXLBoxes = FALSE;

    if (eCodecFormat == CODEC::cvtenum(JP2_CODEC_JP2))
    {
        GDALJP2Box jPBox(fp);
        jPBox.SetType("jP  ");
        jPBox.AppendWritableData(4, "\x0D\x0A\x87\x0A");
        WriteBox(fp, &jPBox);

        GDALJP2Box ftypBox(fp);
        ftypBox.SetType("ftyp");
        // http://docs.opengeospatial.org/is/08-085r5/08-085r5.html Req 19
        const bool bJPXOption = CPLFetchBool(papszOptions, "JPX", true);
        if (nGMLJP2Version == 2 && bJPXOption)
            ftypBox.AppendWritableData(4, "jpx "); /* Branding */
        else
            ftypBox.AppendWritableData(4, "jp2 "); /* Branding */
        ftypBox.AppendUInt32(0);                   /* minimum version */
        ftypBox.AppendWritableData(
            4, "jp2 "); /* Compatibility list: first value */

        if (bInspireTG && poGMLJP2Box != nullptr && !bJPXOption)
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "INSPIRE_TG=YES implies following GMLJP2 specification which "
                "recommends advertise reader requirement 67 feature, and thus "
                "JPX capability");
        }
        else if (poGMLJP2Box != nullptr && bJPXOption)
        {
            /* GMLJP2 uses lbl and asoc boxes, which are JPEG2000 Part II spec
             */
            /* advertizing jpx is required per 8.1 of 05-047r3 GMLJP2 */
            ftypBox.AppendWritableData(
                4, "jpx "); /* Compatibility list: second value */
        }
        WriteBox(fp, &ftypBox);

        const bool bIPR = poSrcDS->GetMetadata("xml:IPR") != nullptr &&
                          CPLFetchBool(papszOptions, "WRITE_METADATA", false);

        /* Reader requirement box */
        if (poGMLJP2Box != nullptr && bJPXOption)
        {
            GDALJP2Box rreqBox(fp);
            rreqBox.SetType("rreq");
            rreqBox.AppendUInt8(1); /* ML = 1 byte for mask length */

            rreqBox.AppendUInt8(0x80 | 0x40 | (bIPR ? 0x20 : 0)); /* FUAM */
            rreqBox.AppendUInt8(0x80);                            /* DCM */

            rreqBox.AppendUInt16(
                2 + (bIPR ? 1 : 0)); /* NSF: Number of standard features */

            rreqBox.AppendUInt16(
                (bProfile1) ? 4 : 5);  /* SF0 : PROFILE 1 or PROFILE 2 */
            rreqBox.AppendUInt8(0x80); /* SM0 */

            rreqBox.AppendUInt16(67);  /* SF1 : GMLJP2 box */
            rreqBox.AppendUInt8(0x40); /* SM1 */

            if (bIPR)
            {
                rreqBox.AppendUInt16(35);  /* SF2 : IPR metadata */
                rreqBox.AppendUInt8(0x20); /* SM2 */
            }
            rreqBox.AppendUInt16(0); /* NVF */
            WriteBox(fp, &rreqBox);
        }

        GDALJP2Box ihdrBox(fp);
        ihdrBox.SetType("ihdr");
        ihdrBox.AppendUInt32(nYSize);
        ihdrBox.AppendUInt32(nXSize);
        ihdrBox.AppendUInt16(static_cast<GUInt16>(nBands));
        GByte BPC;
        if (bSamePrecision)
            BPC = static_cast<GByte>((localctx.pasBandParams[0].prec - 1) |
                                     (localctx.pasBandParams[0].sgnd << 7));
        else
            BPC = 255;
        ihdrBox.AppendUInt8(BPC);
        ihdrBox.AppendUInt8(7); /* C=Compression type: fixed value */
        ihdrBox.AppendUInt8(0); /* UnkC: 0= colourspace of the image is known */
        /*and correctly specified in the Colourspace Specification boxes within
         * the file */
        ihdrBox.AppendUInt8(
            bIPR ? 1 : 0); /* IPR: 0=no intellectual property, 1=IPR box */

        GDALJP2Box bpccBox(fp);
        if (!bSamePrecision)
        {
            bpccBox.SetType("bpcc");
            for (int i = 0; i < nBands; i++)
                bpccBox.AppendUInt8(
                    static_cast<GByte>((localctx.pasBandParams[i].prec - 1) |
                                       (localctx.pasBandParams[i].sgnd << 7)));
        }

        GDALJP2Box colrBox(fp);
        colrBox.SetType("colr");
        colrBox.AppendUInt8(1); /* METHOD: 1=Enumerated Colourspace */
        colrBox.AppendUInt8(
            0); /* PREC: Precedence. 0=(field reserved for ISO use) */
        colrBox.AppendUInt8(0); /* APPROX: Colourspace approximation. */
        GUInt32 enumcs = 16;
        if (eColorSpace == CODEC::cvtenum(JP2_CLRSPC_SRGB))
            enumcs = 16;
        else if (eColorSpace == CODEC::cvtenum(JP2_CLRSPC_GRAY))
            enumcs = 17;
        else if (eColorSpace == CODEC::cvtenum(JP2_CLRSPC_SYCC))
            enumcs = 18;
        colrBox.AppendUInt32(enumcs); /* EnumCS: Enumerated colourspace */

        GDALJP2Box pclrBox(fp);
        GDALJP2Box cmapBox(fp);
        int nCTComponentCount = 0;
        if (poCT != nullptr)
        {
            pclrBox.SetType("pclr");
            const int nEntries = std::min(256, poCT->GetColorEntryCount());
            nCTComponentCount =
                atoi(CSLFetchNameValueDef(papszOptions, "CT_COMPONENTS", "0"));
            if (bInspireTG)
            {
                if (nCTComponentCount != 0 && nCTComponentCount != 3)
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Inspire TG mandates 3 components for color table");
                else
                    nCTComponentCount = 3;
            }
            else if (nCTComponentCount != 3 && nCTComponentCount != 4)
            {
                nCTComponentCount = 3;
                for (int i = 0; i < nEntries; i++)
                {
                    const GDALColorEntry *psEntry = poCT->GetColorEntry(i);
                    if (psEntry->c4 != 255)
                    {
                        CPLDebug(
                            CODEC::debugId(),
                            "Color table has at least one non-opaque value. "
                            "This may cause compatibility problems with some "
                            "readers. "
                            "In which case use CT_COMPONENTS=3 creation "
                            "option");
                        nCTComponentCount = 4;
                        break;
                    }
                }
            }
            nRedBandIndex = 0;
            nGreenBandIndex = 1;
            nBlueBandIndex = 2;
            nAlphaBandIndex = (nCTComponentCount == 4) ? 3 : -1;

            pclrBox.AppendUInt16(static_cast<GUInt16>(nEntries));
            pclrBox.AppendUInt8(static_cast<GByte>(
                nCTComponentCount)); /* NPC: Number of components */
            for (int i = 0; i < nCTComponentCount; i++)
            {
                pclrBox.AppendUInt8(7); /* Bi: unsigned 8 bits */
            }
            for (int i = 0; i < nEntries; i++)
            {
                const GDALColorEntry *psEntry = poCT->GetColorEntry(i);
                pclrBox.AppendUInt8((GByte)psEntry->c1);
                pclrBox.AppendUInt8((GByte)psEntry->c2);
                pclrBox.AppendUInt8((GByte)psEntry->c3);
                if (nCTComponentCount == 4)
                    pclrBox.AppendUInt8((GByte)psEntry->c4);
            }

            cmapBox.SetType("cmap");
            for (int i = 0; i < nCTComponentCount; i++)
            {
                cmapBox.AppendUInt16(0); /* CMPi: code stream component index */
                cmapBox.AppendUInt8(1);  /* MYTPi: 1=palette mapping */
                cmapBox.AppendUInt8(static_cast<GByte>(
                    i)); /* PCOLi: index component from the map */
            }
        }

        GDALJP2Box cdefBox(fp);
        if (((nBands == 3 || nBands == 4) &&
             (eColorSpace == CODEC::cvtenum(JP2_CLRSPC_SRGB) ||
              eColorSpace == CODEC::cvtenum(JP2_CLRSPC_SYCC)) &&
             (nRedBandIndex != 0 || nGreenBandIndex != 1 ||
              nBlueBandIndex != 2)) ||
            nAlphaBandIndex >= 0)
        {
            cdefBox.SetType("cdef");
            int nComponents = (nCTComponentCount == 4) ? 4 : nBands;
            cdefBox.AppendUInt16(static_cast<GUInt16>(nComponents));
            for (int i = 0; i < nComponents; i++)
            {
                cdefBox.AppendUInt16(
                    static_cast<GUInt16>(i)); /* Component number */
                if (i != nAlphaBandIndex)
                {
                    cdefBox.AppendUInt16(
                        0); /* Signification: This channel is the colour image
                               data for the associated colour */
                    if (eColorSpace == CODEC::cvtenum(JP2_CLRSPC_GRAY) &&
                        nComponents == 2)
                        cdefBox.AppendUInt16(
                            1); /* Colour of the component: associated with a
                                   particular colour */
                    else if ((eColorSpace == CODEC::cvtenum(JP2_CLRSPC_SRGB) ||
                              eColorSpace == CODEC::cvtenum(JP2_CLRSPC_SYCC)) &&
                             (nComponents == 3 || nComponents == 4))
                    {
                        if (i == nRedBandIndex)
                            cdefBox.AppendUInt16(1);
                        else if (i == nGreenBandIndex)
                            cdefBox.AppendUInt16(2);
                        else if (i == nBlueBandIndex)
                            cdefBox.AppendUInt16(3);
                        else
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Could not associate band %d with a "
                                     "red/green/blue channel",
                                     i + 1);
                            cdefBox.AppendUInt16(65535);
                        }
                    }
                    else
                        cdefBox.AppendUInt16(
                            65535); /* Colour of the component: not associated
                                       with any particular colour */
                }
                else
                {
                    cdefBox.AppendUInt16(
                        1); /* Signification: Non pre-multiplied alpha */
                    cdefBox.AppendUInt16(
                        0); /* Colour of the component: This channel is
                               associated as the image as a whole */
                }
            }
        }

        // Add res box if needed
        GDALJP2Box *poRes = nullptr;
        if (poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION") != nullptr &&
            poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION") != nullptr &&
            poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT") != nullptr)
        {
            double dfXRes =
                CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_XRESOLUTION"));
            double dfYRes =
                CPLAtof(poSrcDS->GetMetadataItem("TIFFTAG_YRESOLUTION"));
            int nResUnit =
                atoi(poSrcDS->GetMetadataItem("TIFFTAG_RESOLUTIONUNIT"));
#define PIXELS_PER_INCH 2
#define PIXELS_PER_CM 3

            if (nResUnit == PIXELS_PER_INCH)
            {
                // convert pixels per inch to pixels per cm.
                dfXRes = dfXRes * 39.37 / 100.0;
                dfYRes = dfYRes * 39.37 / 100.0;
                nResUnit = PIXELS_PER_CM;
            }

            if (nResUnit == PIXELS_PER_CM && dfXRes > 0 && dfYRes > 0 &&
                dfXRes < 65535 && dfYRes < 65535)
            {
                /* Format a resd box and embed it inside a res box */
                GDALJP2Box oResd;
                oResd.SetType("resd");

                int nYDenom = 1;
                while (nYDenom < 32767 && dfYRes < 32767)
                {
                    dfYRes *= 2;
                    nYDenom *= 2;
                }
                int nXDenom = 1;
                while (nXDenom < 32767 && dfXRes < 32767)
                {
                    dfXRes *= 2;
                    nXDenom *= 2;
                }

                oResd.AppendUInt16((GUInt16)dfYRes);
                oResd.AppendUInt16((GUInt16)nYDenom);
                oResd.AppendUInt16((GUInt16)dfXRes);
                oResd.AppendUInt16((GUInt16)nXDenom);
                oResd.AppendUInt8(2); /* vertical exponent */
                oResd.AppendUInt8(2); /* horizontal exponent */

                GDALJP2Box *poResd = &oResd;
                poRes = GDALJP2Box::CreateAsocBox(1, &poResd);
                poRes->SetType("res ");
            }
        }

        /* Build and write jp2h super box now */
        GDALJP2Box *apoBoxes[7];
        int nBoxes = 1;
        apoBoxes[0] = &ihdrBox;
        if (bpccBox.GetDataLength())
            apoBoxes[nBoxes++] = &bpccBox;
        apoBoxes[nBoxes++] = &colrBox;
        if (pclrBox.GetDataLength())
            apoBoxes[nBoxes++] = &pclrBox;
        if (cmapBox.GetDataLength())
            apoBoxes[nBoxes++] = &cmapBox;
        if (cdefBox.GetDataLength())
            apoBoxes[nBoxes++] = &cdefBox;
        if (poRes)
            apoBoxes[nBoxes++] = poRes;
        GDALJP2Box *psJP2HBox =
            GDALJP2Box::CreateSuperBox("jp2h", nBoxes, apoBoxes);
        WriteBox(fp, psJP2HBox);
        delete psJP2HBox;
        delete poRes;

        if (!bGeoBoxesAfter)
        {
            if (bGeoJP2Option && bGeoreferencingCompatOfGeoJP2)
            {
                GDALJP2Box *poBox = oJP2MD.CreateJP2GeoTIFF();
                WriteBox(fp, poBox);
                delete poBox;
            }

            if (CPLFetchBool(papszOptions, "WRITE_METADATA", false) &&
                !CPLFetchBool(papszOptions, "MAIN_MD_DOMAIN_ONLY", false))
            {
                WriteXMPBox(fp, poSrcDS);
            }

            if (CPLFetchBool(papszOptions, "WRITE_METADATA", false))
            {
                if (!CPLFetchBool(papszOptions, "MAIN_MD_DOMAIN_ONLY", false))
                    WriteXMLBoxes(fp, poSrcDS);
                WriteGDALMetadataBox(fp, poSrcDS, papszOptions);
            }

            if (poGMLJP2Box != nullptr)
            {
                WriteBox(fp, poGMLJP2Box);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try lossless reuse of an existing JPEG2000 codestream           */
    /* -------------------------------------------------------------------- */
    vsi_l_offset nCodeStreamLength = 0;
    vsi_l_offset nCodeStreamStart = 0;
    VSILFILE *fpSrc = nullptr;
    if (CPLFetchBool(papszOptions, "USE_SRC_CODESTREAM", false))
    {
        CPLString osSrcFilename(poSrcDS->GetDescription());
        if (poSrcDS->GetDriver() != nullptr &&
            poSrcDS->GetDriver() == GDALGetDriverByName("VRT"))
        {
            VRTDataset *poVRTDS = (VRTDataset *)poSrcDS;
            GDALDataset *poSimpleSourceDS = poVRTDS->GetSingleSimpleSource();
            if (poSimpleSourceDS)
                osSrcFilename = poSimpleSourceDS->GetDescription();
        }

        fpSrc = VSIFOpenL(osSrcFilename, "rb");
        if (fpSrc)
        {
            nCodeStreamStart = JP2FindCodeStream(fpSrc, &nCodeStreamLength);
        }
        if (nCodeStreamLength == 0)
        {
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "USE_SRC_CODESTREAM=YES specified, but no codestream found");
        }
    }

    if (eCodecFormat == CODEC::cvtenum(JP2_CODEC_JP2))
    {
        // Start codestream box
        nStartJP2C = VSIFTellL(fp);
        if (nCodeStreamLength)
            bUseXLBoxes =
                ((vsi_l_offset)(GUInt32)nCodeStreamLength != nCodeStreamLength);
        else
            bUseXLBoxes = CPLFetchBool(papszOptions, "JP2C_XLBOX",
                                       false) || /* For debugging */
                          (GIntBig)nXSize * nYSize * nBands * nDataTypeSize /
                                  adfRates.back() >
                              4e9;
        GUInt32 nLBox = (bUseXLBoxes) ? 1 : 0;
        CPL_MSBPTR32(&nLBox);
        VSIFWriteL(&nLBox, 1, 4, fp);
        VSIFWriteL("jp2c", 1, 4, fp);
        if (bUseXLBoxes)
        {
            GUIntBig nXLBox = 0;
            VSIFWriteL(&nXLBox, 1, 8, fp);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Do lossless reuse of an existing JPEG2000 codestream            */
    /* -------------------------------------------------------------------- */
    if (fpSrc)
    {
        const char *apszIgnoredOptions[] = {"BLOCKXSIZE",
                                            "BLOCKYSIZE",
                                            "QUALITY",
                                            "REVERSIBLE",
                                            "RESOLUTIONS",
                                            "PROGRESSION",
                                            "SOP",
                                            "EPH",
                                            "YCBCR420",
                                            "YCC",
                                            "NBITS",
                                            "1BIT_ALPHA",
                                            "PRECINCTS",
                                            "TILEPARTS",
                                            "CODEBLOCK_WIDTH",
                                            "CODEBLOCK_HEIGHT",
                                            "PLT",
                                            nullptr};
        for (int i = 0; apszIgnoredOptions[i]; i++)
        {
            if (CSLFetchNameValue(papszOptions, apszIgnoredOptions[i]))
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Option %s ignored when USE_SRC_CODESTREAM=YES",
                         apszIgnoredOptions[i]);
            }
        }
        GByte abyBuffer[4096];
        VSIFSeekL(fpSrc, nCodeStreamStart, SEEK_SET);
        vsi_l_offset nRead = 0;
        /* coverity[tainted_data] */
        while (nRead < nCodeStreamLength)
        {
            int nToRead = (nCodeStreamLength - nRead > 4096)
                              ? 4096
                              : (int)(nCodeStreamLength - nRead);
            if ((int)VSIFReadL(abyBuffer, 1, nToRead, fpSrc) != nToRead)
            {
                VSIFCloseL(fp);
                VSIFCloseL(fpSrc);
                delete poGMLJP2Box;
                return nullptr;
            }
            if (nRead == 0 && (pszProfile || bInspireTG) &&
                abyBuffer[2] == 0xFF && abyBuffer[3] == 0x51)
            {
                if (EQUAL(pszProfile, "UNRESTRICTED"))
                {
                    abyBuffer[6] = 0;
                    abyBuffer[7] = 0;
                }
                else if (EQUAL(pszProfile, "PROFILE_1") || bInspireTG)
                {
                    // TODO: ultimately we should check that we can really set
                    // Profile 1
                    abyBuffer[6] = 0;
                    abyBuffer[7] = 2;
                }
            }
            if ((int)VSIFWriteL(abyBuffer, 1, nToRead, fp) != nToRead ||
                !pfnProgress((nRead + nToRead) * 1.0 / nCodeStreamLength,
                             nullptr, pProgressData))
            {
                VSIFCloseL(fp);
                VSIFCloseL(fpSrc);
                delete poGMLJP2Box;
                return nullptr;
            }
            nRead += nToRead;
        }

        VSIFCloseL(fpSrc);
    }
    else
    {
        localctx.open(fp);
        if (!localctx.initCompress(papszOptions, adfRates, nBlockXSize,
                                   nBlockYSize, bIsIrreversible,
                                   nNumResolutions, eProgOrder, bYCC, nCblockW,
                                   nCblockH, bYCBCR420, bProfile1, nBands,
                                   nXSize, nYSize, eColorSpace, numThreads))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "init compress failed");
            localctx.free();
            VSIFCloseL(fp);
            delete poGMLJP2Box;
            return nullptr;
        }
        const int nTilesX = DIV_ROUND_UP(nXSize, nBlockXSize);
        const int nTilesY = DIV_ROUND_UP(nYSize, nBlockYSize);

        const GUIntBig nTileSize =
            (GUIntBig)nBlockXSize * nBlockYSize * nBands * nDataTypeSize;
        GByte *pTempBuffer = nullptr;

        const bool bUseIOThread =
            CODEC::preferPerTileCompress() && (nTilesX > 1 || nTilesY > 1) &&
            nTileSize < 10 * 1024 * 1024 &&
            strcmp(CPLGetThreadingModel(), "stub") != 0 &&
            CPLTestBool(
                CPLGetConfigOption("JP2OPENJPEG_USE_THREADED_IO", "YES"));

        if (nTileSize > UINT_MAX)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Tile size exceeds 4GB");
            pTempBuffer = nullptr;
        }
        else
        {
            // Double memory buffer when using threaded I/O
            const size_t nBufferSize =
                static_cast<size_t>(bUseIOThread ? nTileSize * 2 : nTileSize);
            pTempBuffer = (GByte *)VSIMalloc(nBufferSize);
        }
        if (pTempBuffer == nullptr)
        {
            localctx.free();
            VSIFCloseL(fp);
            delete poGMLJP2Box;
            return nullptr;
        }

        GByte *pYUV420Buffer = nullptr;
        if (bYCBCR420)
        {
            pYUV420Buffer = (GByte *)VSIMalloc(
                3 * nBlockXSize * nBlockYSize / 2 +
                ((nBands == 4) ? nBlockXSize * nBlockYSize : 0));
            if (pYUV420Buffer == nullptr)
            {
                localctx.free();
                CPLFree(pTempBuffer);
                VSIFCloseL(fp);
                delete poGMLJP2Box;
                return nullptr;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Iterate over the tiles */
        /* --------------------------------------------------------------------
         */
        pfnProgress(0.0, nullptr, pProgressData);

        struct ReadRasterJob
        {
            GDALDataset *poSrcDS;
            int nXOff;
            int nYOff;
            int nWidthToRead;
            int nHeightToRead;
            GDALDataType eDataType;
            GByte *pBuffer;
            int nBands;
            CPLErr eErr;
        };

        const auto ReadRasterFunction = [](void *threadData)
        {
            ReadRasterJob *job = static_cast<ReadRasterJob *>(threadData);
            job->eErr = job->poSrcDS->RasterIO(
                GF_Read, job->nXOff, job->nYOff, job->nWidthToRead,
                job->nHeightToRead, job->pBuffer, job->nWidthToRead,
                job->nHeightToRead, job->eDataType, job->nBands, nullptr, 0, 0,
                0, nullptr);
        };

        CPLWorkerThreadPool oPool;
        if (bUseIOThread)
        {
            oPool.Setup(1, nullptr, nullptr);
        }

        GByte *pabyActiveBuffer = pTempBuffer;
        GByte *pabyBackgroundBuffer =
            pTempBuffer + static_cast<size_t>(nTileSize);

        CPLErr eErr = CE_None;
        int iTile = 0;

        ReadRasterJob job;
        job.eDataType = eDataType;
        job.pBuffer = pabyActiveBuffer;
        job.nBands = nBands;
        job.eErr = CE_Failure;
        job.poSrcDS = poSrcDS;

        if (bUseIOThread)
        {
            job.nXOff = 0;
            job.nYOff = 0;
            job.nWidthToRead = std::min(nBlockXSize, nXSize);
            job.nHeightToRead = std::min(nBlockYSize, nYSize);
            job.pBuffer = pabyBackgroundBuffer;
            ReadRasterFunction(&job);
            eErr = job.eErr;
        }

        for (int nBlockYOff = 0; eErr == CE_None && nBlockYOff < nTilesY;
             nBlockYOff++)
        {
            for (int nBlockXOff = 0; eErr == CE_None && nBlockXOff < nTilesX;
                 nBlockXOff++)
            {
                const int nWidthToRead =
                    std::min(nBlockXSize, nXSize - nBlockXOff * nBlockXSize);
                const int nHeightToRead =
                    std::min(nBlockYSize, nYSize - nBlockYOff * nBlockYSize);

                if (bUseIOThread)
                {
                    // Wait for previous background I/O task to be finished
                    oPool.WaitCompletion();
                    eErr = job.eErr;

                    // Swap buffers
                    std::swap(pabyBackgroundBuffer, pabyActiveBuffer);

                    // Prepare for next I/O task
                    int nNextBlockXOff = nBlockXOff + 1;
                    int nNextBlockYOff = nBlockYOff;
                    if (nNextBlockXOff == nTilesX)
                    {
                        nNextBlockXOff = 0;
                        nNextBlockYOff++;
                    }
                    if (nNextBlockYOff != nTilesY)
                    {
                        job.nXOff = nNextBlockXOff * nBlockXSize;
                        job.nYOff = nNextBlockYOff * nBlockYSize;
                        job.nWidthToRead =
                            std::min(nBlockXSize, nXSize - job.nXOff);
                        job.nHeightToRead =
                            std::min(nBlockYSize, nYSize - job.nYOff);
                        job.pBuffer = pabyBackgroundBuffer;

                        // Submit next job
                        oPool.SubmitJob(ReadRasterFunction, &job);
                    }
                }
                else
                {
                    job.nXOff = nBlockXOff * nBlockXSize;
                    job.nYOff = nBlockYOff * nBlockYSize;
                    job.nWidthToRead = nWidthToRead;
                    job.nHeightToRead = nHeightToRead;
                    ReadRasterFunction(&job);
                    eErr = job.eErr;
                }

                if (b1BitAlpha)
                {
                    for (int i = 0; i < nWidthToRead * nHeightToRead; i++)
                    {
                        if (pabyActiveBuffer[nAlphaBandIndex * nWidthToRead *
                                                 nHeightToRead +
                                             i])
                            pabyActiveBuffer[nAlphaBandIndex * nWidthToRead *
                                                 nHeightToRead +
                                             i] = 1;
                        else
                            pabyActiveBuffer[nAlphaBandIndex * nWidthToRead *
                                                 nHeightToRead +
                                             i] = 0;
                    }
                }
                if (eErr == CE_None)
                {
                    if (bYCBCR420)
                    {
                        int j, i;
                        for (j = 0; j < nHeightToRead; j++)
                        {
                            for (i = 0; i < nWidthToRead; i++)
                            {
                                int R = pabyActiveBuffer[j * nWidthToRead + i];
                                int G = pabyActiveBuffer[nHeightToRead *
                                                             nWidthToRead +
                                                         j * nWidthToRead + i];
                                int B = pabyActiveBuffer[2 * nHeightToRead *
                                                             nWidthToRead +
                                                         j * nWidthToRead + i];
                                int Y =
                                    (int)(0.299 * R + 0.587 * G + 0.114 * B);
                                int Cb =
                                    CLAMP_0_255((int)(-0.1687 * R - 0.3313 * G +
                                                      0.5 * B + 128));
                                int Cr =
                                    CLAMP_0_255((int)(0.5 * R - 0.4187 * G -
                                                      0.0813 * B + 128));
                                pYUV420Buffer[j * nWidthToRead + i] = (GByte)Y;
                                pYUV420Buffer[nHeightToRead * nWidthToRead +
                                              ((j / 2) * ((nWidthToRead) / 2) +
                                               i / 2)] = (GByte)Cb;
                                pYUV420Buffer[5 * nHeightToRead * nWidthToRead /
                                                  4 +
                                              ((j / 2) * ((nWidthToRead) / 2) +
                                               i / 2)] = (GByte)Cr;
                                if (nBands == 4)
                                {
                                    pYUV420Buffer[3 * nHeightToRead *
                                                      nWidthToRead / 2 +
                                                  j * nWidthToRead + i] =
                                        (GByte)pabyActiveBuffer
                                            [3 * nHeightToRead * nWidthToRead +
                                             j * nWidthToRead + i];
                                }
                            }
                        }

                        int nBytesToWrite =
                            3 * nWidthToRead * nHeightToRead / 2;
                        if (nBands == 4)
                            nBytesToWrite += nBlockXSize * nBlockYSize;

                        if (!localctx.compressTile(iTile, pYUV420Buffer,
                                                   nBytesToWrite))
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "compress tile failed");
                            eErr = CE_Failure;
                        }
                    }
                    else
                    {
                        if (!localctx.compressTile(iTile, pabyActiveBuffer,
                                                   nWidthToRead *
                                                       nHeightToRead * nBands *
                                                       nDataTypeSize))
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "compress tile failed");
                            eErr = CE_Failure;
                        }
                    }
                }

                if (!pfnProgress((iTile + 1) * 1.0 / (nTilesX * nTilesY),
                                 nullptr, pProgressData))
                    eErr = CE_Failure;

                iTile++;
            }
        }

        if (bUseIOThread && eErr == CE_Failure)
        {
            // Wait for previous background I/O task to be finished
            // before freeing buffers (pTempBuffer, etc.)
            oPool.WaitCompletion();
        }

        VSIFree(pTempBuffer);
        VSIFree(pYUV420Buffer);

        if (eErr != CE_None)
        {
            localctx.free();
            VSIFCloseL(fp);
            delete poGMLJP2Box;
            return nullptr;
        }

        if (!localctx.finishCompress())
        {
            localctx.free();
            VSIFCloseL(fp);
            delete poGMLJP2Box;
            return nullptr;
        }
        localctx.free();
    }

    /* -------------------------------------------------------------------- */
    /*      Patch JP2C box length and add trailing JP2 boxes                */
    /* -------------------------------------------------------------------- */
    bool bRet = true;
    if (eCodecFormat == CODEC::cvtenum(JP2_CODEC_JP2) &&
        !CPLFetchBool(papszOptions, "JP2C_LENGTH_ZERO",
                      false) /* debug option */)
    {
        vsi_l_offset nEndJP2C = VSIFTellL(fp);
        GUIntBig nBoxSize = nEndJP2C - nStartJP2C;
        if (bUseXLBoxes)
        {
            VSIFSeekL(fp, nStartJP2C + 8, SEEK_SET);
            CPL_MSBPTR64(&nBoxSize);
            if (VSIFWriteL(&nBoxSize, 8, 1, fp) != 1)
                bRet = false;
        }
        else
        {
            GUInt32 nBoxSize32 = (GUInt32)nBoxSize;
            if ((vsi_l_offset)nBoxSize32 != nBoxSize)
            {
                /*  Should not happen hopefully */
                if ((bGeoreferencingCompatOfGeoJP2 || poGMLJP2Box) &&
                    bGeoBoxesAfter)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot write GMLJP2/GeoJP2 boxes as codestream "
                             "is unexpectedly > 4GB");
                    bGeoreferencingCompatOfGeoJP2 = FALSE;
                    delete poGMLJP2Box;
                    poGMLJP2Box = nullptr;
                }
            }
            else
            {
                VSIFSeekL(fp, nStartJP2C, SEEK_SET);
                CPL_MSBPTR32(&nBoxSize32);
                if (VSIFWriteL(&nBoxSize32, 4, 1, fp) != 1)
                    bRet = false;
            }
        }
        VSIFSeekL(fp, 0, SEEK_END);

        if (CPLFetchBool(papszOptions, "WRITE_METADATA", false))
        {
            if (!WriteIPRBox(fp, poSrcDS))
                bRet = false;
        }

        if (bGeoBoxesAfter)
        {
            if (poGMLJP2Box != nullptr)
            {
                if (!WriteBox(fp, poGMLJP2Box))
                    bRet = false;
            }

            if (CPLFetchBool(papszOptions, "WRITE_METADATA", false))
            {
                if (!CPLFetchBool(papszOptions, "MAIN_MD_DOMAIN_ONLY", false))
                {
                    if (!WriteXMLBoxes(fp, poSrcDS))
                        bRet = false;
                }
                if (!WriteGDALMetadataBox(fp, poSrcDS, papszOptions))
                    bRet = false;
            }

            if (bGeoJP2Option && bGeoreferencingCompatOfGeoJP2)
            {
                GDALJP2Box *poBox = oJP2MD.CreateJP2GeoTIFF();
                if (!WriteBox(fp, poBox))
                    bRet = false;
                delete poBox;
            }

            if (CPLFetchBool(papszOptions, "WRITE_METADATA", false) &&
                !CPLFetchBool(papszOptions, "MAIN_MD_DOMAIN_ONLY", false))
            {
                if (!WriteXMPBox(fp, poSrcDS))
                    bRet = false;
            }
        }
    }

    if (VSIFCloseL(fp) != 0)
        bRet = false;
    delete poGMLJP2Box;
    if (!bRet)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Re-open dataset, and copy any auxiliary pam information.        */
    /* -------------------------------------------------------------------- */

    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    auto poDS = (JP2OPJLikeDataset *)JP2OPJLikeDataset::Open(&oOpenInfo);

    if (poDS)
    {
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT & (~GCIF_METADATA));

        /* Only write relevant metadata to PAM, and if needed */
        if (!CPLFetchBool(papszOptions, "WRITE_METADATA", false))
        {
            char **papszSrcMD = CSLDuplicate(poSrcDS->GetMetadata());
            papszSrcMD =
                CSLSetNameValue(papszSrcMD, GDALMD_AREA_OR_POINT, nullptr);
            papszSrcMD = CSLSetNameValue(papszSrcMD, "Corder", nullptr);
            for (char **papszSrcMDIter = papszSrcMD;
                 papszSrcMDIter && *papszSrcMDIter;)
            {
                /* Remove entries like KEY= (without value) */
                if ((*papszSrcMDIter)[0] &&
                    (*papszSrcMDIter)[strlen((*papszSrcMDIter)) - 1] == '=')
                {
                    CPLFree(*papszSrcMDIter);
                    memmove(papszSrcMDIter, papszSrcMDIter + 1,
                            sizeof(char *) *
                                (CSLCount(papszSrcMDIter + 1) + 1));
                }
                else
                    ++papszSrcMDIter;
            }
            char **papszMD = CSLDuplicate(poDS->GetMetadata());
            papszMD = CSLSetNameValue(papszMD, GDALMD_AREA_OR_POINT, nullptr);
            if (papszSrcMD && papszSrcMD[0] != nullptr &&
                CSLCount(papszSrcMD) != CSLCount(papszMD))
            {
                poDS->SetMetadata(papszSrcMD);
            }
            CSLDestroy(papszSrcMD);
            CSLDestroy(papszMD);
        }
    }

    return poDS;
}

#ifdef unused
template <typename CODEC, typename BASE>
void GDALRegisterJP2(const std::string &libraryName,
                     const std::string &driverName)

{
    if (!GDAL_CHECK_VERSION((driverName + " driver").c_str()))
        return;

    if (GDALGetDriverByName(driverName.c_str()) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription(driverName.c_str());
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(
        GDAL_DMD_LONGNAME,
        ("JPEG-2000 driver based on " + libraryName + " library").c_str());

    poDriver->SetMetadataItem(
        GDAL_DMD_HELPTOPIC,
        ("drivers/raster/jp2" + CPLString(libraryName).tolower() + ".html")
            .c_str());
    poDriver->SetMetadataItem(GDAL_DMD_MIMETYPE, "image/jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jp2");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "jp2 j2k");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    BASE::setMetaData(poDriver);

    poDriver->pfnIdentify = JP2OPJLikeDataset<CODEC, BASE>::Identify;
    poDriver->pfnOpen = JP2OPJLikeDataset<CODEC, BASE>::Open;
    poDriver->pfnCreateCopy = JP2OPJLikeDataset<CODEC, BASE>::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
#endif
