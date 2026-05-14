/**********************************************************************
 *
 * Project:  GDAL
 * Purpose:  Global thread pool
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 **********************************************************************
 * Copyright (c) 2020, Even Rouault, <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_worker_thread_pool.h"
#include "gdal_thread_pool.h"

#include <algorithm>
#include <mutex>

// For unclear reasons, attempts at making this a std::unique_ptr<>, even
// through a GetCompressThreadPool() method like GetMutexThreadPool(), lead
// to "ctest -R autotest_alg" (and other autotest components as well)
// to hang forever once the tests have terminated.
static CPLWorkerThreadPool *gpoCompressThreadPool = nullptr;

static std::mutex &GetMutexThreadPool()
{
    static std::mutex gMutexThreadPool;
    return gMutexThreadPool;
}

CPLWorkerThreadPool *GDALGetGlobalThreadPool(int nThreads)
{
    std::lock_guard oGuard(GetMutexThreadPool());
    if (gpoCompressThreadPool == nullptr)
    {
        gpoCompressThreadPool = new CPLWorkerThreadPool();
        if (!gpoCompressThreadPool->Setup(nThreads, nullptr, nullptr, false))
        {
            delete gpoCompressThreadPool;
            gpoCompressThreadPool = nullptr;
        }
    }
    else if (nThreads > gpoCompressThreadPool->GetThreadCount())
    {
        // Increase size of thread pool
        gpoCompressThreadPool->Setup(nThreads, nullptr, nullptr, false);
    }
    return gpoCompressThreadPool;
}

void GDALDestroyGlobalThreadPool()
{
    std::lock_guard oGuard(GetMutexThreadPool());
    delete gpoCompressThreadPool;
    gpoCompressThreadPool = nullptr;
}

/************************************************************************/
/*                         GDALGetNumThreads()                          */
/************************************************************************/

/** Return the number of threads to use, taking into account the
 * GDAL_NUM_THREADS configuration option.
 *
 * @param nMaxVal Maximum number of threads, or -1 if none
 * @param bDefaultAllCPUs Whether the default value should be CPLGetNumCPUs()
 * @param[out] ppszValue Pointer to a string to set the string value used, or
 *                       nullptr if not needed.
 * @param[out] pbOK Pointer to a boolean indicating if the option value was
 *                  correct, or nullptr if not needed.
 *
 * @return value between 1 and std::max(1, nMaxVal)
 *
 * @since 3.13
 */
int GDALGetNumThreads(int nMaxVal, bool bDefaultAllCPUs, const char **ppszValue,
                      bool *pbOK)
{
    return GDALGetNumThreads(nullptr, nullptr, nMaxVal, bDefaultAllCPUs,
                             ppszValue, pbOK);
}

/************************************************************************/
/*                         GDALGetNumThreads()                          */
/************************************************************************/

/** Return the number of threads to use, taking into account first the
 * specified item in a list of options, and falling back to the
 * GDAL_NUM_THREADS configuration option.
 *
 * @param papszOptions null terminated list of options (or nullptr)
 * @param pszItemName item name in papszOptions (or nullptr)
 * @param nMaxVal Maximum number of threads, or -1 if none
 * @param bDefaultAllCPUs Whether the default value should be CPLGetNumCPUs()
 * @param[out] ppszValue Pointer to a string to set the string value used, or
 *                       nullptr if not needed.
 * @param[out] pbOK Pointer to a boolean indicating if the option value was
 *                  correct, or nullptr if not needed.
 *
 * @return value between 1 and std::max(1, nMaxVal)
 *
 * @since 3.13
 */
int GDALGetNumThreads(CSLConstList papszOptions, const char *pszItemName,
                      int nMaxVal, bool bDefaultAllCPUs, const char **ppszValue,
                      bool *pbOK)
{
    const char *pszNumThreads = nullptr;
    if (papszOptions && pszItemName)
        pszNumThreads = CSLFetchNameValue(papszOptions, pszItemName);
    return GDALGetNumThreads(pszNumThreads, nMaxVal, bDefaultAllCPUs, ppszValue,
                             pbOK);
}

/************************************************************************/
/*                         GDALGetNumThreads()                          */
/************************************************************************/

/** Return the number of threads to use, taking into account first the
 * specified value, and if null, falling back to the
 * GDAL_NUM_THREADS configuration option.
 *
 * @param pszNumThreads Value to parse to get the number of threads. If null,
 *                      the GDAL_NUM_THREADS configuration option is used.
 * @param nMaxVal Maximum number of threads, or -1 if none
 * @param bDefaultAllCPUs Whether the default value should be CPLGetNumCPUs()
 * @param[out] ppszValue Pointer to a string to set the string value used, or
 *                       nullptr if not needed.
 * @param[out] pbOK Pointer to a boolean indicating if the option value was
 *                  correct, or nullptr if not needed.
 *
 * @return value between 1 and std::max(1, nMaxVal)
 *
 * @since 3.13
 */
int GDALGetNumThreads(const char *pszNumThreads, int nMaxVal,
                      bool bDefaultAllCPUs, const char **ppszValue, bool *pbOK)
{
    if (!pszNumThreads)
    {
        pszNumThreads = CPLGetConfigOption("GDAL_NUM_THREADS",
                                           bDefaultAllCPUs ? "ALL_CPUS" : "1");
    }
    if (ppszValue)
        *ppszValue = pszNumThreads;
    int nThreads = EQUAL(pszNumThreads, "ALL_CPUS") ? CPLGetNumCPUs()
                                                    : atoi(pszNumThreads);
    if (pbOK)
        *pbOK = EQUAL(pszNumThreads, "ALL_CPUS") ||
                CPLGetValueType(pszNumThreads) == CPL_VALUE_INTEGER;
    if (nMaxVal > 0)
        nThreads = std::min(nThreads, nMaxVal);
    return std::max(nThreads, 1);
}
