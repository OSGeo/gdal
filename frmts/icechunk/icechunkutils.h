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

#ifndef ICECHUNKUTILS_H
#define ICECHUNKUTILS_H

#include "cpl_vsi_virtual.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace gdal::icechunk
{
#ifdef DEBUG
constexpr bool IS_DEBUG_BUILD = true;
#else
constexpr bool IS_DEBUG_BUILD = false;
#endif

void VSIInstallIcechunkFileSystem();
void VSIIcechunkFileSystemClearCaches();

std::string GetFilenameFromDatasetName(const std::string &osDatasetName,
                                       std::string &osBranchName,
                                       std::string &osTagName,
                                       bool &ignoreTimestampEtag);

std::pair<std::unique_ptr<unsigned char, VSIFreeReleaser>, size_t>
DecompressFile(const char *pszFilename, VSIVirtualHandle *poFile,
               int nExpectedFileType, int *pnVersion = nullptr);

std::string CrockfordBase32Encode(const uint8_t *data, size_t size);

template <class T> static std::string CrockfordBase32Encode(const T &buffer)
{
    return CrockfordBase32Encode(buffer.data(), buffer.size());
}
}  // namespace gdal::icechunk

#endif
