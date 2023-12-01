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
