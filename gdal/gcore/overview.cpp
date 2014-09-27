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
                              int nChunkYOff, int nChunkYSize,
                              GDALRasterBand * poOverview,
                              CPL_UNUSED const char * pszResampling_unused,
                              CPL_UNUSED int bHasNoData_unused,
                              CPL_UNUSED float fNoDataValue_unused,
                              CPL_UNUSED GDALColorTable* poColorTable_unused,
                              CPL_UNUSED GDALDataType eSrcDataType)

{
    CPLErr eErr = CE_None;

    int      nDstXOff, nDstXOff2, nDstYOff, nDstYOff2, nOXSize, nOYSize;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

/* -------------------------------------------------------------------- */
/*      Figure out the column to start writing to, and the first column */
/*      to not write to.                                                */
/* -------------------------------------------------------------------- */
    nDstXOff = (int) (0.5 + (nChunkXOff/(double)nSrcWidth) * nOXSize);
    nDstXOff2 = (int)
        (0.5 + ((nChunkXOff+nChunkXSize)/(double)nSrcWidth) * nOXSize);

    if( nChunkXOff + nChunkXSize == nSrcWidth )
        nDstXOff2 = nOXSize;

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

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
    nDstYOff = (int) (0.5 + (nChunkYOff/(double)nSrcHeight) * nOYSize);
    nDstYOff2 = (int)
        (0.5 + ((nChunkYOff+nChunkYSize)/(double)nSrcHeight) * nOYSize);

    if( nChunkYOff + nChunkYSize == nSrcHeight )
        nDstYOff2 = nOYSize;

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

    int      nDstXOff, nDstXOff2, nDstYOff, nDstYOff2, nOXSize, nOYSize;
    T    *pDstScanline;

    T      tNoDataValue = (T)fNoDataValue;
    if (!bHasNoData)
        tNoDataValue = 0;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

/* -------------------------------------------------------------------- */
/*      Figure out the column to start writing to, and the first column */
/*      to not write to.                                                */
/* -------------------------------------------------------------------- */
    nDstXOff = (int) (0.5 + (nChunkXOff/(double)nSrcWidth) * nOXSize);
    nDstXOff2 = (int)
        (0.5 + ((nChunkXOff+nChunkXSize)/(double)nSrcWidth) * nOXSize);

    if( nChunkXOff + nChunkXSize == nSrcWidth )
        nDstXOff2 = nOXSize;

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

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
    nDstYOff = (int) (0.5 + (nChunkYOff/(double)nSrcHeight) * nOYSize);
    nDstYOff2 = (int)
        (0.5 + ((nChunkYOff+nChunkYSize)/(double)nSrcHeight) * nOYSize);

    if( nChunkYOff + nChunkYSize == nSrcHeight )
        nDstYOff2 = nOYSize;


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
    int      nDstXOff, nDstXOff2, nDstYOff, nDstYOff2, nOXSize, nOYSize;
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

/* -------------------------------------------------------------------- */
/*      Figure out the column to start writing to, and the first column */
/*      to not write to.                                                */
/* -------------------------------------------------------------------- */
    nDstXOff = (int) (0.5 + (nChunkXOff/(double)nSrcWidth) * nOXSize);
    nDstXOff2 = (int)
        (0.5 + ((nChunkXOff+nChunkXSize)/(double)nSrcWidth) * nOXSize);

    if( nChunkXOff + nChunkXSize == nSrcWidth )
        nDstXOff2 = nOXSize;


    pafDstScanline = (float *) VSIMalloc((nDstXOff2 - nDstXOff) * sizeof(float));
    if( pafDstScanline == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunk32R: Out of memory for line buffer." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
    nDstYOff = (int) (0.5 + (nChunkYOff/(double)nSrcHeight) * nOYSize);
    nDstYOff2 = (int)
        (0.5 + ((nChunkYOff+nChunkYSize)/(double)nSrcHeight) * nOYSize);

    if( nChunkYOff + nChunkYSize == nSrcHeight )
        nDstYOff2 = nOYSize;


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
    int      nDstXOff, nDstXOff2, nDstYOff, nDstYOff2, nOXSize, nOYSize;
    float    *pafDstScanline;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

/* -------------------------------------------------------------------- */
/*      Figure out the column to start writing to, and the first column */
/*      to not write to.                                                */
/* -------------------------------------------------------------------- */
    nDstXOff = (int) (0.5 + (nChunkXOff/(double)nSrcWidth) * nOXSize);
    nDstXOff2 = (int)
        (0.5 + ((nChunkXOff+nChunkXSize)/(double)nSrcWidth) * nOXSize);

    if( nChunkXOff + nChunkXSize == nSrcWidth )
        nDstXOff2 = nOXSize;


    pafDstScanline = (float *) VSIMalloc((nDstXOff2 - nDstXOff) * sizeof(float));
    if( pafDstScanline == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunk32R: Out of memory for line buffer." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
    nDstYOff = (int) (0.5 + (nChunkYOff/(double)nSrcHeight) * nOYSize);
    nDstYOff2 = (int)
        (0.5 + ((nChunkYOff+nChunkYSize)/(double)nSrcHeight) * nOYSize);

    if( nChunkYOff + nChunkYSize == nSrcHeight )
        nDstYOff2 = nOYSize;


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

static CPLErr
GDALDownsampleChunk32R_Cubic( int nSrcWidth, int nSrcHeight,
                              CPL_UNUSED GDALDataType eWrkDataType,
                              void * pChunk,
                              CPL_UNUSED GByte * pabyChunkNodataMask,
                              int nChunkXOff, int nChunkXSize,
                              int nChunkYOff, int nChunkYSize,
                              GDALRasterBand * poOverview,
                              CPL_UNUSED const char * pszResampling,
                              CPL_UNUSED int bHasNoData,
                              CPL_UNUSED float fNoDataValue,
                              GDALColorTable* poColorTable,
                              CPL_UNUSED GDALDataType eSrcDataType)

{

    CPLErr eErr = CE_None;

    float * pafChunk = (float*) pChunk;

/* -------------------------------------------------------------------- */
/*      Create the filter kernel and allocate scanline buffer.          */
/* -------------------------------------------------------------------- */
    int      nDstXOff, nDstXOff2, nDstYOff, nDstYOff2, nOXSize, nOYSize;
    float    *pafDstScanline;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

/* -------------------------------------------------------------------- */
/*      Figure out the column to start writing to, and the first column */
/*      to not write to.                                                */
/* -------------------------------------------------------------------- */
    nDstXOff = (int) (0.5 + (nChunkXOff/(double)nSrcWidth) * nOXSize);
    nDstXOff2 = (int)
        (0.5 + ((nChunkXOff+nChunkXSize)/(double)nSrcWidth) * nOXSize);

    if( nChunkXOff + nChunkXSize == nSrcWidth )
        nDstXOff2 = nOXSize;


    pafDstScanline = (float *) VSIMalloc((nDstXOff2 - nDstXOff) * sizeof(float));
    if( pafDstScanline == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "GDALDownsampleChunk32R: Out of memory for line buffer." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
    nDstYOff = (int) (0.5 + (nChunkYOff/(double)nSrcHeight) * nOYSize);
    nDstYOff2 = (int)
        (0.5 + ((nChunkYOff+nChunkYSize)/(double)nSrcHeight) * nOYSize);

    if( nChunkYOff + nChunkYSize == nSrcHeight )
        nDstYOff2 = nOYSize;


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
        // GByte *pabySrcScanlineNodataMask;
        int   nSrcYOff, nSrcYOff2 = 0, iDstPixel;

        nSrcYOff = (int) floor(((iDstLine+0.5)/(double)nOYSize) * nSrcHeight - 0.5)-1;
        nSrcYOff2 = nSrcYOff + 4;
        if(nSrcYOff < 0)
            nSrcYOff = 0;
        if(nSrcYOff < nChunkYOff)
            nSrcYOff = nChunkYOff;

        if( nSrcYOff2 > nSrcHeight || iDstLine == nOYSize-1 )
            nSrcYOff2 = nSrcHeight;
        if( nSrcYOff2 > nChunkYOff + nChunkYSize)
            nSrcYOff2 = nChunkYOff + nChunkYSize;

        pafSrcScanline = pafChunk + ((nSrcYOff-nChunkYOff) * nChunkXSize);
#if 0
        // pabySrcScanlineNodataMask is unused.
        if (pabyChunkNodataMask != NULL)
            pabySrcScanlineNodataMask = pabyChunkNodataMask + ((nSrcYOff-nChunkYOff) * nChunkXSize);
        else
            pabySrcScanlineNodataMask = NULL;
#endif

/* -------------------------------------------------------------------- */
/*      Loop over destination pixels                                    */
/* -------------------------------------------------------------------- */
        for( iDstPixel = nDstXOff; iDstPixel < nDstXOff2; iDstPixel++ )
        {
            int   nSrcXOff, nSrcXOff2;

            nSrcXOff = (int) floor(((iDstPixel+0.5)/(double)nOXSize) * nSrcWidth - 0.5)-1;
            nSrcXOff2 = nSrcXOff + 4;

            if(nSrcXOff < 0)
                nSrcXOff = 0;

            if( nSrcXOff2 > nChunkRightXOff || iDstPixel == nOXSize-1 )
                nSrcXOff2 = nChunkRightXOff;

            // If we do not seem to have our full 4x4 window just
            // do nearest resampling.
            if( nSrcXOff2 - nSrcXOff != 4 || nSrcYOff2 - nSrcYOff != 4 )
            {
                int nLSrcYOff = (int) (0.5+(iDstLine/(double)nOYSize) * nSrcHeight);
                int nLSrcXOff = (int) (0.5+(iDstPixel/(double)nOXSize) * nSrcWidth);

                if( nLSrcYOff < nChunkYOff )
                    nLSrcYOff = nChunkYOff;
                if( nLSrcYOff > nChunkYOff + nChunkYSize - 1 )
                    nLSrcYOff = nChunkYOff + nChunkYSize - 1;

                pafDstScanline[iDstPixel - nDstXOff] =
                    pafChunk[(nLSrcYOff-nChunkYOff) * nChunkXSize
                                + (nLSrcXOff - nChunkXOff)];
            }
            else
            {
#define CubicConvolution(distance1,distance2,distance3,f0,f1,f2,f3) \
(  (   -f0 +     f1 - f2 + f3) * distance3                       \
+ (2.0*(f0 - f1) + f2 - f3) * distance2                         \
+ (   -f0          + f2     ) * distance1                       \
+               f1                         )

                int ic;
                double adfRowResults[4];
                double dfSrcX = (((iDstPixel+0.5)/(double)nOXSize) * nSrcWidth);
                double dfDeltaX = dfSrcX - 0.5 - (nSrcXOff+1);
                double dfDeltaX2 = dfDeltaX * dfDeltaX;
                double dfDeltaX3 = dfDeltaX2 * dfDeltaX;

                for ( ic = 0; ic < 4; ic++ )
                {
                    float *pafSrcRow = pafSrcScanline +
                        nSrcXOff-nChunkXOff+(nSrcYOff+ic-nSrcYOff)*nChunkXSize;

                    adfRowResults[ic] =
                        CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
                                            pafSrcRow[0],
                                            pafSrcRow[1],
                                            pafSrcRow[2],
                                            pafSrcRow[3] );
                }

                double dfSrcY = (((iDstLine+0.5)/(double)nOYSize) * nSrcHeight);
                double dfDeltaY = dfSrcY - 0.5 - (nSrcYOff+1);
                double dfDeltaY2 = dfDeltaY * dfDeltaY;
                double dfDeltaY3 = dfDeltaY2 * dfDeltaY;

                pafDstScanline[iDstPixel - nDstXOff] = (float)
                    CubicConvolution(dfDeltaY, dfDeltaY2, dfDeltaY3,
                                        adfRowResults[0],
                                        adfRowResults[1],
                                        adfRowResults[2],
                                        adfRowResults[3] );
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
/*                       GDALDownsampleChunkC32R()                      */
/************************************************************************/

static CPLErr
GDALDownsampleChunkC32R( int nSrcWidth, int nSrcHeight, 
                         float * pafChunk, int nChunkYOff, int nChunkYSize,
                         GDALRasterBand * poOverview,
                         const char * pszResampling )
    
{
    int      nDstYOff, nDstYOff2, nOXSize, nOYSize;
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

/* -------------------------------------------------------------------- */
/*      Figure out the line to start writing to, and the first line     */
/*      to not write to.  In theory this approach should ensure that    */
/*      every output line will be written if all input chunks are       */
/*      processed.                                                      */
/* -------------------------------------------------------------------- */
    nDstYOff = (int) (0.5 + (nChunkYOff/(double)nSrcHeight) * nOYSize);
    nDstYOff2 = (int) 
        (0.5 + ((nChunkYOff+nChunkYSize)/(double)nSrcHeight) * nOYSize);

    if( nChunkYOff + nChunkYSize == nSrcHeight )
        nDstYOff2 = nOYSize;
    
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
    int    nFullResYChunk, nWidth;
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

/* -------------------------------------------------------------------- */
/*      If we are operating on multiple overviews, and using            */
/*      averaging, lets do them in cascading order to reduce the        */
/*      amount of computation.                                          */
/* -------------------------------------------------------------------- */

    /* In case the mask made be computed from another band of the dataset, */
    /* we can't use cascaded generation, as the computation of the overviews */
    /* of the band used for the mask band may not have yet occured (#3033) */
    if( (EQUALN(pszResampling,"AVER",4) || EQUALN(pszResampling,"GAUSS",5)) && nOverviewCount > 1
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
    pChunk = 
        VSIMalloc3((GDALGetDataTypeSize(eType)/8), nFullResYChunk, nWidth );
    if (bUseNoDataMask)
    {
        pabyChunkNodataMask = (GByte *) 
            (GByte*) VSIMalloc2( nFullResYChunk, nWidth );
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
         nChunkYOff < poSrcBand->GetYSize() && eErr == CE_None; 
         nChunkYOff += nFullResYChunk )
    {
        if( !pfnProgress( nChunkYOff / (double) poSrcBand->GetYSize(), 
                          NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }

        if( nFullResYChunk + nChunkYOff > poSrcBand->GetYSize() )
            nFullResYChunk = poSrcBand->GetYSize() - nChunkYOff;
        
        /* read chunk */
        if (eErr == CE_None)
            eErr = poSrcBand->RasterIO( GF_Read, 0, nChunkYOff, nWidth, nFullResYChunk, 
                                pChunk, nWidth, nFullResYChunk, eType,
                                0, 0 );
        if (eErr == CE_None && bUseNoDataMask)
            eErr = poMaskBand->RasterIO( GF_Read, 0, nChunkYOff, nWidth, nFullResYChunk, 
                                pabyChunkNodataMask, nWidth, nFullResYChunk, GDT_Byte,
                                0, 0 );

        /* special case to promote 1bit data to 8bit 0/255 values */
        if( EQUAL(pszResampling,"AVERAGE_BIT2GRAYSCALE") )
        {
            int i;

            if (eType == GDT_Float32)
            {
                float* pafChunk = (float*)pChunk;
                for( i = nFullResYChunk*nWidth - 1; i >= 0; i-- )
                {
                    if( pafChunk[i] == 1.0 )
                        pafChunk[i] = 255.0;
                }
            }
            else if (eType == GDT_Byte)
            {
                GByte* pabyChunk = (GByte*)pChunk;
                for( i = nFullResYChunk*nWidth - 1; i >= 0; i-- )
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
                for( i = nFullResYChunk*nWidth - 1; i >= 0; i-- )
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
                for( i = nFullResYChunk*nWidth - 1; i >= 0; i-- )
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
            if( eType == GDT_Byte || eType == GDT_Float32 )
                eErr = pfnDownsampleFn(nWidth, poSrcBand->GetYSize(),
                                              eType,
                                              pChunk,
                                              pabyChunkNodataMask,
                                              0, nWidth,
                                              nChunkYOff, nFullResYChunk,
                                              papoOvrBands[iOverview], pszResampling,
                                              bHasNoData, fNoDataValue, poColorTable,
                                              poSrcBand->GetRasterDataType());
            else
                eErr = GDALDownsampleChunkC32R(nWidth, poSrcBand->GetYSize(), 
                                               (float*)pChunk, nChunkYOff, nFullResYChunk,
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
    if (!EQUALN(pszResampling, "NEAR", 4) && !EQUAL(pszResampling, "AVERAGE") && !EQUAL(pszResampling, "GAUSS"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALRegenerateOverviewsMultiBand: pszResampling='%s' not supported", pszResampling);
        return CE_Failure;
    }

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

        /* Compute the chunck size of the source such as it will match the size of */
        /* a block of the overview */
        int nFullResXChunk = (nDstBlockXSize * nSrcWidth) / nDstWidth;
        int nFullResYChunk = (nDstBlockYSize * nSrcHeight) / nDstHeight;

        void** papaChunk = (void**) CPLMalloc(nBands * sizeof(void*));
        GByte* pabyChunkNoDataMask = NULL;
        for(iBand=0;iBand<nBands;iBand++)
        {
            papaChunk[iBand] = VSIMalloc3(nFullResXChunk, nFullResYChunk, GDALGetDataTypeSize(eWrkDataType) / 8);
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
            pabyChunkNoDataMask = (GByte*) VSIMalloc2(nFullResXChunk, nFullResYChunk);
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

        int nChunkYOff;
        /* Iterate on destination overview, block by block */
        for( nChunkYOff = 0; nChunkYOff < nSrcHeight && eErr == CE_None; nChunkYOff += nFullResYChunk )
        {
            int nYCount;
            if  (nChunkYOff + nFullResYChunk <= nSrcHeight)
                nYCount = nFullResYChunk;
            else
                nYCount = nSrcHeight - nChunkYOff;

            if( !pfnProgress( dfCurPixelCount / dfTotalPixelCount, 
                              NULL, pProgressData ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                eErr = CE_Failure;
            }

            int nChunkXOff;
            for( nChunkXOff = 0; nChunkXOff < nSrcWidth && eErr == CE_None; nChunkXOff += nFullResXChunk )
            {
                int nXCount;
                if  (nChunkXOff + nFullResXChunk <= nSrcWidth)
                    nXCount = nFullResXChunk;
                else
                    nXCount = nSrcWidth - nChunkXOff;

                /* Read the source buffers for all the bands */
                for(iBand=0;iBand<nBands && eErr == CE_None;iBand++)
                {
                    GDALRasterBand* poSrcBand;
                    if (iSrcOverview == -1)
                        poSrcBand = papoSrcBands[iBand];
                    else
                        poSrcBand = papapoOverviewBands[iBand][iSrcOverview];
                    eErr = poSrcBand->RasterIO( GF_Read,
                                                nChunkXOff, nChunkYOff,
                                                nXCount, nYCount, 
                                                papaChunk[iBand],
                                                nXCount, nYCount,
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
                                                               nChunkXOff, nChunkYOff,
                                                               nXCount, nYCount, 
                                                               pabyChunkNoDataMask,
                                                               nXCount, nYCount,
                                                               GDT_Byte, 0, 0 );
                }

                /* Compute the resulting overview block */
                for(iBand=0;iBand<nBands && eErr == CE_None;iBand++)
                {
                    eErr = pfnDownsampleFn(nSrcWidth, nSrcHeight,
                                                  eWrkDataType,
                                                  papaChunk[iBand],
                                                  pabyChunkNoDataMask,
                                                  nChunkXOff, nXCount,
                                                  nChunkYOff, nYCount,
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
