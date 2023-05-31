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
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
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
