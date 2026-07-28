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

#ifndef ICECHUNKDEFS_H
#define ICECHUNKDEFS_H

#include <array>
#include <cstdint>
#include <vector>

namespace gdal::icechunk
{
using ObjectId8 = std::array<uint8_t, 8>;
using ObjectId12 = std::array<uint8_t, 12>;
using ChunkIdx = std::vector<uint32_t>;
}  // namespace gdal::icechunk

#endif
