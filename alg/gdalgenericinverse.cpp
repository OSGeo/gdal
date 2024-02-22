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

#include <algorithm>
#include <cmath>

#include "gdalgenericinverse.h"
#include <cstdio>

/** Compute (xOut, yOut) corresponding to input (xIn, yIn) using
 * the provided forward transformation to emulate the reverse direction.
 *
 * Uses Newton-Raphson method, extended to 2D variables, that is using
 * inversion of the Jacobian 2D matrix of partial derivatives. The derivatives
 * are estimated numerically from the forward method evaluated at close points.
 *
 * Starts with initial guess provided by user in (guessedXOut, guessedYOut)
 *
 * It iterates at most for 15 iterations or as soon as we get below the specified
 * tolerance (on input coordinates)
 */
bool GDALGenericInverse2D(double xIn, double yIn, double guessedXOut,
                          double guessedYOut,
                          GDALForwardCoordTransformer pfnForwardTranformer,
                          void *pfnForwardTranformerUserData, double &xOut,
                          double &yOut,
                          bool computeJacobianMatrixOnlyAtFirstIter,
                          double toleranceOnInputCoordinates,
                          double toleranceOnOutputCoordinates)
{
    const double dfAbsValOut = std::max(fabs(guessedXOut), fabs(guessedYOut));
    const double dfEps = dfAbsValOut > 0 ? dfAbsValOut * 1e-6 : 1e-6;
    if (toleranceOnInputCoordinates == 0)
    {
        const double dfAbsValIn = std::max(fabs(xIn), fabs(yIn));
        toleranceOnInputCoordinates =
            dfAbsValIn > 0 ? dfAbsValIn * 1e-12 : 1e-12;
    }
    xOut = guessedXOut;
    yOut = guessedYOut;
    double deriv_lam_X = 0;
    double deriv_lam_Y = 0;
    double deriv_phi_X = 0;
    double deriv_phi_Y = 0;
    for (int i = 0; i < 15; i++)
    {
        double xApprox;
        double yApprox;
        if (!pfnForwardTranformer(xOut, yOut, xApprox, yApprox,
                                  pfnForwardTranformerUserData))
            return false;
        const double deltaX = xApprox - xIn;
        const double deltaY = yApprox - yIn;
        if (fabs(deltaX) < toleranceOnInputCoordinates &&
            fabs(deltaY) < toleranceOnInputCoordinates)
        {
            return true;
        }

        if (i == 0 || !computeJacobianMatrixOnlyAtFirstIter)
        {
            // Compute Jacobian matrix
            double xTmp = xOut + dfEps;
            double yTmp = yOut;
            double xTmpOut;
            double yTmpOut;
            if (!pfnForwardTranformer(xTmp, yTmp, xTmpOut, yTmpOut,
                                      pfnForwardTranformerUserData))
                return false;
            const double deriv_X_lam = (xTmpOut - xApprox) / dfEps;
            const double deriv_Y_lam = (yTmpOut - yApprox) / dfEps;

            xTmp = xOut;
            yTmp = yOut + dfEps;
            if (!pfnForwardTranformer(xTmp, yTmp, xTmpOut, yTmpOut,
                                      pfnForwardTranformerUserData))
                return false;
            const double deriv_X_phi = (xTmpOut - xApprox) / dfEps;
            const double deriv_Y_phi = (yTmpOut - yApprox) / dfEps;

            // Inverse of Jacobian matrix
            const double det =
                deriv_X_lam * deriv_Y_phi - deriv_X_phi * deriv_Y_lam;
            if (det != 0)
            {
                deriv_lam_X = deriv_Y_phi / det;
                deriv_lam_Y = -deriv_X_phi / det;
                deriv_phi_X = -deriv_Y_lam / det;
                deriv_phi_Y = deriv_X_lam / det;
            }
            else
            {
                return false;
            }
        }

        const double xOutDelta = deltaX * deriv_lam_X + deltaY * deriv_lam_Y;
        const double yOutDelta = deltaX * deriv_phi_X + deltaY * deriv_phi_Y;
        xOut -= xOutDelta;
        yOut -= yOutDelta;
        if (toleranceOnOutputCoordinates > 0 &&
            fabs(xOutDelta) < toleranceOnOutputCoordinates &&
            fabs(yOutDelta) < toleranceOnOutputCoordinates)
        {
            return true;
        }
    }
    return false;
}
