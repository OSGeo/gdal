/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Convert nearly black or nearly white border to exact black/white.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 2006, MapShots Inc (www.mapshots.com)
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef NEARBLACK_LIB_H
#define NEARBLACK_LIB_H

#ifndef DOXYGEN_SKIP

#include "cpl_progress.h"
#include "cpl_string.h"

#include <string>
#include <vector>

typedef std::vector<int> Color;
typedef std::vector<Color> Colors;

struct GDALNearblackOptions
{
    /*! output format. Use the short format name. */
    std::string osFormat{};

    /*! the progress function to use */
    GDALProgressFunc pfnProgress = GDALDummyProgress;

    /*! pointer to the progress data variable */
    void *pProgressData = nullptr;

    int nMaxNonBlack = 2;
    int nNearDist = 15;
    bool bNearWhite = false;
    bool bSetAlpha = false;
    bool bSetMask = false;

    bool bFloodFill = false;

    Colors oColors{};

    CPLStringList aosCreationOptions{};
};

bool GDALNearblackTwoPassesAlgorithm(const GDALNearblackOptions *psOptions,
                                     GDALDatasetH hSrcDataset,
                                     GDALDatasetH hDstDS,
                                     GDALRasterBandH hMaskBand, int nBands,
                                     int nDstBands, bool bSetMask,
                                     const Colors &oColors);

bool GDALNearblackFloodFill(const GDALNearblackOptions *psOptions,
                            GDALDatasetH hSrcDataset, GDALDatasetH hDstDS,
                            GDALRasterBandH hMaskBand, int nSrcBands,
                            int nDstBands, bool bSetMask,
                            const Colors &oColors);

#endif  // DOXYGEN_SKIP

#endif  // NEARBLACK_LIB_H
