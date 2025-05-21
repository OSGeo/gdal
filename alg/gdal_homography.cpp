/******************************************************************************
 *
 * Project:  Homography Transformer
 * Author:   Nathan Olson
 *
 ******************************************************************************
 * Copyright (c) 2025, Nathan Olson <nathanmolson at gmail dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"

#include <stdlib.h>
#include <string.h>
#include <algorithm>

#include "cpl_atomic_ops.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdallinearsystem.h"

CPL_C_START
CPLXMLNode *GDALSerializeHomographyTransformer(void *pTransformArg);
void *GDALDeserializeHomographyTransformer(CPLXMLNode *psTree);
CPL_C_END

struct HomographyTransformInfo
{
    GDALTransformerInfo sTI{};

    double padfForward[9]{};
    double padfReverse[9]{};

    volatile int nRefCount{};
};

/************************************************************************/
/*               GDALCreateSimilarHomographyTransformer()               */
/************************************************************************/

static void *GDALCreateSimilarHomographyTransformer(void *hTransformArg,
                                                    double dfRatioX,
                                                    double dfRatioY)
{
    VALIDATE_POINTER1(hTransformArg, "GDALCreateSimilarHomographyTransformer",
                      nullptr);

    HomographyTransformInfo *psInfo =
        static_cast<HomographyTransformInfo *>(hTransformArg);

    if (dfRatioX == 1.0 && dfRatioY == 1.0)
    {
        // We can just use a ref count, since using the source transformation
        // is thread-safe.
        CPLAtomicInc(&(psInfo->nRefCount));
    }
    else
    {
        double homography[9];
        for (int i = 0; i < 3; i++)
        {
            homography[3 * i + 1] = psInfo->padfForward[3 * i + 1] / dfRatioX;
            homography[3 * i + 2] = psInfo->padfForward[3 * i + 2] / dfRatioY;
            homography[3 * i] = psInfo->padfForward[3 * i];
        }
        psInfo = static_cast<HomographyTransformInfo *>(
            GDALCreateHomographyTransformer(homography));
    }

    return psInfo;
}

/************************************************************************/
/*                   GDALCreateHomographyTransformer()                  */
/************************************************************************/

/**
 * Create Homography transformer from GCPs.
 *
 * Homography Transformers are serializable.
 *
 * @param adfHomography the forward homography.
 *
 * @return the transform argument or NULL if creation fails.
 */

void *GDALCreateHomographyTransformer(double adfHomography[9])
{
    /* -------------------------------------------------------------------- */
    /*      Allocate transform info.                                        */
    /* -------------------------------------------------------------------- */
    HomographyTransformInfo *psInfo = new HomographyTransformInfo();

    memcpy(psInfo->sTI.abySignature, GDAL_GTI2_SIGNATURE,
           strlen(GDAL_GTI2_SIGNATURE));
    psInfo->sTI.pszClassName = "GDALHomographyTransformer";
    psInfo->sTI.pfnTransform = GDALHomographyTransform;
    psInfo->sTI.pfnCleanup = GDALDestroyHomographyTransformer;
    psInfo->sTI.pfnSerialize = GDALSerializeHomographyTransformer;
    psInfo->sTI.pfnCreateSimilar = GDALCreateSimilarHomographyTransformer;

    psInfo->nRefCount = 1;

    memcpy(psInfo->padfForward, adfHomography, 9 * sizeof(double));
    if (GDALInvHomography(psInfo->padfForward, psInfo->padfReverse))
    {
        return psInfo;
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "GDALCreateHomographyTransformer() failed, because "
             "GDALInvHomography() failed");
    GDALDestroyHomographyTransformer(psInfo);
    return nullptr;
}

/************************************************************************/
/*                        GDALGCPsToHomography()                        */
/************************************************************************/

/**
 * \brief Generate Homography from GCPs.
 *
 * Given a set of GCPs perform least squares fit as a homography.
 *
 * A minimum of four GCPs are required to uniquely define a homography.
 * If there are less than four GCPs, GDALGCPsToGeoTransform() is used to
 * compute the transform.
 *
 * @param nGCPCount the number of GCPs being passed in.
 * @param pasGCPList the list of GCP structures.
 * @param padfHomography the nine double array in which the homography
 * will be returned.
 *
 * @return TRUE on success or FALSE if there aren't enough points to prepare a
 * homography, or pathological geometry is detected
 */
int GDALGCPsToHomography(int nGCPCount, const GDAL_GCP *pasGCPList,
                         double *padfHomography)
{
    if (nGCPCount < 4)
    {
        padfHomography[6] = 1.0;
        padfHomography[7] = 0.0;
        padfHomography[8] = 0.0;
        return GDALGCPsToGeoTransform(nGCPCount, pasGCPList, padfHomography,
                                      FALSE);
    }

    /* -------------------------------------------------------------------- */
    /*      Compute source and destination ranges so we can normalize       */
    /*      the values to make the least squares computation more stable.   */
    /* -------------------------------------------------------------------- */
    double min_pixel = pasGCPList[0].dfGCPPixel;
    double max_pixel = pasGCPList[0].dfGCPPixel;
    double min_line = pasGCPList[0].dfGCPLine;
    double max_line = pasGCPList[0].dfGCPLine;
    double min_geox = pasGCPList[0].dfGCPX;
    double max_geox = pasGCPList[0].dfGCPX;
    double min_geoy = pasGCPList[0].dfGCPY;
    double max_geoy = pasGCPList[0].dfGCPY;

    for (int i = 1; i < nGCPCount; ++i)
    {
        min_pixel = std::min(min_pixel, pasGCPList[i].dfGCPPixel);
        max_pixel = std::max(max_pixel, pasGCPList[i].dfGCPPixel);
        min_line = std::min(min_line, pasGCPList[i].dfGCPLine);
        max_line = std::max(max_line, pasGCPList[i].dfGCPLine);
        min_geox = std::min(min_geox, pasGCPList[i].dfGCPX);
        max_geox = std::max(max_geox, pasGCPList[i].dfGCPX);
        min_geoy = std::min(min_geoy, pasGCPList[i].dfGCPY);
        max_geoy = std::max(max_geoy, pasGCPList[i].dfGCPY);
    }

    constexpr double EPSILON = 1.0e-12;

    if (std::abs(max_pixel - min_pixel) < EPSILON ||
        std::abs(max_line - min_line) < EPSILON ||
        std::abs(max_geox - min_geox) < EPSILON ||
        std::abs(max_geoy - min_geoy) < EPSILON)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALGCPsToHomography() failed: GCPs degenerate in at least "
                 "one dimension.");
        return FALSE;
    }

    double pl_normalize[9], geo_normalize[9];

    pl_normalize[0] = -min_pixel / (max_pixel - min_pixel);
    pl_normalize[1] = 1.0 / (max_pixel - min_pixel);
    pl_normalize[2] = 0.0;
    pl_normalize[3] = -min_line / (max_line - min_line);
    pl_normalize[4] = 0.0;
    pl_normalize[5] = 1.0 / (max_line - min_line);
    pl_normalize[6] = 1.0;
    pl_normalize[7] = 0.0;
    pl_normalize[8] = 0.0;

    geo_normalize[0] = -min_geox / (max_geox - min_geox);
    geo_normalize[1] = 1.0 / (max_geox - min_geox);
    geo_normalize[2] = 0.0;
    geo_normalize[3] = -min_geoy / (max_geoy - min_geoy);
    geo_normalize[4] = 0.0;
    geo_normalize[5] = 1.0 / (max_geoy - min_geoy);
    geo_normalize[6] = 1.0;
    geo_normalize[7] = 0.0;
    geo_normalize[8] = 0.0;

    double inv_geo_normalize[9] = {0.0};
    if (!GDALInvHomography(geo_normalize, inv_geo_normalize))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALGCPsToHomography() failed: GDALInvHomography() failed");
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /* Calculate the best fit homography following                          */
    /* https://www.cs.unc.edu/~ronisen/teaching/fall_2023/pdf_slides/       */
    /* lecture9_transformation.pdf                                          */
    /* Since rank(AtA) = rank(8) = 8, append an additional equation         */
    /* h_normalized[6] = 1 to fully define the solution.                    */
    /* -------------------------------------------------------------------- */
    GDALMatrix AtA(9, 9);
    GDALMatrix rhs(9, 1);
    rhs(6, 0) = 1;
    AtA(6, 6) = 1;

    for (int i = 0; i < nGCPCount; ++i)
    {
        double pixel, line, geox, geoy;

        if (!GDALApplyHomography(pl_normalize, pasGCPList[i].dfGCPPixel,
                                 pasGCPList[i].dfGCPLine, &pixel, &line) ||
            !GDALApplyHomography(geo_normalize, pasGCPList[i].dfGCPX,
                                 pasGCPList[i].dfGCPY, &geox, &geoy))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALGCPsToHomography() failed: GDALApplyHomography() "
                     "failed on GCP %d.",
                     i);
            return FALSE;
        }

        double Ax[] = {1, pixel, line,          0,           0,
                       0, -geox, -geox * pixel, -geox * line};
        double Ay[] = {0,           0, 0, 1, pixel, line, -geoy, -geoy * pixel,
                       -geoy * line};
        int j, k;
        // Populate the lower triangle of symmetric AtA matrix
        for (j = 0; j < 9; j++)
        {
            for (k = j; k < 9; k++)
            {
                AtA(j, k) += Ax[j] * Ax[k] + Ay[j] * Ay[k];
            }
        }
    }
    // Populate the upper triangle of symmetric AtA matrix
    for (int j = 0; j < 9; j++)
    {
        for (int k = 0; k < j; k++)
        {
            AtA(j, k) = AtA(k, j);
        }
    }

    GDALMatrix h_normalized(9, 1);
    if (!GDALLinearSystemSolve(AtA, rhs, h_normalized))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "GDALGCPsToHomography() failed: GDALLinearSystemSolve() failed");
        return FALSE;
    }
    if (std::abs(h_normalized(6, 0)) < 1.0e-15)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALGCPsToHomography() failed: h_normalized(6, 0) not zero");
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /* Check that the homography maps the unit square to a convex           */
    /* quadrilateral.                                                       */
    /* -------------------------------------------------------------------- */
    // First, use the normalized homography to make the corners of the unit
    // square to normalized geo coordinates
    double x[4] = {0, 1, 1, 0};
    double y[4] = {0, 0, 1, 1};
    for (int i = 0; i < 4; i++)
    {
        if (!GDALApplyHomography(h_normalized.data(), x[i], y[i], &x[i], &y[i]))
        {
            return FALSE;
        }
    }
    // Next, compute the vector from the top-left corner to each corner
    for (int i = 3; i >= 0; i--)
    {
        x[i] -= x[0];
        y[i] -= y[0];
    }
    // Finally, check that "v2" (<x[2], y[2]>, the vector from top-left to
    // bottom-right corner) is between v1 and v3, by checking that the
    // vector cross product (v1 x v2) has the same sign as (v2 x v3)
    double cross12 = x[1] * y[2] - x[2] * y[1];
    double cross23 = x[2] * y[3] - x[3] * y[2];
    if (cross12 * cross23 <= 0.0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALGCPsToHomography() failed: cross12 * cross23 <= 0.0");
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Compose the resulting transformation with the normalization     */
    /*      homographies.                                                   */
    /* -------------------------------------------------------------------- */
    double h1p2[9] = {0.0};

    GDALComposeHomographies(pl_normalize, h_normalized.data(), h1p2);
    GDALComposeHomographies(h1p2, inv_geo_normalize, padfHomography);

    return TRUE;
}

/************************************************************************/
/*                      GDALComposeHomographies()                       */
/************************************************************************/

/**
 * \brief Compose two homographies.
 *
 * The resulting homography is the equivalent to padfH1 and then padfH2
 * being applied to a point.
 *
 * @param padfH1 the first homography, nine values.
 * @param padfH2 the second homography, nine values.
 * @param padfHOut the output homography, nine values, may safely be the same
 * array as padfH1 or padfH2.
 */

void GDALComposeHomographies(const double *padfH1, const double *padfH2,
                             double *padfHOut)

{
    double hwrk[9] = {0.0};

    hwrk[1] =
        padfH2[1] * padfH1[1] + padfH2[2] * padfH1[4] + padfH2[0] * padfH1[7];
    hwrk[2] =
        padfH2[1] * padfH1[2] + padfH2[2] * padfH1[5] + padfH2[0] * padfH1[8];
    hwrk[0] =
        padfH2[1] * padfH1[0] + padfH2[2] * padfH1[3] + padfH2[0] * padfH1[6];

    hwrk[4] =
        padfH2[4] * padfH1[1] + padfH2[5] * padfH1[4] + padfH2[3] * padfH1[7];
    hwrk[5] =
        padfH2[4] * padfH1[2] + padfH2[5] * padfH1[5] + padfH2[3] * padfH1[8];
    hwrk[3] =
        padfH2[4] * padfH1[0] + padfH2[5] * padfH1[3] + padfH2[3] * padfH1[6];

    hwrk[7] =
        padfH2[7] * padfH1[1] + padfH2[8] * padfH1[4] + padfH2[6] * padfH1[7];
    hwrk[8] =
        padfH2[7] * padfH1[2] + padfH2[8] * padfH1[5] + padfH2[6] * padfH1[8];
    hwrk[6] =
        padfH2[7] * padfH1[0] + padfH2[8] * padfH1[3] + padfH2[6] * padfH1[6];
    memcpy(padfHOut, hwrk, sizeof(hwrk));
}

/************************************************************************/
/*                        GDALApplyHomography()                         */
/************************************************************************/

/**
 * Apply Homography to x/y coordinate.
 *
 * Applies the following computation, converting a (pixel, line) coordinate
 * into a georeferenced (geo_x, geo_y) location.
 * \code{.c}
 *  *pdfGeoX = (padfHomography[0] + dfPixel * padfHomography[1]
 *                                + dfLine  * padfHomography[2]) /
 *             (padfHomography[6] + dfPixel * padfHomography[7]
 *                                + dfLine  * padfHomography[8]);
 *  *pdfGeoY = (padfHomography[3] + dfPixel * padfHomography[4]
 *                                + dfLine  * padfHomography[5]) /
 *             (padfHomography[6] + dfPixel * padfHomography[7]
 *                                + dfLine  * padfHomography[8]);
 * \endcode
 *
 * @param padfHomography Nine coefficient Homography to apply.
 * @param dfPixel Input pixel position.
 * @param dfLine Input line position.
 * @param pdfGeoX output location where geo_x (easting/longitude)
 * location is placed.
 * @param pdfGeoY output location where geo_y (northing/latitude)
 * location is placed.
*
* @return TRUE on success or FALSE if failure.
 */

int GDALApplyHomography(const double *padfHomography, double dfPixel,
                        double dfLine, double *pdfGeoX, double *pdfGeoY)
{
    double w = padfHomography[6] + dfPixel * padfHomography[7] +
               dfLine * padfHomography[8];
    if (std::abs(w) < 1.0e-15)
    {
        return FALSE;
    }
    double wx = padfHomography[0] + dfPixel * padfHomography[1] +
                dfLine * padfHomography[2];
    double wy = padfHomography[3] + dfPixel * padfHomography[4] +
                dfLine * padfHomography[5];
    *pdfGeoX = wx / w;
    *pdfGeoY = wy / w;
    return TRUE;
}

/************************************************************************/
/*                         GDALInvHomography()                          */
/************************************************************************/

/**
* Invert Homography.
*
* This function will invert a standard 3x3 set of Homography coefficients.
* This converts the equation from being pixel to geo to being geo to pixel.
*
* @param padfHIn Input homography (nine doubles - unaltered).
* @param padfHOut Output homography (nine doubles - updated).
*
* @return TRUE on success or FALSE if the equation is uninvertable.
*/

int GDALInvHomography(const double *padfHIn, double *padfHOut)

{
    // Special case - no rotation - to avoid computing determinant
    // and potential precision issues.
    if (padfHIn[2] == 0.0 && padfHIn[4] == 0.0 && padfHIn[1] != 0.0 &&
        padfHIn[5] != 0.0 && padfHIn[7] == 0.0 && padfHIn[8] == 0.0 &&
        padfHIn[6] != 0.0)
    {
        padfHOut[0] = -padfHIn[0] / padfHIn[1] / padfHIn[6];
        padfHOut[1] = 1.0 / padfHIn[1];
        padfHOut[2] = 0.0;
        padfHOut[3] = -padfHIn[3] / padfHIn[5] / padfHIn[6];
        padfHOut[4] = 0.0;
        padfHOut[5] = 1.0 / padfHIn[5];
        padfHOut[6] = 1.0 / padfHIn[6];
        padfHOut[7] = 0.0;
        padfHOut[8] = 0.0;
        return TRUE;
    }

    // Compute determinant.

    const double det = padfHIn[1] * padfHIn[5] * padfHIn[6] -
                       padfHIn[2] * padfHIn[4] * padfHIn[6] +
                       padfHIn[2] * padfHIn[3] * padfHIn[7] -
                       padfHIn[0] * padfHIn[5] * padfHIn[7] +
                       padfHIn[0] * padfHIn[4] * padfHIn[8] -
                       padfHIn[1] * padfHIn[3] * padfHIn[8];
    const double magnitude =
        std::max(std::max(fabs(padfHIn[1]), fabs(padfHIn[2])),
                 std::max(fabs(padfHIn[4]), fabs(padfHIn[5])));

    if (fabs(det) <= 1e-10 * magnitude * magnitude)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALInvHomography() failed: null determinant");
        return FALSE;
    }

    const double inv_det = 1.0 / det;

    // Compute adjoint, and divide by determinant.

    padfHOut[1] = (padfHIn[5] * padfHIn[6] - padfHIn[3] * padfHIn[8]) * inv_det;
    padfHOut[4] = (padfHIn[3] * padfHIn[7] - padfHIn[4] * padfHIn[6]) * inv_det;
    padfHOut[7] = (padfHIn[4] * padfHIn[8] - padfHIn[5] * padfHIn[7]) * inv_det;

    padfHOut[2] = (padfHIn[0] * padfHIn[8] - padfHIn[2] * padfHIn[6]) * inv_det;
    padfHOut[5] = (padfHIn[1] * padfHIn[6] - padfHIn[0] * padfHIn[7]) * inv_det;
    padfHOut[8] = (padfHIn[2] * padfHIn[7] - padfHIn[1] * padfHIn[8]) * inv_det;

    padfHOut[0] = (padfHIn[2] * padfHIn[3] - padfHIn[0] * padfHIn[5]) * inv_det;
    padfHOut[3] = (padfHIn[0] * padfHIn[4] - padfHIn[1] * padfHIn[3]) * inv_det;
    padfHOut[6] = (padfHIn[1] * padfHIn[5] - padfHIn[2] * padfHIn[4]) * inv_det;

    return TRUE;
}

/************************************************************************/
/*               GDALCreateHomographyTransformerFromGCPs()              */
/************************************************************************/

/**
 * Create Homography transformer from GCPs.
 *
 * Homography Transformers are serializable.
 *
 * @param nGCPCount the number of GCPs in pasGCPList.
 * @param pasGCPList an array of GCPs to be used as input.
 *
 * @return the transform argument or NULL if creation fails.
 */

void *GDALCreateHomographyTransformerFromGCPs(int nGCPCount,
                                              const GDAL_GCP *pasGCPList)
{
    double adfHomography[9];

    if (GDALGCPsToHomography(nGCPCount, pasGCPList, adfHomography))
    {
        return GDALCreateHomographyTransformer(adfHomography);
    }
    return nullptr;
}

/************************************************************************/
/*                  GDALDestroyHomographyTransformer()                  */
/************************************************************************/

/**
 * Destroy Homography transformer.
 *
 * This function is used to destroy information about a homography
 * transformation created with GDALCreateHomographyTransformer().
 *
 * @param pTransformArg the transform arg previously returned by
 * GDALCreateHomographyTransformer().
 */

void GDALDestroyHomographyTransformer(void *pTransformArg)

{
    if (pTransformArg == nullptr)
        return;

    HomographyTransformInfo *psInfo =
        static_cast<HomographyTransformInfo *>(pTransformArg);

    if (CPLAtomicDec(&(psInfo->nRefCount)) == 0)
    {
        delete psInfo;
    }
}

/************************************************************************/
/*                       GDALHomographyTransform()                      */
/************************************************************************/

/**
 * Transforms point based on homography.
 *
 * This function matches the GDALTransformerFunc signature, and can be
 * used to transform one or more points from pixel/line coordinates to
 * georeferenced coordinates (SrcToDst) or vice versa (DstToSrc).
 *
 * @param pTransformArg return value from GDALCreateHomographyTransformer().
 * @param bDstToSrc TRUE if transformation is from the destination
 * (georeferenced) coordinates to pixel/line or FALSE when transforming
 * from pixel/line to georeferenced coordinates.
 * @param nPointCount the number of values in the x, y and z arrays.
 * @param x array containing the X values to be transformed.
 * @param y array containing the Y values to be transformed.
 * @param z array containing the Z values to be transformed.
 * @param panSuccess array in which a flag indicating success (TRUE) or
 * failure (FALSE) of the transformation are placed.
 *
 * @return TRUE if all points have been successfully transformed.
 */

int GDALHomographyTransform(void *pTransformArg, int bDstToSrc, int nPointCount,
                            double *x, double *y, CPL_UNUSED double *z,
                            int *panSuccess)
{
    VALIDATE_POINTER1(pTransformArg, "GDALHomographyTransform", 0);

    HomographyTransformInfo *psInfo =
        static_cast<HomographyTransformInfo *>(pTransformArg);

    double *homography = bDstToSrc ? psInfo->padfReverse : psInfo->padfForward;
    int ret = TRUE;
    for (int i = 0; i < nPointCount; i++)
    {
        double w = homography[6] + x[i] * homography[7] + y[i] * homography[8];
        if (std::abs(w) < 1.0e-15)
        {
            panSuccess[i] = FALSE;
            ret = FALSE;
        }
        else
        {
            double wx =
                homography[0] + x[i] * homography[1] + y[i] * homography[2];
            double wy =
                homography[3] + x[i] * homography[4] + y[i] * homography[5];
            x[i] = wx / w;
            y[i] = wy / w;
            panSuccess[i] = TRUE;
        }
    }

    return ret;
}

/************************************************************************/
/*                 GDALSerializeHomographyTransformer()                 */
/************************************************************************/

CPLXMLNode *GDALSerializeHomographyTransformer(void *pTransformArg)

{
    VALIDATE_POINTER1(pTransformArg, "GDALSerializeHomographyTransformer",
                      nullptr);

    HomographyTransformInfo *psInfo =
        static_cast<HomographyTransformInfo *>(pTransformArg);

    CPLXMLNode *psTree =
        CPLCreateXMLNode(nullptr, CXT_Element, "HomographyTransformer");

    /* -------------------------------------------------------------------- */
    /*      Attach Homography.                                              */
    /* -------------------------------------------------------------------- */
    char szWork[300] = {};

    CPLsnprintf(
        szWork, sizeof(szWork),
        "%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g",
        psInfo->padfForward[0], psInfo->padfForward[1], psInfo->padfForward[2],
        psInfo->padfForward[3], psInfo->padfForward[4], psInfo->padfForward[5],
        psInfo->padfForward[6], psInfo->padfForward[7], psInfo->padfForward[8]);
    CPLCreateXMLElementAndValue(psTree, "Homography", szWork);

    return psTree;
}

/************************************************************************/
/*                     GDALDeserializeHomography()                      */
/************************************************************************/

static void GDALDeserializeHomography(const char *pszH, double adfHomography[9])
{
    CPLsscanf(pszH, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", adfHomography + 0,
              adfHomography + 1, adfHomography + 2, adfHomography + 3,
              adfHomography + 4, adfHomography + 5, adfHomography + 6,
              adfHomography + 7, adfHomography + 8);
}

/************************************************************************/
/*                GDALDeserializeHomographyTransformer()                */
/************************************************************************/

void *GDALDeserializeHomographyTransformer(CPLXMLNode *psTree)

{
    /* -------------------------------------------------------------------- */
    /*        Homography                                                    */
    /* -------------------------------------------------------------------- */
    double padfForward[9];
    if (CPLGetXMLNode(psTree, "Homography") != nullptr)
    {
        GDALDeserializeHomography(CPLGetXMLValue(psTree, "Homography", ""),
                                  padfForward);

        /* -------------------------------------------------------------------- */
        /*      Generate transformation.                                        */
        /* -------------------------------------------------------------------- */
        void *pResult = GDALCreateHomographyTransformer(padfForward);

        return pResult;
    }
    return nullptr;
}
