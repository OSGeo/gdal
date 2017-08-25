/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  Convert nearly black or nearly white border to exact black/white.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 * ****************************************************************************
 * Copyright (c) 2006, MapShots Inc (www.mapshots.com)
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <memory>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "gdal.h"

CPL_CVSID("$Id$")

typedef std::vector<int> Color;
typedef std::vector< Color > Colors;

struct GDALNearblackOptions
{
    /*! output format. The default is GeoTIFF(GTiff). Use the short format name. */
    char *pszFormat;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;

    /*! pointer to the progress data variable */
    void *pProgressData;

    int nMaxNonBlack;
    int nNearDist;
    int bNearWhite;
    int bSetAlpha;
    int bSetMask;

    Colors oColors;

    char** papszCreationOptions;
};

static void ProcessLine( GByte *pabyLine, GByte *pabyMask, int iStart,
                         int iEnd, int nSrcBands, int nDstBands, int nNearDist,
                         int nMaxNonBlack, int bNearWhite, Colors *poColors,
                         int *panLastLineCounts, int bDoHorizontalCheck,
                         int bDoVerticalCheck, int bBottomUp);

/************************************************************************/
/*                            GDALNearblack()                           */
/************************************************************************/

/**
 * Convert nearly black/white borders to exact value.
 *
 * This is the equivalent of the <a href="nearblack.html">nearblack</a> utility.
 *
 * GDALNearblackOptions* must be allocated and freed with GDALNearblackOptionsNew()
 * and GDALNearblackOptionsFree() respectively.
 * pszDest and hDstDS cannot be used at the same time.
 *
 * In-place update (i.e. hDstDS == hSrcDataset) is possible for formats that
 * support it, and if the dataset is opened in update mode.
 *
 * @param pszDest the destination dataset path or NULL.
 * @param hDstDS the destination dataset or NULL. Might be equal to hSrcDataset.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALNearblackOptionsNew() or NULL.
 * @param pbUsageError the pointer to int variable to determine any usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose(), or hDstDS is not NULL) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH CPL_DLL GDALNearblack( const char *pszDest, GDALDatasetH hDstDS,
                                    GDALDatasetH hSrcDataset,
                                    const GDALNearblackOptions *psOptionsIn, int *pbUsageError )

{
    if( pszDest == NULL && hDstDS == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "pszDest == NULL && hDstDS == NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( hSrcDataset == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "hSrcDataset== NULL");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    GDALNearblackOptions* psOptionsToFree = NULL;
    const GDALNearblackOptions* psOptions;
    if( psOptionsIn )
        psOptions = psOptionsIn;
    else
    {
        psOptionsToFree = GDALNearblackOptionsNew(NULL, NULL);
        psOptions = psOptionsToFree;
    }

    int bCloseOutDSOnError = (hDstDS == NULL);
    if( pszDest == NULL )
        pszDest = GDALGetDescription(hDstDS);

    int nXSize = GDALGetRasterXSize( hSrcDataset );
    int nYSize = GDALGetRasterYSize( hSrcDataset );
    int nBands = GDALGetRasterCount( hSrcDataset );
    int nDstBands = nBands;

    int nMaxNonBlack = psOptions->nMaxNonBlack;
    int nNearDist = psOptions->nNearDist;
    int bNearWhite = psOptions->bNearWhite;
    int bSetAlpha = psOptions->bSetAlpha;
    int bSetMask = psOptions->bSetMask;
    Colors oColors = psOptions->oColors;

/* -------------------------------------------------------------------- */
/*      Do we need to create output file?                               */
/* -------------------------------------------------------------------- */
    if( hDstDS == NULL )
    {
        GDALDriverH hDriver = GDALGetDriverByName( psOptions->pszFormat );
        if (hDriver == NULL)
        {
            GDALNearblackOptionsFree(psOptionsToFree);
            return NULL;
        }

        if (bSetAlpha)
        {
            /***** FIXME there should be a way to preserve alpha band data not in the collar *****/
            if (nBands == 4)
                nBands --;
            else
                nDstBands ++;
        }

        if (bSetMask)
        {
            if (nBands == 4)
                nDstBands = nBands = 3;
        }

        hDstDS = GDALCreate( hDriver, pszDest,
                             nXSize, nYSize, nDstBands, GDT_Byte,
                             psOptions->papszCreationOptions );
        if( hDstDS == NULL )
        {
            GDALNearblackOptionsFree(psOptionsToFree);
            return NULL;
        }

        double adfGeoTransform[6];

        if( GDALGetGeoTransform( hSrcDataset, adfGeoTransform ) == CE_None )
        {
            GDALSetGeoTransform( hDstDS, adfGeoTransform );
            GDALSetProjection( hDstDS, GDALGetProjectionRef( hSrcDataset ) );
        }
    }
    else
    {
        if( psOptions->papszCreationOptions != NULL)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Warning: creation options are ignored when writing to an existing file.");
        }

        /***** check the input and output datasets are the same size *****/
        if (GDALGetRasterXSize(hDstDS) != nXSize ||
            GDALGetRasterYSize(hDstDS) != nYSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "The dimensions of the output dataset don't match "
                    "the dimensions of the input dataset.");
            GDALNearblackOptionsFree(psOptionsToFree);
            return NULL;
        }

        if (bSetAlpha)
        {
            if (nBands != 4 &&
                (nBands < 2 ||
                 GDALGetRasterColorInterpretation(GDALGetRasterBand(hDstDS, nBands)) != GCI_AlphaBand))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Last band is not an alpha band.");
                GDALNearblackOptionsFree(psOptionsToFree);
                return NULL;
            }

            nBands --;
        }

        if (bSetMask)
        {
            if (nBands == 4)
                nDstBands = nBands = 3;
        }
    }

    /***** set a color if there are no colors set? *****/

    if ( oColors.empty()) {
        Color oColor;

        /***** loop over the bands to get the right number of values *****/

        int iBand;
        for (iBand = 0; iBand < nBands ; iBand++) {

            /***** black or white? *****/

            if (bNearWhite)
                oColor.push_back(255);
            else
                oColor.push_back(0);
        }

        /***** add the color to the colors *****/

        oColors.push_back(oColor);
    }

    /***** does the number of bands match the number of color values? *****/

    if ( static_cast<int>(oColors.front().size()) != nBands ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "-color args must have the same number of values as the non alpha input band count.\n" );
        GDALNearblackOptionsFree(psOptionsToFree);
        if( bCloseOutDSOnError )
            GDALClose(hDstDS);
        return NULL;
    }

    int iBand;
    for( iBand = 0; iBand < nBands; iBand++ )
    {
        GDALRasterBandH hBand = GDALGetRasterBand(hSrcDataset, iBand+1);
        if (GDALGetRasterDataType(hBand) != GDT_Byte)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Band %d is not of type GDT_Byte. It can lead to unexpected results.", iBand+1);
        }
        if (GDALGetRasterColorTable(hBand) != NULL)
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Band %d has a color table, which is ignored by nearblack. "
                     "It can lead to unexpected results.", iBand+1);
        }
    }

    GDALRasterBandH hMaskBand = NULL;

    if (bSetMask) {

        /***** if there isn't already a mask band on the output file create one *****/

        if ( GMF_PER_DATASET != GDALGetMaskFlags( GDALGetRasterBand(hDstDS, 1) ) )
        {

            if ( CE_None != GDALCreateDatasetMaskBand(hDstDS, GMF_PER_DATASET) ) {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Failed to create mask band on output DS");
                bSetMask = FALSE;
            }
        }

        if (bSetMask) {
            hMaskBand = GDALGetMaskBand(GDALGetRasterBand(hDstDS, 1));
        }
    }

/* -------------------------------------------------------------------- */
/*      Allocate a line buffer.                                         */
/* -------------------------------------------------------------------- */
    GByte *pabyMask=NULL;

    GByte *pabyLine = static_cast<GByte *>(CPLMalloc(nXSize * nDstBands));

    if (bSetMask)
        pabyMask = static_cast<GByte *>(CPLMalloc(nXSize));

    int *panLastLineCounts = static_cast<int *>(CPLCalloc(sizeof(int), nXSize));

/* -------------------------------------------------------------------- */
/*      Processing data one line at a time.                             */
/* -------------------------------------------------------------------- */
    int iLine;

    for( iLine = 0; iLine < nYSize; iLine++ )
    {
        CPLErr eErr;

        eErr = GDALDatasetRasterIO( hSrcDataset, GF_Read, 0, iLine, nXSize, 1,
                                    pabyLine, nXSize, 1, GDT_Byte,
                                    nBands, NULL, nDstBands, nXSize * nDstBands, 1 );
        if( eErr != CE_None )
        {
            if( bCloseOutDSOnError )
                GDALClose(hDstDS);
            hDstDS = NULL;
            break;
        }

        if (bSetAlpha)
        {
            int iCol;
            for(iCol = 0; iCol < nXSize; iCol ++)
            {
                pabyLine[iCol * nDstBands + nDstBands - 1] = 255;
            }
        }

        if (bSetMask)
        {
            int iCol;
            for(iCol = 0; iCol < nXSize; iCol ++)
            {
                pabyMask[iCol] = 255;
            }
        }

        ProcessLine( pabyLine, pabyMask, 0, nXSize-1, nBands, nDstBands,
                     nNearDist, nMaxNonBlack, bNearWhite, &oColors,
                     panLastLineCounts,
                     TRUE, // bDoHorizontalCheck
                     TRUE, // bDoVerticalCheck
                     FALSE // bBottomUp
                    );
        ProcessLine( pabyLine, pabyMask, nXSize-1, 0, nBands, nDstBands,
                     nNearDist, nMaxNonBlack, bNearWhite, &oColors,
                     panLastLineCounts,
                     TRUE,  // bDoHorizontalCheck
                     FALSE, // bDoVerticalCheck
                     FALSE  // bBottomUp
                    );

        eErr = GDALDatasetRasterIO( hDstDS, GF_Write, 0, iLine, nXSize, 1,
                                    pabyLine, nXSize, 1, GDT_Byte,
                                    nDstBands, NULL, nDstBands, nXSize * nDstBands, 1 );

        if( eErr != CE_None )
        {
            if( bCloseOutDSOnError )
                GDALClose(hDstDS);
            hDstDS = NULL;
            break;
        }

        /***** write out the mask band line *****/

        if (bSetMask) {

            eErr = GDALRasterIO ( hMaskBand, GF_Write, 0, iLine, nXSize, 1,
                                  pabyMask, nXSize, 1, GDT_Byte,
                                  0, 0 );
            if( eErr != CE_None )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "ERROR writing out line to mask band.");
                if( bCloseOutDSOnError )
                    GDALClose(hDstDS);
                hDstDS = NULL;
                break;
            }
        }

        if( !(psOptions->pfnProgress( 0.5 * ((iLine+1) / (double) nYSize), NULL, psOptions->pProgressData )) )
        {
            if( bCloseOutDSOnError )
                GDALClose(hDstDS);
            hDstDS = NULL;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Now process from the bottom back up                            .*/
/* -------------------------------------------------------------------- */
    memset( panLastLineCounts, 0, sizeof(int) * nXSize);

    for( iLine = nYSize-1; hDstDS != NULL && iLine >= 0; iLine-- )
    {
        CPLErr eErr;

        eErr = GDALDatasetRasterIO( hDstDS, GF_Read, 0, iLine, nXSize, 1,
                                    pabyLine, nXSize, 1, GDT_Byte,
                                    nDstBands, NULL, nDstBands, nXSize * nDstBands, 1 );
        if( eErr != CE_None )
        {
            if( bCloseOutDSOnError )
                GDALClose(hDstDS);
            hDstDS = NULL;
            break;
        }

        /***** read the mask band line back in *****/

        if (bSetMask) {

            eErr = GDALRasterIO ( hMaskBand, GF_Read, 0, iLine, nXSize, 1,
                                  pabyMask, nXSize, 1, GDT_Byte,
                                  0, 0 );
            if( eErr != CE_None )
            {
                if( bCloseOutDSOnError )
                    GDALClose(hDstDS);
                hDstDS = NULL;
                break;
            }
        }

        ProcessLine( pabyLine, pabyMask, 0, nXSize-1, nBands, nDstBands,
                     nNearDist, nMaxNonBlack, bNearWhite, &oColors,
                     panLastLineCounts,
                     TRUE, // bDoHorizontalCheck
                     TRUE, // bDoVerticalCheck
                     TRUE  // bBottomUp
                   );
        ProcessLine( pabyLine, pabyMask, nXSize-1, 0, nBands, nDstBands,
                     nNearDist, nMaxNonBlack, bNearWhite, &oColors,
                     panLastLineCounts,
                     TRUE,  // bDoHorizontalCheck
                     FALSE, // bDoVerticalCheck
                     TRUE   // bBottomUp
                    );

        eErr = GDALDatasetRasterIO( hDstDS, GF_Write, 0, iLine, nXSize, 1,
                                    pabyLine, nXSize, 1, GDT_Byte,
                                    nDstBands, NULL, nDstBands, nXSize * nDstBands, 1 );
        if( eErr != CE_None )
        {
            if( bCloseOutDSOnError )
                GDALClose(hDstDS);
            hDstDS = NULL;
            break;
        }

        /***** write out the mask band line *****/

        if (bSetMask) {

            eErr = GDALRasterIO ( hMaskBand, GF_Write, 0, iLine, nXSize, 1,
                                  pabyMask, nXSize, 1, GDT_Byte,
                                  0, 0 );
            if( eErr != CE_None )
            {
                if( bCloseOutDSOnError )
                    GDALClose(hDstDS);
                hDstDS = NULL;
                break;
            }
        }

        if( !(psOptions->pfnProgress( 0.5 + 0.5 * (nYSize-iLine) / (double) nYSize, NULL, psOptions->pProgressData )) )
        {
            if( bCloseOutDSOnError )
                GDALClose(hDstDS);
            hDstDS = NULL;
            break;
        }
    }

    CPLFree(pabyLine);
    if (bSetMask)
        CPLFree(pabyMask);

    CPLFree( panLastLineCounts );

    return hDstDS;
}

/************************************************************************/
/*                            ProcessLine()                             */
/*                                                                      */
/*      Process a single scanline of image data.                        */
/************************************************************************/

static void ProcessLine( GByte *pabyLine, GByte *pabyMask, int iStart,
                        int iEnd, int nSrcBands, int nDstBands, int nNearDist,
                        int nMaxNonBlack, int bNearWhite, Colors *poColors,
                        int *panLastLineCounts, int bDoHorizontalCheck,
                        int bDoVerticalCheck, int bBottomUp )
{
    int iDir, i;
    GByte nReplacevalue = 0;
    if( bNearWhite )
        nReplacevalue = 255;

    /* -------------------------------------------------------------------- */
    /*      Vertical checking.                                              */
    /* -------------------------------------------------------------------- */

    if( bDoVerticalCheck )
    {
        const int nXSize = std::max(iStart + 1, iEnd + 1);

        for( i = 0; i < nXSize; i++ )
        {

            // are we already terminated for this column?

            if( panLastLineCounts[i] > nMaxNonBlack )
                continue;

            /***** is the pixel valid data? ****/

            int bIsNonBlack = FALSE;

            /***** loop over the colors *****/

            for (int iColor = 0; iColor < static_cast<int>(poColors->size());
                 iColor++) {

                Color oColor = (*poColors)[iColor];

                bIsNonBlack = FALSE;

                /***** loop over the bands *****/

                for( int iBand = 0; iBand < nSrcBands; iBand++ )
                {
                    int nPix = pabyLine[i * nDstBands + iBand];

                    if( oColor[iBand] - nPix > nNearDist ||
                       nPix > nNearDist + oColor[iBand] )
                    {
                        bIsNonBlack = TRUE;
                        break;
                    }
                }

                if (bIsNonBlack == FALSE)
                    break;
            }

            if (bIsNonBlack) {
                panLastLineCounts[i]++;

                if( panLastLineCounts[i] > nMaxNonBlack )
                    continue;
            }
            //else
            //  panLastLineCounts[i] = 0; // not sure this even makes sense

            /***** replace the pixel values *****/

            int iBand;
            for( iBand = 0; iBand < nSrcBands; iBand++ )
                pabyLine[i * nDstBands + iBand] = nReplacevalue;

            /***** alpha *****/

            if( nDstBands > nSrcBands )
                pabyLine[i * nDstBands + nDstBands - 1] = 0;

            /***** mask *****/

            if (pabyMask != NULL)
                pabyMask[i] = 0;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Horizontal Checking.                                            */
    /* -------------------------------------------------------------------- */

    if( bDoHorizontalCheck )
    {
        int nNonBlackPixels = 0;

        /***** on a bottom up pass assume nMaxNonBlack is 0 *****/

        if (bBottomUp)
            nMaxNonBlack = 0;

        if( iStart < iEnd )
            iDir = 1;
        else
            iDir = -1;
        int bDoTest = TRUE;

        for( i = iStart; i != iEnd; i += iDir )
        {

            /***** not seen any valid data? *****/

            if ( bDoTest ) {

                /***** is the pixel valid data? ****/

                int bIsNonBlack = FALSE;

                /***** loop over the colors *****/

                for( int iColor = 0;
                     iColor < static_cast<int>(poColors->size()); iColor++ ) {

                    Color oColor = (*poColors)[iColor];

                    bIsNonBlack = FALSE;

                    /***** loop over the bands *****/

                    int iBand;
                    for( iBand = 0; iBand < nSrcBands; iBand++ )
                    {
                        int nPix = pabyLine[i * nDstBands + iBand];

                        if( oColor[iBand] - nPix > nNearDist ||
                           nPix > nNearDist + oColor[iBand] )
                        {
                            bIsNonBlack = TRUE;
                            break;
                        }
                    }

                    if (bIsNonBlack == FALSE)
                        break;
                }

                if (bIsNonBlack) {

                    /***** use nNonBlackPixels in grey areas  *****/
                    /***** from the vertical pass's grey areas ****/

                    if( panLastLineCounts[i] <= nMaxNonBlack )
                        nNonBlackPixels = panLastLineCounts[i];
                    else
                        nNonBlackPixels++;
                }

                if( nNonBlackPixels > nMaxNonBlack ) {
                    bDoTest = FALSE;
                    continue;
                }

                /***** replace the pixel values *****/

                int iBand;
                for( iBand = 0; iBand < nSrcBands; iBand++ )
                    pabyLine[i * nDstBands + iBand] = nReplacevalue;

                /***** alpha *****/

                if( nDstBands > nSrcBands )
                    pabyLine[i * nDstBands + nDstBands - 1] = 0;

                /***** mask *****/

                if (pabyMask != NULL)
                    pabyMask[i] = 0;
            }

            /***** seen valid data but test if the *****/
            /***** vertical pass saw any non valid data *****/

            else if( panLastLineCounts[i] == 0 ) {
                bDoTest = TRUE;
                nNonBlackPixels = 0;
            }
        }
    }
}

/************************************************************************/
/*                            IsInt()                                   */
/************************************************************************/

static int IsInt( const char *pszArg )
{
    if( pszArg[0] == '-' )
        pszArg++;

    if( *pszArg == '\0' )
        return FALSE;

    while( *pszArg != '\0' )
    {
        if( *pszArg < '0' || *pszArg > '9' )
            return FALSE;
        pszArg++;
    }

    return TRUE;
}

/************************************************************************/
/*                           GDALNearblackOptionsNew()              */
/************************************************************************/

/**
 * Allocates a GDALNearblackOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="nearblack.html">nearblack</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALNearblackOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALNearblackOptions struct. Must be freed with GDALNearblackOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALNearblackOptions *GDALNearblackOptionsNew(char** papszArgv,
                                                      GDALNearblackOptionsForBinary* psOptionsForBinary)
{
    GDALNearblackOptions *psOptions = new GDALNearblackOptions;

    psOptions->pszFormat = CPLStrdup("HFA");
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = NULL;
    psOptions->papszCreationOptions = NULL;
    psOptions->nMaxNonBlack = 2;
    psOptions->nNearDist = 15;
    psOptions->bNearWhite = FALSE;
    psOptions->bSetAlpha = FALSE;
    psOptions->bSetMask = FALSE;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    int argc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != NULL && i < argc; i++ )
    {
        if( i < argc-1 && (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) )
        {
            ++i;
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[i]);
            if( psOptionsForBinary )
            {
                psOptionsForBinary->bFormatExplicitlySet = TRUE;
            }
        }

        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = TRUE;
        }
        else if( i+1<argc && EQUAL(papszArgv[i],"-co")  )
        {
            psOptions->papszCreationOptions = CSLAddString( psOptions->papszCreationOptions, papszArgv[++i] );
        }
        else if( i+1<argc && EQUAL(papszArgv[i], "-o") )
        {
            i++;
            if( psOptionsForBinary )
            {
                CPLFree(psOptionsForBinary->pszOutFile);
                psOptionsForBinary->pszOutFile = CPLStrdup(papszArgv[i]);
            }
        }
        else if( EQUAL(papszArgv[i], "-white") ) {
            psOptions->bNearWhite = TRUE;
        }

        /***** -color c1,c2,c3...cn *****/

        else if( i+1<argc && EQUAL(papszArgv[i], "-color") )
        {
            Color oColor;

            /***** tokenize the arg on , *****/

            char **papszTokens;
            papszTokens = CSLTokenizeString2( papszArgv[++i], ",", 0 );

            /***** loop over the tokens *****/

            int iToken;
            for( iToken = 0; papszTokens && papszTokens[iToken]; iToken++ )
            {

                /***** ensure the token is an int and add it to the color *****/

                if ( IsInt( papszTokens[iToken] ) )
                    oColor.push_back( atoi( papszTokens[iToken] ) );
                else {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Colors must be valid integers." );
                    CSLDestroy( papszTokens );

                    GDALNearblackOptionsFree(psOptions);
                    return NULL;
                }
            }

            CSLDestroy( papszTokens );

            /***** check if the number of bands is consistent *****/

            if ( !psOptions->oColors.empty() &&
                 psOptions->oColors.front().size() != oColor.size() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "all -color args must have the same number of values.\n" );
                GDALNearblackOptionsFree(psOptions);
                return NULL;
            }

            /***** add the color to the colors *****/

            psOptions->oColors.push_back( oColor );
        }

        else if( i+1<argc && EQUAL(papszArgv[i], "-nb") )
        {
            psOptions->nMaxNonBlack = atoi(papszArgv[++i]);
        }
        else if( i+1<argc && EQUAL(papszArgv[i], "-near") )
        {
            psOptions->nNearDist = atoi(papszArgv[++i]);
        }
        else if( EQUAL(papszArgv[i], "-setalpha") )
            psOptions->bSetAlpha = TRUE;
        else if( EQUAL(papszArgv[i], "-setmask") )
            psOptions->bSetMask = TRUE;
        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALNearblackOptionsFree(psOptions);
            return NULL;
        }
        else if( psOptionsForBinary && psOptionsForBinary->pszInFile == NULL )
        {
            psOptionsForBinary->pszInFile = CPLStrdup(papszArgv[i]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            GDALNearblackOptionsFree(psOptions);
            return NULL;
        }
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->pszFormat = CPLStrdup(psOptions->pszFormat);
    }

    return psOptions;
}

/************************************************************************/
/*                       GDALNearblackOptionsFree()                     */
/************************************************************************/

/**
 * Frees the GDALNearblackOptions struct.
 *
 * @param psOptions the options struct for GDALNearblack().
 *
 * @since GDAL 2.1
 */

void GDALNearblackOptionsFree(GDALNearblackOptions *psOptions)
{
    if( psOptions )
    {
        CPLFree(psOptions->pszFormat);
        CSLDestroy(psOptions->papszCreationOptions);

        delete psOptions;
    }
}

/************************************************************************/
/*                  GDALNearblackOptionsSetProgress()                   */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALNearblack().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALNearblackOptionsSetProgress( GDALNearblackOptions *psOptions,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress ? pfnProgress : GDALDummyProgress;
    psOptions->pProgressData = pProgressData;
}
