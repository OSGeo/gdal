/******************************************************************************
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implementation of the GDALWarpKernel class.  Implements the actual
 *           image warping for a "chunk" of input and output imagery already
 *           loaded into memory.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdalwarper.h"

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <limits>
#include <mutex>
#include <new>
#include <utility>
#include <vector>

#include "cpl_atomic_ops.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_mask.h"
#include "cpl_multiproc.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "cpl_worker_thread_pool.h"
#include "cpl_quad_tree.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_thread_pool.h"
#include "gdalresamplingkernels.h"
#include "gdalwarpkernel_opencl.h"

// #define CHECK_SUM_WITH_GEOS
#ifdef CHECK_SUM_WITH_GEOS
#include "ogr_geometry.h"
#include "ogr_geos.h"
#endif

// We restrict to 64bit processors because they are guaranteed to have SSE2.
// Could possibly be used too on 32bit, but we would need to check at runtime.
#if defined(__x86_64) || defined(_M_X64)
#include "gdalsse_priv.h"

#if __SSE4_1__
#include <smmintrin.h>
#endif

#if __SSE3__
#include <pmmintrin.h>
#endif

#endif

constexpr double BAND_DENSITY_THRESHOLD = 0.0000000001;
constexpr float SRC_DENSITY_THRESHOLD = 0.000000001f;

// #define INSTANTIATE_FLOAT64_SSE2_IMPL

static const int anGWKFilterRadius[] = {
    0,  // Nearest neighbour
    1,  // Bilinear
    2,  // Cubic Convolution (Catmull-Rom)
    2,  // Cubic B-Spline
    3,  // Lanczos windowed sinc
    0,  // Average
    0,  // Mode
    0,  // Reserved GRA_Gauss=7
    0,  // Max
    0,  // Min
    0,  // Med
    0,  // Q1
    0,  // Q3
    0,  // Sum
    0,  // RMS
};

static double GWKBilinear(double dfX);
static double GWKCubic(double dfX);
static double GWKBSpline(double dfX);
static double GWKLanczosSinc(double dfX);

static const FilterFuncType apfGWKFilter[] = {
    nullptr,         // Nearest neighbour
    GWKBilinear,     // Bilinear
    GWKCubic,        // Cubic Convolution (Catmull-Rom)
    GWKBSpline,      // Cubic B-Spline
    GWKLanczosSinc,  // Lanczos windowed sinc
    nullptr,         // Average
    nullptr,         // Mode
    nullptr,         // Reserved GRA_Gauss=7
    nullptr,         // Max
    nullptr,         // Min
    nullptr,         // Med
    nullptr,         // Q1
    nullptr,         // Q3
    nullptr,         // Sum
    nullptr,         // RMS
};

// TODO(schwehr): Can we make these functions have a const * const arg?
static double GWKBilinear4Values(double *padfVals);
static double GWKCubic4Values(double *padfVals);
static double GWKBSpline4Values(double *padfVals);
static double GWKLanczosSinc4Values(double *padfVals);

static const FilterFunc4ValuesType apfGWKFilter4Values[] = {
    nullptr,                // Nearest neighbour
    GWKBilinear4Values,     // Bilinear
    GWKCubic4Values,        // Cubic Convolution (Catmull-Rom)
    GWKBSpline4Values,      // Cubic B-Spline
    GWKLanczosSinc4Values,  // Lanczos windowed sinc
    nullptr,                // Average
    nullptr,                // Mode
    nullptr,                // Reserved GRA_Gauss=7
    nullptr,                // Max
    nullptr,                // Min
    nullptr,                // Med
    nullptr,                // Q1
    nullptr,                // Q3
    nullptr,                // Sum
    nullptr,                // RMS
};

int GWKGetFilterRadius(GDALResampleAlg eResampleAlg)
{
    static_assert(CPL_ARRAYSIZE(anGWKFilterRadius) == GRA_LAST_VALUE + 1,
                  "Bad size of anGWKFilterRadius");
    return anGWKFilterRadius[eResampleAlg];
}

FilterFuncType GWKGetFilterFunc(GDALResampleAlg eResampleAlg)
{
    static_assert(CPL_ARRAYSIZE(apfGWKFilter) == GRA_LAST_VALUE + 1,
                  "Bad size of apfGWKFilter");
    return apfGWKFilter[eResampleAlg];
}

FilterFunc4ValuesType GWKGetFilterFunc4Values(GDALResampleAlg eResampleAlg)
{
    static_assert(CPL_ARRAYSIZE(apfGWKFilter4Values) == GRA_LAST_VALUE + 1,
                  "Bad size of apfGWKFilter4Values");
    return apfGWKFilter4Values[eResampleAlg];
}

#ifdef HAVE_OPENCL
static CPLErr GWKOpenCLCase(GDALWarpKernel *);
#endif

static CPLErr GWKGeneralCase(GDALWarpKernel *);
static CPLErr GWKRealCase(GDALWarpKernel *poWK);
static CPLErr GWKNearestNoMasksOrDstDensityOnlyByte(GDALWarpKernel *poWK);
static CPLErr GWKBilinearNoMasksOrDstDensityOnlyByte(GDALWarpKernel *poWK);
static CPLErr GWKCubicNoMasksOrDstDensityOnlyByte(GDALWarpKernel *poWK);
static CPLErr GWKCubicNoMasksOrDstDensityOnlyFloat(GDALWarpKernel *poWK);
#ifdef INSTANTIATE_FLOAT64_SSE2_IMPL
static CPLErr GWKCubicNoMasksOrDstDensityOnlyDouble(GDALWarpKernel *poWK);
#endif
static CPLErr GWKCubicSplineNoMasksOrDstDensityOnlyByte(GDALWarpKernel *poWK);
static CPLErr GWKNearestByte(GDALWarpKernel *poWK);
static CPLErr GWKNearestNoMasksOrDstDensityOnlyShort(GDALWarpKernel *poWK);
static CPLErr GWKBilinearNoMasksOrDstDensityOnlyShort(GDALWarpKernel *poWK);
static CPLErr GWKBilinearNoMasksOrDstDensityOnlyFloat(GDALWarpKernel *poWK);
#ifdef INSTANTIATE_FLOAT64_SSE2_IMPL
static CPLErr GWKBilinearNoMasksOrDstDensityOnlyDouble(GDALWarpKernel *poWK);
#endif
static CPLErr GWKCubicNoMasksOrDstDensityOnlyShort(GDALWarpKernel *poWK);
static CPLErr GWKCubicSplineNoMasksOrDstDensityOnlyShort(GDALWarpKernel *poWK);
static CPLErr GWKNearestShort(GDALWarpKernel *poWK);
static CPLErr GWKNearestNoMasksOrDstDensityOnlyFloat(GDALWarpKernel *poWK);
static CPLErr GWKNearestFloat(GDALWarpKernel *poWK);
static CPLErr GWKAverageOrMode(GDALWarpKernel *);
static CPLErr GWKSumPreserving(GDALWarpKernel *);
static CPLErr GWKCubicNoMasksOrDstDensityOnlyUShort(GDALWarpKernel *);
static CPLErr GWKCubicSplineNoMasksOrDstDensityOnlyUShort(GDALWarpKernel *);
static CPLErr GWKBilinearNoMasksOrDstDensityOnlyUShort(GDALWarpKernel *);

/************************************************************************/
/*                           GWKJobStruct                               */
/************************************************************************/

struct GWKJobStruct
{
    std::mutex &mutex;
    std::condition_variable &cv;
    int &counter;
    bool &stopFlag;
    GDALWarpKernel *poWK;
    int iYMin;
    int iYMax;
    int (*pfnProgress)(GWKJobStruct *psJob);
    void *pTransformerArg;
    void (*pfnFunc)(
        void *);  // used by GWKRun() to assign the proper pTransformerArg

    GWKJobStruct(std::mutex &mutex_, std::condition_variable &cv_,
                 int &counter_, bool &stopFlag_)
        : mutex(mutex_), cv(cv_), counter(counter_), stopFlag(stopFlag_),
          poWK(nullptr), iYMin(0), iYMax(0), pfnProgress(nullptr),
          pTransformerArg(nullptr), pfnFunc(nullptr)
    {
    }
};

struct GWKThreadData
{
    std::unique_ptr<CPLJobQueue> poJobQueue{};
    std::unique_ptr<std::vector<GWKJobStruct>> threadJobs{};
    int nMaxThreads{0};
    int counter{0};
    bool stopFlag{false};
    std::mutex mutex{};
    std::condition_variable cv{};
    bool bTransformerArgInputAssignedToThread{false};
    void *pTransformerArgInput{
        nullptr};  // owned by calling layer. Not to be destroyed
    std::map<GIntBig, void *> mapThreadToTransformerArg{};
    int nTotalThreadCountForThisRun = 0;
    int nCurThreadCountForThisRun = 0;
};

/************************************************************************/
/*                        GWKProgressThread()                           */
/************************************************************************/

// Return TRUE if the computation must be interrupted.
static int GWKProgressThread(GWKJobStruct *psJob)
{
    bool stop = false;
    {
        std::lock_guard<std::mutex> lock(psJob->mutex);
        psJob->counter++;
        stop = psJob->stopFlag;
    }
    psJob->cv.notify_one();

    return stop;
}

/************************************************************************/
/*                      GWKProgressMonoThread()                         */
/************************************************************************/

// Return TRUE if the computation must be interrupted.
static int GWKProgressMonoThread(GWKJobStruct *psJob)
{
    GDALWarpKernel *poWK = psJob->poWK;
    // coverity[missing_lock]
    if (!poWK->pfnProgress(
            poWK->dfProgressBase +
                poWK->dfProgressScale *
                    (++psJob->counter / static_cast<double>(psJob->iYMax)),
            "", poWK->pProgress))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        psJob->stopFlag = true;
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                       GWKGenericMonoThread()                         */
/************************************************************************/

static CPLErr GWKGenericMonoThread(GDALWarpKernel *poWK,
                                   void (*pfnFunc)(void *pUserData))
{
    GWKThreadData td;

    // NOTE: the mutex is not used.
    GWKJobStruct job(td.mutex, td.cv, td.counter, td.stopFlag);
    job.poWK = poWK;
    job.iYMin = 0;
    job.iYMax = poWK->nDstYSize;
    job.pfnProgress = GWKProgressMonoThread;
    job.pTransformerArg = poWK->pTransformerArg;
    pfnFunc(&job);

    return td.stopFlag ? CE_Failure : CE_None;
}

/************************************************************************/
/*                          GWKThreadsCreate()                          */
/************************************************************************/

void *GWKThreadsCreate(char **papszWarpOptions,
                       GDALTransformerFunc /* pfnTransformer */,
                       void *pTransformerArg)
{
    const char *pszWarpThreads =
        CSLFetchNameValue(papszWarpOptions, "NUM_THREADS");
    if (pszWarpThreads == nullptr)
        pszWarpThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "1");

    int nThreads = 0;
    if (EQUAL(pszWarpThreads, "ALL_CPUS"))
        nThreads = CPLGetNumCPUs();
    else
        nThreads = atoi(pszWarpThreads);
    if (nThreads <= 1)
        nThreads = 0;
    if (nThreads > 128)
        nThreads = 128;

    GWKThreadData *psThreadData = new GWKThreadData();
    auto poThreadPool =
        nThreads > 0 ? GDALGetGlobalThreadPool(nThreads) : nullptr;
    if (nThreads && poThreadPool)
    {
        psThreadData->nMaxThreads = nThreads;
        psThreadData->threadJobs.reset(new std::vector<GWKJobStruct>(
            nThreads,
            GWKJobStruct(psThreadData->mutex, psThreadData->cv,
                         psThreadData->counter, psThreadData->stopFlag)));

        psThreadData->poJobQueue = poThreadPool->CreateJobQueue();
        psThreadData->pTransformerArgInput = pTransformerArg;
    }

    return psThreadData;
}

/************************************************************************/
/*                             GWKThreadsEnd()                          */
/************************************************************************/

void GWKThreadsEnd(void *psThreadDataIn)
{
    if (psThreadDataIn == nullptr)
        return;

    GWKThreadData *psThreadData = static_cast<GWKThreadData *>(psThreadDataIn);
    if (psThreadData->poJobQueue)
    {
        // cppcheck-suppress constVariableReference
        for (auto &pair : psThreadData->mapThreadToTransformerArg)
        {
            CPLAssert(pair.second != psThreadData->pTransformerArgInput);
            GDALDestroyTransformer(pair.second);
        }
        psThreadData->poJobQueue.reset();
    }
    delete psThreadData;
}

/************************************************************************/
/*                         ThreadFuncAdapter()                          */
/************************************************************************/

static void ThreadFuncAdapter(void *pData)
{
    GWKJobStruct *psJob = static_cast<GWKJobStruct *>(pData);
    GWKThreadData *psThreadData =
        static_cast<GWKThreadData *>(psJob->poWK->psThreadData);

    // Look if we have already a per-thread transformer
    void *pTransformerArg = nullptr;
    const GIntBig nThreadId = CPLGetPID();

    {
        std::lock_guard<std::mutex> lock(psThreadData->mutex);
        ++psThreadData->nCurThreadCountForThisRun;

        auto oIter = psThreadData->mapThreadToTransformerArg.find(nThreadId);
        if (oIter != psThreadData->mapThreadToTransformerArg.end())
        {
            pTransformerArg = oIter->second;
        }
        else if (!psThreadData->bTransformerArgInputAssignedToThread &&
                 psThreadData->nCurThreadCountForThisRun ==
                     psThreadData->nTotalThreadCountForThisRun)
        {
            // If we are the last thread to be started, temporarily borrow the
            // original transformer
            psThreadData->bTransformerArgInputAssignedToThread = true;
            pTransformerArg = psThreadData->pTransformerArgInput;
            psThreadData->mapThreadToTransformerArg[nThreadId] =
                pTransformerArg;
        }

        if (pTransformerArg == nullptr)
        {
            CPLAssert(psThreadData->pTransformerArgInput != nullptr);
            CPLAssert(!psThreadData->bTransformerArgInputAssignedToThread);
        }
    }

    // If no transformer assigned to current thread, instantiate one
    if (pTransformerArg == nullptr)
    {
        // This somehow assumes that GDALCloneTransformer() is thread-safe
        // which should normally be the case.
        pTransformerArg =
            GDALCloneTransformer(psThreadData->pTransformerArgInput);

        // Lock for the stop flag and the transformer map.
        std::lock_guard<std::mutex> lock(psThreadData->mutex);
        if (!pTransformerArg)
        {
            psJob->stopFlag = true;
            return;
        }
        psThreadData->mapThreadToTransformerArg[nThreadId] = pTransformerArg;
    }

    psJob->pTransformerArg = pTransformerArg;
    psJob->pfnFunc(pData);

    // Give back original transformer, if borrowed.
    {
        std::lock_guard<std::mutex> lock(psThreadData->mutex);
        if (psThreadData->bTransformerArgInputAssignedToThread &&
            pTransformerArg == psThreadData->pTransformerArgInput)
        {
            psThreadData->mapThreadToTransformerArg.erase(
                psThreadData->mapThreadToTransformerArg.find(nThreadId));
            psThreadData->bTransformerArgInputAssignedToThread = false;
        }
    }
}

/************************************************************************/
/*                                GWKRun()                              */
/************************************************************************/

static CPLErr GWKRun(GDALWarpKernel *poWK, const char *pszFuncName,
                     void (*pfnFunc)(void *pUserData))

{
    const int nDstYSize = poWK->nDstYSize;

    CPLDebug("GDAL",
             "GDALWarpKernel()::%s() "
             "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
             pszFuncName, poWK->nSrcXOff, poWK->nSrcYOff, poWK->nSrcXSize,
             poWK->nSrcYSize, poWK->nDstXOff, poWK->nDstYOff, poWK->nDstXSize,
             poWK->nDstYSize);

    if (!poWK->pfnProgress(poWK->dfProgressBase, "", poWK->pProgress))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return CE_Failure;
    }

    GWKThreadData *psThreadData =
        static_cast<GWKThreadData *>(poWK->psThreadData);
    if (psThreadData == nullptr || psThreadData->poJobQueue == nullptr)
    {
        return GWKGenericMonoThread(poWK, pfnFunc);
    }

    int nThreads = std::min(psThreadData->nMaxThreads, nDstYSize / 2);
    // Config option mostly useful for tests to be able to test multithreading
    // with small rasters
    const int nWarpChunkSize =
        atoi(CPLGetConfigOption("WARP_THREAD_CHUNK_SIZE", "65536"));
    if (nWarpChunkSize > 0)
    {
        GIntBig nChunks =
            static_cast<GIntBig>(nDstYSize) * poWK->nDstXSize / nWarpChunkSize;
        if (nThreads > nChunks)
            nThreads = static_cast<int>(nChunks);
    }
    if (nThreads <= 0)
        nThreads = 1;

    CPLDebug("WARP", "Using %d threads", nThreads);

    auto &jobs = *psThreadData->threadJobs;
    CPLAssert(static_cast<int>(jobs.size()) >= nThreads);
    // Fill-in job structures.
    for (int i = 0; i < nThreads; ++i)
    {
        auto &job = jobs[i];
        job.poWK = poWK;
        job.iYMin =
            static_cast<int>(static_cast<int64_t>(i) * nDstYSize / nThreads);
        job.iYMax = static_cast<int>(static_cast<int64_t>(i + 1) * nDstYSize /
                                     nThreads);
        if (poWK->pfnProgress != GDALDummyProgress)
            job.pfnProgress = GWKProgressThread;
        job.pfnFunc = pfnFunc;
    }

    bool bStopFlag;
    {
        std::unique_lock<std::mutex> lock(psThreadData->mutex);

        psThreadData->nTotalThreadCountForThisRun = nThreads;
        // coverity[missing_lock]
        psThreadData->nCurThreadCountForThisRun = 0;

        // Start jobs.
        for (int i = 0; i < nThreads; ++i)
        {
            auto &job = jobs[i];
            psThreadData->poJobQueue->SubmitJob(ThreadFuncAdapter,
                                                static_cast<void *>(&job));
        }

        /* --------------------------------------------------------------------
         */
        /*      Report progress. */
        /* --------------------------------------------------------------------
         */
        if (poWK->pfnProgress != GDALDummyProgress)
        {
            while (psThreadData->counter < nDstYSize)
            {
                psThreadData->cv.wait(lock);
                if (!poWK->pfnProgress(poWK->dfProgressBase +
                                           poWK->dfProgressScale *
                                               (psThreadData->counter /
                                                static_cast<double>(nDstYSize)),
                                       "", poWK->pProgress))
                {
                    CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
                    psThreadData->stopFlag = true;
                    break;
                }
            }
        }

        bStopFlag = psThreadData->stopFlag;
    }

    /* -------------------------------------------------------------------- */
    /*      Wait for all jobs to complete.                                  */
    /* -------------------------------------------------------------------- */
    psThreadData->poJobQueue->WaitCompletion();

    return bStopFlag ? CE_Failure : CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                            GDALWarpKernel                            */
/* ==================================================================== */
/************************************************************************/

/**
 * \class GDALWarpKernel "gdalwarper.h"
 *
 * Low level image warping class.
 *
 * This class is responsible for low level image warping for one
 * "chunk" of imagery.  The class is essentially a structure with all
 * data members public - primarily so that new special-case functions
 * can be added without changing the class declaration.
 *
 * Applications are normally intended to interactive with warping facilities
 * through the GDALWarpOperation class, though the GDALWarpKernel can in
 * theory be used directly if great care is taken in setting up the
 * control data.
 *
 * <h3>Design Issues</h3>
 *
 * The intention is that PerformWarp() would analyze the setup in terms
 * of the datatype, resampling type, and validity/density mask usage and
 * pick one of many specific implementations of the warping algorithm over
 * a continuum of optimization vs. generality.  At one end there will be a
 * reference general purpose implementation of the algorithm that supports
 * any data type (working internally in double precision complex), all three
 * resampling types, and any or all of the validity/density masks.  At the
 * other end would be highly optimized algorithms for common cases like
 * nearest neighbour resampling on GDT_Byte data with no masks.
 *
 * The full set of optimized versions have not been decided but we should
 * expect to have at least:
 *  - One for each resampling algorithm for 8bit data with no masks.
 *  - One for each resampling algorithm for float data with no masks.
 *  - One for each resampling algorithm for float data with any/all masks
 *    (essentially the generic case for just float data).
 *  - One for each resampling algorithm for 8bit data with support for
 *    input validity masks (per band or per pixel).  This handles the common
 *    case of nodata masking.
 *  - One for each resampling algorithm for float data with support for
 *    input validity masks (per band or per pixel).  This handles the common
 *    case of nodata masking.
 *
 * Some of the specializations would operate on all bands in one pass
 * (especially the ones without masking would do this), while others might
 * process each band individually to reduce code complexity.
 *
 * <h3>Masking Semantics</h3>
 *
 * A detailed explanation of the semantics of the validity and density masks,
 * and their effects on resampling kernels is needed here.
 */

/************************************************************************/
/*                     GDALWarpKernel Data Members                      */
/************************************************************************/

/**
 * \var GDALResampleAlg GDALWarpKernel::eResample;
 *
 * Resampling algorithm.
 *
 * The resampling algorithm to use.  One of GRA_NearestNeighbour, GRA_Bilinear,
 * GRA_Cubic, GRA_CubicSpline, GRA_Lanczos, GRA_Average, GRA_RMS,
 * GRA_Mode or GRA_Sum.
 *
 * This field is required. GDT_NearestNeighbour may be used as a default
 * value.
 */

/**
 * \var GDALDataType GDALWarpKernel::eWorkingDataType;
 *
 * Working pixel data type.
 *
 * The datatype of pixels in the source image (papabySrcimage) and
 * destination image (papabyDstImage) buffers.  Note that operations on
 * some data types (such as GDT_Byte) may be much better optimized than other
 * less common cases.
 *
 * This field is required.  It may not be GDT_Unknown.
 */

/**
 * \var int GDALWarpKernel::nBands;
 *
 * Number of bands.
 *
 * The number of bands (layers) of imagery being warped.  Determines the
 * number of entries in the papabySrcImage, papanBandSrcValid,
 * and papabyDstImage arrays.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nSrcXSize;
 *
 * Source image width in pixels.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nSrcYSize;
 *
 * Source image height in pixels.
 *
 * This field is required.
 */

/**
 * \var double GDALWarpKernel::dfSrcXExtraSize;
 *
 * Number of pixels included in nSrcXSize that are present on the edges of
 * the area of interest to take into account the width of the kernel.
 *
 * This field is required.
 */

/**
 * \var double GDALWarpKernel::dfSrcYExtraSize;
 *
 * Number of pixels included in nSrcYExtraSize that are present on the edges of
 * the area of interest to take into account the height of the kernel.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::papabySrcImage;
 *
 * Array of source image band data.
 *
 * This is an array of pointers (of size GDALWarpKernel::nBands) pointers
 * to image data.  Each individual band of image data is organized as a single
 * block of image data in left to right, then bottom to top order.  The actual
 * type of the image data is determined by GDALWarpKernel::eWorkingDataType.
 *
 * To access the pixel value for the (x=3, y=4) pixel (zero based) of
 * the second band with eWorkingDataType set to GDT_Float32 use code like
 * this:
 *
 * \code
 *   float dfPixelValue;
 *   int   nBand = 2-1;  // Band indexes are zero based.
 *   int   nPixel = 3; // Zero based.
 *   int   nLine = 4;  // Zero based.
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nSrcXSize );
 *   assert( nLine >= 0 && nLine < poKern->nSrcYSize );
 *   assert( nBand >= 0 && nBand < poKern->nBands );
 *   dfPixelValue = ((float *) poKern->papabySrcImage[nBand])
 *                                  [nPixel + nLine * poKern->nSrcXSize];
 * \endcode
 *
 * This field is required.
 */

/**
 * \var GUInt32 **GDALWarpKernel::papanBandSrcValid;
 *
 * Per band validity mask for source pixels.
 *
 * Array of pixel validity mask layers for each source band.   Each of
 * the mask layers is the same size (in pixels) as the source image with
 * one bit per pixel.  Note that it is legal (and common) for this to be
 * NULL indicating that none of the pixels are invalidated, or for some
 * band validity masks to be NULL in which case all pixels of the band are
 * valid.  The following code can be used to test the validity of a particular
 * pixel.
 *
 * \code
 *   int   bIsValid = TRUE;
 *   int   nBand = 2-1;  // Band indexes are zero based.
 *   int   nPixel = 3; // Zero based.
 *   int   nLine = 4;  // Zero based.
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nSrcXSize );
 *   assert( nLine >= 0 && nLine < poKern->nSrcYSize );
 *   assert( nBand >= 0 && nBand < poKern->nBands );
 *
 *   if( poKern->papanBandSrcValid != NULL
 *       && poKern->papanBandSrcValid[nBand] != NULL )
 *   {
 *       GUInt32 *panBandMask = poKern->papanBandSrcValid[nBand];
 *       int    iPixelOffset = nPixel + nLine * poKern->nSrcXSize;
 *
 *       bIsValid = CPLMaskGet(panBandMask, iPixelOffset)
 *   }
 * \endcode
 */

/**
 * \var GUInt32 *GDALWarpKernel::panUnifiedSrcValid;
 *
 * Per pixel validity mask for source pixels.
 *
 * A single validity mask layer that applies to the pixels of all source
 * bands.  It is accessed similarly to papanBandSrcValid, but without the
 * extra level of band indirection.
 *
 * This pointer may be NULL indicating that all pixels are valid.
 *
 * Note that if both panUnifiedSrcValid, and papanBandSrcValid are available,
 * the pixel isn't considered to be valid unless both arrays indicate it is
 * valid.
 */

/**
 * \var float *GDALWarpKernel::pafUnifiedSrcDensity;
 *
 * Per pixel density mask for source pixels.
 *
 * A single density mask layer that applies to the pixels of all source
 * bands.  It contains values between 0.0 and 1.0 indicating the degree to
 * which this pixel should be allowed to contribute to the output result.
 *
 * This pointer may be NULL indicating that all pixels have a density of 1.0.
 *
 * The density for a pixel may be accessed like this:
 *
 * \code
 *   float fDensity = 1.0;
 *   int nPixel = 3;  // Zero based.
 *   int nLine = 4;   // Zero based.
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nSrcXSize );
 *   assert( nLine >= 0 && nLine < poKern->nSrcYSize );
 *   if( poKern->pafUnifiedSrcDensity != NULL )
 *     fDensity = poKern->pafUnifiedSrcDensity
 *                                  [nPixel + nLine * poKern->nSrcXSize];
 * \endcode
 */

/**
 * \var int GDALWarpKernel::nDstXSize;
 *
 * Width of destination image in pixels.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nDstYSize;
 *
 * Height of destination image in pixels.
 *
 * This field is required.
 */

/**
 * \var GByte **GDALWarpKernel::papabyDstImage;
 *
 * Array of destination image band data.
 *
 * This is an array of pointers (of size GDALWarpKernel::nBands) pointers
 * to image data.  Each individual band of image data is organized as a single
 * block of image data in left to right, then bottom to top order.  The actual
 * type of the image data is determined by GDALWarpKernel::eWorkingDataType.
 *
 * To access the pixel value for the (x=3, y=4) pixel (zero based) of
 * the second band with eWorkingDataType set to GDT_Float32 use code like
 * this:
 *
 * \code
 *   float dfPixelValue;
 *   int   nBand = 2-1;  // Band indexes are zero based.
 *   int   nPixel = 3; // Zero based.
 *   int   nLine = 4;  // Zero based.
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nDstXSize );
 *   assert( nLine >= 0 && nLine < poKern->nDstYSize );
 *   assert( nBand >= 0 && nBand < poKern->nBands );
 *   dfPixelValue = ((float *) poKern->papabyDstImage[nBand])
 *                                  [nPixel + nLine * poKern->nSrcYSize];
 * \endcode
 *
 * This field is required.
 */

/**
 * \var GUInt32 *GDALWarpKernel::panDstValid;
 *
 * Per pixel validity mask for destination pixels.
 *
 * A single validity mask layer that applies to the pixels of all destination
 * bands.  It is accessed similarly to papanUnitifiedSrcValid, but based
 * on the size of the destination image.
 *
 * This pointer may be NULL indicating that all pixels are valid.
 */

/**
 * \var float *GDALWarpKernel::pafDstDensity;
 *
 * Per pixel density mask for destination pixels.
 *
 * A single density mask layer that applies to the pixels of all destination
 * bands.  It contains values between 0.0 and 1.0.
 *
 * This pointer may be NULL indicating that all pixels have a density of 1.0.
 *
 * The density for a pixel may be accessed like this:
 *
 * \code
 *   float fDensity = 1.0;
 *   int   nPixel = 3; // Zero based.
 *   int   nLine = 4;  // Zero based.
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nDstXSize );
 *   assert( nLine >= 0 && nLine < poKern->nDstYSize );
 *   if( poKern->pafDstDensity != NULL )
 *     fDensity = poKern->pafDstDensity[nPixel + nLine * poKern->nDstXSize];
 * \endcode
 */

/**
 * \var int GDALWarpKernel::nSrcXOff;
 *
 * X offset to source pixel coordinates for transformation.
 *
 * See pfnTransformer.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nSrcYOff;
 *
 * Y offset to source pixel coordinates for transformation.
 *
 * See pfnTransformer.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nDstXOff;
 *
 * X offset to destination pixel coordinates for transformation.
 *
 * See pfnTransformer.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nDstYOff;
 *
 * Y offset to destination pixel coordinates for transformation.
 *
 * See pfnTransformer.
 *
 * This field is required.
 */

/**
 * \var GDALTransformerFunc GDALWarpKernel::pfnTransformer;
 *
 * Source/destination location transformer.
 *
 * The function to call to transform coordinates between source image
 * pixel/line coordinates and destination image pixel/line coordinates.
 * See GDALTransformerFunc() for details of the semantics of this function.
 *
 * The GDALWarpKern algorithm will only ever use this transformer in
 * "destination to source" mode (bDstToSrc=TRUE), and will always pass
 * partial or complete scanlines of points in the destination image as
 * input.  This means, among other things, that it is safe to the
 * approximating transform GDALApproxTransform() as the transformation
 * function.
 *
 * Source and destination images may be subsets of a larger overall image.
 * The transformation algorithms will expect and return pixel/line coordinates
 * in terms of this larger image, so coordinates need to be offset by
 * the offsets specified in nSrcXOff, nSrcYOff, nDstXOff, and nDstYOff before
 * passing to pfnTransformer, and after return from it.
 *
 * The GDALWarpKernel::pfnTransformerArg value will be passed as the callback
 * data to this function when it is called.
 *
 * This field is required.
 */

/**
 * \var void *GDALWarpKernel::pTransformerArg;
 *
 * Callback data for pfnTransformer.
 *
 * This field may be NULL if not required for the pfnTransformer being used.
 */

/**
 * \var GDALProgressFunc GDALWarpKernel::pfnProgress;
 *
 * The function to call to report progress of the algorithm, and to check
 * for a requested termination of the operation.  It operates according to
 * GDALProgressFunc() semantics.
 *
 * Generally speaking the progress function will be invoked for each
 * scanline of the destination buffer that has been processed.
 *
 * This field may be NULL (internally set to GDALDummyProgress()).
 */

/**
 * \var void *GDALWarpKernel::pProgress;
 *
 * Callback data for pfnProgress.
 *
 * This field may be NULL if not required for the pfnProgress being used.
 */

/************************************************************************/
/*                           GDALWarpKernel()                           */
/************************************************************************/

GDALWarpKernel::GDALWarpKernel()
    : papszWarpOptions(nullptr), eResample(GRA_NearestNeighbour),
      eWorkingDataType(GDT_Unknown), nBands(0), nSrcXSize(0), nSrcYSize(0),
      dfSrcXExtraSize(0.0), dfSrcYExtraSize(0.0), papabySrcImage(nullptr),
      papanBandSrcValid(nullptr), panUnifiedSrcValid(nullptr),
      pafUnifiedSrcDensity(nullptr), nDstXSize(0), nDstYSize(0),
      papabyDstImage(nullptr), panDstValid(nullptr), pafDstDensity(nullptr),
      dfXScale(1.0), dfYScale(1.0), dfXFilter(0.0), dfYFilter(0.0), nXRadius(0),
      nYRadius(0), nFiltInitX(0), nFiltInitY(0), nSrcXOff(0), nSrcYOff(0),
      nDstXOff(0), nDstYOff(0), pfnTransformer(nullptr),
      pTransformerArg(nullptr), pfnProgress(GDALDummyProgress),
      pProgress(nullptr), dfProgressBase(0.0), dfProgressScale(1.0),
      padfDstNoDataReal(nullptr), psThreadData(nullptr)
{
}

/************************************************************************/
/*                          ~GDALWarpKernel()                           */
/************************************************************************/

GDALWarpKernel::~GDALWarpKernel()
{
}

/************************************************************************/
/*                            PerformWarp()                             */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpKernel::PerformWarp();
 *
 * This method performs the warp described in the GDALWarpKernel.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpKernel::PerformWarp()

{
    const CPLErr eErr = Validate();

    if (eErr != CE_None)
        return eErr;

    // See #2445 and #3079.
    if (nSrcXSize <= 0 || nSrcYSize <= 0)
    {
        if (!pfnProgress(dfProgressBase + dfProgressScale, "", pProgress))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return CE_Failure;
        }
        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      Pre-calculate resampling scales and window sizes for filtering. */
    /* -------------------------------------------------------------------- */

    dfXScale = static_cast<double>(nDstXSize) / (nSrcXSize - dfSrcXExtraSize);
    dfYScale = static_cast<double>(nDstYSize) / (nSrcYSize - dfSrcYExtraSize);
    if (nSrcXSize >= nDstXSize && nSrcXSize <= nDstXSize + dfSrcXExtraSize)
        dfXScale = 1.0;
    if (nSrcYSize >= nDstYSize && nSrcYSize <= nDstYSize + dfSrcYExtraSize)
        dfYScale = 1.0;
    if (dfXScale < 1.0)
    {
        double dfXReciprocalScale = 1.0 / dfXScale;
        const int nXReciprocalScale =
            static_cast<int>(dfXReciprocalScale + 0.5);
        if (fabs(dfXReciprocalScale - nXReciprocalScale) < 0.05)
            dfXScale = 1.0 / nXReciprocalScale;
    }
    if (dfYScale < 1.0)
    {
        double dfYReciprocalScale = 1.0 / dfYScale;
        const int nYReciprocalScale =
            static_cast<int>(dfYReciprocalScale + 0.5);
        if (fabs(dfYReciprocalScale - nYReciprocalScale) < 0.05)
            dfYScale = 1.0 / nYReciprocalScale;
    }

    // XSCALE and YSCALE undocumented for now. Can help in some cases.
    // Best would probably be a per-pixel scale computation.
    const char *pszXScale = CSLFetchNameValue(papszWarpOptions, "XSCALE");
    if (pszXScale != nullptr && !EQUAL(pszXScale, "FROM_GRID_SAMPLING"))
        dfXScale = CPLAtof(pszXScale);
    const char *pszYScale = CSLFetchNameValue(papszWarpOptions, "YSCALE");
    if (pszYScale != nullptr)
        dfYScale = CPLAtof(pszYScale);

    // If the xscale is significantly lower than the yscale, this is highly
    // suspicious of a situation of wrapping a very large virtual file in
    // geographic coordinates with left and right parts being close to the
    // antimeridian. In that situation, the xscale computed by the above method
    // is completely wrong. Prefer doing an average of a few sample points
    // instead
    if ((dfYScale / dfXScale > 100 ||
         (pszXScale != nullptr && EQUAL(pszXScale, "FROM_GRID_SAMPLING"))))
    {
        // Sample points along a grid
        const int nPointsX = std::min(10, nDstXSize);
        const int nPointsY = std::min(10, nDstYSize);
        const int nPoints = 3 * nPointsX * nPointsY;
        std::vector<double> padfX;
        std::vector<double> padfY;
        std::vector<double> padfZ(nPoints);
        std::vector<int> pabSuccess(nPoints);
        for (int iY = 0; iY < nPointsY; iY++)
        {
            for (int iX = 0; iX < nPointsX; iX++)
            {
                const double dfX =
                    nPointsX == 1
                        ? 0.0
                        : static_cast<double>(iX) * nDstXSize / (nPointsX - 1);
                const double dfY =
                    nPointsY == 1
                        ? 0.0
                        : static_cast<double>(iY) * nDstYSize / (nPointsY - 1);

                // Reproject each destination sample point and its neighbours
                // at (x+1,y) and (x,y+1), so as to get the local scale.
                padfX.push_back(dfX);
                padfY.push_back(dfY);

                padfX.push_back((iX == nPointsX - 1) ? dfX - 1 : dfX + 1);
                padfY.push_back(dfY);

                padfX.push_back(dfX);
                padfY.push_back((iY == nPointsY - 1) ? dfY - 1 : dfY + 1);
            }
        }
        pfnTransformer(pTransformerArg, TRUE, nPoints, &padfX[0], &padfY[0],
                       &padfZ[0], &pabSuccess[0]);

        // Compute the xscale at each sampling point
        std::vector<double> adfXScales;
        for (int i = 0; i < nPoints; i += 3)
        {
            if (pabSuccess[i] && pabSuccess[i + 1] && pabSuccess[i + 2])
            {
                const double dfPointXScale =
                    1.0 / std::max(std::abs(padfX[i + 1] - padfX[i]),
                                   std::abs(padfX[i + 2] - padfX[i]));
                adfXScales.push_back(dfPointXScale);
            }
        }

        // Sort by increasing xcale
        std::sort(adfXScales.begin(), adfXScales.end());

        if (!adfXScales.empty())
        {
            // Compute the average of scales, but eliminate outliers small
            // scales, if some samples are just along the discontinuity.
            const double dfMaxPointXScale = adfXScales.back();
            double dfSumPointXScale = 0;
            int nCountPointScale = 0;
            for (double dfPointXScale : adfXScales)
            {
                if (dfPointXScale > dfMaxPointXScale / 10)
                {
                    dfSumPointXScale += dfPointXScale;
                    nCountPointScale++;
                }
            }
            if (nCountPointScale > 0)  // should always be true
            {
                const double dfXScaleFromSampling =
                    dfSumPointXScale / nCountPointScale;
#if DEBUG_VERBOSE
                CPLDebug("WARP", "Correcting dfXScale from %f to %f", dfXScale,
                         dfXScaleFromSampling);
#endif
                dfXScale = dfXScaleFromSampling;
            }
        }
    }

#if DEBUG_VERBOSE
    CPLDebug("WARP", "dfXScale = %f, dfYScale = %f", dfXScale, dfYScale);
#endif

    const int bUse4SamplesFormula = dfXScale >= 0.95 && dfYScale >= 0.95;

    // Safety check for callers that would use GDALWarpKernel without using
    // GDALWarpOperation.
    if ((eResample == GRA_CubicSpline || eResample == GRA_Lanczos ||
         ((eResample == GRA_Cubic || eResample == GRA_Bilinear) &&
          !bUse4SamplesFormula)) &&
        atoi(CSLFetchNameValueDef(papszWarpOptions, "EXTRA_ELTS", "0")) !=
            WARP_EXTRA_ELTS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Source arrays must have WARP_EXTRA_ELTS extra elements at "
                 "their end. "
                 "See GDALWarpKernel class definition. If this condition is "
                 "fulfilled, define a EXTRA_ELTS=%d warp options",
                 WARP_EXTRA_ELTS);
        return CE_Failure;
    }

    dfXFilter = anGWKFilterRadius[eResample];
    dfYFilter = anGWKFilterRadius[eResample];

    nXRadius = dfXScale < 1.0 ? static_cast<int>(ceil(dfXFilter / dfXScale))
                              : static_cast<int>(dfXFilter);
    nYRadius = dfYScale < 1.0 ? static_cast<int>(ceil(dfYFilter / dfYScale))
                              : static_cast<int>(dfYFilter);

    // Filter window offset depends on the parity of the kernel radius.
    nFiltInitX = ((anGWKFilterRadius[eResample] + 1) % 2) - nXRadius;
    nFiltInitY = ((anGWKFilterRadius[eResample] + 1) % 2) - nYRadius;

    bApplyVerticalShift =
        CPLFetchBool(papszWarpOptions, "APPLY_VERTICAL_SHIFT", false);
    dfMultFactorVerticalShift = CPLAtof(CSLFetchNameValueDef(
        papszWarpOptions, "MULT_FACTOR_VERTICAL_SHIFT", "1.0"));

    /* -------------------------------------------------------------------- */
    /*      Set up resampling functions.                                    */
    /* -------------------------------------------------------------------- */
    if (CPLFetchBool(papszWarpOptions, "USE_GENERAL_CASE", false))
        return GWKGeneralCase(this);

#if defined(HAVE_OPENCL)
    if ((eWorkingDataType == GDT_Byte || eWorkingDataType == GDT_CInt16 ||
         eWorkingDataType == GDT_UInt16 || eWorkingDataType == GDT_Int16 ||
         eWorkingDataType == GDT_CFloat32 || eWorkingDataType == GDT_Float32) &&
        (eResample == GRA_Bilinear || eResample == GRA_Cubic ||
         eResample == GRA_CubicSpline || eResample == GRA_Lanczos) &&
        !bApplyVerticalShift &&
        // OpenCL warping gives different results than the ones expected by autotest,
        // so disable it by default even if found.
        CPLTestBool(
            CSLFetchNameValueDef(papszWarpOptions, "USE_OPENCL",
                                 CPLGetConfigOption("GDAL_USE_OPENCL", "NO"))))
    {
        if (pafUnifiedSrcDensity != nullptr)
        {
            // If pafUnifiedSrcDensity is only set to 1.0, then we can
            // discard it.
            bool bFoundNotOne = false;
            for (GPtrDiff_t j = 0;
                 j < static_cast<GPtrDiff_t>(nSrcXSize) * nSrcYSize; j++)
            {
                if (pafUnifiedSrcDensity[j] != 1.0)
                {
                    bFoundNotOne = true;
                    break;
                }
            }
            if (!bFoundNotOne)
            {
                CPLFree(pafUnifiedSrcDensity);
                pafUnifiedSrcDensity = nullptr;
            }
        }

        if (pafUnifiedSrcDensity != nullptr)
        {
            // Typically if there's a cutline or an alpha band
            static bool bHasWarned = false;
            if (!bHasWarned)
            {
                bHasWarned = true;
                CPLDebug("WARP", "pafUnifiedSrcDensity is not null, "
                                 "hence OpenCL warper cannot be used");
            }
        }
        else
        {
            const CPLErr eResult = GWKOpenCLCase(this);

            // CE_Warning tells us a suitable OpenCL environment was not available
            // so we fall through to other CPU based methods.
            if (eResult != CE_Warning)
                return eResult;
        }
    }
#endif  // defined HAVE_OPENCL

    const bool bNoMasksOrDstDensityOnly =
        papanBandSrcValid == nullptr && panUnifiedSrcValid == nullptr &&
        pafUnifiedSrcDensity == nullptr && panDstValid == nullptr;

    if (eWorkingDataType == GDT_Byte && eResample == GRA_NearestNeighbour &&
        bNoMasksOrDstDensityOnly)
        return GWKNearestNoMasksOrDstDensityOnlyByte(this);

    if (eWorkingDataType == GDT_Byte && eResample == GRA_Bilinear &&
        bNoMasksOrDstDensityOnly)
        return GWKBilinearNoMasksOrDstDensityOnlyByte(this);

    if (eWorkingDataType == GDT_Byte && eResample == GRA_Cubic &&
        bNoMasksOrDstDensityOnly)
        return GWKCubicNoMasksOrDstDensityOnlyByte(this);

    if (eWorkingDataType == GDT_Byte && eResample == GRA_CubicSpline &&
        bNoMasksOrDstDensityOnly)
        return GWKCubicSplineNoMasksOrDstDensityOnlyByte(this);

    if (eWorkingDataType == GDT_Byte && eResample == GRA_NearestNeighbour)
        return GWKNearestByte(this);

    if ((eWorkingDataType == GDT_Int16 || eWorkingDataType == GDT_UInt16) &&
        eResample == GRA_NearestNeighbour && bNoMasksOrDstDensityOnly)
        return GWKNearestNoMasksOrDstDensityOnlyShort(this);

    if ((eWorkingDataType == GDT_Int16) && eResample == GRA_Cubic &&
        bNoMasksOrDstDensityOnly)
        return GWKCubicNoMasksOrDstDensityOnlyShort(this);

    if ((eWorkingDataType == GDT_Int16) && eResample == GRA_CubicSpline &&
        bNoMasksOrDstDensityOnly)
        return GWKCubicSplineNoMasksOrDstDensityOnlyShort(this);

    if ((eWorkingDataType == GDT_Int16) && eResample == GRA_Bilinear &&
        bNoMasksOrDstDensityOnly)
        return GWKBilinearNoMasksOrDstDensityOnlyShort(this);

    if ((eWorkingDataType == GDT_UInt16) && eResample == GRA_Cubic &&
        bNoMasksOrDstDensityOnly)
        return GWKCubicNoMasksOrDstDensityOnlyUShort(this);

    if ((eWorkingDataType == GDT_UInt16) && eResample == GRA_CubicSpline &&
        bNoMasksOrDstDensityOnly)
        return GWKCubicSplineNoMasksOrDstDensityOnlyUShort(this);

    if ((eWorkingDataType == GDT_UInt16) && eResample == GRA_Bilinear &&
        bNoMasksOrDstDensityOnly)
        return GWKBilinearNoMasksOrDstDensityOnlyUShort(this);

    if ((eWorkingDataType == GDT_Int16 || eWorkingDataType == GDT_UInt16) &&
        eResample == GRA_NearestNeighbour)
        return GWKNearestShort(this);

    if (eWorkingDataType == GDT_Float32 && eResample == GRA_NearestNeighbour &&
        bNoMasksOrDstDensityOnly)
        return GWKNearestNoMasksOrDstDensityOnlyFloat(this);

    if (eWorkingDataType == GDT_Float32 && eResample == GRA_NearestNeighbour)
        return GWKNearestFloat(this);

    if (eWorkingDataType == GDT_Float32 && eResample == GRA_Bilinear &&
        bNoMasksOrDstDensityOnly)
        return GWKBilinearNoMasksOrDstDensityOnlyFloat(this);

    if (eWorkingDataType == GDT_Float32 && eResample == GRA_Cubic &&
        bNoMasksOrDstDensityOnly)
        return GWKCubicNoMasksOrDstDensityOnlyFloat(this);

#ifdef INSTANTIATE_FLOAT64_SSE2_IMPL
    if (eWorkingDataType == GDT_Float64 && eResample == GRA_Bilinear &&
        bNoMasksOrDstDensityOnly)
        return GWKBilinearNoMasksOrDstDensityOnlyDouble(this);

    if (eWorkingDataType == GDT_Float64 && eResample == GRA_Cubic &&
        bNoMasksOrDstDensityOnly)
        return GWKCubicNoMasksOrDstDensityOnlyDouble(this);
#endif

    if (eResample == GRA_Average)
        return GWKAverageOrMode(this);

    if (eResample == GRA_RMS)
        return GWKAverageOrMode(this);

    if (eResample == GRA_Mode)
        return GWKAverageOrMode(this);

    if (eResample == GRA_Max)
        return GWKAverageOrMode(this);

    if (eResample == GRA_Min)
        return GWKAverageOrMode(this);

    if (eResample == GRA_Med)
        return GWKAverageOrMode(this);

    if (eResample == GRA_Q1)
        return GWKAverageOrMode(this);

    if (eResample == GRA_Q3)
        return GWKAverageOrMode(this);

    if (eResample == GRA_Sum)
        return GWKSumPreserving(this);

    if (!GDALDataTypeIsComplex(eWorkingDataType))
    {
        return GWKRealCase(this);
    }

    return GWKGeneralCase(this);
}

/************************************************************************/
/*                              Validate()                              */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpKernel::Validate()
 *
 * Check the settings in the GDALWarpKernel, and issue a CPLError()
 * (and return CE_Failure) if the configuration is considered to be
 * invalid for some reason.
 *
 * This method will also do some standard defaulting such as setting
 * pfnProgress to GDALDummyProgress() if it is NULL.
 *
 * @return CE_None on success or CE_Failure if an error is detected.
 */

CPLErr GDALWarpKernel::Validate()

{
    if (static_cast<size_t>(eResample) >=
        (sizeof(anGWKFilterRadius) / sizeof(anGWKFilterRadius[0])))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported resampling method %d.",
                 static_cast<int>(eResample));
        return CE_Failure;
    }

    // Tuples of values (e.g. "<R>,<G>,<B>" or "(<R1>,<G1>,<B1>),(<R2>,<G2>,<B2>)") that must
    // be ignored as contributing source pixels during resampling. Only taken into account by
    // Average currently
    const char *pszExcludedValues =
        CSLFetchNameValue(papszWarpOptions, "EXCLUDED_VALUES");
    if (pszExcludedValues)
    {
        const CPLStringList aosTokens(
            CSLTokenizeString2(pszExcludedValues, "(,)", 0));
        if ((aosTokens.size() % nBands) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "EXCLUDED_VALUES should contain one or several tuples of "
                     "%d values formatted like <R>,<G>,<B> or "
                     "(<R1>,<G1>,<B1>),(<R2>,<G2>,<B2>) if there are multiple "
                     "tuples",
                     nBands);
            return CE_Failure;
        }
        std::vector<double> adfTuple;
        for (int i = 0; i < aosTokens.size(); ++i)
        {
            adfTuple.push_back(CPLAtof(aosTokens[i]));
            if (((i + 1) % nBands) == 0)
            {
                m_aadfExcludedValues.push_back(adfTuple);
                adfTuple.clear();
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                         GWKOverlayDensity()                          */
/*                                                                      */
/*      Compute the final density for the destination pixel.  This      */
/*      is a function of the overlay density (passed in) and the        */
/*      original density.                                               */
/************************************************************************/

static void GWKOverlayDensity(const GDALWarpKernel *poWK, GPtrDiff_t iDstOffset,
                              double dfDensity)
{
    if (dfDensity < 0.0001 || poWK->pafDstDensity == nullptr)
        return;

    poWK->pafDstDensity[iDstOffset] = static_cast<float>(
        1.0 - (1.0 - dfDensity) * (1.0 - poWK->pafDstDensity[iDstOffset]));
}

/************************************************************************/
/*                          GWKRoundValueT()                            */
/************************************************************************/

template <class T, bool is_signed> struct sGWKRoundValueT
{
    static T eval(double);
};

template <class T> struct sGWKRoundValueT<T, true> /* signed */
{
    static T eval(double dfValue)
    {
        return static_cast<T>(floor(dfValue + 0.5));
    }
};

template <class T> struct sGWKRoundValueT<T, false> /* unsigned */
{
    static T eval(double dfValue)
    {
        return static_cast<T>(dfValue + 0.5);
    }
};

template <class T> static T GWKRoundValueT(double dfValue)
{
    return sGWKRoundValueT<T, std::numeric_limits<T>::is_signed>::eval(dfValue);
}

template <> float GWKRoundValueT<float>(double dfValue)
{
    return static_cast<float>(dfValue);
}

#ifdef notused
template <> double GWKRoundValueT<double>(double dfValue)
{
    return dfValue;
}
#endif

/************************************************************************/
/*                            GWKClampValueT()                          */
/************************************************************************/

template <class T> static CPL_INLINE T GWKClampValueT(double dfValue)
{
    if (dfValue < std::numeric_limits<T>::min())
        return std::numeric_limits<T>::min();
    else if (dfValue > std::numeric_limits<T>::max())
        return std::numeric_limits<T>::max();
    else
        return GWKRoundValueT<T>(dfValue);
}

template <> float GWKClampValueT<float>(double dfValue)
{
    return static_cast<float>(dfValue);
}

#ifdef notused
template <> double GWKClampValueT<double>(double dfValue)
{
    return dfValue;
}
#endif

/************************************************************************/
/*                         GWKSetPixelValueRealT()                      */
/************************************************************************/

template <class T>
static bool GWKSetPixelValueRealT(const GDALWarpKernel *poWK, int iBand,
                                  GPtrDiff_t iDstOffset, double dfDensity,
                                  T value)
{
    T *pDst = reinterpret_cast<T *>(poWK->papabyDstImage[iBand]);

    /* -------------------------------------------------------------------- */
    /*      If the source density is less than 100% we need to fetch the    */
    /*      existing destination value, and mix it with the source to       */
    /*      get the new "to apply" value.  Also compute composite           */
    /*      density.                                                        */
    /*                                                                      */
    /*      We avoid mixing if density is very near one or risk mixing      */
    /*      in very extreme nodata values and causing odd results (#1610)   */
    /* -------------------------------------------------------------------- */
    if (dfDensity < 0.9999)
    {
        if (dfDensity < 0.0001)
            return true;

        double dfDstDensity = 1.0;

        if (poWK->pafDstDensity != nullptr)
            dfDstDensity = poWK->pafDstDensity[iDstOffset];
        else if (poWK->panDstValid != nullptr &&
                 !CPLMaskGet(poWK->panDstValid, iDstOffset))
            dfDstDensity = 0.0;

        // It seems like we also ought to be testing panDstValid[] here!

        const double dfDstReal = pDst[iDstOffset];

        // The destination density is really only relative to the portion
        // not occluded by the overlay.
        const double dfDstInfluence = (1.0 - dfDensity) * dfDstDensity;

        const double dfReal = (value * dfDensity + dfDstReal * dfDstInfluence) /
                              (dfDensity + dfDstInfluence);

        /* --------------------------------------------------------------------
         */
        /*      Actually apply the destination value. */
        /*                                                                      */
        /*      Avoid using the destination nodata value for integer datatypes
         */
        /*      if by chance it is equal to the computed pixel value. */
        /* --------------------------------------------------------------------
         */
        pDst[iDstOffset] = GWKClampValueT<T>(dfReal);
    }
    else
    {
        pDst[iDstOffset] = value;
    }

    if (poWK->padfDstNoDataReal != nullptr &&
        poWK->padfDstNoDataReal[iBand] == static_cast<double>(pDst[iDstOffset]))
    {
        if (pDst[iDstOffset] == std::numeric_limits<T>::min())
            pDst[iDstOffset] = std::numeric_limits<T>::min() + 1;
        else
            pDst[iDstOffset]--;
    }

    return true;
}

/************************************************************************/
/*                          GWKSetPixelValue()                          */
/************************************************************************/

static bool GWKSetPixelValue(const GDALWarpKernel *poWK, int iBand,
                             GPtrDiff_t iDstOffset, double dfDensity,
                             double dfReal, double dfImag)

{
    GByte *pabyDst = poWK->papabyDstImage[iBand];

    /* -------------------------------------------------------------------- */
    /*      If the source density is less than 100% we need to fetch the    */
    /*      existing destination value, and mix it with the source to       */
    /*      get the new "to apply" value.  Also compute composite           */
    /*      density.                                                        */
    /*                                                                      */
    /*      We avoid mixing if density is very near one or risk mixing      */
    /*      in very extreme nodata values and causing odd results (#1610)   */
    /* -------------------------------------------------------------------- */
    if (dfDensity < 0.9999)
    {
        if (dfDensity < 0.0001)
            return true;

        double dfDstDensity = 1.0;
        if (poWK->pafDstDensity != nullptr)
            dfDstDensity = poWK->pafDstDensity[iDstOffset];
        else if (poWK->panDstValid != nullptr &&
                 !CPLMaskGet(poWK->panDstValid, iDstOffset))
            dfDstDensity = 0.0;

        double dfDstReal = 0.0;
        double dfDstImag = 0.0;
        // It seems like we also ought to be testing panDstValid[] here!

        // TODO(schwehr): Factor out this repreated type of set.
        switch (poWK->eWorkingDataType)
        {
            case GDT_Byte:
                dfDstReal = pabyDst[iDstOffset];
                dfDstImag = 0.0;
                break;

            case GDT_Int8:
                dfDstReal = reinterpret_cast<GInt8 *>(pabyDst)[iDstOffset];
                dfDstImag = 0.0;
                break;

            case GDT_Int16:
                dfDstReal = reinterpret_cast<GInt16 *>(pabyDst)[iDstOffset];
                dfDstImag = 0.0;
                break;

            case GDT_UInt16:
                dfDstReal = reinterpret_cast<GUInt16 *>(pabyDst)[iDstOffset];
                dfDstImag = 0.0;
                break;

            case GDT_Int32:
                dfDstReal = reinterpret_cast<GInt32 *>(pabyDst)[iDstOffset];
                dfDstImag = 0.0;
                break;

            case GDT_UInt32:
                dfDstReal = reinterpret_cast<GUInt32 *>(pabyDst)[iDstOffset];
                dfDstImag = 0.0;
                break;

            case GDT_Int64:
                dfDstReal = static_cast<double>(
                    reinterpret_cast<std::int64_t *>(pabyDst)[iDstOffset]);
                dfDstImag = 0.0;
                break;

            case GDT_UInt64:
                dfDstReal = static_cast<double>(
                    reinterpret_cast<std::uint64_t *>(pabyDst)[iDstOffset]);
                dfDstImag = 0.0;
                break;

            case GDT_Float32:
                dfDstReal = reinterpret_cast<float *>(pabyDst)[iDstOffset];
                dfDstImag = 0.0;
                break;

            case GDT_Float64:
                dfDstReal = reinterpret_cast<double *>(pabyDst)[iDstOffset];
                dfDstImag = 0.0;
                break;

            case GDT_CInt16:
                dfDstReal = reinterpret_cast<GInt16 *>(pabyDst)[iDstOffset * 2];
                dfDstImag =
                    reinterpret_cast<GInt16 *>(pabyDst)[iDstOffset * 2 + 1];
                break;

            case GDT_CInt32:
                dfDstReal = reinterpret_cast<GInt32 *>(pabyDst)[iDstOffset * 2];
                dfDstImag =
                    reinterpret_cast<GInt32 *>(pabyDst)[iDstOffset * 2 + 1];
                break;

            case GDT_CFloat32:
                dfDstReal = reinterpret_cast<float *>(pabyDst)[iDstOffset * 2];
                dfDstImag =
                    reinterpret_cast<float *>(pabyDst)[iDstOffset * 2 + 1];
                break;

            case GDT_CFloat64:
                dfDstReal = reinterpret_cast<double *>(pabyDst)[iDstOffset * 2];
                dfDstImag =
                    reinterpret_cast<double *>(pabyDst)[iDstOffset * 2 + 1];
                break;

            case GDT_Unknown:
            case GDT_TypeCount:
                CPLAssert(false);
                return false;
        }

        // The destination density is really only relative to the portion
        // not occluded by the overlay.
        const double dfDstInfluence = (1.0 - dfDensity) * dfDstDensity;

        dfReal = (dfReal * dfDensity + dfDstReal * dfDstInfluence) /
                 (dfDensity + dfDstInfluence);

        dfImag = (dfImag * dfDensity + dfDstImag * dfDstInfluence) /
                 (dfDensity + dfDstInfluence);
    }

/* -------------------------------------------------------------------- */
/*      Actually apply the destination value.                           */
/*                                                                      */
/*      Avoid using the destination nodata value for integer datatypes  */
/*      if by chance it is equal to the computed pixel value.           */
/* -------------------------------------------------------------------- */

// TODO(schwehr): Can we make this a template?
#define CLAMP(type)                                                            \
    do                                                                         \
    {                                                                          \
        type *_pDst = reinterpret_cast<type *>(pabyDst);                       \
        if (dfReal < static_cast<double>(std::numeric_limits<type>::min()))    \
            _pDst[iDstOffset] =                                                \
                static_cast<type>(std::numeric_limits<type>::min());           \
        else if (dfReal >                                                      \
                 static_cast<double>(std::numeric_limits<type>::max()))        \
            _pDst[iDstOffset] =                                                \
                static_cast<type>(std::numeric_limits<type>::max());           \
        else                                                                   \
            _pDst[iDstOffset] = (std::numeric_limits<type>::is_signed)         \
                                    ? static_cast<type>(floor(dfReal + 0.5))   \
                                    : static_cast<type>(dfReal + 0.5);         \
        if (poWK->padfDstNoDataReal != nullptr &&                              \
            poWK->padfDstNoDataReal[iBand] ==                                  \
                static_cast<double>(_pDst[iDstOffset]))                        \
        {                                                                      \
            if (_pDst[iDstOffset] ==                                           \
                static_cast<type>(std::numeric_limits<type>::min()))           \
                _pDst[iDstOffset] =                                            \
                    static_cast<type>(std::numeric_limits<type>::min() + 1);   \
            else                                                               \
                _pDst[iDstOffset]--;                                           \
        }                                                                      \
    } while (false)

    switch (poWK->eWorkingDataType)
    {
        case GDT_Byte:
            CLAMP(GByte);
            break;

        case GDT_Int8:
            CLAMP(GInt8);
            break;

        case GDT_Int16:
            CLAMP(GInt16);
            break;

        case GDT_UInt16:
            CLAMP(GUInt16);
            break;

        case GDT_UInt32:
            CLAMP(GUInt32);
            break;

        case GDT_Int32:
            CLAMP(GInt32);
            break;

        case GDT_UInt64:
            CLAMP(std::uint64_t);
            break;

        case GDT_Int64:
            CLAMP(std::int64_t);
            break;

        case GDT_Float32:
            reinterpret_cast<float *>(pabyDst)[iDstOffset] =
                static_cast<float>(dfReal);
            break;

        case GDT_Float64:
            reinterpret_cast<double *>(pabyDst)[iDstOffset] = dfReal;
            break;

        case GDT_CInt16:
        {
            typedef GInt16 T;
            if (dfReal < static_cast<double>(std::numeric_limits<T>::min()))
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2] =
                    std::numeric_limits<T>::min();
            else if (dfReal >
                     static_cast<double>(std::numeric_limits<T>::max()))
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2] =
                    std::numeric_limits<T>::max();
            else
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2] =
                    static_cast<T>(floor(dfReal + 0.5));
            if (dfImag < static_cast<double>(std::numeric_limits<T>::min()))
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2 + 1] =
                    std::numeric_limits<T>::min();
            else if (dfImag >
                     static_cast<double>(std::numeric_limits<T>::max()))
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2 + 1] =
                    std::numeric_limits<T>::max();
            else
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2 + 1] =
                    static_cast<T>(floor(dfImag + 0.5));
            break;
        }

        case GDT_CInt32:
        {
            typedef GInt32 T;
            if (dfReal < static_cast<double>(std::numeric_limits<T>::min()))
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2] =
                    std::numeric_limits<T>::min();
            else if (dfReal >
                     static_cast<double>(std::numeric_limits<T>::max()))
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2] =
                    std::numeric_limits<T>::max();
            else
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2] =
                    static_cast<T>(floor(dfReal + 0.5));
            if (dfImag < static_cast<double>(std::numeric_limits<T>::min()))
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2 + 1] =
                    std::numeric_limits<T>::min();
            else if (dfImag >
                     static_cast<double>(std::numeric_limits<T>::max()))
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2 + 1] =
                    std::numeric_limits<T>::max();
            else
                reinterpret_cast<T *>(pabyDst)[iDstOffset * 2 + 1] =
                    static_cast<T>(floor(dfImag + 0.5));
            break;
        }

        case GDT_CFloat32:
            reinterpret_cast<float *>(pabyDst)[iDstOffset * 2] =
                static_cast<float>(dfReal);
            reinterpret_cast<float *>(pabyDst)[iDstOffset * 2 + 1] =
                static_cast<float>(dfImag);
            break;

        case GDT_CFloat64:
            reinterpret_cast<double *>(pabyDst)[iDstOffset * 2] = dfReal;
            reinterpret_cast<double *>(pabyDst)[iDstOffset * 2 + 1] = dfImag;
            break;

        case GDT_Unknown:
        case GDT_TypeCount:
            return false;
    }

    return true;
}

/************************************************************************/
/*                       GWKSetPixelValueReal()                         */
/************************************************************************/

static bool GWKSetPixelValueReal(const GDALWarpKernel *poWK, int iBand,
                                 GPtrDiff_t iDstOffset, double dfDensity,
                                 double dfReal)

{
    GByte *pabyDst = poWK->papabyDstImage[iBand];

    /* -------------------------------------------------------------------- */
    /*      If the source density is less than 100% we need to fetch the    */
    /*      existing destination value, and mix it with the source to       */
    /*      get the new "to apply" value.  Also compute composite           */
    /*      density.                                                        */
    /*                                                                      */
    /*      We avoid mixing if density is very near one or risk mixing      */
    /*      in very extreme nodata values and causing odd results (#1610)   */
    /* -------------------------------------------------------------------- */
    if (dfDensity < 0.9999)
    {
        if (dfDensity < 0.0001)
            return true;

        double dfDstReal = 0.0;
        double dfDstDensity = 1.0;

        if (poWK->pafDstDensity != nullptr)
            dfDstDensity = poWK->pafDstDensity[iDstOffset];
        else if (poWK->panDstValid != nullptr &&
                 !CPLMaskGet(poWK->panDstValid, iDstOffset))
            dfDstDensity = 0.0;

        // It seems like we also ought to be testing panDstValid[] here!

        switch (poWK->eWorkingDataType)
        {
            case GDT_Byte:
                dfDstReal = pabyDst[iDstOffset];
                break;

            case GDT_Int8:
                dfDstReal = reinterpret_cast<GInt8 *>(pabyDst)[iDstOffset];
                break;

            case GDT_Int16:
                dfDstReal = reinterpret_cast<GInt16 *>(pabyDst)[iDstOffset];
                break;

            case GDT_UInt16:
                dfDstReal = reinterpret_cast<GUInt16 *>(pabyDst)[iDstOffset];
                break;

            case GDT_Int32:
                dfDstReal = reinterpret_cast<GInt32 *>(pabyDst)[iDstOffset];
                break;

            case GDT_UInt32:
                dfDstReal = reinterpret_cast<GUInt32 *>(pabyDst)[iDstOffset];
                break;

            case GDT_Int64:
                dfDstReal = static_cast<double>(
                    reinterpret_cast<std::int64_t *>(pabyDst)[iDstOffset]);
                break;

            case GDT_UInt64:
                dfDstReal = static_cast<double>(
                    reinterpret_cast<std::uint64_t *>(pabyDst)[iDstOffset]);
                break;

            case GDT_Float32:
                dfDstReal = reinterpret_cast<float *>(pabyDst)[iDstOffset];
                break;

            case GDT_Float64:
                dfDstReal = reinterpret_cast<double *>(pabyDst)[iDstOffset];
                break;

            case GDT_CInt16:
            case GDT_CInt32:
            case GDT_CFloat32:
            case GDT_CFloat64:
            case GDT_Unknown:
            case GDT_TypeCount:
                CPLAssert(false);
                return false;
        }

        // The destination density is really only relative to the portion
        // not occluded by the overlay.
        const double dfDstInfluence = (1.0 - dfDensity) * dfDstDensity;

        dfReal = (dfReal * dfDensity + dfDstReal * dfDstInfluence) /
                 (dfDensity + dfDstInfluence);
    }

    /* -------------------------------------------------------------------- */
    /*      Actually apply the destination value.                           */
    /*                                                                      */
    /*      Avoid using the destination nodata value for integer datatypes  */
    /*      if by chance it is equal to the computed pixel value.           */
    /* -------------------------------------------------------------------- */

    switch (poWK->eWorkingDataType)
    {
        case GDT_Byte:
            CLAMP(GByte);
            break;

        case GDT_Int8:
            CLAMP(GInt8);
            break;

        case GDT_Int16:
            CLAMP(GInt16);
            break;

        case GDT_UInt16:
            CLAMP(GUInt16);
            break;

        case GDT_UInt32:
            CLAMP(GUInt32);
            break;

        case GDT_Int32:
            CLAMP(GInt32);
            break;

        case GDT_UInt64:
            CLAMP(std::uint64_t);
            break;

        case GDT_Int64:
            CLAMP(std::int64_t);
            break;

        case GDT_Float32:
            reinterpret_cast<float *>(pabyDst)[iDstOffset] =
                static_cast<float>(dfReal);
            break;

        case GDT_Float64:
            reinterpret_cast<double *>(pabyDst)[iDstOffset] = dfReal;
            break;

        case GDT_CInt16:
        case GDT_CInt32:
        case GDT_CFloat32:
        case GDT_CFloat64:
            return false;

        case GDT_Unknown:
        case GDT_TypeCount:
            CPLAssert(false);
            return false;
    }

    return true;
}

/************************************************************************/
/*                          GWKGetPixelValue()                          */
/************************************************************************/

/* It is assumed that panUnifiedSrcValid has been checked before */

static bool GWKGetPixelValue(const GDALWarpKernel *poWK, int iBand,
                             GPtrDiff_t iSrcOffset, double *pdfDensity,
                             double *pdfReal, double *pdfImag)

{
    GByte *pabySrc = poWK->papabySrcImage[iBand];

    if (poWK->papanBandSrcValid != nullptr &&
        poWK->papanBandSrcValid[iBand] != nullptr &&
        !CPLMaskGet(poWK->papanBandSrcValid[iBand], iSrcOffset))
    {
        *pdfDensity = 0.0;
        return false;
    }

    *pdfReal = 0.0;
    *pdfImag = 0.0;

    // TODO(schwehr): Fix casting.
    switch (poWK->eWorkingDataType)
    {
        case GDT_Byte:
            *pdfReal = pabySrc[iSrcOffset];
            *pdfImag = 0.0;
            break;

        case GDT_Int8:
            *pdfReal = reinterpret_cast<GInt8 *>(pabySrc)[iSrcOffset];
            *pdfImag = 0.0;
            break;

        case GDT_Int16:
            *pdfReal = reinterpret_cast<GInt16 *>(pabySrc)[iSrcOffset];
            *pdfImag = 0.0;
            break;

        case GDT_UInt16:
            *pdfReal = reinterpret_cast<GUInt16 *>(pabySrc)[iSrcOffset];
            *pdfImag = 0.0;
            break;

        case GDT_Int32:
            *pdfReal = reinterpret_cast<GInt32 *>(pabySrc)[iSrcOffset];
            *pdfImag = 0.0;
            break;

        case GDT_UInt32:
            *pdfReal = reinterpret_cast<GUInt32 *>(pabySrc)[iSrcOffset];
            *pdfImag = 0.0;
            break;

        case GDT_Int64:
            *pdfReal = static_cast<double>(
                reinterpret_cast<std::int64_t *>(pabySrc)[iSrcOffset]);
            *pdfImag = 0.0;
            break;

        case GDT_UInt64:
            *pdfReal = static_cast<double>(
                reinterpret_cast<std::uint64_t *>(pabySrc)[iSrcOffset]);
            *pdfImag = 0.0;
            break;

        case GDT_Float32:
            *pdfReal = reinterpret_cast<float *>(pabySrc)[iSrcOffset];
            *pdfImag = 0.0;
            break;

        case GDT_Float64:
            *pdfReal = reinterpret_cast<double *>(pabySrc)[iSrcOffset];
            *pdfImag = 0.0;
            break;

        case GDT_CInt16:
            *pdfReal = reinterpret_cast<GInt16 *>(pabySrc)[iSrcOffset * 2];
            *pdfImag = reinterpret_cast<GInt16 *>(pabySrc)[iSrcOffset * 2 + 1];
            break;

        case GDT_CInt32:
            *pdfReal = reinterpret_cast<GInt32 *>(pabySrc)[iSrcOffset * 2];
            *pdfImag = reinterpret_cast<GInt32 *>(pabySrc)[iSrcOffset * 2 + 1];
            break;

        case GDT_CFloat32:
            *pdfReal = reinterpret_cast<float *>(pabySrc)[iSrcOffset * 2];
            *pdfImag = reinterpret_cast<float *>(pabySrc)[iSrcOffset * 2 + 1];
            break;

        case GDT_CFloat64:
            *pdfReal = reinterpret_cast<double *>(pabySrc)[iSrcOffset * 2];
            *pdfImag = reinterpret_cast<double *>(pabySrc)[iSrcOffset * 2 + 1];
            break;

        case GDT_Unknown:
        case GDT_TypeCount:
            CPLAssert(false);
            *pdfDensity = 0.0;
            return false;
    }

    if (poWK->pafUnifiedSrcDensity != nullptr)
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
    else
        *pdfDensity = 1.0;

    return *pdfDensity != 0.0;
}

/************************************************************************/
/*                       GWKGetPixelValueReal()                         */
/************************************************************************/

static bool GWKGetPixelValueReal(const GDALWarpKernel *poWK, int iBand,
                                 GPtrDiff_t iSrcOffset, double *pdfDensity,
                                 double *pdfReal)

{
    GByte *pabySrc = poWK->papabySrcImage[iBand];

    if (poWK->papanBandSrcValid != nullptr &&
        poWK->papanBandSrcValid[iBand] != nullptr &&
        !CPLMaskGet(poWK->papanBandSrcValid[iBand], iSrcOffset))
    {
        *pdfDensity = 0.0;
        return false;
    }

    switch (poWK->eWorkingDataType)
    {
        case GDT_Byte:
            *pdfReal = pabySrc[iSrcOffset];
            break;

        case GDT_Int8:
            *pdfReal = reinterpret_cast<GInt8 *>(pabySrc)[iSrcOffset];
            break;

        case GDT_Int16:
            *pdfReal = reinterpret_cast<GInt16 *>(pabySrc)[iSrcOffset];
            break;

        case GDT_UInt16:
            *pdfReal = reinterpret_cast<GUInt16 *>(pabySrc)[iSrcOffset];
            break;

        case GDT_Int32:
            *pdfReal = reinterpret_cast<GInt32 *>(pabySrc)[iSrcOffset];
            break;

        case GDT_UInt32:
            *pdfReal = reinterpret_cast<GUInt32 *>(pabySrc)[iSrcOffset];
            break;

        case GDT_Int64:
            *pdfReal = static_cast<double>(
                reinterpret_cast<std::int64_t *>(pabySrc)[iSrcOffset]);
            break;

        case GDT_UInt64:
            *pdfReal = static_cast<double>(
                reinterpret_cast<std::uint64_t *>(pabySrc)[iSrcOffset]);
            break;

        case GDT_Float32:
            *pdfReal = reinterpret_cast<float *>(pabySrc)[iSrcOffset];
            break;

        case GDT_Float64:
            *pdfReal = reinterpret_cast<double *>(pabySrc)[iSrcOffset];
            break;

        case GDT_CInt16:
        case GDT_CInt32:
        case GDT_CFloat32:
        case GDT_CFloat64:
        case GDT_Unknown:
        case GDT_TypeCount:
            CPLAssert(false);
            return false;
    }

    if (poWK->pafUnifiedSrcDensity != nullptr)
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
    else
        *pdfDensity = 1.0;

    return *pdfDensity != 0.0;
}

/************************************************************************/
/*                          GWKGetPixelRow()                            */
/************************************************************************/

/* It is assumed that adfImag[] is set to 0 by caller code for non-complex */
/* data-types. */

static bool GWKGetPixelRow(const GDALWarpKernel *poWK, int iBand,
                           GPtrDiff_t iSrcOffset, int nHalfSrcLen,
                           double *padfDensity, double adfReal[],
                           double *padfImag)
{
    // We know that nSrcLen is even, so we can *always* unroll loops 2x.
    const int nSrcLen = nHalfSrcLen * 2;
    bool bHasValid = false;

    if (padfDensity != nullptr)
    {
        // Init the density.
        for (int i = 0; i < nSrcLen; i += 2)
        {
            padfDensity[i] = 1.0;
            padfDensity[i + 1] = 1.0;
        }

        if (poWK->panUnifiedSrcValid != nullptr)
        {
            for (int i = 0; i < nSrcLen; i += 2)
            {
                if (CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset + i))
                    bHasValid = true;
                else
                    padfDensity[i] = 0.0;

                if (CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset + i + 1))
                    bHasValid = true;
                else
                    padfDensity[i + 1] = 0.0;
            }

            // Reset or fail as needed.
            if (bHasValid)
                bHasValid = false;
            else
                return false;
        }

        if (poWK->papanBandSrcValid != nullptr &&
            poWK->papanBandSrcValid[iBand] != nullptr)
        {
            for (int i = 0; i < nSrcLen; i += 2)
            {
                if (CPLMaskGet(poWK->papanBandSrcValid[iBand], iSrcOffset + i))
                    bHasValid = true;
                else
                    padfDensity[i] = 0.0;

                if (CPLMaskGet(poWK->papanBandSrcValid[iBand],
                               iSrcOffset + i + 1))
                    bHasValid = true;
                else
                    padfDensity[i + 1] = 0.0;
            }

            // Reset or fail as needed.
            if (bHasValid)
                bHasValid = false;
            else
                return false;
        }
    }

    // TODO(schwehr): Fix casting.
    // Fetch data.
    switch (poWK->eWorkingDataType)
    {
        case GDT_Byte:
        {
            GByte *pSrc =
                reinterpret_cast<GByte *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[i];
                adfReal[i + 1] = pSrc[i + 1];
            }
            break;
        }

        case GDT_Int8:
        {
            GInt8 *pSrc =
                reinterpret_cast<GInt8 *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[i];
                adfReal[i + 1] = pSrc[i + 1];
            }
            break;
        }

        case GDT_Int16:
        {
            GInt16 *pSrc =
                reinterpret_cast<GInt16 *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[i];
                adfReal[i + 1] = pSrc[i + 1];
            }
            break;
        }

        case GDT_UInt16:
        {
            GUInt16 *pSrc =
                reinterpret_cast<GUInt16 *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[i];
                adfReal[i + 1] = pSrc[i + 1];
            }
            break;
        }

        case GDT_Int32:
        {
            GInt32 *pSrc =
                reinterpret_cast<GInt32 *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[i];
                adfReal[i + 1] = pSrc[i + 1];
            }
            break;
        }

        case GDT_UInt32:
        {
            GUInt32 *pSrc =
                reinterpret_cast<GUInt32 *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[i];
                adfReal[i + 1] = pSrc[i + 1];
            }
            break;
        }

        case GDT_Int64:
        {
            auto pSrc =
                reinterpret_cast<std::int64_t *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = static_cast<double>(pSrc[i]);
                adfReal[i + 1] = static_cast<double>(pSrc[i + 1]);
            }
            break;
        }

        case GDT_UInt64:
        {
            auto pSrc =
                reinterpret_cast<std::uint64_t *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = static_cast<double>(pSrc[i]);
                adfReal[i + 1] = static_cast<double>(pSrc[i + 1]);
            }
            break;
        }

        case GDT_Float32:
        {
            float *pSrc =
                reinterpret_cast<float *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[i];
                adfReal[i + 1] = pSrc[i + 1];
            }
            break;
        }

        case GDT_Float64:
        {
            double *pSrc =
                reinterpret_cast<double *>(poWK->papabySrcImage[iBand]);
            pSrc += iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[i];
                adfReal[i + 1] = pSrc[i + 1];
            }
            break;
        }

        case GDT_CInt16:
        {
            GInt16 *pSrc =
                reinterpret_cast<GInt16 *>(poWK->papabySrcImage[iBand]);
            pSrc += 2 * iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[2 * i];
                padfImag[i] = pSrc[2 * i + 1];

                adfReal[i + 1] = pSrc[2 * i + 2];
                padfImag[i + 1] = pSrc[2 * i + 3];
            }
            break;
        }

        case GDT_CInt32:
        {
            GInt32 *pSrc =
                reinterpret_cast<GInt32 *>(poWK->papabySrcImage[iBand]);
            pSrc += 2 * iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[2 * i];
                padfImag[i] = pSrc[2 * i + 1];

                adfReal[i + 1] = pSrc[2 * i + 2];
                padfImag[i + 1] = pSrc[2 * i + 3];
            }
            break;
        }

        case GDT_CFloat32:
        {
            float *pSrc =
                reinterpret_cast<float *>(poWK->papabySrcImage[iBand]);
            pSrc += 2 * iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[2 * i];
                padfImag[i] = pSrc[2 * i + 1];

                adfReal[i + 1] = pSrc[2 * i + 2];
                padfImag[i + 1] = pSrc[2 * i + 3];
            }
            break;
        }

        case GDT_CFloat64:
        {
            double *pSrc =
                reinterpret_cast<double *>(poWK->papabySrcImage[iBand]);
            pSrc += 2 * iSrcOffset;
            for (int i = 0; i < nSrcLen; i += 2)
            {
                adfReal[i] = pSrc[2 * i];
                padfImag[i] = pSrc[2 * i + 1];

                adfReal[i + 1] = pSrc[2 * i + 2];
                padfImag[i + 1] = pSrc[2 * i + 3];
            }
            break;
        }

        case GDT_Unknown:
        case GDT_TypeCount:
            CPLAssert(false);
            if (padfDensity)
                memset(padfDensity, 0, nSrcLen * sizeof(double));
            return false;
    }

    if (padfDensity == nullptr)
        return true;

    if (poWK->pafUnifiedSrcDensity == nullptr)
    {
        for (int i = 0; i < nSrcLen; i += 2)
        {
            // Take into account earlier calcs.
            if (padfDensity[i] > SRC_DENSITY_THRESHOLD)
            {
                padfDensity[i] = 1.0;
                bHasValid = true;
            }

            if (padfDensity[i + 1] > SRC_DENSITY_THRESHOLD)
            {
                padfDensity[i + 1] = 1.0;
                bHasValid = true;
            }
        }
    }
    else
    {
        for (int i = 0; i < nSrcLen; i += 2)
        {
            if (padfDensity[i] > SRC_DENSITY_THRESHOLD)
                padfDensity[i] = poWK->pafUnifiedSrcDensity[iSrcOffset + i];
            if (padfDensity[i] > SRC_DENSITY_THRESHOLD)
                bHasValid = true;

            if (padfDensity[i + 1] > SRC_DENSITY_THRESHOLD)
                padfDensity[i + 1] =
                    poWK->pafUnifiedSrcDensity[iSrcOffset + i + 1];
            if (padfDensity[i + 1] > SRC_DENSITY_THRESHOLD)
                bHasValid = true;
        }
    }

    return bHasValid;
}

/************************************************************************/
/*                          GWKGetPixelT()                              */
/************************************************************************/

template <class T>
static bool GWKGetPixelT(const GDALWarpKernel *poWK, int iBand,
                         GPtrDiff_t iSrcOffset, double *pdfDensity, T *pValue)

{
    T *pSrc = reinterpret_cast<T *>(poWK->papabySrcImage[iBand]);

    if ((poWK->panUnifiedSrcValid != nullptr &&
         !CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset)) ||
        (poWK->papanBandSrcValid != nullptr &&
         poWK->papanBandSrcValid[iBand] != nullptr &&
         !CPLMaskGet(poWK->papanBandSrcValid[iBand], iSrcOffset)))
    {
        *pdfDensity = 0.0;
        return false;
    }

    *pValue = pSrc[iSrcOffset];

    if (poWK->pafUnifiedSrcDensity == nullptr)
        *pdfDensity = 1.0;
    else
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];

    return *pdfDensity != 0.0;
}

/************************************************************************/
/*                        GWKBilinearResample()                         */
/*     Set of bilinear interpolators                                    */
/************************************************************************/

static bool GWKBilinearResample4Sample(const GDALWarpKernel *poWK, int iBand,
                                       double dfSrcX, double dfSrcY,
                                       double *pdfDensity, double *pdfReal,
                                       double *pdfImag)

{
    // Save as local variables to avoid following pointers.
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    int iSrcX = static_cast<int>(floor(dfSrcX - 0.5));
    int iSrcY = static_cast<int>(floor(dfSrcY - 0.5));
    double dfRatioX = 1.5 - (dfSrcX - iSrcX);
    double dfRatioY = 1.5 - (dfSrcY - iSrcY);
    bool bShifted = false;

    if (iSrcX == -1)
    {
        iSrcX = 0;
        dfRatioX = 1;
    }
    if (iSrcY == -1)
    {
        iSrcY = 0;
        dfRatioY = 1;
    }
    GPtrDiff_t iSrcOffset = iSrcX + static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

    // Shift so we don't overrun the array.
    if (static_cast<GPtrDiff_t>(nSrcXSize) * nSrcYSize == iSrcOffset + 1 ||
        static_cast<GPtrDiff_t>(nSrcXSize) * nSrcYSize ==
            iSrcOffset + nSrcXSize + 1)
    {
        bShifted = true;
        --iSrcOffset;
    }

    double adfDensity[2] = {0.0, 0.0};
    double adfReal[2] = {0.0, 0.0};
    double adfImag[2] = {0.0, 0.0};
    double dfAccumulatorReal = 0.0;
    double dfAccumulatorImag = 0.0;
    double dfAccumulatorDensity = 0.0;
    double dfAccumulatorDivisor = 0.0;

    const GPtrDiff_t nSrcPixels =
        static_cast<GPtrDiff_t>(nSrcXSize) * nSrcYSize;
    // Get pixel row.
    if (iSrcY >= 0 && iSrcY < nSrcYSize && iSrcOffset >= 0 &&
        iSrcOffset < nSrcPixels &&
        GWKGetPixelRow(poWK, iBand, iSrcOffset, 1, adfDensity, adfReal,
                       adfImag))
    {
        double dfMult1 = dfRatioX * dfRatioY;
        double dfMult2 = (1.0 - dfRatioX) * dfRatioY;

        // Shifting corrected.
        if (bShifted)
        {
            adfReal[0] = adfReal[1];
            adfImag[0] = adfImag[1];
            adfDensity[0] = adfDensity[1];
        }

        // Upper Left Pixel.
        if (iSrcX >= 0 && iSrcX < nSrcXSize &&
            adfDensity[0] > SRC_DENSITY_THRESHOLD)
        {
            dfAccumulatorDivisor += dfMult1;

            dfAccumulatorReal += adfReal[0] * dfMult1;
            dfAccumulatorImag += adfImag[0] * dfMult1;
            dfAccumulatorDensity += adfDensity[0] * dfMult1;
        }

        // Upper Right Pixel.
        if (iSrcX + 1 >= 0 && iSrcX + 1 < nSrcXSize &&
            adfDensity[1] > SRC_DENSITY_THRESHOLD)
        {
            dfAccumulatorDivisor += dfMult2;

            dfAccumulatorReal += adfReal[1] * dfMult2;
            dfAccumulatorImag += adfImag[1] * dfMult2;
            dfAccumulatorDensity += adfDensity[1] * dfMult2;
        }
    }

    // Get pixel row.
    if (iSrcY + 1 >= 0 && iSrcY + 1 < nSrcYSize &&
        iSrcOffset + nSrcXSize >= 0 && iSrcOffset + nSrcXSize < nSrcPixels &&
        GWKGetPixelRow(poWK, iBand, iSrcOffset + nSrcXSize, 1, adfDensity,
                       adfReal, adfImag))
    {
        double dfMult1 = dfRatioX * (1.0 - dfRatioY);
        double dfMult2 = (1.0 - dfRatioX) * (1.0 - dfRatioY);

        // Shifting corrected
        if (bShifted)
        {
            adfReal[0] = adfReal[1];
            adfImag[0] = adfImag[1];
            adfDensity[0] = adfDensity[1];
        }

        // Lower Left Pixel
        if (iSrcX >= 0 && iSrcX < nSrcXSize &&
            adfDensity[0] > SRC_DENSITY_THRESHOLD)
        {
            dfAccumulatorDivisor += dfMult1;

            dfAccumulatorReal += adfReal[0] * dfMult1;
            dfAccumulatorImag += adfImag[0] * dfMult1;
            dfAccumulatorDensity += adfDensity[0] * dfMult1;
        }

        // Lower Right Pixel.
        if (iSrcX + 1 >= 0 && iSrcX + 1 < nSrcXSize &&
            adfDensity[1] > SRC_DENSITY_THRESHOLD)
        {
            dfAccumulatorDivisor += dfMult2;

            dfAccumulatorReal += adfReal[1] * dfMult2;
            dfAccumulatorImag += adfImag[1] * dfMult2;
            dfAccumulatorDensity += adfDensity[1] * dfMult2;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Return result.                                                  */
    /* -------------------------------------------------------------------- */
    if (dfAccumulatorDivisor == 1.0)
    {
        *pdfReal = dfAccumulatorReal;
        *pdfImag = dfAccumulatorImag;
        *pdfDensity = dfAccumulatorDensity;
        return false;
    }
    else if (dfAccumulatorDivisor < 0.00001)
    {
        *pdfReal = 0.0;
        *pdfImag = 0.0;
        *pdfDensity = 0.0;
        return false;
    }
    else
    {
        *pdfReal = dfAccumulatorReal / dfAccumulatorDivisor;
        *pdfImag = dfAccumulatorImag / dfAccumulatorDivisor;
        *pdfDensity = dfAccumulatorDensity / dfAccumulatorDivisor;
        return true;
    }
}

template <class T>
static bool GWKBilinearResampleNoMasks4SampleT(const GDALWarpKernel *poWK,
                                               int iBand, double dfSrcX,
                                               double dfSrcY, T *pValue)

{

    const int iSrcX = static_cast<int>(floor(dfSrcX - 0.5));
    const int iSrcY = static_cast<int>(floor(dfSrcY - 0.5));
    GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * poWK->nSrcXSize;
    const double dfRatioX = 1.5 - (dfSrcX - iSrcX);
    const double dfRatioY = 1.5 - (dfSrcY - iSrcY);

    const T *const pSrc = reinterpret_cast<T *>(poWK->papabySrcImage[iBand]);

    if (iSrcX >= 0 && iSrcX + 1 < poWK->nSrcXSize && iSrcY >= 0 &&
        iSrcY + 1 < poWK->nSrcYSize)
    {
        const double dfAccumulator =
            (pSrc[iSrcOffset] * dfRatioX +
             pSrc[iSrcOffset + 1] * (1.0 - dfRatioX)) *
                dfRatioY +
            (pSrc[iSrcOffset + poWK->nSrcXSize] * dfRatioX +
             pSrc[iSrcOffset + 1 + poWK->nSrcXSize] * (1.0 - dfRatioX)) *
                (1.0 - dfRatioY);

        *pValue = GWKRoundValueT<T>(dfAccumulator);

        return true;
    }

    double dfAccumulatorDivisor = 0.0;
    double dfAccumulator = 0.0;

    // Upper Left Pixel.
    if (iSrcX >= 0 && iSrcX < poWK->nSrcXSize && iSrcY >= 0 &&
        iSrcY < poWK->nSrcYSize)
    {
        const double dfMult = dfRatioX * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulator += pSrc[iSrcOffset] * dfMult;
    }

    // Upper Right Pixel.
    if (iSrcX + 1 >= 0 && iSrcX + 1 < poWK->nSrcXSize && iSrcY >= 0 &&
        iSrcY < poWK->nSrcYSize)
    {
        const double dfMult = (1.0 - dfRatioX) * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulator += pSrc[iSrcOffset + 1] * dfMult;
    }

    // Lower Right Pixel.
    if (iSrcX + 1 >= 0 && iSrcX + 1 < poWK->nSrcXSize && iSrcY + 1 >= 0 &&
        iSrcY + 1 < poWK->nSrcYSize)
    {
        const double dfMult = (1.0 - dfRatioX) * (1.0 - dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulator += pSrc[iSrcOffset + 1 + poWK->nSrcXSize] * dfMult;
    }

    // Lower Left Pixel.
    if (iSrcX >= 0 && iSrcX < poWK->nSrcXSize && iSrcY + 1 >= 0 &&
        iSrcY + 1 < poWK->nSrcYSize)
    {
        const double dfMult = dfRatioX * (1.0 - dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulator += pSrc[iSrcOffset + poWK->nSrcXSize] * dfMult;
    }

    /* -------------------------------------------------------------------- */
    /*      Return result.                                                  */
    /* -------------------------------------------------------------------- */
    double dfValue = 0.0;

    if (dfAccumulatorDivisor < 0.00001)
    {
        *pValue = 0;
        return false;
    }
    else if (dfAccumulatorDivisor == 1.0)
    {
        dfValue = dfAccumulator;
    }
    else
    {
        dfValue = dfAccumulator / dfAccumulatorDivisor;
    }

    *pValue = GWKRoundValueT<T>(dfValue);

    return true;
}

/************************************************************************/
/*                        GWKCubicResample()                            */
/*     Set of bicubic interpolators using cubic convolution.            */
/************************************************************************/

// http://verona.fi-p.unam.mx/boris/practicas/CubConvInterp.pdf Formula 18
// or http://en.wikipedia.org/wiki/Cubic_Hermite_spline : CINTx(p_1,p0,p1,p2)
// http://en.wikipedia.org/wiki/Bicubic_interpolation: matrix notation

template <typename T>
static inline T CubicConvolution(T distance1, T distance2, T distance3, T f0,
                                 T f1, T f2, T f3)
{
    return (f1 + T(0.5) * (distance1 * (f2 - f0) +
                           distance2 * (2 * f0 - 5 * f1 + 4 * f2 - f3) +
                           distance3 * (3 * (f1 - f2) + f3 - f0)));
}

/************************************************************************/
/*                       GWKCubicComputeWeights()                       */
/************************************************************************/

// adfCoeffs[2] = 1.0 - (adfCoeffs[0] + adfCoeffs[1] - adfCoeffs[3]);

template <typename T>
static inline void GWKCubicComputeWeights(T x, T coeffs[4])
{
    const T halfX = T(0.5) * x;
    const T threeX = T(3.0) * x;
    const T halfX2 = halfX * x;

    coeffs[0] = halfX * (-1 + x * (2 - x));
    coeffs[1] = 1 + halfX2 * (-5 + threeX);
    coeffs[2] = halfX * (1 + x * (4 - threeX));
    coeffs[3] = halfX2 * (-1 + x);
}

// TODO(schwehr): Use an inline function.
#define CONVOL4(v1, v2)                                                        \
    ((v1)[0] * (v2)[0] + (v1)[1] * (v2)[1] + (v1)[2] * (v2)[2] +               \
     (v1)[3] * (v2)[3])

#if 0
// Optimal (in theory...) for max 2 convolutions: 14 multiplications
// instead of 17.
// TODO(schwehr): Use an inline function.
#define GWKCubicComputeWeights_Optim2MAX(dfX_, adfCoeffs, dfHalfX)             \
    {                                                                          \
        const double dfX = dfX_;                                               \
        dfHalfX = 0.5 * dfX;                                                   \
        const double dfThreeX = 3.0 * dfX;                                     \
        const double dfXMinus1 = dfX - 1;                                      \
                                                                               \
        adfCoeffs[0] = -1 + dfX * (2 - dfX);                                   \
        adfCoeffs[1] = dfX * (-5 + dfThreeX);                                  \
        /*adfCoeffs[2] = 1 + dfX * (4 - dfThreeX);*/                           \
        adfCoeffs[2] = -dfXMinus1 - adfCoeffs[1];                              \
        /*adfCoeffs[3] = dfX * (-1 + dfX); */                                  \
        adfCoeffs[3] = dfXMinus1 - adfCoeffs[0];                               \
    }

// TODO(schwehr): Use an inline function.
#define CONVOL4_Optim2MAX(adfCoeffs, v, dfHalfX)                               \
    ((v)[1] + (dfHalfX) * ((adfCoeffs)[0] * (v)[0] + (adfCoeffs)[1] * (v)[1] + \
                           (adfCoeffs)[2] * (v)[2] + (adfCoeffs)[3] * (v)[3]))
#endif

static bool GWKCubicResample4Sample(const GDALWarpKernel *poWK, int iBand,
                                    double dfSrcX, double dfSrcY,
                                    double *pdfDensity, double *pdfReal,
                                    double *pdfImag)

{
    const int iSrcX = static_cast<int>(dfSrcX - 0.5);
    const int iSrcY = static_cast<int>(dfSrcY - 0.5);
    GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * poWK->nSrcXSize;
    const double dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double dfDeltaY = dfSrcY - 0.5 - iSrcY;
    double adfDensity[4] = {};
    double adfReal[4] = {};
    double adfImag[4] = {};

    // Get the bilinear interpolation at the image borders.
    if (iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize || iSrcY - 1 < 0 ||
        iSrcY + 2 >= poWK->nSrcYSize)
        return GWKBilinearResample4Sample(poWK, iBand, dfSrcX, dfSrcY,
                                          pdfDensity, pdfReal, pdfImag);

    double adfValueDens[4] = {};
    double adfValueReal[4] = {};
    double adfValueImag[4] = {};

    double adfCoeffsX[4] = {};
    GWKCubicComputeWeights(dfDeltaX, adfCoeffsX);

    for (GPtrDiff_t i = -1; i < 3; i++)
    {
        if (!GWKGetPixelRow(poWK, iBand, iSrcOffset + i * poWK->nSrcXSize - 1,
                            2, adfDensity, adfReal, adfImag) ||
            adfDensity[0] < SRC_DENSITY_THRESHOLD ||
            adfDensity[1] < SRC_DENSITY_THRESHOLD ||
            adfDensity[2] < SRC_DENSITY_THRESHOLD ||
            adfDensity[3] < SRC_DENSITY_THRESHOLD)
        {
            return GWKBilinearResample4Sample(poWK, iBand, dfSrcX, dfSrcY,
                                              pdfDensity, pdfReal, pdfImag);
        }

        adfValueDens[i + 1] = CONVOL4(adfCoeffsX, adfDensity);
        adfValueReal[i + 1] = CONVOL4(adfCoeffsX, adfReal);
        adfValueImag[i + 1] = CONVOL4(adfCoeffsX, adfImag);
    }

    /* -------------------------------------------------------------------- */
    /*      For now, if we have any pixels missing in the kernel area,      */
    /*      we fallback on using bilinear interpolation.  Ideally we        */
    /*      should do "weight adjustment" of our results similarly to       */
    /*      what is done for the cubic spline and lanc. interpolators.      */
    /* -------------------------------------------------------------------- */

    double adfCoeffsY[4] = {};
    GWKCubicComputeWeights(dfDeltaY, adfCoeffsY);

    *pdfDensity = CONVOL4(adfCoeffsY, adfValueDens);
    *pdfReal = CONVOL4(adfCoeffsY, adfValueReal);
    *pdfImag = CONVOL4(adfCoeffsY, adfValueImag);

    return true;
}

#if defined(__x86_64) || defined(_M_X64)

/************************************************************************/
/*                           XMMLoad4Values()                           */
/*                                                                      */
/*  Load 4 packed byte or uint16, cast them to float and put them in a  */
/*  m128 register.                                                      */
/************************************************************************/

static CPL_INLINE __m128 XMMLoad4Values(const GByte *ptr)
{
    unsigned int i;
    memcpy(&i, ptr, 4);
    __m128i xmm_i = _mm_cvtsi32_si128(i);
    // Zero extend 4 packed unsigned 8-bit integers in a to packed
    // 32-bit integers.
#if __SSE4_1__
    xmm_i = _mm_cvtepu8_epi32(xmm_i);
#else
    xmm_i = _mm_unpacklo_epi8(xmm_i, _mm_setzero_si128());
    xmm_i = _mm_unpacklo_epi16(xmm_i, _mm_setzero_si128());
#endif
    return _mm_cvtepi32_ps(xmm_i);
}

static CPL_INLINE __m128 XMMLoad4Values(const GUInt16 *ptr)
{
    GUInt64 i;
    memcpy(&i, ptr, 8);
    __m128i xmm_i = _mm_cvtsi64_si128(i);
    // Zero extend 4 packed unsigned 16-bit integers in a to packed
    // 32-bit integers.
#if __SSE4_1__
    xmm_i = _mm_cvtepu16_epi32(xmm_i);
#else
    xmm_i = _mm_unpacklo_epi16(xmm_i, _mm_setzero_si128());
#endif
    return _mm_cvtepi32_ps(xmm_i);
}

/************************************************************************/
/*                           XMMHorizontalAdd()                         */
/*                                                                      */
/*  Return the sum of the 4 floating points of the register.            */
/************************************************************************/

#if __SSE3__
static CPL_INLINE float XMMHorizontalAdd(__m128 v)
{
    __m128 shuf = _mm_movehdup_ps(v);   // (v3   , v3   , v1   , v1)
    __m128 sums = _mm_add_ps(v, shuf);  // (v3+v3, v3+v2, v1+v1, v1+v0)
    shuf = _mm_movehl_ps(shuf, sums);   // (v3   , v3   , v3+v3, v3+v2)
    sums = _mm_add_ss(sums, shuf);      // (v1+v0)+(v3+v2)
    return _mm_cvtss_f32(sums);
}
#else
static CPL_INLINE float XMMHorizontalAdd(__m128 v)
{
    __m128 shuf = _mm_movehl_ps(v, v);     // (v3   , v2   , v3   , v2)
    __m128 sums = _mm_add_ps(v, shuf);     // (v3+v3, v2+v2, v3+v1, v2+v0)
    shuf = _mm_shuffle_ps(sums, sums, 1);  // (v2+v0, v2+v0, v2+v0, v3+v1)
    sums = _mm_add_ss(sums, shuf);         // (v2+v0)+(v3+v1)
    return _mm_cvtss_f32(sums);
}
#endif

#endif  // (defined(__x86_64) || defined(_M_X64))

/************************************************************************/
/*            GWKCubicResampleSrcMaskIsDensity4SampleRealT()            */
/************************************************************************/

// Note: if USE_SSE_CUBIC_IMPL, only instantiate that for Byte and UInt16,
// because there are a few assumptions above those types.
// We do not define USE_SSE_CUBIC_IMPL since in practice, it gives zero
// perf benefit.

template <class T>
static CPL_INLINE bool GWKCubicResampleSrcMaskIsDensity4SampleRealT(
    const GDALWarpKernel *poWK, int iBand, double dfSrcX, double dfSrcY,
    double *pdfDensity, double *pdfReal)
{
    const int iSrcX = static_cast<int>(dfSrcX - 0.5);
    const int iSrcY = static_cast<int>(dfSrcY - 0.5);
    const GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * poWK->nSrcXSize;

    // Get the bilinear interpolation at the image borders.
    if (iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize || iSrcY - 1 < 0 ||
        iSrcY + 2 >= poWK->nSrcYSize)
    {
        double adfImagIgnored[4] = {};
        return GWKBilinearResample4Sample(poWK, iBand, dfSrcX, dfSrcY,
                                          pdfDensity, pdfReal, adfImagIgnored);
    }

#if defined(USE_SSE_CUBIC_IMPL) && (defined(__x86_64) || defined(_M_X64))
    const float fDeltaX = static_cast<float>(dfSrcX) - 0.5f - iSrcX;
    const float fDeltaY = static_cast<float>(dfSrcY) - 0.5f - iSrcY;

    // TODO(schwehr): Explain the magic numbers.
    float afTemp[4 + 4 + 4 + 1];
    float *pafAligned =
        reinterpret_cast<float *>(afTemp + ((size_t)afTemp & 0xf));
    float *pafCoeffs = pafAligned;
    float *pafDensity = pafAligned + 4;
    float *pafValue = pafAligned + 8;

    const float fHalfDeltaX = 0.5f * fDeltaX;
    const float fThreeDeltaX = 3.0f * fDeltaX;
    const float fHalfDeltaX2 = fHalfDeltaX * fDeltaX;

    pafCoeffs[0] = fHalfDeltaX * (-1 + fDeltaX * (2 - fDeltaX));
    pafCoeffs[1] = 1 + fHalfDeltaX2 * (-5 + fThreeDeltaX);
    pafCoeffs[2] = fHalfDeltaX * (1 + fDeltaX * (4 - fThreeDeltaX));
    pafCoeffs[3] = fHalfDeltaX2 * (-1 + fDeltaX);
    __m128 xmmCoeffs = _mm_load_ps(pafCoeffs);
    const __m128 xmmThreshold = _mm_load1_ps(&SRC_DENSITY_THRESHOLD);

    __m128 xmmMaskLowDensity = _mm_setzero_ps();
    for (GPtrDiff_t i = -1, iOffset = iSrcOffset - poWK->nSrcXSize - 1; i < 3;
         i++, iOffset += poWK->nSrcXSize)
    {
        const __m128 xmmDensity =
            _mm_loadu_ps(poWK->pafUnifiedSrcDensity + iOffset);
        xmmMaskLowDensity = _mm_or_ps(xmmMaskLowDensity,
                                      _mm_cmplt_ps(xmmDensity, xmmThreshold));
        pafDensity[i + 1] = XMMHorizontalAdd(_mm_mul_ps(xmmCoeffs, xmmDensity));

        const __m128 xmmValues =
            XMMLoad4Values(((T *)poWK->papabySrcImage[iBand]) + iOffset);
        pafValue[i + 1] = XMMHorizontalAdd(_mm_mul_ps(xmmCoeffs, xmmValues));
    }
    if (_mm_movemask_ps(xmmMaskLowDensity))
    {
        double adfImagIgnored[4] = {};
        return GWKBilinearResample4Sample(poWK, iBand, dfSrcX, dfSrcY,
                                          pdfDensity, pdfReal, adfImagIgnored);
    }

    const float fHalfDeltaY = 0.5f * fDeltaY;
    const float fThreeDeltaY = 3.0f * fDeltaY;
    const float fHalfDeltaY2 = fHalfDeltaY * fDeltaY;

    pafCoeffs[0] = fHalfDeltaY * (-1 + fDeltaY * (2 - fDeltaY));
    pafCoeffs[1] = 1 + fHalfDeltaY2 * (-5 + fThreeDeltaY);
    pafCoeffs[2] = fHalfDeltaY * (1 + fDeltaY * (4 - fThreeDeltaY));
    pafCoeffs[3] = fHalfDeltaY2 * (-1 + fDeltaY);

    xmmCoeffs = _mm_load_ps(pafCoeffs);

    const __m128 xmmDensity = _mm_load_ps(pafDensity);
    const __m128 xmmValue = _mm_load_ps(pafValue);
    *pdfDensity = XMMHorizontalAdd(_mm_mul_ps(xmmCoeffs, xmmDensity));
    *pdfReal = XMMHorizontalAdd(_mm_mul_ps(xmmCoeffs, xmmValue));

    // We did all above computations on float32 whereas the general case is
    // float64. Not sure if one is fundamentally more correct than the other
    // one, but we want our optimization to give the same result as the
    // general case as much as possible, so if the resulting value is
    // close to some_int_value + 0.5, redo the computation with the general
    // case.
    // Note: If other types than Byte or UInt16, will need changes.
    if (fabs(*pdfReal - static_cast<int>(*pdfReal) - 0.5) > .007)
        return true;

#endif  // defined(USE_SSE_CUBIC_IMPL) && (defined(__x86_64) || defined(_M_X64))

    const double dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double dfDeltaY = dfSrcY - 0.5 - iSrcY;

    double adfValueDens[4] = {};
    double adfValueReal[4] = {};

    double adfCoeffsX[4] = {};
    GWKCubicComputeWeights(dfDeltaX, adfCoeffsX);

    double adfCoeffsY[4] = {};
    GWKCubicComputeWeights(dfDeltaY, adfCoeffsY);

    for (GPtrDiff_t i = -1; i < 3; i++)
    {
        const GPtrDiff_t iOffset = iSrcOffset + i * poWK->nSrcXSize - 1;
#if !(defined(USE_SSE_CUBIC_IMPL) && (defined(__x86_64) || defined(_M_X64)))
        if (poWK->pafUnifiedSrcDensity[iOffset + 0] < SRC_DENSITY_THRESHOLD ||
            poWK->pafUnifiedSrcDensity[iOffset + 1] < SRC_DENSITY_THRESHOLD ||
            poWK->pafUnifiedSrcDensity[iOffset + 2] < SRC_DENSITY_THRESHOLD ||
            poWK->pafUnifiedSrcDensity[iOffset + 3] < SRC_DENSITY_THRESHOLD)
        {
            double adfImagIgnored[4] = {};
            return GWKBilinearResample4Sample(poWK, iBand, dfSrcX, dfSrcY,
                                              pdfDensity, pdfReal,
                                              adfImagIgnored);
        }
#endif

        adfValueDens[i + 1] =
            CONVOL4(adfCoeffsX, poWK->pafUnifiedSrcDensity + iOffset);

        adfValueReal[i + 1] = CONVOL4(
            adfCoeffsX,
            reinterpret_cast<T *>(poWK->papabySrcImage[iBand]) + iOffset);
    }

    *pdfDensity = CONVOL4(adfCoeffsY, adfValueDens);
    *pdfReal = CONVOL4(adfCoeffsY, adfValueReal);

    return true;
}

/************************************************************************/
/*              GWKCubicResampleSrcMaskIsDensity4SampleReal()             */
/*     Bi-cubic when source has and only has pafUnifiedSrcDensity.      */
/************************************************************************/

static bool GWKCubicResampleSrcMaskIsDensity4SampleReal(
    const GDALWarpKernel *poWK, int iBand, double dfSrcX, double dfSrcY,
    double *pdfDensity, double *pdfReal)

{
    const int iSrcX = static_cast<int>(dfSrcX - 0.5);
    const int iSrcY = static_cast<int>(dfSrcY - 0.5);
    const GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * poWK->nSrcXSize;
    const double dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double dfDeltaY = dfSrcY - 0.5 - iSrcY;

    // Get the bilinear interpolation at the image borders.
    if (iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize || iSrcY - 1 < 0 ||
        iSrcY + 2 >= poWK->nSrcYSize)
    {
        double adfImagIgnored[4] = {};
        return GWKBilinearResample4Sample(poWK, iBand, dfSrcX, dfSrcY,
                                          pdfDensity, pdfReal, adfImagIgnored);
    }

    double adfCoeffsX[4] = {};
    GWKCubicComputeWeights(dfDeltaX, adfCoeffsX);

    double adfCoeffsY[4] = {};
    GWKCubicComputeWeights(dfDeltaY, adfCoeffsY);

    double adfValueDens[4] = {};
    double adfValueReal[4] = {};
    double adfDensity[4] = {};
    double adfReal[4] = {};
    double adfImagIgnored[4] = {};

    for (GPtrDiff_t i = -1; i < 3; i++)
    {
        if (!GWKGetPixelRow(poWK, iBand, iSrcOffset + i * poWK->nSrcXSize - 1,
                            2, adfDensity, adfReal, adfImagIgnored) ||
            adfDensity[0] < SRC_DENSITY_THRESHOLD ||
            adfDensity[1] < SRC_DENSITY_THRESHOLD ||
            adfDensity[2] < SRC_DENSITY_THRESHOLD ||
            adfDensity[3] < SRC_DENSITY_THRESHOLD)
        {
            return GWKBilinearResample4Sample(poWK, iBand, dfSrcX, dfSrcY,
                                              pdfDensity, pdfReal,
                                              adfImagIgnored);
        }

        adfValueDens[i + 1] = CONVOL4(adfCoeffsX, adfDensity);
        adfValueReal[i + 1] = CONVOL4(adfCoeffsX, adfReal);
    }

    *pdfDensity = CONVOL4(adfCoeffsY, adfValueDens);
    *pdfReal = CONVOL4(adfCoeffsY, adfValueReal);

    return true;
}

template <class T>
static bool GWKCubicResampleNoMasks4SampleT(const GDALWarpKernel *poWK,
                                            int iBand, double dfSrcX,
                                            double dfSrcY, T *pValue)

{
    const int iSrcX = static_cast<int>(dfSrcX - 0.5);
    const int iSrcY = static_cast<int>(dfSrcY - 0.5);
    const GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * poWK->nSrcXSize;
    const double dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double dfDeltaY = dfSrcY - 0.5 - iSrcY;
    const double dfDeltaY2 = dfDeltaY * dfDeltaY;
    const double dfDeltaY3 = dfDeltaY2 * dfDeltaY;

    // Get the bilinear interpolation at the image borders.
    if (iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize || iSrcY - 1 < 0 ||
        iSrcY + 2 >= poWK->nSrcYSize)
        return GWKBilinearResampleNoMasks4SampleT(poWK, iBand, dfSrcX, dfSrcY,
                                                  pValue);

    double adfCoeffs[4] = {};
    GWKCubicComputeWeights(dfDeltaX, adfCoeffs);

    double adfValue[4] = {};

    for (GPtrDiff_t i = -1; i < 3; i++)
    {
        const GPtrDiff_t iOffset = iSrcOffset + i * poWK->nSrcXSize - 1;

        adfValue[i + 1] = CONVOL4(
            adfCoeffs,
            reinterpret_cast<T *>(poWK->papabySrcImage[iBand]) + iOffset);
    }

    const double dfValue =
        CubicConvolution(dfDeltaY, dfDeltaY2, dfDeltaY3, adfValue[0],
                         adfValue[1], adfValue[2], adfValue[3]);

    *pValue = GWKClampValueT<T>(dfValue);

    return true;
}

/************************************************************************/
/*                          GWKLanczosSinc()                            */
/************************************************************************/

/*
 * Lanczos windowed sinc interpolation kernel with radius r.
 *        /
 *        | sinc(x) * sinc(x/r), if |x| < r
 * L(x) = | 1, if x = 0                     ,
 *        | 0, otherwise
 *        \
 *
 * where sinc(x) = sin(PI * x) / (PI * x).
 */

static double GWKLanczosSinc(double dfX)
{
    if (dfX == 0.0)
        return 1.0;

    const double dfPIX = M_PI * dfX;
    const double dfPIXoverR = dfPIX / 3;
    const double dfPIX2overR = dfPIX * dfPIXoverR;
    // Given that sin(3x) = 3 sin(x) - 4 sin^3 (x)
    // we can compute sin(dfSinPIX) from sin(dfPIXoverR)
    const double dfSinPIXoverR = sin(dfPIXoverR);
    const double dfSinPIXoverRSquared = dfSinPIXoverR * dfSinPIXoverR;
    const double dfSinPIXMulSinPIXoverR =
        (3 - 4 * dfSinPIXoverRSquared) * dfSinPIXoverRSquared;
    return dfSinPIXMulSinPIXoverR / dfPIX2overR;
}

static double GWKLanczosSinc4Values(double *padfValues)
{
    for (int i = 0; i < 4; i++)
    {
        if (padfValues[i] == 0.0)
        {
            padfValues[i] = 1.0;
        }
        else
        {
            const double dfPIX = M_PI * padfValues[i];
            const double dfPIXoverR = dfPIX / 3;
            const double dfPIX2overR = dfPIX * dfPIXoverR;
            // Given that sin(3x) = 3 sin(x) - 4 sin^3 (x)
            // we can compute sin(dfSinPIX) from sin(dfPIXoverR)
            const double dfSinPIXoverR = sin(dfPIXoverR);
            const double dfSinPIXoverRSquared = dfSinPIXoverR * dfSinPIXoverR;
            const double dfSinPIXMulSinPIXoverR =
                (3 - 4 * dfSinPIXoverRSquared) * dfSinPIXoverRSquared;
            padfValues[i] = dfSinPIXMulSinPIXoverR / dfPIX2overR;
        }
    }
    return padfValues[0] + padfValues[1] + padfValues[2] + padfValues[3];
}

/************************************************************************/
/*                           GWKBilinear()                              */
/************************************************************************/

static double GWKBilinear(double dfX)
{
    double dfAbsX = fabs(dfX);
    if (dfAbsX <= 1.0)
        return 1 - dfAbsX;
    else
        return 0.0;
}

static double GWKBilinear4Values(double *padfValues)
{
    double dfAbsX0 = fabs(padfValues[0]);
    double dfAbsX1 = fabs(padfValues[1]);
    double dfAbsX2 = fabs(padfValues[2]);
    double dfAbsX3 = fabs(padfValues[3]);
    if (dfAbsX0 <= 1.0)
        padfValues[0] = 1 - dfAbsX0;
    else
        padfValues[0] = 0.0;
    if (dfAbsX1 <= 1.0)
        padfValues[1] = 1 - dfAbsX1;
    else
        padfValues[1] = 0.0;
    if (dfAbsX2 <= 1.0)
        padfValues[2] = 1 - dfAbsX2;
    else
        padfValues[2] = 0.0;
    if (dfAbsX3 <= 1.0)
        padfValues[3] = 1 - dfAbsX3;
    else
        padfValues[3] = 0.0;
    return padfValues[0] + padfValues[1] + padfValues[2] + padfValues[3];
}

/************************************************************************/
/*                            GWKCubic()                                */
/************************************************************************/

static double GWKCubic(double dfX)
{
    return CubicKernel(dfX);
}

static double GWKCubic4Values(double *padfValues)
{
    const double dfAbsX_0 = fabs(padfValues[0]);
    const double dfAbsX_1 = fabs(padfValues[1]);
    const double dfAbsX_2 = fabs(padfValues[2]);
    const double dfAbsX_3 = fabs(padfValues[3]);
    const double dfX2_0 = padfValues[0] * padfValues[0];
    const double dfX2_1 = padfValues[1] * padfValues[1];
    const double dfX2_2 = padfValues[2] * padfValues[2];
    const double dfX2_3 = padfValues[3] * padfValues[3];

    double dfVal0 = 0.0;
    if (dfAbsX_0 <= 1.0)
        dfVal0 = dfX2_0 * (1.5 * dfAbsX_0 - 2.5) + 1.0;
    else if (dfAbsX_0 <= 2.0)
        dfVal0 = dfX2_0 * (-0.5 * dfAbsX_0 + 2.5) - 4.0 * dfAbsX_0 + 2.0;

    double dfVal1 = 0.0;
    if (dfAbsX_1 <= 1.0)
        dfVal1 = dfX2_1 * (1.5 * dfAbsX_1 - 2.5) + 1.0;
    else if (dfAbsX_1 <= 2.0)
        dfVal1 = dfX2_1 * (-0.5 * dfAbsX_1 + 2.5) - 4.0 * dfAbsX_1 + 2.0;

    double dfVal2 = 0.0;
    if (dfAbsX_2 <= 1.0)
        dfVal2 = dfX2_2 * (1.5 * dfAbsX_2 - 2.5) + 1.0;
    else if (dfAbsX_2 <= 2.0)
        dfVal2 = dfX2_2 * (-0.5 * dfAbsX_2 + 2.5) - 4.0 * dfAbsX_2 + 2.0;

    double dfVal3 = 0.0;
    if (dfAbsX_3 <= 1.0)
        dfVal3 = dfX2_3 * (1.5 * dfAbsX_3 - 2.5) + 1.0;
    else if (dfAbsX_3 <= 2.0)
        dfVal3 = dfX2_3 * (-0.5 * dfAbsX_3 + 2.5) - 4.0 * dfAbsX_3 + 2.0;

    padfValues[0] = dfVal0;
    padfValues[1] = dfVal1;
    padfValues[2] = dfVal2;
    padfValues[3] = dfVal3;
    return dfVal0 + dfVal1 + dfVal2 + dfVal3;
}

/************************************************************************/
/*                           GWKBSpline()                               */
/************************************************************************/

// https://www.cs.utexas.edu/~fussell/courses/cs384g-fall2013/lectures/mitchell/Mitchell.pdf
// Equation 8 with (B,C)=(1,0)
// 1/6 * ( 3 * |x|^3 -  6 * |x|^2 + 4) |x| < 1
// 1/6 * ( -|x|^3 + 6 |x|^2  - 12|x| + 8) |x| >= 1 and |x| < 2

static double GWKBSpline(double x)
{
    const double xp2 = x + 2.0;
    const double xp1 = x + 1.0;
    const double xm1 = x - 1.0;

    // This will most likely be used, so we'll compute it ahead of time to
    // avoid stalling the processor.
    const double xp2c = xp2 * xp2 * xp2;

    // Note that the test is computed only if it is needed.
    // TODO(schwehr): Make this easier to follow.
    return xp2 > 0.0
               ? ((xp1 > 0.0)
                      ? ((x > 0.0)
                             ? ((xm1 > 0.0) ? -4.0 * xm1 * xm1 * xm1 : 0.0) +
                                   6.0 * x * x * x
                             : 0.0) +
                            -4.0 * xp1 * xp1 * xp1
                      : 0.0) +
                     xp2c
               : 0.0;  // * 0.166666666666666666666
}

static double GWKBSpline4Values(double *padfValues)
{
    for (int i = 0; i < 4; i++)
    {
        const double x = padfValues[i];
        const double xp2 = x + 2.0;
        const double xp1 = x + 1.0;
        const double xm1 = x - 1.0;

        // This will most likely be used, so we'll compute it ahead of time to
        // avoid stalling the processor.
        const double xp2c = xp2 * xp2 * xp2;

        // Note that the test is computed only if it is needed.
        // TODO(schwehr): Make this easier to follow.
        padfValues[i] =
            (xp2 > 0.0)
                ? ((xp1 > 0.0)
                       ? ((x > 0.0)
                              ? ((xm1 > 0.0) ? -4.0 * xm1 * xm1 * xm1 : 0.0) +
                                    6.0 * x * x * x
                              : 0.0) +
                             -4.0 * xp1 * xp1 * xp1
                       : 0.0) +
                      xp2c
                : 0.0;  // * 0.166666666666666666666
    }
    return padfValues[0] + padfValues[1] + padfValues[2] + padfValues[3];
}
/************************************************************************/
/*                       GWKResampleWrkStruct                           */
/************************************************************************/

typedef struct _GWKResampleWrkStruct GWKResampleWrkStruct;

typedef bool (*pfnGWKResampleType)(const GDALWarpKernel *poWK, int iBand,
                                   double dfSrcX, double dfSrcY,
                                   double *pdfDensity, double *pdfReal,
                                   double *pdfImag,
                                   GWKResampleWrkStruct *psWrkStruct);

struct _GWKResampleWrkStruct
{
    pfnGWKResampleType pfnGWKResample;

    // Space for saved X weights.
    double *padfWeightsX;
    bool *pabCalcX;

    double *padfWeightsY;       // Only used by GWKResampleOptimizedLanczos.
    int iLastSrcX;              // Only used by GWKResampleOptimizedLanczos.
    int iLastSrcY;              // Only used by GWKResampleOptimizedLanczos.
    double dfLastDeltaX;        // Only used by GWKResampleOptimizedLanczos.
    double dfLastDeltaY;        // Only used by GWKResampleOptimizedLanczos.
    double dfCosPiXScale;       // Only used by GWKResampleOptimizedLanczos.
    double dfSinPiXScale;       // Only used by GWKResampleOptimizedLanczos.
    double dfCosPiXScaleOver3;  // Only used by GWKResampleOptimizedLanczos.
    double dfSinPiXScaleOver3;  // Only used by GWKResampleOptimizedLanczos.
    double dfCosPiYScale;       // Only used by GWKResampleOptimizedLanczos.
    double dfSinPiYScale;       // Only used by GWKResampleOptimizedLanczos.
    double dfCosPiYScaleOver3;  // Only used by GWKResampleOptimizedLanczos.
    double dfSinPiYScaleOver3;  // Only used by GWKResampleOptimizedLanczos.

    // Space for saving a row of pixels.
    double *padfRowDensity;
    double *padfRowReal;
    double *padfRowImag;
};

/************************************************************************/
/*                    GWKResampleCreateWrkStruct()                      */
/************************************************************************/

static bool GWKResample(const GDALWarpKernel *poWK, int iBand, double dfSrcX,
                        double dfSrcY, double *pdfDensity, double *pdfReal,
                        double *pdfImag, GWKResampleWrkStruct *psWrkStruct);

static bool GWKResampleOptimizedLanczos(const GDALWarpKernel *poWK, int iBand,
                                        double dfSrcX, double dfSrcY,
                                        double *pdfDensity, double *pdfReal,
                                        double *pdfImag,
                                        GWKResampleWrkStruct *psWrkStruct);

static GWKResampleWrkStruct *GWKResampleCreateWrkStruct(GDALWarpKernel *poWK)
{
    const int nXDist = (poWK->nXRadius + 1) * 2;
    const int nYDist = (poWK->nYRadius + 1) * 2;

    GWKResampleWrkStruct *psWrkStruct = static_cast<GWKResampleWrkStruct *>(
        CPLCalloc(1, sizeof(GWKResampleWrkStruct)));

    // Alloc space for saved X weights.
    psWrkStruct->padfWeightsX =
        static_cast<double *>(CPLCalloc(nXDist, sizeof(double)));
    psWrkStruct->pabCalcX =
        static_cast<bool *>(CPLMalloc(nXDist * sizeof(bool)));

    psWrkStruct->padfWeightsY =
        static_cast<double *>(CPLCalloc(nYDist, sizeof(double)));
    psWrkStruct->iLastSrcX = -10;
    psWrkStruct->iLastSrcY = -10;
    psWrkStruct->dfLastDeltaX = -10;
    psWrkStruct->dfLastDeltaY = -10;

    // Alloc space for saving a row of pixels.
    if (poWK->pafUnifiedSrcDensity == nullptr &&
        poWK->panUnifiedSrcValid == nullptr &&
        poWK->papanBandSrcValid == nullptr)
    {
        psWrkStruct->padfRowDensity = nullptr;
    }
    else
    {
        psWrkStruct->padfRowDensity =
            static_cast<double *>(CPLCalloc(nXDist, sizeof(double)));
    }
    psWrkStruct->padfRowReal =
        static_cast<double *>(CPLCalloc(nXDist, sizeof(double)));
    psWrkStruct->padfRowImag =
        static_cast<double *>(CPLCalloc(nXDist, sizeof(double)));

    if (poWK->eResample == GRA_Lanczos)
    {
        psWrkStruct->pfnGWKResample = GWKResampleOptimizedLanczos;

        if (poWK->dfXScale < 1)
        {
            psWrkStruct->dfCosPiXScaleOver3 = cos(M_PI / 3 * poWK->dfXScale);
            psWrkStruct->dfSinPiXScaleOver3 =
                sqrt(1 - psWrkStruct->dfCosPiXScaleOver3 *
                             psWrkStruct->dfCosPiXScaleOver3);
            // "Naive":
            // const double dfCosPiXScale = cos(  M_PI * dfXScale );
            // const double dfSinPiXScale = sin(  M_PI * dfXScale );
            // but given that cos(3x) = 4 cos^3(x) - 3 cos(x) and x between 0 and M_PI
            psWrkStruct->dfCosPiXScale = (4 * psWrkStruct->dfCosPiXScaleOver3 *
                                              psWrkStruct->dfCosPiXScaleOver3 -
                                          3) *
                                         psWrkStruct->dfCosPiXScaleOver3;
            psWrkStruct->dfSinPiXScale = sqrt(
                1 - psWrkStruct->dfCosPiXScale * psWrkStruct->dfCosPiXScale);
        }

        if (poWK->dfYScale < 1)
        {
            psWrkStruct->dfCosPiYScaleOver3 = cos(M_PI / 3 * poWK->dfYScale);
            psWrkStruct->dfSinPiYScaleOver3 =
                sqrt(1 - psWrkStruct->dfCosPiYScaleOver3 *
                             psWrkStruct->dfCosPiYScaleOver3);
            // "Naive":
            // const double dfCosPiYScale = cos(  M_PI * dfYScale );
            // const double dfSinPiYScale = sin(  M_PI * dfYScale );
            // but given that cos(3x) = 4 cos^3(x) - 3 cos(x) and x between 0 and M_PI
            psWrkStruct->dfCosPiYScale = (4 * psWrkStruct->dfCosPiYScaleOver3 *
                                              psWrkStruct->dfCosPiYScaleOver3 -
                                          3) *
                                         psWrkStruct->dfCosPiYScaleOver3;
            psWrkStruct->dfSinPiYScale = sqrt(
                1 - psWrkStruct->dfCosPiYScale * psWrkStruct->dfCosPiYScale);
        }
    }
    else
        psWrkStruct->pfnGWKResample = GWKResample;

    return psWrkStruct;
}

/************************************************************************/
/*                    GWKResampleDeleteWrkStruct()                      */
/************************************************************************/

static void GWKResampleDeleteWrkStruct(GWKResampleWrkStruct *psWrkStruct)
{
    CPLFree(psWrkStruct->padfWeightsX);
    CPLFree(psWrkStruct->padfWeightsY);
    CPLFree(psWrkStruct->pabCalcX);
    CPLFree(psWrkStruct->padfRowDensity);
    CPLFree(psWrkStruct->padfRowReal);
    CPLFree(psWrkStruct->padfRowImag);
    CPLFree(psWrkStruct);
}

/************************************************************************/
/*                           GWKResample()                              */
/************************************************************************/

static bool GWKResample(const GDALWarpKernel *poWK, int iBand, double dfSrcX,
                        double dfSrcY, double *pdfDensity, double *pdfReal,
                        double *pdfImag, GWKResampleWrkStruct *psWrkStruct)

{
    // Save as local variables to avoid following pointers in loops.
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    double dfAccumulatorReal = 0.0;
    double dfAccumulatorImag = 0.0;
    double dfAccumulatorDensity = 0.0;
    double dfAccumulatorWeight = 0.0;
    const int iSrcX = static_cast<int>(floor(dfSrcX - 0.5));
    const int iSrcY = static_cast<int>(floor(dfSrcY - 0.5));
    const GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
    const double dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double dfDeltaY = dfSrcY - 0.5 - iSrcY;

    const double dfXScale = poWK->dfXScale;
    const double dfYScale = poWK->dfYScale;

    const int nXDist = (poWK->nXRadius + 1) * 2;

    // Space for saved X weights.
    double *padfWeightsX = psWrkStruct->padfWeightsX;
    bool *pabCalcX = psWrkStruct->pabCalcX;

    // Space for saving a row of pixels.
    double *padfRowDensity = psWrkStruct->padfRowDensity;
    double *padfRowReal = psWrkStruct->padfRowReal;
    double *padfRowImag = psWrkStruct->padfRowImag;

    // Mark as needing calculation (don't calculate the weights yet,
    // because a mask may render it unnecessary).
    memset(pabCalcX, false, nXDist * sizeof(bool));

    FilterFuncType pfnGetWeight = apfGWKFilter[poWK->eResample];
    CPLAssert(pfnGetWeight);

    // Skip sampling over edge of image.
    int j = poWK->nFiltInitY;
    int jMax = poWK->nYRadius;
    if (iSrcY + j < 0)
        j = -iSrcY;
    if (iSrcY + jMax >= nSrcYSize)
        jMax = nSrcYSize - iSrcY - 1;

    int iMin = poWK->nFiltInitX;
    int iMax = poWK->nXRadius;
    if (iSrcX + iMin < 0)
        iMin = -iSrcX;
    if (iSrcX + iMax >= nSrcXSize)
        iMax = nSrcXSize - iSrcX - 1;

    const int bXScaleBelow1 = (dfXScale < 1.0);
    const int bYScaleBelow1 = (dfYScale < 1.0);

    GPtrDiff_t iRowOffset =
        iSrcOffset + static_cast<GPtrDiff_t>(j - 1) * nSrcXSize + iMin;

    // Loop over pixel rows in the kernel.
    for (; j <= jMax; ++j)
    {
        iRowOffset += nSrcXSize;

        // Get pixel values.
        // We can potentially read extra elements after the "normal" end of the
        // source arrays, but the contract of papabySrcImage[iBand],
        // papanBandSrcValid[iBand], panUnifiedSrcValid and pafUnifiedSrcDensity
        // is to have WARP_EXTRA_ELTS reserved at their end.
        if (!GWKGetPixelRow(poWK, iBand, iRowOffset, (iMax - iMin + 2) / 2,
                            padfRowDensity, padfRowReal, padfRowImag))
            continue;

        // Calculate the Y weight.
        double dfWeight1 = (bYScaleBelow1)
                               ? pfnGetWeight((j - dfDeltaY) * dfYScale)
                               : pfnGetWeight(j - dfDeltaY);

        // Iterate over pixels in row.
        double dfAccumulatorRealLocal = 0.0;
        double dfAccumulatorImagLocal = 0.0;
        double dfAccumulatorDensityLocal = 0.0;
        double dfAccumulatorWeightLocal = 0.0;

        for (int i = iMin; i <= iMax; ++i)
        {
            // Skip sampling if pixel has zero density.
            if (padfRowDensity != nullptr &&
                padfRowDensity[i - iMin] < SRC_DENSITY_THRESHOLD)
                continue;

            double dfWeight2 = 0.0;

            // Make or use a cached set of weights for this row.
            if (pabCalcX[i - iMin])
            {
                // Use saved weight value instead of recomputing it.
                dfWeight2 = padfWeightsX[i - iMin];
            }
            else
            {
                // Calculate & save the X weight.
                padfWeightsX[i - iMin] = dfWeight2 =
                    (bXScaleBelow1) ? pfnGetWeight((i - dfDeltaX) * dfXScale)
                                    : pfnGetWeight(i - dfDeltaX);

                pabCalcX[i - iMin] = true;
            }

            // Accumulate!
            dfAccumulatorRealLocal += padfRowReal[i - iMin] * dfWeight2;
            dfAccumulatorImagLocal += padfRowImag[i - iMin] * dfWeight2;
            if (padfRowDensity != nullptr)
                dfAccumulatorDensityLocal +=
                    padfRowDensity[i - iMin] * dfWeight2;
            dfAccumulatorWeightLocal += dfWeight2;
        }

        dfAccumulatorReal += dfAccumulatorRealLocal * dfWeight1;
        dfAccumulatorImag += dfAccumulatorImagLocal * dfWeight1;
        dfAccumulatorDensity += dfAccumulatorDensityLocal * dfWeight1;
        dfAccumulatorWeight += dfAccumulatorWeightLocal * dfWeight1;
    }

    if (dfAccumulatorWeight < 0.000001 ||
        (padfRowDensity != nullptr && dfAccumulatorDensity < 0.000001))
    {
        *pdfDensity = 0.0;
        return false;
    }

    // Calculate the output taking into account weighting.
    if (dfAccumulatorWeight < 0.99999 || dfAccumulatorWeight > 1.00001)
    {
        *pdfReal = dfAccumulatorReal / dfAccumulatorWeight;
        *pdfImag = dfAccumulatorImag / dfAccumulatorWeight;
        if (padfRowDensity != nullptr)
            *pdfDensity = dfAccumulatorDensity / dfAccumulatorWeight;
        else
            *pdfDensity = 1.0;
    }
    else
    {
        *pdfReal = dfAccumulatorReal;
        *pdfImag = dfAccumulatorImag;
        if (padfRowDensity != nullptr)
            *pdfDensity = dfAccumulatorDensity;
        else
            *pdfDensity = 1.0;
    }

    return true;
}

/************************************************************************/
/*                      GWKResampleOptimizedLanczos()                   */
/************************************************************************/

static bool GWKResampleOptimizedLanczos(const GDALWarpKernel *poWK, int iBand,
                                        double dfSrcX, double dfSrcY,
                                        double *pdfDensity, double *pdfReal,
                                        double *pdfImag,
                                        GWKResampleWrkStruct *psWrkStruct)

{
    // Save as local variables to avoid following pointers in loops.
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    double dfAccumulatorReal = 0.0;
    double dfAccumulatorImag = 0.0;
    double dfAccumulatorDensity = 0.0;
    double dfAccumulatorWeight = 0.0;
    const int iSrcX = static_cast<int>(floor(dfSrcX - 0.5));
    const int iSrcY = static_cast<int>(floor(dfSrcY - 0.5));
    const GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
    const double dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double dfDeltaY = dfSrcY - 0.5 - iSrcY;

    const double dfXScale = poWK->dfXScale;
    const double dfYScale = poWK->dfYScale;

    // Space for saved X weights.
    double *const padfWeightsXShifted =
        psWrkStruct->padfWeightsX - poWK->nFiltInitX;
    double *const padfWeightsYShifted =
        psWrkStruct->padfWeightsY - poWK->nFiltInitY;

    // Space for saving a row of pixels.
    double *const padfRowDensity = psWrkStruct->padfRowDensity;
    double *const padfRowReal = psWrkStruct->padfRowReal;
    double *const padfRowImag = psWrkStruct->padfRowImag;

    // Skip sampling over edge of image.
    int jMin = poWK->nFiltInitY;
    int jMax = poWK->nYRadius;
    if (iSrcY + jMin < 0)
        jMin = -iSrcY;
    if (iSrcY + jMax >= nSrcYSize)
        jMax = nSrcYSize - iSrcY - 1;

    int iMin = poWK->nFiltInitX;
    int iMax = poWK->nXRadius;
    if (iSrcX + iMin < 0)
        iMin = -iSrcX;
    if (iSrcX + iMax >= nSrcXSize)
        iMax = nSrcXSize - iSrcX - 1;

    if (dfXScale < 1.0)
    {
        while ((iMin - dfDeltaX) * dfXScale < -3.0)
            iMin++;
        while ((iMax - dfDeltaX) * dfXScale > 3.0)
            iMax--;

        // clang-format off
        /*
        Naive version:
        for (int i = iMin; i <= iMax; ++i)
        {
            psWrkStruct->padfWeightsXShifted[i] =
                GWKLanczosSinc((i - dfDeltaX) * dfXScale);
        }

        but given that:

        GWKLanczosSinc(x):
            if (dfX == 0.0)
                return 1.0;

            const double dfPIX = M_PI * dfX;
            const double dfPIXoverR = dfPIX / 3;
            const double dfPIX2overR = dfPIX * dfPIXoverR;
            return sin(dfPIX) * sin(dfPIXoverR) / dfPIX2overR;

        and
            sin (a + b) = sin a cos b + cos a sin b.
            cos (a + b) = cos a cos b - sin a sin b.

        we can skip any sin() computation within the loop
        */
        // clang-format on

        if (iSrcX != psWrkStruct->iLastSrcX ||
            dfDeltaX != psWrkStruct->dfLastDeltaX)
        {
            double dfX = (iMin - dfDeltaX) * dfXScale;

            double dfPIXover3 = M_PI / 3 * dfX;
            double dfCosOver3 = cos(dfPIXover3);
            double dfSinOver3 = sin(dfPIXover3);

            // "Naive":
            // double dfSin = sin( M_PI * dfX );
            // double dfCos = cos( M_PI * dfX );
            // but given that cos(3x) = 4 cos^3(x) - 3 cos(x) and sin(3x) = 3 sin(x) - 4 sin^3 (x).
            double dfSin = (3 - 4 * dfSinOver3 * dfSinOver3) * dfSinOver3;
            double dfCos = (4 * dfCosOver3 * dfCosOver3 - 3) * dfCosOver3;

            const double dfCosPiXScaleOver3 = psWrkStruct->dfCosPiXScaleOver3;
            const double dfSinPiXScaleOver3 = psWrkStruct->dfSinPiXScaleOver3;
            const double dfCosPiXScale = psWrkStruct->dfCosPiXScale;
            const double dfSinPiXScale = psWrkStruct->dfSinPiXScale;
            constexpr double THREE_PI_PI = 3 * M_PI * M_PI;
            padfWeightsXShifted[iMin] =
                dfX == 0 ? 1.0 : THREE_PI_PI * dfSin * dfSinOver3 / (dfX * dfX);
            for (int i = iMin + 1; i <= iMax; ++i)
            {
                dfX += dfXScale;
                const double dfNewSin =
                    dfSin * dfCosPiXScale + dfCos * dfSinPiXScale;
                const double dfNewSinOver3 = dfSinOver3 * dfCosPiXScaleOver3 +
                                             dfCosOver3 * dfSinPiXScaleOver3;
                padfWeightsXShifted[i] =
                    dfX == 0
                        ? 1.0
                        : THREE_PI_PI * dfNewSin * dfNewSinOver3 / (dfX * dfX);
                const double dfNewCos =
                    dfCos * dfCosPiXScale - dfSin * dfSinPiXScale;
                const double dfNewCosOver3 = dfCosOver3 * dfCosPiXScaleOver3 -
                                             dfSinOver3 * dfSinPiXScaleOver3;
                dfSin = dfNewSin;
                dfCos = dfNewCos;
                dfSinOver3 = dfNewSinOver3;
                dfCosOver3 = dfNewCosOver3;
            }

            psWrkStruct->iLastSrcX = iSrcX;
            psWrkStruct->dfLastDeltaX = dfDeltaX;
        }
    }
    else
    {
        while (iMin - dfDeltaX < -3.0)
            iMin++;
        while (iMax - dfDeltaX > 3.0)
            iMax--;

        if (iSrcX != psWrkStruct->iLastSrcX ||
            dfDeltaX != psWrkStruct->dfLastDeltaX)
        {
            // Optimisation of GWKLanczosSinc(i - dfDeltaX) based on the
            // following trigonometric formulas.

            // TODO(schwehr): Move this somewhere where it can be rendered at
            // LaTeX.
            // clang-format off
            // sin(M_PI * (dfBase + k)) = sin(M_PI * dfBase) * cos(M_PI * k) +
            //                            cos(M_PI * dfBase) * sin(M_PI * k)
            // sin(M_PI * (dfBase + k)) = dfSinPIBase * cos(M_PI * k) + dfCosPIBase * sin(M_PI * k)
            // sin(M_PI * (dfBase + k)) = dfSinPIBase * cos(M_PI * k)
            // sin(M_PI * (dfBase + k)) = dfSinPIBase * (((k % 2) == 0) ? 1 : -1)

            // sin(M_PI / dfR * (dfBase + k)) = sin(M_PI / dfR * dfBase) * cos(M_PI / dfR * k) +
            //                                  cos(M_PI / dfR * dfBase) * sin(M_PI / dfR * k)
            // sin(M_PI / dfR * (dfBase + k)) = dfSinPIBaseOverR * cos(M_PI / dfR * k) + dfCosPIBaseOverR * sin(M_PI / dfR * k)
            // clang-format on

            const double dfSinPIDeltaXOver3 = sin((-M_PI / 3.0) * dfDeltaX);
            const double dfSin2PIDeltaXOver3 =
                dfSinPIDeltaXOver3 * dfSinPIDeltaXOver3;
            // Ok to use sqrt(1-sin^2) since M_PI / 3 * dfDeltaX < PI/2.
            const double dfCosPIDeltaXOver3 = sqrt(1.0 - dfSin2PIDeltaXOver3);
            const double dfSinPIDeltaX =
                (3.0 - 4 * dfSin2PIDeltaXOver3) * dfSinPIDeltaXOver3;
            const double dfInvPI2Over3 = 3.0 / (M_PI * M_PI);
            const double dfInvPI2Over3xSinPIDeltaX =
                dfInvPI2Over3 * dfSinPIDeltaX;
            const double dfInvPI2Over3xSinPIDeltaXxm0d5SinPIDeltaXOver3 =
                -0.5 * dfInvPI2Over3xSinPIDeltaX * dfSinPIDeltaXOver3;
            const double dfSinPIOver3 = 0.8660254037844386;
            const double dfInvPI2Over3xSinPIDeltaXxSinPIOver3xCosPIDeltaXOver3 =
                dfSinPIOver3 * dfInvPI2Over3xSinPIDeltaX * dfCosPIDeltaXOver3;
            const double padfCst[] = {
                dfInvPI2Over3xSinPIDeltaX * dfSinPIDeltaXOver3,
                dfInvPI2Over3xSinPIDeltaXxm0d5SinPIDeltaXOver3 -
                    dfInvPI2Over3xSinPIDeltaXxSinPIOver3xCosPIDeltaXOver3,
                dfInvPI2Over3xSinPIDeltaXxm0d5SinPIDeltaXOver3 +
                    dfInvPI2Over3xSinPIDeltaXxSinPIOver3xCosPIDeltaXOver3};

            for (int i = iMin; i <= iMax; ++i)
            {
                const double dfX = i - dfDeltaX;
                if (dfX == 0.0)
                    padfWeightsXShifted[i] = 1.0;
                else
                    padfWeightsXShifted[i] = padfCst[(i + 3) % 3] / (dfX * dfX);
#if DEBUG_VERBOSE
                    // TODO(schwehr): AlmostEqual.
                    // CPLAssert(fabs(padfWeightsX[i-poWK->nFiltInitX] -
                    //               GWKLanczosSinc(dfX, 3.0)) < 1e-10);
#endif
            }

            psWrkStruct->iLastSrcX = iSrcX;
            psWrkStruct->dfLastDeltaX = dfDeltaX;
        }
    }

    if (dfYScale < 1.0)
    {
        while ((jMin - dfDeltaY) * dfYScale < -3.0)
            jMin++;
        while ((jMax - dfDeltaY) * dfYScale > 3.0)
            jMax--;

        // clang-format off
        /*
        Naive version:
        for (int j = jMin; j <= jMax; ++j)
        {
            padfWeightsYShifted[j] =
                GWKLanczosSinc((j - dfDeltaY) * dfYScale);
        }
        */
        // clang-format on

        if (iSrcY != psWrkStruct->iLastSrcY ||
            dfDeltaY != psWrkStruct->dfLastDeltaY)
        {
            double dfY = (jMin - dfDeltaY) * dfYScale;

            double dfPIYover3 = M_PI / 3 * dfY;
            double dfCosOver3 = cos(dfPIYover3);
            double dfSinOver3 = sin(dfPIYover3);

            // "Naive":
            // double dfSin = sin( M_PI * dfY );
            // double dfCos = cos( M_PI * dfY );
            // but given that cos(3x) = 4 cos^3(x) - 3 cos(x) and sin(3x) = 3 sin(x) - 4 sin^3 (x).
            double dfSin = (3 - 4 * dfSinOver3 * dfSinOver3) * dfSinOver3;
            double dfCos = (4 * dfCosOver3 * dfCosOver3 - 3) * dfCosOver3;

            const double dfCosPiYScaleOver3 = psWrkStruct->dfCosPiYScaleOver3;
            const double dfSinPiYScaleOver3 = psWrkStruct->dfSinPiYScaleOver3;
            const double dfCosPiYScale = psWrkStruct->dfCosPiYScale;
            const double dfSinPiYScale = psWrkStruct->dfSinPiYScale;
            constexpr double THREE_PI_PI = 3 * M_PI * M_PI;
            padfWeightsYShifted[jMin] =
                dfY == 0 ? 1.0 : THREE_PI_PI * dfSin * dfSinOver3 / (dfY * dfY);
            for (int j = jMin + 1; j <= jMax; ++j)
            {
                dfY += dfYScale;
                const double dfNewSin =
                    dfSin * dfCosPiYScale + dfCos * dfSinPiYScale;
                const double dfNewSinOver3 = dfSinOver3 * dfCosPiYScaleOver3 +
                                             dfCosOver3 * dfSinPiYScaleOver3;
                padfWeightsYShifted[j] =
                    dfY == 0
                        ? 1.0
                        : THREE_PI_PI * dfNewSin * dfNewSinOver3 / (dfY * dfY);
                const double dfNewCos =
                    dfCos * dfCosPiYScale - dfSin * dfSinPiYScale;
                const double dfNewCosOver3 = dfCosOver3 * dfCosPiYScaleOver3 -
                                             dfSinOver3 * dfSinPiYScaleOver3;
                dfSin = dfNewSin;
                dfCos = dfNewCos;
                dfSinOver3 = dfNewSinOver3;
                dfCosOver3 = dfNewCosOver3;
            }

            psWrkStruct->iLastSrcY = iSrcY;
            psWrkStruct->dfLastDeltaY = dfDeltaY;
        }
    }
    else
    {
        while (jMin - dfDeltaY < -3.0)
            jMin++;
        while (jMax - dfDeltaY > 3.0)
            jMax--;

        if (iSrcY != psWrkStruct->iLastSrcY ||
            dfDeltaY != psWrkStruct->dfLastDeltaY)
        {
            const double dfSinPIDeltaYOver3 = sin((-M_PI / 3.0) * dfDeltaY);
            const double dfSin2PIDeltaYOver3 =
                dfSinPIDeltaYOver3 * dfSinPIDeltaYOver3;
            // Ok to use sqrt(1-sin^2) since M_PI / 3 * dfDeltaY < PI/2.
            const double dfCosPIDeltaYOver3 = sqrt(1.0 - dfSin2PIDeltaYOver3);
            const double dfSinPIDeltaY =
                (3.0 - 4.0 * dfSin2PIDeltaYOver3) * dfSinPIDeltaYOver3;
            const double dfInvPI2Over3 = 3.0 / (M_PI * M_PI);
            const double dfInvPI2Over3xSinPIDeltaY =
                dfInvPI2Over3 * dfSinPIDeltaY;
            const double dfInvPI2Over3xSinPIDeltaYxm0d5SinPIDeltaYOver3 =
                -0.5 * dfInvPI2Over3xSinPIDeltaY * dfSinPIDeltaYOver3;
            const double dfSinPIOver3 = 0.8660254037844386;
            const double dfInvPI2Over3xSinPIDeltaYxSinPIOver3xCosPIDeltaYOver3 =
                dfSinPIOver3 * dfInvPI2Over3xSinPIDeltaY * dfCosPIDeltaYOver3;
            const double padfCst[] = {
                dfInvPI2Over3xSinPIDeltaY * dfSinPIDeltaYOver3,
                dfInvPI2Over3xSinPIDeltaYxm0d5SinPIDeltaYOver3 -
                    dfInvPI2Over3xSinPIDeltaYxSinPIOver3xCosPIDeltaYOver3,
                dfInvPI2Over3xSinPIDeltaYxm0d5SinPIDeltaYOver3 +
                    dfInvPI2Over3xSinPIDeltaYxSinPIOver3xCosPIDeltaYOver3};

            for (int j = jMin; j <= jMax; ++j)
            {
                const double dfY = j - dfDeltaY;
                if (dfY == 0.0)
                    padfWeightsYShifted[j] = 1.0;
                else
                    padfWeightsYShifted[j] = padfCst[(j + 3) % 3] / (dfY * dfY);
#if DEBUG_VERBOSE
                    // TODO(schwehr): AlmostEqual.
                    // CPLAssert(fabs(padfWeightsYShifted[j] -
                    //               GWKLanczosSinc(dfY, 3.0)) < 1e-10);
#endif
            }

            psWrkStruct->iLastSrcY = iSrcY;
            psWrkStruct->dfLastDeltaY = dfDeltaY;
        }
    }

    // If we have no density information, we can simply compute the
    // accumulated weight.
    if (padfRowDensity == nullptr)
    {
        double dfRowAccWeight = 0.0;
        for (int i = iMin; i <= iMax; ++i)
        {
            dfRowAccWeight += padfWeightsXShifted[i];
        }
        double dfColAccWeight = 0.0;
        for (int j = jMin; j <= jMax; ++j)
        {
            dfColAccWeight += padfWeightsYShifted[j];
        }
        dfAccumulatorWeight = dfRowAccWeight * dfColAccWeight;
    }

    // Loop over pixel rows in the kernel.

    if (poWK->eWorkingDataType == GDT_Byte && !poWK->panUnifiedSrcValid &&
        !poWK->papanBandSrcValid && !poWK->pafUnifiedSrcDensity &&
        !padfRowDensity)
    {
        // Optimization for Byte case without any masking/alpha

        if (dfAccumulatorWeight < 0.000001)
        {
            *pdfDensity = 0.0;
            return false;
        }

        const GByte *pSrc =
            reinterpret_cast<const GByte *>(poWK->papabySrcImage[iBand]);
        pSrc += iSrcOffset + static_cast<GPtrDiff_t>(jMin) * nSrcXSize;

#if defined(__x86_64) || defined(_M_X64)
        if (iMax - iMin + 1 == 6)
        {
            // This is just an optimized version of the general case in
            // the else clause.

            pSrc += iMin;
            int j = jMin;
            const auto fourXWeights =
                XMMReg4Double::Load4Val(padfWeightsXShifted + iMin);

            // Process 2 lines at the same time.
            for (; j < jMax; j += 2)
            {
                const XMMReg4Double v_acc =
                    XMMReg4Double::Load4Val(pSrc) * fourXWeights;
                const XMMReg4Double v_acc2 =
                    XMMReg4Double::Load4Val(pSrc + nSrcXSize) * fourXWeights;
                const double dfRowAcc = v_acc.GetHorizSum();
                const double dfRowAccEnd =
                    pSrc[4] * padfWeightsXShifted[iMin + 4] +
                    pSrc[5] * padfWeightsXShifted[iMin + 5];
                dfAccumulatorReal +=
                    (dfRowAcc + dfRowAccEnd) * padfWeightsYShifted[j];
                const double dfRowAcc2 = v_acc2.GetHorizSum();
                const double dfRowAcc2End =
                    pSrc[nSrcXSize + 4] * padfWeightsXShifted[iMin + 4] +
                    pSrc[nSrcXSize + 5] * padfWeightsXShifted[iMin + 5];
                dfAccumulatorReal +=
                    (dfRowAcc2 + dfRowAcc2End) * padfWeightsYShifted[j + 1];
                pSrc += 2 * nSrcXSize;
            }
            if (j == jMax)
            {
                // Process last line if there's an odd number of them.

                const XMMReg4Double v_acc =
                    XMMReg4Double::Load4Val(pSrc) * fourXWeights;
                const double dfRowAcc = v_acc.GetHorizSum();
                const double dfRowAccEnd =
                    pSrc[4] * padfWeightsXShifted[iMin + 4] +
                    pSrc[5] * padfWeightsXShifted[iMin + 5];
                dfAccumulatorReal +=
                    (dfRowAcc + dfRowAccEnd) * padfWeightsYShifted[j];
            }
        }
        else
#endif
        {
            for (int j = jMin; j <= jMax; ++j)
            {
                int i = iMin;
                double dfRowAcc1 = 0.0;
                double dfRowAcc2 = 0.0;
                // A bit of loop unrolling
                for (; i < iMax; i += 2)
                {
                    dfRowAcc1 += pSrc[i] * padfWeightsXShifted[i];
                    dfRowAcc2 += pSrc[i + 1] * padfWeightsXShifted[i + 1];
                }
                if (i == iMax)
                {
                    // Process last column if there's an odd number of them.
                    dfRowAcc1 += pSrc[i] * padfWeightsXShifted[i];
                }

                dfAccumulatorReal +=
                    (dfRowAcc1 + dfRowAcc2) * padfWeightsYShifted[j];
                pSrc += nSrcXSize;
            }
        }

        // Calculate the output taking into account weighting.
        if (dfAccumulatorWeight < 0.99999 || dfAccumulatorWeight > 1.00001)
        {
            const double dfInvAcc = 1.0 / dfAccumulatorWeight;
            *pdfReal = dfAccumulatorReal * dfInvAcc;
            *pdfDensity = 1.0;
        }
        else
        {
            *pdfReal = dfAccumulatorReal;
            *pdfDensity = 1.0;
        }

        return true;
    }

    GPtrDiff_t iRowOffset =
        iSrcOffset + static_cast<GPtrDiff_t>(jMin - 1) * nSrcXSize + iMin;

    int nCountValid = 0;
    const bool bIsNonComplex = !GDALDataTypeIsComplex(poWK->eWorkingDataType);

    for (int j = jMin; j <= jMax; ++j)
    {
        iRowOffset += nSrcXSize;

        // Get pixel values.
        // We can potentially read extra elements after the "normal" end of the
        // source arrays, but the contract of papabySrcImage[iBand],
        // papanBandSrcValid[iBand], panUnifiedSrcValid and pafUnifiedSrcDensity
        // is to have WARP_EXTRA_ELTS reserved at their end.
        if (!GWKGetPixelRow(poWK, iBand, iRowOffset, (iMax - iMin + 2) / 2,
                            padfRowDensity, padfRowReal, padfRowImag))
            continue;

        const double dfWeight1 = padfWeightsYShifted[j];

        // Iterate over pixels in row.
        if (padfRowDensity != nullptr)
        {
            for (int i = iMin; i <= iMax; ++i)
            {
                // Skip sampling if pixel has zero density.
                if (padfRowDensity[i - iMin] < SRC_DENSITY_THRESHOLD)
                    continue;

                nCountValid++;

                //  Use a cached set of weights for this row.
                const double dfWeight2 = dfWeight1 * padfWeightsXShifted[i];

                // Accumulate!
                dfAccumulatorReal += padfRowReal[i - iMin] * dfWeight2;
                dfAccumulatorImag += padfRowImag[i - iMin] * dfWeight2;
                dfAccumulatorDensity += padfRowDensity[i - iMin] * dfWeight2;
                dfAccumulatorWeight += dfWeight2;
            }
        }
        else if (bIsNonComplex)
        {
            double dfRowAccReal = 0.0;
            for (int i = iMin; i <= iMax; ++i)
            {
                const double dfWeight2 = padfWeightsXShifted[i];

                // Accumulate!
                dfRowAccReal += padfRowReal[i - iMin] * dfWeight2;
            }

            dfAccumulatorReal += dfRowAccReal * dfWeight1;
        }
        else
        {
            double dfRowAccReal = 0.0;
            double dfRowAccImag = 0.0;
            for (int i = iMin; i <= iMax; ++i)
            {
                const double dfWeight2 = padfWeightsXShifted[i];

                // Accumulate!
                dfRowAccReal += padfRowReal[i - iMin] * dfWeight2;
                dfRowAccImag += padfRowImag[i - iMin] * dfWeight2;
            }

            dfAccumulatorReal += dfRowAccReal * dfWeight1;
            dfAccumulatorImag += dfRowAccImag * dfWeight1;
        }
    }

    if (dfAccumulatorWeight < 0.000001 ||
        (padfRowDensity != nullptr &&
         (dfAccumulatorDensity < 0.000001 ||
          nCountValid < (jMax - jMin + 1) * (iMax - iMin + 1) / 2)))
    {
        *pdfDensity = 0.0;
        return false;
    }

    // Calculate the output taking into account weighting.
    if (dfAccumulatorWeight < 0.99999 || dfAccumulatorWeight > 1.00001)
    {
        const double dfInvAcc = 1.0 / dfAccumulatorWeight;
        *pdfReal = dfAccumulatorReal * dfInvAcc;
        *pdfImag = dfAccumulatorImag * dfInvAcc;
        if (padfRowDensity != nullptr)
            *pdfDensity = dfAccumulatorDensity * dfInvAcc;
        else
            *pdfDensity = 1.0;
    }
    else
    {
        *pdfReal = dfAccumulatorReal;
        *pdfImag = dfAccumulatorImag;
        if (padfRowDensity != nullptr)
            *pdfDensity = dfAccumulatorDensity;
        else
            *pdfDensity = 1.0;
    }

    return true;
}

/************************************************************************/
/*                        GWKComputeWeights()                           */
/************************************************************************/

static void GWKComputeWeights(GDALResampleAlg eResample, int iMin, int iMax,
                              double dfDeltaX, double dfXScale, int jMin,
                              int jMax, double dfDeltaY, double dfYScale,
                              double *padfWeightsHorizontal,
                              double *padfWeightsVertical, double &dfInvWeights)
{

    const FilterFuncType pfnGetWeight = apfGWKFilter[eResample];
    CPLAssert(pfnGetWeight);
    const FilterFunc4ValuesType pfnGetWeight4Values =
        apfGWKFilter4Values[eResample];
    CPLAssert(pfnGetWeight4Values);

    int i = iMin;  // Used after for.
    int iC = 0;    // Used after for.
    double dfAccumulatorWeightHorizontal = 0.0;
    for (; i + 2 < iMax; i += 4, iC += 4)
    {
        padfWeightsHorizontal[iC] = (i - dfDeltaX) * dfXScale;
        padfWeightsHorizontal[iC + 1] = padfWeightsHorizontal[iC] + dfXScale;
        padfWeightsHorizontal[iC + 2] =
            padfWeightsHorizontal[iC + 1] + dfXScale;
        padfWeightsHorizontal[iC + 3] =
            padfWeightsHorizontal[iC + 2] + dfXScale;
        dfAccumulatorWeightHorizontal +=
            pfnGetWeight4Values(padfWeightsHorizontal + iC);
    }
    for (; i <= iMax; ++i, ++iC)
    {
        const double dfWeight = pfnGetWeight((i - dfDeltaX) * dfXScale);
        padfWeightsHorizontal[iC] = dfWeight;
        dfAccumulatorWeightHorizontal += dfWeight;
    }

    int j = jMin;  // Used after for.
    int jC = 0;    // Used after for.
    double dfAccumulatorWeightVertical = 0.0;
    for (; j + 2 < jMax; j += 4, jC += 4)
    {
        padfWeightsVertical[jC] = (j - dfDeltaY) * dfYScale;
        padfWeightsVertical[jC + 1] = padfWeightsVertical[jC] + dfYScale;
        padfWeightsVertical[jC + 2] = padfWeightsVertical[jC + 1] + dfYScale;
        padfWeightsVertical[jC + 3] = padfWeightsVertical[jC + 2] + dfYScale;
        dfAccumulatorWeightVertical +=
            pfnGetWeight4Values(padfWeightsVertical + jC);
    }
    for (; j <= jMax; ++j, ++jC)
    {
        const double dfWeight = pfnGetWeight((j - dfDeltaY) * dfYScale);
        padfWeightsVertical[jC] = dfWeight;
        dfAccumulatorWeightVertical += dfWeight;
    }

    dfInvWeights =
        1. / (dfAccumulatorWeightHorizontal * dfAccumulatorWeightVertical);
}

/************************************************************************/
/*                        GWKResampleNoMasksT()                         */
/************************************************************************/

template <class T>
static bool
GWKResampleNoMasksT(const GDALWarpKernel *poWK, int iBand, double dfSrcX,
                    double dfSrcY, T *pValue, double *padfWeightsHorizontal,
                    double *padfWeightsVertical, double &dfInvWeights)

{
    // Commonly used; save locally.
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    const int iSrcX = static_cast<int>(floor(dfSrcX - 0.5));
    const int iSrcY = static_cast<int>(floor(dfSrcY - 0.5));
    const GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

    const int nXRadius = poWK->nXRadius;
    const int nYRadius = poWK->nYRadius;

    // Politely refuse to process invalid coordinates or obscenely small image.
    if (iSrcX >= nSrcXSize || iSrcY >= nSrcYSize || nXRadius > nSrcXSize ||
        nYRadius > nSrcYSize)
        return GWKBilinearResampleNoMasks4SampleT(poWK, iBand, dfSrcX, dfSrcY,
                                                  pValue);

    T *pSrcBand = reinterpret_cast<T *>(poWK->papabySrcImage[iBand]);
    const double dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double dfDeltaY = dfSrcY - 0.5 - iSrcY;

    const double dfXScale = std::min(poWK->dfXScale, 1.0);
    const double dfYScale = std::min(poWK->dfYScale, 1.0);

    int iMin = 1 - nXRadius;
    if (iSrcX + iMin < 0)
        iMin = -iSrcX;
    int iMax = nXRadius;
    if (iSrcX + iMax >= nSrcXSize - 1)
        iMax = nSrcXSize - 1 - iSrcX;

    int jMin = 1 - nYRadius;
    if (iSrcY + jMin < 0)
        jMin = -iSrcY;
    int jMax = nYRadius;
    if (iSrcY + jMax >= nSrcYSize - 1)
        jMax = nSrcYSize - 1 - iSrcY;

    if (iBand == 0)
    {
        GWKComputeWeights(poWK->eResample, iMin, iMax, dfDeltaX, dfXScale, jMin,
                          jMax, dfDeltaY, dfYScale, padfWeightsHorizontal,
                          padfWeightsVertical, dfInvWeights);
    }

    // Loop over all rows in the kernel.
    double dfAccumulator = 0.0;
    for (int jC = 0, j = jMin; j <= jMax; ++j, ++jC)
    {
        const GPtrDiff_t iSampJ =
            iSrcOffset + static_cast<GPtrDiff_t>(j) * nSrcXSize;

        // Loop over all pixels in the row.
        double dfAccumulatorLocal = 0.0;
        double dfAccumulatorLocal2 = 0.0;
        int iC = 0;
        int i = iMin;
        // Process by chunk of 4 cols.
        for (; i + 2 < iMax; i += 4, iC += 4)
        {
            // Retrieve the pixel & accumulate.
            dfAccumulatorLocal +=
                pSrcBand[i + iSampJ] * padfWeightsHorizontal[iC];
            dfAccumulatorLocal +=
                pSrcBand[i + 1 + iSampJ] * padfWeightsHorizontal[iC + 1];
            dfAccumulatorLocal2 +=
                pSrcBand[i + 2 + iSampJ] * padfWeightsHorizontal[iC + 2];
            dfAccumulatorLocal2 +=
                pSrcBand[i + 3 + iSampJ] * padfWeightsHorizontal[iC + 3];
        }
        dfAccumulatorLocal += dfAccumulatorLocal2;
        if (i < iMax)
        {
            dfAccumulatorLocal +=
                pSrcBand[i + iSampJ] * padfWeightsHorizontal[iC];
            dfAccumulatorLocal +=
                pSrcBand[i + 1 + iSampJ] * padfWeightsHorizontal[iC + 1];
            i += 2;
            iC += 2;
        }
        if (i == iMax)
        {
            dfAccumulatorLocal +=
                pSrcBand[i + iSampJ] * padfWeightsHorizontal[iC];
        }

        dfAccumulator += padfWeightsVertical[jC] * dfAccumulatorLocal;
    }

    *pValue = GWKClampValueT<T>(dfAccumulator * dfInvWeights);

    return true;
}

/* We restrict to 64bit processors because they are guaranteed to have SSE2 */
/* Could possibly be used too on 32bit, but we would need to check at runtime */
#if defined(__x86_64) || defined(_M_X64)

/************************************************************************/
/*                    GWKResampleNoMasks_SSE2_T()                       */
/************************************************************************/

template <class T>
static bool GWKResampleNoMasks_SSE2_T(const GDALWarpKernel *poWK, int iBand,
                                      double dfSrcX, double dfSrcY, T *pValue,
                                      double *padfWeightsHorizontal,
                                      double *padfWeightsVertical,
                                      double &dfInvWeights)
{
    // Commonly used; save locally.
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    const int iSrcX = static_cast<int>(floor(dfSrcX - 0.5));
    const int iSrcY = static_cast<int>(floor(dfSrcY - 0.5));
    const GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
    const int nXRadius = poWK->nXRadius;
    const int nYRadius = poWK->nYRadius;

    // Politely refuse to process invalid coordinates or obscenely small image.
    if (iSrcX >= nSrcXSize || iSrcY >= nSrcYSize || nXRadius > nSrcXSize ||
        nYRadius > nSrcYSize)
        return GWKBilinearResampleNoMasks4SampleT(poWK, iBand, dfSrcX, dfSrcY,
                                                  pValue);

    const T *pSrcBand =
        reinterpret_cast<const T *>(poWK->papabySrcImage[iBand]);

    const double dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double dfDeltaY = dfSrcY - 0.5 - iSrcY;
    const double dfXScale = std::min(poWK->dfXScale, 1.0);
    const double dfYScale = std::min(poWK->dfYScale, 1.0);

    int iMin = 1 - nXRadius;
    if (iSrcX + iMin < 0)
        iMin = -iSrcX;
    int iMax = nXRadius;
    if (iSrcX + iMax >= nSrcXSize - 1)
        iMax = nSrcXSize - 1 - iSrcX;

    int jMin = 1 - nYRadius;
    if (iSrcY + jMin < 0)
        jMin = -iSrcY;
    int jMax = nYRadius;
    if (iSrcY + jMax >= nSrcYSize - 1)
        jMax = nSrcYSize - 1 - iSrcY;

    if (iBand == 0)
    {
        GWKComputeWeights(poWK->eResample, iMin, iMax, dfDeltaX, dfXScale, jMin,
                          jMax, dfDeltaY, dfYScale, padfWeightsHorizontal,
                          padfWeightsVertical, dfInvWeights);
    }

    GPtrDiff_t iSampJ = iSrcOffset + static_cast<GPtrDiff_t>(jMin) * nSrcXSize;
    // Process by chunk of 4 rows.
    int jC = 0;
    int j = jMin;
    double dfAccumulator = 0.0;
    for (; j + 2 < jMax; j += 4, iSampJ += 4 * nSrcXSize, jC += 4)
    {
        // Loop over all pixels in the row.
        int iC = 0;
        int i = iMin;
        // Process by chunk of 4 cols.
        XMMReg4Double v_acc_1 = XMMReg4Double::Zero();
        XMMReg4Double v_acc_2 = XMMReg4Double::Zero();
        XMMReg4Double v_acc_3 = XMMReg4Double::Zero();
        XMMReg4Double v_acc_4 = XMMReg4Double::Zero();
        for (; i + 2 < iMax; i += 4, iC += 4)
        {
            // Retrieve the pixel & accumulate.
            XMMReg4Double v_pixels_1 =
                XMMReg4Double::Load4Val(pSrcBand + i + iSampJ);
            XMMReg4Double v_pixels_2 =
                XMMReg4Double::Load4Val(pSrcBand + i + iSampJ + nSrcXSize);
            XMMReg4Double v_pixels_3 =
                XMMReg4Double::Load4Val(pSrcBand + i + iSampJ + 2 * nSrcXSize);
            XMMReg4Double v_pixels_4 =
                XMMReg4Double::Load4Val(pSrcBand + i + iSampJ + 3 * nSrcXSize);

            XMMReg4Double v_padfWeight =
                XMMReg4Double::Load4Val(padfWeightsHorizontal + iC);

            v_acc_1 += v_pixels_1 * v_padfWeight;
            v_acc_2 += v_pixels_2 * v_padfWeight;
            v_acc_3 += v_pixels_3 * v_padfWeight;
            v_acc_4 += v_pixels_4 * v_padfWeight;
        }

        if (i < iMax)
        {
            XMMReg2Double v_pixels_1 =
                XMMReg2Double::Load2Val(pSrcBand + i + iSampJ);
            XMMReg2Double v_pixels_2 =
                XMMReg2Double::Load2Val(pSrcBand + i + iSampJ + nSrcXSize);
            XMMReg2Double v_pixels_3 =
                XMMReg2Double::Load2Val(pSrcBand + i + iSampJ + 2 * nSrcXSize);
            XMMReg2Double v_pixels_4 =
                XMMReg2Double::Load2Val(pSrcBand + i + iSampJ + 3 * nSrcXSize);

            XMMReg2Double v_padfWeight =
                XMMReg2Double::Load2Val(padfWeightsHorizontal + iC);

            v_acc_1.AddToLow(v_pixels_1 * v_padfWeight);
            v_acc_2.AddToLow(v_pixels_2 * v_padfWeight);
            v_acc_3.AddToLow(v_pixels_3 * v_padfWeight);
            v_acc_4.AddToLow(v_pixels_4 * v_padfWeight);

            i += 2;
            iC += 2;
        }

        double dfAccumulatorLocal_1 = v_acc_1.GetHorizSum();
        double dfAccumulatorLocal_2 = v_acc_2.GetHorizSum();
        double dfAccumulatorLocal_3 = v_acc_3.GetHorizSum();
        double dfAccumulatorLocal_4 = v_acc_4.GetHorizSum();

        if (i == iMax)
        {
            dfAccumulatorLocal_1 += static_cast<double>(pSrcBand[i + iSampJ]) *
                                    padfWeightsHorizontal[iC];
            dfAccumulatorLocal_2 +=
                static_cast<double>(pSrcBand[i + iSampJ + nSrcXSize]) *
                padfWeightsHorizontal[iC];
            dfAccumulatorLocal_3 +=
                static_cast<double>(pSrcBand[i + iSampJ + 2 * nSrcXSize]) *
                padfWeightsHorizontal[iC];
            dfAccumulatorLocal_4 +=
                static_cast<double>(pSrcBand[i + iSampJ + 3 * nSrcXSize]) *
                padfWeightsHorizontal[iC];
        }

        dfAccumulator += padfWeightsVertical[jC] * dfAccumulatorLocal_1;
        dfAccumulator += padfWeightsVertical[jC + 1] * dfAccumulatorLocal_2;
        dfAccumulator += padfWeightsVertical[jC + 2] * dfAccumulatorLocal_3;
        dfAccumulator += padfWeightsVertical[jC + 3] * dfAccumulatorLocal_4;
    }
    for (; j <= jMax; ++j, iSampJ += nSrcXSize, ++jC)
    {
        // Loop over all pixels in the row.
        int iC = 0;
        int i = iMin;
        // Process by chunk of 4 cols.
        XMMReg4Double v_acc = XMMReg4Double::Zero();
        for (; i + 2 < iMax; i += 4, iC += 4)
        {
            // Retrieve the pixel & accumulate.
            XMMReg4Double v_pixels =
                XMMReg4Double::Load4Val(pSrcBand + i + iSampJ);
            XMMReg4Double v_padfWeight =
                XMMReg4Double::Load4Val(padfWeightsHorizontal + iC);

            v_acc += v_pixels * v_padfWeight;
        }

        double dfAccumulatorLocal = v_acc.GetHorizSum();

        if (i < iMax)
        {
            dfAccumulatorLocal +=
                pSrcBand[i + iSampJ] * padfWeightsHorizontal[iC];
            dfAccumulatorLocal +=
                pSrcBand[i + 1 + iSampJ] * padfWeightsHorizontal[iC + 1];
            i += 2;
            iC += 2;
        }
        if (i == iMax)
        {
            dfAccumulatorLocal += static_cast<double>(pSrcBand[i + iSampJ]) *
                                  padfWeightsHorizontal[iC];
        }

        dfAccumulator += padfWeightsVertical[jC] * dfAccumulatorLocal;
    }

    *pValue = GWKClampValueT<T>(dfAccumulator * dfInvWeights);

    return true;
}

/************************************************************************/
/*                     GWKResampleNoMasksT<GByte>()                     */
/************************************************************************/

template <>
bool GWKResampleNoMasksT<GByte>(const GDALWarpKernel *poWK, int iBand,
                                double dfSrcX, double dfSrcY, GByte *pValue,
                                double *padfWeightsHorizontal,
                                double *padfWeightsVertical,
                                double &dfInvWeights)
{
    return GWKResampleNoMasks_SSE2_T(poWK, iBand, dfSrcX, dfSrcY, pValue,
                                     padfWeightsHorizontal, padfWeightsVertical,
                                     dfInvWeights);
}

/************************************************************************/
/*                     GWKResampleNoMasksT<GInt16>()                    */
/************************************************************************/

template <>
bool GWKResampleNoMasksT<GInt16>(const GDALWarpKernel *poWK, int iBand,
                                 double dfSrcX, double dfSrcY, GInt16 *pValue,
                                 double *padfWeightsHorizontal,
                                 double *padfWeightsVertical,
                                 double &dfInvWeights)
{
    return GWKResampleNoMasks_SSE2_T(poWK, iBand, dfSrcX, dfSrcY, pValue,
                                     padfWeightsHorizontal, padfWeightsVertical,
                                     dfInvWeights);
}

/************************************************************************/
/*                     GWKResampleNoMasksT<GUInt16>()                   */
/************************************************************************/

template <>
bool GWKResampleNoMasksT<GUInt16>(const GDALWarpKernel *poWK, int iBand,
                                  double dfSrcX, double dfSrcY, GUInt16 *pValue,
                                  double *padfWeightsHorizontal,
                                  double *padfWeightsVertical,
                                  double &dfInvWeights)
{
    return GWKResampleNoMasks_SSE2_T(poWK, iBand, dfSrcX, dfSrcY, pValue,
                                     padfWeightsHorizontal, padfWeightsVertical,
                                     dfInvWeights);
}

/************************************************************************/
/*                     GWKResampleNoMasksT<float>()                     */
/************************************************************************/

template <>
bool GWKResampleNoMasksT<float>(const GDALWarpKernel *poWK, int iBand,
                                double dfSrcX, double dfSrcY, float *pValue,
                                double *padfWeightsHorizontal,
                                double *padfWeightsVertical,
                                double &dfInvWeights)
{
    return GWKResampleNoMasks_SSE2_T(poWK, iBand, dfSrcX, dfSrcY, pValue,
                                     padfWeightsHorizontal, padfWeightsVertical,
                                     dfInvWeights);
}

#ifdef INSTANTIATE_FLOAT64_SSE2_IMPL

/************************************************************************/
/*                     GWKResampleNoMasksT<double>()                    */
/************************************************************************/

template <>
bool GWKResampleNoMasksT<double>(const GDALWarpKernel *poWK, int iBand,
                                 double dfSrcX, double dfSrcY, double *pValue,
                                 double *padfWeightsHorizontal,
                                 double *padfWeightsVertical,
                                 double &dfInvWeights)
{
    return GWKResampleNoMasks_SSE2_T(poWK, iBand, dfSrcX, dfSrcY, pValue,
                                     padfWeightsHorizontal, padfWeightsVertical,
                                     dfInvWeights);
}

#endif /* INSTANTIATE_FLOAT64_SSE2_IMPL */

#endif /* defined(__x86_64) || defined(_M_X64) */

/************************************************************************/
/*                     GWKRoundSourceCoordinates()                      */
/************************************************************************/

static void GWKRoundSourceCoordinates(
    int nDstXSize, double *padfX, double *padfY, double *padfZ, int *pabSuccess,
    double dfSrcCoordPrecision, double dfErrorThreshold,
    GDALTransformerFunc pfnTransformer, void *pTransformerArg, double dfDstXOff,
    double dfDstY)
{
    double dfPct = 0.8;
    if (dfErrorThreshold > 0 && dfSrcCoordPrecision / dfErrorThreshold >= 10.0)
    {
        dfPct = 1.0 - 2 * 1.0 / (dfSrcCoordPrecision / dfErrorThreshold);
    }
    const double dfExactTransformThreshold = 0.5 * dfPct * dfSrcCoordPrecision;

    for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
    {
        const double dfXBefore = padfX[iDstX];
        const double dfYBefore = padfY[iDstX];
        padfX[iDstX] = floor(padfX[iDstX] / dfSrcCoordPrecision + 0.5) *
                       dfSrcCoordPrecision;
        padfY[iDstX] = floor(padfY[iDstX] / dfSrcCoordPrecision + 0.5) *
                       dfSrcCoordPrecision;

        // If we are in an uncertainty zone, go to non-approximated
        // transformation.
        // Due to the 80% of half-precision threshold, dfSrcCoordPrecision must
        // be at least 10 times greater than the approximation error.
        if (fabs(dfXBefore - padfX[iDstX]) > dfExactTransformThreshold ||
            fabs(dfYBefore - padfY[iDstX]) > dfExactTransformThreshold)
        {
            padfX[iDstX] = iDstX + dfDstXOff;
            padfY[iDstX] = dfDstY;
            padfZ[iDstX] = 0.0;
            pfnTransformer(pTransformerArg, TRUE, 1, padfX + iDstX,
                           padfY + iDstX, padfZ + iDstX, pabSuccess + iDstX);
            padfX[iDstX] = floor(padfX[iDstX] / dfSrcCoordPrecision + 0.5) *
                           dfSrcCoordPrecision;
            padfY[iDstX] = floor(padfY[iDstX] / dfSrcCoordPrecision + 0.5) *
                           dfSrcCoordPrecision;
        }
    }
}

/************************************************************************/
/*                           GWKOpenCLCase()                            */
/*                                                                      */
/*      This is identical to GWKGeneralCase(), but functions via        */
/*      OpenCL. This means we have vector optimization (SSE) and/or     */
/*      GPU optimization depending on our prefs. The code itself is     */
/*      general and not optimized, but by defining constants we can     */
/*      make some pretty darn good code on the fly.                     */
/************************************************************************/

#if defined(HAVE_OPENCL)
static CPLErr GWKOpenCLCase(GDALWarpKernel *poWK)
{
    const int nDstXSize = poWK->nDstXSize;
    const int nDstYSize = poWK->nDstYSize;
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;
    const int nDstXOff = poWK->nDstXOff;
    const int nDstYOff = poWK->nDstYOff;
    const int nSrcXOff = poWK->nSrcXOff;
    const int nSrcYOff = poWK->nSrcYOff;
    bool bUseImag = false;

    cl_channel_type imageFormat;
    switch (poWK->eWorkingDataType)
    {
        case GDT_Byte:
            imageFormat = CL_UNORM_INT8;
            break;
        case GDT_UInt16:
            imageFormat = CL_UNORM_INT16;
            break;
        case GDT_CInt16:
            bUseImag = true;
            [[fallthrough]];
        case GDT_Int16:
            imageFormat = CL_SNORM_INT16;
            break;
        case GDT_CFloat32:
            bUseImag = true;
            [[fallthrough]];
        case GDT_Float32:
            imageFormat = CL_FLOAT;
            break;
        default:
            // No support for higher precision formats.
            CPLDebug("OpenCL", "Unsupported resampling OpenCL data type %d.",
                     static_cast<int>(poWK->eWorkingDataType));
            return CE_Warning;
    }

    OCLResampAlg resampAlg;
    switch (poWK->eResample)
    {
        case GRA_Bilinear:
            resampAlg = OCL_Bilinear;
            break;
        case GRA_Cubic:
            resampAlg = OCL_Cubic;
            break;
        case GRA_CubicSpline:
            resampAlg = OCL_CubicSpline;
            break;
        case GRA_Lanczos:
            resampAlg = OCL_Lanczos;
            break;
        default:
            // No support for higher precision formats.
            CPLDebug("OpenCL",
                     "Unsupported resampling OpenCL resampling alg %d.",
                     static_cast<int>(poWK->eResample));
            return CE_Warning;
    }

    struct oclWarper *warper = nullptr;
    cl_int err;
    CPLErr eErr = CE_None;

    // TODO(schwehr): Fix indenting.
    try
    {

        // Using a factor of 2 or 4 seems to have much less rounding error
        // than 3 on the GPU.
        // Then the rounding error can cause strange artifacts under the
        // right conditions.
        warper = GDALWarpKernelOpenCL_createEnv(
            nSrcXSize, nSrcYSize, nDstXSize, nDstYSize, imageFormat,
            poWK->nBands, 4, bUseImag, poWK->papanBandSrcValid != nullptr,
            poWK->pafDstDensity, poWK->padfDstNoDataReal, resampAlg, &err);

        if (err != CL_SUCCESS || warper == nullptr)
        {
            eErr = CE_Warning;
            if (warper != nullptr)
                throw eErr;
            return eErr;
        }

        CPLDebug("GDAL",
                 "GDALWarpKernel()::GWKOpenCLCase() "
                 "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
                 nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize, nDstXOff, nDstYOff,
                 nDstXSize, nDstYSize);

        if (!poWK->pfnProgress(poWK->dfProgressBase, "", poWK->pProgress))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            eErr = CE_Failure;
            throw eErr;
        }

        /* ====================================================================
         */
        /*      Loop over bands. */
        /* ====================================================================
         */
        for (int iBand = 0; iBand < poWK->nBands; iBand++)
        {
            if (poWK->papanBandSrcValid != nullptr &&
                poWK->papanBandSrcValid[iBand] != nullptr)
            {
                GDALWarpKernelOpenCL_setSrcValid(
                    warper,
                    reinterpret_cast<int *>(poWK->papanBandSrcValid[iBand]),
                    iBand);
                if (err != CL_SUCCESS)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "OpenCL routines reported failure (%d) on line %d.",
                        static_cast<int>(err), __LINE__);
                    eErr = CE_Failure;
                    throw eErr;
                }
            }

            err = GDALWarpKernelOpenCL_setSrcImg(
                warper, poWK->papabySrcImage[iBand], iBand);
            if (err != CL_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "OpenCL routines reported failure (%d) on line %d.",
                         static_cast<int>(err), __LINE__);
                eErr = CE_Failure;
                throw eErr;
            }

            err = GDALWarpKernelOpenCL_setDstImg(
                warper, poWK->papabyDstImage[iBand], iBand);
            if (err != CL_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "OpenCL routines reported failure (%d) on line %d.",
                         static_cast<int>(err), __LINE__);
                eErr = CE_Failure;
                throw eErr;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Allocate x,y,z coordinate arrays for transformation ... one */
        /*      scanlines worth of positions. */
        /* --------------------------------------------------------------------
         */

        // For x, 2 *, because we cache the precomputed values at the end.
        double *padfX =
            static_cast<double *>(CPLMalloc(2 * sizeof(double) * nDstXSize));
        double *padfY =
            static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
        double *padfZ =
            static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
        int *pabSuccess =
            static_cast<int *>(CPLMalloc(sizeof(int) * nDstXSize));
        const double dfSrcCoordPrecision = CPLAtof(CSLFetchNameValueDef(
            poWK->papszWarpOptions, "SRC_COORD_PRECISION", "0"));
        const double dfErrorThreshold = CPLAtof(CSLFetchNameValueDef(
            poWK->papszWarpOptions, "ERROR_THRESHOLD", "0"));

        // Precompute values.
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
            padfX[nDstXSize + iDstX] = iDstX + 0.5 + poWK->nDstXOff;

        /* ====================================================================
         */
        /*      Loop over output lines. */
        /* ====================================================================
         */
        for (int iDstY = 0; iDstY < nDstYSize && eErr == CE_None; ++iDstY)
        {
            /* ----------------------------------------------------------------
             */
            /*      Setup points to transform to source image space. */
            /* ----------------------------------------------------------------
             */
            memcpy(padfX, padfX + nDstXSize, sizeof(double) * nDstXSize);
            const double dfYConst = iDstY + 0.5 + poWK->nDstYOff;
            for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
                padfY[iDstX] = dfYConst;
            memset(padfZ, 0, sizeof(double) * nDstXSize);

            /* ----------------------------------------------------------------
             */
            /*      Transform the points from destination pixel/line
             * coordinates*/
            /*      to source pixel/line coordinates. */
            /* ----------------------------------------------------------------
             */
            poWK->pfnTransformer(poWK->pTransformerArg, TRUE, nDstXSize, padfX,
                                 padfY, padfZ, pabSuccess);
            if (dfSrcCoordPrecision > 0.0)
            {
                GWKRoundSourceCoordinates(
                    nDstXSize, padfX, padfY, padfZ, pabSuccess,
                    dfSrcCoordPrecision, dfErrorThreshold, poWK->pfnTransformer,
                    poWK->pTransformerArg, 0.5 + nDstXOff,
                    iDstY + 0.5 + nDstYOff);
            }

            err = GDALWarpKernelOpenCL_setCoordRow(
                warper, padfX, padfY, nSrcXOff, nSrcYOff, pabSuccess, iDstY);
            if (err != CL_SUCCESS)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "OpenCL routines reported failure (%d) on line %d.",
                         static_cast<int>(err), __LINE__);
                eErr = CE_Failure;
                break;
            }

            // Update the valid & density masks because we don't do so in the
            // kernel.
            for (int iDstX = 0; iDstX < nDstXSize && eErr == CE_None; iDstX++)
            {
                const double dfX = padfX[iDstX];
                const double dfY = padfY[iDstX];
                const GPtrDiff_t iDstOffset =
                    iDstX + static_cast<GPtrDiff_t>(iDstY) * nDstXSize;

                // See GWKGeneralCase() for appropriate commenting.
                if (!pabSuccess[iDstX] || dfX < nSrcXOff || dfY < nSrcYOff)
                    continue;

                int iSrcX = static_cast<int>(dfX) - nSrcXOff;
                int iSrcY = static_cast<int>(dfY) - nSrcYOff;

                if (iSrcX < 0 || iSrcX >= nSrcXSize || iSrcY < 0 ||
                    iSrcY >= nSrcYSize)
                    continue;

                GPtrDiff_t iSrcOffset =
                    iSrcX + static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                double dfDensity = 1.0;

                if (poWK->pafUnifiedSrcDensity != nullptr && iSrcX >= 0 &&
                    iSrcY >= 0 && iSrcX < nSrcXSize && iSrcY < nSrcYSize)
                    dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];

                GWKOverlayDensity(poWK, iDstOffset, dfDensity);

                // Because this is on the bit-wise level, it can't be done well
                // in OpenCL.
                if (poWK->panDstValid != nullptr)
                    poWK->panDstValid[iDstOffset >> 5] |=
                        0x01 << (iDstOffset & 0x1f);
            }
        }

        CPLFree(padfX);
        CPLFree(padfY);
        CPLFree(padfZ);
        CPLFree(pabSuccess);

        if (eErr != CE_None)
            throw eErr;

        err = GDALWarpKernelOpenCL_runResamp(
            warper, poWK->pafUnifiedSrcDensity, poWK->panUnifiedSrcValid,
            poWK->pafDstDensity, poWK->panDstValid, poWK->dfXScale,
            poWK->dfYScale, poWK->dfXFilter, poWK->dfYFilter, poWK->nXRadius,
            poWK->nYRadius, poWK->nFiltInitX, poWK->nFiltInitY);

        if (err != CL_SUCCESS)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "OpenCL routines reported failure (%d) on line %d.",
                     static_cast<int>(err), __LINE__);
            eErr = CE_Failure;
            throw eErr;
        }

        /* ====================================================================
         */
        /*      Loop over output lines. */
        /* ====================================================================
         */
        for (int iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++)
        {
            for (int iBand = 0; iBand < poWK->nBands; iBand++)
            {
                void *rowReal = nullptr;
                void *rowImag = nullptr;
                GByte *pabyDst = poWK->papabyDstImage[iBand];

                err = GDALWarpKernelOpenCL_getRow(warper, &rowReal, &rowImag,
                                                  iDstY, iBand);
                if (err != CL_SUCCESS)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "OpenCL routines reported failure (%d) on line %d.",
                        static_cast<int>(err), __LINE__);
                    eErr = CE_Failure;
                    throw eErr;
                }

                // Copy the data from the warper to GDAL's memory.
                switch (poWK->eWorkingDataType)
                {
                    case GDT_Byte:
                        memcpy(&(pabyDst[iDstY * nDstXSize]), rowReal,
                               sizeof(GByte) * nDstXSize);
                        break;
                    case GDT_Int16:
                        memcpy(&(reinterpret_cast<GInt16 *>(
                                   pabyDst)[iDstY * nDstXSize]),
                               rowReal, sizeof(GInt16) * nDstXSize);
                        break;
                    case GDT_UInt16:
                        memcpy(&(reinterpret_cast<GUInt16 *>(
                                   pabyDst)[iDstY * nDstXSize]),
                               rowReal, sizeof(GUInt16) * nDstXSize);
                        break;
                    case GDT_Float32:
                        memcpy(&(reinterpret_cast<float *>(
                                   pabyDst)[iDstY * nDstXSize]),
                               rowReal, sizeof(float) * nDstXSize);
                        break;
                    case GDT_CInt16:
                    {
                        GInt16 *pabyDstI16 = &(reinterpret_cast<GInt16 *>(
                            pabyDst)[iDstY * nDstXSize]);
                        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
                        {
                            pabyDstI16[iDstX * 2] =
                                static_cast<GInt16 *>(rowReal)[iDstX];
                            pabyDstI16[iDstX * 2 + 1] =
                                static_cast<GInt16 *>(rowImag)[iDstX];
                        }
                    }
                    break;
                    case GDT_CFloat32:
                    {
                        float *pabyDstF32 = &(reinterpret_cast<float *>(
                            pabyDst)[iDstY * nDstXSize]);
                        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
                        {
                            pabyDstF32[iDstX * 2] =
                                static_cast<float *>(rowReal)[iDstX];
                            pabyDstF32[iDstX * 2 + 1] =
                                static_cast<float *>(rowImag)[iDstX];
                        }
                    }
                    break;
                    default:
                        // No support for higher precision formats.
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Unsupported resampling OpenCL data type %d.",
                                 static_cast<int>(poWK->eWorkingDataType));
                        eErr = CE_Failure;
                        throw eErr;
                }
            }
        }
    }
    catch (const CPLErr &)
    {
    }

    if ((err = GDALWarpKernelOpenCL_deleteEnv(warper)) != CL_SUCCESS)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OpenCL routines reported failure (%d) on line %d.",
                 static_cast<int>(err), __LINE__);
        return CE_Failure;
    }

    return eErr;
}
#endif /* defined(HAVE_OPENCL) */

/************************************************************************/
/*                     GWKCheckAndComputeSrcOffsets()                   */
/************************************************************************/
static CPL_INLINE bool
GWKCheckAndComputeSrcOffsets(GWKJobStruct *psJob, int *_pabSuccess, int _iDstX,
                             int _iDstY, double *_padfX, double *_padfY,
                             int _nSrcXSize, int _nSrcYSize,
                             GPtrDiff_t &iSrcOffset)
{
    const GDALWarpKernel *_poWK = psJob->poWK;
    for (int iTry = 0; iTry < 2; ++iTry)
    {
        if (iTry == 1)
        {
            // If the source coordinate is slightly outside of the source raster
            // retry to transform it alone, so that the exact coordinate
            // transformer is used.

            _padfX[_iDstX] = _iDstX + 0.5 + _poWK->nDstXOff;
            _padfY[_iDstX] = _iDstY + 0.5 + _poWK->nDstYOff;
            double dfZ = 0;
            _poWK->pfnTransformer(psJob->pTransformerArg, TRUE, 1,
                                  _padfX + _iDstX, _padfY + _iDstX, &dfZ,
                                  _pabSuccess + _iDstX);
        }
        if (!_pabSuccess[_iDstX])
            return false;

        // If this happens this is likely the symptom of a bug somewhere.
        if (std::isnan(_padfX[_iDstX]) || std::isnan(_padfY[_iDstX]))
        {
            static bool bNanCoordFound = false;
            if (!bNanCoordFound)
            {
                CPLDebug("WARP",
                         "GWKCheckAndComputeSrcOffsets(): "
                         "NaN coordinate found on point %d.",
                         _iDstX);
                bNanCoordFound = true;
            }
            return false;
        }

        /* --------------------------------------------------------------------
         */
        /*      Figure out what pixel we want in our source raster, and skip */
        /*      further processing if it is well off the source image. */
        /* --------------------------------------------------------------------
         */
        /* We test against the value before casting to avoid the */
        /* problem of asymmetric truncation effects around zero.  That is */
        /* -0.5 will be 0 when cast to an int. */
        if (_padfX[_iDstX] < _poWK->nSrcXOff)
        {
            // If the source coordinate is slightly outside of the source raster
            // retry to transform it alone, so that the exact coordinate
            // transformer is used.
            if (iTry == 0 && _padfX[_iDstX] > _poWK->nSrcXOff - 1)
                continue;
            return false;
        }

        if (_padfY[_iDstX] < _poWK->nSrcYOff)
        {
            // If the source coordinate is slightly outside of the source raster
            // retry to transform it alone, so that the exact coordinate
            // transformer is used.
            if (iTry == 0 && _padfY[_iDstX] > _poWK->nSrcYOff - 1)
                continue;
            return false;
        }

        // Check for potential overflow when casting from float to int, (if
        // operating outside natural projection area, padfX/Y can be a very huge
        // positive number before doing the actual conversion), as such cast is
        // undefined behavior that can trigger exception with some compilers
        // (see #6753)
        if (_padfX[_iDstX] + 1e-10 > _nSrcXSize + _poWK->nSrcXOff)
        {
            // If the source coordinate is slightly outside of the source raster
            // retry to transform it alone, so that the exact coordinate
            // transformer is used.
            if (iTry == 0 && _padfX[_iDstX] < _nSrcXSize + _poWK->nSrcXOff + 1)
                continue;
            return false;
        }
        if (_padfY[_iDstX] + 1e-10 > _nSrcYSize + _poWK->nSrcYOff)
        {
            // If the source coordinate is slightly outside of the source raster
            // retry to transform it alone, so that the exact coordinate
            // transformer is used.
            if (iTry == 0 && _padfY[_iDstX] < _nSrcYSize + _poWK->nSrcYOff + 1)
                continue;
            return false;
        }

        break;
    }

    int iSrcX = static_cast<int>(_padfX[_iDstX] + 1.0e-10) - _poWK->nSrcXOff;
    int iSrcY = static_cast<int>(_padfY[_iDstX] + 1.0e-10) - _poWK->nSrcYOff;
    if (iSrcX == _nSrcXSize)
        iSrcX--;
    if (iSrcY == _nSrcYSize)
        iSrcY--;

    // Those checks should normally be OK given the previous ones.
    CPLAssert(iSrcX >= 0);
    CPLAssert(iSrcY >= 0);
    CPLAssert(iSrcX < _nSrcXSize);
    CPLAssert(iSrcY < _nSrcYSize);

    iSrcOffset = iSrcX + static_cast<GPtrDiff_t>(iSrcY) * _nSrcXSize;

    return true;
}

/************************************************************************/
/*                   GWKOneSourceCornerFailsToReproject()               */
/************************************************************************/

static bool GWKOneSourceCornerFailsToReproject(GWKJobStruct *psJob)
{
    GDALWarpKernel *poWK = psJob->poWK;
    for (int iY = 0; iY <= 1; ++iY)
    {
        for (int iX = 0; iX <= 1; ++iX)
        {
            double dfXTmp = poWK->nSrcXOff + iX * poWK->nSrcXSize;
            double dfYTmp = poWK->nSrcYOff + iY * poWK->nSrcYSize;
            double dfZTmp = 0;
            int nSuccess = FALSE;
            poWK->pfnTransformer(psJob->pTransformerArg, FALSE, 1, &dfXTmp,
                                 &dfYTmp, &dfZTmp, &nSuccess);
            if (!nSuccess)
                return true;
        }
    }
    return false;
}

/************************************************************************/
/*                       GWKAdjustSrcOffsetOnEdge()                     */
/************************************************************************/

static bool GWKAdjustSrcOffsetOnEdge(GWKJobStruct *psJob,
                                     GPtrDiff_t &iSrcOffset)
{
    GDALWarpKernel *poWK = psJob->poWK;
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    // Check if the computed source position slightly altered
    // fails to reproject. If so, then we are at the edge of
    // the validity area, and it is worth checking neighbour
    // source pixels for validity.
    int nSuccess = FALSE;
    {
        double dfXTmp =
            poWK->nSrcXOff + static_cast<int>(iSrcOffset % nSrcXSize);
        double dfYTmp =
            poWK->nSrcYOff + static_cast<int>(iSrcOffset / nSrcXSize);
        double dfZTmp = 0;
        poWK->pfnTransformer(psJob->pTransformerArg, FALSE, 1, &dfXTmp, &dfYTmp,
                             &dfZTmp, &nSuccess);
    }
    if (nSuccess)
    {
        double dfXTmp =
            poWK->nSrcXOff + static_cast<int>(iSrcOffset % nSrcXSize);
        double dfYTmp =
            poWK->nSrcYOff + static_cast<int>(iSrcOffset / nSrcXSize) + 1;
        double dfZTmp = 0;
        nSuccess = FALSE;
        poWK->pfnTransformer(psJob->pTransformerArg, FALSE, 1, &dfXTmp, &dfYTmp,
                             &dfZTmp, &nSuccess);
    }
    if (nSuccess)
    {
        double dfXTmp =
            poWK->nSrcXOff + static_cast<int>(iSrcOffset % nSrcXSize) + 1;
        double dfYTmp =
            poWK->nSrcYOff + static_cast<int>(iSrcOffset / nSrcXSize);
        double dfZTmp = 0;
        nSuccess = FALSE;
        poWK->pfnTransformer(psJob->pTransformerArg, FALSE, 1, &dfXTmp, &dfYTmp,
                             &dfZTmp, &nSuccess);
    }

    if (!nSuccess && (iSrcOffset % nSrcXSize) + 1 < nSrcXSize &&
        CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset + 1))
    {
        iSrcOffset++;
        return true;
    }
    else if (!nSuccess && (iSrcOffset / nSrcXSize) + 1 < nSrcYSize &&
             CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset + nSrcXSize))
    {
        iSrcOffset += nSrcXSize;
        return true;
    }
    else if (!nSuccess && (iSrcOffset % nSrcXSize) > 0 &&
             CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset - 1))
    {
        iSrcOffset--;
        return true;
    }
    else if (!nSuccess && (iSrcOffset / nSrcXSize) > 0 &&
             CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset - nSrcXSize))
    {
        iSrcOffset -= nSrcXSize;
        return true;
    }

    return false;
}

/************************************************************************/
/*                 GWKAdjustSrcOffsetOnEdgeUnifiedSrcDensity()          */
/************************************************************************/

static bool GWKAdjustSrcOffsetOnEdgeUnifiedSrcDensity(GWKJobStruct *psJob,
                                                      GPtrDiff_t &iSrcOffset)
{
    GDALWarpKernel *poWK = psJob->poWK;
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    // Check if the computed source position slightly altered
    // fails to reproject. If so, then we are at the edge of
    // the validity area, and it is worth checking neighbour
    // source pixels for validity.
    int nSuccess = FALSE;
    {
        double dfXTmp =
            poWK->nSrcXOff + static_cast<int>(iSrcOffset % nSrcXSize);
        double dfYTmp =
            poWK->nSrcYOff + static_cast<int>(iSrcOffset / nSrcXSize);
        double dfZTmp = 0;
        poWK->pfnTransformer(psJob->pTransformerArg, FALSE, 1, &dfXTmp, &dfYTmp,
                             &dfZTmp, &nSuccess);
    }
    if (nSuccess)
    {
        double dfXTmp =
            poWK->nSrcXOff + static_cast<int>(iSrcOffset % nSrcXSize);
        double dfYTmp =
            poWK->nSrcYOff + static_cast<int>(iSrcOffset / nSrcXSize) + 1;
        double dfZTmp = 0;
        nSuccess = FALSE;
        poWK->pfnTransformer(psJob->pTransformerArg, FALSE, 1, &dfXTmp, &dfYTmp,
                             &dfZTmp, &nSuccess);
    }
    if (nSuccess)
    {
        double dfXTmp =
            poWK->nSrcXOff + static_cast<int>(iSrcOffset % nSrcXSize) + 1;
        double dfYTmp =
            poWK->nSrcYOff + static_cast<int>(iSrcOffset / nSrcXSize);
        double dfZTmp = 0;
        nSuccess = FALSE;
        poWK->pfnTransformer(psJob->pTransformerArg, FALSE, 1, &dfXTmp, &dfYTmp,
                             &dfZTmp, &nSuccess);
    }

    if (!nSuccess && (iSrcOffset % nSrcXSize) + 1 < nSrcXSize &&
        poWK->pafUnifiedSrcDensity[iSrcOffset + 1] >= SRC_DENSITY_THRESHOLD)
    {
        iSrcOffset++;
        return true;
    }
    else if (!nSuccess && (iSrcOffset / nSrcXSize) + 1 < nSrcYSize &&
             poWK->pafUnifiedSrcDensity[iSrcOffset + nSrcXSize] >=
                 SRC_DENSITY_THRESHOLD)
    {
        iSrcOffset += nSrcXSize;
        return true;
    }
    else if (!nSuccess && (iSrcOffset % nSrcXSize) > 0 &&
             poWK->pafUnifiedSrcDensity[iSrcOffset - 1] >=
                 SRC_DENSITY_THRESHOLD)
    {
        iSrcOffset--;
        return true;
    }
    else if (!nSuccess && (iSrcOffset / nSrcXSize) > 0 &&
             poWK->pafUnifiedSrcDensity[iSrcOffset - nSrcXSize] >=
                 SRC_DENSITY_THRESHOLD)
    {
        iSrcOffset -= nSrcXSize;
        return true;
    }

    return false;
}

/************************************************************************/
/*                           GWKGeneralCase()                           */
/*                                                                      */
/*      This is the most general case.  It attempts to handle all       */
/*      possible features with relatively little concern for            */
/*      efficiency.                                                     */
/************************************************************************/

static void GWKGeneralCaseThread(void *pData)
{
    GWKJobStruct *psJob = reinterpret_cast<GWKJobStruct *>(pData);
    GDALWarpKernel *poWK = psJob->poWK;
    const int iYMin = psJob->iYMin;
    const int iYMax = psJob->iYMax;
    const double dfMultFactorVerticalShiftPipeline =
        poWK->bApplyVerticalShift
            ? CPLAtof(CSLFetchNameValueDef(
                  poWK->papszWarpOptions, "MULT_FACTOR_VERTICAL_SHIFT_PIPELINE",
                  "1.0"))
            : 0.0;

    int nDstXSize = poWK->nDstXSize;
    int nSrcXSize = poWK->nSrcXSize;
    int nSrcYSize = poWK->nSrcYSize;

    /* -------------------------------------------------------------------- */
    /*      Allocate x,y,z coordinate arrays for transformation ... one     */
    /*      scanlines worth of positions.                                   */
    /* -------------------------------------------------------------------- */
    // For x, 2 *, because we cache the precomputed values at the end.
    double *padfX =
        static_cast<double *>(CPLMalloc(2 * sizeof(double) * nDstXSize));
    double *padfY =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfZ =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    int *pabSuccess = static_cast<int *>(CPLMalloc(sizeof(int) * nDstXSize));

    const bool bUse4SamplesFormula =
        poWK->dfXScale >= 0.95 && poWK->dfYScale >= 0.95;

    GWKResampleWrkStruct *psWrkStruct = nullptr;
    if (poWK->eResample != GRA_NearestNeighbour)
    {
        psWrkStruct = GWKResampleCreateWrkStruct(poWK);
    }
    const double dfSrcCoordPrecision = CPLAtof(CSLFetchNameValueDef(
        poWK->papszWarpOptions, "SRC_COORD_PRECISION", "0"));
    const double dfErrorThreshold = CPLAtof(
        CSLFetchNameValueDef(poWK->papszWarpOptions, "ERROR_THRESHOLD", "0"));

    const bool bOneSourceCornerFailsToReproject =
        GWKOneSourceCornerFailsToReproject(psJob);

    // Precompute values.
    for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        padfX[nDstXSize + iDstX] = iDstX + 0.5 + poWK->nDstXOff;

    /* ==================================================================== */
    /*      Loop over output lines.                                         */
    /* ==================================================================== */
    for (int iDstY = iYMin; iDstY < iYMax; iDstY++)
    {
        /* --------------------------------------------------------------------
         */
        /*      Setup points to transform to source image space. */
        /* --------------------------------------------------------------------
         */
        memcpy(padfX, padfX + nDstXSize, sizeof(double) * nDstXSize);
        const double dfY = iDstY + 0.5 + poWK->nDstYOff;
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
            padfY[iDstX] = dfY;
        memset(padfZ, 0, sizeof(double) * nDstXSize);

        /* --------------------------------------------------------------------
         */
        /*      Transform the points from destination pixel/line coordinates */
        /*      to source pixel/line coordinates. */
        /* --------------------------------------------------------------------
         */
        poWK->pfnTransformer(psJob->pTransformerArg, TRUE, nDstXSize, padfX,
                             padfY, padfZ, pabSuccess);
        if (dfSrcCoordPrecision > 0.0)
        {
            GWKRoundSourceCoordinates(
                nDstXSize, padfX, padfY, padfZ, pabSuccess, dfSrcCoordPrecision,
                dfErrorThreshold, poWK->pfnTransformer, psJob->pTransformerArg,
                0.5 + poWK->nDstXOff, iDstY + 0.5 + poWK->nDstYOff);
        }

        /* ====================================================================
         */
        /*      Loop over pixels in output scanline. */
        /* ====================================================================
         */
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            GPtrDiff_t iSrcOffset = 0;
            if (!GWKCheckAndComputeSrcOffsets(psJob, pabSuccess, iDstX, iDstY,
                                              padfX, padfY, nSrcXSize,
                                              nSrcYSize, iSrcOffset))
                continue;

            /* --------------------------------------------------------------------
             */
            /*      Do not try to apply transparent/invalid source pixels to the
             */
            /*      destination.  This currently ignores the multi-pixel input
             */
            /*      of bilinear and cubic resamples. */
            /* --------------------------------------------------------------------
             */
            double dfDensity = 1.0;

            if (poWK->pafUnifiedSrcDensity != nullptr)
            {
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                if (dfDensity < SRC_DENSITY_THRESHOLD)
                {
                    if (!bOneSourceCornerFailsToReproject)
                    {
                        continue;
                    }
                    else if (GWKAdjustSrcOffsetOnEdgeUnifiedSrcDensity(
                                 psJob, iSrcOffset))
                    {
                        dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                    }
                    else
                    {
                        continue;
                    }
                }
            }

            if (poWK->panUnifiedSrcValid != nullptr &&
                !CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset))
            {
                if (!bOneSourceCornerFailsToReproject)
                {
                    continue;
                }
                else if (!GWKAdjustSrcOffsetOnEdge(psJob, iSrcOffset))
                {
                    continue;
                }
            }

            /* ====================================================================
             */
            /*      Loop processing each band. */
            /* ====================================================================
             */
            bool bHasFoundDensity = false;

            const GPtrDiff_t iDstOffset =
                iDstX + static_cast<GPtrDiff_t>(iDstY) * nDstXSize;
            for (int iBand = 0; iBand < poWK->nBands; iBand++)
            {
                double dfBandDensity = 0.0;
                double dfValueReal = 0.0;
                double dfValueImag = 0.0;

                /* --------------------------------------------------------------------
                 */
                /*      Collect the source value. */
                /* --------------------------------------------------------------------
                 */
                if (poWK->eResample == GRA_NearestNeighbour || nSrcXSize == 1 ||
                    nSrcYSize == 1)
                {
                    // FALSE is returned if dfBandDensity == 0, which is
                    // checked below.
                    CPL_IGNORE_RET_VAL(GWKGetPixelValue(
                        poWK, iBand, iSrcOffset, &dfBandDensity, &dfValueReal,
                        &dfValueImag));
                }
                else if (poWK->eResample == GRA_Bilinear && bUse4SamplesFormula)
                {
                    GWKBilinearResample4Sample(
                        poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                        padfY[iDstX] - poWK->nSrcYOff, &dfBandDensity,
                        &dfValueReal, &dfValueImag);
                }
                else if (poWK->eResample == GRA_Cubic && bUse4SamplesFormula)
                {
                    GWKCubicResample4Sample(
                        poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                        padfY[iDstX] - poWK->nSrcYOff, &dfBandDensity,
                        &dfValueReal, &dfValueImag);
                }
                else
#ifdef DEBUG
                    // Only useful for clang static analyzer.
                    if (psWrkStruct != nullptr)
#endif
                    {
                        psWrkStruct->pfnGWKResample(
                            poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                            padfY[iDstX] - poWK->nSrcYOff, &dfBandDensity,
                            &dfValueReal, &dfValueImag, psWrkStruct);
                    }

                // If we didn't find any valid inputs skip to next band.
                if (dfBandDensity < BAND_DENSITY_THRESHOLD)
                    continue;

                if (poWK->bApplyVerticalShift)
                {
                    if (!std::isfinite(padfZ[iDstX]))
                        continue;
                    // Subtract padfZ[] since the coordinate transformation is
                    // from target to source
                    dfValueReal =
                        dfValueReal * poWK->dfMultFactorVerticalShift -
                        padfZ[iDstX] * dfMultFactorVerticalShiftPipeline;
                }

                bHasFoundDensity = true;

                /* --------------------------------------------------------------------
                 */
                /*      We have a computed value from the source.  Now apply it
                 * to      */
                /*      the destination pixel. */
                /* --------------------------------------------------------------------
                 */
                GWKSetPixelValue(poWK, iBand, iDstOffset, dfBandDensity,
                                 dfValueReal, dfValueImag);
            }

            if (!bHasFoundDensity)
                continue;

            /* --------------------------------------------------------------------
             */
            /*      Update destination density/validity masks. */
            /* --------------------------------------------------------------------
             */
            GWKOverlayDensity(poWK, iDstOffset, dfDensity);

            if (poWK->panDstValid != nullptr)
            {
                CPLMaskSet(poWK->panDstValid, iDstOffset);
            }
        } /* Next iDstX */

        /* --------------------------------------------------------------------
         */
        /*      Report progress to the user, and optionally cancel out. */
        /* --------------------------------------------------------------------
         */
        if (psJob->pfnProgress && psJob->pfnProgress(psJob))
            break;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup and return.                                             */
    /* -------------------------------------------------------------------- */
    CPLFree(padfX);
    CPLFree(padfY);
    CPLFree(padfZ);
    CPLFree(pabSuccess);
    if (psWrkStruct)
        GWKResampleDeleteWrkStruct(psWrkStruct);
}

static CPLErr GWKGeneralCase(GDALWarpKernel *poWK)
{
    return GWKRun(poWK, "GWKGeneralCase", GWKGeneralCaseThread);
}

/************************************************************************/
/*                            GWKRealCase()                             */
/*                                                                      */
/*      General case for non-complex data types.                        */
/************************************************************************/

static void GWKRealCaseThread(void *pData)

{
    GWKJobStruct *psJob = static_cast<GWKJobStruct *>(pData);
    GDALWarpKernel *poWK = psJob->poWK;
    const int iYMin = psJob->iYMin;
    const int iYMax = psJob->iYMax;

    const int nDstXSize = poWK->nDstXSize;
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;
    const double dfMultFactorVerticalShiftPipeline =
        poWK->bApplyVerticalShift
            ? CPLAtof(CSLFetchNameValueDef(
                  poWK->papszWarpOptions, "MULT_FACTOR_VERTICAL_SHIFT_PIPELINE",
                  "1.0"))
            : 0.0;

    /* -------------------------------------------------------------------- */
    /*      Allocate x,y,z coordinate arrays for transformation ... one     */
    /*      scanlines worth of positions.                                   */
    /* -------------------------------------------------------------------- */

    // For x, 2 *, because we cache the precomputed values at the end.
    double *padfX =
        static_cast<double *>(CPLMalloc(2 * sizeof(double) * nDstXSize));
    double *padfY =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfZ =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    int *pabSuccess = static_cast<int *>(CPLMalloc(sizeof(int) * nDstXSize));

    const bool bUse4SamplesFormula =
        poWK->dfXScale >= 0.95 && poWK->dfYScale >= 0.95;

    GWKResampleWrkStruct *psWrkStruct = nullptr;
    if (poWK->eResample != GRA_NearestNeighbour)
    {
        psWrkStruct = GWKResampleCreateWrkStruct(poWK);
    }
    const double dfSrcCoordPrecision = CPLAtof(CSLFetchNameValueDef(
        poWK->papszWarpOptions, "SRC_COORD_PRECISION", "0"));
    const double dfErrorThreshold = CPLAtof(
        CSLFetchNameValueDef(poWK->papszWarpOptions, "ERROR_THRESHOLD", "0"));

    const bool bSrcMaskIsDensity = poWK->panUnifiedSrcValid == nullptr &&
                                   poWK->papanBandSrcValid == nullptr &&
                                   poWK->pafUnifiedSrcDensity != nullptr;

    const bool bOneSourceCornerFailsToReproject =
        GWKOneSourceCornerFailsToReproject(psJob);

    // Precompute values.
    for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        padfX[nDstXSize + iDstX] = iDstX + 0.5 + poWK->nDstXOff;

    /* ==================================================================== */
    /*      Loop over output lines.                                         */
    /* ==================================================================== */
    for (int iDstY = iYMin; iDstY < iYMax; iDstY++)
    {
        /* --------------------------------------------------------------------
         */
        /*      Setup points to transform to source image space. */
        /* --------------------------------------------------------------------
         */
        memcpy(padfX, padfX + nDstXSize, sizeof(double) * nDstXSize);
        const double dfY = iDstY + 0.5 + poWK->nDstYOff;
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
            padfY[iDstX] = dfY;
        memset(padfZ, 0, sizeof(double) * nDstXSize);

        /* --------------------------------------------------------------------
         */
        /*      Transform the points from destination pixel/line coordinates */
        /*      to source pixel/line coordinates. */
        /* --------------------------------------------------------------------
         */
        poWK->pfnTransformer(psJob->pTransformerArg, TRUE, nDstXSize, padfX,
                             padfY, padfZ, pabSuccess);
        if (dfSrcCoordPrecision > 0.0)
        {
            GWKRoundSourceCoordinates(
                nDstXSize, padfX, padfY, padfZ, pabSuccess, dfSrcCoordPrecision,
                dfErrorThreshold, poWK->pfnTransformer, psJob->pTransformerArg,
                0.5 + poWK->nDstXOff, iDstY + 0.5 + poWK->nDstYOff);
        }

        /* ====================================================================
         */
        /*      Loop over pixels in output scanline. */
        /* ====================================================================
         */
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            GPtrDiff_t iSrcOffset = 0;
            if (!GWKCheckAndComputeSrcOffsets(psJob, pabSuccess, iDstX, iDstY,
                                              padfX, padfY, nSrcXSize,
                                              nSrcYSize, iSrcOffset))
                continue;

            /* --------------------------------------------------------------------
             */
            /*      Do not try to apply transparent/invalid source pixels to the
             */
            /*      destination.  This currently ignores the multi-pixel input
             */
            /*      of bilinear and cubic resamples. */
            /* --------------------------------------------------------------------
             */
            double dfDensity = 1.0;

            if (poWK->pafUnifiedSrcDensity != nullptr)
            {
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                if (dfDensity < SRC_DENSITY_THRESHOLD)
                {
                    if (!bOneSourceCornerFailsToReproject)
                    {
                        continue;
                    }
                    else if (GWKAdjustSrcOffsetOnEdgeUnifiedSrcDensity(
                                 psJob, iSrcOffset))
                    {
                        dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                    }
                    else
                    {
                        continue;
                    }
                }
            }

            if (poWK->panUnifiedSrcValid != nullptr &&
                !CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset))
            {
                if (!bOneSourceCornerFailsToReproject)
                {
                    continue;
                }
                else if (!GWKAdjustSrcOffsetOnEdge(psJob, iSrcOffset))
                {
                    continue;
                }
            }

            /* ====================================================================
             */
            /*      Loop processing each band. */
            /* ====================================================================
             */
            bool bHasFoundDensity = false;

            const GPtrDiff_t iDstOffset =
                iDstX + static_cast<GPtrDiff_t>(iDstY) * nDstXSize;
            for (int iBand = 0; iBand < poWK->nBands; iBand++)
            {
                double dfBandDensity = 0.0;
                double dfValueReal = 0.0;

                /* --------------------------------------------------------------------
                 */
                /*      Collect the source value. */
                /* --------------------------------------------------------------------
                 */
                if (poWK->eResample == GRA_NearestNeighbour || nSrcXSize == 1 ||
                    nSrcYSize == 1)
                {
                    // FALSE is returned if dfBandDensity == 0, which is
                    // checked below.
                    CPL_IGNORE_RET_VAL(GWKGetPixelValueReal(
                        poWK, iBand, iSrcOffset, &dfBandDensity, &dfValueReal));
                }
                else if (poWK->eResample == GRA_Bilinear && bUse4SamplesFormula)
                {
                    double dfValueImagIgnored = 0.0;
                    GWKBilinearResample4Sample(
                        poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                        padfY[iDstX] - poWK->nSrcYOff, &dfBandDensity,
                        &dfValueReal, &dfValueImagIgnored);
                }
                else if (poWK->eResample == GRA_Cubic && bUse4SamplesFormula)
                {
                    if (bSrcMaskIsDensity)
                    {
                        if (poWK->eWorkingDataType == GDT_Byte)
                        {
                            GWKCubicResampleSrcMaskIsDensity4SampleRealT<GByte>(
                                poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                                padfY[iDstX] - poWK->nSrcYOff, &dfBandDensity,
                                &dfValueReal);
                        }
                        else if (poWK->eWorkingDataType == GDT_UInt16)
                        {
                            GWKCubicResampleSrcMaskIsDensity4SampleRealT<
                                GUInt16>(poWK, iBand,
                                         padfX[iDstX] - poWK->nSrcXOff,
                                         padfY[iDstX] - poWK->nSrcYOff,
                                         &dfBandDensity, &dfValueReal);
                        }
                        else
                        {
                            GWKCubicResampleSrcMaskIsDensity4SampleReal(
                                poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                                padfY[iDstX] - poWK->nSrcYOff, &dfBandDensity,
                                &dfValueReal);
                        }
                    }
                    else
                    {
                        double dfValueImagIgnored = 0.0;
                        GWKCubicResample4Sample(
                            poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                            padfY[iDstX] - poWK->nSrcYOff, &dfBandDensity,
                            &dfValueReal, &dfValueImagIgnored);
                    }
                }
                else
#ifdef DEBUG
                    // Only useful for clang static analyzer.
                    if (psWrkStruct != nullptr)
#endif
                    {
                        double dfValueImagIgnored = 0.0;
                        psWrkStruct->pfnGWKResample(
                            poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                            padfY[iDstX] - poWK->nSrcYOff, &dfBandDensity,
                            &dfValueReal, &dfValueImagIgnored, psWrkStruct);
                    }

                // If we didn't find any valid inputs skip to next band.
                if (dfBandDensity < BAND_DENSITY_THRESHOLD)
                    continue;

                if (poWK->bApplyVerticalShift)
                {
                    if (!std::isfinite(padfZ[iDstX]))
                        continue;
                    // Subtract padfZ[] since the coordinate transformation is
                    // from target to source
                    dfValueReal =
                        dfValueReal * poWK->dfMultFactorVerticalShift -
                        padfZ[iDstX] * dfMultFactorVerticalShiftPipeline;
                }

                bHasFoundDensity = true;

                /* --------------------------------------------------------------------
                 */
                /*      We have a computed value from the source.  Now apply it
                 * to      */
                /*      the destination pixel. */
                /* --------------------------------------------------------------------
                 */
                GWKSetPixelValueReal(poWK, iBand, iDstOffset, dfBandDensity,
                                     dfValueReal);
            }

            if (!bHasFoundDensity)
                continue;

            /* --------------------------------------------------------------------
             */
            /*      Update destination density/validity masks. */
            /* --------------------------------------------------------------------
             */
            GWKOverlayDensity(poWK, iDstOffset, dfDensity);

            if (poWK->panDstValid != nullptr)
            {
                CPLMaskSet(poWK->panDstValid, iDstOffset);
            }
        }  // Next iDstX.

        /* --------------------------------------------------------------------
         */
        /*      Report progress to the user, and optionally cancel out. */
        /* --------------------------------------------------------------------
         */
        if (psJob->pfnProgress && psJob->pfnProgress(psJob))
            break;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup and return.                                             */
    /* -------------------------------------------------------------------- */
    CPLFree(padfX);
    CPLFree(padfY);
    CPLFree(padfZ);
    CPLFree(pabSuccess);
    if (psWrkStruct)
        GWKResampleDeleteWrkStruct(psWrkStruct);
}

static CPLErr GWKRealCase(GDALWarpKernel *poWK)
{
    return GWKRun(poWK, "GWKRealCase", GWKRealCaseThread);
}

/************************************************************************/
/*                 GWKCubicResampleNoMasks4MultiBandT()                 */
/************************************************************************/

/* We restrict to 64bit processors because they are guaranteed to have SSE2 */
/* and enough SSE registries */
#if defined(__x86_64) || defined(_M_X64)

static inline float Convolute4x4(const __m128 row0, const __m128 row1,
                                 const __m128 row2, const __m128 row3,
                                 const __m128 weightsXY0,
                                 const __m128 weightsXY1,
                                 const __m128 weightsXY2,
                                 const __m128 weightsXY3)
{
    return XMMHorizontalAdd(_mm_add_ps(
        _mm_add_ps(_mm_mul_ps(row0, weightsXY0), _mm_mul_ps(row1, weightsXY1)),
        _mm_add_ps(_mm_mul_ps(row2, weightsXY2),
                   _mm_mul_ps(row3, weightsXY3))));
}

template <class T>
static void GWKCubicResampleNoMasks4MultiBandT(const GDALWarpKernel *poWK,
                                               double dfSrcX, double dfSrcY,
                                               const GPtrDiff_t iDstOffset)
{
    const double dfSrcXShifted = dfSrcX - 0.5;
    const int iSrcX = static_cast<int>(dfSrcXShifted);
    const double dfSrcYShifted = dfSrcY - 0.5;
    const int iSrcY = static_cast<int>(dfSrcYShifted);
    const GPtrDiff_t iSrcOffset =
        iSrcX + static_cast<GPtrDiff_t>(iSrcY) * poWK->nSrcXSize;

    // Get the bilinear interpolation at the image borders.
    if (iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize || iSrcY - 1 < 0 ||
        iSrcY + 2 >= poWK->nSrcYSize)
    {
        for (int iBand = 0; iBand < poWK->nBands; iBand++)
        {
            T value;
            GWKBilinearResampleNoMasks4SampleT(poWK, iBand, dfSrcX, dfSrcY,
                                               &value);
            reinterpret_cast<T *>(poWK->papabyDstImage[iBand])[iDstOffset] =
                value;
        }
    }
    else
    {
        const float fDeltaX = static_cast<float>(dfSrcXShifted) - iSrcX;
        const float fDeltaY = static_cast<float>(dfSrcYShifted) - iSrcY;

        float afCoeffsX[4];
        float afCoeffsY[4];
        GWKCubicComputeWeights(fDeltaX, afCoeffsX);
        GWKCubicComputeWeights(fDeltaY, afCoeffsY);
        const auto weightsX = _mm_loadu_ps(afCoeffsX);
        const auto weightsXY0 =
            _mm_mul_ps(_mm_load1_ps(&afCoeffsY[0]), weightsX);
        const auto weightsXY1 =
            _mm_mul_ps(_mm_load1_ps(&afCoeffsY[1]), weightsX);
        const auto weightsXY2 =
            _mm_mul_ps(_mm_load1_ps(&afCoeffsY[2]), weightsX);
        const auto weightsXY3 =
            _mm_mul_ps(_mm_load1_ps(&afCoeffsY[3]), weightsX);

        const GPtrDiff_t iOffset = iSrcOffset - poWK->nSrcXSize - 1;

        int iBand = 0;
        // Process 2 bands at a time
        for (; iBand + 1 < poWK->nBands; iBand += 2)
        {
            const T *CPL_RESTRICT pBand0 =
                reinterpret_cast<const T *>(poWK->papabySrcImage[iBand]);
            const auto row0_0 = XMMLoad4Values(pBand0 + iOffset);
            const auto row1_0 =
                XMMLoad4Values(pBand0 + iOffset + poWK->nSrcXSize);
            const auto row2_0 =
                XMMLoad4Values(pBand0 + iOffset + 2 * poWK->nSrcXSize);
            const auto row3_0 =
                XMMLoad4Values(pBand0 + iOffset + 3 * poWK->nSrcXSize);

            const T *CPL_RESTRICT pBand1 =
                reinterpret_cast<const T *>(poWK->papabySrcImage[iBand + 1]);
            const auto row0_1 = XMMLoad4Values(pBand1 + iOffset);
            const auto row1_1 =
                XMMLoad4Values(pBand1 + iOffset + poWK->nSrcXSize);
            const auto row2_1 =
                XMMLoad4Values(pBand1 + iOffset + 2 * poWK->nSrcXSize);
            const auto row3_1 =
                XMMLoad4Values(pBand1 + iOffset + 3 * poWK->nSrcXSize);

            const float fValue_0 =
                Convolute4x4(row0_0, row1_0, row2_0, row3_0, weightsXY0,
                             weightsXY1, weightsXY2, weightsXY3);

            const float fValue_1 =
                Convolute4x4(row0_1, row1_1, row2_1, row3_1, weightsXY0,
                             weightsXY1, weightsXY2, weightsXY3);

            T *CPL_RESTRICT pDstBand0 =
                reinterpret_cast<T *>(poWK->papabyDstImage[iBand]);
            pDstBand0[iDstOffset] = GWKClampValueT<T>(fValue_0);

            T *CPL_RESTRICT pDstBand1 =
                reinterpret_cast<T *>(poWK->papabyDstImage[iBand + 1]);
            pDstBand1[iDstOffset] = GWKClampValueT<T>(fValue_1);
        }
        if (iBand < poWK->nBands)
        {
            const T *CPL_RESTRICT pBand0 =
                reinterpret_cast<const T *>(poWK->papabySrcImage[iBand]);
            const auto row0 = XMMLoad4Values(pBand0 + iOffset);
            const auto row1 =
                XMMLoad4Values(pBand0 + iOffset + poWK->nSrcXSize);
            const auto row2 =
                XMMLoad4Values(pBand0 + iOffset + 2 * poWK->nSrcXSize);
            const auto row3 =
                XMMLoad4Values(pBand0 + iOffset + 3 * poWK->nSrcXSize);

            const float fValue =
                Convolute4x4(row0, row1, row2, row3, weightsXY0, weightsXY1,
                             weightsXY2, weightsXY3);

            T *CPL_RESTRICT pDstBand =
                reinterpret_cast<T *>(poWK->papabyDstImage[iBand]);
            pDstBand[iDstOffset] = GWKClampValueT<T>(fValue);
        }
    }

    if (poWK->pafDstDensity)
        poWK->pafDstDensity[iDstOffset] = 1.0f;
}

#endif  // defined(__x86_64) || defined(_M_X64)

/************************************************************************/
/*                GWKResampleNoMasksOrDstDensityOnlyThreadInternal()    */
/************************************************************************/

template <class T, GDALResampleAlg eResample, int bUse4SamplesFormula>
static void GWKResampleNoMasksOrDstDensityOnlyThreadInternal(void *pData)

{
    GWKJobStruct *psJob = static_cast<GWKJobStruct *>(pData);
    GDALWarpKernel *poWK = psJob->poWK;
    const int iYMin = psJob->iYMin;
    const int iYMax = psJob->iYMax;
    const double dfMultFactorVerticalShiftPipeline =
        poWK->bApplyVerticalShift
            ? CPLAtof(CSLFetchNameValueDef(
                  poWK->papszWarpOptions, "MULT_FACTOR_VERTICAL_SHIFT_PIPELINE",
                  "1.0"))
            : 0.0;

    const int nDstXSize = poWK->nDstXSize;
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    /* -------------------------------------------------------------------- */
    /*      Allocate x,y,z coordinate arrays for transformation ... one     */
    /*      scanlines worth of positions.                                   */
    /* -------------------------------------------------------------------- */

    // For x, 2 *, because we cache the precomputed values at the end.
    double *padfX =
        static_cast<double *>(CPLMalloc(2 * sizeof(double) * nDstXSize));
    double *padfY =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfZ =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    int *pabSuccess = static_cast<int *>(CPLMalloc(sizeof(int) * nDstXSize));

    const int nXRadius = poWK->nXRadius;
    double *padfWeightsX =
        static_cast<double *>(CPLCalloc(1 + nXRadius * 2, sizeof(double)));
    double *padfWeightsY = static_cast<double *>(
        CPLCalloc(1 + poWK->nYRadius * 2, sizeof(double)));
    const double dfSrcCoordPrecision = CPLAtof(CSLFetchNameValueDef(
        poWK->papszWarpOptions, "SRC_COORD_PRECISION", "0"));
    const double dfErrorThreshold = CPLAtof(
        CSLFetchNameValueDef(poWK->papszWarpOptions, "ERROR_THRESHOLD", "0"));

    // Precompute values.
    for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        padfX[nDstXSize + iDstX] = iDstX + 0.5 + poWK->nDstXOff;

    /* ==================================================================== */
    /*      Loop over output lines.                                         */
    /* ==================================================================== */
    for (int iDstY = iYMin; iDstY < iYMax; iDstY++)
    {
        /* --------------------------------------------------------------------
         */
        /*      Setup points to transform to source image space. */
        /* --------------------------------------------------------------------
         */
        memcpy(padfX, padfX + nDstXSize, sizeof(double) * nDstXSize);
        const double dfY = iDstY + 0.5 + poWK->nDstYOff;
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
            padfY[iDstX] = dfY;
        memset(padfZ, 0, sizeof(double) * nDstXSize);

        /* --------------------------------------------------------------------
         */
        /*      Transform the points from destination pixel/line coordinates */
        /*      to source pixel/line coordinates. */
        /* --------------------------------------------------------------------
         */
        poWK->pfnTransformer(psJob->pTransformerArg, TRUE, nDstXSize, padfX,
                             padfY, padfZ, pabSuccess);
        if (dfSrcCoordPrecision > 0.0)
        {
            GWKRoundSourceCoordinates(
                nDstXSize, padfX, padfY, padfZ, pabSuccess, dfSrcCoordPrecision,
                dfErrorThreshold, poWK->pfnTransformer, psJob->pTransformerArg,
                0.5 + poWK->nDstXOff, iDstY + 0.5 + poWK->nDstYOff);
        }

        /* ====================================================================
         */
        /*      Loop over pixels in output scanline. */
        /* ====================================================================
         */
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            GPtrDiff_t iSrcOffset = 0;
            if (!GWKCheckAndComputeSrcOffsets(psJob, pabSuccess, iDstX, iDstY,
                                              padfX, padfY, nSrcXSize,
                                              nSrcYSize, iSrcOffset))
                continue;

            /* ====================================================================
             */
            /*      Loop processing each band. */
            /* ====================================================================
             */
            const GPtrDiff_t iDstOffset =
                iDstX + static_cast<GPtrDiff_t>(iDstY) * nDstXSize;

#if defined(__x86_64) || defined(_M_X64)
            if constexpr (bUse4SamplesFormula && eResample == GRA_Cubic &&
                          (std::is_same<T, GByte>::value ||
                           std::is_same<T, GUInt16>::value))
            {
                if (poWK->nBands > 1 && !poWK->bApplyVerticalShift)
                {
                    GWKCubicResampleNoMasks4MultiBandT<T>(
                        poWK, padfX[iDstX] - poWK->nSrcXOff,
                        padfY[iDstX] - poWK->nSrcYOff, iDstOffset);

                    continue;
                }
            }
#endif  // defined(__x86_64) || defined(_M_X64)

            [[maybe_unused]] double dfInvWeights = 0;
            for (int iBand = 0; iBand < poWK->nBands; iBand++)
            {
                T value = 0;
                if constexpr (eResample == GRA_NearestNeighbour)
                {
                    value = reinterpret_cast<T *>(
                        poWK->papabySrcImage[iBand])[iSrcOffset];
                }
                else if constexpr (bUse4SamplesFormula)
                {
                    if constexpr (eResample == GRA_Bilinear)
                        GWKBilinearResampleNoMasks4SampleT(
                            poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                            padfY[iDstX] - poWK->nSrcYOff, &value);
                    else
                        GWKCubicResampleNoMasks4SampleT(
                            poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                            padfY[iDstX] - poWK->nSrcYOff, &value);
                }
                else
                {
                    GWKResampleNoMasksT(
                        poWK, iBand, padfX[iDstX] - poWK->nSrcXOff,
                        padfY[iDstX] - poWK->nSrcYOff, &value, padfWeightsX,
                        padfWeightsY, dfInvWeights);
                }

                if (poWK->bApplyVerticalShift)
                {
                    if (!std::isfinite(padfZ[iDstX]))
                        continue;
                    // Subtract padfZ[] since the coordinate transformation is
                    // from target to source
                    value = GWKClampValueT<T>(
                        value * poWK->dfMultFactorVerticalShift -
                        padfZ[iDstX] * dfMultFactorVerticalShiftPipeline);
                }

                if (poWK->pafDstDensity)
                    poWK->pafDstDensity[iDstOffset] = 1.0f;

                reinterpret_cast<T *>(poWK->papabyDstImage[iBand])[iDstOffset] =
                    value;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Report progress to the user, and optionally cancel out. */
        /* --------------------------------------------------------------------
         */
        if (psJob->pfnProgress && psJob->pfnProgress(psJob))
            break;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup and return.                                             */
    /* -------------------------------------------------------------------- */
    CPLFree(padfX);
    CPLFree(padfY);
    CPLFree(padfZ);
    CPLFree(pabSuccess);
    CPLFree(padfWeightsX);
    CPLFree(padfWeightsY);
}

template <class T, GDALResampleAlg eResample>
static void GWKResampleNoMasksOrDstDensityOnlyThread(void *pData)
{
    GWKResampleNoMasksOrDstDensityOnlyThreadInternal<T, eResample, FALSE>(
        pData);
}

template <class T, GDALResampleAlg eResample>
static void GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread(void *pData)

{
    GWKJobStruct *psJob = static_cast<GWKJobStruct *>(pData);
    GDALWarpKernel *poWK = psJob->poWK;
    static_assert(eResample == GRA_Bilinear || eResample == GRA_Cubic);
    const bool bUse4SamplesFormula =
        poWK->dfXScale >= 0.95 && poWK->dfYScale >= 0.95;
    if (bUse4SamplesFormula)
        GWKResampleNoMasksOrDstDensityOnlyThreadInternal<T, eResample, TRUE>(
            pData);
    else
        GWKResampleNoMasksOrDstDensityOnlyThreadInternal<T, eResample, FALSE>(
            pData);
}

static CPLErr GWKNearestNoMasksOrDstDensityOnlyByte(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKNearestNoMasksOrDstDensityOnlyByte",
        GWKResampleNoMasksOrDstDensityOnlyThread<GByte, GRA_NearestNeighbour>);
}

static CPLErr GWKBilinearNoMasksOrDstDensityOnlyByte(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKBilinearNoMasksOrDstDensityOnlyByte",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<GByte,
                                                           GRA_Bilinear>);
}

static CPLErr GWKCubicNoMasksOrDstDensityOnlyByte(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKCubicNoMasksOrDstDensityOnlyByte",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<GByte, GRA_Cubic>);
}

static CPLErr GWKCubicNoMasksOrDstDensityOnlyFloat(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKCubicNoMasksOrDstDensityOnlyFloat",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<float, GRA_Cubic>);
}

#ifdef INSTANTIATE_FLOAT64_SSE2_IMPL

static CPLErr GWKCubicNoMasksOrDstDensityOnlyDouble(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKCubicNoMasksOrDstDensityOnlyDouble",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<double, GRA_Cubic>);
}
#endif

static CPLErr GWKCubicSplineNoMasksOrDstDensityOnlyByte(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKCubicSplineNoMasksOrDstDensityOnlyByte",
        GWKResampleNoMasksOrDstDensityOnlyThread<GByte, GRA_CubicSpline>);
}

/************************************************************************/
/*                          GWKNearestByte()                            */
/*                                                                      */
/*      Case for 8bit input data with nearest neighbour resampling      */
/*      using valid flags. Should be as fast as possible for this       */
/*      particular transformation type.                                 */
/************************************************************************/

template <class T> static void GWKNearestThread(void *pData)

{
    GWKJobStruct *psJob = static_cast<GWKJobStruct *>(pData);
    GDALWarpKernel *poWK = psJob->poWK;
    const int iYMin = psJob->iYMin;
    const int iYMax = psJob->iYMax;
    const double dfMultFactorVerticalShiftPipeline =
        poWK->bApplyVerticalShift
            ? CPLAtof(CSLFetchNameValueDef(
                  poWK->papszWarpOptions, "MULT_FACTOR_VERTICAL_SHIFT_PIPELINE",
                  "1.0"))
            : 0.0;

    const int nDstXSize = poWK->nDstXSize;
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    /* -------------------------------------------------------------------- */
    /*      Allocate x,y,z coordinate arrays for transformation ... one     */
    /*      scanlines worth of positions.                                   */
    /* -------------------------------------------------------------------- */

    // For x, 2 *, because we cache the precomputed values at the end.
    double *padfX =
        static_cast<double *>(CPLMalloc(2 * sizeof(double) * nDstXSize));
    double *padfY =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfZ =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    int *pabSuccess = static_cast<int *>(CPLMalloc(sizeof(int) * nDstXSize));

    const double dfSrcCoordPrecision = CPLAtof(CSLFetchNameValueDef(
        poWK->papszWarpOptions, "SRC_COORD_PRECISION", "0"));
    const double dfErrorThreshold = CPLAtof(
        CSLFetchNameValueDef(poWK->papszWarpOptions, "ERROR_THRESHOLD", "0"));

    const bool bOneSourceCornerFailsToReproject =
        GWKOneSourceCornerFailsToReproject(psJob);

    // Precompute values.
    for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        padfX[nDstXSize + iDstX] = iDstX + 0.5 + poWK->nDstXOff;

    /* ==================================================================== */
    /*      Loop over output lines.                                         */
    /* ==================================================================== */
    for (int iDstY = iYMin; iDstY < iYMax; iDstY++)
    {

        /* --------------------------------------------------------------------
         */
        /*      Setup points to transform to source image space. */
        /* --------------------------------------------------------------------
         */
        memcpy(padfX, padfX + nDstXSize, sizeof(double) * nDstXSize);
        const double dfY = iDstY + 0.5 + poWK->nDstYOff;
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
            padfY[iDstX] = dfY;
        memset(padfZ, 0, sizeof(double) * nDstXSize);

        /* --------------------------------------------------------------------
         */
        /*      Transform the points from destination pixel/line coordinates */
        /*      to source pixel/line coordinates. */
        /* --------------------------------------------------------------------
         */
        poWK->pfnTransformer(psJob->pTransformerArg, TRUE, nDstXSize, padfX,
                             padfY, padfZ, pabSuccess);
        if (dfSrcCoordPrecision > 0.0)
        {
            GWKRoundSourceCoordinates(
                nDstXSize, padfX, padfY, padfZ, pabSuccess, dfSrcCoordPrecision,
                dfErrorThreshold, poWK->pfnTransformer, psJob->pTransformerArg,
                0.5 + poWK->nDstXOff, iDstY + 0.5 + poWK->nDstYOff);
        }
        /* ====================================================================
         */
        /*      Loop over pixels in output scanline. */
        /* ====================================================================
         */
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            GPtrDiff_t iSrcOffset = 0;
            if (!GWKCheckAndComputeSrcOffsets(psJob, pabSuccess, iDstX, iDstY,
                                              padfX, padfY, nSrcXSize,
                                              nSrcYSize, iSrcOffset))
                continue;

            /* --------------------------------------------------------------------
             */
            /*      Do not try to apply invalid source pixels to the dest. */
            /* --------------------------------------------------------------------
             */
            if (poWK->panUnifiedSrcValid != nullptr &&
                !CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset))
            {
                if (!bOneSourceCornerFailsToReproject)
                {
                    continue;
                }
                else if (!GWKAdjustSrcOffsetOnEdge(psJob, iSrcOffset))
                {
                    continue;
                }
            }

            /* --------------------------------------------------------------------
             */
            /*      Do not try to apply transparent source pixels to the
             * destination.*/
            /* --------------------------------------------------------------------
             */
            double dfDensity = 1.0;

            if (poWK->pafUnifiedSrcDensity != nullptr)
            {
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                if (dfDensity < SRC_DENSITY_THRESHOLD)
                    continue;
            }

            /* ====================================================================
             */
            /*      Loop processing each band. */
            /* ====================================================================
             */

            const GPtrDiff_t iDstOffset =
                iDstX + static_cast<GPtrDiff_t>(iDstY) * nDstXSize;

            for (int iBand = 0; iBand < poWK->nBands; iBand++)
            {
                T value = 0;
                double dfBandDensity = 0.0;

                /* --------------------------------------------------------------------
                 */
                /*      Collect the source value. */
                /* --------------------------------------------------------------------
                 */
                if (GWKGetPixelT(poWK, iBand, iSrcOffset, &dfBandDensity,
                                 &value))
                {

                    if (poWK->bApplyVerticalShift)
                    {
                        if (!std::isfinite(padfZ[iDstX]))
                            continue;
                        // Subtract padfZ[] since the coordinate transformation
                        // is from target to source
                        value = GWKClampValueT<T>(
                            value * poWK->dfMultFactorVerticalShift -
                            padfZ[iDstX] * dfMultFactorVerticalShiftPipeline);
                    }

                    if (dfBandDensity < 1.0)
                    {
                        if (dfBandDensity == 0.0)
                        {
                            // Do nothing.
                        }
                        else
                        {
                            // Let the general code take care of mixing.
                            GWKSetPixelValueRealT(poWK, iBand, iDstOffset,
                                                  dfBandDensity, value);
                        }
                    }
                    else
                    {
                        reinterpret_cast<T *>(
                            poWK->papabyDstImage[iBand])[iDstOffset] = value;
                    }
                }
            }

            /* --------------------------------------------------------------------
             */
            /*      Mark this pixel valid/opaque in the output. */
            /* --------------------------------------------------------------------
             */
            GWKOverlayDensity(poWK, iDstOffset, dfDensity);

            if (poWK->panDstValid != nullptr)
            {
                CPLMaskSet(poWK->panDstValid, iDstOffset);
            }
        } /* Next iDstX */

        /* --------------------------------------------------------------------
         */
        /*      Report progress to the user, and optionally cancel out. */
        /* --------------------------------------------------------------------
         */
        if (psJob->pfnProgress && psJob->pfnProgress(psJob))
            break;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup and return.                                             */
    /* -------------------------------------------------------------------- */
    CPLFree(padfX);
    CPLFree(padfY);
    CPLFree(padfZ);
    CPLFree(pabSuccess);
}

static CPLErr GWKNearestByte(GDALWarpKernel *poWK)
{
    return GWKRun(poWK, "GWKNearestByte", GWKNearestThread<GByte>);
}

static CPLErr GWKNearestNoMasksOrDstDensityOnlyShort(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKNearestNoMasksOrDstDensityOnlyShort",
        GWKResampleNoMasksOrDstDensityOnlyThread<GInt16, GRA_NearestNeighbour>);
}

static CPLErr GWKBilinearNoMasksOrDstDensityOnlyShort(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKBilinearNoMasksOrDstDensityOnlyShort",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<GInt16,
                                                           GRA_Bilinear>);
}

static CPLErr GWKBilinearNoMasksOrDstDensityOnlyUShort(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKBilinearNoMasksOrDstDensityOnlyUShort",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<GUInt16,
                                                           GRA_Bilinear>);
}

static CPLErr GWKBilinearNoMasksOrDstDensityOnlyFloat(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKBilinearNoMasksOrDstDensityOnlyFloat",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<float,
                                                           GRA_Bilinear>);
}

#ifdef INSTANTIATE_FLOAT64_SSE2_IMPL

static CPLErr GWKBilinearNoMasksOrDstDensityOnlyDouble(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKBilinearNoMasksOrDstDensityOnlyDouble",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<double,
                                                           GRA_Bilinear>);
}
#endif

static CPLErr GWKCubicNoMasksOrDstDensityOnlyShort(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKCubicNoMasksOrDstDensityOnlyShort",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<GInt16, GRA_Cubic>);
}

static CPLErr GWKCubicNoMasksOrDstDensityOnlyUShort(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKCubicNoMasksOrDstDensityOnlyUShort",
        GWKResampleNoMasksOrDstDensityOnlyHas4SampleThread<GUInt16, GRA_Cubic>);
}

static CPLErr GWKCubicSplineNoMasksOrDstDensityOnlyShort(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKCubicSplineNoMasksOrDstDensityOnlyShort",
        GWKResampleNoMasksOrDstDensityOnlyThread<GInt16, GRA_CubicSpline>);
}

static CPLErr GWKCubicSplineNoMasksOrDstDensityOnlyUShort(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKCubicSplineNoMasksOrDstDensityOnlyUShort",
        GWKResampleNoMasksOrDstDensityOnlyThread<GUInt16, GRA_CubicSpline>);
}

static CPLErr GWKNearestShort(GDALWarpKernel *poWK)
{
    return GWKRun(poWK, "GWKNearestShort", GWKNearestThread<GInt16>);
}

static CPLErr GWKNearestNoMasksOrDstDensityOnlyFloat(GDALWarpKernel *poWK)
{
    return GWKRun(
        poWK, "GWKNearestNoMasksOrDstDensityOnlyFloat",
        GWKResampleNoMasksOrDstDensityOnlyThread<float, GRA_NearestNeighbour>);
}

static CPLErr GWKNearestFloat(GDALWarpKernel *poWK)
{
    return GWKRun(poWK, "GWKNearestFloat", GWKNearestThread<float>);
}

/************************************************************************/
/*                           GWKAverageOrMode()                         */
/*                                                                      */
/************************************************************************/

static void GWKAverageOrModeThread(void *pData);

static CPLErr GWKAverageOrMode(GDALWarpKernel *poWK)
{
    return GWKRun(poWK, "GWKAverageOrMode", GWKAverageOrModeThread);
}

// Overall logic based on GWKGeneralCaseThread().
static void GWKAverageOrModeThread(void *pData)
{
    GWKJobStruct *psJob = static_cast<GWKJobStruct *>(pData);
    GDALWarpKernel *poWK = psJob->poWK;
    const int iYMin = psJob->iYMin;
    const int iYMax = psJob->iYMax;
    const double dfMultFactorVerticalShiftPipeline =
        poWK->bApplyVerticalShift
            ? CPLAtof(CSLFetchNameValueDef(
                  poWK->papszWarpOptions, "MULT_FACTOR_VERTICAL_SHIFT_PIPELINE",
                  "1.0"))
            : 0.0;

    const int nDstXSize = poWK->nDstXSize;
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    /* -------------------------------------------------------------------- */
    /*      Find out which algorithm to use (small optim.)                  */
    /* -------------------------------------------------------------------- */
    int nAlgo = 0;

    // These vars only used with nAlgo == 3.
    int *panVals = nullptr;
    int nBins = 0;
    int nBinsOffset = 0;

    // Only used with nAlgo = 2.
    float *pafRealVals = nullptr;
    float *pafImagVals = nullptr;
    int *panRealSums = nullptr;
    int *panImagSums = nullptr;

    // Only used with nAlgo = 6.
    float quant = 0.5;

    // To control array allocation only when data type is complex
    const bool bIsComplex = GDALDataTypeIsComplex(poWK->eWorkingDataType) != 0;

    if (poWK->eResample == GRA_Average)
    {
        nAlgo = GWKAOM_Average;
    }
    else if (poWK->eResample == GRA_RMS)
    {
        nAlgo = GWKAOM_RMS;
    }
    else if (poWK->eResample == GRA_Mode)
    {
        // TODO check color table count > 256.
        if (poWK->eWorkingDataType == GDT_Byte ||
            poWK->eWorkingDataType == GDT_UInt16 ||
            poWK->eWorkingDataType == GDT_Int16)
        {
            nAlgo = GWKAOM_Imode;

            // In the case of a paletted or non-paletted byte band,
            // Input values are between 0 and 255.
            if (poWK->eWorkingDataType == GDT_Byte)
            {
                nBins = 256;
            }
            // In the case of Int8, input values are between -128 and 127.
            else if (poWK->eWorkingDataType == GDT_Int8)
            {
                nBins = 256;
                nBinsOffset = 128;
            }
            // In the case of Int16, input values are between -32768 and 32767.
            else if (poWK->eWorkingDataType == GDT_Int16)
            {
                nBins = 65536;
                nBinsOffset = 32768;
            }
            // In the case of UInt16, input values are between 0 and 65537.
            else if (poWK->eWorkingDataType == GDT_UInt16)
            {
                nBins = 65536;
            }
            panVals =
                static_cast<int *>(VSI_MALLOC_VERBOSE(nBins * sizeof(int)));
            if (panVals == nullptr)
                return;
        }
        else
        {
            nAlgo = GWKAOM_Fmode;

            if (nSrcXSize > 0 && nSrcYSize > 0)
            {
                pafRealVals = static_cast<float *>(
                    VSI_MALLOC3_VERBOSE(nSrcXSize, nSrcYSize, sizeof(float)));
                panRealSums = static_cast<int *>(
                    VSI_MALLOC3_VERBOSE(nSrcXSize, nSrcYSize, sizeof(int)));
                if (pafRealVals == nullptr || panRealSums == nullptr)
                {
                    VSIFree(pafRealVals);
                    VSIFree(panRealSums);
                    return;
                }
            }
        }
    }
    else if (poWK->eResample == GRA_Max)
    {
        nAlgo = GWKAOM_Max;
    }
    else if (poWK->eResample == GRA_Min)
    {
        nAlgo = GWKAOM_Min;
    }
    else if (poWK->eResample == GRA_Med)
    {
        nAlgo = GWKAOM_Quant;
        quant = 0.5;
    }
    else if (poWK->eResample == GRA_Q1)
    {
        nAlgo = GWKAOM_Quant;
        quant = 0.25;
    }
    else if (poWK->eResample == GRA_Q3)
    {
        nAlgo = GWKAOM_Quant;
        quant = 0.75;
    }
#ifdef disabled
    else if (poWK->eResample == GRA_Sum)
    {
        nAlgo = GWKAOM_Sum;
    }
#endif
    else
    {
        // Other resample algorithms not permitted here.
        CPLDebug("GDAL", "GDALWarpKernel():GWKAverageOrModeThread() ERROR, "
                         "illegal resample");
        return;
    }

    CPLDebug("GDAL", "GDALWarpKernel():GWKAverageOrModeThread() using algo %d",
             nAlgo);

    /* -------------------------------------------------------------------- */
    /*      Allocate x,y,z coordinate arrays for transformation ... two     */
    /*      scanlines worth of positions.                                   */
    /* -------------------------------------------------------------------- */

    double *padfX =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfY =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfZ =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfX2 =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfY2 =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    double *padfZ2 =
        static_cast<double *>(CPLMalloc(sizeof(double) * nDstXSize));
    int *pabSuccess = static_cast<int *>(CPLMalloc(sizeof(int) * nDstXSize));
    int *pabSuccess2 = static_cast<int *>(CPLMalloc(sizeof(int) * nDstXSize));

    const double dfSrcCoordPrecision = CPLAtof(CSLFetchNameValueDef(
        poWK->papszWarpOptions, "SRC_COORD_PRECISION", "0"));
    const double dfErrorThreshold = CPLAtof(
        CSLFetchNameValueDef(poWK->papszWarpOptions, "ERROR_THRESHOLD", "0"));

    const double dfExcludedValuesThreshold =
        CPLAtof(CSLFetchNameValueDef(poWK->papszWarpOptions,
                                     "EXCLUDED_VALUES_PCT_THRESHOLD", "50")) /
        100.0;
    const double dfNodataValuesThreshold =
        CPLAtof(CSLFetchNameValueDef(poWK->papszWarpOptions,
                                     "NODATA_VALUES_PCT_THRESHOLD", "100")) /
        100.0;

    const int nXMargin =
        2 * std::max(1, static_cast<int>(std::ceil(1. / poWK->dfXScale)));
    const int nYMargin =
        2 * std::max(1, static_cast<int>(std::ceil(1. / poWK->dfYScale)));

    /* ==================================================================== */
    /*      Loop over output lines.                                         */
    /* ==================================================================== */
    for (int iDstY = iYMin; iDstY < iYMax; iDstY++)
    {

        /* --------------------------------------------------------------------
         */
        /*      Setup points to transform to source image space. */
        /* --------------------------------------------------------------------
         */
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            padfX[iDstX] = iDstX + poWK->nDstXOff;
            padfY[iDstX] = iDstY + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
            padfX2[iDstX] = iDstX + 1.0 + poWK->nDstXOff;
            padfY2[iDstX] = iDstY + 1.0 + poWK->nDstYOff;
            padfZ2[iDstX] = 0.0;
        }

        /* --------------------------------------------------------------------
         */
        /*      Transform the points from destination pixel/line coordinates */
        /*      to source pixel/line coordinates. */
        /* --------------------------------------------------------------------
         */
        poWK->pfnTransformer(psJob->pTransformerArg, TRUE, nDstXSize, padfX,
                             padfY, padfZ, pabSuccess);
        poWK->pfnTransformer(psJob->pTransformerArg, TRUE, nDstXSize, padfX2,
                             padfY2, padfZ2, pabSuccess2);

        if (dfSrcCoordPrecision > 0.0)
        {
            GWKRoundSourceCoordinates(
                nDstXSize, padfX, padfY, padfZ, pabSuccess, dfSrcCoordPrecision,
                dfErrorThreshold, poWK->pfnTransformer, psJob->pTransformerArg,
                poWK->nDstXOff, iDstY + poWK->nDstYOff);
            GWKRoundSourceCoordinates(
                nDstXSize, padfX2, padfY2, padfZ2, pabSuccess2,
                dfSrcCoordPrecision, dfErrorThreshold, poWK->pfnTransformer,
                psJob->pTransformerArg, 1.0 + poWK->nDstXOff,
                iDstY + 1.0 + poWK->nDstYOff);
        }

        /* ====================================================================
         */
        /*      Loop over pixels in output scanline. */
        /* ====================================================================
         */
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            GPtrDiff_t iSrcOffset = 0;
            double dfDensity = 1.0;
            bool bHasFoundDensity = false;

            if (!pabSuccess[iDstX] || !pabSuccess2[iDstX])
                continue;

            // Add some checks so that padfX[iDstX] - poWK->nSrcXOff is in
            // reasonable range (https://github.com/OSGeo/gdal/issues/2365)
            if (!(padfX[iDstX] - poWK->nSrcXOff >= -nXMargin &&
                  padfX2[iDstX] - poWK->nSrcXOff >= -nXMargin &&
                  padfY[iDstX] - poWK->nSrcYOff >= -nYMargin &&
                  padfY2[iDstX] - poWK->nSrcYOff >= -nYMargin &&
                  padfX[iDstX] - poWK->nSrcXOff - nSrcXSize <= nXMargin &&
                  padfX2[iDstX] - poWK->nSrcXOff - nSrcXSize <= nXMargin &&
                  padfY[iDstX] - poWK->nSrcYOff - nSrcYSize <= nYMargin &&
                  padfY2[iDstX] - poWK->nSrcYOff - nSrcYSize <= nYMargin))
            {
                continue;
            }

            const GPtrDiff_t iDstOffset =
                iDstX + static_cast<GPtrDiff_t>(iDstY) * nDstXSize;

            // Compute corners in source crs.

            // The transformation might not have preserved ordering of
            // coordinates so do the necessary swapping (#5433).
            // NOTE: this is really an approximative fix. To do something
            // more precise we would for example need to compute the
            // transformation of coordinates in the
            // [iDstX,iDstY]x[iDstX+1,iDstY+1] square back to source
            // coordinates, and take the bounding box of the got source
            // coordinates.

            if (padfX[iDstX] > padfX2[iDstX])
                std::swap(padfX[iDstX], padfX2[iDstX]);

            // Detect situations where the target pixel is close to the
            // antimeridian and when padfX[iDstX] and padfX2[iDstX] are very
            // close to the left-most and right-most columns of the source
            // raster. The 2 value below was experimentally determined to
            // avoid false-positives and false-negatives.
            // Addresses https://github.com/OSGeo/gdal/issues/6478
            bool bWrapOverX = false;
            const int nThresholdWrapOverX = std::min(2, nSrcXSize / 10);
            if (poWK->nSrcXOff == 0 &&
                padfX[iDstX] * poWK->dfXScale < nThresholdWrapOverX &&
                (nSrcXSize - padfX2[iDstX]) * poWK->dfXScale <
                    nThresholdWrapOverX)
            {
                // Check there is a discontinuity by checking at mid-pixel.
                // NOTE: all this remains fragile. To confidently
                // detect antimeridian warping we should probably try to access
                // georeferenced coordinates, and not rely only on tests on
                // image space coordinates. But accessing georeferenced
                // coordinates from here is not trivial, and we would for example
                // have to handle both geographic, Mercator, etc.
                // Let's hope this heuristics is good enough for now.
                double x = iDstX + 0.5 + poWK->nDstXOff;
                double y = iDstY + poWK->nDstYOff;
                double z = 0;
                int bSuccess = FALSE;
                poWK->pfnTransformer(psJob->pTransformerArg, TRUE, 1, &x, &y,
                                     &z, &bSuccess);
                if (bSuccess && x < padfX[iDstX])
                {
                    bWrapOverX = true;
                    std::swap(padfX[iDstX], padfX2[iDstX]);
                    padfX2[iDstX] += nSrcXSize;
                }
            }

            const double dfXMin = padfX[iDstX] - poWK->nSrcXOff;
            const double dfXMax = padfX2[iDstX] - poWK->nSrcXOff;
            constexpr double EPS = 1e-10;
            // Check that [dfXMin, dfXMax] intersect with [0,nSrcXSize] with a tolerance
            if (!(dfXMax > -EPS && dfXMin < nSrcXSize + EPS))
                continue;
            int iSrcXMin = static_cast<int>(std::max(floor(dfXMin + EPS), 0.0));
            int iSrcXMax = static_cast<int>(
                std::min(ceil(dfXMax - EPS), static_cast<double>(INT_MAX)));
            if (!bWrapOverX)
                iSrcXMax = std::min(iSrcXMax, nSrcXSize);
            if (iSrcXMin == iSrcXMax && iSrcXMax < nSrcXSize)
                iSrcXMax++;

            if (padfY[iDstX] > padfY2[iDstX])
                std::swap(padfY[iDstX], padfY2[iDstX]);
            const double dfYMin = padfY[iDstX] - poWK->nSrcYOff;
            const double dfYMax = padfY2[iDstX] - poWK->nSrcYOff;
            // Check that [dfYMin, dfYMax] intersect with [0,nSrcYSize] with a tolerance
            if (!(dfYMax > -EPS && dfYMin < nSrcYSize + EPS))
                continue;
            int iSrcYMin = static_cast<int>(std::max(floor(dfYMin + EPS), 0.0));
            int iSrcYMax =
                std::min(static_cast<int>(ceil(dfYMax - EPS)), nSrcYSize);
            if (iSrcYMin == iSrcYMax && iSrcYMax < nSrcYSize)
                iSrcYMax++;

#define COMPUTE_WEIGHT_Y(iSrcY)                                                \
    ((iSrcY == iSrcYMin)                                                       \
         ? ((iSrcYMin + 1 == iSrcYMax) ? 1.0 : 1 - (dfYMin - iSrcYMin))        \
     : (iSrcY + 1 == iSrcYMax) ? 1 - (iSrcYMax - dfYMax)                       \
                               : 1.0)

#define COMPUTE_WEIGHT(iSrcX, dfWeightY)                                       \
    ((iSrcX == iSrcXMin)       ? ((iSrcXMin + 1 == iSrcXMax)                   \
                                      ? dfWeightY                              \
                                      : dfWeightY * (1 - (dfXMin - iSrcXMin))) \
     : (iSrcX + 1 == iSrcXMax) ? dfWeightY * (1 - (iSrcXMax - dfXMax))         \
                               : dfWeightY)

            bool bDone = false;

            // Special Average mode where we process all bands together,
            // to avoid averaging tuples that match an entry of m_aadfExcludedValues
            if (nAlgo == GWKAOM_Average &&
                (!poWK->m_aadfExcludedValues.empty() ||
                 dfNodataValuesThreshold < 1 - EPS) &&
                !poWK->bApplyVerticalShift && !bIsComplex)
            {
                double dfTotalWeightInvalid = 0.0;
                double dfTotalWeightExcluded = 0.0;
                double dfTotalWeightRegular = 0.0;
                std::vector<double> adfValueReal(poWK->nBands, 0);
                std::vector<double> adfValueAveraged(poWK->nBands, 0);
                std::vector<int> anCountExcludedValues(
                    poWK->m_aadfExcludedValues.size(), 0);

                for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                {
                    const double dfWeightY = COMPUTE_WEIGHT_Y(iSrcY);
                    iSrcOffset =
                        iSrcXMin + static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                    for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                         iSrcX++, iSrcOffset++)
                    {
                        if (bWrapOverX)
                            iSrcOffset =
                                (iSrcX % nSrcXSize) +
                                static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

                        const double dfWeight =
                            COMPUTE_WEIGHT(iSrcX, dfWeightY);
                        if (dfWeight <= 0)
                            continue;

                        if (poWK->panUnifiedSrcValid != nullptr &&
                            !CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset))
                        {
                            dfTotalWeightInvalid += dfWeight;
                            continue;
                        }

                        bool bAllValid = true;
                        for (int iBand = 0; iBand < poWK->nBands; iBand++)
                        {
                            double dfBandDensity = 0;
                            double dfValueImagTmp = 0;
                            if (!(GWKGetPixelValue(
                                      poWK, iBand, iSrcOffset, &dfBandDensity,
                                      &adfValueReal[iBand], &dfValueImagTmp) &&
                                  dfBandDensity > BAND_DENSITY_THRESHOLD))
                            {
                                bAllValid = false;
                                break;
                            }
                        }

                        if (!bAllValid)
                        {
                            dfTotalWeightInvalid += dfWeight;
                            continue;
                        }

                        bool bExcludedValueFound = false;
                        for (size_t i = 0;
                             i < poWK->m_aadfExcludedValues.size(); ++i)
                        {
                            if (poWK->m_aadfExcludedValues[i] == adfValueReal)
                            {
                                bExcludedValueFound = true;
                                ++anCountExcludedValues[i];
                                dfTotalWeightExcluded += dfWeight;
                                break;
                            }
                        }
                        if (!bExcludedValueFound)
                        {
                            // Weighted incremental algorithm mean
                            // Cf https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Weighted_incremental_algorithm
                            dfTotalWeightRegular += dfWeight;
                            for (int iBand = 0; iBand < poWK->nBands; iBand++)
                            {
                                adfValueAveraged[iBand] +=
                                    (dfWeight / dfTotalWeightRegular) *
                                    (adfValueReal[iBand] -
                                     adfValueAveraged[iBand]);
                            }
                        }
                    }
                }

                const double dfTotalWeight = dfTotalWeightInvalid +
                                             dfTotalWeightExcluded +
                                             dfTotalWeightRegular;
                if (dfTotalWeightInvalid > 0 &&
                    dfTotalWeightInvalid >=
                        dfNodataValuesThreshold * dfTotalWeight)
                {
                    // Do nothing. Let bHasFoundDensity to false.
                }
                else if (dfTotalWeightExcluded > 0 &&
                         dfTotalWeightExcluded >=
                             dfExcludedValuesThreshold * dfTotalWeight)
                {
                    // Find the most represented excluded value tuple
                    size_t iExcludedValue = 0;
                    int nExcludedValueCount = 0;
                    for (size_t i = 0; i < poWK->m_aadfExcludedValues.size();
                         ++i)
                    {
                        if (anCountExcludedValues[i] > nExcludedValueCount)
                        {
                            iExcludedValue = i;
                            nExcludedValueCount = anCountExcludedValues[i];
                        }
                    }

                    bHasFoundDensity = true;

                    for (int iBand = 0; iBand < poWK->nBands; iBand++)
                    {
                        GWKSetPixelValue(
                            poWK, iBand, iDstOffset, /* dfBandDensity = */ 1.0,
                            poWK->m_aadfExcludedValues[iExcludedValue][iBand],
                            0);
                    }
                }
                else if (dfTotalWeightRegular > 0)
                {
                    bHasFoundDensity = true;

                    for (int iBand = 0; iBand < poWK->nBands; iBand++)
                    {
                        GWKSetPixelValue(poWK, iBand, iDstOffset,
                                         /* dfBandDensity = */ 1.0,
                                         adfValueAveraged[iBand], 0);
                    }
                }

                // Skip below loop on bands
                bDone = true;
            }

            /* ====================================================================
             */
            /*      Loop processing each band. */
            /* ====================================================================
             */

            for (int iBand = 0; !bDone && iBand < poWK->nBands; iBand++)
            {
                double dfBandDensity = 0.0;
                double dfValueReal = 0.0;
                double dfValueImag = 0.0;
                double dfValueRealTmp = 0.0;
                double dfValueImagTmp = 0.0;

                /* --------------------------------------------------------------------
                 */
                /*      Collect the source value. */
                /* --------------------------------------------------------------------
                 */

                // Loop over source lines and pixels - 3 possible algorithms.

                // poWK->eResample == GRA_Average.
                if (nAlgo == GWKAOM_Average)
                {
                    double dfTotalWeight = 0.0;

                    // This code adapted from GDALDownsampleChunk32R_AverageT()
                    // in gcore/overview.cpp.
                    for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                    {
                        const double dfWeightY = COMPUTE_WEIGHT_Y(iSrcY);
                        iSrcOffset = iSrcXMin +
                                     static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                        for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                             iSrcX++, iSrcOffset++)
                        {
                            if (bWrapOverX)
                                iSrcOffset =
                                    (iSrcX % nSrcXSize) +
                                    static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

                            if (poWK->panUnifiedSrcValid != nullptr &&
                                !CPLMaskGet(poWK->panUnifiedSrcValid,
                                            iSrcOffset))
                            {
                                continue;
                            }

                            if (GWKGetPixelValue(
                                    poWK, iBand, iSrcOffset, &dfBandDensity,
                                    &dfValueRealTmp, &dfValueImagTmp) &&
                                dfBandDensity > BAND_DENSITY_THRESHOLD)
                            {
                                const double dfWeight =
                                    COMPUTE_WEIGHT(iSrcX, dfWeightY);
                                if (dfWeight > 0)
                                {
                                    // Weighted incremental algorithm mean
                                    // Cf https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Weighted_incremental_algorithm
                                    dfTotalWeight += dfWeight;
                                    dfValueReal +=
                                        (dfWeight / dfTotalWeight) *
                                        (dfValueRealTmp - dfValueReal);
                                    if (bIsComplex)
                                    {
                                        dfValueImag +=
                                            (dfWeight / dfTotalWeight) *
                                            (dfValueImagTmp - dfValueImag);
                                    }
                                }
                            }
                        }
                    }

                    if (dfTotalWeight > 0)
                    {
                        if (poWK->bApplyVerticalShift)
                        {
                            if (!std::isfinite(padfZ[iDstX]))
                                continue;
                            // Subtract padfZ[] since the coordinate
                            // transformation is from target to source
                            dfValueReal =
                                dfValueReal * poWK->dfMultFactorVerticalShift -
                                padfZ[iDstX] *
                                    dfMultFactorVerticalShiftPipeline;
                        }

                        dfBandDensity = 1;
                        bHasFoundDensity = true;
                    }
                }  // GRA_Average.
                // poWK->eResample == GRA_RMS.
                if (nAlgo == GWKAOM_RMS)
                {
                    double dfTotalReal = 0.0;
                    double dfTotalImag = 0.0;
                    double dfTotalWeight = 0.0;
                    // This code adapted from GDALDownsampleChunk32R_AverageT()
                    // in gcore/overview.cpp.
                    for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                    {
                        const double dfWeightY = COMPUTE_WEIGHT_Y(iSrcY);
                        iSrcOffset = iSrcXMin +
                                     static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                        for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                             iSrcX++, iSrcOffset++)
                        {
                            if (bWrapOverX)
                                iSrcOffset =
                                    (iSrcX % nSrcXSize) +
                                    static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

                            if (poWK->panUnifiedSrcValid != nullptr &&
                                !CPLMaskGet(poWK->panUnifiedSrcValid,
                                            iSrcOffset))
                            {
                                continue;
                            }

                            if (GWKGetPixelValue(
                                    poWK, iBand, iSrcOffset, &dfBandDensity,
                                    &dfValueRealTmp, &dfValueImagTmp) &&
                                dfBandDensity > BAND_DENSITY_THRESHOLD)
                            {
                                const double dfWeight =
                                    COMPUTE_WEIGHT(iSrcX, dfWeightY);
                                dfTotalWeight += dfWeight;
                                dfTotalReal +=
                                    dfValueRealTmp * dfValueRealTmp * dfWeight;
                                if (bIsComplex)
                                    dfTotalImag += dfValueImagTmp *
                                                   dfValueImagTmp * dfWeight;
                            }
                        }
                    }

                    if (dfTotalWeight > 0)
                    {
                        dfValueReal = sqrt(dfTotalReal / dfTotalWeight);

                        if (poWK->bApplyVerticalShift)
                        {
                            if (!std::isfinite(padfZ[iDstX]))
                                continue;
                            // Subtract padfZ[] since the coordinate
                            // transformation is from target to source
                            dfValueReal =
                                dfValueReal * poWK->dfMultFactorVerticalShift -
                                padfZ[iDstX] *
                                    dfMultFactorVerticalShiftPipeline;
                        }

                        if (bIsComplex)
                            dfValueImag = sqrt(dfTotalImag / dfTotalWeight);

                        dfBandDensity = 1;
                        bHasFoundDensity = true;
                    }
                }  // GRA_RMS.
#ifdef disabled
                else if (nAlgo == GWKAOM_Sum)
                // poWK->eResample == GRA_Sum
                {
                    double dfTotalReal = 0.0;
                    double dfTotalImag = 0.0;
                    bool bFoundValid = false;

                    for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                    {
                        const double dfWeightY = COMPUTE_WEIGHT_Y(iSrcY);
                        iSrcOffset = iSrcXMin +
                                     static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                        for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                             iSrcX++, iSrcOffset++)
                        {
                            if (bWrapOverX)
                                iSrcOffset =
                                    (iSrcX % nSrcXSize) +
                                    static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

                            if (poWK->panUnifiedSrcValid != nullptr &&
                                !CPLMaskGet(poWK->panUnifiedSrcValid,
                                            iSrcOffset))
                            {
                                continue;
                            }

                            if (GWKGetPixelValue(
                                    poWK, iBand, iSrcOffset, &dfBandDensity,
                                    &dfValueRealTmp, &dfValueImagTmp) &&
                                dfBandDensity > BAND_DENSITY_THRESHOLD)
                            {
                                const double dfWeight =
                                    COMPUTE_WEIGHT(iSrcX, dfWeightY);
                                bFoundValid = true;
                                dfTotalReal += dfValueRealTmp * dfWeight;
                                if (bIsComplex)
                                {
                                    dfTotalImag += dfValueImagTmp * dfWeight;
                                }
                            }
                        }
                    }

                    if (bFoundValid)
                    {
                        dfValueReal = dfTotalReal;

                        if (poWK->bApplyVerticalShift)
                        {
                            if (!std::isfinite(padfZ[iDstX]))
                                continue;
                            // Subtract padfZ[] since the coordinate
                            // transformation is from target to source
                            dfValueReal =
                                dfValueReal * poWK->dfMultFactorVerticalShift -
                                padfZ[iDstX] *
                                    dfMultFactorVerticalShiftPipeline;
                        }

                        if (bIsComplex)
                        {
                            dfValueImag = dfTotalImag;
                        }
                        dfBandDensity = 1;
                        bHasFoundDensity = true;
                    }
                }  // GRA_Sum.
#endif
                else if (nAlgo == GWKAOM_Imode || nAlgo == GWKAOM_Fmode)
                // poWK->eResample == GRA_Mode
                {
                    // This code adapted from GDALDownsampleChunk32R_Mode() in
                    // gcore/overview.cpp.
                    if (nAlgo == GWKAOM_Fmode)  // int32 or float.
                    {
                        // Does it make sense it makes to run a
                        // majority filter on floating point data? But, here it
                        // is for the sake of compatibility. It won't look
                        // right on RGB images by the nature of the filter.
                        int iMaxInd = 0;
                        int iMaxVal = -1;
                        int i = 0;

                        for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                        {
                            iSrcOffset =
                                iSrcXMin +
                                static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                            for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                                 iSrcX++, iSrcOffset++)
                            {
                                if (bWrapOverX)
                                    iSrcOffset =
                                        (iSrcX % nSrcXSize) +
                                        static_cast<GPtrDiff_t>(iSrcY) *
                                            nSrcXSize;

                                if (poWK->panUnifiedSrcValid != nullptr &&
                                    !CPLMaskGet(poWK->panUnifiedSrcValid,
                                                iSrcOffset))
                                    continue;

                                if (GWKGetPixelValue(
                                        poWK, iBand, iSrcOffset, &dfBandDensity,
                                        &dfValueRealTmp, &dfValueImagTmp) &&
                                    dfBandDensity > BAND_DENSITY_THRESHOLD)
                                {
                                    const float fVal =
                                        static_cast<float>(dfValueRealTmp);

                                    // Check array for existing entry.
                                    for (i = 0; i < iMaxInd; ++i)
                                        if (pafRealVals[i] == fVal &&
                                            ++panRealSums[i] >
                                                panRealSums[iMaxVal])
                                        {
                                            iMaxVal = i;
                                            break;
                                        }

                                    // Add to arr if entry not already there.
                                    if (i == iMaxInd)
                                    {
                                        pafRealVals[iMaxInd] = fVal;
                                        panRealSums[iMaxInd] = 1;

                                        if (iMaxVal < 0)
                                            iMaxVal = iMaxInd;

                                        ++iMaxInd;
                                    }
                                }
                            }
                        }

                        if (iMaxVal != -1)
                        {
                            dfValueReal = pafRealVals[iMaxVal];

                            if (poWK->bApplyVerticalShift)
                            {
                                if (!std::isfinite(padfZ[iDstX]))
                                    continue;
                                // Subtract padfZ[] since the coordinate
                                // transformation is from target to source
                                dfValueReal =
                                    dfValueReal *
                                        poWK->dfMultFactorVerticalShift -
                                    padfZ[iDstX] *
                                        dfMultFactorVerticalShiftPipeline;
                            }

                            dfBandDensity = 1;
                            bHasFoundDensity = true;
                        }
                    }
                    else  // byte or int16.
                    {
                        int nMaxVal = 0;
                        int iMaxInd = -1;

                        memset(panVals, 0, nBins * sizeof(int));

                        for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                        {
                            iSrcOffset =
                                iSrcXMin +
                                static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                            for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                                 iSrcX++, iSrcOffset++)
                            {
                                if (bWrapOverX)
                                    iSrcOffset =
                                        (iSrcX % nSrcXSize) +
                                        static_cast<GPtrDiff_t>(iSrcY) *
                                            nSrcXSize;

                                if (poWK->panUnifiedSrcValid != nullptr &&
                                    !CPLMaskGet(poWK->panUnifiedSrcValid,
                                                iSrcOffset))
                                    continue;

                                if (GWKGetPixelValue(
                                        poWK, iBand, iSrcOffset, &dfBandDensity,
                                        &dfValueRealTmp, &dfValueImagTmp) &&
                                    dfBandDensity > BAND_DENSITY_THRESHOLD)
                                {
                                    const int nVal =
                                        static_cast<int>(dfValueRealTmp);
                                    if (++panVals[nVal + nBinsOffset] > nMaxVal)
                                    {
                                        // Sum the density.
                                        // Is it the most common value so far?
                                        iMaxInd = nVal;
                                        nMaxVal = panVals[nVal + nBinsOffset];
                                    }
                                }
                            }
                        }

                        if (iMaxInd != -1)
                        {
                            dfValueReal = iMaxInd;

                            if (poWK->bApplyVerticalShift)
                            {
                                if (!std::isfinite(padfZ[iDstX]))
                                    continue;
                                // Subtract padfZ[] since the coordinate
                                // transformation is from target to source
                                dfValueReal =
                                    dfValueReal *
                                        poWK->dfMultFactorVerticalShift -
                                    padfZ[iDstX] *
                                        dfMultFactorVerticalShiftPipeline;
                            }

                            dfBandDensity = 1;
                            bHasFoundDensity = true;
                        }
                    }
                }  // GRA_Mode.
                else if (nAlgo == GWKAOM_Max)
                // poWK->eResample == GRA_Max.
                {
                    bool bFoundValid = false;
                    double dfTotalReal = std::numeric_limits<double>::lowest();
                    // This code adapted from nAlgo 1 method, GRA_Average.
                    for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                    {
                        iSrcOffset = iSrcXMin +
                                     static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                        for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                             iSrcX++, iSrcOffset++)
                        {
                            if (bWrapOverX)
                                iSrcOffset =
                                    (iSrcX % nSrcXSize) +
                                    static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

                            if (poWK->panUnifiedSrcValid != nullptr &&
                                !CPLMaskGet(poWK->panUnifiedSrcValid,
                                            iSrcOffset))
                            {
                                continue;
                            }

                            // Returns pixel value if it is not no data.
                            if (GWKGetPixelValue(
                                    poWK, iBand, iSrcOffset, &dfBandDensity,
                                    &dfValueRealTmp, &dfValueImagTmp) &&
                                dfBandDensity > BAND_DENSITY_THRESHOLD)
                            {
                                bFoundValid = true;
                                if (dfTotalReal < dfValueRealTmp)
                                {
                                    dfTotalReal = dfValueRealTmp;
                                }
                            }
                        }
                    }

                    if (bFoundValid)
                    {
                        dfValueReal = dfTotalReal;

                        if (poWK->bApplyVerticalShift)
                        {
                            if (!std::isfinite(padfZ[iDstX]))
                                continue;
                            // Subtract padfZ[] since the coordinate
                            // transformation is from target to source
                            dfValueReal =
                                dfValueReal * poWK->dfMultFactorVerticalShift -
                                padfZ[iDstX] *
                                    dfMultFactorVerticalShiftPipeline;
                        }

                        dfBandDensity = 1;
                        bHasFoundDensity = true;
                    }
                }  // GRA_Max.
                else if (nAlgo == GWKAOM_Min)
                // poWK->eResample == GRA_Min.
                {
                    bool bFoundValid = false;
                    double dfTotalReal = std::numeric_limits<double>::max();
                    // This code adapted from nAlgo 1 method, GRA_Average.
                    for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                    {
                        iSrcOffset = iSrcXMin +
                                     static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                        for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                             iSrcX++, iSrcOffset++)
                        {
                            if (bWrapOverX)
                                iSrcOffset =
                                    (iSrcX % nSrcXSize) +
                                    static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

                            if (poWK->panUnifiedSrcValid != nullptr &&
                                !CPLMaskGet(poWK->panUnifiedSrcValid,
                                            iSrcOffset))
                            {
                                continue;
                            }

                            // Returns pixel value if it is not no data.
                            if (GWKGetPixelValue(
                                    poWK, iBand, iSrcOffset, &dfBandDensity,
                                    &dfValueRealTmp, &dfValueImagTmp) &&
                                dfBandDensity > BAND_DENSITY_THRESHOLD)
                            {
                                bFoundValid = true;
                                if (dfTotalReal > dfValueRealTmp)
                                {
                                    dfTotalReal = dfValueRealTmp;
                                }
                            }
                        }
                    }

                    if (bFoundValid)
                    {
                        dfValueReal = dfTotalReal;

                        if (poWK->bApplyVerticalShift)
                        {
                            if (!std::isfinite(padfZ[iDstX]))
                                continue;
                            // Subtract padfZ[] since the coordinate
                            // transformation is from target to source
                            dfValueReal =
                                dfValueReal * poWK->dfMultFactorVerticalShift -
                                padfZ[iDstX] *
                                    dfMultFactorVerticalShiftPipeline;
                        }

                        dfBandDensity = 1;
                        bHasFoundDensity = true;
                    }
                }  // GRA_Min.
                else if (nAlgo == GWKAOM_Quant)
                // poWK->eResample == GRA_Med | GRA_Q1 | GRA_Q3.
                {
                    bool bFoundValid = false;
                    std::vector<double> dfRealValuesTmp;

                    // This code adapted from nAlgo 1 method, GRA_Average.
                    for (int iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++)
                    {
                        iSrcOffset = iSrcXMin +
                                     static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;
                        for (int iSrcX = iSrcXMin; iSrcX < iSrcXMax;
                             iSrcX++, iSrcOffset++)
                        {
                            if (bWrapOverX)
                                iSrcOffset =
                                    (iSrcX % nSrcXSize) +
                                    static_cast<GPtrDiff_t>(iSrcY) * nSrcXSize;

                            if (poWK->panUnifiedSrcValid != nullptr &&
                                !CPLMaskGet(poWK->panUnifiedSrcValid,
                                            iSrcOffset))
                            {
                                continue;
                            }

                            // Returns pixel value if it is not no data.
                            if (GWKGetPixelValue(
                                    poWK, iBand, iSrcOffset, &dfBandDensity,
                                    &dfValueRealTmp, &dfValueImagTmp) &&
                                dfBandDensity > BAND_DENSITY_THRESHOLD)
                            {
                                bFoundValid = true;
                                dfRealValuesTmp.push_back(dfValueRealTmp);
                            }
                        }
                    }

                    if (bFoundValid)
                    {
                        std::sort(dfRealValuesTmp.begin(),
                                  dfRealValuesTmp.end());
                        int quantIdx = static_cast<int>(
                            std::ceil(quant * dfRealValuesTmp.size() - 1));
                        dfValueReal = dfRealValuesTmp[quantIdx];

                        if (poWK->bApplyVerticalShift)
                        {
                            if (!std::isfinite(padfZ[iDstX]))
                                continue;
                            // Subtract padfZ[] since the coordinate
                            // transformation is from target to source
                            dfValueReal =
                                dfValueReal * poWK->dfMultFactorVerticalShift -
                                padfZ[iDstX] *
                                    dfMultFactorVerticalShiftPipeline;
                        }

                        dfBandDensity = 1;
                        bHasFoundDensity = true;
                        dfRealValuesTmp.clear();
                    }
                }  // Quantile.

                /* --------------------------------------------------------------------
                 */
                /*      We have a computed value from the source.  Now apply it
                 * to      */
                /*      the destination pixel. */
                /* --------------------------------------------------------------------
                 */
                if (bHasFoundDensity)
                {
                    // TODO: Should we compute dfBandDensity in fct of
                    // nCount/nCount2, or use as a threshold to set the dest
                    // value?
                    // dfBandDensity = (float) nCount / nCount2;
                    // if( (float) nCount / nCount2 > 0.1 )
                    // or fix gdalwarp crop_to_cutline to crop partially
                    // overlapping pixels.
                    GWKSetPixelValue(poWK, iBand, iDstOffset, dfBandDensity,
                                     dfValueReal, dfValueImag);
                }
            }

            if (!bHasFoundDensity)
                continue;

            /* --------------------------------------------------------------------
             */
            /*      Update destination density/validity masks. */
            /* --------------------------------------------------------------------
             */
            GWKOverlayDensity(poWK, iDstOffset, dfDensity);

            if (poWK->panDstValid != nullptr)
            {
                CPLMaskSet(poWK->panDstValid, iDstOffset);
            }
        } /* Next iDstX */

        /* --------------------------------------------------------------------
         */
        /*      Report progress to the user, and optionally cancel out. */
        /* --------------------------------------------------------------------
         */
        if (psJob->pfnProgress && psJob->pfnProgress(psJob))
            break;
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup and return.                                             */
    /* -------------------------------------------------------------------- */
    CPLFree(padfX);
    CPLFree(padfY);
    CPLFree(padfZ);
    CPLFree(padfX2);
    CPLFree(padfY2);
    CPLFree(padfZ2);
    CPLFree(pabSuccess);
    CPLFree(pabSuccess2);
    VSIFree(panVals);
    VSIFree(pafRealVals);
    VSIFree(panRealSums);
    if (bIsComplex)
    {
        VSIFree(pafImagVals);
        VSIFree(panImagSums);
    }
}

/************************************************************************/
/*                         getOrientation()                             */
/************************************************************************/

typedef std::pair<double, double> XYPair;

// Returns 1 whether (p1,p2,p3) is clockwise oriented,
// -1 if it is counter-clockwise oriented,
// or 0 if it is colinear.
static int getOrientation(const XYPair &p1, const XYPair &p2, const XYPair &p3)
{
    const double p1x = p1.first;
    const double p1y = p1.second;
    const double p2x = p2.first;
    const double p2y = p2.second;
    const double p3x = p3.first;
    const double p3y = p3.second;
    const double val = (p2y - p1y) * (p3x - p2x) - (p2x - p1x) * (p3y - p2y);
    if (std::abs(val) < 1e-20)
        return 0;
    else if (val > 0)
        return 1;
    else
        return -1;
}

/************************************************************************/
/*                          isConvex()                                  */
/************************************************************************/

typedef std::vector<XYPair> XYPoly;

// poly must be closed
static bool isConvex(const XYPoly &poly)
{
    const size_t n = poly.size();
    size_t i = 0;
    int last_orientation = getOrientation(poly[i], poly[i + 1], poly[i + 2]);
    ++i;
    for (; i < n - 2; ++i)
    {
        const int orientation =
            getOrientation(poly[i], poly[i + 1], poly[i + 2]);
        if (orientation != 0)
        {
            if (last_orientation == 0)
                last_orientation = orientation;
            else if (orientation != last_orientation)
                return false;
        }
    }
    return true;
}

/************************************************************************/
/*                     pointIntersectsConvexPoly()                      */
/************************************************************************/

// Returns whether xy intersects poly, that must be closed and convex.
static bool pointIntersectsConvexPoly(const XYPair &xy, const XYPoly &poly)
{
    const size_t n = poly.size();
    double dx1 = xy.first - poly[0].first;
    double dy1 = xy.second - poly[0].second;
    double dx2 = poly[1].first - poly[0].first;
    double dy2 = poly[1].second - poly[0].second;
    double prevCrossProduct = dx1 * dy2 - dx2 * dy1;

    // Check if the point remains on the same side (left/right) of all edges
    for (size_t i = 2; i < n; i++)
    {
        dx1 = xy.first - poly[i - 1].first;
        dy1 = xy.second - poly[i - 1].second;

        dx2 = poly[i].first - poly[i - 1].first;
        dy2 = poly[i].second - poly[i - 1].second;

        double crossProduct = dx1 * dy2 - dx2 * dy1;
        if (std::abs(prevCrossProduct) < 1e-20)
            prevCrossProduct = crossProduct;
        else if (prevCrossProduct * crossProduct < 0)
            return false;
    }

    return true;
}

/************************************************************************/
/*                     getIntersection()                                */
/************************************************************************/

/* Returns intersection of [p1,p2] with [p3,p4], if
 * it is a single point, and the 2 segments are not colinear.
 */
static bool getIntersection(const XYPair &p1, const XYPair &p2,
                            const XYPair &p3, const XYPair &p4, XYPair &xy)
{
    const double x1 = p1.first;
    const double y1 = p1.second;
    const double x2 = p2.first;
    const double y2 = p2.second;
    const double x3 = p3.first;
    const double y3 = p3.second;
    const double x4 = p4.first;
    const double y4 = p4.second;
    const double t_num = (x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4);
    const double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (t_num * denom < 0 || std::abs(t_num) > std::abs(denom) || denom == 0)
        return false;

    const double u_num = (x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2);
    if (u_num * denom < 0 || std::abs(u_num) > std::abs(denom))
        return false;

    const double t = t_num / denom;
    xy.first = x1 + t * (x2 - x1);
    xy.second = y1 + t * (y2 - y1);
    return true;
}

/************************************************************************/
/*                     getConvexPolyIntersection()                      */
/************************************************************************/

// poly1 and poly2 must be closed and convex.
// The returned intersection will not necessary be closed.
static void getConvexPolyIntersection(const XYPoly &poly1, const XYPoly &poly2,
                                      XYPoly &intersection)
{
    intersection.clear();

    // Add all points of poly1 inside poly2
    for (size_t i = 0; i < poly1.size() - 1; ++i)
    {
        if (pointIntersectsConvexPoly(poly1[i], poly2))
            intersection.push_back(poly1[i]);
    }
    if (intersection.size() == poly1.size() - 1)
    {
        // poly1 is inside poly2
        return;
    }

    // Add all points of poly2 inside poly1
    for (size_t i = 0; i < poly2.size() - 1; ++i)
    {
        if (pointIntersectsConvexPoly(poly2[i], poly1))
            intersection.push_back(poly2[i]);
    }

    // Compute the intersection of all edges of both polygons
    XYPair xy;
    for (size_t i1 = 0; i1 < poly1.size() - 1; ++i1)
    {
        for (size_t i2 = 0; i2 < poly2.size() - 1; ++i2)
        {
            if (getIntersection(poly1[i1], poly1[i1 + 1], poly2[i2],
                                poly2[i2 + 1], xy))
            {
                intersection.push_back(xy);
            }
        }
    }

    if (intersection.empty())
        return;

    // Find lowest-left point in intersection set
    double lowest_x = std::numeric_limits<double>::max();
    double lowest_y = std::numeric_limits<double>::max();
    for (const auto &pair : intersection)
    {
        const double x = pair.first;
        const double y = pair.second;
        if (y < lowest_y || (y == lowest_y && x < lowest_x))
        {
            lowest_x = x;
            lowest_y = y;
        }
    }

    const auto sortFunc = [&](const XYPair &p1, const XYPair &p2)
    {
        const double p1x_diff = p1.first - lowest_x;
        const double p1y_diff = p1.second - lowest_y;
        const double p2x_diff = p2.first - lowest_x;
        const double p2y_diff = p2.second - lowest_y;
        if (p2y_diff == 0.0 && p1y_diff == 0.0)
        {
            if (p1x_diff >= 0)
            {
                if (p2x_diff >= 0)
                    return p1.first < p2.first;
                return true;
            }
            else
            {
                if (p2x_diff >= 0)
                    return false;
                return p1.first < p2.first;
            }
        }

        if (p2x_diff == 0.0 && p1x_diff == 0.0)
            return p1.second < p2.second;

        double tan_p1;
        if (p1x_diff == 0.0)
            tan_p1 = p1y_diff == 0.0 ? 0.0 : std::numeric_limits<double>::max();
        else
            tan_p1 = p1y_diff / p1x_diff;

        double tan_p2;
        if (p2x_diff == 0.0)
            tan_p2 = p2y_diff == 0.0 ? 0.0 : std::numeric_limits<double>::max();
        else
            tan_p2 = p2y_diff / p2x_diff;

        if (tan_p1 >= 0)
        {
            if (tan_p2 >= 0)
                return tan_p1 < tan_p2;
            else
                return true;
        }
        else
        {
            if (tan_p2 >= 0)
                return false;
            else
                return tan_p1 < tan_p2;
        }
    };

    // Sort points by increasing atan2(y-lowest_y, x-lowest_x) to form a convex
    // hull
    std::sort(intersection.begin(), intersection.end(), sortFunc);

    // Remove duplicated points
    size_t j = 1;
    for (size_t i = 1; i < intersection.size(); ++i)
    {
        if (intersection[i] != intersection[i - 1])
        {
            if (j < i)
                intersection[j] = intersection[i];
            ++j;
        }
    }
    intersection.resize(j);
}

/************************************************************************/
/*                            getArea()                                 */
/************************************************************************/

// poly may or may not be closed.
static double getArea(const XYPoly &poly)
{
    // CPLAssert(poly.size() >= 2);
    const size_t nPointCount = poly.size();
    double dfAreaSum =
        poly[0].first * (poly[1].second - poly[nPointCount - 1].second);

    for (size_t i = 1; i < nPointCount - 1; i++)
    {
        dfAreaSum += poly[i].first * (poly[i + 1].second - poly[i - 1].second);
    }

    dfAreaSum += poly[nPointCount - 1].first *
                 (poly[0].second - poly[nPointCount - 2].second);

    return 0.5 * std::fabs(dfAreaSum);
}

/************************************************************************/
/*                           GWKSumPreserving()                         */
/************************************************************************/

static void GWKSumPreservingThread(void *pData);

static CPLErr GWKSumPreserving(GDALWarpKernel *poWK)
{
    return GWKRun(poWK, "GWKSumPreserving", GWKSumPreservingThread);
}

static void GWKSumPreservingThread(void *pData)
{
    GWKJobStruct *psJob = static_cast<GWKJobStruct *>(pData);
    GDALWarpKernel *poWK = psJob->poWK;
    const int iYMin = psJob->iYMin;
    const int iYMax = psJob->iYMax;
    const bool bIsAffineNoRotation =
        GDALTransformIsAffineNoRotation(poWK->pfnTransformer,
                                        poWK->pTransformerArg) &&
        // for debug/testing purposes
        CPLTestBool(
            CPLGetConfigOption("GDAL_WARP_USE_AFFINE_OPTIMIZATION", "YES"));

    const int nDstXSize = poWK->nDstXSize;
    const int nSrcXSize = poWK->nSrcXSize;
    const int nSrcYSize = poWK->nSrcYSize;

    std::vector<double> adfX0(nSrcXSize + 1);
    std::vector<double> adfY0(nSrcXSize + 1);
    std::vector<double> adfZ0(nSrcXSize + 1);
    std::vector<double> adfX1(nSrcXSize + 1);
    std::vector<double> adfY1(nSrcXSize + 1);
    std::vector<double> adfZ1(nSrcXSize + 1);
    std::vector<int> abSuccess0(nSrcXSize + 1);
    std::vector<int> abSuccess1(nSrcXSize + 1);

    CPLRectObj sGlobalBounds;
    sGlobalBounds.minx = -2 * poWK->dfXScale;
    sGlobalBounds.miny = iYMin - 2 * poWK->dfYScale;
    sGlobalBounds.maxx = nDstXSize + 2 * poWK->dfXScale;
    sGlobalBounds.maxy = iYMax + 2 * poWK->dfYScale;
    CPLQuadTree *hQuadTree = CPLQuadTreeCreate(&sGlobalBounds, nullptr);

    struct SourcePixel
    {
        int iSrcX;
        int iSrcY;

        // Coordinates of source pixel in target pixel coordinates
        double dfDstX0;
        double dfDstY0;
        double dfDstX1;
        double dfDstY1;
        double dfDstX2;
        double dfDstY2;
        double dfDstX3;
        double dfDstY3;

        // Source pixel total area (might be larger than the one described
        // by above coordinates, if the pixel was crossing the antimeridian
        // and split)
        double dfArea;
    };

    std::vector<SourcePixel> sourcePixels;

    XYPoly discontinuityLeft(5);
    XYPoly discontinuityRight(5);

    /* ==================================================================== */
    /*      First pass: transform the 4 corners of each potential           */
    /*      contributing source pixel to target pixel coordinates.          */
    /* ==================================================================== */

    // Special case for top line
    {
        int iY = 0;
        for (int iX = 0; iX <= nSrcXSize; ++iX)
        {
            adfX1[iX] = iX + poWK->nSrcXOff;
            adfY1[iX] = iY + poWK->nSrcYOff;
            adfZ1[iX] = 0;
        }

        poWK->pfnTransformer(psJob->pTransformerArg, FALSE, nSrcXSize + 1,
                             adfX1.data(), adfY1.data(), adfZ1.data(),
                             abSuccess1.data());

        for (int iX = 0; iX <= nSrcXSize; ++iX)
        {
            if (abSuccess1[iX] && !std::isfinite(adfX1[iX]))
                abSuccess1[iX] = FALSE;
            else
            {
                adfX1[iX] -= poWK->nDstXOff;
                adfY1[iX] -= poWK->nDstYOff;
            }
        }
    }

    const auto getInsideXSign = [poWK, nDstXSize](double dfX)
    {
        return dfX - poWK->nDstXOff >= -2 * poWK->dfXScale &&
                       dfX - poWK->nDstXOff <= nDstXSize + 2 * poWK->dfXScale
                   ? 1
                   : -1;
    };

    const auto FindDiscontinuity =
        [poWK, psJob, getInsideXSign](
            double dfXLeft, double dfXRight, double dfY,
            int XLeftReprojectedInsideSign, double &dfXMidReprojectedLeft,
            double &dfXMidReprojectedRight, double &dfYMidReprojected)
    {
        for (int i = 0; i < 10 && dfXRight - dfXLeft > 1e-8; ++i)
        {
            double dfXMid = (dfXLeft + dfXRight) / 2;
            double dfXMidReprojected = dfXMid;
            dfYMidReprojected = dfY;
            double dfZ = 0;
            int nSuccess = 0;
            poWK->pfnTransformer(psJob->pTransformerArg, FALSE, 1,
                                 &dfXMidReprojected, &dfYMidReprojected, &dfZ,
                                 &nSuccess);
            if (XLeftReprojectedInsideSign != getInsideXSign(dfXMidReprojected))
            {
                dfXRight = dfXMid;
                dfXMidReprojectedRight = dfXMidReprojected;
            }
            else
            {
                dfXLeft = dfXMid;
                dfXMidReprojectedLeft = dfXMidReprojected;
            }
        }
    };

    for (int iY = 0; iY < nSrcYSize; ++iY)
    {
        std::swap(adfX0, adfX1);
        std::swap(adfY0, adfY1);
        std::swap(adfZ0, adfZ1);
        std::swap(abSuccess0, abSuccess1);

        for (int iX = 0; iX <= nSrcXSize; ++iX)
        {
            adfX1[iX] = iX + poWK->nSrcXOff;
            adfY1[iX] = iY + 1 + poWK->nSrcYOff;
            adfZ1[iX] = 0;
        }

        poWK->pfnTransformer(psJob->pTransformerArg, FALSE, nSrcXSize + 1,
                             adfX1.data(), adfY1.data(), adfZ1.data(),
                             abSuccess1.data());

        for (int iX = 0; iX <= nSrcXSize; ++iX)
        {
            if (abSuccess1[iX] && !std::isfinite(adfX1[iX]))
                abSuccess1[iX] = FALSE;
            else
            {
                adfX1[iX] -= poWK->nDstXOff;
                adfY1[iX] -= poWK->nDstYOff;
            }
        }

        for (int iX = 0; iX < nSrcXSize; ++iX)
        {
            if (abSuccess0[iX] && abSuccess0[iX + 1] && abSuccess1[iX] &&
                abSuccess1[iX + 1])
            {
                /* --------------------------------------------------------------------
                 */
                /*      Do not try to apply transparent source pixels to the
                 * destination.*/
                /* --------------------------------------------------------------------
                 */
                const auto iSrcOffset =
                    iX + static_cast<GPtrDiff_t>(iY) * nSrcXSize;
                if (poWK->panUnifiedSrcValid != nullptr &&
                    !CPLMaskGet(poWK->panUnifiedSrcValid, iSrcOffset))
                {
                    continue;
                }

                if (poWK->pafUnifiedSrcDensity != nullptr)
                {
                    if (poWK->pafUnifiedSrcDensity[iSrcOffset] <
                        SRC_DENSITY_THRESHOLD)
                        continue;
                }

                SourcePixel sp;
                sp.dfArea = 0;
                sp.dfDstX0 = adfX0[iX];
                sp.dfDstY0 = adfY0[iX];
                sp.dfDstX1 = adfX0[iX + 1];
                sp.dfDstY1 = adfY0[iX + 1];
                sp.dfDstX2 = adfX1[iX + 1];
                sp.dfDstY2 = adfY1[iX + 1];
                sp.dfDstX3 = adfX1[iX];
                sp.dfDstY3 = adfY1[iX];

                // Detect pixel that likely cross the anti-meridian and
                // introduce a discontinuity when reprojected.

                if (getInsideXSign(adfX0[iX]) !=
                        getInsideXSign(adfX0[iX + 1]) &&
                    getInsideXSign(adfX0[iX]) == getInsideXSign(adfX1[iX]) &&
                    getInsideXSign(adfX0[iX + 1]) ==
                        getInsideXSign(adfX1[iX + 1]) &&
                    (adfY1[iX] - adfY0[iX]) * (adfY1[iX + 1] - adfY0[iX + 1]) >
                        0)
                {
                    double dfXMidReprojectedLeftTop = 0;
                    double dfXMidReprojectedRightTop = 0;
                    double dfYMidReprojectedTop = 0;
                    FindDiscontinuity(
                        iX + poWK->nSrcXOff, iX + poWK->nSrcXOff + 1,
                        iY + poWK->nSrcYOff, getInsideXSign(adfX0[iX]),
                        dfXMidReprojectedLeftTop, dfXMidReprojectedRightTop,
                        dfYMidReprojectedTop);
                    double dfXMidReprojectedLeftBottom = 0;
                    double dfXMidReprojectedRightBottom = 0;
                    double dfYMidReprojectedBottom = 0;
                    FindDiscontinuity(
                        iX + poWK->nSrcXOff, iX + poWK->nSrcXOff + 1,
                        iY + poWK->nSrcYOff + 1, getInsideXSign(adfX1[iX]),
                        dfXMidReprojectedLeftBottom,
                        dfXMidReprojectedRightBottom, dfYMidReprojectedBottom);

                    discontinuityLeft[0] = XYPair(adfX0[iX], adfY0[iX]);
                    discontinuityLeft[1] =
                        XYPair(dfXMidReprojectedLeftTop, dfYMidReprojectedTop);
                    discontinuityLeft[2] = XYPair(dfXMidReprojectedLeftBottom,
                                                  dfYMidReprojectedBottom);
                    discontinuityLeft[3] = XYPair(adfX1[iX], adfY1[iX]);
                    discontinuityLeft[4] = XYPair(adfX0[iX], adfY0[iX]);

                    discontinuityRight[0] =
                        XYPair(adfX0[iX + 1], adfY0[iX + 1]);
                    discontinuityRight[1] =
                        XYPair(dfXMidReprojectedRightTop, dfYMidReprojectedTop);
                    discontinuityRight[2] = XYPair(dfXMidReprojectedRightBottom,
                                                   dfYMidReprojectedBottom);
                    discontinuityRight[3] =
                        XYPair(adfX1[iX + 1], adfY1[iX + 1]);
                    discontinuityRight[4] =
                        XYPair(adfX0[iX + 1], adfY0[iX + 1]);

                    sp.dfArea = getArea(discontinuityLeft) +
                                getArea(discontinuityRight);
                    if (getInsideXSign(adfX0[iX]) >= 1)
                    {
                        sp.dfDstX1 = dfXMidReprojectedLeftTop;
                        sp.dfDstY1 = dfYMidReprojectedTop;
                        sp.dfDstX2 = dfXMidReprojectedLeftBottom;
                        sp.dfDstY2 = dfYMidReprojectedBottom;
                    }
                    else
                    {
                        sp.dfDstX0 = dfXMidReprojectedRightTop;
                        sp.dfDstY0 = dfYMidReprojectedTop;
                        sp.dfDstX3 = dfXMidReprojectedRightBottom;
                        sp.dfDstY3 = dfYMidReprojectedBottom;
                    }
                }

                // Bounding box of source pixel (expressed in target pixel
                // coordinates)
                CPLRectObj sRect;
                sRect.minx = std::min(std::min(sp.dfDstX0, sp.dfDstX1),
                                      std::min(sp.dfDstX2, sp.dfDstX3));
                sRect.miny = std::min(std::min(sp.dfDstY0, sp.dfDstY1),
                                      std::min(sp.dfDstY2, sp.dfDstY3));
                sRect.maxx = std::max(std::max(sp.dfDstX0, sp.dfDstX1),
                                      std::max(sp.dfDstX2, sp.dfDstX3));
                sRect.maxy = std::max(std::max(sp.dfDstY0, sp.dfDstY1),
                                      std::max(sp.dfDstY2, sp.dfDstY3));
                if (!(sRect.minx < nDstXSize && sRect.maxx > 0 &&
                      sRect.miny < iYMax && sRect.maxy > iYMin))
                {
                    continue;
                }

                sp.iSrcX = iX;
                sp.iSrcY = iY;

                if (!bIsAffineNoRotation)
                {
                    // Check polygon validity (no self-crossing)
                    XYPair xy;
                    if (getIntersection(XYPair(sp.dfDstX0, sp.dfDstY0),
                                        XYPair(sp.dfDstX1, sp.dfDstY1),
                                        XYPair(sp.dfDstX2, sp.dfDstY2),
                                        XYPair(sp.dfDstX3, sp.dfDstY3), xy) ||
                        getIntersection(XYPair(sp.dfDstX1, sp.dfDstY1),
                                        XYPair(sp.dfDstX2, sp.dfDstY2),
                                        XYPair(sp.dfDstX0, sp.dfDstY0),
                                        XYPair(sp.dfDstX3, sp.dfDstY3), xy))
                    {
                        continue;
                    }
                }

                CPLQuadTreeInsertWithBounds(
                    hQuadTree,
                    reinterpret_cast<void *>(
                        static_cast<uintptr_t>(sourcePixels.size())),
                    &sRect);

                sourcePixels.push_back(sp);
            }
        }
    }

    std::vector<double> adfRealValue(poWK->nBands);
    std::vector<double> adfImagValue(poWK->nBands);
    std::vector<double> adfBandDensity(poWK->nBands);
    std::vector<double> adfWeight(poWK->nBands);

#ifdef CHECK_SUM_WITH_GEOS
    auto hGEOSContext = OGRGeometry::createGEOSContext();
    auto seq1 = GEOSCoordSeq_create_r(hGEOSContext, 5, 2);
    GEOSCoordSeq_setXY_r(hGEOSContext, seq1, 0, 0.0, 0.0);
    GEOSCoordSeq_setXY_r(hGEOSContext, seq1, 1, 1.0, 0.0);
    GEOSCoordSeq_setXY_r(hGEOSContext, seq1, 2, 1.0, 1.0);
    GEOSCoordSeq_setXY_r(hGEOSContext, seq1, 3, 0.0, 1.0);
    GEOSCoordSeq_setXY_r(hGEOSContext, seq1, 4, 0.0, 0.0);
    auto hLR1 = GEOSGeom_createLinearRing_r(hGEOSContext, seq1);
    auto hP1 = GEOSGeom_createPolygon_r(hGEOSContext, hLR1, nullptr, 0);

    auto seq2 = GEOSCoordSeq_create_r(hGEOSContext, 5, 2);
    auto hLR2 = GEOSGeom_createLinearRing_r(hGEOSContext, seq2);
    auto hP2 = GEOSGeom_createPolygon_r(hGEOSContext, hLR2, nullptr, 0);
#endif

    const XYPoly xy1{
        {0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {0.0, 0.0}};
    XYPoly xy2(5);
    XYPoly xy2_triangle(4);
    XYPoly intersection;

    /* ==================================================================== */
    /*      Loop over output lines.                                         */
    /* ==================================================================== */
    for (int iDstY = iYMin; iDstY < iYMax; iDstY++)
    {
        CPLRectObj sRect;
        sRect.miny = iDstY;
        sRect.maxy = iDstY + 1;

        /* ====================================================================
         */
        /*      Loop over pixels in output scanline. */
        /* ====================================================================
         */
        for (int iDstX = 0; iDstX < nDstXSize; iDstX++)
        {
            sRect.minx = iDstX;
            sRect.maxx = iDstX + 1;
            int nSourcePixels = 0;
            void **pahSourcePixel =
                CPLQuadTreeSearch(hQuadTree, &sRect, &nSourcePixels);
            if (nSourcePixels == 0)
            {
                CPLFree(pahSourcePixel);
                continue;
            }

            std::fill(adfRealValue.begin(), adfRealValue.end(), 0);
            std::fill(adfImagValue.begin(), adfImagValue.end(), 0);
            std::fill(adfBandDensity.begin(), adfBandDensity.end(), 0);
            std::fill(adfWeight.begin(), adfWeight.end(), 0);
            double dfDensity = 0;
            double dfTotalWeight = 0;

            /* ====================================================================
             */
            /*          Iterate over each contributing source pixel to add its
             */
            /*          value weighed by the ratio of the area of its
             * intersection  */
            /*          with the target pixel divided by the area of the source
             */
            /*          pixel. */
            /* ====================================================================
             */
            for (int i = 0; i < nSourcePixels; ++i)
            {
                const int iSourcePixel = static_cast<int>(
                    reinterpret_cast<uintptr_t>(pahSourcePixel[i]));
                auto &sp = sourcePixels[iSourcePixel];

                double dfWeight = 0.0;
                if (bIsAffineNoRotation)
                {
                    // Optimization since the source pixel is a rectangle in
                    // target pixel coordinates
                    double dfSrcMinX = std::min(sp.dfDstX0, sp.dfDstX2);
                    double dfSrcMaxX = std::max(sp.dfDstX0, sp.dfDstX2);
                    double dfSrcMinY = std::min(sp.dfDstY0, sp.dfDstY2);
                    double dfSrcMaxY = std::max(sp.dfDstY0, sp.dfDstY2);
                    double dfIntersMinX = std::max<double>(dfSrcMinX, iDstX);
                    double dfIntersMaxX = std::min(dfSrcMaxX, iDstX + 1.0);
                    double dfIntersMinY = std::max<double>(dfSrcMinY, iDstY);
                    double dfIntersMaxY = std::min(dfSrcMaxY, iDstY + 1.0);
                    dfWeight =
                        ((dfIntersMaxX - dfIntersMinX) *
                         (dfIntersMaxY - dfIntersMinY)) /
                        ((dfSrcMaxX - dfSrcMinX) * (dfSrcMaxY - dfSrcMinY));
                }
                else
                {
                    // Compute the polygon of the source pixel in target pixel
                    // coordinates, and shifted to the target pixel (unit square
                    // coordinates)

                    xy2[0] = {sp.dfDstX0 - iDstX, sp.dfDstY0 - iDstY};
                    xy2[1] = {sp.dfDstX1 - iDstX, sp.dfDstY1 - iDstY};
                    xy2[2] = {sp.dfDstX2 - iDstX, sp.dfDstY2 - iDstY};
                    xy2[3] = {sp.dfDstX3 - iDstX, sp.dfDstY3 - iDstY};
                    xy2[4] = {sp.dfDstX0 - iDstX, sp.dfDstY0 - iDstY};

                    if (isConvex(xy2))
                    {
                        getConvexPolyIntersection(xy1, xy2, intersection);
                        if (intersection.size() >= 3)
                        {
                            dfWeight = getArea(intersection);
                        }
                    }
                    else
                    {
                        // Split xy2 into 2 triangles.
                        xy2_triangle[0] = xy2[0];
                        xy2_triangle[1] = xy2[1];
                        xy2_triangle[2] = xy2[2];
                        xy2_triangle[3] = xy2[0];
                        getConvexPolyIntersection(xy1, xy2_triangle,
                                                  intersection);
                        if (intersection.size() >= 3)
                        {
                            dfWeight = getArea(intersection);
                        }

                        xy2_triangle[1] = xy2[2];
                        xy2_triangle[2] = xy2[3];
                        getConvexPolyIntersection(xy1, xy2_triangle,
                                                  intersection);
                        if (intersection.size() >= 3)
                        {
                            dfWeight += getArea(intersection);
                        }
                    }
                    if (dfWeight > 0.0)
                    {
                        if (sp.dfArea == 0)
                            sp.dfArea = getArea(xy2);
                        dfWeight /= sp.dfArea;
                    }

#ifdef CHECK_SUM_WITH_GEOS
                    GEOSCoordSeq_setXY_r(hGEOSContext, seq2, 0,
                                         sp.dfDstX0 - iDstX,
                                         sp.dfDstY0 - iDstY);
                    GEOSCoordSeq_setXY_r(hGEOSContext, seq2, 1,
                                         sp.dfDstX1 - iDstX,
                                         sp.dfDstY1 - iDstY);
                    GEOSCoordSeq_setXY_r(hGEOSContext, seq2, 2,
                                         sp.dfDstX2 - iDstX,
                                         sp.dfDstY2 - iDstY);
                    GEOSCoordSeq_setXY_r(hGEOSContext, seq2, 3,
                                         sp.dfDstX3 - iDstX,
                                         sp.dfDstY3 - iDstY);
                    GEOSCoordSeq_setXY_r(hGEOSContext, seq2, 4,
                                         sp.dfDstX0 - iDstX,
                                         sp.dfDstY0 - iDstY);

                    double dfWeightGEOS = 0.0;
                    auto hIntersection =
                        GEOSIntersection_r(hGEOSContext, hP1, hP2);
                    if (hIntersection)
                    {
                        double dfIntersArea = 0.0;
                        if (GEOSArea_r(hGEOSContext, hIntersection,
                                       &dfIntersArea) &&
                            dfIntersArea > 0)
                        {
                            double dfSourceArea = 0.0;
                            if (GEOSArea_r(hGEOSContext, hP2, &dfSourceArea))
                            {
                                dfWeightGEOS = dfIntersArea / dfSourceArea;
                            }
                        }
                        GEOSGeom_destroy_r(hGEOSContext, hIntersection);
                    }
                    if (fabs(dfWeight - dfWeightGEOS) > 1e-5 * dfWeightGEOS)
                    {
                        /* ok */ printf("dfWeight=%f dfWeightGEOS=%f\n",
                                        dfWeight, dfWeightGEOS);
                        printf("xy2: ");  // ok
                        for (const auto &xy : xy2)
                            printf("[%f, %f], ", xy.first, xy.second);  // ok
                        printf("\n");                                   // ok
                        printf("intersection: ");                       // ok
                        for (const auto &xy : intersection)
                            printf("[%f, %f], ", xy.first, xy.second);  // ok
                        printf("\n");                                   // ok
                    }
#endif
                }
                if (dfWeight > 0.0)
                {
                    const GPtrDiff_t iSrcOffset =
                        sp.iSrcX +
                        static_cast<GPtrDiff_t>(sp.iSrcY) * nSrcXSize;
                    dfTotalWeight += dfWeight;

                    if (poWK->pafUnifiedSrcDensity != nullptr)
                    {
                        dfDensity +=
                            dfWeight * poWK->pafUnifiedSrcDensity[iSrcOffset];
                    }
                    else
                    {
                        dfDensity += dfWeight;
                    }

                    for (int iBand = 0; iBand < poWK->nBands; ++iBand)
                    {
                        // Returns pixel value if it is not no data.
                        double dfBandDensity;
                        double dfRealValue;
                        double dfImagValue;
                        if (!(GWKGetPixelValue(poWK, iBand, iSrcOffset,
                                               &dfBandDensity, &dfRealValue,
                                               &dfImagValue) &&
                              dfBandDensity > BAND_DENSITY_THRESHOLD))
                        {
                            continue;
                        }

                        adfRealValue[iBand] += dfRealValue * dfWeight;
                        adfImagValue[iBand] += dfImagValue * dfWeight;
                        adfBandDensity[iBand] += dfBandDensity * dfWeight;
                        adfWeight[iBand] += dfWeight;
                    }
                }
            }

            CPLFree(pahSourcePixel);

            /* --------------------------------------------------------------------
             */
            /*          Update destination pixel value. */
            /* --------------------------------------------------------------------
             */
            bool bHasFoundDensity = false;
            const GPtrDiff_t iDstOffset =
                iDstX + static_cast<GPtrDiff_t>(iDstY) * nDstXSize;
            for (int iBand = 0; iBand < poWK->nBands; ++iBand)
            {
                if (adfWeight[iBand] > 0)
                {
                    const double dfBandDensity =
                        adfBandDensity[iBand] / adfWeight[iBand];
                    if (dfBandDensity > BAND_DENSITY_THRESHOLD)
                    {
                        bHasFoundDensity = true;
                        GWKSetPixelValue(poWK, iBand, iDstOffset, dfBandDensity,
                                         adfRealValue[iBand],
                                         adfImagValue[iBand]);
                    }
                }
            }

            if (!bHasFoundDensity)
                continue;

            /* --------------------------------------------------------------------
             */
            /*          Update destination density/validity masks. */
            /* --------------------------------------------------------------------
             */
            GWKOverlayDensity(poWK, iDstOffset, dfDensity / dfTotalWeight);

            if (poWK->panDstValid != nullptr)
            {
                CPLMaskSet(poWK->panDstValid, iDstOffset);
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Report progress to the user, and optionally cancel out. */
        /* --------------------------------------------------------------------
         */
        if (psJob->pfnProgress && psJob->pfnProgress(psJob))
            break;
    }

#ifdef CHECK_SUM_WITH_GEOS
    GEOSGeom_destroy_r(hGEOSContext, hP1);
    GEOSGeom_destroy_r(hGEOSContext, hP2);
    OGRGeometry::freeGEOSContext(hGEOSContext);
#endif
    CPLQuadTreeDestroy(hQuadTree);
}
