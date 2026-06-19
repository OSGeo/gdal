/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Icechunk driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef ICECHUNKDRIVERCORE_H
#define ICECHUNKDRIVERCORE_H

#include "gdal_priv.h"

constexpr const char *FS_PREFIX = "/vsiicechunk/";

constexpr const char *DRIVER_NAME = "Icechunk";
constexpr const char *ICECHUNK_PREFIX = "ICECHUNK:";

// cf https://icechunk.io/en/stable/reference/spec/#node-paths
constexpr const unsigned char abySIG[] = "ICE"
                                         "\xF0"
                                         "\x9F"
                                         "\xA7"
                                         "\x8A"
                                         "CHUNK";
constexpr int SIG_SIZE = static_cast<int>(sizeof(abySIG)) - 1;
constexpr int IMPLEMENTATION_NAME_SIZE = 24;
constexpr int SPEC_VERSION_SIZE = 1;
constexpr int FILE_TYPE_SIZE = 1;
constexpr int COMPRESSION_ALGO_SIZE = 1;
constexpr int HEADER_SIZE = SIG_SIZE + IMPLEMENTATION_NAME_SIZE +
                            SPEC_VERSION_SIZE + FILE_TYPE_SIZE +
                            COMPRESSION_ALGO_SIZE;

constexpr unsigned char FILE_TYPE_SNAPSHOT = 1;
constexpr unsigned char FILE_TYPE_MANIFEST = 2;
constexpr unsigned char FILE_TYPE_TRANSACTION_LOG = 4;
constexpr unsigned char FILE_TYPE_REPO_INFO = 6;

constexpr unsigned char COMPRESSION_ALGO_NONE = 0;
constexpr unsigned char COMPRESSION_ALGO_ZSTD = 1;

constexpr const char *LIST_BRANCHES = "list-branches";
constexpr const char *LIST_TAGS = "list-tags";

#define IcechunkDriverIdentify PLUGIN_SYMBOL_NAME(IcechunkDriverIdentify)
int IcechunkDriverIdentify(GDALOpenInfo *poOpenInfo);

#define IcechunkDriverSetCommonMetadata                                        \
    PLUGIN_SYMBOL_NAME(IcechunkDriverSetCommonMetadata)
void IcechunkDriverSetCommonMetadata(GDALDriver *poDriver);

#endif
