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

#include "cpl_worker_thread_pool.h"

CPLWorkerThreadPool CPL_DLL *GDALGetGlobalThreadPool(int nThreads);

void GDALDestroyGlobalThreadPool();

#endif  // GDAL_THREAD_POOL_H
