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

#ifndef GDAL_THREAD_POOL_H
#define GDAL_THREAD_POOL_H

#include "cpl_port.h"

/** Somewhat arbitrary threshold to bound the number of threads */
const int GDAL_DEFAULT_MAX_THREAD_COUNT = 1024;

class CPLWorkerThreadPool;

CPLWorkerThreadPool CPL_DLL *GDALGetGlobalThreadPool(int nThreads);

void GDALDestroyGlobalThreadPool();

int CPL_DLL GDALGetNumThreads(int nMaxVal = -1, bool bDefaultAllCPUs = false,
                              const char **ppszValue = nullptr,
                              bool *pbOK = nullptr);

int CPL_DLL GDALGetNumThreads(const char *pszNumThreads, int nMaxVal = -1,
                              bool bDefaultAllCPUs = false,
                              const char **ppszValue = nullptr,
                              bool *pbOK = nullptr);

int CPL_DLL GDALGetNumThreads(CSLConstList papszOptions,
                              const char *pszItemName = "NUM_THREADS",
                              int nMaxVal = -1, bool bDefaultAllCPUs = false,
                              const char **ppszValue = nullptr,
                              bool *pbOK = nullptr);

#endif  // GDAL_THREAD_POOL_H
