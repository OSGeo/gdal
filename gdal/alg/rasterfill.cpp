/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Interpolate in nodata areas.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 * Copyright (c) 2015, Sean Gillies <sean@mapbox.com>
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
 ***************************************************************************/

#include "cpl_port.h"
#include "gdal_alg.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <string>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           GDALFilterLine()                           */
/*                                                                      */
/*      Apply 3x3 filtering one one scanline with masking for which     */
/*      pixels are to be interpolated (ThisFMask) and which window      */
/*      pixels are valid to include in the interpolation (TMask).       */
/************************************************************************/

static void
GDALFilterLine( const float *pafLastLine, const float *pafThisLine, const float *pafNextLine,
                float *pafOutLine,
                const GByte *pabyLastTMask, const GByte *pabyThisTMask, const GByte *pabyNextTMask,
                const GByte *pabyThisFMask, int nXSize )

{
    for( int iX = 0; iX < nXSize; iX++ )
    {
        if( !pabyThisFMask[iX] )
        {
            pafOutLine[iX] = pafThisLine[iX];
            continue;
        }

        CPLAssert( pabyThisTMask[iX] );

        double dfValSum = 0.0;
        double dfWeightSum = 0.0;

        // Previous line.
        if( pafLastLine != nullptr )
        {
            if( iX > 0 && pabyLastTMask[iX-1] )
            {
                dfValSum += pafLastLine[iX-1];
                dfWeightSum += 1.0;
            }
            if( pabyLastTMask[iX] )
            {
                dfValSum += pafLastLine[iX];
                dfWeightSum += 1.0;
            }
            if( iX < nXSize-1 && pabyLastTMask[iX+1] )
            {
                dfValSum += pafLastLine[iX+1];
                dfWeightSum += 1.0;
            }
        }

        // Current Line.
        if( iX > 0 && pabyThisTMask[iX-1] )
        {
            dfValSum += pafThisLine[iX-1];
            dfWeightSum += 1.0;
        }
        if( pabyThisTMask[iX] )
        {
            dfValSum += pafThisLine[iX];
            dfWeightSum += 1.0;
        }
        if( iX < nXSize-1 && pabyThisTMask[iX+1] )
        {
            dfValSum += pafThisLine[iX+1];
            dfWeightSum += 1.0;
        }

        // Next line.
        if( pafNextLine != nullptr )
        {
            if( iX > 0 && pabyNextTMask[iX-1] )
            {
                dfValSum += pafNextLine[iX-1];
                dfWeightSum += 1.0;
            }
            if( pabyNextTMask[iX] )
            {
                dfValSum += pafNextLine[iX];
                dfWeightSum += 1.0;
            }
            if( iX < nXSize-1 && pabyNextTMask[iX+1] )
            {
                dfValSum += pafNextLine[iX+1];
                dfWeightSum += 1.0;
            }
        }

        pafOutLine[iX] = static_cast<float>(dfValSum / dfWeightSum);
    }
}

/************************************************************************/
/*                          GDALMultiFilter()                           */
/*                                                                      */
/*      Apply multiple iterations of a 3x3 smoothing filter over a      */
/*      band with masking controlling what pixels should be             */
/*      filtered (FiltMaskBand non zero) and which pixels can be        */
/*      considered valid contributors to the filter                     */
/*      (TargetMaskBand non zero).                                      */
/*                                                                      */
/*      This implementation attempts to apply many iterations in        */
/*      one IO pass by managing the filtering over a rolling buffer     */
/*      of nIterations+2 scanlines.  While possibly clever this        */
/*      makes the algorithm implementation largely                      */
/*      incomprehensible.                                               */
/************************************************************************/

static CPLErr
GDALMultiFilter( GDALRasterBandH hTargetBand,
                 GDALRasterBandH hTargetMaskBand,
                 GDALRasterBandH hFiltMaskBand,
                 int nIterations,
                 GDALProgressFunc pfnProgress,
                 void * pProgressArg )

{
    const int nXSize = GDALGetRasterBandXSize(hTargetBand);
    const int nYSize = GDALGetRasterBandYSize(hTargetBand);

/* -------------------------------------------------------------------- */
/*      Report starting progress value.                                 */
/* -------------------------------------------------------------------- */
    if( !pfnProgress( 0.0, "Smoothing Filter...", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate rotating buffers.                                      */
/* -------------------------------------------------------------------- */
    const int nBufLines = nIterations + 2;

    GByte *pabyTMaskBuf =
        static_cast<GByte *>(VSI_MALLOC2_VERBOSE(nXSize, nBufLines));
    GByte *pabyFMaskBuf =
        static_cast<GByte *>(VSI_MALLOC2_VERBOSE(nXSize, nBufLines));

    float *paf3PassLineBuf = static_cast<float *>(
        VSI_MALLOC3_VERBOSE(nXSize, nBufLines, 3 * sizeof(float)));
    if( pabyTMaskBuf == nullptr || pabyFMaskBuf == nullptr ||
        paf3PassLineBuf == nullptr )
    {
        CPLFree( pabyTMaskBuf );
        CPLFree( pabyFMaskBuf );
        CPLFree( paf3PassLineBuf );

        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Process rotating buffers.                                       */
/* -------------------------------------------------------------------- */

    CPLErr eErr = CE_None;

    int iPassCounter = 0;

    for( int nNewLine = 0;  // Line being loaded (zero based scanline).
         eErr == CE_None && nNewLine < nYSize+nIterations;
         nNewLine++ )
    {
/* -------------------------------------------------------------------- */
/*      Rotate pass buffers.                                            */
/* -------------------------------------------------------------------- */
        iPassCounter = (iPassCounter + 1) % 3;

        float * const pafSLastPass =
            paf3PassLineBuf + ((iPassCounter + 0) % 3) * nXSize * nBufLines;
        float * const pafLastPass =
            paf3PassLineBuf + ((iPassCounter + 1) % 3) * nXSize * nBufLines;
        float * const pafThisPass =
            paf3PassLineBuf + ((iPassCounter + 2) % 3) * nXSize * nBufLines;

/* -------------------------------------------------------------------- */
/*      Where does the new line go in the rotating buffer?              */
/* -------------------------------------------------------------------- */
        const int iBufOffset = nNewLine % nBufLines;

/* -------------------------------------------------------------------- */
/*      Read the new data line if it is't off the bottom of the         */
/*      image.                                                          */
/* -------------------------------------------------------------------- */
        if( nNewLine < nYSize )
        {
            eErr =
                GDALRasterIO( hTargetMaskBand, GF_Read,
                              0, nNewLine, nXSize, 1,
                              pabyTMaskBuf + nXSize * iBufOffset, nXSize, 1,
                              GDT_Byte, 0, 0 );

            if( eErr != CE_None )
                break;

            eErr =
                GDALRasterIO( hFiltMaskBand, GF_Read,
                              0, nNewLine, nXSize, 1,
                              pabyFMaskBuf + nXSize * iBufOffset, nXSize, 1,
                              GDT_Byte, 0, 0 );

            if( eErr != CE_None )
                break;

            eErr =
                GDALRasterIO( hTargetBand, GF_Read,
                              0, nNewLine, nXSize, 1,
                              pafThisPass + nXSize * iBufOffset, nXSize, 1,
                              GDT_Float32, 0, 0 );

            if( eErr != CE_None )
                break;
        }

/* -------------------------------------------------------------------- */
/*      Loop over the loaded data, applying the filter to all loaded    */
/*      lines with neighbours.                                          */
/* -------------------------------------------------------------------- */
        for( int iFLine = nNewLine-1;
             eErr == CE_None && iFLine >= nNewLine-nIterations;
             iFLine-- )
        {
            const int iLastOffset = (iFLine-1) % nBufLines;
            const int iThisOffset = (iFLine  ) % nBufLines;
            const int iNextOffset = (iFLine+1) % nBufLines;

            // Default to preserving the old value.
            if( iFLine >= 0 )
                memcpy( pafThisPass + iThisOffset * nXSize,
                        pafLastPass + iThisOffset * nXSize,
                        sizeof(float) * nXSize );

            // TODO: Enable first and last line.
            // Skip the first and last line.
            if( iFLine < 1 || iFLine >= nYSize-1 )
            {
                continue;
            }

            GDALFilterLine(
                pafSLastPass + iLastOffset * nXSize,
                pafLastPass  + iThisOffset * nXSize,
                pafThisPass  + iNextOffset * nXSize,
                pafThisPass  + iThisOffset * nXSize,
                pabyTMaskBuf + iLastOffset * nXSize,
                pabyTMaskBuf + iThisOffset * nXSize,
                pabyTMaskBuf + iNextOffset * nXSize,
                pabyFMaskBuf + iThisOffset * nXSize,
                nXSize );
        }

/* -------------------------------------------------------------------- */
/*      Write out the top data line that will be rolling out of our     */
/*      buffer.                                                         */
/* -------------------------------------------------------------------- */
        const int iLineToSave = nNewLine - nIterations;

        if( iLineToSave >= 0 && eErr == CE_None )
        {
            const int iBufOffset2 = iLineToSave % nBufLines;

            eErr =
                GDALRasterIO( hTargetBand, GF_Write,
                              0, iLineToSave, nXSize, 1,
                              pafThisPass + nXSize * iBufOffset2, nXSize, 1,
                              GDT_Float32, 0, 0 );
        }

/* -------------------------------------------------------------------- */
/*      Report progress.                                                */
/* -------------------------------------------------------------------- */
        if( eErr == CE_None &&
            !pfnProgress(
                (nNewLine + 1) / static_cast<double>(nYSize+nIterations),
                "Smoothing Filter...", pProgressArg) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CPLFree( pabyTMaskBuf );
    CPLFree( pabyFMaskBuf );
    CPLFree( paf3PassLineBuf );

    return eErr;
}

/************************************************************************/
/*                             QUAD_CHECK()                             */
/*                                                                      */
/*      macro for checking whether a point is nearer than the           */
/*      existing closest point.                                         */
/************************************************************************/

inline void QUAD_CHECK(double& dfQuadDist, float& fQuadValue,
                       int target_x, GUInt32 target_y,
                       int origin_x, int origin_y,
                       float fTargetValue,
                       GUInt32 nNoDataVal)
{
    if( target_y != nNoDataVal )
    {
        const double dfDx =
            static_cast<double>(target_x) - static_cast<double>(origin_x);
        const double dfDy =
            static_cast<double>(target_y) - static_cast<double>(origin_y);
        double dfDistSq = dfDx * dfDx + dfDy * dfDy;

        if( dfDistSq < dfQuadDist*dfQuadDist )
        {
            CPLAssert( dfDistSq > 0.0 );
            dfQuadDist = sqrt(dfDistSq);
            fQuadValue = fTargetValue;
        }
    }
}

/************************************************************************/
/*                           GDALFillNodata()                           */
/************************************************************************/

/**
 * Fill selected raster regions by interpolation from the edges.
 *
 * This algorithm will interpolate values for all designated
 * nodata pixels (marked by zeros in hMaskBand).  For each pixel
 * a four direction conic search is done to find values to interpolate
 * from (using inverse distance weighting).  Once all values are
 * interpolated, zero or more smoothing iterations (3x3 average
 * filters on interpolated pixels) are applied to smooth out
 * artifacts.
 *
 * This algorithm is generally suitable for interpolating missing
 * regions of fairly continuously varying rasters (such as elevation
 * models for instance).  It is also suitable for filling small holes
 * and cracks in more irregularly varying images (like airphotos).  It
 * is generally not so great for interpolating a raster from sparse
 * point data - see the algorithms defined in gdal_grid.h for that case.
 *
 * @param hTargetBand the raster band to be modified in place.
 * @param hMaskBand a mask band indicating pixels to be interpolated
 * (zero valued).
 * @param dfMaxSearchDist the maximum number of pixels to search in all
 * directions to find values to interpolate from.
 * @param bDeprecatedOption unused argument, should be zero.
 * @param nSmoothingIterations the number of 3x3 smoothing filter passes to
 * run (0 or more).
 * @param papszOptions additional name=value options in a string list.
 * <ul>
 * <li>TEMP_FILE_DRIVER=gdal_driver_name. For example MEM.</li>
 * <li>NODATA=value (starting with GDAL 2.4).
 * Source pixels at that value will be ignored by the interpolator. Warning:
 * currently this will not be honored by smoothing passes.</li>
 * </ul>
 * @param pfnProgress the progress function to report completion.
 * @param pProgressArg callback data for progress function.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr CPL_STDCALL
GDALFillNodata( GDALRasterBandH hTargetBand,
                GDALRasterBandH hMaskBand,
                double dfMaxSearchDist,
                CPL_UNUSED int bDeprecatedOption,
                int nSmoothingIterations,
                char **papszOptions,
                GDALProgressFunc pfnProgress,
                void * pProgressArg )

{
    VALIDATE_POINTER1( hTargetBand, "GDALFillNodata", CE_Failure );

    const int nXSize = GDALGetRasterBandXSize(hTargetBand);
    const int nYSize = GDALGetRasterBandYSize(hTargetBand);

    if( dfMaxSearchDist == 0.0 )
        dfMaxSearchDist = std::max(nXSize, nYSize) + 1;

    const int nMaxSearchDist = static_cast<int>(floor(dfMaxSearchDist));

    // Special "x" pixel values identifying pixels as special.
    GDALDataType eType = GDT_UInt16;
    GUInt32 nNoDataVal = 65535;

    if( nXSize > 65533 || nYSize > 65533 )
    {
        eType = GDT_UInt32;
        nNoDataVal = 4000002;
    }

    if( hMaskBand == nullptr )
        hMaskBand = GDALGetMaskBand( hTargetBand );

    // If there are smoothing iterations, reserve 10% of the progress for them.
    const double dfProgressRatio = nSmoothingIterations > 0 ? 0.9 : 1.0;

    const char* pszNoData = CSLFetchNameValue(papszOptions, "NODATA");
    bool bHasNoData = false;
    float fNoData = 0.0f;
    if( pszNoData )
    {
        bHasNoData = true;
        fNoData = static_cast<float>(CPLAtof(pszNoData));
    }

/* -------------------------------------------------------------------- */
/*      Initialize progress counter.                                    */
/* -------------------------------------------------------------------- */
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    if( !pfnProgress( 0.0, "Filling...", pProgressArg ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Determine format driver for temp work files.                    */
/* -------------------------------------------------------------------- */
    CPLString osTmpFileDriver = CSLFetchNameValueDef(
            papszOptions, "TEMP_FILE_DRIVER", "GTiff");
    GDALDriverH hDriver = GDALGetDriverByName(osTmpFileDriver.c_str());

    if( hDriver == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Given driver is not registered");
        return CE_Failure;
    }

    if( GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Given driver is incapable of creating temp work files");
        return CE_Failure;
    }

    char **papszWorkFileOptions = nullptr;
    if( osTmpFileDriver == "GTiff" )
    {
        papszWorkFileOptions = CSLSetNameValue(
                papszWorkFileOptions, "COMPRESS", "LZW");
        papszWorkFileOptions = CSLSetNameValue(
                papszWorkFileOptions, "BIGTIFF", "IF_SAFER");
    }

/* -------------------------------------------------------------------- */
/*      Create a work file to hold the Y "last value" indices.          */
/* -------------------------------------------------------------------- */
    const CPLString osTmpFile = CPLGenerateTempFilename("");
    const CPLString osYTmpFile = osTmpFile + "fill_y_work.tif";

    GDALDatasetH hYDS =
        GDALCreate( hDriver, osYTmpFile, nXSize, nYSize, 1,
                    eType, papszWorkFileOptions );

    if( hYDS == nullptr )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Could not create Y index work file. Check driver capabilities.");
        return CE_Failure;
    }

    GDALRasterBandH hYBand = GDALGetRasterBand( hYDS, 1 );

/* -------------------------------------------------------------------- */
/*      Create a work file to hold the pixel value associated with      */
/*      the "last xy value" pixel.                                      */
/* -------------------------------------------------------------------- */
    const CPLString osValTmpFile = osTmpFile + "fill_val_work.tif";

    GDALDatasetH hValDS =
        GDALCreate( hDriver, osValTmpFile, nXSize, nYSize, 1,
                    GDALGetRasterDataType( hTargetBand ),
                    papszWorkFileOptions );

    if( hValDS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Could not create XY value work file. Check driver capabilities.");
        return CE_Failure;
    }

    GDALRasterBandH hValBand = GDALGetRasterBand( hValDS, 1 );

/* -------------------------------------------------------------------- */
/*      Create a mask file to make it clear what pixels can be filtered */
/*      on the filtering pass.                                          */
/* -------------------------------------------------------------------- */
    const CPLString osFiltMaskTmpFile = osTmpFile + "fill_filtmask_work.tif";

    GDALDatasetH hFiltMaskDS =
        GDALCreate( hDriver, osFiltMaskTmpFile, nXSize, nYSize, 1,
                    GDT_Byte, papszWorkFileOptions );

    if( hFiltMaskDS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Could not create mask work file. Check driver capabilities.");
        return CE_Failure;
    }

    GDALRasterBandH hFiltMaskBand = GDALGetRasterBand( hFiltMaskDS, 1 );

/* -------------------------------------------------------------------- */
/*      Allocate buffers for last scanline and this scanline.           */
/* -------------------------------------------------------------------- */

    GUInt32 *panLastY =
        static_cast<GUInt32 *>(VSI_CALLOC_VERBOSE(nXSize, sizeof(GUInt32)));
    GUInt32 *panThisY =
        static_cast<GUInt32 *>(VSI_CALLOC_VERBOSE(nXSize, sizeof(GUInt32)));
    GUInt32 *panTopDownY =
        static_cast<GUInt32 *>(VSI_CALLOC_VERBOSE(nXSize, sizeof(GUInt32)));
    float *pafLastValue =
        static_cast<float *>(VSI_CALLOC_VERBOSE(nXSize, sizeof(float)));
    float *pafThisValue =
        static_cast<float *>(VSI_CALLOC_VERBOSE(nXSize, sizeof(float)));
    float *pafTopDownValue =
        static_cast<float *>(VSI_CALLOC_VERBOSE(nXSize, sizeof(float)));
    float *pafScanline =
        static_cast<float *>(VSI_CALLOC_VERBOSE(nXSize, sizeof(float)));
    GByte *pabyMask = static_cast<GByte *>(VSI_CALLOC_VERBOSE(nXSize, 1));
    GByte *pabyFiltMask = static_cast<GByte *>(VSI_CALLOC_VERBOSE(nXSize, 1));

    CPLErr eErr = CE_None;

    if( panLastY == nullptr || panThisY == nullptr || panTopDownY == nullptr ||
        pafLastValue == nullptr || pafThisValue == nullptr ||
        pafTopDownValue == nullptr ||
        pafScanline == nullptr || pabyMask == nullptr || pabyFiltMask == nullptr )
    {
        eErr = CE_Failure;
        goto end;
    }

    for( int iX = 0; iX < nXSize; iX++ )
    {
        panLastY[iX] = nNoDataVal;
    }

/* ==================================================================== */
/*      Make first pass from top to bottom collecting the "last         */
/*      known value" for each column and writing it out to the work     */
/*      files.                                                          */
/* ==================================================================== */

    for( int iY = 0; iY < nYSize && eErr == CE_None; iY++ )
    {
/* -------------------------------------------------------------------- */
/*      Read data and mask for this line.                               */
/* -------------------------------------------------------------------- */
        eErr =
            GDALRasterIO( hMaskBand, GF_Read, 0, iY, nXSize, 1,
                          pabyMask, nXSize, 1, GDT_Byte, 0, 0 );

        if( eErr != CE_None )
            break;

        eErr =
            GDALRasterIO( hTargetBand, GF_Read, 0, iY, nXSize, 1,
                          pafScanline, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

/* -------------------------------------------------------------------- */
/*      Figure out the most recent pixel for each column.               */
/* -------------------------------------------------------------------- */

        for( int iX = 0; iX < nXSize; iX++ )
        {
            if( pabyMask[iX] )
            {
                pafThisValue[iX] = pafScanline[iX];
                panThisY[iX] = iY;
            }
            else if( iY <= dfMaxSearchDist + panLastY[iX] )
            {
                pafThisValue[iX] = pafLastValue[iX];
                panThisY[iX] = panLastY[iX];
            }
            else
            {
                panThisY[iX] = nNoDataVal;
            }
        }

/* -------------------------------------------------------------------- */
/*      Write out best index/value to working files.                    */
/* -------------------------------------------------------------------- */
        eErr = GDALRasterIO( hYBand, GF_Write, 0, iY, nXSize, 1,
                             panThisY, nXSize, 1, GDT_UInt32, 0, 0 );
        if( eErr != CE_None )
            break;

        eErr = GDALRasterIO( hValBand, GF_Write, 0, iY, nXSize, 1,
                             pafThisValue, nXSize, 1, GDT_Float32, 0, 0 );
        if( eErr != CE_None )
            break;

/* -------------------------------------------------------------------- */
/*      Flip this/last buffers.                                         */
/* -------------------------------------------------------------------- */
        std::swap(pafThisValue, pafLastValue);
        std::swap(panThisY, panLastY);

/* -------------------------------------------------------------------- */
/*      report progress.                                                */
/* -------------------------------------------------------------------- */
        if( !pfnProgress(
                dfProgressRatio * (0.5*(iY+1) /
                                   static_cast<double>(nYSize)),
                "Filling...", pProgressArg ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

    for( int iX = 0; iX < nXSize; iX++ )
    {
        panLastY[iX] = nNoDataVal;
    }

/* ==================================================================== */
/*      Now we will do collect similar this/last information from       */
/*      bottom to top and use it in combination with the top to         */
/*      bottom search info to interpolate.                              */
/* ==================================================================== */
    for( int iY = nYSize-1; iY >= 0 && eErr == CE_None; iY-- )
    {
        eErr =
            GDALRasterIO( hMaskBand, GF_Read, 0, iY, nXSize, 1,
                          pabyMask, nXSize, 1, GDT_Byte, 0, 0 );

        if( eErr != CE_None )
            break;

        eErr =
            GDALRasterIO( hTargetBand, GF_Read, 0, iY, nXSize, 1,
                          pafScanline, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

/* -------------------------------------------------------------------- */
/*      Figure out the most recent pixel for each column.               */
/* -------------------------------------------------------------------- */

        for( int iX = 0; iX < nXSize; iX++ )
        {
            if( pabyMask[iX] )
            {
                pafThisValue[iX] = pafScanline[iX];
                panThisY[iX] = iY;
            }
            else if( panLastY[iX] - iY <= dfMaxSearchDist )
            {
                pafThisValue[iX] = pafLastValue[iX];
                panThisY[iX] = panLastY[iX];
            }
            else
            {
                panThisY[iX] = nNoDataVal;
            }
        }

/* -------------------------------------------------------------------- */
/*      Load the last y and corresponding value from the top down pass. */
/* -------------------------------------------------------------------- */
        eErr =
            GDALRasterIO( hYBand, GF_Read, 0, iY, nXSize, 1,
                          panTopDownY, nXSize, 1, GDT_UInt32, 0, 0 );

        if( eErr != CE_None )
            break;

        eErr =
            GDALRasterIO( hValBand, GF_Read, 0, iY, nXSize, 1,
                          pafTopDownValue, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

/* -------------------------------------------------------------------- */
/*      Attempt to interpolate any pixels that are nodata.              */
/* -------------------------------------------------------------------- */
        memset( pabyFiltMask, 0, nXSize );
        for( int iX = 0; iX < nXSize; iX++ )
        {
            int nThisMaxSearchDist = nMaxSearchDist;

            // If this was a valid target - no change.
            if( pabyMask[iX] )
                continue;

            // Quadrants 0:topleft, 1:bottomleft, 2:topright, 3:bottomright
            double adfQuadDist[4] = {};
            float fQuadValue[4] = {};

            for( int iQuad = 0; iQuad < 4; iQuad++ )
            {
                adfQuadDist[iQuad] = dfMaxSearchDist + 1.0;
                fQuadValue[iQuad] = 0.0;
            }

            // Step left and right by one pixel searching for the closest
            // target value for each quadrant.
            for( int iStep = 0; iStep <= nThisMaxSearchDist; iStep++ )
            {
                const int iLeftX = std::max(0, iX - iStep);
                const int iRightX = std::min(nXSize - 1, iX + iStep);

                // Top left includes current line.
                QUAD_CHECK(adfQuadDist[0], fQuadValue[0],
                           iLeftX, panTopDownY[iLeftX], iX, iY,
                           pafTopDownValue[iLeftX], nNoDataVal );

                // Bottom left.
                QUAD_CHECK(adfQuadDist[1], fQuadValue[1],
                           iLeftX, panLastY[iLeftX], iX, iY,
                           pafLastValue[iLeftX], nNoDataVal );

                // Top right and bottom right do no include center pixel.
                if( iStep == 0 )
                     continue;

                // Top right includes current line.
                QUAD_CHECK(adfQuadDist[2], fQuadValue[2],
                           iRightX, panTopDownY[iRightX], iX, iY,
                           pafTopDownValue[iRightX], nNoDataVal );

                // Bottom right.
                QUAD_CHECK(adfQuadDist[3], fQuadValue[3],
                           iRightX, panLastY[iRightX], iX, iY,
                           pafLastValue[iRightX], nNoDataVal );

                // Every four steps, recompute maximum distance.
                if( (iStep & 0x3) == 0 )
                    nThisMaxSearchDist = static_cast<int>(floor(
                        std::max(std::max(adfQuadDist[0], adfQuadDist[1]),
                                 std::max(adfQuadDist[2], adfQuadDist[3]))));
            }

            double dfWeightSum = 0.0;
            double dfValueSum = 0.0;
            bool bHasSrcValues = false;

            for( int iQuad = 0; iQuad < 4; iQuad++ )
            {
                if( adfQuadDist[iQuad] <= dfMaxSearchDist )
                {
                    bHasSrcValues = true;
                    if( !bHasNoData || fQuadValue[iQuad] != fNoData )
                    {
                        const double dfWeight = 1.0 / adfQuadDist[iQuad];
                        dfWeightSum += dfWeight;
                        dfValueSum += fQuadValue[iQuad] * dfWeight;
                    }
                }
            }

            if( bHasSrcValues )
            {
                pabyFiltMask[iX] = 255;
                if( dfWeightSum > 0.0 )
                    pafScanline[iX] = static_cast<float>(dfValueSum / dfWeightSum);
                else
                    pafScanline[iX] = fNoData;
            }
        }

/* -------------------------------------------------------------------- */
/*      Write out the updated data and mask information.                */
/* -------------------------------------------------------------------- */
        eErr =
            GDALRasterIO( hTargetBand, GF_Write, 0, iY, nXSize, 1,
                          pafScanline, nXSize, 1, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            break;

        eErr =
            GDALRasterIO( hFiltMaskBand, GF_Write, 0, iY, nXSize, 1,
                          pabyFiltMask, nXSize, 1, GDT_Byte, 0, 0 );

        if( eErr != CE_None )
            break;

/* -------------------------------------------------------------------- */
/*      Flip this/last buffers.                                         */
/* -------------------------------------------------------------------- */
        std::swap(pafThisValue, pafLastValue);
        std::swap(panThisY, panLastY);

/* -------------------------------------------------------------------- */
/*      report progress.                                                */
/* -------------------------------------------------------------------- */
        if( !pfnProgress(
                dfProgressRatio*(0.5+0.5*(nYSize-iY) /
                                 static_cast<double>(nYSize)),
                "Filling...", pProgressArg) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* ==================================================================== */
/*      Now we will do iterative average filters over the               */
/*      interpolated values to smooth things out and make linear        */
/*      artifacts less obvious.                                         */
/* ==================================================================== */
    if( eErr == CE_None && nSmoothingIterations > 0 )
    {
        // Force masks to be to flushed and recomputed.
        GDALFlushRasterCache( hMaskBand );

        void *pScaledProgress =
            GDALCreateScaledProgress( dfProgressRatio, 1.0, pfnProgress, pProgressArg );

        eErr = GDALMultiFilter( hTargetBand, hMaskBand, hFiltMaskBand,
                                nSmoothingIterations,
                                GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress( pScaledProgress );
    }

/* -------------------------------------------------------------------- */
/*      Close and clean up temporary files. Free working buffers        */
/* -------------------------------------------------------------------- */
end:
    CPLFree(panLastY);
    CPLFree(panThisY);
    CPLFree(panTopDownY);
    CPLFree(pafLastValue);
    CPLFree(pafThisValue);
    CPLFree(pafTopDownValue);
    CPLFree(pafScanline);
    CPLFree(pabyMask);
    CPLFree(pabyFiltMask);

    GDALClose( hYDS );
    GDALClose( hValDS );
    GDALClose( hFiltMaskDS );

    CSLDestroy(papszWorkFileOptions);

    GDALDeleteDataset( hDriver, osYTmpFile );
    GDALDeleteDataset( hDriver, osValTmpFile );
    GDALDeleteDataset( hDriver, osFiltMaskTmpFile );

    return eErr;
}
