/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Free standing functions for GDAL.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Revision 1.31  2002/04/19 12:22:05  dron
 * added GDALWriteWorldFile()
 *
 * Revision 1.30  2002/04/16 13:59:33  warmerda
 * added GDALVersionInfo
 *
 * Revision 1.29  2001/12/07 20:04:21  warmerda
 * fixed serious bug in random sampler
 *
 * Revision 1.28  2001/11/30 03:41:26  warmerda
 * Fixed bug with the block sampling rate being too low to satisfy large
 * sample count values.  Fixed bug with tiled images including some uninitialized
 * or zero data in the sample set on partial edge tiles.
 *
 * Revision 1.27  2001/11/26 20:14:01  warmerda
 * added GDALProjDef stubs for old 'bridges'
 *
 * Revision 1.26  2001/11/19 16:03:16  warmerda
 * moved GDALDectoDMS here
 *
 * Revision 1.25  2001/08/15 15:05:44  warmerda
 * return magnitude for complex samples in random sampler
 *
 * Revision 1.24  2001/07/18 04:04:30  warmerda
 * added CPL_CVSID
 *
 * Revision 1.23  2001/05/01 18:09:25  warmerda
 * added GDALReadWorldFile()
 *
 * Revision 1.22  2000/12/04 20:45:14  warmerda
 * removed unused variable.
 *
 * Revision 1.21  2000/10/06 15:22:49  warmerda
 * added GDALDataTypeUnion
 *
 * Revision 1.20  2000/08/18 15:24:48  warmerda
 * added GDALTermProgress
 *
 * Revision 1.19  2000/08/09 16:25:42  warmerda
 * don't crash if block is null
 *
 * Revision 1.18  2000/07/11 14:35:43  warmerda
 * added documentation
 *
 * Revision 1.17  2000/07/05 17:53:33  warmerda
 * Removed unused code related to nXCheck.
 *
 * Revision 1.16  2000/06/27 17:21:26  warmerda
 * added GDALGetRasterSampleOverview
 *
 * Revision 1.15  2000/06/26 22:17:49  warmerda
 * added scaled progress support
 *
 * Revision 1.14  2000/06/05 17:24:05  warmerda
 * added real complex support
 *
 * Revision 1.13  2000/04/21 21:55:32  warmerda
 * made more robust if block read fails
 *
 * Revision 1.12  2000/04/17 20:59:40  warmerda
 * Removed printf.
 *
 * Revision 1.11  2000/04/17 20:59:14  warmerda
 * fixed sampling bug
 *
 * Revision 1.10  2000/03/31 13:41:45  warmerda
 * added gcp support functions
 *
 * Revision 1.9  2000/03/24 00:09:19  warmerda
 * added sort-of random sampling
 *
 * Revision 1.8  2000/03/23 16:53:21  warmerda
 * use overviews for approximate min/max
 *
 * Revision 1.7  2000/03/09 23:21:44  warmerda
 * added GDALDummyProgress
 *
 * Revision 1.6  2000/03/06 21:59:44  warmerda
 * added min/max calculate
 *
 * Revision 1.5  2000/03/06 02:20:15  warmerda
 * added getname functions for colour interpretations
 *
 * Revision 1.4  1999/07/23 19:35:47  warmerda
 * added GDALGetDataTypeName
 *
 * Revision 1.3  1999/05/17 02:00:45  vgough
 * made pure_virtual C linkage
 *
 * Revision 1.2  1999/05/16 19:32:13  warmerda
 * Added __pure_virtual.
 *
 * Revision 1.1  1998/12/06 02:50:16  warmerda
 * New
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           __pure_virtual()                           */
/*                                                                      */
/*      The following is a gross hack to remove the last remaining      */
/*      dependency on the GNU C++ standard library.                     */
/************************************************************************/

#ifdef __GNUC__

extern "C"
void __pure_virtual()

{
}

#endif

/************************************************************************/
/*                         GDALDataTypeUnion()                          */
/************************************************************************/

/**
 * Return the smallest data type that can fully express both input data
 * types.
 *
 * @param eType1 
 * @param eType2
 *
 * @return a data type able to express eType1 and eType2.
 */

GDALDataType GDALDataTypeUnion( GDALDataType eType1, GDALDataType eType2 )

{
    int		bFloating, bComplex, nBits, bSigned;

    bComplex = GDALDataTypeIsComplex(eType1) | GDALDataTypeIsComplex(eType2);
    
    switch( eType1 )
    {
      case GDT_Byte:
        nBits = 8;
        bSigned = FALSE;
        bFloating = FALSE;
        break;
        
      case GDT_Int16:
      case GDT_CInt16:
        nBits = 16;
        bSigned = TRUE;
        bFloating = FALSE;
        break;
        
      case GDT_UInt16:
        nBits = 16;
        bSigned = FALSE;
        bFloating = FALSE;
        break;
        
      case GDT_Int32:
      case GDT_CInt32:
        nBits = 32;
        bSigned = TRUE;
        bFloating = FALSE;
        break;
        
      case GDT_UInt32:
        nBits = 32;
        bSigned = FALSE;
        bFloating = FALSE;
        break;

      case GDT_Float32:
      case GDT_CFloat32:
        nBits = 32;
        bSigned = TRUE;
        bFloating = TRUE;
        break;

      case GDT_Float64:
      case GDT_CFloat64:
        nBits = 64;
        bSigned = TRUE;
        bFloating = TRUE;
        break;

      default:
        CPLAssert( FALSE );
        return GDT_Unknown;
    }

    switch( eType2 )
    {
      case GDT_Byte:
        break;
        
      case GDT_Int16:
        nBits = MAX(nBits,16);
        bSigned = TRUE;
        break;
        
      case GDT_UInt16:
        nBits = MAX(nBits,16);
        break;
        
      case GDT_Int32:
      case GDT_CInt32:
        nBits = MAX(nBits,32);
        bSigned = TRUE;
        break;
        
      case GDT_UInt32:
        nBits = MAX(nBits,32);
        break;

      case GDT_Float32:
      case GDT_CFloat32:
        nBits = MAX(nBits,32);
        bSigned = TRUE;
        bFloating = TRUE;
        break;

      case GDT_Float64:
      case GDT_CFloat64:
        nBits = MAX(nBits,64);
        bSigned = TRUE;
        bFloating = TRUE;
        break;

      default:
        CPLAssert( FALSE );
        return GDT_Unknown;
    }

    if( nBits == 8 )
        return GDT_Byte;
    else if( nBits == 16 && bComplex )
        return GDT_CInt16;
    else if( nBits == 16 && bSigned )
        return GDT_Int16;
    else if( nBits == 16 && !bSigned )
        return GDT_UInt16;
    else if( nBits == 32 && bFloating && bComplex )
        return GDT_CFloat32;
    else if( nBits == 32 && bFloating )
        return GDT_Float32;
    else if( nBits == 32 && bComplex )
        return GDT_CInt32;
    else if( nBits == 32 && bSigned )
        return GDT_Int32;
    else if( nBits == 32 && !bSigned )
        return GDT_UInt32;
    else if( nBits == 64 && bComplex )
        return GDT_CFloat64;
    else
        return GDT_Float64;
}


/************************************************************************/
/*                        GDALGetDataTypeSize()                         */
/************************************************************************/
int GDALGetDataTypeSize( GDALDataType eDataType )

{
    switch( eDataType )
    {
      case GDT_Byte:
        return 8;

      case GDT_UInt16:
      case GDT_Int16:
        return 16;

      case GDT_UInt32:
      case GDT_Int32:
      case GDT_Float32:
      case GDT_CInt16:
        return 32;

      case GDT_Float64:
      case GDT_CInt32:
      case GDT_CFloat32:
        return 64;

      case GDT_CFloat64:
        return 128;

      default:
        CPLAssert( FALSE );
        return 0;
    }
}

/************************************************************************/
/*                       GDALDataTypeIsComplex()                        */
/************************************************************************/

int GDALDataTypeIsComplex( GDALDataType eDataType )

{
    switch( eDataType )
    {
      case GDT_CInt16:
      case GDT_CInt32:
      case GDT_CFloat32:
      case GDT_CFloat64:
        return TRUE;

      default:
        return FALSE;
    }
}

/************************************************************************/
/*                        GDALGetDataTypeName()                         */
/************************************************************************/

const char *GDALGetDataTypeName( GDALDataType eDataType )

{
    switch( eDataType )
    {
      case GDT_Byte:
        return "Byte";

      case GDT_UInt16:
        return "UInt16";

      case GDT_Int16:
        return "Int16";

      case GDT_UInt32:
        return "UInt32";
        
      case GDT_Int32:
        return "Int32";
        
      case GDT_Float32:
        return "Float32";

      case GDT_Float64:
        return "Float64";

      case GDT_CInt16:
        return "CInt16";

      case GDT_CInt32:
        return "CInt32";

      case GDT_CFloat32:
        return "CFloat32";

      case GDT_CFloat64:
        return "CFloat64";

      default:
        return NULL;
    }
}

/************************************************************************/
/*                  GDALGetPaletteInterpretationName()                  */
/************************************************************************/

const char *GDALGetPaletteInterpretationName( GDALPaletteInterp eInterp )

{
    switch( eInterp )
    {
      case GPI_Gray:
        return "Gray";

      case GPI_RGB:
        return "RGB";
        
      case GPI_CMYK:
        return "CMYK";

      case GPI_HLS:
        return "HLS";
        
      default:
        return "Unknown";
    }
}

/************************************************************************/
/*                   GDALGetColorInterpretationName()                   */
/************************************************************************/

const char *GDALGetColorInterpretationName( GDALColorInterp eInterp )

{
    switch( eInterp )
    {
      case GCI_Undefined:
        return "Undefined";

      case GCI_GrayIndex:
        return "Gray";

      case GCI_PaletteIndex:
        return "Palette";

      case GCI_RedBand:
        return "Red";

      case GCI_GreenBand:
        return "Green";

      case GCI_BlueBand:
        return "Blue";

      case GCI_AlphaBand:
        return "Alpha";

      case GCI_HueBand:
        return "Hue";

      case GCI_SaturationBand:
        return "Saturation";

      case GCI_LightnessBand:
        return "Lightness";

      case GCI_CyanBand:
        return "Cyan";

      case GCI_MagentaBand:
        return "Magenta";

      case GCI_YellowBand:
        return "Yellow";

      case GCI_BlackBand:
        return "Black";
        
      default:
        return "Unknown";
    }
}

/************************************************************************/
/*                      GDALComputeRasterMinMax()                       */
/************************************************************************/

/**
 * Compute the min/max values for a band.
 * 
 * If approximate is OK, then the band's GetMinimum()/GetMaximum() will
 * be trusted.  If it doesn't work, a subsample of blocks will be read to
 * get an approximate min/max.  If the band has a nodata value it will
 * be excluded from the minimum and maximum.
 *
 * If bApprox is FALSE, then all pixels will be read and used to compute
 * an exact range.
 * 
 * @param hBand the band to copmute the range for.
 * @param bApproxOK TRUE if an approximate (faster) answer is OK, otherwise
 * FALSE.
 * @param adfMinMax the array in which the minimum (adfMinMax[0]) and the
 * maximum (adfMinMax[1]) are returned.
 */

void GDALComputeRasterMinMax( GDALRasterBandH hBand, int bApproxOK, 
                              double adfMinMax[2] )

{
    double       dfMin=0.0, dfMax=0.0;
    GDALRasterBand *poBand;

/* -------------------------------------------------------------------- */
/*      Does the driver already know the min/max?                       */
/* -------------------------------------------------------------------- */
    if( bApproxOK )
    {
        int          bSuccessMin, bSuccessMax;

        dfMin = GDALGetRasterMinimum( hBand, &bSuccessMin );
        dfMax = GDALGetRasterMaximum( hBand, &bSuccessMax );

        if( bSuccessMin && bSuccessMax )
        {
            adfMinMax[0] = dfMin;
            adfMinMax[1] = dfMax;
            return;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      If we have overview bands, use them for min/max.                */
/* -------------------------------------------------------------------- */
    if( bApproxOK )
        poBand = (GDALRasterBand *) GDALGetRasterSampleOverview( hBand, 2500 );
    else 
        poBand = (GDALRasterBand *) hBand;
    
/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
    int         nBlockXSize, nBlockYSize;
    int         nBlocksPerRow, nBlocksPerColumn;
    int         nSampleRate;
    int         bGotNoDataValue, bFirstValue = TRUE;
    double      dfNoDataValue;

    dfNoDataValue = poBand->GetNoDataValue( &bGotNoDataValue );

    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    nBlocksPerRow = (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (poBand->GetYSize() + nBlockYSize - 1) / nBlockYSize;

    if( bApproxOK )
        nSampleRate = 
            (int) MAX(1,sqrt((double) nBlocksPerRow * nBlocksPerColumn));
    else
        nSampleRate = 1;
    
    for( int iSampleBlock = 0; 
         iSampleBlock < nBlocksPerRow * nBlocksPerColumn;
         iSampleBlock += nSampleRate )
    {
        double dfValue = 0.0;
        int  iXBlock, iYBlock, nXCheck, nYCheck;
        GDALRasterBlock *poBlock;

        iYBlock = iSampleBlock / nBlocksPerRow;
        iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;
        
        poBlock = poBand->GetBlockRef( iXBlock, iYBlock );
        if( poBlock == NULL )
            continue;
        
        if( (iXBlock+1) * nBlockXSize > poBand->GetXSize() )
            nXCheck = poBand->GetXSize() - iXBlock * nBlockXSize;
        else
            nXCheck = nBlockXSize;

        if( (iYBlock+1) * nBlockYSize > poBand->GetYSize() )
            nYCheck = poBand->GetYSize() - iYBlock * nBlockYSize;
        else
            nYCheck = nBlockYSize;

        /* this isn't the fastest way to do this, but is easier for now */
        for( int iY = 0; iY < nYCheck; iY++ )
        {
            for( int iX = 0; iX < nXCheck; iX++ )
            {
                int    iOffset = iX + iY * nBlockXSize;

                switch( poBlock->GetDataType() )
                {
                  case GDT_Byte:
                    dfValue = ((GByte *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_UInt16:
                    dfValue = ((GUInt16 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = ((GInt16 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = ((GUInt32 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = ((GInt32 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Float32:
                    dfValue = ((float *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Float64:
                    dfValue = ((double *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_CInt16:
                    dfValue = ((GInt16 *) poBlock->GetDataRef())[iOffset*2];
                    break;
                  case GDT_CInt32:
                    dfValue = ((GInt32 *) poBlock->GetDataRef())[iOffset*2];
                    break;
                  case GDT_CFloat32:
                    dfValue = ((float *) poBlock->GetDataRef())[iOffset*2];
                    break;
                  case GDT_CFloat64:
                    dfValue = ((double *) poBlock->GetDataRef())[iOffset*2];
                    break;
                  default:
                    CPLAssert( FALSE );
                }
                
                if( bGotNoDataValue && dfValue == dfNoDataValue )
                    continue;

                if( bFirstValue )
                {
                    dfMin = dfMax = dfValue;
                    bFirstValue = FALSE;
                }
                else
                {
                    dfMin = MIN(dfMin,dfValue);
                    dfMax = MAX(dfMax,dfValue);
                }
            }
        }
    }

    adfMinMax[0] = dfMin;
    adfMinMax[1] = dfMax;
}

/************************************************************************/
/*                         GDALDummyProgress()                          */
/************************************************************************/

/**
 * Stub progress function.
 *
 * Many long running operations within GDAL the option of passing a
 * progress function.  The progress function is intended to provide a 
 * way of displaying a progress indicator to the user, and for the user
 * to terminate the process prematurely.  Applications not desiring 
 * to utilize this support should normally pass GDALDummyProgress as
 * the pfnProgress argument and NULL as the pData argument.  
 * 
 * Applications wishing to take advantage of the progress semantics should
 * pass a function implementing GDALProgressFunc semantics. 
 *
 * <pre>
 * typedef int (*GDALProgressFunc)(double dfComplete,
 *                                 const char *pszMessage, 
 *                                 void *pData);
 * </pre>
 *
 * @param dfComplete Passed in the with ratio of the operation that is
 * complete, and is a value between 0.0 and 1.0.  
 * 
 * @param pszMessage This is normally passed in as NULL, but will occasionally
 * be passed in with a message about what is happening that may be displayed
 * to the user. 
 *
 * @param pData Application data (as passed via pData into GDAL operation).
 *
 * @return TRUE if the operation should continue, or FALSE if the user
 * has requested a cancel. 
 * 
 * For example, an application might implement the following simple
 * text progress reporting mechanism, using pData to pass a default message:
 *
 * <pre>
 * int MyTextProgress( double dfComplete, const char *pszMessage, void *pData)
 * {
 *     if( pszMessage != NULL )
 *         printf( "%d%% complete: %s\n", (int) (dfComplete*100), pszMessage );
 *     else if( pData != NULL )
 *         printf( "%d%% complete:%s\n", (int) (dfComplete*100),
 *                 (char *) pData );
 *     else
 *         printf( "%d%% complete.\n", (int) (dfComplete*100) );
 *     
 *     return TRUE;
 * }
 * </pre>
 *
 * This could be utilized with the GDALDataset::BuildOverviews() method like
 * this:
 *
 * <pre>
 *      int       anOverviewList[3] = {2, 4, 8};
 *
 *      poDataset->BuildOverviews( "NEAREST", 3, anOverviewList, 0, NULL, 
 *                                 MyTextProgress, "building overviews" );
 * </pre>
 * 
 * More often that implementing custom progress functions, applications 
 * will just use existing progress functions like GDALDummyProgress(), and 
 * GDALScaledProgress().  Python scripts also can pass progress functions.
 */

int GDALDummyProgress( double, const char *, void * )

{
    return TRUE;
}

/************************************************************************/
/*                         GDALScaledProgress()                         */
/************************************************************************/
typedef struct { 
    GDALProgressFunc pfnProgress;
    void *pData;
    double dfMin;
    double dfMax;
} GDALScaledProgressInfo;

int GDALScaledProgress( double dfComplete, const char *pszMessage, 
                        void *pData )

{
    GDALScaledProgressInfo *psInfo = (GDALScaledProgressInfo *) pData;

    return psInfo->pfnProgress( dfComplete * (psInfo->dfMax - psInfo->dfMin)
                                + psInfo->dfMin,
                                pszMessage, psInfo->pData );
}

/************************************************************************/
/*                      GDALCreateScaledProgress()                      */
/************************************************************************/

void *GDALCreateScaledProgress( double dfMin, double dfMax, 
                                GDALProgressFunc pfnProgress, 
                                void * pData )

{
    GDALScaledProgressInfo *psInfo;

    psInfo = (GDALScaledProgressInfo *) 
        CPLCalloc(sizeof(GDALScaledProgressInfo),1);

    if( ABS(dfMin-dfMax) < 0.0000001 )
        dfMax = dfMin + 0.01;

    psInfo->pData = pData;
    psInfo->pfnProgress = pfnProgress;
    psInfo->dfMin = dfMin;
    psInfo->dfMax = dfMax;

    return (void *) psInfo;
}

/************************************************************************/
/*                     GDALDestroyScaledProgress()                      */
/************************************************************************/

void GDALDestroyScaledProgress( void * pData )

{
    CPLFree( pData );
}

/************************************************************************/
/*                          GDALTermProgress()                          */
/************************************************************************/

int GDALTermProgress( double dfComplete, const char *pszMessage, void * )

{
    static double dfLastComplete = -1.0;

    if( dfLastComplete > dfComplete )
    {
        if( dfLastComplete > 1.0 )
            dfLastComplete = -1.0;
        else
            dfLastComplete = dfComplete;
    }

    if( floor(dfLastComplete*10) != floor(dfComplete*10) )
    {
        int    nPercent = (int) floor(dfComplete*100);

        if( nPercent == 0 && pszMessage != NULL )
            fprintf( stdout, "%s:", pszMessage );

        if( nPercent == 100 )
            fprintf( stdout, "%d - done.\n", (int) floor(dfComplete*100) );
        else
        {
            fprintf( stdout, "%d.", (int) floor(dfComplete*100) );
            fflush( stdout );
        }
    }
    else if( floor(dfLastComplete*30) != floor(dfComplete*30) )
    {
        fprintf( stdout, "." );
        fflush( stdout );
    }

    dfLastComplete = dfComplete;

    return TRUE;
}

/************************************************************************/
/*                    GDALGetRasterSampleOverview()                     */
/************************************************************************/

/**
 * Fetch best sampling overview.
 *
 * Returns the most reduced overview of the given band that still satisfies
 * the desired number of samples.  This function can be used with zero
 * as the number of desired samples to fetch the most reduced overview. 
 * The same band as was passed in will be returned if it has not overviews,
 * or if none of the overviews have enough samples. 
 *
 * @param hBand the band to search for overviews on.
 * @param nDesiredSamples the returned band will have at least this many 
 * pixels.
 * @return optimal overview or hBand itself. 
 */

GDALRasterBandH GDALGetRasterSampleOverview( GDALRasterBandH hBand, 
                                             int nDesiredSamples )

{
    int     nBestSamples; 
    GDALRasterBandH hBestBand = hBand;

    nBestSamples = GDALGetRasterBandXSize(hBand) 
        * GDALGetRasterBandYSize(hBand);

    for( int iOverview = 0; 
         iOverview < GDALGetOverviewCount( hBand );
         iOverview++ )
    {
        GDALRasterBandH hOBand = GDALGetOverview( hBand, iOverview );
        int    nOSamples;

        nOSamples = GDALGetRasterBandXSize(hOBand) 
            * GDALGetRasterBandYSize(hOBand);

        if( nOSamples < nBestSamples && nOSamples > nDesiredSamples )
        {
            nBestSamples = nOSamples;
            hBestBand = hOBand;
        }
    }

    return hBestBand;
}

/************************************************************************/
/*                     GDALGetRandomRasterSample()                      */
/************************************************************************/

int GDALGetRandomRasterSample( GDALRasterBandH hBand, int nSamples, 
                               float *pafSampleBuf )

{
    GDALRasterBand *poBand;

    poBand = (GDALRasterBand *) GDALGetRasterSampleOverview( hBand, nSamples );

/* -------------------------------------------------------------------- */
/*      Figure out the ratio of blocks we will read to get an           */
/*      approximate value.                                              */
/* -------------------------------------------------------------------- */
    int         nBlockXSize, nBlockYSize;
    int         nBlocksPerRow, nBlocksPerColumn;
    int         nSampleRate;
    int         bGotNoDataValue;
    double      dfNoDataValue;
    int         nActualSamples = 0;
    int         nBlockSampleRate;
    int		nBlockPixels, nBlockCount;

    dfNoDataValue = poBand->GetNoDataValue( &bGotNoDataValue );

    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

    nBlocksPerRow = (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (poBand->GetYSize() + nBlockYSize - 1) / nBlockYSize;

    nBlockPixels = nBlockXSize * nBlockYSize;
    nBlockCount = nBlocksPerRow * nBlocksPerColumn;

    nSampleRate = (int) MAX(1,sqrt((double) nBlockCount)-2.0);

    if( nSampleRate == nBlocksPerRow && nSampleRate > 1 )
        nSampleRate--;

    while( nSampleRate > 1 
           && ((nBlockCount-1) / nSampleRate + 1) * nBlockPixels < nSamples )
        nSampleRate--;

    nBlockSampleRate = 
        MAX(1,nBlockPixels / (nSamples / ((nBlockCount-1) / nSampleRate + 1)));
    
    for( int iSampleBlock = 0; 
         iSampleBlock < nBlockCount;
         iSampleBlock += nSampleRate )
    {
        double dfValue = 0.0, dfReal, dfImag;
        int  iXBlock, iYBlock, iX, iY, iXValid, iYValid, iRemainder = 0;
        GDALRasterBlock *poBlock;

        iYBlock = iSampleBlock / nBlocksPerRow;
        iXBlock = iSampleBlock - nBlocksPerRow * iYBlock;

        poBlock = poBand->GetBlockRef( iXBlock, iYBlock );
        if( poBlock == NULL )
            continue;

        if( iXBlock * nBlockXSize > poBand->GetXSize() )
            iXValid = poBand->GetXSize() - iXBlock * nBlockXSize;
        else
            iXValid = nBlockXSize;

        if( iYBlock * nBlockYSize > poBand->GetYSize() )
            iYValid = poBand->GetYSize() - iYBlock * nBlockYSize;
        else
            iYValid = nBlockYSize;

        for( iY = 0; iY < iYValid; iY++ )
        {
            for( iX = iRemainder; iX < iXValid; iX += nBlockSampleRate )
            {
                int	iOffset;

                iOffset = iX + iY * nBlockXSize; 
                switch( poBlock->GetDataType() )
                {
                  case GDT_Byte:
                    dfValue = ((GByte *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_UInt16:
                    dfValue = ((GUInt16 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Int16:
                    dfValue = ((GInt16 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_UInt32:
                    dfValue = ((GUInt32 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Int32:
                    dfValue = ((GInt32 *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Float32:
                    dfValue = ((float *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_Float64:
                    dfValue = ((double *) poBlock->GetDataRef())[iOffset];
                    break;
                  case GDT_CInt16:
                    dfReal = ((GInt16 *) poBlock->GetDataRef())[iOffset*2];
                    dfImag = ((GInt16 *) poBlock->GetDataRef())[iOffset*2+1];
                    dfValue = sqrt(dfReal*dfReal + dfImag*dfImag);
                    break;
                  case GDT_CInt32:
                    dfReal = ((GInt32 *) poBlock->GetDataRef())[iOffset*2];
                    dfImag = ((GInt32 *) poBlock->GetDataRef())[iOffset*2+1];
                    dfValue = sqrt(dfReal*dfReal + dfImag*dfImag);
                    break;
                  case GDT_CFloat32:
                    dfReal = ((float *) poBlock->GetDataRef())[iOffset*2];
                    dfImag = ((float *) poBlock->GetDataRef())[iOffset*2+1];
                    dfValue = sqrt(dfReal*dfReal + dfImag*dfImag);
                    break;
                  case GDT_CFloat64:
                    dfReal = ((double *) poBlock->GetDataRef())[iOffset*2];
                    dfImag = ((double *) poBlock->GetDataRef())[iOffset*2+1];
                    dfValue = sqrt(dfReal*dfReal + dfImag*dfImag);
                    break;
                  default:
                    CPLAssert( FALSE );
                }
            
                if( bGotNoDataValue && dfValue == dfNoDataValue )
                    continue;

                if( nActualSamples < nSamples )
                    pafSampleBuf[nActualSamples++] = dfValue;
            }

            iRemainder = iX - iXValid;
        }
    }

    return nActualSamples;
}

/************************************************************************/
/*                            GDALInitGCPs()                            */
/************************************************************************/

void GDALInitGCPs( int nCount, GDAL_GCP * psGCP )

{
    for( int iGCP = 0; iGCP < nCount; iGCP++ )
    {
        memset( psGCP, 0, sizeof(GDAL_GCP) );
        psGCP->pszId = CPLStrdup("");
        psGCP->pszInfo = CPLStrdup("");
        psGCP++;
    }
}

/************************************************************************/
/*                           GDALDeinitGCPs()                           */
/************************************************************************/

void GDALDeinitGCPs( int nCount, GDAL_GCP * psGCP )

{
    for( int iGCP = 0; iGCP < nCount; iGCP++ )
    {
        CPLFree( psGCP->pszId );
        CPLFree( psGCP->pszInfo );
        psGCP++;
    }
}

/************************************************************************/
/*                         GDALDuplicateGCPs()                          */
/************************************************************************/

GDAL_GCP *GDALDuplicateGCPs( int nCount, const GDAL_GCP *pasGCPList )

{
    GDAL_GCP	*pasReturn;

    pasReturn = (GDAL_GCP *) CPLMalloc(sizeof(GDAL_GCP) * nCount);
    GDALInitGCPs( nCount, pasReturn );

    for( int iGCP = 0; iGCP < nCount; iGCP++ )
    {
        CPLFree( pasReturn[iGCP].pszId );
        pasReturn[iGCP].pszId = CPLStrdup( pasGCPList[iGCP].pszId );

        CPLFree( pasReturn[iGCP].pszInfo );
        pasReturn[iGCP].pszInfo = CPLStrdup( pasGCPList[iGCP].pszInfo );

        pasReturn[iGCP].dfGCPPixel = pasGCPList[iGCP].dfGCPPixel;
        pasReturn[iGCP].dfGCPLine = pasGCPList[iGCP].dfGCPLine;
        pasReturn[iGCP].dfGCPX = pasGCPList[iGCP].dfGCPX;
        pasReturn[iGCP].dfGCPY = pasGCPList[iGCP].dfGCPY;
        pasReturn[iGCP].dfGCPZ = pasGCPList[iGCP].dfGCPZ;
    }

    return pasReturn;
}
                             
/************************************************************************/
/*                         GDALReadWorldFile()                          */
/*                                                                      */
/*      Helper function for translator implementators wanting           */
/*      support for ESRI world files.                                   */
/************************************************************************/

int GDALReadWorldFile( const char * pszBaseFilename, const char *pszExtension,
                       double *padfGeoTransform )

{
    const char	*pszTFW;
    char	szExtUpper[32], szExtLower[32];
    int		i;
    FILE	*fpTFW;
    char	**papszLines;

    if( *pszExtension == '.' )
        pszExtension++;

/* -------------------------------------------------------------------- */
/*      Generate upper and lower case versions of the extension.        */
/* -------------------------------------------------------------------- */
    strncpy( szExtUpper, pszExtension, 32 );
    strncpy( szExtLower, pszExtension, 32 );

    for( i = 0; szExtUpper[i] != '\0' && i < 32; i++ )
    {
	szExtUpper[i] = toupper(szExtUpper[i]);
	szExtLower[i] = tolower(szExtLower[i]);
    }

/* -------------------------------------------------------------------- */
/*      Try lower case, then upper case.                                */
/* -------------------------------------------------------------------- */
    pszTFW = CPLResetExtension( pszBaseFilename, szExtLower );

    fpTFW = VSIFOpen( pszTFW, "rt" );

#ifndef WIN32
    if( fpTFW == NULL )
    {
        pszTFW = CPLResetExtension( pszBaseFilename, szExtUpper );
        fpTFW = VSIFOpen( pszTFW, "rt" );
    }
#endif
    
    if( fpTFW == NULL )
        return FALSE;

    VSIFClose( fpTFW );

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    papszLines = CSLLoad( pszTFW );
    if( CSLCount(papszLines) >= 6 
        && atof(papszLines[0]) != 0.0
        && atof(papszLines[3]) != 0.0 )
    {
        padfGeoTransform[0] = atof(papszLines[4]);
        padfGeoTransform[1] = atof(papszLines[0]);
        padfGeoTransform[2] = atof(papszLines[2]);
        padfGeoTransform[3] = atof(papszLines[5]);
        padfGeoTransform[4] = atof(papszLines[1]);
        padfGeoTransform[5] = atof(papszLines[3]);

        // correct for center of pixel vs. top left of pixel
        padfGeoTransform[0] -= 0.5 * padfGeoTransform[1];
        padfGeoTransform[0] -= 0.5 * padfGeoTransform[2];
        padfGeoTransform[3] -= 0.5 * padfGeoTransform[4];
        padfGeoTransform[3] -= 0.5 * padfGeoTransform[5];

        CSLDestroy(papszLines);

        return TRUE;
    }
    else
    {
        CPLDebug( "GDAL", 
                  "GDALReadWorldFile(%s) found file, but it was corrupt.",
                  pszTFW );
        CSLDestroy(papszLines);
        return FALSE;
    }
}

/************************************************************************/
/*                         GDALWriteWorldFile()                          */
/*                                                                      */
/*      Helper function for translator implementators wanting           */
/*      support for ESRI world files.                                   */
/************************************************************************/

int GDALWriteWorldFile( const char * pszBaseFilename, const char *pszExtension,
                       double *padfGeoTransform )

{
    const char	*pszTFW;
	FILE	*fpTFW;

	pszTFW = CPLResetExtension( pszBaseFilename, pszExtension );
	fpTFW = VSIFOpen( pszTFW, "wt" );
	if( fpTFW == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We open the file, now fill it with the world data.                        */
/* -------------------------------------------------------------------- */
	fprintf( fpTFW, "%.10f\n", padfGeoTransform[1] );
	fprintf( fpTFW, "%.10f\n", padfGeoTransform[4] );
	fprintf( fpTFW, "%.10f\n", padfGeoTransform[2] );
	fprintf( fpTFW, "%.10f\n", padfGeoTransform[5] );
	fprintf( fpTFW, "%.10f\n", padfGeoTransform[0] 
                 + 0.5 * padfGeoTransform[1]
                 + 0.5 * padfGeoTransform[2] );
	fprintf( fpTFW, "%.10f\n", padfGeoTransform[3]
                 + 0.5 * padfGeoTransform[4]
                 + 0.5 * padfGeoTransform[5] );

	VSIFClose( fpTFW );
	return TRUE;
}

/************************************************************************/
/*                          GDALVersionInfo()                           */
/************************************************************************/

/**
 * Get runtime version information.
 *
 * Available pszRequest values:
 * <ul>
 * <li> "VERSION_NUM": Returns GDAL_VERSION_NUM formatted as a string.  ie. "1170"
 * <li> "RELEASE_DATE": Returns GDAL_RELEASE_DATE formatted as a string.  
 * ie. "20020416".
 * <li> "RELEASE_NAME": Returns the GDAL_RELEASE_NAME. ie. "1.1.7"
 * <li> "--version": Returns one line version message suitable for use in 
 * response to --version requests.  ie. "GDAL 1.1.7, released 2002/04/16"
 * </ul>
 *
 * @param pszRequest the type of version info desired, as listed above.
 *
 * @return an internal string containing the requested information.
 */

const char *GDALVersionInfo( const char *pszRequest )

{
    static char szResult[128];

    
    if( pszRequest == NULL || EQUAL(pszRequest,"VERSION_NUM") )
        sprintf( szResult, "%d", GDAL_VERSION_NUM );
    else if( EQUAL(pszRequest,"RELEASE_DATE") )
        sprintf( szResult, "%d", GDAL_RELEASE_DATE );
    else if( EQUAL(pszRequest,"RELEASE_NAME") )
        sprintf( szResult, "%s", GDAL_RELEASE_NAME );
    else // --version
        sprintf( szResult, "GDAL %s, released %d/%02d/%02d",
                 GDAL_RELEASE_NAME, 
                 GDAL_RELEASE_DATE / 10000, 
                 (GDAL_RELEASE_DATE % 10000) / 100,
                 GDAL_RELEASE_DATE % 100 );

    return szResult;
}

/************************************************************************/
/*                            GDALDecToDMS()                            */
/*                                                                      */
/*      Translate a decimal degrees value to a DMS string with          */
/*      hemisphere.                                                     */
/************************************************************************/

const char *GDALDecToDMS( double dfAngle, const char * pszAxis,
                          int nPrecision )

{
    int		nDegrees, nMinutes;
    double	dfSeconds;
    char	szFormat[30];
    static char szBuffer[50];
    const char	*pszHemisphere;
    

    nDegrees = (int) ABS(dfAngle);
    nMinutes = (int) ((ABS(dfAngle) - nDegrees) * 60);
    dfSeconds = (ABS(dfAngle) * 3600 - nDegrees*3600 - nMinutes*60);

    if( EQUAL(pszAxis,"Long") && dfAngle < 0.0 )
        pszHemisphere = "W";
    else if( EQUAL(pszAxis,"Long") )
        pszHemisphere = "E";
    else if( dfAngle < 0.0 )
        pszHemisphere = "S";
    else
        pszHemisphere = "N";

    sprintf( szFormat, "%%3dd%%2d\'%%.%df\"%s", nPrecision, pszHemisphere );
    sprintf( szBuffer, szFormat, nDegrees, nMinutes, dfSeconds );

    return( szBuffer );
}

/************************************************************************/
/* -------------------------------------------------------------------- */
/*      The following stubs are present to ensure that older GDAL       */
/*      bridges don't fail with newer libraries.                        */
/* -------------------------------------------------------------------- */
/************************************************************************/

CPL_C_START

void *GDALCreateProjDef( const char * pszDef )
{
    CPLDebug( "GDAL", "GDALCreateProjDef no longer supported." );
    return NULL;
}

CPLErr GDALReprojectToLongLat( void *pDef, double *, double * )
{
    CPLDebug( "GDAL", "GDALReprojectToLatLong no longer supported." );
    return CE_Failure;
}

CPLErr GDALReprojectFromLongLat( void *pDef, double *, double * )
{
    CPLDebug( "GDAL", "GDALReprojectFromLatLong no longer supported." );
    return CE_Failure;
}

void GDALDestroyProjDef( void *pDef )

{
    CPLDebug( "GDAL", "GDALDestroyProjDef no longer supported." );
}

CPL_C_END
