/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Helper code to implement overview support in different drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2007-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_priv.h"

CPL_CVSID("$Id$");

typedef CPLErr (*GDALDownsampleFunction)
                      ( int nSrcWidth, int nSrcHeight,
                        GDALDataType eWrkDataType,
                        void * pChunk,
                        GByte * pabyChunkNodataMask,
                        int nChunkXOff, int nChunkXSize,
                        int nChunkYOff, int nChunkYSize,
                        int nDstXOff, int nDstXOff2,
                        int nDstYOff, int nDstYOff2,
                        GDALRasterBand * poOverview,
                        const char * pszResampling,
                        int bHasNoData, float fNoDataValue,
                        GDALColorTable* poColorTable,
                        GDALDataType eSrcDataType);

/************************************************************************/
/*                     GDALDownsampleChunk32R_Near()                    */
/************************************************************************/

template <class T>
static CPLErr
GDALDownsampleChunk32R_NearT( int nSrcWidth, int nSrcHeight,
                              GDALDataType eWrkDataType,
                              T * pChunk,
                              CPL_UNUSED GByte * pabyChunkNodataMask_unused,
                              int nChunkXOff, int nChunkXSize,
                              int nChunkYOff, CPL_UNUSED int nChunkYSize,
                              int nDstXOff, int nDstXOff2,
                              int nDstYOff, int nDstYOff2,
                              GDALRasterBand * poOverview,
                              CPL_UNUSED const char * pszResampling_unused,
                              CPL_UNUSED int bHasNoData_unused,
                              CPL_UNUSED float fNoDataValue_unused,
                              CPL_UNUSED GDALColorTable* poColorTable_unused,
                              CPL_UNUSED GDALDataType eSrcDataType)

{
    CPLErr eErr = CE_None;

    int nOXSize, nOYSize;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

    int nDstXWidth = nDstXOff2 - nDstXOff;

/* -------------------------------------------------------------------- */
/*      Allocate scanline buffer.                                       */
/* -------------------------------------------------------------------- */

    T* pDstScanline = (T *) VSIMalloc(nDstXWidth * (GDALGetDataTypeSize(eWrkDataType) / 8));
    int* panSrcXOff = (int*)VSIMalloc(nDstXWidth * sizeof(int));

    if( pDstScanline == NULL || panSrcXOff == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunk32R: Out of memory for line buffer." );
        VSIFree(pDstScanline);
        VSIFree(panSrcXOff);
        return CE_Failure;
    }

/* ==================================================================== */
/*      Precompute inner loop constants.                                */
/* ==================================================================== */
    int iDstPixel;
    for( iDstPixel = nDstXOff; iDstPixel < nDstXOff2; iDstPixel++ )
    {
        int   nSrcXOff;

        nSrcXOff =
            (int) (0.5 + (iDstPixel/(double)nOXSize) * nSrcWidth);
        if ( nSrcXOff < nChunkXOff )
            nSrcXOff = nChunkXOff;

        panSrcXOff[iDstPixel - nDstXOff] = nSrcXOff;
    }

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2 && eErr == CE_None; iDstLine++ )
    {
        T *pSrcScanline;
        int   nSrcYOff;

        nSrcYOff = (int) (0.5 + (iDstLine/(double)nOYSize) * nSrcHeight);
        if ( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        pSrcScanline = pChunk + ((nSrcYOff-nChunkYOff) * nChunkXSize) - nChunkXOff;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( iDstPixel = 0; iDstPixel < nDstXWidth; iDstPixel++ )
        {
            pDstScanline[iDstPixel] = pSrcScanline[panSrcXOff[iDstPixel]];
        }

        eErr = poOverview->RasterIO( GF_Write, nDstXOff, iDstLine, nDstXWidth, 1,
                                     pDstScanline, nDstXWidth, 1, eWrkDataType,
                                     0, 0 );
    }

    CPLFree( pDstScanline );
    CPLFree( panSrcXOff );

    return eErr;
}

static CPLErr
GDALDownsampleChunk32R_Near( int nSrcWidth, int nSrcHeight,
                        GDALDataType eWrkDataType,
                        void * pChunk,
                        GByte * pabyChunkNodataMask_unused,
                        int nChunkXOff, int nChunkXSize,
                        int nChunkYOff, int nChunkYSize,
                        int nDstXOff, int nDstXOff2,
                        int nDstYOff, int nDstYOff2,
                        GDALRasterBand * poOverview,
                        const char * pszResampling_unused,
                        int bHasNoData_unused, float fNoDataValue_unused,
                        GDALColorTable* poColorTable_unused,
                        GDALDataType eSrcDataType)
{
    if (eWrkDataType == GDT_Byte)
        return GDALDownsampleChunk32R_NearT(nSrcWidth, nSrcHeight,
                        eWrkDataType,
                        (GByte *) pChunk,
                        pabyChunkNodataMask_unused,
                        nChunkXOff, nChunkXSize,
                        nChunkYOff, nChunkYSize,
                        nDstXOff, nDstXOff2,
                        nDstYOff, nDstYOff2,
                        poOverview,
                        pszResampling_unused,
                        bHasNoData_unused, fNoDataValue_unused,
                        poColorTable_unused,
                        eSrcDataType);
    else if (eWrkDataType == GDT_Float32)
        return GDALDownsampleChunk32R_NearT(nSrcWidth, nSrcHeight,
                        eWrkDataType,
                        (float *) pChunk,
                        pabyChunkNodataMask_unused,
                        nChunkXOff, nChunkXSize,
                        nChunkYOff, nChunkYSize,
                        nDstXOff, nDstXOff2,
                        nDstYOff, nDstYOff2,
                        poOverview,
                        pszResampling_unused,
                        bHasNoData_unused, fNoDataValue_unused,
                        poColorTable_unused,
                        eSrcDataType);

    CPLAssert(0);
    return CE_Failure;
}

/************************************************************************/
/*                    GDALDownsampleChunk32R_Average()                  */
/************************************************************************/

template <class T, class Tsum>
static CPLErr
GDALDownsampleChunk32R_AverageT( int nSrcWidth, int nSrcHeight,
                                 GDALDataType eWrkDataType,
                                 T* pChunk,
                                 GByte * pabyChunkNodataMask,
                                 int nChunkXOff, int nChunkXSize,
                                 int nChunkYOff, int nChunkYSize,
                                 int nDstXOff, int nDstXOff2,
                                 int nDstYOff, int nDstYOff2,
                                 GDALRasterBand * poOverview,
                                 const char * pszResampling,
                                 int bHasNoData, float fNoDataValue,
                                 GDALColorTable* poColorTable,
                                 CPL_UNUSED GDALDataType eSrcDataType)
{
    CPLErr eErr = CE_None;

    int bBit2Grayscale = EQUALN(pszResampling,"AVERAGE_BIT2GRAYSCALE",13);
    if (bBit2Grayscale)
        poColorTable = NULL;

    int nOXSize, nOYSize;
    T    *pDstScanline;

    T      tNoDataValue = (T)fNoDataValue;
    if (!bHasNoData)
        tNoDataValue = 0;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

    int nChunkRightXOff = MIN(nSrcWidth, nChunkXOff + nChunkXSize);
    int nDstXWidth = nDstXOff2 - nDstXOff;

/* -------------------------------------------------------------------- */
/*      Allocate scanline buffer.                                       */
/* -------------------------------------------------------------------- */

    pDstScanline = (T *) VSIMalloc(nDstXWidth * (GDALGetDataTypeSize(eWrkDataType) / 8));
    int* panSrcXOffShifted = (int*)VSIMalloc(2 * nDstXWidth * sizeof(int));

    if( pDstScanline == NULL || panSrcXOffShifted == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunk32R: Out of memory for line buffer." );
        VSIFree(pDstScanline);
        VSIFree(panSrcXOffShifted);
        return CE_Failure;
    }

    int nEntryCount = 0;
    GDALColorEntry* aEntries = NULL;
    if (poColorTable)
    {
        int i;
        nEntryCount = poColorTable->GetColorEntryCount();
        aEntries = (GDALColorEntry* )CPLMalloc(sizeof(GDALColorEntry) * nEntryCount);
        for(i=0;i<nEntryCount;i++)
        {
            poColorTable->GetColorEntryAsRGB(i, &aEntries[i]);
        }
    }

/* ==================================================================== */
/*      Precompute inner loop constants.                                */
/* ==================================================================== */
    int iDstPixel;
    int bSrcXSpacingIsTwo = TRUE;
    for( iDstPixel = nDstXOff; iDstPixel < nDstXOff2; iDstPixel++ )
    {
        int   nSrcXOff, nSrcXOff2;

        nSrcXOff =
            (int) (0.5 + (iDstPixel/(double)nOXSize) * nSrcWidth);
        if ( nSrcXOff < nChunkXOff )
            nSrcXOff = nChunkXOff;
        nSrcXOff2 = (int)
            (0.5 + ((iDstPixel+1)/(double)nOXSize) * nSrcWidth);

        if( nSrcXOff2 > nChunkRightXOff || iDstPixel == nOXSize-1 )
            nSrcXOff2 = nChunkRightXOff;

        panSrcXOffShifted[2 * (iDstPixel - nDstXOff)] = nSrcXOff - nChunkXOff;
        panSrcXOffShifted[2 * (iDstPixel - nDstXOff) + 1] = nSrcXOff2 - nChunkXOff;
        if (nSrcXOff2 - nSrcXOff != 2)
            bSrcXSpacingIsTwo = FALSE;
    }

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2 && eErr == CE_None; iDstLine++ )
    {
        int   nSrcYOff, nSrcYOff2 = 0;

        nSrcYOff = (int) (0.5 + (iDstLine/(double)nOYSize) * nSrcHeight);
        if ( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        nSrcYOff2 =
            (int) (0.5 + ((iDstLine+1)/(double)nOYSize) * nSrcHeight);

        if( nSrcYOff2 > nSrcHeight || iDstLine == nOYSize-1 )
            nSrcYOff2 = nSrcHeight;
        if( nSrcYOff2 > nChunkYOff + nChunkYSize)
            nSrcYOff2 = nChunkYOff + nChunkYSize;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        if (poColorTable == NULL)
        {
            if (bSrcXSpacingIsTwo && nSrcYOff2 == nSrcYOff + 2 &&
                pabyChunkNodataMask == NULL && eWrkDataType == GDT_Byte)
            {
                /* Optimized case : no nodata, overview by a factor of 2 and regular x and y src spacing */
                T* pSrcScanlineShifted = pChunk + panSrcXOffShifted[0] + (nSrcYOff - nChunkYOff) * nChunkXSize;
                for( iDstPixel = 0; iDstPixel < nDstXWidth; iDstPixel++ )
                {
                    Tsum nTotal;

                    nTotal = pSrcScanlineShifted[0];
                    nTotal += pSrcScanlineShifted[1];
                    nTotal += pSrcScanlineShifted[nChunkXSize];
                    nTotal += pSrcScanlineShifted[1+nChunkXSize];

                    pDstScanline[iDstPixel] = (T) ((nTotal + 2) / 4);
                    pSrcScanlineShifted += 2;
                }
            }
            else
            {
                nSrcYOff -= nChunkYOff;
                nSrcYOff2 -= nChunkYOff;

                for( iDstPixel = 0; iDstPixel < nDstXWidth; iDstPixel++ )
                {
                    int  nSrcXOff = panSrcXOffShifted[2 * iDstPixel],
                         nSrcXOff2 = panSrcXOffShifted[2 * iDstPixel + 1];

                    T val;
                    Tsum dfTotal = 0;
                    int    nCount = 0, iX, iY;

                    for( iY = nSrcYOff; iY < nSrcYOff2; iY++ )
                    {
                        for( iX = nSrcXOff; iX < nSrcXOff2; iX++ )
                        {
                            val = pChunk[iX + iY *nChunkXSize];
                            if (pabyChunkNodataMask == NULL ||
                                pabyChunkNodataMask[iX + iY *nChunkXSize])
                            {
                                dfTotal += val;
                                nCount++;
                            }
                        }
                    }

                    if( nCount == 0 )
                        pDstScanline[iDstPixel] = tNoDataValue;
                    else if (eWrkDataType == GDT_Byte)
                        pDstScanline[iDstPixel] = (T) ((dfTotal + nCount / 2) / nCount);
                    else
                        pDstScanline[iDstPixel] = (T) (dfTotal / nCount);
                }
            }
        }
        else
        {
            nSrcYOff -= nChunkYOff;
            nSrcYOff2 -= nChunkYOff;

            for( iDstPixel = 0; iDstPixel < nDstXWidth; iDstPixel++ )
            {
                int  nSrcXOff = panSrcXOffShifted[2 * iDstPixel],
                     nSrcXOff2 = panSrcXOffShifted[2 * iDstPixel + 1];

                T val;
                int    nTotalR = 0, nTotalG = 0, nTotalB = 0;
                int    nCount = 0, iX, iY;

                for( iY = nSrcYOff; iY < nSrcYOff2; iY++ )
                {
                    for( iX = nSrcXOff; iX < nSrcXOff2; iX++ )
                    {
                        val = pChunk[iX + iY *nChunkXSize];
                        if (bHasNoData == FALSE || val != tNoDataValue)
                        {
                            int nVal = (int)val;
                            if (nVal >= 0 && nVal < nEntryCount)
                            {
                                nTotalR += aEntries[nVal].c1;
                                nTotalG += aEntries[nVal].c2;
                                nTotalB += aEntries[nVal].c3;
                                nCount++;
                            }
                        }
                    }
                }

                if( nCount == 0 )
                    pDstScanline[iDstPixel] = tNoDataValue;
                else
                {
                    int nR = nTotalR / nCount, nG = nTotalG / nCount, nB = nTotalB / nCount;
                    int i;
                    Tsum dfMinDist = 0;
                    int iBestEntry = 0;
                    for(i=0;i<nEntryCount;i++)
                    {
                        Tsum dfDist = (nR - aEntries[i].c1) *  (nR - aEntries[i].c1) +
                            (nG - aEntries[i].c2) *  (nG - aEntries[i].c2) +
                            (nB - aEntries[i].c3) *  (nB - aEntries[i].c3);
                        if (i == 0 || dfDist < dfMinDist)
                        {
                            dfMinDist = dfDist;
                            iBestEntry = i;
                        }
                    }
                    pDstScanline[iDstPixel] = (T)iBestEntry;
                }
            }
        }

        eErr = poOverview->RasterIO( GF_Write, nDstXOff, iDstLine, nDstXWidth, 1,
                                     pDstScanline, nDstXWidth, 1, eWrkDataType,
                                     0, 0 );
    }

    CPLFree( pDstScanline );
    CPLFree( aEntries );
    CPLFree( panSrcXOffShifted );

    return eErr;
}

static CPLErr
GDALDownsampleChunk32R_Average( int nSrcWidth, int nSrcHeight,
                        GDALDataType eWrkDataType,
                        void * pChunk,
                        GByte * pabyChunkNodataMask,
                        int nChunkXOff, int nChunkXSize,
                        int nChunkYOff, int nChunkYSize,
                        int nDstXOff, int nDstXOff2,
                        int nDstYOff, int nDstYOff2,
                        GDALRasterBand * poOverview,
                        const char * pszResampling,
                        int bHasNoData, float fNoDataValue,
                        GDALColorTable* poColorTable,
                        GDALDataType eSrcDataType)
{
    if (eWrkDataType == GDT_Byte)
        return GDALDownsampleChunk32R_AverageT<GByte, int>(nSrcWidth, nSrcHeight,
                        eWrkDataType,
                        (GByte *) pChunk,
                        pabyChunkNodataMask,
                        nChunkXOff, nChunkXSize,
                        nChunkYOff, nChunkYSize,
                        nDstXOff, nDstXOff2,
                        nDstYOff, nDstYOff2,
                        poOverview,
                        pszResampling,
                        bHasNoData, fNoDataValue,
                        poColorTable,
                        eSrcDataType);
    else if (eWrkDataType == GDT_Float32)
        return GDALDownsampleChunk32R_AverageT<float, double>(nSrcWidth, nSrcHeight,
                        eWrkDataType,
                        (float *) pChunk,
                        pabyChunkNodataMask,
                        nChunkXOff, nChunkXSize,
                        nChunkYOff, nChunkYSize,
                        nDstXOff, nDstXOff2,
                        nDstYOff, nDstYOff2,
                        poOverview,
                        pszResampling,
                        bHasNoData, fNoDataValue,
                        poColorTable,
                        eSrcDataType);

    CPLAssert(0);
    return CE_Failure;
}

/************************************************************************/
/*                    GDALDownsampleChunk32R_Gauss()                    */
/************************************************************************/

static CPLErr
GDALDownsampleChunk32R_Gauss( int nSrcWidth, int nSrcHeight,
                              CPL_UNUSED GDALDataType eWrkDataType,
                              void * pChunk,
                              GByte * pabyChunkNodataMask,
                              int nChunkXOff, int nChunkXSize,
                              int nChunkYOff, int nChunkYSize,
                              int nDstXOff, int nDstXOff2,
                              int nDstYOff, int nDstYOff2,
                              GDALRasterBand * poOverview,
                              CPL_UNUSED const char * pszResampling,
                              int bHasNoData, float fNoDataValue,
                              GDALColorTable* poColorTable,
                              CPL_UNUSED GDALDataType eSrcDataType)

{
    CPLErr eErr = CE_None;

    float * pafChunk = (float*) pChunk;

/* -------------------------------------------------------------------- */
/*      Create the filter kernel and allocate scanline buffer.          */
/* -------------------------------------------------------------------- */
    int nOXSize, nOYSize;
    float    *pafDstScanline;
    int nGaussMatrixDim = 3;
    const int *panGaussMatrix;
    static const int anGaussMatrix3x3[] ={
        1,2,1,
        2,4,2,
        1,2,1
    };
    static const int anGaussMatrix5x5[] = {
        1,4,6,4,1,
        4,16,24,16,4,
        6,24,36,24,6,
        4,16,24,16,4,
        1,4,6,4,1};
    static const int anGaussMatrix7x7[] = {
        1,6,15,20,15,6,1,
        6,36,90,120,90,36,6,
        15,90,225,300,225,90,15,
        20,120,300,400,300,120,20,
        15,90,225,300,225,90,15,
        6,36,90,120,90,36,6,
        1,6,15,20,15,6,1};

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();
    int nResYFactor = (int) (0.5 + (double)nSrcHeight/(double)nOYSize);

    // matrix for gauss filter
    if(nResYFactor <= 2 )
    {
        panGaussMatrix = anGaussMatrix3x3;
        nGaussMatrixDim=3;
    }
    else if (nResYFactor <= 4)
    {
        panGaussMatrix = anGaussMatrix5x5;
        nGaussMatrixDim=5;
    }
    else
    {
        panGaussMatrix = anGaussMatrix7x7;
        nGaussMatrixDim=7;
    }

    pafDstScanline = (float *) VSIMalloc((nDstXOff2 - nDstXOff) * sizeof(float));
    if( pafDstScanline == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunk32R: Out of memory for line buffer." );
        return CE_Failure;
    }

    int nEntryCount = 0;
    GDALColorEntry* aEntries = NULL;
    if (poColorTable)
    {
        int i;
        nEntryCount = poColorTable->GetColorEntryCount();
        aEntries = (GDALColorEntry* )CPLMalloc(sizeof(GDALColorEntry) * nEntryCount);
        for(i=0;i<nEntryCount;i++)
        {
            poColorTable->GetColorEntryAsRGB(i, &aEntries[i]);
        }
    }

    int nChunkRightXOff = MIN(nSrcWidth, nChunkXOff + nChunkXSize);

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2 && eErr == CE_None; iDstLine++ )
    {
        float *pafSrcScanline;
        GByte *pabySrcScanlineNodataMask;
        int   nSrcYOff, nSrcYOff2 = 0, iDstPixel;

        nSrcYOff = (int) (0.5 + (iDstLine/(double)nOYSize) * nSrcHeight);
        nSrcYOff2 = (int) (0.5 + ((iDstLine+1)/(double)nOYSize) * nSrcHeight) + 1;

        if( nSrcYOff < nChunkYOff )
        {
            nSrcYOff = nChunkYOff;
            nSrcYOff2++;
        }

        int iSizeY = nSrcYOff2 - nSrcYOff;
        nSrcYOff = nSrcYOff + iSizeY/2 - nGaussMatrixDim/2;
        nSrcYOff2 = nSrcYOff + nGaussMatrixDim;
        if(nSrcYOff < 0)
            nSrcYOff = 0;


        if( nSrcYOff2 > nSrcHeight || iDstLine == nOYSize-1 )
            nSrcYOff2 = nSrcHeight;
        if( nSrcYOff2 > nChunkYOff + nChunkYSize)
            nSrcYOff2 = nChunkYOff + nChunkYSize;

        pafSrcScanline = pafChunk + ((nSrcYOff-nChunkYOff) * nChunkXSize);
        if (pabyChunkNodataMask != NULL)
            pabySrcScanlineNodataMask = pabyChunkNodataMask + ((nSrcYOff-nChunkYOff) * nChunkXSize);
        else
            pabySrcScanlineNodataMask = NULL;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( iDstPixel = nDstXOff; iDstPixel < nDstXOff2; iDstPixel++ )
        {
            int   nSrcXOff, nSrcXOff2;

            nSrcXOff = (int) (0.5 + (iDstPixel/(double)nOXSize) * nSrcWidth);
            nSrcXOff2 = (int)(0.5 + ((iDstPixel+1)/(double)nOXSize) * nSrcWidth) + 1;

            int iSizeX = nSrcXOff2 - nSrcXOff;
            nSrcXOff = nSrcXOff + iSizeX/2 - nGaussMatrixDim/2;
            nSrcXOff2 = nSrcXOff + nGaussMatrixDim;
            if(nSrcXOff < 0)
                nSrcXOff = 0;

            if( nSrcXOff2 > nChunkRightXOff || iDstPixel == nOXSize-1 )
                nSrcXOff2 = nChunkRightXOff;

            if (poColorTable == NULL)
            {
                double dfTotal = 0.0, val;
                int  nCount = 0, iX, iY;
                int  i = 0,j = 0;
                const int *panLineWeight = panGaussMatrix;

                for( j=0, iY = nSrcYOff; iY < nSrcYOff2;
                        iY++, j++, panLineWeight += nGaussMatrixDim )
                {
                    for( i=0, iX = nSrcXOff; iX < nSrcXOff2; iX++,++i )
                    {
                        val = pafSrcScanline[iX-nChunkXOff+(iY-nSrcYOff)*nChunkXSize];
                        if (pabySrcScanlineNodataMask == NULL ||
                            pabySrcScanlineNodataMask[iX-nChunkXOff+(iY-nSrcYOff)*nChunkXSize])
                        {
                            int nWeight = panLineWeight[i];
                            dfTotal += val * nWeight;
                            nCount += nWeight;
                        }
                    }
                }

                if (bHasNoData && nCount == 0)
                {
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                }
                else
                {
                    if( nCount == 0 )
                        pafDstScanline[iDstPixel - nDstXOff] = 0.0;
                    else
                        pafDstScanline[iDstPixel - nDstXOff] = (float) (dfTotal / nCount);
                }
            }
            else
            {
                double val;
                int  nTotalR = 0, nTotalG = 0, nTotalB = 0;
                int  nTotalWeight = 0, iX, iY;
                int  i = 0,j = 0;
                const int *panLineWeight = panGaussMatrix;

                for( j=0, iY = nSrcYOff; iY < nSrcYOff2;
                        iY++, j++, panLineWeight += nGaussMatrixDim )
                {
                    for( i=0, iX = nSrcXOff; iX < nSrcXOff2; iX++,++i )
                    {
                        val = pafSrcScanline[iX-nChunkXOff+(iY-nSrcYOff)*nChunkXSize];
                        if (bHasNoData == FALSE || val != fNoDataValue)
                        {
                            int nVal = (int)val;
                            if (nVal >= 0 && nVal < nEntryCount)
                            {
                                int nWeight = panLineWeight[i];
                                nTotalR += aEntries[nVal].c1 * nWeight;
                                nTotalG += aEntries[nVal].c2 * nWeight;
                                nTotalB += aEntries[nVal].c3 * nWeight;
                                nTotalWeight += nWeight;
                            }
                        }
                    }
                }

                if (bHasNoData && nTotalWeight == 0)
                {
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                }
                else
                {
                    if( nTotalWeight == 0 )
                        pafDstScanline[iDstPixel - nDstXOff] = 0.0;
                    else
                    {
                        int nR = nTotalR / nTotalWeight, nG = nTotalG / nTotalWeight, nB = nTotalB / nTotalWeight;
                        int i;
                        double dfMinDist = 0;
                        int iBestEntry = 0;
                        for(i=0;i<nEntryCount;i++)
                        {
                            double dfDist = (nR - aEntries[i].c1) *  (nR - aEntries[i].c1) +
                                (nG - aEntries[i].c2) *  (nG - aEntries[i].c2) +
                                (nB - aEntries[i].c3) *  (nB - aEntries[i].c3);
                            if (i == 0 || dfDist < dfMinDist)
                            {
                                dfMinDist = dfDist;
                                iBestEntry = i;
                            }
                        }
                        pafDstScanline[iDstPixel - nDstXOff] =
                            (float) iBestEntry;
                    }
                }
            }

        }

        eErr = poOverview->RasterIO( GF_Write, nDstXOff, iDstLine, nDstXOff2 - nDstXOff, 1,
                                     pafDstScanline, nDstXOff2 - nDstXOff, 1, GDT_Float32,
                                     0, 0 );
    }

    CPLFree( pafDstScanline );
    CPLFree( aEntries );

    return eErr;
}

/************************************************************************/
/*                    GDALDownsampleChunk32R_Mode()                     */
/************************************************************************/

static CPLErr
GDALDownsampleChunk32R_Mode( int nSrcWidth, int nSrcHeight,
                             CPL_UNUSED GDALDataType eWrkDataType,
                             void * pChunk,
                             GByte * pabyChunkNodataMask,
                             int nChunkXOff, int nChunkXSize,
                             int nChunkYOff, int nChunkYSize,
                             int nDstXOff, int nDstXOff2,
                             int nDstYOff, int nDstYOff2,
                             GDALRasterBand * poOverview,
                             CPL_UNUSED const char * pszResampling,
                             int bHasNoData, float fNoDataValue,
                             GDALColorTable* poColorTable,
                             GDALDataType eSrcDataType)

{
    CPLErr eErr = CE_None;

    float * pafChunk = (float*) pChunk;

/* -------------------------------------------------------------------- */
/*      Create the filter kernel and allocate scanline buffer.          */
/* -------------------------------------------------------------------- */
    int nOXSize, nOYSize;
    float    *pafDstScanline;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

    pafDstScanline = (float *) VSIMalloc((nDstXOff2 - nDstXOff) * sizeof(float));
    if( pafDstScanline == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunk32R: Out of memory for line buffer." );
        return CE_Failure;
    }

    int nEntryCount = 0;
    GDALColorEntry* aEntries = NULL;
    if (poColorTable)
    {
        int i;
        nEntryCount = poColorTable->GetColorEntryCount();
        aEntries = (GDALColorEntry* )CPLMalloc(sizeof(GDALColorEntry) * nEntryCount);
        for(i=0;i<nEntryCount;i++)
        {
            poColorTable->GetColorEntryAsRGB(i, &aEntries[i]);
        }
    }

    int      nMaxNumPx = 0;
    float*   pafVals = NULL;
    int*     panSums = NULL;

    int nChunkRightXOff = MIN(nSrcWidth, nChunkXOff + nChunkXSize);

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2 && eErr == CE_None; iDstLine++ )
    {
        float *pafSrcScanline;
        GByte *pabySrcScanlineNodataMask;
        int   nSrcYOff, nSrcYOff2 = 0, iDstPixel;

        nSrcYOff = (int) (0.5 + (iDstLine/(double)nOYSize) * nSrcHeight);
        if ( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;

        nSrcYOff2 =
            (int) (0.5 + ((iDstLine+1)/(double)nOYSize) * nSrcHeight);

        if( nSrcYOff2 > nSrcHeight || iDstLine == nOYSize-1 )
            nSrcYOff2 = nSrcHeight;
        if( nSrcYOff2 > nChunkYOff + nChunkYSize)
            nSrcYOff2 = nChunkYOff + nChunkYSize;

        pafSrcScanline = pafChunk + ((nSrcYOff-nChunkYOff) * nChunkXSize);
        if (pabyChunkNodataMask != NULL)
            pabySrcScanlineNodataMask = pabyChunkNodataMask + ((nSrcYOff-nChunkYOff) * nChunkXSize);
        else
            pabySrcScanlineNodataMask = NULL;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( iDstPixel = nDstXOff; iDstPixel < nDstXOff2; iDstPixel++ )
        {
            int   nSrcXOff, nSrcXOff2;

            nSrcXOff =
                (int) (0.5 + (iDstPixel/(double)nOXSize) * nSrcWidth);
            if ( nSrcXOff < nChunkXOff )
                nSrcXOff = nChunkXOff;
            nSrcXOff2 = (int)
                (0.5 + ((iDstPixel+1)/(double)nOXSize) * nSrcWidth);

            if( nSrcXOff2 > nChunkRightXOff || iDstPixel == nOXSize-1 )
                nSrcXOff2 = nChunkRightXOff;

            if (eSrcDataType != GDT_Byte || nEntryCount > 256)
            {
                /* I'm not sure how much sense it makes to run a majority
                    filter on floating point data, but here it is for the sake
                    of compatability. It won't look right on RGB images by the
                    nature of the filter. */
                int     nNumPx = (nSrcYOff2-nSrcYOff)*(nSrcXOff2-nSrcXOff);
                int     iMaxInd = 0, iMaxVal = -1, iY, iX;

                if (nNumPx > nMaxNumPx)
                {
                    pafVals = (float*) CPLRealloc(pafVals, nNumPx * sizeof(float));
                    panSums = (int*) CPLRealloc(panSums, nNumPx * sizeof(int));
                    nMaxNumPx = nNumPx;
                }

                for( iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    int     iTotYOff = (iY-nSrcYOff)*nChunkXSize-nChunkXOff;
                    for( iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        if (pabySrcScanlineNodataMask == NULL ||
                            pabySrcScanlineNodataMask[iX+iTotYOff])
                        {
                            float fVal = pafSrcScanline[iX+iTotYOff];
                            int i;

                            //Check array for existing entry
                            for( i = 0; i < iMaxInd; ++i )
                                if( pafVals[i] == fVal
                                    && ++panSums[i] > panSums[iMaxVal] )
                                {
                                    iMaxVal = i;
                                    break;
                                }

                            //Add to arr if entry not already there
                            if( i == iMaxInd )
                            {
                                pafVals[iMaxInd] = fVal;
                                panSums[iMaxInd] = 1;

                                if( iMaxVal < 0 )
                                    iMaxVal = iMaxInd;

                                ++iMaxInd;
                            }
                        }
                    }
                }

                if( iMaxVal == -1 )
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                else
                    pafDstScanline[iDstPixel - nDstXOff] = pafVals[iMaxVal];
            }
            else /* if (eSrcDataType == GDT_Byte && nEntryCount < 256) */
            {
                /* So we go here for a paletted or non-paletted byte band */
                /* The input values are then between 0 and 255 */
                int     anVals[256], nMaxVal = 0, iMaxInd = -1, iY, iX;

                memset(anVals, 0, 256*sizeof(int));

                for( iY = nSrcYOff; iY < nSrcYOff2; ++iY )
                {
                    int     iTotYOff = (iY-nSrcYOff)*nChunkXSize-nChunkXOff;
                    for( iX = nSrcXOff; iX < nSrcXOff2; ++iX )
                    {
                        float  val = pafSrcScanline[iX+iTotYOff];
                        if (bHasNoData == FALSE || val != fNoDataValue)
                        {
                            int nVal = (int) val;
                            if ( ++anVals[nVal] > nMaxVal)
                            {
                                //Sum the density
                                //Is it the most common value so far?
                                iMaxInd = nVal;
                                nMaxVal = anVals[nVal];
                            }
                        }
                    }
                }

                if( iMaxInd == -1 )
                    pafDstScanline[iDstPixel - nDstXOff] = fNoDataValue;
                else
                    pafDstScanline[iDstPixel - nDstXOff] = (float)iMaxInd;
            }
        }

        eErr = poOverview->RasterIO( GF_Write, nDstXOff, iDstLine, nDstXOff2 - nDstXOff, 1,
                                     pafDstScanline, nDstXOff2 - nDstXOff, 1, GDT_Float32,
                                     0, 0 );
    }

    CPLFree( pafDstScanline );
    CPLFree( aEntries );
    CPLFree( pafVals );
    CPLFree( panSums );

    return eErr;
}

/************************************************************************/
/*                    GDALDownsampleChunk32R_Cubic()                    */
/************************************************************************/

static double GDALGetCubicWeight(double dfX)
{
    /* http://en.wikipedia.org/wiki/Bicubic_interpolation#Bicubic_convolution_algorithm */
    /* W(x) formula with a = -0.5 */
    /* or http://www.cs.utexas.edu/users/fussell/courses/cs384g/lectures/mitchell/Mitchell.pdf */
    /* k(x) (formula 8) with (B,C)=(0,0.5) */
    double dfAbsX = fabs(dfX);
    if( dfAbsX <= 1.0 )
    {
        double dfAbsX2 = dfAbsX * dfAbsX;
        return dfAbsX2 * (1.5 * dfAbsX - 2.5) + 1;
    }
    else if( dfAbsX < 2.0 )
    {
        double dfAbsX2 = dfAbsX * dfAbsX;
        return dfAbsX2 * (-0.5 * dfAbsX + 2.5) - 4 * dfAbsX + 2;
    }
    else
        return 0.0;
}

static CPLErr
GDALDownsampleChunk32R_Cubic( int nSrcWidth, int nSrcHeight,
                              CPL_UNUSED GDALDataType eWrkDataType,
                              void * pChunk,
                              GByte * pabyChunkNodataMask,
                              int nChunkXOff, int nChunkXSize,
                              int nChunkYOff, int nChunkYSize,
                              int nDstXOff, int nDstXOff2,
                              int nDstYOff, int nDstYOff2,
                              GDALRasterBand * poOverview,
                              CPL_UNUSED const char * pszResampling,
                              int bHasNoData,
                              float fNoDataValue,
                              CPL_UNUSED GDALColorTable* poColorTable,
                              CPL_UNUSED GDALDataType eSrcDataType)

{

    CPLErr eErr = CE_None;

    float * pafChunk = (float*) pChunk;
    if (!bHasNoData)
        fNoDataValue = 0.0f;

/* -------------------------------------------------------------------- */
/*      Allocate work buffers.                                          */
/* -------------------------------------------------------------------- */
    int nDstXSize = nDstXOff2 - nDstXOff;

    int nKernelRadius = 2;
    double dfXScale = (double)poOverview->GetXSize() / nSrcWidth;
    double dfXScaledRadius = nKernelRadius / dfXScale;
    double dfYScale = (double)poOverview->GetYSize() / nSrcHeight;
    double dfYScaledRadius = nKernelRadius / dfYScale;

    float* pafDstScanline = (float *) VSIMalloc(nDstXSize * sizeof(float));

    /* Temporary array to store result of horizontal filter */
    double* padfHorizontalFiltered = (double*) VSIMalloc(nChunkYSize * nDstXSize * sizeof(double));

    /* To store convolution coefficients */
    double* padfWeights = (double*) CPLMalloc((int)(2 + 2 * MAX(dfXScaledRadius, dfYScaledRadius) + 0.5) * sizeof(double));

    GByte* pabyChunkNodataMaskHorizontalFiltered = NULL;
    if( pabyChunkNodataMask )
        pabyChunkNodataMaskHorizontalFiltered = (GByte*) VSIMalloc(nChunkYSize * nDstXSize);
    if( pafDstScanline == NULL || padfHorizontalFiltered == NULL ||
        padfWeights == NULL || (pabyChunkNodataMask != NULL && pabyChunkNodataMaskHorizontalFiltered == NULL) )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunk32R_Cubic: Out of memory for work buffers." );
        VSIFree(pafDstScanline);
        VSIFree(padfHorizontalFiltered);
        VSIFree(padfWeights);
        VSIFree(pabyChunkNodataMaskHorizontalFiltered);
        return CE_Failure;
    }

/* ==================================================================== */
/*      Fist pass: horizontal filter                                    */
/* ==================================================================== */

    for( int iDstPixel = nDstXOff; iDstPixel < nDstXOff2; iDstPixel++ )
    {
        double dfSrcPixel = (iDstPixel+0.5)/dfXScale;
        int nSrcPixelStart = (int)(MAX(nChunkXOff, dfSrcPixel - dfXScaledRadius + 0.5));
        int nSrcPixelStop = (int)(MIN(nChunkXOff + nChunkXSize, dfSrcPixel + dfXScaledRadius + 0.5));
#if 0
        if( (int)(dfSrcPixel - dfXScaledRadius + 0.5) < nChunkXOff &&
            nChunkXOff > 0 )
        {
            printf("truncated iDstPixel = %d\n", iDstPixel);
        }
        if( (int)(dfSrcPixel + dfXScaledRadius + 0.5) > nChunkXOff + nChunkXSize &&
            nChunkXOff + nChunkXSize < nSrcWidth )
        {
            printf("truncated iDstPixel = %d\n", iDstPixel);
        }
#endif
        int nSrcPixelCount = nSrcPixelStop - nSrcPixelStart;
        double dfWeightSum = 0.0;

        /* Compute convolution coefficients */
        for( int nSrcPixel = nSrcPixelStart; nSrcPixel < nSrcPixelStop; nSrcPixel++)
        {
            double dfWeight = GDALGetCubicWeight(dfXScale * (nSrcPixel - dfSrcPixel + 0.5));
            padfWeights[nSrcPixel - nSrcPixelStart] = dfWeight;
            dfWeightSum += dfWeight;
        }

        if( dfWeightSum != 0 )
        {
            for(int i=0;i<nSrcPixelCount;i++)
                padfWeights[i] /= dfWeightSum;
        }

        if( pabyChunkNodataMask == NULL )
        {
            for( int iSrcLineOff = 0; iSrcLineOff < nChunkYSize; iSrcLineOff ++ )
            {
                double dfVal = 0.0;
                for(int i=0, j=iSrcLineOff * nChunkXSize + (nSrcPixelStart - nChunkXOff);
                    i<nSrcPixelCount;i++, j++)
                {
                    dfVal += pafChunk[j] * padfWeights[i];
                }
                padfHorizontalFiltered[iSrcLineOff * nDstXSize + iDstPixel - nDstXOff] = dfVal;
            }
        }
        else
        {
            for( int iSrcLineOff = 0; iSrcLineOff < nChunkYSize; iSrcLineOff ++ )
            {
                double dfVal = 0.0;
                dfWeightSum = 0.0;
                for(int i=0, j=iSrcLineOff * nChunkXSize + (nSrcPixelStart - nChunkXOff);
                    i<nSrcPixelCount;i++, j++)
                {
                    if( pabyChunkNodataMask[j] )
                    {
                        dfVal += pafChunk[j] * padfWeights[i];
                        dfWeightSum += padfWeights[i];
                    }
                }
                int nTempOffset = iSrcLineOff * nDstXSize + iDstPixel - nDstXOff;
                if( dfWeightSum > 0.0 )
                {
                    padfHorizontalFiltered[nTempOffset] = dfVal / dfWeightSum;
                    pabyChunkNodataMaskHorizontalFiltered[nTempOffset] = 1;
                }
                else
                    pabyChunkNodataMaskHorizontalFiltered[nTempOffset] = 0;
            }
        }
    }

/* ==================================================================== */
/*      Second pass: vertical filter                                    */
/* ==================================================================== */

    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; iDstLine++ )
    {
        double dfSrcLine = (iDstLine+0.5)/dfYScale;
        int nSrcLineStart = (int)(MAX(nChunkYOff, dfSrcLine - dfYScaledRadius + 0.5));
        int nSrcLineStop = (int)(MIN(nChunkYOff + nChunkYSize, dfSrcLine + dfYScaledRadius + 0.5));
#if 0
        if( (int)(dfSrcLine - dfYScaledRadius + 0.5) < nChunkYOff &&
            nChunkYOff > 0 )
        {
            printf("truncated iDstLine = %d\n", iDstLine);
        }
        if( (int)(dfSrcLine + dfYScaledRadius + 0.5) > nChunkYOff + nChunkYSize &&
            nChunkYOff + nChunkYSize < nSrcHeight )
        {
            printf("truncated iDstLine = %d\n", iDstLine);
        }
#endif
        int nSrcLineCount = nSrcLineStop - nSrcLineStart;
        double dfWeightSum = 0.0;

        /* Compute convolution coefficients */
        for( int nSrcLine = nSrcLineStart; nSrcLine < nSrcLineStop; nSrcLine++)
        {
            double dfWeight = GDALGetCubicWeight(dfYScale * (nSrcLine - dfSrcLine + 0.5));
            padfWeights[nSrcLine - nSrcLineStart] = dfWeight;
            dfWeightSum += dfWeight;
        }
        if( dfWeightSum != 0 )
        {
            for(int i=0;i<nSrcLineCount;i++)
                padfWeights[i] /= dfWeightSum;
        }

        if( pabyChunkNodataMask == NULL )
        {
            for( int iFilteredPixelOff = 0; iFilteredPixelOff < nDstXSize; iFilteredPixelOff ++ )
            {
                double dfVal = 0.0;
                for(int i=0, j=(nSrcLineStart - nChunkYOff) * nDstXSize + iFilteredPixelOff;
                    i<nSrcLineCount; i++, j+=nDstXSize)
                {
                    dfVal += padfHorizontalFiltered[j] * padfWeights[i];
                }
                pafDstScanline[iFilteredPixelOff] = (float)dfVal;
            }
        }
        else
        {
            for( int iFilteredPixelOff = 0; iFilteredPixelOff < nDstXSize; iFilteredPixelOff ++ )
            {
                double dfVal = 0.0;
                dfWeightSum = 0.0;
                for(int i=0, j=(nSrcLineStart - nChunkYOff) * nDstXSize + iFilteredPixelOff;
                    i<nSrcLineCount; i++, j+=nDstXSize)
                {
                    if( pabyChunkNodataMaskHorizontalFiltered[j] )
                    {
                        dfVal += padfHorizontalFiltered[j] * padfWeights[i];
                        dfWeightSum += padfWeights[i];
                    }
                }
                if( dfWeightSum > 0.0 )
                {
                    pafDstScanline[iFilteredPixelOff] = (float)(dfVal / dfWeightSum);
                }
                else
                {
                    pafDstScanline[iFilteredPixelOff] = fNoDataValue;
                }
            }
        }

        eErr = poOverview->RasterIO( GF_Write, nDstXOff, iDstLine, nDstXSize, 1,
                                     pafDstScanline, nDstXSize, 1, GDT_Float32,
                                     0, 0 );
    }

    VSIFree( padfWeights );
    VSIFree( padfHorizontalFiltered );
    VSIFree( pafDstScanline );
    VSIFree(pabyChunkNodataMaskHorizontalFiltered);

    return eErr;
}

/************************************************************************/
/*                       GDALDownsampleChunkC32R()                      */
/************************************************************************/

static CPLErr
GDALDownsampleChunkC32R( int nSrcWidth, int nSrcHeight, 
                         float * pafChunk, int nChunkYOff, int nChunkYSize,
                         int nDstYOff, int nDstYOff2,
                         GDALRasterBand * poOverview,
                         const char * pszResampling )
    
{
    int      nOXSize, nOYSize;
    float    *pafDstScanline;
    CPLErr   eErr = CE_None;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

    pafDstScanline = (float *) VSIMalloc(nOXSize * sizeof(float) * 2);
    if( pafDstScanline == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunkC32R: Out of memory for line buffer." );
        return CE_Failure;
    }

/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2 && eErr == CE_None; iDstLine++ )
    {
        float *pafSrcScanline;
        int   nSrcYOff, nSrcYOff2, iDstPixel;

        nSrcYOff = (int) (0.5 + (iDstLine/(double)nOYSize) * nSrcHeight);
        if( nSrcYOff < nChunkYOff )
            nSrcYOff = nChunkYOff;
        
        nSrcYOff2 = (int) (0.5 + ((iDstLine+1)/(double)nOYSize) * nSrcHeight);
        if( nSrcYOff2 > nSrcHeight || iDstLine == nOYSize-1 )
            nSrcYOff2 = nSrcHeight;
        if( nSrcYOff2 > nChunkYOff + nChunkYSize )
            nSrcYOff2 = nChunkYOff + nChunkYSize;

        pafSrcScanline = pafChunk + ((nSrcYOff-nChunkYOff) * nSrcWidth) * 2;

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( iDstPixel = 0; iDstPixel < nOXSize; iDstPixel++ )
        {
            int   nSrcXOff, nSrcXOff2;

            nSrcXOff = (int) (0.5 + (iDstPixel/(double)nOXSize) * nSrcWidth);
            nSrcXOff2 = (int) 
                (0.5 + ((iDstPixel+1)/(double)nOXSize) * nSrcWidth);
            if( nSrcXOff2 > nSrcWidth )
                nSrcXOff2 = nSrcWidth;
            
            if( EQUALN(pszResampling,"NEAR",4) )
            {
                pafDstScanline[iDstPixel*2] = pafSrcScanline[nSrcXOff*2];
                pafDstScanline[iDstPixel*2+1] = pafSrcScanline[nSrcXOff*2+1];
            }
            else if( EQUAL(pszResampling,"AVERAGE_MAGPHASE") )
            {
                double dfTotalR = 0.0, dfTotalI = 0.0, dfTotalM = 0.0;
                int    nCount = 0, iX, iY;

                for( iY = nSrcYOff; iY < nSrcYOff2; iY++ )
                {
                    for( iX = nSrcXOff; iX < nSrcXOff2; iX++ )
                    {
                        double  dfR, dfI;

                        dfR = pafSrcScanline[iX*2+(iY-nSrcYOff)*nSrcWidth*2];
                        dfI = pafSrcScanline[iX*2+(iY-nSrcYOff)*nSrcWidth*2+1];
                        dfTotalR += dfR;
                        dfTotalI += dfI;
                        dfTotalM += sqrt( dfR*dfR + dfI*dfI );
                        nCount++;
                    }
                }
                
                CPLAssert( nCount > 0 );
                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel*2] = 0.0;
                    pafDstScanline[iDstPixel*2+1] = 0.0;
                }
                else
                {
                    double      dfM, dfDesiredM, dfRatio=1.0;

                    pafDstScanline[iDstPixel*2  ] = (float) (dfTotalR/nCount);
                    pafDstScanline[iDstPixel*2+1] = (float) (dfTotalI/nCount);
                    
                    dfM = sqrt(pafDstScanline[iDstPixel*2  ]*pafDstScanline[iDstPixel*2  ]
                             + pafDstScanline[iDstPixel*2+1]*pafDstScanline[iDstPixel*2+1]);
                    dfDesiredM = dfTotalM / nCount;
                    if( dfM != 0.0 )
                        dfRatio = dfDesiredM / dfM;

                    pafDstScanline[iDstPixel*2  ] *= (float) dfRatio;
                    pafDstScanline[iDstPixel*2+1] *= (float) dfRatio;
                }
            }
            else if( EQUALN(pszResampling,"AVER",4) )
            {
                double dfTotalR = 0.0, dfTotalI = 0.0;
                int    nCount = 0, iX, iY;

                for( iY = nSrcYOff; iY < nSrcYOff2; iY++ )
                {
                    for( iX = nSrcXOff; iX < nSrcXOff2; iX++ )
                    {
                        dfTotalR += pafSrcScanline[iX*2+(iY-nSrcYOff)*nSrcWidth*2];
                        dfTotalI += pafSrcScanline[iX*2+(iY-nSrcYOff)*nSrcWidth*2+1];
                        nCount++;
                    }
                }
                
                CPLAssert( nCount > 0 );
                if( nCount == 0 )
                {
                    pafDstScanline[iDstPixel*2] = 0.0;
                    pafDstScanline[iDstPixel*2+1] = 0.0;
                }
                else
                {
                    pafDstScanline[iDstPixel*2  ] = (float) (dfTotalR/nCount);
                    pafDstScanline[iDstPixel*2+1] = (float) (dfTotalI/nCount);
                }
            }
        }

        eErr = poOverview->RasterIO( GF_Write, 0, iDstLine, nOXSize, 1, 
                                     pafDstScanline, nOXSize, 1, GDT_CFloat32, 
                                     0, 0 );
    }

    CPLFree( pafDstScanline );

    return eErr;
}

/************************************************************************/
/*                  GDALRegenerateCascadingOverviews()                  */
/*                                                                      */
/*      Generate a list of overviews in order from largest to           */
/*      smallest, computing each from the next larger.                  */
/************************************************************************/

static CPLErr
GDALRegenerateCascadingOverviews( 
    GDALRasterBand *poSrcBand, int nOverviews, GDALRasterBand **papoOvrBands, 
    const char * pszResampling, 
    GDALProgressFunc pfnProgress, void * pProgressData )

{
/* -------------------------------------------------------------------- */
/*      First, we must put the overviews in order from largest to       */
/*      smallest.                                                       */
/* -------------------------------------------------------------------- */
    int   i, j;

    for( i = 0; i < nOverviews-1; i++ )
    {
        for( j = 0; j < nOverviews - i - 1; j++ )
        {

            if( papoOvrBands[j]->GetXSize() 
                * (float) papoOvrBands[j]->GetYSize() <
                papoOvrBands[j+1]->GetXSize()
                * (float) papoOvrBands[j+1]->GetYSize() )
            {
                GDALRasterBand * poTempBand;

                poTempBand = papoOvrBands[j];
                papoOvrBands[j] = papoOvrBands[j+1];
                papoOvrBands[j+1] = poTempBand;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Count total pixels so we can prepare appropriate scaled         */
/*      progress functions.                                             */
/* -------------------------------------------------------------------- */
    double       dfTotalPixels = 0.0;

    for( i = 0; i < nOverviews; i++ )
    {
        dfTotalPixels += papoOvrBands[i]->GetXSize()
            * (double) papoOvrBands[i]->GetYSize();
    }

/* -------------------------------------------------------------------- */
/*      Generate all the bands.                                         */
/* -------------------------------------------------------------------- */
    double      dfPixelsProcessed = 0.0;

    for( i = 0; i < nOverviews; i++ )
    {
        void    *pScaledProgressData;
        double  dfPixels;
        GDALRasterBand *poBaseBand;
        CPLErr  eErr;

        if( i == 0 )
            poBaseBand = poSrcBand;
        else
            poBaseBand = papoOvrBands[i-1];

        dfPixels = papoOvrBands[i]->GetXSize() 
            * (double) papoOvrBands[i]->GetYSize();

        pScaledProgressData = GDALCreateScaledProgress( 
            dfPixelsProcessed / dfTotalPixels,
            (dfPixelsProcessed + dfPixels) / dfTotalPixels, 
            pfnProgress, pProgressData );

        eErr = GDALRegenerateOverviews( (GDALRasterBandH) poBaseBand, 
                                        1, (GDALRasterBandH *) papoOvrBands+i, 
                                        pszResampling, 
                                        GDALScaledProgress, 
                                        pScaledProgressData );
        GDALDestroyScaledProgress( pScaledProgressData );

        if( eErr != CE_None )
            return eErr;

        dfPixelsProcessed += dfPixels;

        /* we only do the bit2grayscale promotion on the base band */
        if( EQUALN(pszResampling,"AVERAGE_BIT2GRAYSCALE",13) )
            pszResampling = "AVERAGE";
    }

    return CE_None;
}

/************************************************************************/
/*                    GDALGetDownsampleFunction()                       */
/************************************************************************/

static
GDALDownsampleFunction GDALGetDownsampleFunction(const char* pszResampling)
{
    if( EQUALN(pszResampling,"NEAR",4) )
        return GDALDownsampleChunk32R_Near;
    else if( EQUALN(pszResampling,"AVER",4) )
        return GDALDownsampleChunk32R_Average;
    else if( EQUALN(pszResampling,"GAUSS",5) )
        return GDALDownsampleChunk32R_Gauss;
    else if( EQUALN(pszResampling,"MODE",4) )
        return GDALDownsampleChunk32R_Mode;
    else if( EQUALN(pszResampling,"CUBIC",5) )
        return GDALDownsampleChunk32R_Cubic;
    else
    {
       CPLError( CE_Failure, CPLE_AppDefined,
                  "GDALGetDownsampleFunction: Unsupported resampling method \"%s\".",
                  pszResampling );
        return NULL;
    }
}

/************************************************************************/
/*                      GDALGetOvrWorkDataType()                        */
/************************************************************************/

static GDALDataType GDALGetOvrWorkDataType(const char* pszResampling,
                                        GDALDataType eSrcDataType)
{
    if( (EQUALN(pszResampling,"NEAR",4) || EQUALN(pszResampling,"AVER",4)) &&
        eSrcDataType == GDT_Byte)
        return GDT_Byte;
    else
        return GDT_Float32;
}

/************************************************************************/
/*                      GDALRegenerateOverviews()                       */
/************************************************************************/

/**
 * \brief Generate downsampled overviews.
 *
 * This function will generate one or more overview images from a base
 * image using the requested downsampling algorithm.  It's primary use
 * is for generating overviews via GDALDataset::BuildOverviews(), but it
 * can also be used to generate downsampled images in one file from another
 * outside the overview architecture.
 *
 * The output bands need to exist in advance. 
 *
 * The full set of resampling algorithms is documented in 
 * GDALDataset::BuildOverviews().
 *
 * This function will honour properly NODATA_VALUES tuples (special dataset metadata) so
 * that only a given RGB triplet (in case of a RGB image) will be considered as the
 * nodata value and not each value of the triplet independantly per band.
 *
 * @param hSrcBand the source (base level) band. 
 * @param nOverviewCount the number of downsampled bands being generated.
 * @param pahOvrBands the list of downsampled bands to be generated.
 * @param pszResampling Resampling algorithm (eg. "AVERAGE"). 
 * @param pfnProgress progress report function.
 * @param pProgressData progress function callback data.
 * @return CE_None on success or CE_Failure on failure.
 */
CPLErr 
GDALRegenerateOverviews( GDALRasterBandH hSrcBand,
                         int nOverviewCount, GDALRasterBandH *pahOvrBands, 
                         const char * pszResampling, 
                         GDALProgressFunc pfnProgress, void * pProgressData )

{
    GDALRasterBand *poSrcBand = (GDALRasterBand *) hSrcBand;
    GDALRasterBand **papoOvrBands = (GDALRasterBand **) pahOvrBands;
    int    nFullResYChunk, nWidth, nHeight;
    int    nFRXBlockSize, nFRYBlockSize;
    GDALDataType eType;
    int    bHasNoData;
    float  fNoDataValue;
    GDALColorTable* poColorTable = NULL;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if( EQUAL(pszResampling,"NONE") )
        return CE_None;

    GDALDownsampleFunction pfnDownsampleFn = GDALGetDownsampleFunction(pszResampling);
    if (pfnDownsampleFn == NULL)
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Check color tables...                                           */
/* -------------------------------------------------------------------- */
    if ((EQUALN(pszResampling,"AVER",4)
         || EQUALN(pszResampling,"MODE",4)
         || EQUALN(pszResampling,"GAUSS",5)) &&
        poSrcBand->GetColorInterpretation() == GCI_PaletteIndex)
    {
        poColorTable = poSrcBand->GetColorTable();
        if (poColorTable != NULL)
        {
            if (poColorTable->GetPaletteInterpretation() != GPI_RGB)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                        "Computing overviews on palette index raster bands "
                        "with a palette whose color interpreation is not RGB "
                        "will probably lead to unexpected results.");
                poColorTable = NULL;
            }
        }
        else
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                    "Computing overviews on palette index raster bands "
                    "without a palette will probably lead to unexpected results.");
        }
    }
    // Not ready yet
    else if( EQUAL(pszResampling,"CUBIC") && poSrcBand->GetColorInterpretation() == GCI_PaletteIndex )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                    "Computing %s overviews on palette index raster bands "
                    "will probably lead to unexpected results.", pszResampling);
    }


    /* If we have a nodata mask and we are doing something more complicated */
    /* than nearest neighbouring, we have to fetch to nodata mask */ 

    GDALRasterBand* poMaskBand = NULL;
    int nMaskFlags = 0;
    int bUseNoDataMask = FALSE;

    if( !EQUALN(pszResampling,"NEAR",4) )
    {
        /* Special case if we are the alpha band. We want it to be considered */
        /* as the mask band to avoid alpha=0 to be taken into account in average */
        /* computation */
        if( poSrcBand->GetColorInterpretation() == GCI_AlphaBand )
        {
            poMaskBand = poSrcBand;
            nMaskFlags = GMF_ALPHA | GMF_PER_DATASET;
        }
        else
        {
            poMaskBand = poSrcBand->GetMaskBand();
            nMaskFlags = poSrcBand->GetMaskFlags();
        }

        bUseNoDataMask = ((nMaskFlags & GMF_ALL_VALID) == 0);
    }

    int nKernelRadius = 0;
    if( EQUAL(pszResampling,"CUBIC") )
        nKernelRadius = 2;

/* -------------------------------------------------------------------- */
/*      If we are operating on multiple overviews, and using            */
/*      averaging, lets do them in cascading order to reduce the        */
/*      amount of computation.                                          */
/* -------------------------------------------------------------------- */

    /* In case the mask made be computed from another band of the dataset, */
    /* we can't use cascaded generation, as the computation of the overviews */
    /* of the band used for the mask band may not have yet occured (#3033) */
    if( (EQUALN(pszResampling,"AVER",4) || EQUALN(pszResampling,"GAUSS",5) || EQUAL(pszResampling,"CUBIC")) && nOverviewCount > 1
         && !(bUseNoDataMask && nMaskFlags != GMF_NODATA))
        return GDALRegenerateCascadingOverviews( poSrcBand, 
                                                 nOverviewCount, papoOvrBands,
                                                 pszResampling, 
                                                 pfnProgress,
                                                 pProgressData );

/* -------------------------------------------------------------------- */
/*      Setup one horizontal swath to read from the raw buffer.         */
/* -------------------------------------------------------------------- */
    void *pChunk;
    GByte *pabyChunkNodataMask = NULL;

    poSrcBand->GetBlockSize( &nFRXBlockSize, &nFRYBlockSize );
    
    if( nFRYBlockSize < 16 || nFRYBlockSize > 256 )
        nFullResYChunk = 64;
    else
        nFullResYChunk = nFRYBlockSize;

    if( GDALDataTypeIsComplex( poSrcBand->GetRasterDataType() ) )
        eType = GDT_CFloat32;
    else
        eType = GDALGetOvrWorkDataType(pszResampling, poSrcBand->GetRasterDataType());

    nWidth = poSrcBand->GetXSize();
    nHeight = poSrcBand->GetYSize();
    
    int nMaxOvrFactor = 0;
    for( int iOverview = 0; iOverview < nOverviewCount; iOverview ++ )
    {
        int nDstWidth = papoOvrBands[iOverview]->GetXSize();
        int nDstHeight = papoOvrBands[iOverview]->GetYSize();
        nMaxOvrFactor = MAX( nMaxOvrFactor, (int)((double)nWidth / nDstWidth + 0.5) );
        nMaxOvrFactor = MAX( nMaxOvrFactor, (int)((double)nHeight / nDstHeight + 0.5) );
    }
    int nMaxChunkYSizeQueried = nFullResYChunk + 2 * nKernelRadius * nMaxOvrFactor;

    pChunk = 
        VSIMalloc3((GDALGetDataTypeSize(eType)/8), nMaxChunkYSizeQueried, nWidth );
    if (bUseNoDataMask)
    {
        pabyChunkNodataMask = (GByte *) 
            (GByte*) VSIMalloc2( nMaxChunkYSizeQueried, nWidth );
    }

    if( pChunk == NULL || (bUseNoDataMask && pabyChunkNodataMask == NULL))
    {
        CPLFree(pChunk);
        CPLFree(pabyChunkNodataMask);
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Out of memory in GDALRegenerateOverviews()." );

        return CE_Failure;
    }

    fNoDataValue = (float) poSrcBand->GetNoDataValue(&bHasNoData);

/* -------------------------------------------------------------------- */
/*      Loop over image operating on chunks.                            */
/* -------------------------------------------------------------------- */
    int  nChunkYOff = 0;
    CPLErr eErr = CE_None;

    for( nChunkYOff = 0; 
         nChunkYOff < nHeight && eErr == CE_None; 
         nChunkYOff += nFullResYChunk )
    {
        if( !pfnProgress( nChunkYOff / (double) nHeight, 
                          NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }

        if( nFullResYChunk + nChunkYOff > nHeight )
            nFullResYChunk = nHeight - nChunkYOff;
        
        int nChunkYOffQueried = nChunkYOff - nKernelRadius * nMaxOvrFactor;
        int nChunkYSizeQueried = nFullResYChunk + 2 * nKernelRadius * nMaxOvrFactor;
        if( nChunkYOffQueried < 0 )
        {
            nChunkYSizeQueried += nChunkYOffQueried;
            nChunkYOffQueried = 0;
        }
        if( nChunkYOffQueried + nChunkYSizeQueried > nHeight )
            nChunkYSizeQueried = nHeight - nChunkYOffQueried;

        /* read chunk */
        if (eErr == CE_None)
            eErr = poSrcBand->RasterIO( GF_Read, 0, nChunkYOffQueried, nWidth, nChunkYSizeQueried, 
                                pChunk, nWidth, nChunkYSizeQueried, eType,
                                0, 0 );
        if (eErr == CE_None && bUseNoDataMask)
            eErr = poMaskBand->RasterIO( GF_Read, 0, nChunkYOffQueried, nWidth, nChunkYSizeQueried, 
                                pabyChunkNodataMask, nWidth, nChunkYSizeQueried, GDT_Byte,
                                0, 0 );

        /* special case to promote 1bit data to 8bit 0/255 values */
        if( EQUAL(pszResampling,"AVERAGE_BIT2GRAYSCALE") )
        {
            int i;

            if (eType == GDT_Float32)
            {
                float* pafChunk = (float*)pChunk;
                for( i = nChunkYSizeQueried*nWidth - 1; i >= 0; i-- )
                {
                    if( pafChunk[i] == 1.0 )
                        pafChunk[i] = 255.0;
                }
            }
            else if (eType == GDT_Byte)
            {
                GByte* pabyChunk = (GByte*)pChunk;
                for( i = nChunkYSizeQueried*nWidth - 1; i >= 0; i-- )
                {
                    if( pabyChunk[i] == 1 )
                        pabyChunk[i] = 255;
                }
            }
            else {
                CPLAssert(0);
            }
        }
        else if( EQUAL(pszResampling,"AVERAGE_BIT2GRAYSCALE_MINISWHITE") )
        {
            int i;

            if (eType == GDT_Float32)
            {
                float* pafChunk = (float*)pChunk;
                for( i = nChunkYSizeQueried*nWidth - 1; i >= 0; i-- )
                {
                    if( pafChunk[i] == 1.0 )
                        pafChunk[i] = 0.0;
                    else if( pafChunk[i] == 0.0 )
                        pafChunk[i] = 255.0;
                }
            }
            else if (eType == GDT_Byte)
            {
                GByte* pabyChunk = (GByte*)pChunk;
                for( i = nChunkYSizeQueried*nWidth - 1; i >= 0; i-- )
                {
                    if( pabyChunk[i] == 1 )
                        pabyChunk[i] = 0;
                    else if( pabyChunk[i] == 0 )
                        pabyChunk[i] = 255;
                }
            }
            else {
                CPLAssert(0);
            }
        }

        for( int iOverview = 0; iOverview < nOverviewCount && eErr == CE_None; iOverview++ )
        {
            int nDstWidth = papoOvrBands[iOverview]->GetXSize();
            int nDstHeight = papoOvrBands[iOverview]->GetYSize();

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
            int nDstYOff = (int) (0.5 + (nChunkYOff/(double)nHeight) * nDstHeight);
            int nDstYOff2 = (int)
                (0.5 + ((nChunkYOff+nFullResYChunk)/(double)nHeight) * nDstHeight);

            if( nChunkYOff + nFullResYChunk == nHeight )
                nDstYOff2 = nDstHeight;

            if( eType == GDT_Byte || eType == GDT_Float32 )
                eErr = pfnDownsampleFn(nWidth, nHeight,
                                              eType,
                                              pChunk,
                                              pabyChunkNodataMask,
                                              0, nWidth,
                                              nChunkYOffQueried, nChunkYSizeQueried,
                                              0, nDstWidth,
                                              nDstYOff, nDstYOff2,
                                              papoOvrBands[iOverview], pszResampling,
                                              bHasNoData, fNoDataValue, poColorTable,
                                              poSrcBand->GetRasterDataType());
            else
                eErr = GDALDownsampleChunkC32R(nWidth, nHeight, 
                                               (float*)pChunk,
                                               nChunkYOffQueried, nChunkYSizeQueried,
                                               nDstYOff, nDstYOff2,
                                               papoOvrBands[iOverview], pszResampling);
        }
    }

    VSIFree( pChunk );
    VSIFree( pabyChunkNodataMask );
    
/* -------------------------------------------------------------------- */
/*      Renormalized overview mean / stddev if needed.                  */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && EQUAL(pszResampling,"AVERAGE_MP") )
    {
        GDALOverviewMagnitudeCorrection( (GDALRasterBandH) poSrcBand, 
                                         nOverviewCount, 
                                         (GDALRasterBandH *) papoOvrBands,
                                         GDALDummyProgress, NULL );
    }

/* -------------------------------------------------------------------- */
/*      It can be important to flush out data to overviews.             */
/* -------------------------------------------------------------------- */
    for( int iOverview = 0; 
         eErr == CE_None && iOverview < nOverviewCount; 
         iOverview++ )
    {
        eErr = papoOvrBands[iOverview]->FlushCache();
    }

    if (eErr == CE_None)
        pfnProgress( 1.0, NULL, pProgressData );

    return eErr;
}



/************************************************************************/
/*            GDALRegenerateOverviewsMultiBand()                        */
/************************************************************************/

/**
 * \brief Variant of GDALRegenerateOverviews, specialy dedicated for generating
 * compressed pixel-interleaved overviews (JPEG-IN-TIFF for example)
 *
 * This function will generate one or more overview images from a base
 * image using the requested downsampling algorithm.  It's primary use
 * is for generating overviews via GDALDataset::BuildOverviews(), but it
 * can also be used to generate downsampled images in one file from another
 * outside the overview architecture.
 *
 * The output bands need to exist in advance and share the same characteristics
 * (type, dimensions)
 *
 * The resampling algorithms supported for the moment are "NEAREST", "AVERAGE"
 * and "GAUSS"
 *
 * The pseudo-algorithm used by the function is :
 *    for each overview
 *       iterate on lines of the source by a step of deltay
 *           iterate on columns of the source  by a step of deltax
 *               read the source data of size deltax * deltay for all the bands
 *               generate the corresponding overview block for all the bands
 *
 * This function will honour properly NODATA_VALUES tuples (special dataset metadata) so
 * that only a given RGB triplet (in case of a RGB image) will be considered as the
 * nodata value and not each value of the triplet independantly per band.
 *
 * @param nBands the number of bands, size of papoSrcBands and size of
 *               first dimension of papapoOverviewBands
 * @param papoSrcBands the list of source bands to downsample
 * @param nOverviews the number of downsampled overview levels being generated.
 * @param papapoOverviewBands bidimension array of bands. First dimension is indexed
 *                            by nBands. Second dimension is indexed by nOverviews.
 * @param pszResampling Resampling algorithm ("NEAREST", "AVERAGE" or "GAUSS"). 
 * @param pfnProgress progress report function.
 * @param pProgressData progress function callback data.
 * @return CE_None on success or CE_Failure on failure.
 */

CPLErr 
GDALRegenerateOverviewsMultiBand(int nBands, GDALRasterBand** papoSrcBands,
                                 int nOverviews,
                                 GDALRasterBand*** papapoOverviewBands,
                                 const char * pszResampling, 
                                 GDALProgressFunc pfnProgress, void * pProgressData )
{
    CPLErr eErr = CE_None;
    int iOverview, iBand;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    if( EQUAL(pszResampling,"NONE") )
        return CE_None;

    /* Sanity checks */
    if (!EQUALN(pszResampling, "NEAR", 4) &&
        !EQUAL(pszResampling, "AVERAGE") &&
        !EQUAL(pszResampling, "GAUSS") &&
        !EQUAL(pszResampling, "CUBIC"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALRegenerateOverviewsMultiBand: pszResampling='%s' not supported", pszResampling);
        return CE_Failure;
    }

    int nKernelRadius = 0;
    if( EQUAL(pszResampling,"CUBIC") )
        nKernelRadius = 2;

    GDALDownsampleFunction pfnDownsampleFn = GDALGetDownsampleFunction(pszResampling);
    if (pfnDownsampleFn == NULL)
        return CE_Failure;

    int nSrcWidth = papoSrcBands[0]->GetXSize();
    int nSrcHeight = papoSrcBands[0]->GetYSize();
    GDALDataType eDataType = papoSrcBands[0]->GetRasterDataType();
    for(iBand=1;iBand<nBands;iBand++)
    {
        if (papoSrcBands[iBand]->GetXSize() != nSrcWidth ||
            papoSrcBands[iBand]->GetYSize() != nSrcHeight)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "GDALRegenerateOverviewsMultiBand: all the source bands must have the same dimensions");
            return CE_Failure;
        }
        if (papoSrcBands[iBand]->GetRasterDataType() != eDataType)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                    "GDALRegenerateOverviewsMultiBand: all the source bands must have the same data type");
            return CE_Failure;
        }
    }

    for(iOverview=0;iOverview<nOverviews;iOverview++)
    {
        int nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        int nDstHeight = papapoOverviewBands[0][iOverview]->GetYSize();
        for(iBand=1;iBand<nBands;iBand++)
        {
            if (papapoOverviewBands[iBand][iOverview]->GetXSize() != nDstWidth ||
                papapoOverviewBands[iBand][iOverview]->GetYSize() != nDstHeight)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "GDALRegenerateOverviewsMultiBand: all the overviews bands of the same level must have the same dimensions");
                return CE_Failure;
            }
            if (papapoOverviewBands[iBand][iOverview]->GetRasterDataType() != eDataType)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                        "GDALRegenerateOverviewsMultiBand: all the overviews bands must have the same data type as the source bands");
                return CE_Failure;
            }
        }
    }

    /* First pass to compute the total number of pixels to read */
    double dfTotalPixelCount = 0;
    for(iOverview=0;iOverview<nOverviews;iOverview++)
    {
        nSrcWidth = papoSrcBands[0]->GetXSize();
        nSrcHeight = papoSrcBands[0]->GetYSize();

        int nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        /* Try to use previous level of overview as the source to compute */
        /* the next level */
        if (iOverview > 0 && papapoOverviewBands[0][iOverview - 1]->GetXSize() > nDstWidth)
        {
            nSrcWidth = papapoOverviewBands[0][iOverview - 1]->GetXSize();
            nSrcHeight = papapoOverviewBands[0][iOverview - 1]->GetYSize();
        }

        dfTotalPixelCount += (double)nSrcWidth * nSrcHeight;
    }

    nSrcWidth = papoSrcBands[0]->GetXSize();
    nSrcHeight = papoSrcBands[0]->GetYSize();

    GDALDataType eWrkDataType = GDALGetOvrWorkDataType(pszResampling, eDataType);

    /* If we have a nodata mask and we are doing something more complicated */
    /* than nearest neighbouring, we have to fetch to nodata mask */ 
    int bUseNoDataMask = (!EQUALN(pszResampling,"NEAR",4) &&
                          (papoSrcBands[0]->GetMaskFlags() & GMF_ALL_VALID) == 0);

    int* pabHasNoData = (int*)CPLMalloc(nBands * sizeof(int));
    float* pafNoDataValue = (float*)CPLMalloc(nBands * sizeof(float));

    for(iBand=0;iBand<nBands;iBand++)
    {
        pabHasNoData[iBand] = FALSE;
        pafNoDataValue[iBand] = (float) papoSrcBands[iBand]->GetNoDataValue(&pabHasNoData[iBand]);
    }

    /* Second pass to do the real job ! */
    double dfCurPixelCount = 0;
    for(iOverview=0;iOverview<nOverviews && eErr == CE_None;iOverview++)
    {
        int iSrcOverview = -1; /* -1 means the source bands */

        int nDstBlockXSize, nDstBlockYSize;
        int nDstWidth, nDstHeight;
        papapoOverviewBands[0][iOverview]->GetBlockSize(&nDstBlockXSize, &nDstBlockYSize);
        nDstWidth = papapoOverviewBands[0][iOverview]->GetXSize();
        nDstHeight = papapoOverviewBands[0][iOverview]->GetYSize();

        /* Try to use previous level of overview as the source to compute */
        /* the next level */
        if (iOverview > 0 && papapoOverviewBands[0][iOverview - 1]->GetXSize() > nDstWidth)
        {
            nSrcWidth = papapoOverviewBands[0][iOverview - 1]->GetXSize();
            nSrcHeight = papapoOverviewBands[0][iOverview - 1]->GetYSize();
            iSrcOverview = iOverview - 1;
        }

        /* Compute the maximum chunck size of the source such as it will match the size of */
        /* a block of the overview */
        int nFullResXChunk = 1 + (int)(((double)nDstBlockXSize * nSrcWidth) / nDstWidth);
        int nFullResYChunk = 1 + (int)(((double)nDstBlockYSize * nSrcHeight) / nDstHeight);
        
        int nOvrFactor = MAX( (int)(0.5 + (double)nSrcWidth / nDstWidth),
                              (int)(0.5 + (double)nSrcHeight / nDstHeight) );
        int nFullResXChunkQueried = nFullResXChunk + 2 * nKernelRadius * nOvrFactor;
        int nFullResYChunkQueried = nFullResYChunk + 2 * nKernelRadius * nOvrFactor;

        void** papaChunk = (void**) CPLMalloc(nBands * sizeof(void*));
        GByte* pabyChunkNoDataMask = NULL;
        for(iBand=0;iBand<nBands;iBand++)
        {
            papaChunk[iBand] = VSIMalloc3(nFullResXChunkQueried, nFullResYChunkQueried, GDALGetDataTypeSize(eWrkDataType) / 8);
            if( papaChunk[iBand] == NULL )
            {
                while ( --iBand >= 0)
                    CPLFree(papaChunk[iBand]);
                CPLFree(papaChunk);
                CPLFree(pabHasNoData);
                CPLFree(pafNoDataValue);

                CPLError( CE_Failure, CPLE_OutOfMemory,
                        "GDALRegenerateOverviewsMultiBand: Out of memory." );
                return CE_Failure;
            }
        }
        if (bUseNoDataMask)
        {
            pabyChunkNoDataMask = (GByte*) VSIMalloc2(nFullResXChunkQueried, nFullResYChunkQueried);
            if( pabyChunkNoDataMask == NULL )
            {
                for(iBand=0;iBand<nBands;iBand++)
                {
                    CPLFree(papaChunk[iBand]);
                }
                CPLFree(papaChunk);
                CPLFree(pabHasNoData);
                CPLFree(pafNoDataValue);

                CPLError( CE_Failure, CPLE_OutOfMemory,
                        "GDALRegenerateOverviewsMultiBand: Out of memory." );
                return CE_Failure;
            }
        }

        int nDstYOff;
        /* Iterate on destination overview, block by block */
        for( nDstYOff = 0; nDstYOff < nDstHeight && eErr == CE_None; nDstYOff += nDstBlockYSize )
        {
            int nDstYCount;
            if  (nDstYOff + nDstBlockYSize <= nDstHeight)
                nDstYCount = nDstBlockYSize;
            else
                nDstYCount = nDstHeight - nDstYOff;

            int nChunkYOff = (int) (0.5 + nDstYOff / (double)nDstHeight * nSrcHeight);
            int nChunkYOff2 = (int) (0.5 + (nDstYOff + nDstBlockYSize) / (double)nDstHeight * nSrcHeight);
            if( nChunkYOff2 > nSrcHeight || nDstYOff + nDstBlockYSize == nDstHeight)
                nChunkYOff2 = nSrcHeight;
            int nYCount = nChunkYOff2 - nChunkYOff;
            CPLAssert(nYCount <= nFullResYChunk);

            int nChunkYOffQueried = nChunkYOff - nKernelRadius * nOvrFactor;
            int nChunkYSizeQueried = nYCount + 2 * nKernelRadius * nOvrFactor;
            if( nChunkYOffQueried < 0 )
            {
                nChunkYSizeQueried += nChunkYOffQueried;
                nChunkYOffQueried = 0;
            }
            if( nChunkYSizeQueried + nChunkYOffQueried > nSrcHeight )
                nChunkYSizeQueried = nSrcHeight - nChunkYOffQueried;
            CPLAssert(nChunkYSizeQueried <= nFullResYChunkQueried);

            if( !pfnProgress( dfCurPixelCount / dfTotalPixelCount, 
                              NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                eErr = CE_Failure;
            }

            int nDstXOff;
            /* Iterate on destination overview, block by block */
            for( nDstXOff = 0; nDstXOff < nDstWidth && eErr == CE_None; nDstXOff += nDstBlockXSize )
            {
                int nDstXCount;
                if  (nDstXOff + nDstBlockXSize <= nDstWidth)
                    nDstXCount = nDstBlockXSize;
                else
                    nDstXCount = nDstWidth - nDstXOff;

                int nChunkXOff = (int) (0.5 + nDstXOff / (double)nDstWidth * nSrcWidth);
                int nChunkXOff2 = (int) (0.5 + (nDstXOff + nDstBlockXSize) / (double)nDstWidth * nSrcWidth);
                if( nChunkXOff2 > nSrcWidth || nDstXOff + nDstBlockXSize == nDstWidth)
                    nChunkXOff2 = nSrcWidth;
                int nXCount = nChunkXOff2 - nChunkXOff;
                CPLAssert(nXCount <= nFullResXChunk);

                int nChunkXOffQueried = nChunkXOff - nKernelRadius * nOvrFactor;
                int nChunkXSizeQueried = nXCount + 2 * nKernelRadius * nOvrFactor;
                if( nChunkXOffQueried < 0 )
                {
                    nChunkXSizeQueried += nChunkXOffQueried;
                    nChunkXOffQueried = 0;
                }
                if( nChunkXSizeQueried + nChunkXOffQueried > nSrcWidth )
                    nChunkXSizeQueried = nSrcWidth - nChunkXOffQueried;
                CPLAssert(nChunkXSizeQueried <= nFullResXChunkQueried);
                /*CPLDebug("GDAL", "Reading (%dx%d -> %dx%d) for output (%dx%d -> %dx%d)",
                         nChunkXOff, nChunkYOff, nXCount, nYCount,
                         nDstXOff, nDstYOff, nDstXCount, nDstYCount);*/

                /* Read the source buffers for all the bands */
                for(iBand=0;iBand<nBands && eErr == CE_None;iBand++)
                {
                    GDALRasterBand* poSrcBand;
                    if (iSrcOverview == -1)
                        poSrcBand = papoSrcBands[iBand];
                    else
                        poSrcBand = papapoOverviewBands[iBand][iSrcOverview];
                    eErr = poSrcBand->RasterIO( GF_Read,
                                                nChunkXOffQueried, nChunkYOffQueried,
                                                nChunkXSizeQueried, nChunkYSizeQueried, 
                                                papaChunk[iBand],
                                                nChunkXSizeQueried, nChunkYSizeQueried,
                                                eWrkDataType, 0, 0 );
                }

                if (bUseNoDataMask && eErr == CE_None)
                {
                    GDALRasterBand* poSrcBand;
                    if (iSrcOverview == -1)
                        poSrcBand = papoSrcBands[0];
                    else
                        poSrcBand = papapoOverviewBands[0][iSrcOverview];
                    eErr = poSrcBand->GetMaskBand()->RasterIO( GF_Read,
                                                               nChunkXOffQueried, nChunkYOffQueried,
                                                               nChunkXSizeQueried, nChunkYSizeQueried, 
                                                               pabyChunkNoDataMask,
                                                               nChunkXSizeQueried, nChunkYSizeQueried,
                                                               GDT_Byte, 0, 0 );
                }

                /* Compute the resulting overview block */
                for(iBand=0;iBand<nBands && eErr == CE_None;iBand++)
                {
                    eErr = pfnDownsampleFn(nSrcWidth, nSrcHeight,
                                                  eWrkDataType,
                                                  papaChunk[iBand],
                                                  pabyChunkNoDataMask,
                                                  nChunkXOffQueried, nChunkXSizeQueried,
                                                  nChunkYOffQueried, nChunkYSizeQueried,
                                                  nDstXOff, nDstXOff + nDstXCount,
                                                  nDstYOff, nDstYOff + nDstYCount,
                                                  papapoOverviewBands[iBand][iOverview],
                                                  pszResampling,
                                                  pabHasNoData[iBand],
                                                  pafNoDataValue[iBand],
                                                  /*poColorTable*/ NULL,
                                                  eDataType);
                }
            }

            dfCurPixelCount += (double)nYCount * nSrcWidth;
        }

        /* Flush the data to overviews */
        for(iBand=0;iBand<nBands;iBand++)
        {
            CPLFree(papaChunk[iBand]);
            papapoOverviewBands[iBand][iOverview]->FlushCache();
        }
        CPLFree(papaChunk);
        CPLFree(pabyChunkNoDataMask);

    }

    CPLFree(pabHasNoData);
    CPLFree(pafNoDataValue);

    if (eErr == CE_None)
        pfnProgress( 1.0, NULL, pProgressData );

    return eErr;
}


/************************************************************************/
/*                        GDALComputeBandStats()                        */
/************************************************************************/

CPLErr CPL_STDCALL 
GDALComputeBandStats( GDALRasterBandH hSrcBand,
                      int nSampleStep,
                      double *pdfMean, double *pdfStdDev, 
                      GDALProgressFunc pfnProgress, 
                      void *pProgressData )

{
    VALIDATE_POINTER1( hSrcBand, "GDALComputeBandStats", CE_Failure );

    GDALRasterBand *poSrcBand = (GDALRasterBand *) hSrcBand;
    int         iLine, nWidth, nHeight;
    GDALDataType eType = poSrcBand->GetRasterDataType();
    GDALDataType eWrkType;
    int         bComplex;
    float       *pafData;
    double      dfSum=0.0, dfSum2=0.0;
    int         nSamples = 0;

    if( pfnProgress == NULL )
        pfnProgress = GDALDummyProgress;

    nWidth = poSrcBand->GetXSize();
    nHeight = poSrcBand->GetYSize();

    if( nSampleStep >= nHeight || nSampleStep < 1 )
        nSampleStep = 1;

    bComplex = GDALDataTypeIsComplex(eType);
    if( bComplex )
    {
        pafData = (float *) VSIMalloc(nWidth * 2 * sizeof(float));
        eWrkType = GDT_CFloat32;
    }
    else
    {
        pafData = (float *) VSIMalloc(nWidth * sizeof(float));
        eWrkType = GDT_Float32;
    }

    if( pafData == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALComputeBandStats: Out of memory for buffer." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Loop over all sample lines.                                     */
/* -------------------------------------------------------------------- */
    for( iLine = 0; iLine < nHeight; iLine += nSampleStep )
    {
        int     iPixel;

        if( !pfnProgress( iLine / (double) nHeight,
                          NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            CPLFree( pafData );
            return CE_Failure;
        }

        CPLErr eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nWidth, 1,
                             pafData, nWidth, 1, eWrkType,
                             0, 0 );
        if ( eErr != CE_None )
        {
            CPLFree( pafData );
            return eErr;
        }

        for( iPixel = 0; iPixel < nWidth; iPixel++ )
        {
            float       fValue;

            if( bComplex )
            {
                // Compute the magnitude of the complex value.

                fValue = (float) 
                    sqrt(pafData[iPixel*2  ] * pafData[iPixel*2  ]
                         + pafData[iPixel*2+1] * pafData[iPixel*2+1]);
            }
            else
            {
                fValue = pafData[iPixel];
            }

            dfSum  += fValue;
            dfSum2 += fValue * fValue;
        }

        nSamples += nWidth;
    }

    if( !pfnProgress( 1.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        CPLFree( pafData );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Produce the result values.                                      */
/* -------------------------------------------------------------------- */
    if( pdfMean != NULL )
        *pdfMean = dfSum / nSamples;

    if( pdfStdDev != NULL )
    {
        double  dfMean = dfSum / nSamples;

        *pdfStdDev = sqrt((dfSum2 / nSamples) - (dfMean * dfMean));
    }

    CPLFree( pafData );

    return CE_None;
}

/************************************************************************/
/*                  GDALOverviewMagnitudeCorrection()                   */
/*                                                                      */
/*      Correct the mean and standard deviation of the overviews of     */
/*      the given band to match the base layer approximately.           */
/************************************************************************/

CPLErr
GDALOverviewMagnitudeCorrection( GDALRasterBandH hBaseBand, 
                                 int nOverviewCount,
                                 GDALRasterBandH *pahOverviews,
                                 GDALProgressFunc pfnProgress, 
                                 void *pProgressData )

{
    VALIDATE_POINTER1( hBaseBand, "GDALOverviewMagnitudeCorrection", CE_Failure );

    CPLErr      eErr;
    double      dfOrigMean, dfOrigStdDev;

/* -------------------------------------------------------------------- */
/*      Compute mean/stddev for source raster.                          */
/* -------------------------------------------------------------------- */
    eErr = GDALComputeBandStats( hBaseBand, 2, &dfOrigMean, &dfOrigStdDev, 
                                 pfnProgress, pProgressData );

    if( eErr != CE_None )
        return eErr;
    
/* -------------------------------------------------------------------- */
/*      Loop on overview bands.                                         */
/* -------------------------------------------------------------------- */
    int         iOverview;

    for( iOverview = 0; iOverview < nOverviewCount; iOverview++ )
    {
        GDALRasterBand *poOverview = (GDALRasterBand *)pahOverviews[iOverview];
        double  dfOverviewMean, dfOverviewStdDev;
        double  dfGain;

        eErr = GDALComputeBandStats( pahOverviews[iOverview], 1, 
                                     &dfOverviewMean, &dfOverviewStdDev, 
                                     pfnProgress, pProgressData );

        if( eErr != CE_None )
            return eErr;

        if( dfOrigStdDev < 0.0001 )
            dfGain = 1.0;
        else
            dfGain = dfOrigStdDev / dfOverviewStdDev;

/* -------------------------------------------------------------------- */
/*      Apply gain and offset.                                          */
/* -------------------------------------------------------------------- */
        GDALDataType    eWrkType, eType = poOverview->GetRasterDataType();
        int             iLine, nWidth, nHeight, bComplex;
        float           *pafData;

        nWidth = poOverview->GetXSize();
        nHeight = poOverview->GetYSize();

        bComplex = GDALDataTypeIsComplex(eType);
        if( bComplex )
        {
            pafData = (float *) VSIMalloc2(nWidth, 2 * sizeof(float));
            eWrkType = GDT_CFloat32;
        }
        else
        {
            pafData = (float *) VSIMalloc2(nWidth, sizeof(float));
            eWrkType = GDT_Float32;
        }

        if( pafData == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory,
                      "GDALOverviewMagnitudeCorrection: Out of memory for buffer." );
            return CE_Failure;
        }

        for( iLine = 0; iLine < nHeight; iLine++ )
        {
            int iPixel;
            
            if( !pfnProgress( iLine / (double) nHeight,
                              NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                CPLFree( pafData );
                return CE_Failure;
            }

            poOverview->RasterIO( GF_Read, 0, iLine, nWidth, 1,
                                  pafData, nWidth, 1, eWrkType,
                                  0, 0 );
            
            for( iPixel = 0; iPixel < nWidth; iPixel++ )
            {
                if( bComplex )
                {
                    pafData[iPixel*2] *= (float) dfGain;
                    pafData[iPixel*2+1] *= (float) dfGain;
                }
                else
                {
                    pafData[iPixel] = (float)
                        ((pafData[iPixel]-dfOverviewMean)*dfGain + dfOrigMean);

                }
            }

            poOverview->RasterIO( GF_Write, 0, iLine, nWidth, 1,
                                  pafData, nWidth, 1, eWrkType,
                                  0, 0 );
        }

        if( !pfnProgress( 1.0, NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            CPLFree( pafData );
            return CE_Failure;
        }
        
        CPLFree( pafData );
    }

    return CE_None;
}
