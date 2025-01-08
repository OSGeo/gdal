/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Common code shared between the GTiff and libertiff drivers
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef TIFF_COMMON_HPP
#define TIFF_COMMON_HPP

#include "cpl_string.h"

#include "gdal_priv.h"

namespace gdal
{
namespace tiff_common
{
char CPL_DLL *PrepareTIFFErrorFormat(const char *module, const char *fmt);

std::unique_ptr<GDALColorTable> CPL_DLL TIFFColorMapTagToColorTable(
    const unsigned short *panRed, const unsigned short *panGreen,
    const unsigned short *panBlue, int nColorCount, int &nColorTableMultiplier,
    int nDefaultColorTableMultiplier, bool bNoDataSet, double dfNoDataValue);

CPLStringList CPL_DLL TIFFRPCTagToRPCMetadata(const double adfRPC[92]);

}  // namespace tiff_common
}  // namespace gdal

#endif
