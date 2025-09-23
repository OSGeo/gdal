/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTSourcedRasterBand
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_vrt.h"
#include "vrtdataset.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <set>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_error_internal.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_quad_tree.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_thread_pool.h"
#include "ogr_geometry.h"

/*! @cond Doxygen_Suppress */

/************************************************************************/
/* ==================================================================== */
/*                          VRTSourcedRasterBand                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand(GDALDataset *poDSIn, int nBandIn)
{
    VRTRasterBand::Initialize(poDSIn->GetRasterXSize(),
                              poDSIn->GetRasterYSize());

    poDS = poDSIn;
    nBand = nBandIn;
}

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand(GDALDataType eType, int nXSize,
                                           int nYSize)
{
    VRTRasterBand::Initialize(nXSize, nYSize);

    eDataType = eType;
}

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand(GDALDataset *poDSIn, int nBandIn,
                                           GDALDataType eType, int nXSize,
                                           int nYSize)
    : VRTSourcedRasterBand(poDSIn, nBandIn, eType, nXSize, nYSize, 0, 0)
{
}

/************************************************************************/
/*                        VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::VRTSourcedRasterBand(GDALDataset *poDSIn, int nBandIn,
                                           GDALDataType eType, int nXSize,
                                           int nYSize, int nBlockXSizeIn,
                                           int nBlockYSizeIn)
{
    VRTRasterBand::Initialize(nXSize, nYSize);

    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = eType;
    if (nBlockXSizeIn > 0)
        nBlockXSize = nBlockXSizeIn;
    if (nBlockYSizeIn > 0)
        nBlockYSize = nBlockYSizeIn;
}

/************************************************************************/
/*                       ~VRTSourcedRasterBand()                        */
/************************************************************************/

VRTSourcedRasterBand::~VRTSourcedRasterBand()

{
    VRTSourcedRasterBand::CloseDependentDatasets();
    CSLDestroy(m_papszSourceList);
}

/************************************************************************/
/*                  CanIRasterIOBeForwardedToEachSource()               */
/************************************************************************/

bool VRTSourcedRasterBand::CanIRasterIOBeForwardedToEachSource(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    int nBufXSize, int nBufYSize, GDALRasterIOExtraArg *psExtraArg) const
{
    const auto IsNonNearestInvolved = [this, psExtraArg]
    {
        if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
        {
            return true;
        }
        for (int i = 0; i < nSources; i++)
        {
            if (papoSources[i]->GetType() == VRTComplexSource::GetTypeStatic())
            {
                auto *const poComplexSource =
                    static_cast<VRTComplexSource *>(papoSources[i]);
                const auto &osSourceResampling =
                    poComplexSource->GetResampling();
                if (!osSourceResampling.empty() &&
                    osSourceResampling != "nearest")
                    return true;
            }
        }
        return false;
    };

    // If resampling with non-nearest neighbour, we need to be careful
    // if the VRT band exposes a nodata value, but the sources do not have it.
    // To also avoid edge effects on sources when downsampling, use the
    // base implementation of IRasterIO() (that is acquiring sources at their
    // nominal resolution, and then downsampling), but only if none of the
    // contributing sources have overviews.
    if (eRWFlag == GF_Read && (nXSize != nBufXSize || nYSize != nBufYSize) &&
        nSources != 0 && IsNonNearestInvolved())
    {
        bool bSourceHasOverviews = false;
        const bool bIsDownsampling = (nBufXSize < nXSize && nBufYSize < nYSize);
        int nContributingSources = 0;
        bool bSourceFullySatisfiesRequest = true;
        for (int i = 0; i < nSources; i++)
        {
            if (!papoSources[i]->IsSimpleSource())
            {
                return false;
            }
            else
            {
                VRTSimpleSource *const poSource =
                    static_cast<VRTSimpleSource *>(papoSources[i]);

                if (poSource->GetType() == VRTComplexSource::GetTypeStatic())
                {
                    auto *const poComplexSource =
                        static_cast<VRTComplexSource *>(poSource);
                    const auto &osSourceResampling =
                        poComplexSource->GetResampling();
                    if (!osSourceResampling.empty() &&
                        osSourceResampling != "nearest")
                    {
                        const int lMaskFlags =
                            const_cast<VRTSourcedRasterBand *>(this)
                                ->GetMaskFlags();
                        if ((lMaskFlags != GMF_ALL_VALID &&
                             lMaskFlags != GMF_NODATA) ||
                            IsMaskBand())
                        {
                            // Unfortunately this will prevent using overviews
                            // of the sources, but it is unpractical to use
                            // them without serious implementation complications
                            return false;
                        }
                    }
                }

                double dfXOff = nXOff;
                double dfYOff = nYOff;
                double dfXSize = nXSize;
                double dfYSize = nYSize;
                if (psExtraArg->bFloatingPointWindowValidity)
                {
                    dfXOff = psExtraArg->dfXOff;
                    dfYOff = psExtraArg->dfYOff;
                    dfXSize = psExtraArg->dfXSize;
                    dfYSize = psExtraArg->dfYSize;
                }

                // The window we will actually request from the source raster
                // band.
                double dfReqXOff = 0.0;
                double dfReqYOff = 0.0;
                double dfReqXSize = 0.0;
                double dfReqYSize = 0.0;
                int nReqXOff = 0;
                int nReqYOff = 0;
                int nReqXSize = 0;
                int nReqYSize = 0;

                // The window we will actual set _within_ the pData buffer.
                int nOutXOff = 0;
                int nOutYOff = 0;
                int nOutXSize = 0;
                int nOutYSize = 0;

                bool bError = false;
                if (!poSource->GetSrcDstWindow(
                        dfXOff, dfYOff, dfXSize, dfYSize, nBufXSize, nBufYSize,
                        &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                        &nReqXOff, &nReqYOff, &nReqXSize, &nReqYSize, &nOutXOff,
                        &nOutYOff, &nOutXSize, &nOutYSize, bError))
                {
                    continue;
                }
                auto poBand = poSource->GetRasterBand();
                if (poBand == nullptr)
                {
                    return false;
                }
                ++nContributingSources;
                if (!(nOutXOff == 0 && nOutYOff == 0 &&
                      nOutXSize == nBufXSize && nOutYSize == nBufYSize))
                    bSourceFullySatisfiesRequest = false;
                if (m_bNoDataValueSet)
                {
                    int bSrcHasNoData = FALSE;
                    const double dfSrcNoData =
                        poBand->GetNoDataValue(&bSrcHasNoData);
                    if (!bSrcHasNoData || dfSrcNoData != m_dfNoDataValue)
                    {
                        return false;
                    }
                }
                if (bIsDownsampling)
                {
                    if (poBand->GetOverviewCount() != 0)
                    {
                        bSourceHasOverviews = true;
                    }
                }
            }
        }
        if (bIsDownsampling && !bSourceHasOverviews &&
            (nContributingSources > 1 || !bSourceFullySatisfiesRequest))
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                      CanMultiThreadRasterIO()                        */
/************************************************************************/

bool VRTSourcedRasterBand::CanMultiThreadRasterIO(
    double dfXOff, double dfYOff, double dfXSize, double dfYSize,
    int &nContributingSources) const
{
    int iLastSource = 0;
    CPLRectObj sSourceBounds;
    CPLQuadTree *hQuadTree = nullptr;
    bool bRet = true;
    std::set<std::string> oSetDSName;

    nContributingSources = 0;
    for (int iSource = 0; iSource < nSources; iSource++)
    {
        const auto poSource = papoSources[iSource];
        if (!poSource->IsSimpleSource())
        {
            bRet = false;
            break;
        }
        const auto poSimpleSource = cpl::down_cast<VRTSimpleSource *>(poSource);
        if (poSimpleSource->DstWindowIntersects(dfXOff, dfYOff, dfXSize,
                                                dfYSize))
        {
            // Only build hQuadTree if there are 2 or more sources
            if (nContributingSources == 1)
            {
                std::string &oFirstSrcDSName =
                    cpl::down_cast<VRTSimpleSource *>(papoSources[iLastSource])
                        ->m_osSrcDSName;
                oSetDSName.insert(oFirstSrcDSName);

                CPLRectObj sGlobalBounds;
                sGlobalBounds.minx = dfXOff;
                sGlobalBounds.miny = dfYOff;
                sGlobalBounds.maxx = dfXOff + dfXSize;
                sGlobalBounds.maxy = dfYOff + dfYSize;
                hQuadTree = CPLQuadTreeCreate(&sGlobalBounds, nullptr);

                CPLQuadTreeInsertWithBounds(
                    hQuadTree,
                    reinterpret_cast<void *>(
                        static_cast<uintptr_t>(iLastSource)),
                    &sSourceBounds);
            }

            // Check there are not several sources with the same name, to avoid
            // the same GDALDataset* to be used from multiple threads. We may
            // be a bit too pessimistic, for example if working with unnamed
            // Memory datasets, but that would involve comparing
            // poSource->GetRasterBandNoOpen()->GetDataset()
            if (oSetDSName.find(poSimpleSource->m_osSrcDSName) !=
                oSetDSName.end())
            {
                bRet = false;
                break;
            }
            oSetDSName.insert(poSimpleSource->m_osSrcDSName);

            double dfSourceXOff;
            double dfSourceYOff;
            double dfSourceXSize;
            double dfSourceYSize;
            poSimpleSource->GetDstWindow(dfSourceXOff, dfSourceYOff,
                                         dfSourceXSize, dfSourceYSize);
            constexpr double EPSILON = 1e-1;
            sSourceBounds.minx = dfSourceXOff + EPSILON;
            sSourceBounds.miny = dfSourceYOff + EPSILON;
            sSourceBounds.maxx = dfSourceXOff + dfSourceXSize - EPSILON;
            sSourceBounds.maxy = dfSourceYOff + dfSourceYSize - EPSILON;
            iLastSource = iSource;

            if (hQuadTree)
            {
                // Check that the new source doesn't overlap an existing one.
                if (CPLQuadTreeHasMatch(hQuadTree, &sSourceBounds))
                {
                    bRet = false;
                    break;
                }

                CPLQuadTreeInsertWithBounds(
                    hQuadTree,
                    reinterpret_cast<void *>(static_cast<uintptr_t>(iSource)),
                    &sSourceBounds);
            }

            ++nContributingSources;
        }
    }

    if (hQuadTree)
        CPLQuadTreeDestroy(hQuadTree);

    return bRet;
}

/************************************************************************/
/*                 VRTSourcedRasterBandRasterIOJob                      */
/************************************************************************/

/** Structure used to declare a threaded job to satisfy IRasterIO()
 * on a given source.
 */
struct VRTSourcedRasterBandRasterIOJob
{
    std::atomic<int> *pnCompletedJobs = nullptr;
    std::atomic<bool> *pbSuccess = nullptr;
    VRTDataset::QueueWorkingStates *poQueueWorkingStates = nullptr;
    CPLErrorAccumulator *poErrorAccumulator = nullptr;

    GDALDataType eVRTBandDataType = GDT_Unknown;
    int nXOff = 0;
    int nYOff = 0;
    int nXSize = 0;
    int nYSize = 0;
    void *pData = nullptr;
    int nBufXSize = 0;
    int nBufYSize = 0;
    GDALDataType eBufType = GDT_Unknown;
    GSpacing nPixelSpace = 0;
    GSpacing nLineSpace = 0;
    GDALRasterIOExtraArg *psExtraArg = nullptr;
    VRTSimpleSource *poSource = nullptr;

    static void Func(void *pData);
};

/************************************************************************/
/*                 VRTSourcedRasterBandRasterIOJob::Func()              */
/************************************************************************/

void VRTSourcedRasterBandRasterIOJob::Func(void *pData)
{
    auto psJob = std::unique_ptr<VRTSourcedRasterBandRasterIOJob>(
        static_cast<VRTSourcedRasterBandRasterIOJob *>(pData));
    if (*psJob->pbSuccess)
    {
        GDALRasterIOExtraArg sArg = *(psJob->psExtraArg);
        sArg.pfnProgress = nullptr;
        sArg.pProgressData = nullptr;

        std::unique_ptr<VRTSource::WorkingState> poWorkingState;
        {
            std::lock_guard oLock(psJob->poQueueWorkingStates->oMutex);
            poWorkingState =
                std::move(psJob->poQueueWorkingStates->oStates.back());
            psJob->poQueueWorkingStates->oStates.pop_back();
            CPLAssert(poWorkingState.get());
        }

        auto oAccumulator = psJob->poErrorAccumulator->InstallForCurrentScope();
        CPL_IGNORE_RET_VAL(oAccumulator);

        if (psJob->poSource->RasterIO(
                psJob->eVRTBandDataType, psJob->nXOff, psJob->nYOff,
                psJob->nXSize, psJob->nYSize, psJob->pData, psJob->nBufXSize,
                psJob->nBufYSize, psJob->eBufType, psJob->nPixelSpace,
                psJob->nLineSpace, &sArg, *(poWorkingState.get())) != CE_None)
        {
            *psJob->pbSuccess = false;
        }

        {
            std::lock_guard oLock(psJob->poQueueWorkingStates->oMutex);
            psJob->poQueueWorkingStates->oStates.push_back(
                std::move(poWorkingState));
        }
    }

    ++(*psJob->pnCompletedJobs);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr VRTSourcedRasterBand::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpace, GSpacing nLineSpace, GDALRasterIOExtraArg *psExtraArg)

{
    if (eRWFlag == GF_Write)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Writing through VRTSourcedRasterBand is not supported.");
        return CE_Failure;
    }

    const std::string osFctId("VRTSourcedRasterBand::IRasterIO");
    GDALAntiRecursionGuard oGuard(osFctId);
    if (oGuard.GetCallDepth() >= 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        return CE_Failure;
    }

    GDALAntiRecursionGuard oGuard2(oGuard, poDS->GetDescription());
    // Allow 2 recursion depths on the same dataset for non-nearest resampling
    if (oGuard2.GetCallDepth() > 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        return CE_Failure;
    }

    /* ==================================================================== */
    /*      Do we have overviews that would be appropriate to satisfy       */
    /*      this request?                                                   */
    /* ==================================================================== */
    auto l_poDS = dynamic_cast<VRTDataset *>(poDS);
    if (l_poDS &&
        l_poDS->m_apoOverviews.empty() &&  // do not use virtual overviews
        (nBufXSize < nXSize || nBufYSize < nYSize) && GetOverviewCount() > 0)
    {
        if (OverviewRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                             nBufXSize, nBufYSize, eBufType, nPixelSpace,
                             nLineSpace, psExtraArg) == CE_None)
            return CE_None;
    }

    // If resampling with non-nearest neighbour, we need to be careful
    // if the VRT band exposes a nodata value, but the sources do not have it.
    // To also avoid edge effects on sources when downsampling, use the
    // base implementation of IRasterIO() (that is acquiring sources at their
    // nominal resolution, and then downsampling), but only if none of the
    // contributing sources have overviews.
    if (l_poDS && !CanIRasterIOBeForwardedToEachSource(
                      eRWFlag, nXOff, nYOff, nXSize, nYSize, nBufXSize,
                      nBufYSize, psExtraArg))
    {
        const bool bBackupEnabledOverviews = l_poDS->AreOverviewsEnabled();
        if (!l_poDS->m_apoOverviews.empty() && l_poDS->AreOverviewsEnabled())
        {
            // Disable use of implicit overviews to avoid infinite
            // recursion
            l_poDS->SetEnableOverviews(false);
        }

        const auto eResampleAlgBackup = psExtraArg->eResampleAlg;
        if (psExtraArg->eResampleAlg == GRIORA_NearestNeighbour)
        {
            std::string osResampling;
            for (int i = 0; i < nSources; i++)
            {
                if (papoSources[i]->GetType() ==
                    VRTComplexSource::GetTypeStatic())
                {
                    auto *const poComplexSource =
                        static_cast<VRTComplexSource *>(papoSources[i]);
                    if (!poComplexSource->GetResampling().empty())
                    {
                        if (i == 0)
                            osResampling = poComplexSource->GetResampling();
                        else if (osResampling !=
                                 poComplexSource->GetResampling())
                        {
                            osResampling.clear();
                            break;
                        }
                    }
                }
            }
            if (!osResampling.empty())
                psExtraArg->eResampleAlg =
                    GDALRasterIOGetResampleAlg(osResampling.c_str());
        }

        const auto eErr = GDALRasterBand::IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg);

        psExtraArg->eResampleAlg = eResampleAlgBackup;
        l_poDS->SetEnableOverviews(bBackupEnabledOverviews);
        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize the buffer to some background value. Use the         */
    /*      nodata value if available.                                      */
    /* -------------------------------------------------------------------- */
    if (SkipBufferInitialization())
    {
        // Do nothing
    }
    else if (nPixelSpace == GDALGetDataTypeSizeBytes(eBufType) &&
             !(m_bNoDataValueSet && m_dfNoDataValue != 0.0) &&
             !(m_bNoDataSetAsInt64 && m_nNoDataValueInt64 != 0) &&
             !(m_bNoDataSetAsUInt64 && m_nNoDataValueUInt64 != 0))
    {
        if (nLineSpace == nBufXSize * nPixelSpace)
        {
            memset(pData, 0, static_cast<size_t>(nBufYSize * nLineSpace));
        }
        else
        {
            for (int iLine = 0; iLine < nBufYSize; iLine++)
            {
                memset(static_cast<GByte *>(pData) +
                           static_cast<GIntBig>(iLine) * nLineSpace,
                       0, static_cast<size_t>(nBufXSize * nPixelSpace));
            }
        }
    }
    else if (m_bNoDataSetAsInt64)
    {
        for (int iLine = 0; iLine < nBufYSize; iLine++)
        {
            GDALCopyWords(&m_nNoDataValueInt64, GDT_Int64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GIntBig>(nLineSpace) * iLine,
                          eBufType, static_cast<int>(nPixelSpace), nBufXSize);
        }
    }
    else if (m_bNoDataSetAsUInt64)
    {
        for (int iLine = 0; iLine < nBufYSize; iLine++)
        {
            GDALCopyWords(&m_nNoDataValueUInt64, GDT_UInt64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GIntBig>(nLineSpace) * iLine,
                          eBufType, static_cast<int>(nPixelSpace), nBufXSize);
        }
    }
    else
    {
        double dfWriteValue = 0.0;
        if (m_bNoDataValueSet)
            dfWriteValue = m_dfNoDataValue;

        for (int iLine = 0; iLine < nBufYSize; iLine++)
        {
            GDALCopyWords(&dfWriteValue, GDT_Float64, 0,
                          static_cast<GByte *>(pData) +
                              static_cast<GIntBig>(nLineSpace) * iLine,
                          eBufType, static_cast<int>(nPixelSpace), nBufXSize);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Overlay each source in turn over top this.                      */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;

    double dfXOff = nXOff;
    double dfYOff = nYOff;
    double dfXSize = nXSize;
    double dfYSize = nYSize;
    if (psExtraArg->bFloatingPointWindowValidity)
    {
        dfXOff = psExtraArg->dfXOff;
        dfYOff = psExtraArg->dfYOff;
        dfXSize = psExtraArg->dfXSize;
        dfYSize = psExtraArg->dfYSize;
    }

    if (l_poDS)
        l_poDS->m_bMultiThreadedRasterIOLastUsed = false;

    int nContributingSources = 0;
    int nMaxThreads = 0;
    constexpr int MINIMUM_PIXEL_COUNT_FOR_THREADED_IO = 1000 * 1000;
    if (l_poDS &&
        (static_cast<int64_t>(nBufXSize) * nBufYSize >=
             MINIMUM_PIXEL_COUNT_FOR_THREADED_IO ||
         static_cast<int64_t>(nXSize) * nYSize >=
             MINIMUM_PIXEL_COUNT_FOR_THREADED_IO) &&
        CanMultiThreadRasterIO(dfXOff, dfYOff, dfXSize, dfYSize,
                               nContributingSources) &&
        nContributingSources > 1 &&
        (nMaxThreads = VRTDataset::GetNumThreads(l_poDS)) > 1)
    {
        l_poDS->m_bMultiThreadedRasterIOLastUsed = true;
        l_poDS->m_oMapSharedSources.InitMutex();

        CPLErrorAccumulator errorAccumulator;
        std::atomic<bool> bSuccess = true;
        CPLWorkerThreadPool *psThreadPool = GDALGetGlobalThreadPool(
            std::min(nContributingSources, nMaxThreads));
        const int nThreads =
            std::min(nContributingSources, psThreadPool->GetThreadCount());
        CPLDebugOnly("VRT",
                     "IRasterIO(): use optimized "
                     "multi-threaded code path for mosaic. "
                     "Using %d threads",
                     nThreads);

        {
            std::lock_guard oLock(l_poDS->m_oQueueWorkingStates.oMutex);
            if (l_poDS->m_oQueueWorkingStates.oStates.size() <
                static_cast<size_t>(nThreads))
            {
                l_poDS->m_oQueueWorkingStates.oStates.resize(nThreads);
            }
            for (int i = 0; i < nThreads; ++i)
            {
                if (!l_poDS->m_oQueueWorkingStates.oStates[i])
                    l_poDS->m_oQueueWorkingStates.oStates[i] =
                        std::make_unique<VRTSource::WorkingState>();
            }
        }

        auto oQueue = psThreadPool->CreateJobQueue();
        std::atomic<int> nCompletedJobs = 0;
        for (int iSource = 0; iSource < nSources; iSource++)
        {
            auto poSource = papoSources[iSource];
            if (!poSource->IsSimpleSource())
                continue;
            auto poSimpleSource = cpl::down_cast<VRTSimpleSource *>(poSource);
            if (poSimpleSource->DstWindowIntersects(dfXOff, dfYOff, dfXSize,
                                                    dfYSize))
            {
                auto psJob = new VRTSourcedRasterBandRasterIOJob();
                psJob->pbSuccess = &bSuccess;
                psJob->pnCompletedJobs = &nCompletedJobs;
                psJob->poQueueWorkingStates = &(l_poDS->m_oQueueWorkingStates);
                psJob->poErrorAccumulator = &errorAccumulator;
                psJob->eVRTBandDataType = eDataType;
                psJob->nXOff = nXOff;
                psJob->nYOff = nYOff;
                psJob->nXSize = nXSize;
                psJob->nYSize = nYSize;
                psJob->pData = pData;
                psJob->nBufXSize = nBufXSize;
                psJob->nBufYSize = nBufYSize;
                psJob->eBufType = eBufType;
                psJob->nPixelSpace = nPixelSpace;
                psJob->nLineSpace = nLineSpace;
                psJob->psExtraArg = psExtraArg;
                psJob->poSource = poSimpleSource;

                if (!oQueue->SubmitJob(VRTSourcedRasterBandRasterIOJob::Func,
                                       psJob))
                {
                    delete psJob;
                    bSuccess = false;
                    break;
                }
            }
        }

        while (oQueue->WaitEvent())
        {
            // Quite rough progress callback. We could do better by counting
            // the number of contributing pixels.
            if (psExtraArg->pfnProgress)
            {
                psExtraArg->pfnProgress(double(nCompletedJobs.load()) /
                                            nContributingSources,
                                        "", psExtraArg->pProgressData);
            }
        }

        errorAccumulator.ReplayErrors();
        eErr = bSuccess ? CE_None : CE_Failure;
    }
    else
    {
        GDALProgressFunc const pfnProgressGlobal = psExtraArg->pfnProgress;
        void *const pProgressDataGlobal = psExtraArg->pProgressData;

        VRTSource::WorkingState oWorkingState;
        for (int iSource = 0; eErr == CE_None && iSource < nSources; iSource++)
        {
            psExtraArg->pfnProgress = GDALScaledProgress;
            psExtraArg->pProgressData = GDALCreateScaledProgress(
                1.0 * iSource / nSources, 1.0 * (iSource + 1) / nSources,
                pfnProgressGlobal, pProgressDataGlobal);
            if (psExtraArg->pProgressData == nullptr)
                psExtraArg->pfnProgress = nullptr;

            eErr = papoSources[iSource]->RasterIO(
                eDataType, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize,
                nBufYSize, eBufType, nPixelSpace, nLineSpace, psExtraArg,
                l_poDS ? l_poDS->m_oWorkingState : oWorkingState);

            GDALDestroyScaledProgress(psExtraArg->pProgressData);
        }

        psExtraArg->pfnProgress = pfnProgressGlobal;
        psExtraArg->pProgressData = pProgressDataGlobal;
    }

    if (eErr == CE_None && psExtraArg->pfnProgress)
    {
        psExtraArg->pfnProgress(1.0, "", psExtraArg->pProgressData);
    }

    return eErr;
}

/************************************************************************/
/*                         IGetDataCoverageStatus()                     */
/************************************************************************/

int VRTSourcedRasterBand::IGetDataCoverageStatus(int nXOff, int nYOff,
                                                 int nXSize, int nYSize,
                                                 int nMaskFlagStop,
                                                 double *pdfDataPct)
{
    if (pdfDataPct)
        *pdfDataPct = -1.0;

    // Particular case for a single simple source covering the whole dataset
    if (nSources == 1 && papoSources[0]->IsSimpleSource() &&
        papoSources[0]->GetType() == VRTSimpleSource::GetTypeStatic())
    {
        VRTSimpleSource *poSource =
            static_cast<VRTSimpleSource *>(papoSources[0]);

        GDALRasterBand *poBand = poSource->GetRasterBand();
        if (!poBand)
            poBand = poSource->GetMaskBandMainBand();
        if (!poBand)
        {
            return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
                   GDAL_DATA_COVERAGE_STATUS_DATA;
        }

        /* Check that it uses the full source dataset */
        double dfReqXOff = 0.0;
        double dfReqYOff = 0.0;
        double dfReqXSize = 0.0;
        double dfReqYSize = 0.0;
        int nReqXOff = 0;
        int nReqYOff = 0;
        int nReqXSize = 0;
        int nReqYSize = 0;
        int nOutXOff = 0;
        int nOutYOff = 0;
        int nOutXSize = 0;
        int nOutYSize = 0;
        bool bError = false;
        if (poSource->GetSrcDstWindow(
                0, 0, GetXSize(), GetYSize(), GetXSize(), GetYSize(),
                &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize, &nReqXOff,
                &nReqYOff, &nReqXSize, &nReqYSize, &nOutXOff, &nOutYOff,
                &nOutXSize, &nOutYSize, bError) &&
            nReqXOff == 0 && nReqYOff == 0 && nReqXSize == GetXSize() &&
            nReqXSize == poBand->GetXSize() && nReqYSize == GetYSize() &&
            nReqYSize == poBand->GetYSize() && nOutXOff == 0 && nOutYOff == 0 &&
            nOutXSize == GetXSize() && nOutYSize == GetYSize())
        {
            return poBand->GetDataCoverageStatus(nXOff, nYOff, nXSize, nYSize,
                                                 nMaskFlagStop, pdfDataPct);
        }
    }

#ifndef HAVE_GEOS
    return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
           GDAL_DATA_COVERAGE_STATUS_DATA;
#else
    int nStatus = 0;

    auto poPolyNonCoveredBySources = std::make_unique<OGRPolygon>();
    {
        auto poLR = std::make_unique<OGRLinearRing>();
        poLR->addPoint(nXOff, nYOff);
        poLR->addPoint(nXOff, nYOff + nYSize);
        poLR->addPoint(nXOff + nXSize, nYOff + nYSize);
        poLR->addPoint(nXOff + nXSize, nYOff);
        poLR->addPoint(nXOff, nYOff);
        poPolyNonCoveredBySources->addRingDirectly(poLR.release());
    }

    for (int iSource = 0; iSource < nSources; iSource++)
    {
        if (!papoSources[iSource]->IsSimpleSource())
        {
            return GDAL_DATA_COVERAGE_STATUS_UNIMPLEMENTED |
                   GDAL_DATA_COVERAGE_STATUS_DATA;
        }
        VRTSimpleSource *poSS =
            static_cast<VRTSimpleSource *>(papoSources[iSource]);
        // Check if the AOI is fully inside the source
        double dfDstXOff = std::max(0.0, poSS->m_dfDstXOff);
        double dfDstYOff = std::max(0.0, poSS->m_dfDstYOff);
        double dfDstXSize = poSS->m_dfDstXSize;
        double dfDstYSize = poSS->m_dfDstYSize;
        auto l_poBand = poSS->GetRasterBand();
        if (!l_poBand)
            continue;
        if (dfDstXSize == -1)
            dfDstXSize = l_poBand->GetXSize() - dfDstXOff;
        if (dfDstYSize == -1)
            dfDstYSize = l_poBand->GetYSize() - dfDstYOff;

        if (nXOff >= dfDstXOff && nYOff >= dfDstYOff &&
            nXOff + nXSize <= dfDstXOff + dfDstXSize &&
            nYOff + nYSize <= dfDstYOff + dfDstYSize)
        {
            if (pdfDataPct)
                *pdfDataPct = 100.0;
            return GDAL_DATA_COVERAGE_STATUS_DATA;
        }
        // Check intersection of bounding boxes.
        if (dfDstXOff + dfDstXSize > nXOff && dfDstYOff + dfDstYSize > nYOff &&
            dfDstXOff < nXOff + nXSize && dfDstYOff < nYOff + nYSize)
        {
            nStatus |= GDAL_DATA_COVERAGE_STATUS_DATA;
            if (poPolyNonCoveredBySources)
            {
                OGRPolygon oPolySource;
                auto poLR = std::make_unique<OGRLinearRing>();
                poLR->addPoint(dfDstXOff, dfDstYOff);
                poLR->addPoint(dfDstXOff, dfDstYOff + dfDstYSize);
                poLR->addPoint(dfDstXOff + dfDstXSize, dfDstYOff + dfDstYSize);
                poLR->addPoint(dfDstXOff + dfDstXSize, dfDstYOff);
                poLR->addPoint(dfDstXOff, dfDstYOff);
                oPolySource.addRingDirectly(poLR.release());
                auto poRes = std::unique_ptr<OGRGeometry>(
                    poPolyNonCoveredBySources->Difference(&oPolySource));
                if (poRes && poRes->IsEmpty())
                {
                    if (pdfDataPct)
                        *pdfDataPct = 100.0;
                    return GDAL_DATA_COVERAGE_STATUS_DATA;
                }
                else if (poRes && poRes->getGeometryType() == wkbPolygon)
                {
                    poPolyNonCoveredBySources.reset(
                        poRes.release()->toPolygon());
                }
                else
                {
                    poPolyNonCoveredBySources.reset();
                }
            }
        }
        if (nMaskFlagStop != 0 && (nStatus & nMaskFlagStop) != 0)
        {
            return nStatus;
        }
    }
    if (poPolyNonCoveredBySources)
    {
        if (!poPolyNonCoveredBySources->IsEmpty())
            nStatus |= GDAL_DATA_COVERAGE_STATUS_EMPTY;
        if (pdfDataPct)
            *pdfDataPct = 100.0 * (1.0 - poPolyNonCoveredBySources->get_Area() /
                                             nXSize / nYSize);
    }
    return nStatus;
#endif  // HAVE_GEOS
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTSourcedRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                        void *pImage)

{
    const int nPixelSize = GDALGetDataTypeSizeBytes(eDataType);

    int nReadXSize = 0;
    if ((nBlockXOff + 1) * nBlockXSize > GetXSize())
        nReadXSize = GetXSize() - nBlockXOff * nBlockXSize;
    else
        nReadXSize = nBlockXSize;

    int nReadYSize = 0;
    if ((nBlockYOff + 1) * nBlockYSize > GetYSize())
        nReadYSize = GetYSize() - nBlockYOff * nBlockYSize;
    else
        nReadYSize = nBlockYSize;

    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);

    return IRasterIO(
        GF_Read, nBlockXOff * nBlockXSize, nBlockYOff * nBlockYSize, nReadXSize,
        nReadYSize, pImage, nReadXSize, nReadYSize, eDataType, nPixelSize,
        static_cast<GSpacing>(nPixelSize) * nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                        CPLGettimeofday()                             */
/************************************************************************/

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <sys/timeb.h>

namespace
{
struct CPLTimeVal
{
    time_t tv_sec; /* seconds */
    long tv_usec;  /* and microseconds */
};
}  // namespace

static int CPLGettimeofday(struct CPLTimeVal *tp, void * /* timezonep*/)
{
    struct _timeb theTime;

    _ftime(&theTime);
    tp->tv_sec = static_cast<time_t>(theTime.time);
    tp->tv_usec = theTime.millitm * 1000;
    return 0;
}
#else
#include <sys/time.h> /* for gettimeofday() */
#define CPLTimeVal timeval
#define CPLGettimeofday(t, u) gettimeofday(t, u)
#endif

/************************************************************************/
/*                    CanUseSourcesMinMaxImplementations()              */
/************************************************************************/

bool VRTSourcedRasterBand::CanUseSourcesMinMaxImplementations()
{
    const char *pszUseSources =
        CPLGetConfigOption("VRT_MIN_MAX_FROM_SOURCES", nullptr);
    if (pszUseSources)
        return CPLTestBool(pszUseSources);

    // Use heuristics to determine if we are going to use the source
    // GetMinimum() or GetMaximum() implementation: all the sources must be
    // "simple" sources with a dataset description that match a "regular" file
    // on the filesystem, whose open time and GetMinimum()/GetMaximum()
    // implementations we hope to be fast enough.
    // In case of doubt return FALSE.
    struct CPLTimeVal tvStart;
    memset(&tvStart, 0, sizeof(CPLTimeVal));
    if (nSources > 1)
        CPLGettimeofday(&tvStart, nullptr);
    for (int iSource = 0; iSource < nSources; iSource++)
    {
        if (!(papoSources[iSource]->IsSimpleSource()))
            return false;
        VRTSimpleSource *const poSimpleSource =
            static_cast<VRTSimpleSource *>(papoSources[iSource]);
        const char *pszFilename = poSimpleSource->m_osSrcDSName.c_str();
        // /vsimem/ should be fast.
        if (STARTS_WITH(pszFilename, "/vsimem/"))
            continue;
        // but not other /vsi filesystems
        if (STARTS_WITH(pszFilename, "/vsi"))
            return false;
        char ch = '\0';
        // We will assume that filenames that are only with ascii characters
        // are real filenames and so we will not try to 'stat' them.
        for (int i = 0; (ch = pszFilename[i]) != '\0'; i++)
        {
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                  (ch >= '0' && ch <= '9') || ch == ':' || ch == '/' ||
                  ch == '\\' || ch == ' ' || ch == '.' || ch == '_'))
                break;
        }
        if (ch != '\0')
        {
            // Otherwise do a real filesystem check.
            VSIStatBuf sStat;
            if (VSIStat(pszFilename, &sStat) != 0)
                return false;
            if (nSources > 1)
            {
                struct CPLTimeVal tvCur;
                CPLGettimeofday(&tvCur, nullptr);
                if (tvCur.tv_sec - tvStart.tv_sec +
                        (tvCur.tv_usec - tvStart.tv_usec) * 1e-6 >
                    1)
                    return false;
            }
        }
    }
    return true;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double VRTSourcedRasterBand::GetMinimum(int *pbSuccess)
{
    const char *const pszValue = GetMetadataItem("STATISTICS_MINIMUM");
    if (pszValue != nullptr)
    {
        if (pbSuccess != nullptr)
            *pbSuccess = TRUE;

        return CPLAtofM(pszValue);
    }

    if (!CanUseSourcesMinMaxImplementations())
        return GDALRasterBand::GetMinimum(pbSuccess);

    const std::string osFctId("VRTSourcedRasterBand::GetMinimum");
    GDALAntiRecursionGuard oGuard(osFctId);
    if (oGuard.GetCallDepth() >= 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        if (pbSuccess != nullptr)
            *pbSuccess = FALSE;
        return 0;
    }

    GDALAntiRecursionGuard oGuard2(oGuard, poDS->GetDescription());
    if (oGuard2.GetCallDepth() >= 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        if (pbSuccess != nullptr)
            *pbSuccess = FALSE;
        return 0;
    }

    struct CPLTimeVal tvStart;
    memset(&tvStart, 0, sizeof(CPLTimeVal));
    if (nSources > 1)
        CPLGettimeofday(&tvStart, nullptr);
    double dfMin = 0;
    for (int iSource = 0; iSource < nSources; iSource++)
    {
        int bSuccess = FALSE;
        double dfSourceMin =
            papoSources[iSource]->GetMinimum(GetXSize(), GetYSize(), &bSuccess);
        if (!bSuccess)
        {
            dfMin = GDALRasterBand::GetMinimum(pbSuccess);
            return dfMin;
        }

        if (iSource == 0 || dfSourceMin < dfMin)
        {
            dfMin = dfSourceMin;
            if (dfMin == 0 && eDataType == GDT_Byte)
                break;
        }
        if (nSources > 1)
        {
            struct CPLTimeVal tvCur;
            CPLGettimeofday(&tvCur, nullptr);
            if (tvCur.tv_sec - tvStart.tv_sec +
                    (tvCur.tv_usec - tvStart.tv_usec) * 1e-6 >
                1)
            {
                return GDALRasterBand::GetMinimum(pbSuccess);
            }
        }
    }

    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;

    return dfMin;
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double VRTSourcedRasterBand::GetMaximum(int *pbSuccess)
{
    const char *const pszValue = GetMetadataItem("STATISTICS_MAXIMUM");
    if (pszValue != nullptr)
    {
        if (pbSuccess != nullptr)
            *pbSuccess = TRUE;

        return CPLAtofM(pszValue);
    }

    if (!CanUseSourcesMinMaxImplementations())
        return GDALRasterBand::GetMaximum(pbSuccess);

    const std::string osFctId("VRTSourcedRasterBand::GetMaximum");
    GDALAntiRecursionGuard oGuard(osFctId);
    if (oGuard.GetCallDepth() >= 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        if (pbSuccess != nullptr)
            *pbSuccess = FALSE;
        return 0;
    }

    GDALAntiRecursionGuard oGuard2(oGuard, poDS->GetDescription());
    if (oGuard2.GetCallDepth() >= 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        if (pbSuccess != nullptr)
            *pbSuccess = FALSE;
        return 0;
    }

    struct CPLTimeVal tvStart;
    memset(&tvStart, 0, sizeof(CPLTimeVal));
    if (nSources > 1)
        CPLGettimeofday(&tvStart, nullptr);
    double dfMax = 0;
    for (int iSource = 0; iSource < nSources; iSource++)
    {
        int bSuccess = FALSE;
        const double dfSourceMax =
            papoSources[iSource]->GetMaximum(GetXSize(), GetYSize(), &bSuccess);
        if (!bSuccess)
        {
            dfMax = GDALRasterBand::GetMaximum(pbSuccess);
            return dfMax;
        }

        if (iSource == 0 || dfSourceMax > dfMax)
        {
            dfMax = dfSourceMax;
            if (dfMax == 255.0 && eDataType == GDT_Byte)
                break;
        }
        if (nSources > 1)
        {
            struct CPLTimeVal tvCur;
            CPLGettimeofday(&tvCur, nullptr);
            if (tvCur.tv_sec - tvStart.tv_sec +
                    (tvCur.tv_usec - tvStart.tv_usec) * 1e-6 >
                1)
            {
                return GDALRasterBand::GetMaximum(pbSuccess);
            }
        }
    }

    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;

    return dfMax;
}

/************************************************************************/
/* IsMosaicOfNonOverlappingSimpleSourcesOfFullRasterNoResAndTypeChange() */
/************************************************************************/

/* Returns true if the VRT raster band consists of non-overlapping simple
 * sources or complex sources that don't change values, and use the full extent
 * of the source band.
 */
bool VRTSourcedRasterBand::
    IsMosaicOfNonOverlappingSimpleSourcesOfFullRasterNoResAndTypeChange(
        bool bAllowMaxValAdjustment) const
{
    bool bRet = true;
    CPLRectObj sGlobalBounds;
    sGlobalBounds.minx = 0;
    sGlobalBounds.miny = 0;
    sGlobalBounds.maxx = nRasterXSize;
    sGlobalBounds.maxy = nRasterYSize;
    CPLQuadTree *hQuadTree = CPLQuadTreeCreate(&sGlobalBounds, nullptr);
    for (int i = 0; i < nSources; ++i)
    {
        if (!papoSources[i]->IsSimpleSource())
        {
            bRet = false;
            break;
        }

        auto poSimpleSource = cpl::down_cast<VRTSimpleSource *>(papoSources[i]);
        const char *pszType = poSimpleSource->GetType();
        if (pszType == VRTSimpleSource::GetTypeStatic())
        {
            // ok
        }
        else if (pszType == VRTComplexSource::GetTypeStatic())
        {
            auto poComplexSource =
                cpl::down_cast<VRTComplexSource *>(papoSources[i]);
            if (!poComplexSource->AreValuesUnchanged())
            {
                bRet = false;
                break;
            }
        }
        else
        {
            bRet = false;
            break;
        }

        if (!bAllowMaxValAdjustment && poSimpleSource->NeedMaxValAdjustment())
        {
            bRet = false;
            break;
        }

        auto poSimpleSourceBand = poSimpleSource->GetRasterBand();
        if (poSimpleSourceBand == nullptr ||
            poSimpleSourceBand->GetRasterDataType() != eDataType)
        {
            bRet = false;
            break;
        }

        double dfReqXOff = 0.0;
        double dfReqYOff = 0.0;
        double dfReqXSize = 0.0;
        double dfReqYSize = 0.0;
        int nReqXOff = 0;
        int nReqYOff = 0;
        int nReqXSize = 0;
        int nReqYSize = 0;
        int nOutXOff = 0;
        int nOutYOff = 0;
        int nOutXSize = 0;
        int nOutYSize = 0;

        bool bError = false;
        if (!poSimpleSource->GetSrcDstWindow(
                0, 0, nRasterXSize, nRasterYSize, nRasterXSize, nRasterYSize,
                &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize, &nReqXOff,
                &nReqYOff, &nReqXSize, &nReqYSize, &nOutXOff, &nOutYOff,
                &nOutXSize, &nOutYSize, bError) ||
            nReqXOff != 0 || nReqYOff != 0 ||
            nReqXSize != poSimpleSourceBand->GetXSize() ||
            nReqYSize != poSimpleSourceBand->GetYSize() ||
            nOutXSize != nReqXSize || nOutYSize != nReqYSize)
        {
            bRet = false;
            break;
        }

        CPLRectObj sBounds;
        constexpr double EPSILON = 1e-1;
        sBounds.minx = nOutXOff + EPSILON;
        sBounds.miny = nOutYOff + EPSILON;
        sBounds.maxx = nOutXOff + nOutXSize - EPSILON;
        sBounds.maxy = nOutYOff + nOutYSize - EPSILON;

        // Check that the new source doesn't overlap an existing one.
        if (CPLQuadTreeHasMatch(hQuadTree, &sBounds))
        {
            bRet = false;
            break;
        }

        CPLQuadTreeInsertWithBounds(
            hQuadTree, reinterpret_cast<void *>(static_cast<uintptr_t>(i)),
            &sBounds);
    }
    CPLQuadTreeDestroy(hQuadTree);

    return bRet;
}

/************************************************************************/
/*                       ComputeRasterMinMax()                          */
/************************************************************************/

CPLErr VRTSourcedRasterBand::ComputeRasterMinMax(int bApproxOK,
                                                 double *adfMinMax)
{
    /* -------------------------------------------------------------------- */
    /*      Does the driver already know the min/max?                       */
    /* -------------------------------------------------------------------- */
    if (bApproxOK)
    {
        int bSuccessMin = FALSE;
        int bSuccessMax = FALSE;

        const double dfMin = GetMinimum(&bSuccessMin);
        const double dfMax = GetMaximum(&bSuccessMax);

        if (bSuccessMin && bSuccessMax)
        {
            adfMinMax[0] = dfMin;
            adfMinMax[1] = dfMax;
            return CE_None;
        }
    }

    const std::string osFctId("VRTSourcedRasterBand::ComputeRasterMinMax");
    GDALAntiRecursionGuard oGuard(osFctId);
    if (oGuard.GetCallDepth() >= 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        return CE_Failure;
    }

    GDALAntiRecursionGuard oGuard2(oGuard, poDS->GetDescription());
    if (oGuard2.GetCallDepth() >= 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      If we have overview bands, use them for min/max.                */
    /* -------------------------------------------------------------------- */
    if (bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews())
    {
        GDALRasterBand *const poBand =
            GetRasterSampleOverview(GDALSTAT_APPROX_NUMSAMPLES);

        if (poBand != nullptr && poBand != this)
        {
            auto l_poDS = dynamic_cast<VRTDataset *>(poDS);
            if (l_poDS && !l_poDS->m_apoOverviews.empty() &&
                dynamic_cast<VRTSourcedRasterBand *>(poBand) != nullptr)
            {
                auto apoTmpOverviews = std::move(l_poDS->m_apoOverviews);
                l_poDS->m_apoOverviews.clear();
                auto eErr = poBand->GDALRasterBand::ComputeRasterMinMax(
                    TRUE, adfMinMax);
                l_poDS->m_apoOverviews = std::move(apoTmpOverviews);
                return eErr;
            }
            else
            {
                return poBand->ComputeRasterMinMax(TRUE, adfMinMax);
            }
        }
    }

    if (IsMosaicOfNonOverlappingSimpleSourcesOfFullRasterNoResAndTypeChange(
            /*bAllowMaxValAdjustment = */ true))
    {
        CPLDebugOnly(
            "VRT", "ComputeRasterMinMax(): use optimized code path for mosaic");

        uint64_t nCoveredArea = 0;

        // If source bands have nodata value, we can't use source band's
        // ComputeRasterMinMax() as we don't know if there are pixels actually
        // at the nodata value, so use ComputeStatistics() instead that takes
        // into account that aspect.
        bool bUseComputeStatistics = false;
        for (int i = 0; i < nSources; ++i)
        {
            auto poSimpleSource =
                cpl::down_cast<VRTSimpleSource *>(papoSources[i]);
            auto poSimpleSourceBand = poSimpleSource->GetRasterBand();
            int bHasNoData = FALSE;
            CPL_IGNORE_RET_VAL(poSimpleSourceBand->GetNoDataValue(&bHasNoData));
            if (bHasNoData)
            {
                bUseComputeStatistics = true;
                break;
            }
            nCoveredArea +=
                static_cast<uint64_t>(poSimpleSourceBand->GetXSize()) *
                poSimpleSourceBand->GetYSize();
        }

        if (bUseComputeStatistics)
        {
            CPLErr eErr;
            std::string osLastErrorMsg;
            {
                CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
                CPLErrorReset();
                eErr =
                    ComputeStatistics(bApproxOK, &adfMinMax[0], &adfMinMax[1],
                                      nullptr, nullptr, nullptr, nullptr);
                if (eErr == CE_Failure)
                {
                    osLastErrorMsg = CPLGetLastErrorMsg();
                }
            }
            if (eErr == CE_Failure)
            {
                if (strstr(osLastErrorMsg.c_str(), "no valid pixels found") !=
                    nullptr)
                {
                    ReportError(CE_Failure, CPLE_AppDefined,
                                "Failed to compute min/max, no valid pixels "
                                "found in sampling.");
                }
                else
                {
                    ReportError(CE_Failure, CPLE_AppDefined, "%s",
                                osLastErrorMsg.c_str());
                }
            }
            return eErr;
        }

        bool bSignedByte = false;
        if (eDataType == GDT_Byte)
        {
            EnablePixelTypeSignedByteWarning(false);
            const char *pszPixelType =
                GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
            EnablePixelTypeSignedByteWarning(true);
            bSignedByte =
                pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE");
        }

        double dfGlobalMin = std::numeric_limits<double>::max();
        double dfGlobalMax = -std::numeric_limits<double>::max();

        // If the mosaic doesn't cover the whole VRT raster, take into account
        // VRT nodata value.
        if (nCoveredArea < static_cast<uint64_t>(nRasterXSize) * nRasterYSize)
        {
            if (m_bNoDataValueSet && m_bHideNoDataValue)
            {
                if (IsNoDataValueInDataTypeRange())
                {
                    dfGlobalMin = std::min(dfGlobalMin, m_dfNoDataValue);
                    dfGlobalMax = std::max(dfGlobalMax, m_dfNoDataValue);
                }
            }
            else if (!m_bNoDataValueSet)
            {
                dfGlobalMin = std::min(dfGlobalMin, 0.0);
                dfGlobalMax = std::max(dfGlobalMax, 0.0);
            }
        }

        for (int i = 0; i < nSources; ++i)
        {
            auto poSimpleSource =
                cpl::down_cast<VRTSimpleSource *>(papoSources[i]);
            double adfMinMaxSource[2] = {0};

            auto poSimpleSourceBand = poSimpleSource->GetRasterBand();
            CPLErr eErr = poSimpleSourceBand->ComputeRasterMinMax(
                bApproxOK, adfMinMaxSource);
            if (eErr == CE_Failure)
            {
                return CE_Failure;
            }
            else
            {
                if (poSimpleSource->NeedMaxValAdjustment())
                {
                    const double dfMaxValue =
                        static_cast<double>(poSimpleSource->m_nMaxValue);
                    adfMinMaxSource[0] =
                        std::min(adfMinMaxSource[0], dfMaxValue);
                    adfMinMaxSource[1] =
                        std::min(adfMinMaxSource[1], dfMaxValue);
                }

                if (m_bNoDataValueSet && !m_bHideNoDataValue &&
                    m_dfNoDataValue >= adfMinMaxSource[0] &&
                    m_dfNoDataValue <= adfMinMaxSource[1])
                {
                    return GDALRasterBand::ComputeRasterMinMax(bApproxOK,
                                                               adfMinMax);
                }

                dfGlobalMin = std::min(dfGlobalMin, adfMinMaxSource[0]);
                dfGlobalMax = std::max(dfGlobalMax, adfMinMaxSource[1]);
            }

            // Early exit if we know we reached theoretical bounds
            if (eDataType == GDT_Byte && !bSignedByte && dfGlobalMin == 0.0 &&
                dfGlobalMax == 255.0)
            {
                break;
            }
        }

        if (dfGlobalMin > dfGlobalMax)
        {
            adfMinMax[0] = 0.0;
            adfMinMax[1] = 0.0;
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Failed to compute min/max, no valid pixels found in "
                        "sampling.");
            return CE_Failure;
        }

        adfMinMax[0] = dfGlobalMin;
        adfMinMax[1] = dfGlobalMax;
        return CE_None;
    }
    else
    {
        return GDALRasterBand::ComputeRasterMinMax(bApproxOK, adfMinMax);
    }
}

// #define naive_update_not_used

namespace
{
struct Context
{
    CPL_DISALLOW_COPY_ASSIGN(Context)
    Context() = default;

    // Protected by mutex
    std::mutex oMutex{};
    uint64_t nTotalIteratedPixels = 0;
    uint64_t nLastReportedPixels = 0;
    bool bFailure = false;
    bool bFallbackToBase = false;
    // End of protected by mutex

    bool bApproxOK = false;
    GDALProgressFunc pfnProgress = nullptr;
    void *pProgressData = nullptr;

    // VRTSourcedRasterBand parameters
    double dfNoDataValue = 0;
    bool bNoDataValueSet = false;
    bool bHideNoDataValue = false;

    double dfGlobalMin = std::numeric_limits<double>::max();
    double dfGlobalMax = -std::numeric_limits<double>::max();
#ifdef naive_update_not_used
    // This native method uses the fact that stddev = sqrt(sum_of_squares/N -
    // mean^2) and that thus sum_of_squares = N * (stddev^2 + mean^2)
    double dfGlobalSum = 0;
    double dfGlobalSumSquare = 0;
#else
    // This method uses
    // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Parallel_algorithm
    // which is more numerically robust
    double dfGlobalMean = 0;
    double dfGlobalM2 = 0;
#endif
    uint64_t nGlobalValidPixels = 0;
    uint64_t nTotalPixelsOfSources = 0;
};
}  // namespace

static void UpdateStatsWithConstantValue(Context &sContext, double dfVal,
                                         uint64_t nPixelCount)
{
    sContext.dfGlobalMin = std::min(sContext.dfGlobalMin, dfVal);
    sContext.dfGlobalMax = std::max(sContext.dfGlobalMax, dfVal);
#ifdef naive_update_not_used
    sContext.dfGlobalSum += dfVal * nPixelCount;
    sContext.dfGlobalSumSquare += dfVal * dfVal * nPixelCount;
#else
    const auto nNewGlobalValidPixels =
        sContext.nGlobalValidPixels + nPixelCount;
    const double dfDelta = dfVal - sContext.dfGlobalMean;
    sContext.dfGlobalMean += nPixelCount * dfDelta / nNewGlobalValidPixels;
    sContext.dfGlobalM2 += dfDelta * dfDelta * nPixelCount *
                           sContext.nGlobalValidPixels / nNewGlobalValidPixels;
#endif
    sContext.nGlobalValidPixels += nPixelCount;
}

/************************************************************************/
/*                         ComputeStatistics()                          */
/************************************************************************/

CPLErr VRTSourcedRasterBand::ComputeStatistics(int bApproxOK, double *pdfMin,
                                               double *pdfMax, double *pdfMean,
                                               double *pdfStdDev,
                                               GDALProgressFunc pfnProgress,
                                               void *pProgressData)

{
    const std::string osFctId("VRTSourcedRasterBand::ComputeStatistics");
    GDALAntiRecursionGuard oGuard(osFctId);
    if (oGuard.GetCallDepth() >= 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        return CE_Failure;
    }

    GDALAntiRecursionGuard oGuard2(oGuard, poDS->GetDescription());
    if (oGuard2.GetCallDepth() >= 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      If we have overview bands, use them for statistics.             */
    /* -------------------------------------------------------------------- */
    if (bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews())
    {
        GDALRasterBand *const poBand =
            GetRasterSampleOverview(GDALSTAT_APPROX_NUMSAMPLES);

        if (poBand != nullptr && poBand != this)
        {
            auto l_poDS = dynamic_cast<VRTDataset *>(poDS);
            CPLErr eErr;
            if (l_poDS && !l_poDS->m_apoOverviews.empty() &&
                dynamic_cast<VRTSourcedRasterBand *>(poBand) != nullptr)
            {
                auto apoTmpOverviews = std::move(l_poDS->m_apoOverviews);
                l_poDS->m_apoOverviews.clear();
                eErr = poBand->GDALRasterBand::ComputeStatistics(
                    TRUE, pdfMin, pdfMax, pdfMean, pdfStdDev, pfnProgress,
                    pProgressData);
                l_poDS->m_apoOverviews = std::move(apoTmpOverviews);
            }
            else
            {
                eErr = poBand->ComputeStatistics(TRUE, pdfMin, pdfMax, pdfMean,
                                                 pdfStdDev, pfnProgress,
                                                 pProgressData);
            }
            if (eErr == CE_None && pdfMin && pdfMax && pdfMean && pdfStdDev)
            {
                SetMetadataItem("STATISTICS_APPROXIMATE", "YES");
                SetMetadataItem(
                    "STATISTICS_VALID_PERCENT",
                    poBand->GetMetadataItem("STATISTICS_VALID_PERCENT"));
                SetStatistics(*pdfMin, *pdfMax, *pdfMean, *pdfStdDev);
            }
            return eErr;
        }
    }

    if (IsMosaicOfNonOverlappingSimpleSourcesOfFullRasterNoResAndTypeChange(
            /*bAllowMaxValAdjustment = */ false))
    {
        Context sContext;
        sContext.bApproxOK = bApproxOK;
        sContext.dfNoDataValue = m_dfNoDataValue;
        sContext.bNoDataValueSet = m_bNoDataValueSet;
        sContext.bHideNoDataValue = m_bHideNoDataValue;
        sContext.pfnProgress = pfnProgress;
        sContext.pProgressData = pProgressData;

        struct Job
        {
            Context *psContext = nullptr;
            GDALRasterBand *poRasterBand = nullptr;
            uint64_t nPixelCount = 0;
            uint64_t nLastAddedPixels = 0;
            uint64_t nValidPixels = 0;
            double dfMin = 0;
            double dfMax = 0;
            double dfMean = 0;
            double dfStdDev = 0;

            static int CPL_STDCALL ProgressFunc(double dfComplete,
                                                const char *pszMessage,
                                                void *pProgressArg)
            {
                auto psJob = static_cast<Job *>(pProgressArg);
                auto psContext = psJob->psContext;
                const uint64_t nNewAddedPixels =
                    dfComplete == 1.0
                        ? psJob->nPixelCount
                        : static_cast<uint64_t>(
                              dfComplete * psJob->nPixelCount + 0.5);
                const auto nUpdateThreshold =
                    std::min(psContext->nTotalPixelsOfSources / 1000,
                             static_cast<uint64_t>(1000 * 1000));
                std::lock_guard<std::mutex> oLock(psContext->oMutex);
                psContext->nTotalIteratedPixels +=
                    (nNewAddedPixels - psJob->nLastAddedPixels);
                psJob->nLastAddedPixels = nNewAddedPixels;
                if (psContext->nTotalIteratedPixels ==
                    psContext->nTotalPixelsOfSources)
                {
                    psContext->nLastReportedPixels =
                        psContext->nTotalIteratedPixels;
                    return psContext->pfnProgress(1.0, pszMessage,
                                                  psContext->pProgressData);
                }
                else if (psContext->nTotalIteratedPixels -
                             psContext->nLastReportedPixels >
                         nUpdateThreshold)
                {
                    psContext->nLastReportedPixels =
                        psContext->nTotalIteratedPixels;
                    return psContext->pfnProgress(
                        static_cast<double>(psContext->nTotalIteratedPixels) /
                            psContext->nTotalPixelsOfSources,
                        pszMessage, psContext->pProgressData);
                }
                return 1;
            }

            static void UpdateStats(const Job *psJob)
            {
                const auto nValidPixels = psJob->nValidPixels;
                auto psContext = psJob->psContext;
                if (nValidPixels > 0)
                {
                    psContext->dfGlobalMin =
                        std::min(psContext->dfGlobalMin, psJob->dfMin);
                    psContext->dfGlobalMax =
                        std::max(psContext->dfGlobalMax, psJob->dfMax);
#ifdef naive_update_not_used
                    psContext->dfGlobalSum += nValidPixels * psJob->dfMean;
                    psContext->dfGlobalSumSquare +=
                        nValidPixels * (psJob->dfStdDev * psJob->dfStdDev +
                                        psJob->dfMean * psJob->dfMean);
                    psContext->nGlobalValidPixels += nValidPixels;
#else
                    const auto nNewGlobalValidPixels =
                        psContext->nGlobalValidPixels + nValidPixels;
                    const double dfDelta =
                        psJob->dfMean - psContext->dfGlobalMean;
                    psContext->dfGlobalMean +=
                        nValidPixels * dfDelta / nNewGlobalValidPixels;
                    psContext->dfGlobalM2 +=
                        nValidPixels * psJob->dfStdDev * psJob->dfStdDev +
                        dfDelta * dfDelta * nValidPixels *
                            psContext->nGlobalValidPixels /
                            nNewGlobalValidPixels;
                    psContext->nGlobalValidPixels = nNewGlobalValidPixels;
#endif
                }
                int bHasNoData = FALSE;
                const double dfNoDataValue =
                    psJob->poRasterBand->GetNoDataValue(&bHasNoData);
                if (nValidPixels < psJob->nPixelCount && bHasNoData &&
                    !std::isnan(dfNoDataValue) &&
                    (!psContext->bNoDataValueSet ||
                     dfNoDataValue != psContext->dfNoDataValue))
                {
                    const auto eBandDT =
                        psJob->poRasterBand->GetRasterDataType();
                    // Check that the band nodata value is in the range of the
                    // original raster type
                    GByte abyTempBuffer[2 * sizeof(double)];
                    CPLAssert(GDALGetDataTypeSizeBytes(eBandDT) <=
                              static_cast<int>(sizeof(abyTempBuffer)));
                    GDALCopyWords(&dfNoDataValue, GDT_Float64, 0,
                                  &abyTempBuffer[0], eBandDT, 0, 1);
                    double dfNoDataValueAfter = dfNoDataValue;
                    GDALCopyWords(&abyTempBuffer[0], eBandDT, 0,
                                  &dfNoDataValueAfter, GDT_Float64, 0, 1);
                    if (!std::isfinite(dfNoDataValue) ||
                        std::fabs(dfNoDataValueAfter - dfNoDataValue) < 1.0)
                    {
                        UpdateStatsWithConstantValue(
                            *psContext, dfNoDataValueAfter,
                            psJob->nPixelCount - nValidPixels);
                    }
                }
            }
        };

        const auto JobRunner = [](void *pData)
        {
            auto psJob = static_cast<Job *>(pData);
            auto psContext = psJob->psContext;
            {
                std::lock_guard<std::mutex> oLock(psContext->oMutex);
                if (psContext->bFallbackToBase || psContext->bFailure)
                    return;
            }

            auto poSimpleSourceBand = psJob->poRasterBand;
            psJob->nPixelCount =
                static_cast<uint64_t>(poSimpleSourceBand->GetXSize()) *
                poSimpleSourceBand->GetYSize();

            CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
            CPLErr eErr = poSimpleSourceBand->ComputeStatistics(
                psContext->bApproxOK, &psJob->dfMin, &psJob->dfMax,
                &psJob->dfMean, &psJob->dfStdDev,
                psContext->pfnProgress == nullptr ||
                        psContext->pfnProgress == GDALDummyProgress
                    ? GDALDummyProgress
                    : Job::ProgressFunc,
                psJob);
            const char *pszValidPercent =
                poSimpleSourceBand->GetMetadataItem("STATISTICS_VALID_PERCENT");
            psJob->nValidPixels =
                pszValidPercent
                    ? static_cast<uint64_t>(CPLAtof(pszValidPercent) *
                                            psJob->nPixelCount / 100.0)
                    : psJob->nPixelCount;
            if (eErr == CE_Failure)
            {
                if (pszValidPercent != nullptr &&
                    CPLAtof(pszValidPercent) == 0.0)
                {
                    // ok: no valid sample
                }
                else
                {
                    std::lock_guard<std::mutex> oLock(psContext->oMutex);
                    psContext->bFailure = true;
                }
            }
            else
            {
                int bHasNoData = FALSE;
                CPL_IGNORE_RET_VAL(
                    psJob->poRasterBand->GetNoDataValue(&bHasNoData));
                if (!bHasNoData && psContext->bNoDataValueSet &&
                    !psContext->bHideNoDataValue &&
                    psContext->dfNoDataValue >= psJob->dfMin &&
                    psContext->dfNoDataValue <= psJob->dfMax)
                {
                    std::lock_guard<std::mutex> oLock(psContext->oMutex);
                    psJob->psContext->bFallbackToBase = true;
                    return;
                }
            }
        };

        CPLWorkerThreadPool *poThreadPool = nullptr;
        int nThreads =
            nSources > 1
                ? VRTDataset::GetNumThreads(dynamic_cast<VRTDataset *>(poDS))
                : 0;
        if (nThreads > 1024)
            nThreads = 1024;  // to please Coverity
        if (nThreads > 1)
        {
            // Check that all sources refer to different datasets
            // before allowing multithreaded access
            // If the datasets belong to the MEM driver, check GDALDataset*
            // pointer values. Otherwise use dataset name.
            std::set<std::string> oSetDatasetNames;
            std::set<GDALDataset *> oSetDatasetPointers;
            for (int i = 0; i < nSources; ++i)
            {
                auto poSimpleSource =
                    cpl::down_cast<VRTSimpleSource *>(papoSources[i]);
                assert(poSimpleSource);
                auto poSimpleSourceBand = poSimpleSource->GetRasterBand();
                assert(poSimpleSourceBand);
                auto poSourceDataset = poSimpleSourceBand->GetDataset();
                if (poSourceDataset == nullptr)
                {
                    nThreads = 0;
                    break;
                }
                auto poDriver = poSourceDataset->GetDriver();
                if (poDriver && EQUAL(poDriver->GetDescription(), "MEM"))
                {
                    if (oSetDatasetPointers.find(poSourceDataset) !=
                        oSetDatasetPointers.end())
                    {
                        nThreads = 0;
                        break;
                    }
                    oSetDatasetPointers.insert(poSourceDataset);
                }
                else
                {
                    if (oSetDatasetNames.find(
                            poSourceDataset->GetDescription()) !=
                        oSetDatasetNames.end())
                    {
                        nThreads = 0;
                        break;
                    }
                    oSetDatasetNames.insert(poSourceDataset->GetDescription());
                }
            }
            if (nThreads > 1)
            {
                poThreadPool = GDALGetGlobalThreadPool(nThreads);
            }
        }

        // Compute total number of pixels of sources
        for (int i = 0; i < nSources; ++i)
        {
            auto poSimpleSource =
                static_cast<VRTSimpleSource *>(papoSources[i]);
            assert(poSimpleSource);
            auto poSimpleSourceBand = poSimpleSource->GetRasterBand();
            assert(poSimpleSourceBand);
            sContext.nTotalPixelsOfSources +=
                static_cast<uint64_t>(poSimpleSourceBand->GetXSize()) *
                poSimpleSourceBand->GetYSize();
        }

        if (poThreadPool)
        {
            CPLDebugOnly("VRT", "ComputeStatistics(): use optimized "
                                "multi-threaded code path for mosaic");
            std::vector<Job> asJobs(nSources);
            auto poQueue = poThreadPool->CreateJobQueue();
            for (int i = 0; i < nSources; ++i)
            {
                auto poSimpleSource =
                    static_cast<VRTSimpleSource *>(papoSources[i]);
                assert(poSimpleSource);
                auto poSimpleSourceBand = poSimpleSource->GetRasterBand();
                assert(poSimpleSourceBand);
                asJobs[i].psContext = &sContext;
                asJobs[i].poRasterBand = poSimpleSourceBand;
                if (!poQueue->SubmitJob(JobRunner, &asJobs[i]))
                {
                    sContext.bFailure = true;
                    break;
                }
            }
            poQueue->WaitCompletion();
            if (!(sContext.bFailure || sContext.bFallbackToBase))
            {
                for (int i = 0; i < nSources; ++i)
                {
                    Job::UpdateStats(&asJobs[i]);
                }
            }
        }
        else
        {
            CPLDebugOnly(
                "VRT",
                "ComputeStatistics(): use optimized code path for mosaic");
            for (int i = 0; i < nSources; ++i)
            {
                auto poSimpleSource =
                    static_cast<VRTSimpleSource *>(papoSources[i]);
                assert(poSimpleSource);
                auto poSimpleSourceBand = poSimpleSource->GetRasterBand();
                assert(poSimpleSourceBand);
                Job sJob;
                sJob.psContext = &sContext;
                sJob.poRasterBand = poSimpleSourceBand;
                JobRunner(&sJob);
                if (sContext.bFailure || sContext.bFallbackToBase)
                    break;
                Job::UpdateStats(&sJob);
            }
        }

        if (sContext.bFailure)
            return CE_Failure;
        if (sContext.bFallbackToBase)
        {
            // If the VRT band nodata value is in the [min, max] range
            // of the source and that the source has no nodata value set,
            // then we can't use the optimization.
            CPLDebugOnly("VRT", "ComputeStatistics(): revert back to "
                                "generic case because of nodata value in range "
                                "of source raster");
            return GDALRasterBand::ComputeStatistics(
                bApproxOK, pdfMin, pdfMax, pdfMean, pdfStdDev, pfnProgress,
                pProgressData);
        }

        const uint64_t nTotalPixels =
            static_cast<uint64_t>(nRasterXSize) * nRasterYSize;
        if (m_bNoDataValueSet && m_bHideNoDataValue &&
            !std::isnan(m_dfNoDataValue) && IsNoDataValueInDataTypeRange())
        {
            UpdateStatsWithConstantValue(sContext, m_dfNoDataValue,
                                         nTotalPixels -
                                             sContext.nGlobalValidPixels);
        }
        else if (!m_bNoDataValueSet)
        {
            sContext.nGlobalValidPixels = nTotalPixels;
        }

#ifdef naive_update_not_used
        const double dfGlobalMean =
            sContext.nGlobalValidPixels > 0
                ? sContext.dfGlobalSum / sContext.nGlobalValidPixels
                : 0;
        const double dfGlobalStdDev =
            sContext.nGlobalValidPixels > 0
                ? sqrt(sContext.dfGlobalSumSquare /
                           sContext.nGlobalValidPixels -
                       dfGlobalMean * dfGlobalMean)
                : 0;
#else
        const double dfGlobalMean = sContext.dfGlobalMean;
        const double dfGlobalStdDev =
            sContext.nGlobalValidPixels > 0
                ? sqrt(sContext.dfGlobalM2 / sContext.nGlobalValidPixels)
                : 0;
#endif
        if (sContext.nGlobalValidPixels > 0)
        {
            if (bApproxOK)
            {
                SetMetadataItem("STATISTICS_APPROXIMATE", "YES");
            }
            else if (GetMetadataItem("STATISTICS_APPROXIMATE"))
            {
                SetMetadataItem("STATISTICS_APPROXIMATE", nullptr);
            }
            SetStatistics(sContext.dfGlobalMin, sContext.dfGlobalMax,
                          dfGlobalMean, dfGlobalStdDev);
        }
        else
        {
            sContext.dfGlobalMin = 0.0;
            sContext.dfGlobalMax = 0.0;
        }

        SetValidPercent(nTotalPixels, sContext.nGlobalValidPixels);

        if (pdfMin)
            *pdfMin = sContext.dfGlobalMin;
        if (pdfMax)
            *pdfMax = sContext.dfGlobalMax;
        if (pdfMean)
            *pdfMean = dfGlobalMean;
        if (pdfStdDev)
            *pdfStdDev = dfGlobalStdDev;

        if (sContext.nGlobalValidPixels == 0)
        {
            ReportError(CE_Failure, CPLE_AppDefined,
                        "Failed to compute statistics, no valid pixels found "
                        "in sampling.");
        }

        return sContext.nGlobalValidPixels > 0 ? CE_None : CE_Failure;
    }
    else
    {
        return GDALRasterBand::ComputeStatistics(bApproxOK, pdfMin, pdfMax,
                                                 pdfMean, pdfStdDev,
                                                 pfnProgress, pProgressData);
    }
}

/************************************************************************/
/*                            GetHistogram()                            */
/************************************************************************/

CPLErr VRTSourcedRasterBand::GetHistogram(double dfMin, double dfMax,
                                          int nBuckets, GUIntBig *panHistogram,
                                          int bIncludeOutOfRange, int bApproxOK,
                                          GDALProgressFunc pfnProgress,
                                          void *pProgressData)

{
    /* -------------------------------------------------------------------- */
    /*      If we have overviews, use them for the histogram.               */
    /* -------------------------------------------------------------------- */
    if (bApproxOK && GetOverviewCount() > 0 && !HasArbitraryOverviews())
    {
        // FIXME: Should we use the most reduced overview here or use some
        // minimum number of samples like GDALRasterBand::ComputeStatistics()
        // does?
        GDALRasterBand *poBand = GetRasterSampleOverview(0);

        if (poBand != nullptr && poBand != this)
        {
            auto l_poDS = dynamic_cast<VRTDataset *>(poDS);
            if (l_poDS && !l_poDS->m_apoOverviews.empty() &&
                dynamic_cast<VRTSourcedRasterBand *>(poBand) != nullptr)
            {
                auto apoTmpOverviews = std::move(l_poDS->m_apoOverviews);
                l_poDS->m_apoOverviews.clear();
                auto eErr = poBand->GDALRasterBand::GetHistogram(
                    dfMin, dfMax, nBuckets, panHistogram, bIncludeOutOfRange,
                    bApproxOK, pfnProgress, pProgressData);
                l_poDS->m_apoOverviews = std::move(apoTmpOverviews);
                return eErr;
            }
            else
            {
                return poBand->GetHistogram(
                    dfMin, dfMax, nBuckets, panHistogram, bIncludeOutOfRange,
                    bApproxOK, pfnProgress, pProgressData);
            }
        }
    }

    if (nSources != 1)
        return VRTRasterBand::GetHistogram(dfMin, dfMax, nBuckets, panHistogram,
                                           bIncludeOutOfRange, bApproxOK,
                                           pfnProgress, pProgressData);

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    const std::string osFctId("VRTSourcedRasterBand::GetHistogram");
    GDALAntiRecursionGuard oGuard(osFctId);
    if (oGuard.GetCallDepth() >= 32)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        return CE_Failure;
    }

    GDALAntiRecursionGuard oGuard2(oGuard, poDS->GetDescription());
    if (oGuard2.GetCallDepth() >= 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Recursion detected");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Try with source bands.                                          */
    /* -------------------------------------------------------------------- */
    const CPLErr eErr = papoSources[0]->GetHistogram(
        GetXSize(), GetYSize(), dfMin, dfMax, nBuckets, panHistogram,
        bIncludeOutOfRange, bApproxOK, pfnProgress, pProgressData);
    if (eErr != CE_None)
    {
        const CPLErr eErr2 = GDALRasterBand::GetHistogram(
            dfMin, dfMax, nBuckets, panHistogram, bIncludeOutOfRange, bApproxOK,
            pfnProgress, pProgressData);
        return eErr2;
    }

    SetDefaultHistogram(dfMin, dfMax, nBuckets, panHistogram);

    return CE_None;
}

/************************************************************************/
/*                             AddSource()                              */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddSource(VRTSource *poNewSource)

{
    nSources++;

    papoSources = static_cast<VRTSource **>(
        CPLRealloc(papoSources, sizeof(void *) * nSources));
    papoSources[nSources - 1] = poNewSource;

    auto l_poDS = static_cast<VRTDataset *>(poDS);
    l_poDS->SetNeedsFlush();
    l_poDS->SourceAdded();

    if (poNewSource->IsSimpleSource())
    {
        VRTSimpleSource *poSS = static_cast<VRTSimpleSource *>(poNewSource);
        if (GetMetadataItem("NBITS", "IMAGE_STRUCTURE") != nullptr)
        {
            int nBits = atoi(GetMetadataItem("NBITS", "IMAGE_STRUCTURE"));
            if (nBits >= 1 && nBits <= 31)
            {
                poSS->SetMaxValue(static_cast<int>((1U << nBits) - 1));
            }
        }
    }

    return CE_None;
}

/*! @endcond */

/************************************************************************/
/*                              VRTAddSource()                          */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddSource().
 */

CPLErr CPL_STDCALL VRTAddSource(VRTSourcedRasterBandH hVRTBand,
                                VRTSourceH hNewSource)
{
    VALIDATE_POINTER1(hVRTBand, "VRTAddSource", CE_Failure);

    return reinterpret_cast<VRTSourcedRasterBand *>(hVRTBand)->AddSource(
        reinterpret_cast<VRTSource *>(hNewSource));
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTSourcedRasterBand::XMLInit(const CPLXMLNode *psTree,
                                     const char *pszVRTPath,
                                     VRTMapSharedResources &oMapSharedSources)

{
    {
        const CPLErr eErr =
            VRTRasterBand::XMLInit(psTree, pszVRTPath, oMapSharedSources);
        if (eErr != CE_None)
            return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Process sources.                                                */
    /* -------------------------------------------------------------------- */
    VRTDriver *const poDriver =
        static_cast<VRTDriver *>(GDALGetDriverByName("VRT"));

    for (const CPLXMLNode *psChild = psTree->psChild;
         psChild != nullptr && poDriver != nullptr; psChild = psChild->psNext)
    {
        if (psChild->eType != CXT_Element)
            continue;

        CPLErrorReset();
        VRTSource *const poSource =
            poDriver->ParseSource(psChild, pszVRTPath, oMapSharedSources);
        if (poSource != nullptr)
            AddSource(poSource);
        else if (CPLGetLastErrorType() != CE_None)
            return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Done.                                                           */
    /* -------------------------------------------------------------------- */
    const char *pszSubclass =
        CPLGetXMLValue(psTree, "subclass", "VRTSourcedRasterBand");
    if (nSources == 0 && !EQUAL(pszSubclass, "VRTDerivedRasterBand"))
        CPLDebug("VRT", "No valid sources found for band in VRT file %s",
                 GetDataset() ? GetDataset()->GetDescription() : "");

    return CE_None;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTSourcedRasterBand::SerializeToXML(const char *pszVRTPath,
                                                 bool &bHasWarnedAboutRAMUsage,
                                                 size_t &nAccRAMUsage)

{
    CPLXMLNode *psTree = VRTRasterBand::SerializeToXML(
        pszVRTPath, bHasWarnedAboutRAMUsage, nAccRAMUsage);
    CPLXMLNode *psLastChild = psTree->psChild;
    while (psLastChild != nullptr && psLastChild->psNext != nullptr)
        psLastChild = psLastChild->psNext;

    /* -------------------------------------------------------------------- */
    /*      Process Sources.                                                */
    /* -------------------------------------------------------------------- */

    GIntBig nUsableRAM = -1;

    for (int iSource = 0; iSource < nSources; iSource++)
    {
        CPLXMLNode *const psXMLSrc =
            papoSources[iSource]->SerializeToXML(pszVRTPath);

        if (psXMLSrc == nullptr)
            break;

        // Creating the CPLXMLNode tree representation of a VRT can easily
        // take several times RAM usage than its string serialization, or its
        // internal representation in the driver.
        // We multiply the estimate by a factor of 2, experimentally found to
        // be more realistic than the conservative raw estimate.
        nAccRAMUsage += 2 * CPLXMLNodeGetRAMUsageEstimate(psXMLSrc);
        if (!bHasWarnedAboutRAMUsage && nAccRAMUsage > 512 * 1024 * 1024)
        {
            if (nUsableRAM < 0)
                nUsableRAM = CPLGetUsablePhysicalRAM();
            if (nUsableRAM > 0 &&
                nAccRAMUsage > static_cast<uint64_t>(nUsableRAM) / 10 * 8)
            {
                bHasWarnedAboutRAMUsage = true;
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Serialization of this VRT file has already consumed "
                         "at least %.02f GB of RAM over a total of %.02f. This "
                         "process may abort",
                         double(nAccRAMUsage) / (1024 * 1024 * 1024),
                         double(nUsableRAM) / (1024 * 1024 * 1024));
            }
        }

        if (psLastChild == nullptr)
            psTree->psChild = psXMLSrc;
        else
            psLastChild->psNext = psXMLSrc;
        psLastChild = psXMLSrc;
    }

    return psTree;
}

/************************************************************************/
/*                     SkipBufferInitialization()                       */
/************************************************************************/

bool VRTSourcedRasterBand::SkipBufferInitialization()
{
    if (m_nSkipBufferInitialization >= 0)
        return m_nSkipBufferInitialization != 0;
    /* -------------------------------------------------------------------- */
    /*      Check if we can avoid buffer initialization.                    */
    /* -------------------------------------------------------------------- */

    // Note: if one day we do alpha compositing, we will need to check that.
    m_nSkipBufferInitialization = FALSE;
    if (nSources != 1 || !papoSources[0]->IsSimpleSource())
    {
        return false;
    }
    VRTSimpleSource *poSS = static_cast<VRTSimpleSource *>(papoSources[0]);
    if (poSS->GetType() == VRTSimpleSource::GetTypeStatic())
    {
        auto l_poBand = poSS->GetRasterBand();
        if (l_poBand != nullptr && poSS->m_dfSrcXOff >= 0.0 &&
            poSS->m_dfSrcYOff >= 0.0 &&
            poSS->m_dfSrcXOff + poSS->m_dfSrcXSize <= l_poBand->GetXSize() &&
            poSS->m_dfSrcYOff + poSS->m_dfSrcYSize <= l_poBand->GetYSize() &&
            poSS->m_dfDstXOff <= 0.0 && poSS->m_dfDstYOff <= 0.0 &&
            poSS->m_dfDstXOff + poSS->m_dfDstXSize >= nRasterXSize &&
            poSS->m_dfDstYOff + poSS->m_dfDstYSize >= nRasterYSize)
        {
            m_nSkipBufferInitialization = TRUE;
        }
    }
    return m_nSkipBufferInitialization != 0;
}

/************************************************************************/
/*                          ConfigureSource()                           */
/************************************************************************/

void VRTSourcedRasterBand::ConfigureSource(VRTSimpleSource *poSimpleSource,
                                           GDALRasterBand *poSrcBand,
                                           int bAddAsMaskBand, double dfSrcXOff,
                                           double dfSrcYOff, double dfSrcXSize,
                                           double dfSrcYSize, double dfDstXOff,
                                           double dfDstYOff, double dfDstXSize,
                                           double dfDstYSize)
{
    /* -------------------------------------------------------------------- */
    /*      Default source and dest rectangles.                             */
    /* -------------------------------------------------------------------- */
    if (dfSrcYSize == -1)
    {
        dfSrcXOff = 0;
        dfSrcYOff = 0;
        dfSrcXSize = poSrcBand->GetXSize();
        dfSrcYSize = poSrcBand->GetYSize();
    }

    if (dfDstYSize == -1)
    {
        dfDstXOff = 0;
        dfDstYOff = 0;
        dfDstXSize = nRasterXSize;
        dfDstYSize = nRasterYSize;
    }

    if (bAddAsMaskBand)
        poSimpleSource->SetSrcMaskBand(poSrcBand);
    else
        poSimpleSource->SetSrcBand(poSrcBand);

    poSimpleSource->SetSrcWindow(dfSrcXOff, dfSrcYOff, dfSrcXSize, dfSrcYSize);
    poSimpleSource->SetDstWindow(dfDstXOff, dfDstYOff, dfDstXSize, dfDstYSize);

    /* -------------------------------------------------------------------- */
    /*      If we can get the associated GDALDataset, add a reference to it.*/
    /* -------------------------------------------------------------------- */
    GDALDataset *poSrcBandDataset = poSrcBand->GetDataset();
    if (poSrcBandDataset != nullptr)
    {
        VRTDataset *poVRTSrcBandDataset =
            dynamic_cast<VRTDataset *>(poSrcBandDataset);
        if (poVRTSrcBandDataset && !poVRTSrcBandDataset->m_bCanTakeRef)
        {
            // Situation triggered by VRTDataset::AddVirtualOverview()
            // We create an overview dataset that is a VRT of a reduction of
            // ourselves. But we don't want to take a reference on ourselves,
            // otherwise this will prevent us to be closed in number of
            // circumstances
            poSimpleSource->m_bDropRefOnSrcBand = false;
        }
        else
        {
            poSrcBandDataset->Reference();
        }
    }
}

/************************************************************************/
/*                          AddSimpleSource()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddSimpleSource(
    const char *pszFilename, int nBandIn, double dfSrcXOff, double dfSrcYOff,
    double dfSrcXSize, double dfSrcYSize, double dfDstXOff, double dfDstYOff,
    double dfDstXSize, double dfDstYSize, const char *pszResampling,
    double dfNoDataValueIn)

{
    /* -------------------------------------------------------------------- */
    /*      Create source.                                                  */
    /* -------------------------------------------------------------------- */
    VRTSimpleSource *poSimpleSource = nullptr;

    if (pszResampling != nullptr && STARTS_WITH_CI(pszResampling, "aver"))
    {
        auto poAveragedSource = new VRTAveragedSource();
        poSimpleSource = poAveragedSource;
        if (dfNoDataValueIn != VRT_NODATA_UNSET)
            poAveragedSource->SetNoDataValue(dfNoDataValueIn);
    }
    else
    {
        poSimpleSource = new VRTSimpleSource();
        if (dfNoDataValueIn != VRT_NODATA_UNSET)
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "NODATA setting not currently supported for nearest  "
                "neighbour sampled simple sources on Virtual Datasources.");
    }

    poSimpleSource->SetSrcBand(pszFilename, nBandIn);

    poSimpleSource->SetSrcWindow(dfSrcXOff, dfSrcYOff, dfSrcXSize, dfSrcYSize);
    poSimpleSource->SetDstWindow(dfDstXOff, dfDstYOff, dfDstXSize, dfDstYSize);

    /* -------------------------------------------------------------------- */
    /*      add to list.                                                    */
    /* -------------------------------------------------------------------- */
    return AddSource(poSimpleSource);
}

/************************************************************************/
/*                          AddSimpleSource()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddSimpleSource(
    GDALRasterBand *poSrcBand, double dfSrcXOff, double dfSrcYOff,
    double dfSrcXSize, double dfSrcYSize, double dfDstXOff, double dfDstYOff,
    double dfDstXSize, double dfDstYSize, const char *pszResampling,
    double dfNoDataValueIn)

{
    /* -------------------------------------------------------------------- */
    /*      Create source.                                                  */
    /* -------------------------------------------------------------------- */
    VRTSimpleSource *poSimpleSource = nullptr;

    if (pszResampling != nullptr && STARTS_WITH_CI(pszResampling, "aver"))
    {
        auto poAveragedSource = new VRTAveragedSource();
        poSimpleSource = poAveragedSource;
        if (dfNoDataValueIn != VRT_NODATA_UNSET)
            poAveragedSource->SetNoDataValue(dfNoDataValueIn);
    }
    else
    {
        poSimpleSource = new VRTSimpleSource();
        if (dfNoDataValueIn != VRT_NODATA_UNSET)
            CPLError(
                CE_Warning, CPLE_AppDefined,
                "NODATA setting not currently supported for "
                "neighbour sampled simple sources on Virtual Datasources.");
    }

    ConfigureSource(poSimpleSource, poSrcBand, FALSE, dfSrcXOff, dfSrcYOff,
                    dfSrcXSize, dfSrcYSize, dfDstXOff, dfDstYOff, dfDstXSize,
                    dfDstYSize);

    /* -------------------------------------------------------------------- */
    /*      add to list.                                                    */
    /* -------------------------------------------------------------------- */
    return AddSource(poSimpleSource);
}

/************************************************************************/
/*                         AddMaskBandSource()                          */
/************************************************************************/

// poSrcBand is not the mask band, but the band from which the mask band is
// taken.
CPLErr VRTSourcedRasterBand::AddMaskBandSource(
    GDALRasterBand *poSrcBand, double dfSrcXOff, double dfSrcYOff,
    double dfSrcXSize, double dfSrcYSize, double dfDstXOff, double dfDstYOff,
    double dfDstXSize, double dfDstYSize)
{
    /* -------------------------------------------------------------------- */
    /*      Create source.                                                  */
    /* -------------------------------------------------------------------- */
    VRTSimpleSource *poSimpleSource = new VRTSimpleSource();

    ConfigureSource(poSimpleSource, poSrcBand, TRUE, dfSrcXOff, dfSrcYOff,
                    dfSrcXSize, dfSrcYSize, dfDstXOff, dfDstYOff, dfDstXSize,
                    dfDstYSize);

    /* -------------------------------------------------------------------- */
    /*      add to list.                                                    */
    /* -------------------------------------------------------------------- */
    return AddSource(poSimpleSource);
}

/*! @endcond */

/************************************************************************/
/*                         VRTAddSimpleSource()                         */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddSimpleSource().
 */

CPLErr CPL_STDCALL VRTAddSimpleSource(VRTSourcedRasterBandH hVRTBand,
                                      GDALRasterBandH hSrcBand, int nSrcXOff,
                                      int nSrcYOff, int nSrcXSize,
                                      int nSrcYSize, int nDstXOff, int nDstYOff,
                                      int nDstXSize, int nDstYSize,
                                      const char *pszResampling,
                                      double dfNoDataValue)
{
    VALIDATE_POINTER1(hVRTBand, "VRTAddSimpleSource", CE_Failure);

    return reinterpret_cast<VRTSourcedRasterBand *>(hVRTBand)->AddSimpleSource(
        reinterpret_cast<GDALRasterBand *>(hSrcBand), nSrcXOff, nSrcYOff,
        nSrcXSize, nSrcYSize, nDstXOff, nDstYOff, nDstXSize, nDstYSize,
        pszResampling, dfNoDataValue);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                          AddComplexSource()                          */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddComplexSource(
    const char *pszFilename, int nBandIn, double dfSrcXOff, double dfSrcYOff,
    double dfSrcXSize, double dfSrcYSize, double dfDstXOff, double dfDstYOff,
    double dfDstXSize, double dfDstYSize, double dfScaleOff,
    double dfScaleRatio, double dfNoDataValueIn, int nColorTableComponent)

{
    /* -------------------------------------------------------------------- */
    /*      Create source.                                                  */
    /* -------------------------------------------------------------------- */
    VRTComplexSource *const poSource = new VRTComplexSource();

    poSource->SetSrcBand(pszFilename, nBandIn);

    poSource->SetSrcWindow(dfSrcXOff, dfSrcYOff, dfSrcXSize, dfSrcYSize);
    poSource->SetDstWindow(dfDstXOff, dfDstYOff, dfDstXSize, dfDstYSize);

    /* -------------------------------------------------------------------- */
    /*      Set complex parameters.                                         */
    /* -------------------------------------------------------------------- */
    if (dfNoDataValueIn != VRT_NODATA_UNSET)
        poSource->SetNoDataValue(dfNoDataValueIn);

    if (dfScaleOff != 0.0 || dfScaleRatio != 1.0)
        poSource->SetLinearScaling(dfScaleOff, dfScaleRatio);

    poSource->SetColorTableComponent(nColorTableComponent);

    /* -------------------------------------------------------------------- */
    /*      add to list.                                                    */
    /* -------------------------------------------------------------------- */
    return AddSource(poSource);
}

/************************************************************************/
/*                          AddComplexSource()                          */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddComplexSource(
    GDALRasterBand *poSrcBand, double dfSrcXOff, double dfSrcYOff,
    double dfSrcXSize, double dfSrcYSize, double dfDstXOff, double dfDstYOff,
    double dfDstXSize, double dfDstYSize, double dfScaleOff,
    double dfScaleRatio, double dfNoDataValueIn, int nColorTableComponent)

{
    /* -------------------------------------------------------------------- */
    /*      Create source.                                                  */
    /* -------------------------------------------------------------------- */
    VRTComplexSource *const poSource = new VRTComplexSource();

    ConfigureSource(poSource, poSrcBand, FALSE, dfSrcXOff, dfSrcYOff,
                    dfSrcXSize, dfSrcYSize, dfDstXOff, dfDstYOff, dfDstXSize,
                    dfDstYSize);

    /* -------------------------------------------------------------------- */
    /*      Set complex parameters.                                         */
    /* -------------------------------------------------------------------- */
    if (dfNoDataValueIn != VRT_NODATA_UNSET)
        poSource->SetNoDataValue(dfNoDataValueIn);

    if (dfScaleOff != 0.0 || dfScaleRatio != 1.0)
        poSource->SetLinearScaling(dfScaleOff, dfScaleRatio);

    poSource->SetColorTableComponent(nColorTableComponent);

    /* -------------------------------------------------------------------- */
    /*      add to list.                                                    */
    /* -------------------------------------------------------------------- */
    return AddSource(poSource);
}

/*! @endcond */

/************************************************************************/
/*                         VRTAddComplexSource()                        */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddComplexSource().
 */

CPLErr CPL_STDCALL VRTAddComplexSource(
    VRTSourcedRasterBandH hVRTBand, GDALRasterBandH hSrcBand, int nSrcXOff,
    int nSrcYOff, int nSrcXSize, int nSrcYSize, int nDstXOff, int nDstYOff,
    int nDstXSize, int nDstYSize, double dfScaleOff, double dfScaleRatio,
    double dfNoDataValue)
{
    VALIDATE_POINTER1(hVRTBand, "VRTAddComplexSource", CE_Failure);

    return reinterpret_cast<VRTSourcedRasterBand *>(hVRTBand)->AddComplexSource(
        reinterpret_cast<GDALRasterBand *>(hSrcBand), nSrcXOff, nSrcYOff,
        nSrcXSize, nSrcYSize, nDstXOff, nDstYOff, nDstXSize, nDstYSize,
        dfScaleOff, dfScaleRatio, dfNoDataValue);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                           AddFuncSource()                            */
/************************************************************************/

CPLErr VRTSourcedRasterBand::AddFuncSource(VRTImageReadFunc pfnReadFunc,
                                           void *pCBData,
                                           double dfNoDataValueIn)

{
    /* -------------------------------------------------------------------- */
    /*      Create source.                                                  */
    /* -------------------------------------------------------------------- */
    VRTFuncSource *const poFuncSource = new VRTFuncSource;

    poFuncSource->fNoDataValue = static_cast<float>(dfNoDataValueIn);
    poFuncSource->pfnReadFunc = pfnReadFunc;
    poFuncSource->pCBData = pCBData;
    poFuncSource->eType = GetRasterDataType();

    /* -------------------------------------------------------------------- */
    /*      add to list.                                                    */
    /* -------------------------------------------------------------------- */
    return AddSource(poFuncSource);
}

/*! @endcond */

/************************************************************************/
/*                          VRTAddFuncSource()                          */
/************************************************************************/

/**
 * @see VRTSourcedRasterBand::AddFuncSource().
 */

CPLErr CPL_STDCALL VRTAddFuncSource(VRTSourcedRasterBandH hVRTBand,
                                    VRTImageReadFunc pfnReadFunc, void *pCBData,
                                    double dfNoDataValue)
{
    VALIDATE_POINTER1(hVRTBand, "VRTAddFuncSource", CE_Failure);

    return reinterpret_cast<VRTSourcedRasterBand *>(hVRTBand)->AddFuncSource(
        pfnReadFunc, pCBData, dfNoDataValue);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **VRTSourcedRasterBand::GetMetadataDomainList()
{
    return CSLAddString(GDALRasterBand::GetMetadataDomainList(),
                        "LocationInfo");
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *VRTSourcedRasterBand::GetMetadataItem(const char *pszName,
                                                  const char *pszDomain)

{
    /* ==================================================================== */
    /*      LocationInfo handling.                                          */
    /* ==================================================================== */
    if (pszDomain != nullptr && EQUAL(pszDomain, "LocationInfo") &&
        (STARTS_WITH_CI(pszName, "Pixel_") ||
         STARTS_WITH_CI(pszName, "GeoPixel_")))
    {
        /* --------------------------------------------------------------------
         */
        /*      What pixel are we aiming at? */
        /* --------------------------------------------------------------------
         */
        int iPixel = 0;
        int iLine = 0;

        if (STARTS_WITH_CI(pszName, "Pixel_"))
        {
            // TODO(schwehr): Replace sscanf.
            if (sscanf(pszName + 6, "%d_%d", &iPixel, &iLine) != 2)
                return nullptr;
        }
        else if (STARTS_WITH_CI(pszName, "GeoPixel_"))
        {
            const double dfGeoX = CPLAtof(pszName + 9);
            const char *const pszUnderscore = strchr(pszName + 9, '_');
            if (!pszUnderscore)
                return nullptr;
            const double dfGeoY = CPLAtof(pszUnderscore + 1);

            if (GetDataset() == nullptr)
                return nullptr;

            double adfGeoTransform[6] = {0.0};
            if (GetDataset()->GetGeoTransform(adfGeoTransform) != CE_None)
                return nullptr;

            double adfInvGeoTransform[6] = {0.0};
            if (!GDALInvGeoTransform(adfGeoTransform, adfInvGeoTransform))
                return nullptr;

            iPixel = static_cast<int>(floor(adfInvGeoTransform[0] +
                                            adfInvGeoTransform[1] * dfGeoX +
                                            adfInvGeoTransform[2] * dfGeoY));
            iLine = static_cast<int>(floor(adfInvGeoTransform[3] +
                                           adfInvGeoTransform[4] * dfGeoX +
                                           adfInvGeoTransform[5] * dfGeoY));
        }
        else
        {
            return nullptr;
        }

        if (iPixel < 0 || iLine < 0 || iPixel >= GetXSize() ||
            iLine >= GetYSize())
            return nullptr;

        /* --------------------------------------------------------------------
         */
        /*      Find the file(s) at this location. */
        /* --------------------------------------------------------------------
         */
        char **papszFileList = nullptr;
        int nListSize = 0;     // keep it in this scope
        int nListMaxSize = 0;  // keep it in this scope
        CPLHashSet *const hSetFiles =
            CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, nullptr);

        for (int iSource = 0; iSource < nSources; iSource++)
        {
            if (!papoSources[iSource]->IsSimpleSource())
                continue;

            VRTSimpleSource *const poSrc =
                static_cast<VRTSimpleSource *>(papoSources[iSource]);

            double dfReqXOff = 0.0;
            double dfReqYOff = 0.0;
            double dfReqXSize = 0.0;
            double dfReqYSize = 0.0;
            int nReqXOff = 0;
            int nReqYOff = 0;
            int nReqXSize = 0;
            int nReqYSize = 0;
            int nOutXOff = 0;
            int nOutYOff = 0;
            int nOutXSize = 0;
            int nOutYSize = 0;

            bool bError = false;
            if (!poSrc->GetSrcDstWindow(iPixel, iLine, 1, 1, 1, 1, &dfReqXOff,
                                        &dfReqYOff, &dfReqXSize, &dfReqYSize,
                                        &nReqXOff, &nReqYOff, &nReqXSize,
                                        &nReqYSize, &nOutXOff, &nOutYOff,
                                        &nOutXSize, &nOutYSize, bError))
            {
                if (bError)
                {
                    CSLDestroy(papszFileList);
                    CPLHashSetDestroy(hSetFiles);
                    return nullptr;
                }
                continue;
            }

            poSrc->GetFileList(&papszFileList, &nListSize, &nListMaxSize,
                               hSetFiles);
        }

        /* --------------------------------------------------------------------
         */
        /*      Format into XML. */
        /* --------------------------------------------------------------------
         */
        m_osLastLocationInfo = "<LocationInfo>";
        for (int i = 0; i < nListSize && papszFileList[i] != nullptr; i++)
        {
            m_osLastLocationInfo += "<File>";
            char *const pszXMLEscaped =
                CPLEscapeString(papszFileList[i], -1, CPLES_XML);
            m_osLastLocationInfo += pszXMLEscaped;
            CPLFree(pszXMLEscaped);
            m_osLastLocationInfo += "</File>";
        }
        m_osLastLocationInfo += "</LocationInfo>";

        CSLDestroy(papszFileList);
        CPLHashSetDestroy(hSetFiles);

        return m_osLastLocationInfo.c_str();
    }

    /* ==================================================================== */
    /*      Other domains.                                                  */
    /* ==================================================================== */

    return GDALRasterBand::GetMetadataItem(pszName, pszDomain);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **VRTSourcedRasterBand::GetMetadata(const char *pszDomain)

{
    /* ==================================================================== */
    /*      vrt_sources domain handling.                                    */
    /* ==================================================================== */
    if (pszDomain != nullptr && EQUAL(pszDomain, "vrt_sources"))
    {
        CSLDestroy(m_papszSourceList);
        m_papszSourceList = nullptr;

        /* --------------------------------------------------------------------
         */
        /*      Process SimpleSources. */
        /* --------------------------------------------------------------------
         */
        for (int iSource = 0; iSource < nSources; iSource++)
        {
            CPLXMLNode *const psXMLSrc =
                papoSources[iSource]->SerializeToXML(nullptr);
            if (psXMLSrc == nullptr)
                continue;

            char *const pszXML = CPLSerializeXMLTree(psXMLSrc);

            m_papszSourceList = CSLSetNameValue(
                m_papszSourceList, CPLSPrintf("source_%d", iSource), pszXML);
            CPLFree(pszXML);
            CPLDestroyXMLNode(psXMLSrc);
        }

        return m_papszSourceList;
    }

    /* ==================================================================== */
    /*      Other domains.                                                  */
    /* ==================================================================== */

    return GDALRasterBand::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::SetMetadataItem(const char *pszName,
                                             const char *pszValue,
                                             const char *pszDomain)

{
#if DEBUG_VERBOSE
    CPLDebug("VRT", "VRTSourcedRasterBand::SetMetadataItem(%s,%s,%s)\n",
             pszName, pszValue ? pszValue : "(null)",
             pszDomain ? pszDomain : "(null)");
#endif

    if (pszDomain != nullptr && EQUAL(pszDomain, "new_vrt_sources"))
    {
        VRTDriver *const poDriver =
            static_cast<VRTDriver *>(GDALGetDriverByName("VRT"));

        CPLXMLNode *const psTree = CPLParseXMLString(pszValue);
        if (psTree == nullptr)
            return CE_Failure;

        auto l_poDS = dynamic_cast<VRTDataset *>(GetDataset());
        if (l_poDS == nullptr)
        {
            CPLDestroyXMLNode(psTree);
            return CE_Failure;
        }
        VRTSource *const poSource =
            poDriver->ParseSource(psTree, nullptr, l_poDS->m_oMapSharedSources);
        CPLDestroyXMLNode(psTree);

        if (poSource != nullptr)
            return AddSource(poSource);

        return CE_Failure;
    }
    else if (pszDomain != nullptr && EQUAL(pszDomain, "vrt_sources"))
    {
        int iSource = 0;
        // TODO(schwehr): Replace sscanf.
        if (sscanf(pszName, "source_%d", &iSource) != 1 || iSource < 0 ||
            iSource >= nSources)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s metadata item name is not recognized. "
                     "Should be between source_0 and source_%d",
                     pszName, nSources - 1);
            return CE_Failure;
        }

        VRTDriver *const poDriver =
            static_cast<VRTDriver *>(GDALGetDriverByName("VRT"));

        CPLXMLNode *const psTree = CPLParseXMLString(pszValue);
        if (psTree == nullptr)
            return CE_Failure;

        auto l_poDS = dynamic_cast<VRTDataset *>(GetDataset());
        if (l_poDS == nullptr)
        {
            CPLDestroyXMLNode(psTree);
            return CE_Failure;
        }
        VRTSource *const poSource =
            poDriver->ParseSource(psTree, nullptr, l_poDS->m_oMapSharedSources);
        CPLDestroyXMLNode(psTree);

        if (poSource != nullptr)
        {
            delete papoSources[iSource];
            papoSources[iSource] = poSource;
            static_cast<VRTDataset *>(poDS)->SetNeedsFlush();
            return CE_None;
        }

        return CE_Failure;
    }

    return VRTRasterBand::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTSourcedRasterBand::SetMetadata(char **papszNewMD,
                                         const char *pszDomain)

{
    if (pszDomain != nullptr && (EQUAL(pszDomain, "new_vrt_sources") ||
                                 EQUAL(pszDomain, "vrt_sources")))
    {
        VRTDriver *const poDriver =
            static_cast<VRTDriver *>(GDALGetDriverByName("VRT"));

        if (EQUAL(pszDomain, "vrt_sources"))
        {
            for (int i = 0; i < nSources; i++)
                delete papoSources[i];
            CPLFree(papoSources);
            papoSources = nullptr;
            nSources = 0;
        }

        for (const char *const pszMDItem :
             cpl::Iterate(CSLConstList(papszNewMD)))
        {
            const char *const pszXML = CPLParseNameValue(pszMDItem, nullptr);
            CPLXMLTreeCloser psTree(CPLParseXMLString(pszXML));
            if (!psTree)
                return CE_Failure;

            auto l_poDS = dynamic_cast<VRTDataset *>(GetDataset());
            if (l_poDS == nullptr)
            {
                return CE_Failure;
            }
            VRTSource *const poSource = poDriver->ParseSource(
                psTree.get(), nullptr, l_poDS->m_oMapSharedSources);
            if (poSource == nullptr)
                return CE_Failure;

            const CPLErr eErr = AddSource(poSource);
            // cppcheck-suppress knownConditionTrueFalse
            if (eErr != CE_None)
                return eErr;
        }

        return CE_None;
    }

    return VRTRasterBand::SetMetadata(papszNewMD, pszDomain);
}

/************************************************************************/
/*                             GetFileList()                            */
/************************************************************************/

void VRTSourcedRasterBand::GetFileList(char ***ppapszFileList, int *pnSize,
                                       int *pnMaxSize, CPLHashSet *hSetFiles)
{
    for (int i = 0; i < nSources; i++)
    {
        papoSources[i]->GetFileList(ppapszFileList, pnSize, pnMaxSize,
                                    hSetFiles);
    }

    VRTRasterBand::GetFileList(ppapszFileList, pnSize, pnMaxSize, hSetFiles);
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTSourcedRasterBand::CloseDependentDatasets()
{
    int ret = VRTRasterBand::CloseDependentDatasets();

    if (nSources == 0)
        return ret;

    for (int i = 0; i < nSources; i++)
        delete papoSources[i];

    CPLFree(papoSources);
    papoSources = nullptr;
    nSources = 0;

    return TRUE;
}

/************************************************************************/
/*                               FlushCache()                           */
/************************************************************************/

CPLErr VRTSourcedRasterBand::FlushCache(bool bAtClosing)
{
    CPLErr eErr = VRTRasterBand::FlushCache(bAtClosing);
    for (int i = 0; i < nSources && eErr == CE_None; i++)
    {
        eErr = papoSources[i]->FlushCache(bAtClosing);
    }
    return eErr;
}

/************************************************************************/
/*                           RemoveCoveredSources()                     */
/************************************************************************/

/** Remove sources that are covered by other sources.
 *
 * This method removes sources that are covered entirely by (one or several)
 * sources of higher priority (even if they declare a nodata setting).
 * This optimizes the size of the VRT and the rendering time.
 */
void VRTSourcedRasterBand::RemoveCoveredSources(CSLConstList papszOptions)
{
#ifndef HAVE_GEOS
    if (CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "EMIT_ERROR_IF_GEOS_NOT_AVAILABLE", "TRUE")))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "RemoveCoveredSources() not implemented in builds "
                 "without GEOS support");
    }
#else
    (void)papszOptions;

    CPLRectObj globalBounds;
    globalBounds.minx = 0;
    globalBounds.miny = 0;
    globalBounds.maxx = nRasterXSize;
    globalBounds.maxy = nRasterYSize;

    // Create an index with the bbox of all sources
    CPLQuadTree *hTree = CPLQuadTreeCreate(&globalBounds, nullptr);
    for (int i = 0; i < nSources; i++)
    {
        if (papoSources[i]->IsSimpleSource())
        {
            VRTSimpleSource *poSS =
                cpl::down_cast<VRTSimpleSource *>(papoSources[i]);
            void *hFeature =
                reinterpret_cast<void *>(static_cast<uintptr_t>(i));
            CPLRectObj rect;
            rect.minx = std::max(0.0, poSS->m_dfDstXOff);
            rect.miny = std::max(0.0, poSS->m_dfDstYOff);
            rect.maxx = std::min(double(nRasterXSize),
                                 poSS->m_dfDstXOff + poSS->m_dfDstXSize);
            rect.maxy = std::min(double(nRasterYSize),
                                 poSS->m_dfDstYOff + poSS->m_dfDstYSize);
            CPLQuadTreeInsertWithBounds(hTree, hFeature, &rect);
        }
    }

    for (int i = 0; i < nSources; i++)
    {
        if (papoSources[i]->IsSimpleSource())
        {
            VRTSimpleSource *poSS =
                cpl::down_cast<VRTSimpleSource *>(papoSources[i]);
            CPLRectObj rect;
            rect.minx = std::max(0.0, poSS->m_dfDstXOff);
            rect.miny = std::max(0.0, poSS->m_dfDstYOff);
            rect.maxx = std::min(double(nRasterXSize),
                                 poSS->m_dfDstXOff + poSS->m_dfDstXSize);
            rect.maxy = std::min(double(nRasterYSize),
                                 poSS->m_dfDstYOff + poSS->m_dfDstYSize);

            // Find sources whose extent intersect with the current one
            int nFeatureCount = 0;
            void **pahFeatures =
                CPLQuadTreeSearch(hTree, &rect, &nFeatureCount);

            // Compute the bounding box of those sources, only if they are
            // on top of the current one
            CPLRectObj rectIntersecting;
            rectIntersecting.minx = std::numeric_limits<double>::max();
            rectIntersecting.miny = std::numeric_limits<double>::max();
            rectIntersecting.maxx = -std::numeric_limits<double>::max();
            rectIntersecting.maxy = -std::numeric_limits<double>::max();
            for (int j = 0; j < nFeatureCount; j++)
            {
                const int curFeature = static_cast<int>(
                    reinterpret_cast<uintptr_t>(pahFeatures[j]));
                if (curFeature > i)
                {
                    VRTSimpleSource *poOtherSS =
                        cpl::down_cast<VRTSimpleSource *>(
                            papoSources[curFeature]);
                    rectIntersecting.minx =
                        std::min(rectIntersecting.minx, poOtherSS->m_dfDstXOff);
                    rectIntersecting.miny =
                        std::min(rectIntersecting.miny, poOtherSS->m_dfDstYOff);
                    rectIntersecting.maxx = std::max(
                        rectIntersecting.maxx,
                        poOtherSS->m_dfDstXOff + poOtherSS->m_dfDstXSize);
                    rectIntersecting.maxy = std::max(
                        rectIntersecting.maxy,
                        poOtherSS->m_dfDstYOff + poOtherSS->m_dfDstXSize);
                }
            }

            // If the boundinx box of those sources overlap the current one,
            // then compute their union, and check if it contains the current
            // source
            if (rectIntersecting.minx <= rect.minx &&
                rectIntersecting.miny <= rect.miny &&
                rectIntersecting.maxx >= rect.maxx &&
                rectIntersecting.maxy >= rect.maxy)
            {
                OGRPolygon oPoly;
                {
                    auto poLR = new OGRLinearRing();
                    poLR->addPoint(rect.minx, rect.miny);
                    poLR->addPoint(rect.minx, rect.maxy);
                    poLR->addPoint(rect.maxx, rect.maxy);
                    poLR->addPoint(rect.maxx, rect.miny);
                    poLR->addPoint(rect.minx, rect.miny);
                    oPoly.addRingDirectly(poLR);
                }

                std::unique_ptr<OGRGeometry> poUnion;
                for (int j = 0; j < nFeatureCount; j++)
                {
                    const int curFeature = static_cast<int>(
                        reinterpret_cast<uintptr_t>(pahFeatures[j]));
                    if (curFeature > i)
                    {
                        VRTSimpleSource *poOtherSS =
                            cpl::down_cast<VRTSimpleSource *>(
                                papoSources[curFeature]);
                        CPLRectObj otherRect;
                        otherRect.minx = std::max(0.0, poOtherSS->m_dfDstXOff);
                        otherRect.miny = std::max(0.0, poOtherSS->m_dfDstYOff);
                        otherRect.maxx = std::min(double(nRasterXSize),
                                                  poOtherSS->m_dfDstXOff +
                                                      poOtherSS->m_dfDstXSize);
                        otherRect.maxy = std::min(double(nRasterYSize),
                                                  poOtherSS->m_dfDstYOff +
                                                      poOtherSS->m_dfDstYSize);
                        OGRPolygon oOtherPoly;
                        {
                            auto poLR = new OGRLinearRing();
                            poLR->addPoint(otherRect.minx, otherRect.miny);
                            poLR->addPoint(otherRect.minx, otherRect.maxy);
                            poLR->addPoint(otherRect.maxx, otherRect.maxy);
                            poLR->addPoint(otherRect.maxx, otherRect.miny);
                            poLR->addPoint(otherRect.minx, otherRect.miny);
                            oOtherPoly.addRingDirectly(poLR);
                        }
                        if (poUnion == nullptr)
                            poUnion.reset(oOtherPoly.clone());
                        else
                            poUnion.reset(oOtherPoly.Union(poUnion.get()));
                    }
                }

                if (poUnion != nullptr && poUnion->Contains(&oPoly))
                {
                    // We can remove the current source
                    delete papoSources[i];
                    papoSources[i] = nullptr;
                }
            }
            CPLFree(pahFeatures);

            void *hFeature =
                reinterpret_cast<void *>(static_cast<uintptr_t>(i));
            CPLQuadTreeRemove(hTree, hFeature, &rect);
        }
    }

    // Compact the papoSources array
    int iDst = 0;
    for (int iSrc = 0; iSrc < nSources; iSrc++)
    {
        if (papoSources[iSrc])
            papoSources[iDst++] = papoSources[iSrc];
    }
    nSources = iDst;

    CPLQuadTreeDestroy(hTree);
#endif
}

/*! @endcond */
