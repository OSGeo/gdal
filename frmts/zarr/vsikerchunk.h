/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef VSIKERCHUNK_H
#define VSIKERCHUNK_H

#include "cpl_progress.h"

#include <string>

// "Public" API

// SPECIFICATION:
// Cf https://fsspec.github.io/kerchunk/spec.html#version-1
constexpr const char *JSON_REF_FS_PREFIX = "/vsikerchunk_json_ref/";
constexpr const char *JSON_REF_CACHED_FS_PREFIX =
    "/vsikerchunk_json_ref_cached/";

// SPECIFICATION:
// Cf https://fsspec.github.io/kerchunk/spec.html#parquet-references
constexpr const char *PARQUET_REF_FS_PREFIX = "/vsikerchunk_parquet_ref/";

void VSIInstallKerchunkFileSystems();
void VSIKerchunkFileSystemsCleanCache();
bool VSIKerchunkConvertJSONToParquet(const char *pszSrcJSONFilename,
                                     const char *pszDstDirname,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData);

// Private API
void VSIInstallKerchunkJSONRefFileSystem();
void VSIInstallKerchunkParquetRefFileSystem();
std::string VSIKerchunkMorphURIToVSIPath(const std::string &osURI,
                                         const std::string &osRootDirname);
void VSIKerchunkParquetRefFileSystemCleanCache();

#endif
