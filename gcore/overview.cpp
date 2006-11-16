/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Helper code to implement overview support in different drivers.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.15  2006/11/16 03:12:53  fwarmerdam
 * Default to dummy progress if pfnProgress NULL in ComputeBandStats.
 *
 * Revision 1.14  2006/03/28 14:49:56  fwarmerdam
 * updated contact info
 *
 * Revision 1.13  2006/02/07 19:11:36  fwarmerdam
 * applied some strategic improved outofmemory checking
 *
 * Revision 1.12  2005/04/04 15:24:48  fwarmerdam
 * Most C entry points now CPL_STDCALL
 *
 * Revision 1.11  2003/05/21 04:24:38  warmerda
 * avoid warnings with cast
 *
 * Revision 1.10  2002/07/09 20:33:12  warmerda
 * expand tabs
 *
 * Revision 1.9  2002/04/16 17:49:29  warmerda
 * Ensure dfRatio is assigned a value.
 *
 * Revision 1.8  2001/07/18 04:04:30  warmerda
 * added CPL_CVSID
 *
 * Revision 1.7  2001/07/16 15:21:46  warmerda
 * added AVERAGE_MAGPHASE option for complex images
 *
 * Revision 1.6  2001/01/30 22:32:42  warmerda
 * added AVERAGE_MP (magnitude preserving averaging) overview resampling type
 *
 * Revision 1.5  2000/11/22 18:41:45  warmerda
 * fixed bug in complex overview generation
 *
 * Revision 1.4  2000/08/18 15:25:06  warmerda
 * added cascading overview regeneration to speed up averaged overviews
 *
 * Revision 1.3  2000/07/17 17:08:45  warmerda
 * added support for complex data
 *
 * Revision 1.2  2000/06/26 22:17:58  warmerda
 * added scaled progress support
 *
 * Revision 1.1  2000/04/21 21:54:05  warmerda
 * New
 *
 */

#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                       GDALDownsampleChunk32R()                       */
/************************************************************************/

static CPLErr
GDALDownsampleChunk32R( int nSrcWidth, int nSrcHeight, 
                        float * pafChunk, int nChunkYOff, int nChunkYSize,
                        GDALRasterBand * poOverview,
                        const char * pszResampling )

{
    int      nDstYOff, nDstYOff2, nOXSize, nOYSize;
    float    *pafDstScanline;

    nOXSize = poOverview->GetXSize();
    nOYSize = poOverview->GetYSize();

    pafDstScanline = (float *) VSIMalloc(nOXSize * sizeof(float));
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
    
/* ==================================================================== */
/*      Loop over destination scanlines.                                */
/* ==================================================================== */
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; iDstLine++ )
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

        pafSrcScanline = pafChunk + ((nSrcYOff-nChunkYOff) * nSrcWidth);

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
                pafDstScanline[iDstPixel] = pafSrcScanline[nSrcXOff];
            }
            else if( EQUALN(pszResampling,"AVER",4) )
            {
                double dfTotal = 0.0;
                int    nCount = 0, iX, iY;

                for( iY = nSrcYOff; iY < nSrcYOff2; iY++ )
                 {
                    for( iX = nSrcXOff; iX < nSrcXOff2; iX++ )
                    {
                        dfTotal += pafSrcScanline[iX+(iY-nSrcYOff)*nSrcWidth];
                        nCount++;
                    }
                }
                
                CPLAssert( nCount > 0 );
                if( nCount == 0 )
                    pafDstScanline[iDstPixel] = 0.0;
                else
                    pafDstScanline[iDstPixel] = (float) (dfTotal / nCount);
            }
        }

        poOverview->RasterIO( GF_Write, 0, iDstLine, nOXSize, 1, 
                              pafDstScanline, nOXSize, 1, GDT_Float32, 
                              0, 0 );
    }

    CPLFree( pafDstScanline );

    return CE_None;
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
    for( int iDstLine = nDstYOff; iDstLine < nDstYOff2; iDstLine++ )
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

        poOverview->RasterIO( GF_Write, 0, iDstLine, nOXSize, 1, 
                              pafDstScanline, nOXSize, 1, GDT_CFloat32, 
                              0, 0 );
    }

    CPLFree( pafDstScanline );

    return CE_None;
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

        eErr = GDALRegenerateOverviews( poBaseBand, 1, papoOvrBands + i, 
                                        pszResampling, 
                                        GDALScaledProgress, 
                                        pScaledProgressData );
        GDALDestroyScaledProgress( pScaledProgressData );

        if( eErr != CE_None )
            return eErr;

        dfPixelsProcessed += dfPixels;
    }

    return CE_None;
}

/************************************************************************/
/*                      GDALRegenerateOverviews()                       */
/************************************************************************/
CPLErr 
GDALRegenerateOverviews( GDALRasterBand *poSrcBand,
                         int nOverviews, GDALRasterBand **papoOvrBands, 
                         const char * pszResampling, 
                         GDALProgressFunc pfnProgress, void * pProgressData )

{
    int    nFullResYChunk, nWidth;
    int    nFRXBlockSize, nFRYBlockSize;
    GDALDataType eType;

/* -------------------------------------------------------------------- */
/*      If we are operating on multiple overviews, and using            */
/*      averaging, lets do them in cascading order to reduce the        */
/*      amount of computation.                                          */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszResampling,"AVER",4) && nOverviews > 1 )
        return GDALRegenerateCascadingOverviews( poSrcBand, 
                                                 nOverviews, papoOvrBands,
                                                 pszResampling, 
                                                 pfnProgress,
                                                 pProgressData );

/* -------------------------------------------------------------------- */
/*      Setup one horizontal swath to read from the raw buffer.         */
/* -------------------------------------------------------------------- */
    float *pafChunk;

    poSrcBand->GetBlockSize( &nFRXBlockSize, &nFRYBlockSize );
    
    if( nFRYBlockSize < 4 || nFRYBlockSize > 256 )
        nFullResYChunk = 32;
    else
        nFullResYChunk = nFRYBlockSize;

    if( GDALDataTypeIsComplex( poSrcBand->GetRasterDataType() ) )
        eType = GDT_CFloat32;
    else
        eType = GDT_Float32;

    nWidth = poSrcBand->GetXSize();
    pafChunk = (float *) 
        VSIMalloc((GDALGetDataTypeSize(eType)/8) * nFullResYChunk * nWidth );

    if( pafChunk == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory, 
                  "Out of memory in GDALRegenerateOverviews()." );

        return CE_Failure;
    }
    
/* -------------------------------------------------------------------- */
/*      Loop over image operating on chunks.                            */
/* -------------------------------------------------------------------- */
    int  nChunkYOff = 0;

    for( nChunkYOff = 0; 
         nChunkYOff < poSrcBand->GetYSize(); 
         nChunkYOff += nFullResYChunk )
    {
        if( !pfnProgress( nChunkYOff / (double) poSrcBand->GetYSize(), 
                          NULL, pProgressData ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            return CE_Failure;
        }

        if( nFullResYChunk + nChunkYOff > poSrcBand->GetYSize() )
            nFullResYChunk = poSrcBand->GetYSize() - nChunkYOff;
        
        /* read chunk */
        poSrcBand->RasterIO( GF_Read, 0, nChunkYOff, nWidth, nFullResYChunk, 
                             pafChunk, nWidth, nFullResYChunk, eType,
                             0, 0 );
        
        for( int iOverview = 0; iOverview < nOverviews; iOverview++ )
        {
            if( eType == GDT_Float32 )
                GDALDownsampleChunk32R(nWidth, poSrcBand->GetYSize(), 
                                       pafChunk, nChunkYOff, nFullResYChunk,
                                       papoOvrBands[iOverview], pszResampling);
            else
                GDALDownsampleChunkC32R(nWidth, poSrcBand->GetYSize(), 
                                       pafChunk, nChunkYOff, nFullResYChunk,
                                       papoOvrBands[iOverview], pszResampling);
        }
    }

    VSIFree( pafChunk );
    
/* -------------------------------------------------------------------- */
/*      Renormalized overview mean / stddev if needed.                  */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszResampling,"AVERAGE_MP") )
    {
        GDALOverviewMagnitudeCorrection( (GDALRasterBandH) poSrcBand, 
                                         nOverviews, 
                                         (GDALRasterBandH *) papoOvrBands,
                                         GDALDummyProgress, NULL );
    }

/* -------------------------------------------------------------------- */
/*      It can be important to flush out data to overviews.             */
/* -------------------------------------------------------------------- */
    for( int iOverview = 0; iOverview < nOverviews; iOverview++ )
        papoOvrBands[iOverview]->FlushCache();

    pfnProgress( 1.0, NULL, pProgressData );

    return CE_None;
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

    if( nSampleStep >= nHeight )
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

        poSrcBand->RasterIO( GF_Read, 0, iLine, nWidth, 1,
                             pafData, nWidth, 1, eWrkType,
                             0, 0 );

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
            pafData = (float *) CPLMalloc(nWidth * 2 * sizeof(float));
            eWrkType = GDT_CFloat32;
        }
        else
        {
            pafData = (float *) CPLMalloc(nWidth * sizeof(float));
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
