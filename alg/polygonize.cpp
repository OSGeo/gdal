/******************************************************************************
 * Project:  GDAL
 * Purpose:  Raster to Polygon Converter
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2009-2020, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "gdal_alg.h"

#include <stddef.h>
#include <stdio.h>
#include <cstdlib>
#include <string.h>

#include <algorithm>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "gdal_alg_priv.h"
#include "gdal.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"

#include "polygonize_polygonizer.h"

using namespace gdal::polygonizer;

/************************************************************************/
/*                          GPMaskImageData()                           */
/*                                                                      */
/*      Mask out image pixels to a special nodata value if the mask     */
/*      band is zero.                                                   */
/************************************************************************/

template <class DataType>
static CPLErr GPMaskImageData(GDALRasterBandH hMaskBand, GByte *pabyMaskLine,
                              int iY, int nXSize, DataType *panImageLine)

{
    const CPLErr eErr = GDALRasterIO(hMaskBand, GF_Read, 0, iY, nXSize, 1,
                                     pabyMaskLine, nXSize, 1, GDT_Byte, 0, 0);
    if (eErr != CE_None)
        return eErr;

    for (int i = 0; i < nXSize; i++)
    {
        if (pabyMaskLine[i] == 0)
            panImageLine[i] = GP_NODATA_MARKER;
    }

    return CE_None;
}

/************************************************************************/
/*                           GDALPolygonizeT()                          */
/************************************************************************/

template <class DataType, class EqualityTest>
static CPLErr GDALPolygonizeT(GDALRasterBandH hSrcBand,
                              GDALRasterBandH hMaskBand, OGRLayerH hOutLayer,
                              int iPixValField, char **papszOptions,
                              GDALProgressFunc pfnProgress, void *pProgressArg,
                              GDALDataType eDT)

{
    VALIDATE_POINTER1(hSrcBand, "GDALPolygonize", CE_Failure);
    VALIDATE_POINTER1(hOutLayer, "GDALPolygonize", CE_Failure);

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    const int nConnectedness =
        CSLFetchNameValue(papszOptions, "8CONNECTED") ? 8 : 4;

    /* -------------------------------------------------------------------- */
    /*      Confirm our output layer will support feature creation.         */
    /* -------------------------------------------------------------------- */
    if (!OGR_L_TestCapability(hOutLayer, OLCSequentialWrite))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Output feature layer does not appear to support creation "
                 "of features in GDALPolygonize().");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Allocate working buffers.                                       */
    /* -------------------------------------------------------------------- */
    const int nXSize = GDALGetRasterBandXSize(hSrcBand);
    const int nYSize = GDALGetRasterBandYSize(hSrcBand);
    if (nXSize > std::numeric_limits<int>::max() - 2)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too wide raster");
        return CE_Failure;
    }

    DataType *panLastLineVal =
        static_cast<DataType *>(VSI_MALLOC2_VERBOSE(sizeof(DataType), nXSize));
    DataType *panThisLineVal =
        static_cast<DataType *>(VSI_MALLOC2_VERBOSE(sizeof(DataType), nXSize));
    GInt32 *panLastLineId =
        static_cast<GInt32 *>(VSI_MALLOC2_VERBOSE(sizeof(GInt32), nXSize));
    GInt32 *panThisLineId =
        static_cast<GInt32 *>(VSI_MALLOC2_VERBOSE(sizeof(GInt32), nXSize));

    GByte *pabyMaskLine = static_cast<GByte *>(VSI_MALLOC_VERBOSE(nXSize));

    if (panLastLineVal == nullptr || panThisLineVal == nullptr ||
        panLastLineId == nullptr || panThisLineId == nullptr ||
        pabyMaskLine == nullptr)
    {
        CPLFree(panThisLineId);
        CPLFree(panLastLineId);
        CPLFree(panThisLineVal);
        CPLFree(panLastLineVal);
        CPLFree(pabyMaskLine);
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Get the geotransform, if there is one, so we can convert the    */
    /*      vectors into georeferenced coordinates.                         */
    /* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
    bool bGotGeoTransform = false;
    const char *pszDatasetForGeoRef =
        CSLFetchNameValue(papszOptions, "DATASET_FOR_GEOREF");
    if (pszDatasetForGeoRef)
    {
        GDALDatasetH hSrcDS = GDALOpen(pszDatasetForGeoRef, GA_ReadOnly);
        if (hSrcDS)
        {
            bGotGeoTransform =
                GDALGetGeoTransform(hSrcDS, adfGeoTransform) == CE_None;
            GDALClose(hSrcDS);
        }
    }
    else
    {
        GDALDatasetH hSrcDS = GDALGetBandDataset(hSrcBand);
        if (hSrcDS)
            bGotGeoTransform =
                GDALGetGeoTransform(hSrcDS, adfGeoTransform) == CE_None;
    }
    if (!bGotGeoTransform)
    {
        adfGeoTransform[0] = 0;
        adfGeoTransform[1] = 1;
        adfGeoTransform[2] = 0;
        adfGeoTransform[3] = 0;
        adfGeoTransform[4] = 0;
        adfGeoTransform[5] = 1;
    }

    /* -------------------------------------------------------------------- */
    /*      The first pass over the raster is only used to build up the     */
    /*      polygon id map so we will know in advance what polygons are     */
    /*      what on the second pass.                                        */
    /* -------------------------------------------------------------------- */
    GDALRasterPolygonEnumeratorT<DataType, EqualityTest> oFirstEnum(
        nConnectedness);

    CPLErr eErr = CE_None;

    for (int iY = 0; eErr == CE_None && iY < nYSize; iY++)
    {
        eErr = GDALRasterIO(hSrcBand, GF_Read, 0, iY, nXSize, 1, panThisLineVal,
                            nXSize, 1, eDT, 0, 0);

        if (eErr == CE_None && hMaskBand != nullptr)
            eErr = GPMaskImageData(hMaskBand, pabyMaskLine, iY, nXSize,
                                   panThisLineVal);

        if (eErr != CE_None)
            break;

        if (iY == 0)
            eErr = oFirstEnum.ProcessLine(nullptr, panThisLineVal, nullptr,
                                          panThisLineId, nXSize)
                       ? CE_None
                       : CE_Failure;
        else
            eErr = oFirstEnum.ProcessLine(panLastLineVal, panThisLineVal,
                                          panLastLineId, panThisLineId, nXSize)
                       ? CE_None
                       : CE_Failure;

        if (eErr != CE_None)
            break;

        // Swap lines.
        std::swap(panLastLineVal, panThisLineVal);
        std::swap(panLastLineId, panThisLineId);

        /* --------------------------------------------------------------------
         */
        /*      Report progress, and support interrupts. */
        /* --------------------------------------------------------------------
         */
        if (!pfnProgress(0.10 * ((iY + 1) / static_cast<double>(nYSize)), "",
                         pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            eErr = CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Make a pass through the maps, ensuring every polygon id         */
    /*      points to the final id it should use, not an intermediate       */
    /*      value.                                                          */
    /* -------------------------------------------------------------------- */
    if (eErr == CE_None)
        oFirstEnum.CompleteMerges();

    /* -------------------------------------------------------------------- */
    /*      We will use a new enumerator for the second pass primarily      */
    /*      so we can preserve the first pass map.                          */
    /* -------------------------------------------------------------------- */
    GDALRasterPolygonEnumeratorT<DataType, EqualityTest> oSecondEnum(
        nConnectedness);

    OGRPolygonWriter<DataType> oPolygonWriter{hOutLayer, iPixValField,
                                              adfGeoTransform};
    Polygonizer<GInt32, DataType> oPolygonizer{-1, &oPolygonWriter};
    TwoArm *paoLastLineArm =
        static_cast<TwoArm *>(VSI_CALLOC_VERBOSE(sizeof(TwoArm), nXSize + 2));
    TwoArm *paoThisLineArm =
        static_cast<TwoArm *>(VSI_CALLOC_VERBOSE(sizeof(TwoArm), nXSize + 2));

    if (paoThisLineArm == nullptr || paoLastLineArm == nullptr)
    {
        eErr = CE_Failure;
    }
    else
    {
        for (int i = 0; i < nXSize + 2; ++i)
        {
            paoLastLineArm[i].poPolyInside = oPolygonizer.getTheOuterPolygon();
        }
    }

    /* ==================================================================== */
    /*      Second pass during which we will actually collect polygon       */
    /*      edges as geometries.                                            */
    /* ==================================================================== */
    for (int iY = 0; eErr == CE_None && iY < nYSize + 1; iY++)
    {
        /* --------------------------------------------------------------------
         */
        /*      Read the image data. */
        /* --------------------------------------------------------------------
         */
        if (iY < nYSize)
        {
            eErr = GDALRasterIO(hSrcBand, GF_Read, 0, iY, nXSize, 1,
                                panThisLineVal, nXSize, 1, eDT, 0, 0);
            if (eErr == CE_None && hMaskBand != nullptr)
                eErr = GPMaskImageData(hMaskBand, pabyMaskLine, iY, nXSize,
                                       panThisLineVal);
        }

        if (eErr != CE_None)
            continue;

        /* --------------------------------------------------------------------
         */
        /*      Determine what polygon the various pixels belong to (redoing */
        /*      the same thing done in the first pass above). */
        /* --------------------------------------------------------------------
         */
        if (iY == nYSize)
        {
            for (int iX = 0; iX < nXSize; iX++)
                panThisLineId[iX] =
                    decltype(oPolygonizer)::THE_OUTER_POLYGON_ID;
        }
        else if (iY == 0)
        {
            eErr = oSecondEnum.ProcessLine(nullptr, panThisLineVal, nullptr,
                                           panThisLineId, nXSize)
                       ? CE_None
                       : CE_Failure;
        }
        else
        {
            eErr = oSecondEnum.ProcessLine(panLastLineVal, panThisLineVal,
                                           panLastLineId, panThisLineId, nXSize)
                       ? CE_None
                       : CE_Failure;
        }

        if (eErr != CE_None)
            continue;

        if (iY < nYSize)
        {
            for (int iX = 0; iX < nXSize; iX++)
            {
                // TODO: maybe we can reserve -1 as the lookup result for -1 polygon id in the panPolyIdMap,
                //       so the this expression becomes: panLastLineId[iX] = *(oFirstEnum.panPolyIdMap + panThisLineId[iX]).
                //       This would eliminate the condition checking.
                panLastLineId[iX] =
                    panThisLineId[iX] == -1
                        ? -1
                        : oFirstEnum.panPolyIdMap[panThisLineId[iX]];
            }

            oPolygonizer.processLine(panLastLineId, panLastLineVal,
                                     paoThisLineArm, paoLastLineArm, iY,
                                     nXSize);
            eErr = oPolygonWriter.getErr();
        }
        else
        {
            oPolygonizer.processLine(panThisLineId, panLastLineVal,
                                     paoThisLineArm, paoLastLineArm, iY,
                                     nXSize);
            eErr = oPolygonWriter.getErr();
        }

        if (eErr != CE_None)
            continue;

        /* --------------------------------------------------------------------
         */
        /*      Swap pixel value, and polygon id lines to be ready for the */
        /*      next line. */
        /* --------------------------------------------------------------------
         */
        std::swap(panLastLineVal, panThisLineVal);
        std::swap(panLastLineId, panThisLineId);
        std::swap(paoThisLineArm, paoLastLineArm);

        /* --------------------------------------------------------------------
         */
        /*      Report progress, and support interrupts. */
        /* --------------------------------------------------------------------
         */
        if (!pfnProgress(0.10 + 0.90 * ((iY + 1) / static_cast<double>(nYSize)),
                         "", pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            eErr = CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Cleanup                                                         */
    /* -------------------------------------------------------------------- */
    CPLFree(panThisLineId);
    CPLFree(panLastLineId);
    CPLFree(panThisLineVal);
    CPLFree(panLastLineVal);
    CPLFree(paoThisLineArm);
    CPLFree(paoLastLineArm);
    CPLFree(pabyMaskLine);

    return eErr;
}

/******************************************************************************/
/*                          GDALFloatEquals()                                 */
/* Code from:                                                                 */
/* http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm  */
/******************************************************************************/
GBool GDALFloatEquals(float A, float B)
{
    // This function will allow maxUlps-1 floats between A and B.
    const int maxUlps = MAX_ULPS;

    // Make sure maxUlps is non-negative and small enough that the default NAN
    // won't compare as equal to anything.
#if MAX_ULPS <= 0 || MAX_ULPS >= 4 * 1024 * 1024
#error "Invalid MAX_ULPS"
#endif

    // This assignation could violate strict aliasing. It causes a warning with
    // gcc -O2. Use of memcpy preferred. Credits for Even Rouault. Further info
    // at http://trac.osgeo.org/gdal/ticket/4005#comment:6
    int aInt = 0;
    memcpy(&aInt, &A, 4);

    // Make aInt lexicographically ordered as a twos-complement int.
    if (aInt < 0)
        aInt = INT_MIN - aInt;

    // Make bInt lexicographically ordered as a twos-complement int.
    int bInt = 0;
    memcpy(&bInt, &B, 4);

    if (bInt < 0)
        bInt = INT_MIN - bInt;
#ifdef COMPAT_WITH_ICC_CONVERSION_CHECK
    const int intDiff =
        abs(static_cast<int>(static_cast<GUIntBig>(static_cast<GIntBig>(aInt) -
                                                   static_cast<GIntBig>(bInt)) &
                             0xFFFFFFFFU));
#else
    // To make -ftrapv happy we compute the diff on larger type and
    // cast down later.
    const int intDiff = abs(static_cast<int>(static_cast<GIntBig>(aInt) -
                                             static_cast<GIntBig>(bInt)));
#endif
    if (intDiff <= maxUlps)
        return true;
    return false;
}

/************************************************************************/
/*                           GDALPolygonize()                           */
/************************************************************************/

/**
 * Create polygon coverage from raster data.
 *
 * This function creates vector polygons for all connected regions of pixels in
 * the raster sharing a common pixel value.  Optionally each polygon may be
 * labeled with the pixel value in an attribute.  Optionally a mask band
 * can be provided to determine which pixels are eligible for processing.
 *
 * Note that currently the source pixel band values are read into a
 * signed 64bit integer buffer (Int64), so floating point or complex
 * bands will be implicitly truncated before processing. If you want to use a
 * version using 32bit float buffers, see GDALFPolygonize().
 *
 * Polygon features will be created on the output layer, with polygon
 * geometries representing the polygons.  The polygon geometries will be
 * in the georeferenced coordinate system of the image (based on the
 * geotransform of the source dataset).  It is acceptable for the output
 * layer to already have features.  Note that GDALPolygonize() does not
 * set the coordinate system on the output layer.  Application code should
 * do this when the layer is created, presumably matching the raster
 * coordinate system.
 *
 * The algorithm used attempts to minimize memory use so that very large
 * rasters can be processed.  However, if the raster has many polygons
 * or very large/complex polygons, the memory use for holding polygon
 * enumerations and active polygon geometries may grow to be quite large.
 *
 * The algorithm will generally produce very dense polygon geometries, with
 * edges that follow exactly on pixel boundaries for all non-interior pixels.
 * For non-thematic raster data (such as satellite images) the result will
 * essentially be one small polygon per pixel, and memory and output layer
 * sizes will be substantial.  The algorithm is primarily intended for
 * relatively simple thematic imagery, masks, and classification results.
 *
 * @param hSrcBand the source raster band to be processed.
 * @param hMaskBand an optional mask band.  All pixels in the mask band with a
 * value other than zero will be considered suitable for collection as
 * polygons.
 * @param hOutLayer the vector feature layer to which the polygons should
 * be written.
 * @param iPixValField the attribute field index indicating the feature
 * attribute into which the pixel value of the polygon should be written. Or
 * -1 to indicate that the pixel value must not be written.
 * @param papszOptions a name/value list of additional options
 * <ul>
 * <li>8CONNECTED=8: May be set to "8" to use 8 connectedness.
 * Otherwise 4 connectedness will be applied to the algorithm</li>
 * <li>DATASET_FOR_GEOREF=dataset_name: Name of a dataset from which to read
 * the geotransform. This useful if hSrcBand has no related dataset, which is
 * typical for mask bands.</li>
 * </ul>
 * @param pfnProgress callback for reporting algorithm progress matching the
 * GDALProgressFunc() semantics.  May be NULL.
 * @param pProgressArg callback argument passed to pfnProgress.
 *
 * @return CE_None on success or CE_Failure on a failure.
 */

CPLErr CPL_STDCALL GDALPolygonize(GDALRasterBandH hSrcBand,
                                  GDALRasterBandH hMaskBand,
                                  OGRLayerH hOutLayer, int iPixValField,
                                  char **papszOptions,
                                  GDALProgressFunc pfnProgress,
                                  void *pProgressArg)

{
    return GDALPolygonizeT<std::int64_t, IntEqualityTest>(
        hSrcBand, hMaskBand, hOutLayer, iPixValField, papszOptions, pfnProgress,
        pProgressArg, GDT_Int64);
}

/************************************************************************/
/*                           GDALFPolygonize()                           */
/************************************************************************/

/**
 * Create polygon coverage from raster data.
 *
 * This function creates vector polygons for all connected regions of pixels in
 * the raster sharing a common pixel value.  Optionally each polygon may be
 * labeled with the pixel value in an attribute.  Optionally a mask band
 * can be provided to determine which pixels are eligible for processing.
 *
 * The source pixel band values are read into a 32bit float buffer. If you want
 * to use a (probably faster) version using signed 32bit integer buffer, see
 * GDALPolygonize().
 *
 * Polygon features will be created on the output layer, with polygon
 * geometries representing the polygons.  The polygon geometries will be
 * in the georeferenced coordinate system of the image (based on the
 * geotransform of the source dataset).  It is acceptable for the output
 * layer to already have features.  Note that GDALFPolygonize() does not
 * set the coordinate system on the output layer.  Application code should
 * do this when the layer is created, presumably matching the raster
 * coordinate system.
 *
 * The algorithm used attempts to minimize memory use so that very large
 * rasters can be processed.  However, if the raster has many polygons
 * or very large/complex polygons, the memory use for holding polygon
 * enumerations and active polygon geometries may grow to be quite large.
 *
 * The algorithm will generally produce very dense polygon geometries, with
 * edges that follow exactly on pixel boundaries for all non-interior pixels.
 * For non-thematic raster data (such as satellite images) the result will
 * essentially be one small polygon per pixel, and memory and output layer
 * sizes will be substantial.  The algorithm is primarily intended for
 * relatively simple thematic imagery, masks, and classification results.
 *
 * @param hSrcBand the source raster band to be processed.
 * @param hMaskBand an optional mask band.  All pixels in the mask band with a
 * value other than zero will be considered suitable for collection as
 * polygons.
 * @param hOutLayer the vector feature layer to which the polygons should
 * be written.
 * @param iPixValField the attribute field index indicating the feature
 * attribute into which the pixel value of the polygon should be written. Or
 * -1 to indicate that the pixel value must not be written.
 * @param papszOptions a name/value list of additional options
 * <ul>
 * <li>8CONNECTED=8: May be set to "8" to use 8 connectedness.
 * Otherwise 4 connectedness will be applied to the algorithm</li>
 * <li>DATASET_FOR_GEOREF=dataset_name: Name of a dataset from which to read
 * the geotransform. This useful if hSrcBand has no related dataset, which is
 * typical for mask bands.</li>
 * </ul>
 * @param pfnProgress callback for reporting algorithm progress matching the
 * GDALProgressFunc() semantics.  May be NULL.
 * @param pProgressArg callback argument passed to pfnProgress.
 *
 * @return CE_None on success or CE_Failure on a failure.
 *
 * @since GDAL 1.9.0
 */

CPLErr CPL_STDCALL GDALFPolygonize(GDALRasterBandH hSrcBand,
                                   GDALRasterBandH hMaskBand,
                                   OGRLayerH hOutLayer, int iPixValField,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressArg)

{
    return GDALPolygonizeT<float, FloatEqualityTest>(
        hSrcBand, hMaskBand, hOutLayer, iPixValField, papszOptions, pfnProgress,
        pProgressArg, GDT_Float32);
}
