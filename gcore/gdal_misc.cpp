/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Free standing functions for GDAL.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 * Revision 1.49  2003/08/18 12:43:47  warmerda
 * always include ogr_spatialref.h
 *
 * Revision 1.48  2003/08/12 22:13:32  warmerda
 * Changed GDALReadTabFile(0 so that if the CoordSys results in a PROJCS, but
 * the UNITS keyword is set to "degrees" we just use the GEOGCS portion.  It
 * seems that the projection is "just for display", and the GCPs will actually
 * be in lat/long.
 *
 * Revision 1.47  2003/06/03 19:44:00  warmerda
 * added GDALRPCInfo support
 *
 * Revision 1.46  2003/05/23 15:52:54  warmerda
 * Cosmetic changes made.
 *
 * Revision 1.45  2003/05/21 04:31:53  warmerda
 * avoid warnings
 *
 * Revision 1.44  2003/04/30 17:13:48  warmerda
 * added docs for many C functions
 *
 * Revision 1.43  2003/03/13 14:37:17  warmerda
 * better end of range checking in GDALTermProgress
 *
 * Revision 1.42  2003/02/15 20:22:14  warmerda
 * GDALReadTabFile() returns true if it gets GCPs but cant make geotransform
 *
 * Revision 1.41  2003/01/27 21:55:52  warmerda
 * various documentation improvements
 *
 * Revision 1.40  2002/12/11 21:21:46  warmerda
 * fixed debug format problem
 *
 * Revision 1.39  2002/12/09 20:05:31  warmerda
 * fixed return flag from GDALReadTabFile
 *
 * Revision 1.38  2002/12/09 18:53:25  warmerda
 * GDALDecToDMS() now calls CPLDecToDMS()
 *
 * Revision 1.37  2002/12/05 17:55:30  warmerda
 * gdalreadtabfile should not be static
 *
 * Revision 1.36  2002/12/05 15:46:38  warmerda
 * added GDALReadTabFile()
 *
 * Revision 1.35  2002/07/09 20:33:12  warmerda
 * expand tabs
 *
 * Revision 1.34  2002/05/06 21:37:29  warmerda
 * added GDALGCPsToGeoTransform
 *
 * Revision 1.33  2002/04/25 16:18:41  warmerda
 * added extra checking
 *
 * Revision 1.32  2002/04/24 19:21:26  warmerda
 * Include <ctype.h> for toupper(), tolower().
 *
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
 */

#include "gdal_priv.h"
#include "cpl_string.h"
#include <ctype.h>

CPL_CVSID("$Id$");

#include "ogr_spatialref.h"

#ifdef HAVE_MITAB
// from mitab component.
OGRSpatialReference * MITABCoordSys2SpatialRef( const char * pszCoordSys );
#endif

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
    int         bFloating, bComplex, nBits, bSigned;

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

/**
 * Get data type size in bits.
 *
 * Returns the size of a a GDT_* type in bits, <b>not bytes</b>!
 *
 * @param data type, such as GDT_Byte. 
 * @return the number of bits or zero if it is not recognised.
 */

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

/**
 * Is data type complex? 
 *
 * @return TRUE if the passed type is complex (one of GDT_CInt16, GDT_CInt32, 
 * GDT_CFloat32 or GDT_CFloat64), that is it consists of a real and imaginary
 * component. 
 */

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

/**
 * Get name of data type.
 *
 * Returns a symbolic name for the data type.  This is essentially the
 * the enumerated item name with the GDT_ prefix removed.  So GDT_Byte returns
 * "Byte".  The returned strings are static strings and should not be modified
 * or freed by the application.  These strings are useful for reporting
 * datatypes in debug statements, errors and other user output. 
 *
 * @param eDataType type to get name of.
 * @return string corresponding to type.
 */

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
 * This is a stub (does nothing) implementation of the GDALProgressFunc()
 * semantics.  It is primarily useful for passing to functions that take
 * a GDALProgressFunc() argument but for which the application does not want
 * to use one of the other progress functions that actually do something.
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

/**
 * Scaled progress transformer.
 *
 * This is the progress function that should be passed along with the
 * callback data returned by GDALCreateScaledProgress().
 */

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

/**
 * Create scaled progress transformer.
 *
 * Sometimes when an operations wants to report progress it actually
 * invokes several subprocesses which also take GDALProgressFunc()s, 
 * and it is desirable to map the progress of each sub operation into
 * a portion of 0.0 to 1.0 progress of the overall process.  The scaled
 * progress function can be used for this. 
 *
 * For each subsection a scaled progress function is created and
 * instead of passing the overall progress func down to the sub functions,
 * the GDALScaledProgress() function is passed instead.
 *
 * @param dfMin the value to which 0.0 in the sub operation is mapped.
 * @param dfMax the value to which 1.0 is the sub operation is mapped.
 * @param pfnProgress the overall progress function.
 * @param dData the overall progress function callback data. 
 *
 * @return pointer to pass as pProgressArg to sub functions.  Should be freed
 * with GDALDestroyScaledProgress(). 
 *
 * Example:
 *
 * \code
 *   int MyOperation( ..., GDALProgressFunc pfnProgress, void *pProgressData );
 *
 *   {
 *       void *pScaledProgress;
 *
 *       pScaledProgress = GDALCreateScaledProgress( 0.0, 0.5, pfnProgress, 
 *                                                   pProgressData );
 *       GDALDoLongSlowOperation( ..., GDALScaledProgressFunc, pProgressData );
 *       GDALDestroyScaledProgress( pScaledProgress );
 *
 *       pScaledProgress = GDALCreateScaledProgress( 0.5, 1.0, pfnProgress, 
 *                                                   pProgressData );
 *       GDALDoAnotherOperation( ..., GDALScaledProgressFunc, pProgressData );
 *       GDALDestroyScaledProgress( pScaledProgress );
 *
 *       return ...;
 *   }
 * \endcode
 */

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

/**
 * Cleanup scaled progress handle.
 *
 * This function cleans up the data associated with a scaled progress function
 * as returned by GADLCreateScaledProgress(). 
 *
 * @param pData scaled progress handle returned by GDALCreateScaledProgress().
 */

void GDALDestroyScaledProgress( void * pData )

{
    CPLFree( pData );
}

/************************************************************************/
/*                          GDALTermProgress()                          */
/************************************************************************/

/**
 * Simple progress report to terminal.
 *
 * This progress reporter prints simple progress report to the
 * terminal window.  The progress report generally looks something like
 * this:

\verbatim
0...10...20...30...40...50...60...70...80...90...100 - done.
\endverbatim

 * Every 2.5% of progress another number or period is emitted.  Note that
 * GDALTermProgress() uses internal static data to keep track of the last
 * percentage reported and will get confused if two terminal based progress
 * reportings are active at the same time.
 *
 * @param dfComplete completion ratio from 0.0 to 1.0.
 * @param pszMessage optional message.
 * @param pProgressArg ignored callback data argument. 
 *
 * @return Always returns TRUE indicating the process should continue.
 */

int GDALTermProgress( double dfComplete, const char *pszMessage, 
                      void * pProgressArg )

{
    static double dfLastComplete = -1.0;

    (void) pProgressArg;

    if( dfLastComplete > dfComplete )
    {
        if( dfLastComplete >= 1.0 )
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
    int         nBlockPixels, nBlockCount;

    dfNoDataValue = poBand->GetNoDataValue( &bGotNoDataValue );

    poBand->GetBlockSize( &nBlockXSize, &nBlockYSize );

    nBlocksPerRow = (poBand->GetXSize() + nBlockXSize - 1) / nBlockXSize;
    nBlocksPerColumn = (poBand->GetYSize() + nBlockYSize - 1) / nBlockYSize;

    nBlockPixels = nBlockXSize * nBlockYSize;
    nBlockCount = nBlocksPerRow * nBlocksPerColumn;

    if( nBlocksPerRow == 0 || nBlocksPerColumn == 0 || nBlockPixels == 0 
        || nBlockCount == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GDALGetRandomSample(): returning because band"
                  " appears degenerate." );

        return FALSE;
    }

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
                int     iOffset;

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
                    pafSampleBuf[nActualSamples++] = (float) dfValue;
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
    GDAL_GCP    *pasReturn;

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
/*                         GDALReadTabFile()                            */
/*                                                                      */
/*      Helper function for translator implementators wanting           */
/*      support for MapInfo .tab-files.                                 */
/************************************************************************/

#define MAX_GCP 256
 
int GDALReadTabFile( const char * pszBaseFilename, 
                     double *padfGeoTransform, char **ppszWKT, 
                     int *pnGCPCount, GDAL_GCP **ppasGCPs )


{
    const char	*pszTAB;
    FILE	*fpTAB;
    char	**papszLines;
    char    **papszTok=NULL;
    int 	bTypeRasterFound = FALSE;
    int		bInsideTableDef = FALSE;
    int		iLine, numLines=0;
    int 	nCoordinateCount = 0;
    GDAL_GCP    asGCPs[MAX_GCP];
    

/* -------------------------------------------------------------------- */
/*      Try lower case, then upper case.                                */
/* -------------------------------------------------------------------- */
    pszTAB = CPLResetExtension( pszBaseFilename, "tab" );

    fpTAB = VSIFOpen( pszTAB, "rt" );

#ifndef WIN32
    if( fpTAB == NULL )
    {
        pszTAB = CPLResetExtension( pszBaseFilename, "TAB" );
        fpTAB = VSIFOpen( pszTAB, "rt" );
    }
#endif
    
    if( fpTAB == NULL )
        return FALSE;

    VSIFClose( fpTAB );

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    papszLines = CSLLoad( pszTAB );

    numLines = CSLCount(papszLines);

    // Iterate all lines in the TAB-file
    for(iLine=0; iLine<numLines; iLine++)
    {
        CSLDestroy(papszTok);
        papszTok = CSLTokenizeStringComplex(papszLines[iLine], " \t(),;", 
                                            TRUE, FALSE);

        if (CSLCount(papszTok) < 2)
            continue;

        // Did we find table definition
        if (EQUAL(papszTok[0], "Definition") && EQUAL(papszTok[1], "Table") )
        {
            bInsideTableDef = TRUE;
        }
        else if (bInsideTableDef && (EQUAL(papszTok[0], "Type")) )
        {
            // Only RASTER-type will be handled
            if (EQUAL(papszTok[1], "RASTER"))
            {
            	bTypeRasterFound = TRUE;
            }
            else
            {
                CSLDestroy(papszTok);
                CSLDestroy(papszLines);
                return FALSE;
            }
        }
        else if (bTypeRasterFound && bInsideTableDef
                 && CSLCount(papszTok) > 5
                 && EQUAL(papszTok[4], "Label") 
                 && nCoordinateCount < MAX_GCP )
        {
            GDALInitGCPs( 1, asGCPs + nCoordinateCount );
            
            asGCPs[nCoordinateCount].dfGCPPixel = atof(papszTok[2]);
            asGCPs[nCoordinateCount].dfGCPLine = atof(papszTok[3]);
            asGCPs[nCoordinateCount].dfGCPX = atof(papszTok[0]);
            asGCPs[nCoordinateCount].dfGCPY = atof(papszTok[1]);
            CPLFree( asGCPs[nCoordinateCount].pszId );
            asGCPs[nCoordinateCount].pszId = CPLStrdup(papszTok[5]);

            nCoordinateCount++;
        }
        else if( bTypeRasterFound && bInsideTableDef 
                 && EQUAL(papszTok[0],"CoordSys") 
                 && ppszWKT != NULL )
        {
#ifdef HAVE_MITAB
            OGRSpatialReference *poSRS = NULL;
            
            poSRS = MITABCoordSys2SpatialRef( papszLines[iLine] );
            if( poSRS != NULL )
            {
                poSRS->exportToWkt( ppszWKT );
                delete poSRS;
            }

#else
            CPLDebug( "GDAL", "GDALReadTabFile(): Found `%s',\n"
                 "but GDALReadTabFile() not configured with MITAB callout.",
                      papszLines[iLine] );
#endif
        }
        else if( EQUAL(papszTok[0],"Units") 
                 && CSLCount(papszTok) > 1 
                 && EQUAL(papszTok[1],"degree") )
        {
            /*
            ** If we have units of "degree", but a projected coordinate
            ** system we need to convert it to geographic.  See to01_02.TAB.
            */
            if( ppszWKT != NULL && *ppszWKT != NULL
                && EQUALN(*ppszWKT,"PROJCS",6) )
            {
                OGRSpatialReference oSRS, oSRSGeogCS;
                char *pszSrcWKT = *ppszWKT;

                oSRS.importFromWkt( &pszSrcWKT );
                oSRSGeogCS.CopyGeogCSFrom( &oSRS );
                CPLFree( *ppszWKT );
                oSRSGeogCS.exportToWkt( ppszWKT );
            }
        }
            
    }

/* -------------------------------------------------------------------- */
/*      Try to convert the GCPs into a geotransform definition, if      */
/*      possible.  Otherwise we will need to use them as GCPs.          */
/* -------------------------------------------------------------------- */
    if( !GDALGCPsToGeoTransform( nCoordinateCount, asGCPs, padfGeoTransform, 
                                 FALSE ) )
    {
        CPLDebug( "GDAL", 
                  "GDALReadTabFile(%s) found file, wasn't able to derive a\n"
                  "first order geotransform.  Using points as GCPs.",
                  pszTAB );

        *ppasGCPs = (GDAL_GCP *) 
            CPLCalloc(sizeof(GDAL_GCP),nCoordinateCount);
        memcpy( *ppasGCPs, asGCPs, sizeof(GDAL_GCP) * nCoordinateCount );
        *pnGCPCount = nCoordinateCount;
    }
    else
    {
        GDALDeinitGCPs( nCoordinateCount, asGCPs );
    }
     
    CSLDestroy(papszTok);
    CSLDestroy(papszLines);

    return TRUE;
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
    const char  *pszTFW;
    char        szExtUpper[32], szExtLower[32];
    int         i;
    FILE        *fpTFW;
    char        **papszLines;

    if( *pszExtension == '.' )
        pszExtension++;

/* -------------------------------------------------------------------- */
/*      Generate upper and lower case versions of the extension.        */
/* -------------------------------------------------------------------- */
    strncpy( szExtUpper, pszExtension, 32 );
    strncpy( szExtLower, pszExtension, 32 );

    for( i = 0; szExtUpper[i] != '\0' && i < 32; i++ )
    {
        szExtUpper[i] = (char) toupper(szExtUpper[i]);
        szExtLower[i] = (char) tolower(szExtLower[i]);
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
    const char  *pszTFW;
    FILE    *fpTFW;

    pszTFW = CPLResetExtension( pszBaseFilename, pszExtension );
    fpTFW = VSIFOpen( pszTFW, "wt" );
    if( fpTFW == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We open the file, now fill it with the world data.              */
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
    return CPLDecToDMS( dfAngle, pszAxis, nPrecision );
}

/************************************************************************/
/*                       GDALGCPsToGeoTransform()                       */
/************************************************************************/

/**
 * Generate Geotransform from GCPs. 
 *
 * Given a set of GCPs perform first order fit as a geotransform. 
 * 
 * @param nGCPCount the number of GCPs being passed in.
 * @param pasGCPs the list of GCP structures. 
 * @param padfGeoTransform the six double array in which the affine 
 * geotransformation will be returned. 
 * @param bApproxOK If FALSE the function will fail if the geotransform is not 
 * essentially an exact fit (within 0.25 pixel) for all GCPs. 
 * 
 * @return TRUE on success or FALSE if there aren't enough points to prepare a
 * geotransform, or if bApproxOK is FALSE and the fit is poor.
 */

int GDALGCPsToGeoTransform( int nGCPCount, const GDAL_GCP *pasGCPs,
                            double *padfGeoTransform, int bApproxOK )

{
    int   iAnchor=0, iPnt1, iPnt2, i;
    double adfDPixel[2], adfDLine[2], adfDX[2], adfDY[2];

/* -------------------------------------------------------------------- */
/*      Recognise a few special cases.                                  */
/* -------------------------------------------------------------------- */
    if( nGCPCount < 2 )
        return FALSE;

    if( nGCPCount == 2 )
    {
        if( pasGCPs[1].dfGCPPixel == pasGCPs[0].dfGCPPixel 
            || pasGCPs[1].dfGCPLine == pasGCPs[0].dfGCPLine )
            return FALSE;

        padfGeoTransform[1] = (pasGCPs[1].dfGCPX - pasGCPs[0].dfGCPX)
            / (pasGCPs[1].dfGCPPixel - pasGCPs[0].dfGCPPixel);
        padfGeoTransform[2] = 0.0;

        padfGeoTransform[4] = 0.0;
        padfGeoTransform[5] = (pasGCPs[1].dfGCPY - pasGCPs[0].dfGCPY)
            / (pasGCPs[1].dfGCPLine - pasGCPs[0].dfGCPLine);
        
        padfGeoTransform[0] = pasGCPs[0].dfGCPX 
            - pasGCPs[0].dfGCPPixel * padfGeoTransform[1]
            - pasGCPs[0].dfGCPLine * padfGeoTransform[2];
        
        padfGeoTransform[3] = pasGCPs[0].dfGCPY 
            - pasGCPs[0].dfGCPPixel * padfGeoTransform[4]
            - pasGCPs[0].dfGCPLine * padfGeoTransform[5];

        return TRUE;
    }

/* -------------------------------------------------------------------- */
/*      We use the first point as our anchor.  Select two other         */
/*      points that don't have the same Pixel value to analyse.         */
/* -------------------------------------------------------------------- */
    iPnt1 = -1;
    iPnt2 = -1;
    for( i = 1; (iPnt1 == -1 || iPnt2 == -1 ) && i < nGCPCount; i++ )
    {
        double dfDPixel = pasGCPs[i].dfGCPPixel-pasGCPs[iAnchor].dfGCPPixel;
        double dfDLine = pasGCPs[i].dfGCPLine - pasGCPs[iAnchor].dfGCPLine;
        double dfDX = pasGCPs[i].dfGCPX - pasGCPs[iAnchor].dfGCPX;
        double dfDY = pasGCPs[i].dfGCPY - pasGCPs[iAnchor].dfGCPY;
        
        if( iPnt1 == -1 && ABS(dfDPixel) > 0.001 )
        {
            iPnt1 = i;
            adfDPixel[0] = dfDPixel;
            adfDLine[0] = dfDLine;
            adfDX[0] = dfDX;
            adfDY[0] = dfDY;
        }
        else if( iPnt2 == -1 )
        {
            iPnt2 = i;
            adfDPixel[1] = dfDPixel;
            adfDLine[1] = dfDLine;
            adfDX[1] = dfDX;
            adfDY[1] = dfDY;
        }
    }

/* -------------------------------------------------------------------- */
/*      If necessary, scale one of the points to avoid divide by        */
/*      zeros.                                                          */
/* -------------------------------------------------------------------- */
    if( ABS((adfDLine[0] / adfDPixel[0] - adfDLine[1])) < 0.0001 )
    {
        adfDX[1] *= 2;
        adfDY[1] *= 2;
        adfDPixel[1] *= 2;
        adfDLine[1] *= 2;
    }

/* -------------------------------------------------------------------- */
/*      Compute X related coefficients.                                 */
/* -------------------------------------------------------------------- */
    padfGeoTransform[2] = 
        (adfDX[1] - (adfDPixel[1] * adfDX[0]) / adfDPixel[0]) 
        / (adfDLine[1] - (adfDLine[0]*adfDPixel[1]) / adfDPixel[0]);

    padfGeoTransform[1] = (adfDX[0] - adfDLine[0] * padfGeoTransform[2])
        / adfDPixel[0];

/* -------------------------------------------------------------------- */
/*      Compute Y related coefficients.                                 */
/* -------------------------------------------------------------------- */
    padfGeoTransform[5] = 
        (adfDY[1] - (adfDPixel[1] * adfDY[0]) / adfDPixel[0])
        / (adfDLine[1] - (adfDLine[0]*adfDPixel[1]) / adfDPixel[0]);

    padfGeoTransform[4] = 
        (adfDY[0] - adfDLine[0] * padfGeoTransform[5]) / adfDPixel[0];

/* -------------------------------------------------------------------- */
/*      Compute top/left origin.                                        */
/* -------------------------------------------------------------------- */

    padfGeoTransform[0] = pasGCPs[0].dfGCPX 
        - pasGCPs[0].dfGCPPixel * padfGeoTransform[1]
        - pasGCPs[0].dfGCPLine * padfGeoTransform[2];
        
    padfGeoTransform[3] = pasGCPs[0].dfGCPY 
        - pasGCPs[0].dfGCPPixel * padfGeoTransform[4]
        - pasGCPs[0].dfGCPLine * padfGeoTransform[5];

/* -------------------------------------------------------------------- */
/*      Now check if any of the input points fit this poorly.           */
/* -------------------------------------------------------------------- */
    if( !bApproxOK )
    {
        double dfPixelSize = ABS(padfGeoTransform[1]) 
            + ABS(padfGeoTransform[2])
            + ABS(padfGeoTransform[4])
            + ABS(padfGeoTransform[5]);

        for( i = 0; i < nGCPCount; i++ )
        {
            double      dfErrorX, dfErrorY;

            dfErrorX = 
                (pasGCPs[i].dfGCPPixel * padfGeoTransform[1]
                 + pasGCPs[i].dfGCPLine * padfGeoTransform[2]
                 + padfGeoTransform[0]) 
                - pasGCPs[i].dfGCPX;
            dfErrorY = 
                (pasGCPs[i].dfGCPPixel * padfGeoTransform[4]
                 + pasGCPs[i].dfGCPLine * padfGeoTransform[5]
                 + padfGeoTransform[3]) 
                - pasGCPs[i].dfGCPY;

            if( ABS(dfErrorX) > 0.25 * dfPixelSize 
                || ABS(dfErrorY) > 0.25 * dfPixelSize )
                return FALSE;
        }
    }

    return TRUE;
}

/************************************************************************/
/*                          _FetchDblFromMD()                           */
/************************************************************************/

static int _FetchDblFromMD( char **papszMD, const char *pszKey, 
                            double *padfTarget, int nCount, double dfDefault )

{
    char szFullKey[200];

    sprintf( szFullKey, "RPC_%s", pszKey );

    const char *pszValue = CSLFetchNameValue( papszMD, szFullKey );
    int i;
    
    for( i = 0; i < nCount; i++ )
        padfTarget[i] = dfDefault;

    if( pszKey == NULL )
        return FALSE;

    if( nCount == 1 )
    {
        *padfTarget = atof( pszValue );
        return TRUE;
    }

    char **papszTokens = CSLTokenizeStringComplex( pszValue, " ,", 
                                                   FALSE, FALSE );

    if( CSLCount( papszTokens ) != nCount )
    {
        CSLDestroy( papszTokens );
        return FALSE;
    }

    for( i = 0; i < nCount; i++ )
        padfTarget[i] = atof(papszTokens[i]);

    CSLDestroy( papszTokens );

    return TRUE;
}

/************************************************************************/
/*                         GDALExtractRPCInfo()                         */
/************************************************************************/

int GDALExtractRPCInfo( char **papszMD, GDALRPCInfo *psRPC )

{
    if( CSLFetchNameValue( papszMD, "RPC_LINE_NUM_COEFF" ) == NULL )
        return FALSE;

    if( CSLFetchNameValue( papszMD, "RPC_LINE_NUM_COEFF" ) == NULL 
        || CSLFetchNameValue( papszMD, "RPC_LINE_DEN_COEFF" ) == NULL 
        || CSLFetchNameValue( papszMD, "RPC_SAMP_NUM_COEFF" ) == NULL 
        || CSLFetchNameValue( papszMD, "RPC_SAMP_DEN_COEFF" ) == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                 "Some required RPC metadata missing in GDALExtractRPCInfo()");
        return FALSE;
    }

    _FetchDblFromMD( papszMD, "LINE_OFF", &(psRPC->dfLINE_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, "LINE_SCALE", &(psRPC->dfLINE_SCALE), 1, 1.0 );
    _FetchDblFromMD( papszMD, "SAMP_OFF", &(psRPC->dfSAMP_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, "SAMP_SCALE", &(psRPC->dfSAMP_SCALE), 1, 1.0 );
    _FetchDblFromMD( papszMD, "HEIGHT_OFF", &(psRPC->dfHEIGHT_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, "HEIGHT_SCALE", &(psRPC->dfHEIGHT_SCALE),1, 1.0);
    _FetchDblFromMD( papszMD, "LAT_OFF", &(psRPC->dfLAT_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, "LAT_SCALE", &(psRPC->dfLAT_SCALE), 1, 1.0 );
    _FetchDblFromMD( papszMD, "LONG_OFF", &(psRPC->dfLONG_OFF), 1, 0.0 );
    _FetchDblFromMD( papszMD, "LONG_SCALE", &(psRPC->dfLONG_SCALE), 1, 1.0 );

    _FetchDblFromMD( papszMD, "LINE_NUM_COEFF", psRPC->adfLINE_NUM_COEFF, 
                     20, 0.0 );
    _FetchDblFromMD( papszMD, "LINE_DEN_COEFF", psRPC->adfLINE_DEN_COEFF, 
                     20, 0.0 );
    _FetchDblFromMD( papszMD, "SAMP_NUM_COEFF", psRPC->adfSAMP_NUM_COEFF, 
                     20, 0.0 );
    _FetchDblFromMD( papszMD, "SAMP_DEN_COEFF", psRPC->adfSAMP_DEN_COEFF, 
                     20, 0.0 );
    
    _FetchDblFromMD( papszMD, "MIN_LONG", &(psRPC->dfMIN_LONG), 1, -180.0 );
    _FetchDblFromMD( papszMD, "MIN_LAT", &(psRPC->dfMIN_LAT), 1, -90.0 );
    _FetchDblFromMD( papszMD, "MAX_LONG", &(psRPC->dfMAX_LONG), 1, 180.0 );
    _FetchDblFromMD( papszMD, "MAX_LAT", &(psRPC->dfMAX_LAT), 1, 90.0 );

    return TRUE;
}

/************************************************************************/
/* -------------------------------------------------------------------- */
/*      The following stubs are present to ensure that older GDAL       */
/*      bridges don't fail with newer libraries.                        */
/* -------------------------------------------------------------------- */
/************************************************************************/

CPL_C_START

void *GDALCreateProjDef( const char * )
{
    CPLDebug( "GDAL", "GDALCreateProjDef no longer supported." );
    return NULL;
}

CPLErr GDALReprojectToLongLat( void *, double *, double * )
{
    CPLDebug( "GDAL", "GDALReprojectToLatLong no longer supported." );
    return CE_Failure;
}

CPLErr GDALReprojectFromLongLat( void *, double *, double * )
{
    CPLDebug( "GDAL", "GDALReprojectFromLatLong no longer supported." );
    return CE_Failure;
}

void GDALDestroyProjDef( void * )

{
    CPLDebug( "GDAL", "GDALDestroyProjDef no longer supported." );
}

CPL_C_END
