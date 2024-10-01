/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Generic method to compute inverse coordinate transformation from
 *           forward method
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDALGENERICINVERSE_H
#define GDALGENERICINVERSE_H
#include <stdbool.h>

typedef bool (*GDALForwardCoordTransformer)(double xIn, double yIn,
                                            double &xOut, double &yOut,
                                            void *pUserData);

bool GDALGenericInverse2D(double xIn, double yIn, double guessedXOut,
                          double guessedYOut,
                          GDALForwardCoordTransformer pfnForwardTranformer,
                          void *pfnForwardTranformerUserData, double &xOut,
                          double &yOut,
                          bool computeJacobianMatrixOnlyAtFirstIter = false,
                          double toleranceOnInputCoordinates = 0,
                          double toleranceOnOutputCoordinates = 0);

#endif  // GDALGENERICINVERSE_H
