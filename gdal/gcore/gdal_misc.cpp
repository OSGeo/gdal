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
 ****************************************************************************/

#include "gdal_priv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_multiproc.h"
#include <ctype.h>
#include <string>

CPL_CVSID("$Id$");

#include "ogr_spatialref.h"

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
 * \brief Return the smallest data type that can fully express both input data
 * types.
 *
 * @param eType1 first data type.
 * @param eType2 second data type.
 *
 * @return a data type able to express eType1 and eType2.
 */

GDALDataType CPL_STDCALL 
GDALDataTypeUnion( GDALDataType eType1, GDALDataType eType2 )

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
      case GDT_CInt16:
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
 * \brief Get data type size in bits.
 *
 * Returns the size of a a GDT_* type in bits, <b>not bytes</b>!
 *
 * @param eDataType type, such as GDT_Byte.
 * @return the number of bits or zero if it is not recognised.
 */

int CPL_STDCALL GDALGetDataTypeSize( GDALDataType eDataType )

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
        return 0;
    }
}

/************************************************************************/
/*                       GDALDataTypeIsComplex()                        */
/************************************************************************/

/**
 * \brief Is data type complex? 
 *
 * @return TRUE if the passed type is complex (one of GDT_CInt16, GDT_CInt32, 
 * GDT_CFloat32 or GDT_CFloat64), that is it consists of a real and imaginary
 * component. 
 */

int CPL_STDCALL GDALDataTypeIsComplex( GDALDataType eDataType )

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
 * \brief Get name of data type.
 *
 * Returns a symbolic name for the data type.  This is essentially the
 * the enumerated item name with the GDT_ prefix removed.  So GDT_Byte returns
 * "Byte".  The returned strings are static strings and should not be modified
 * or freed by the application.  These strings are useful for reporting
 * datatypes in debug statements, errors and other user output. 
 *
 * @param eDataType type to get name of.
 * @return string corresponding to existing data type
 *         or NULL pointer if invalid type given.
 */

const char * CPL_STDCALL GDALGetDataTypeName( GDALDataType eDataType )

{
    switch( eDataType )
    {
      case GDT_Unknown:
        return "Unknown";

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
/*                        GDALGetDataTypeByName()                       */
/************************************************************************/

/**
 * \brief Get data type by symbolic name.
 *
 * Returns a data type corresponding to the given symbolic name. This
 * function is opposite to the GDALGetDataTypeName().
 *
 * @param pszName string containing the symbolic name of the type.
 * 
 * @return GDAL data type.
 */

GDALDataType CPL_STDCALL GDALGetDataTypeByName( const char *pszName )

{
    VALIDATE_POINTER1( pszName, "GDALGetDataTypeByName", GDT_Unknown );

    int	iType;
    
    for( iType = 1; iType < GDT_TypeCount; iType++ )
    {
        if( GDALGetDataTypeName((GDALDataType)iType) != NULL
            && EQUAL(GDALGetDataTypeName((GDALDataType)iType), pszName) )
        {
            return (GDALDataType)iType;
        }
    }

    return GDT_Unknown;
}

/************************************************************************/
/*                        GDALGetAsyncStatusTypeByName()                */
/************************************************************************/
/**
 * Get AsyncStatusType by symbolic name.
 *
 * Returns a data type corresponding to the given symbolic name. This
 * function is opposite to the GDALGetAsyncStatusTypeName().
 *
 * @param pszName string containing the symbolic name of the type.
 * 
 * @return GDAL AsyncStatus type.
 */
GDALAsyncStatusType CPL_DLL CPL_STDCALL GDALGetAsyncStatusTypeByName( const char *pszName )
{
    VALIDATE_POINTER1( pszName, "GDALGetAsyncStatusTypeByName", GARIO_ERROR);

    int	iType;

    for( iType = 1; iType < GARIO_TypeCount; iType++ )
    {
        if( GDALGetAsyncStatusTypeName((GDALAsyncStatusType)iType) != NULL
            && EQUAL(GDALGetAsyncStatusTypeName((GDALAsyncStatusType)iType), pszName) )
        {
            return (GDALAsyncStatusType)iType;
        }
    }

    return GARIO_ERROR;
}


/************************************************************************/
/*                        GDALGetAsyncStatusTypeName()                 */
/************************************************************************/

/**
 * Get name of AsyncStatus data type.
 *
 * Returns a symbolic name for the AsyncStatus data type.  This is essentially the
 * the enumerated item name with the GARIO_ prefix removed.  So GARIO_COMPLETE returns
 * "COMPLETE".  The returned strings are static strings and should not be modified
 * or freed by the application.  These strings are useful for reporting
 * datatypes in debug statements, errors and other user output. 
 *
 * @param eAsyncStatusType type to get name of.
 * @return string corresponding to type.
 */

 const char * CPL_STDCALL GDALGetAsyncStatusTypeName( GDALAsyncStatusType eAsyncStatusType )

{
    switch( eAsyncStatusType )
    {
      case GARIO_PENDING:
        return "PENDING";

      case GARIO_UPDATE:
        return "UPDATE";

      case GARIO_ERROR:
        return "ERROR";

      case GARIO_COMPLETE:
        return "COMPLETE";
      default:
        return NULL;
    }
}

/************************************************************************/
/*                  GDALGetPaletteInterpretationName()                  */
/************************************************************************/

/**
 * \brief Get name of palette interpretation
 *
 * Returns a symbolic name for the palette interpretation.  This is the
 * the enumerated item name with the GPI_ prefix removed.  So GPI_Gray returns
 * "Gray".  The returned strings are static strings and should not be modified
 * or freed by the application.
 *
 * @param eInterp palette interpretation to get name of.
 * @return string corresponding to palette interpretation.
 */

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

/**
 * \brief Get name of color interpretation
 *
 * Returns a symbolic name for the color interpretation.  This is derived from
 * the enumerated item name with the GCI_ prefix removed, but there are some
 * variations. So GCI_GrayIndex returns "Gray" and GCI_RedBand returns "Red".
 * The returned strings are static strings and should not be modified
 * or freed by the application.
 *
 * @param eInterp color interpretation to get name of.
 * @return string corresponding to color interpretation
 *         or NULL pointer if invalid enumerator given.
 */

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
        
      case GCI_YCbCr_YBand:
        return "YCbCr_Y";
        
      case GCI_YCbCr_CbBand:
        return "YCbCr_Cb";
        
      case GCI_YCbCr_CrBand:
        return "YCbCr_Cr";
        
      default:
        return "Unknown";
    }
}

/************************************************************************/
/*                GDALGetColorInterpretationByName()                    */
/************************************************************************/

/**
 * \brief Get color interpreation by symbolic name.
 *
 * Returns a color interpreation corresponding to the given symbolic name. This
 * function is opposite to the GDALGetColorInterpretationName().
 *
 * @param pszName string containing the symbolic name of the color interpretation.
 * 
 * @return GDAL color interpretation.
 *
 * @since GDAL 1.7.0
 */

GDALColorInterp GDALGetColorInterpretationByName( const char *pszName )

{
    VALIDATE_POINTER1( pszName, "GDALGetColorInterpretationByName", GCI_Undefined );

    int	iType;
    
    for( iType = 0; iType <= GCI_Max; iType++ )
    {
        if( EQUAL(GDALGetColorInterpretationName((GDALColorInterp)iType), pszName) )
        {
            return (GDALColorInterp)iType;
        }
    }

    return GCI_Undefined;
}

/************************************************************************/
/*                         GDALDummyProgress()                          */
/************************************************************************/

/**
 * \brief Stub progress function.
 *
 * This is a stub (does nothing) implementation of the GDALProgressFunc()
 * semantics.  It is primarily useful for passing to functions that take
 * a GDALProgressFunc() argument but for which the application does not want
 * to use one of the other progress functions that actually do something.
 */

int CPL_STDCALL GDALDummyProgress( double dfComplete, const char *pszMessage, 
                                   void *pData )

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
 * \brief Scaled progress transformer.
 *
 * This is the progress function that should be passed along with the
 * callback data returned by GDALCreateScaledProgress().
 */

int CPL_STDCALL GDALScaledProgress( double dfComplete, const char *pszMessage, 
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
 * \brief Create scaled progress transformer.
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
 * @param pData the overall progress function callback data. 
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
 *       GDALDoLongSlowOperation( ..., GDALScaledProgress, pScaledProgress );
 *       GDALDestroyScaledProgress( pScaledProgress );
 *
 *       pScaledProgress = GDALCreateScaledProgress( 0.5, 1.0, pfnProgress, 
 *                                                   pProgressData );
 *       GDALDoAnotherOperation( ..., GDALScaledProgress, pScaledProgress );
 *       GDALDestroyScaledProgress( pScaledProgress );
 *
 *       return ...;
 *   }
 * \endcode
 */

void * CPL_STDCALL GDALCreateScaledProgress( double dfMin, double dfMax, 
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
 * \brief Cleanup scaled progress handle.
 *
 * This function cleans up the data associated with a scaled progress function
 * as returned by GADLCreateScaledProgress(). 
 *
 * @param pData scaled progress handle returned by GDALCreateScaledProgress().
 */

void CPL_STDCALL GDALDestroyScaledProgress( void * pData )

{
    CPLFree( pData );
}

/************************************************************************/
/*                          GDALTermProgress()                          */
/************************************************************************/

/**
 * \brief Simple progress report to terminal.
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
 * The GDALTermProgress() function maintains an internal memory of the 
 * last percentage complete reported in a static variable, and this makes
 * it unsuitable to have multiple GDALTermProgress()'s active eithin a 
 * single thread or across multiple threads.
 *
 * @param dfComplete completion ratio from 0.0 to 1.0.
 * @param pszMessage optional message.
 * @param pProgressArg ignored callback data argument. 
 *
 * @return Always returns TRUE indicating the process should continue.
 */

int CPL_STDCALL GDALTermProgress( double dfComplete, const char *pszMessage, 
                      void * pProgressArg )

{
    static int nLastTick = -1;
    int nThisTick = (int) (dfComplete * 40.0);

    (void) pProgressArg;

    nThisTick = MIN(40,MAX(0,nThisTick));

    // Have we started a new progress run?  
    if( nThisTick < nLastTick && nLastTick >= 39 )
        nLastTick = -1;

    if( nThisTick <= nLastTick )
        return TRUE;

    while( nThisTick > nLastTick )
    {
        nLastTick++;
        if( nLastTick % 4 == 0 )
            fprintf( stdout, "%d", (nLastTick / 4) * 10 );
        else
            fprintf( stdout, "." );
    }

    if( nThisTick == 40 )
        fprintf( stdout, " - done.\n" );
    else
        fflush( stdout );

    return TRUE;
}

/************************************************************************/
/*                     GDALGetRandomRasterSample()                      */
/************************************************************************/

int CPL_STDCALL 
GDALGetRandomRasterSample( GDALRasterBandH hBand, int nSamples, 
                           float *pafSampleBuf )

{
    VALIDATE_POINTER1( hBand, "GDALGetRandomRasterSample", 0 );

    GDALRasterBand *poBand;

    poBand = (GDALRasterBand *) GDALGetRasterSampleOverview( hBand, nSamples );
    CPLAssert( NULL != poBand );

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
                  "GDALGetRandomRasterSample(): returning because band"
                  " appears degenerate." );

        return FALSE;
    }

    nSampleRate = (int) MAX(1,sqrt((double) nBlockCount)-2.0);

    if( nSampleRate == nBlocksPerRow && nSampleRate > 1 )
        nSampleRate--;

    while( nSampleRate > 1 
           && ((nBlockCount-1) / nSampleRate + 1) * nBlockPixels < nSamples )
        nSampleRate--;

    if ((nSamples / ((nBlockCount-1) / nSampleRate + 1)) == 0)
        nBlockSampleRate = 1;
    else
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

        poBlock = poBand->GetLockedBlockRef( iXBlock, iYBlock );
        if( poBlock == NULL )
            continue;
        if( poBlock->GetDataRef() == NULL )
        {
            poBlock->DropLock();
            continue;
        }

        if( (iXBlock + 1) * nBlockXSize > poBand->GetXSize() )
            iXValid = poBand->GetXSize() - iXBlock * nBlockXSize;
        else
            iXValid = nBlockXSize;

        if( (iYBlock + 1) * nBlockYSize > poBand->GetYSize() )
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

        poBlock->DropLock();
    }

    return nActualSamples;
}

/************************************************************************/
/*                            GDALInitGCPs()                            */
/************************************************************************/

void CPL_STDCALL GDALInitGCPs( int nCount, GDAL_GCP * psGCP )

{
    if( nCount > 0 )
    {
        VALIDATE_POINTER0( psGCP, "GDALInitGCPs" );
    }

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

void CPL_STDCALL GDALDeinitGCPs( int nCount, GDAL_GCP * psGCP )

{
    if ( nCount > 0 )
    {
        VALIDATE_POINTER0( psGCP, "GDALDeinitGCPs" );
    }

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

GDAL_GCP * CPL_STDCALL 
GDALDuplicateGCPs( int nCount, const GDAL_GCP *pasGCPList )

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
/*                       GDALFindAssociatedFile()                       */
/************************************************************************/

/**
 * Find file with alternate extension.
 *
 * Finds the file with the indicated extension, substituting it in place
 * of the extension of the base filename.  Generally used to search for 
 * associated files like world files .RPB files, etc.  If necessary, the
 * extension will be tried in both upper and lower case.  If a sibling file
 * list is available it will be used instead of doing VSIStatExL() calls to 
 * probe the file system.  
 *
 * Note that the result is a dynamic CPLString so this method should not 
 * be used in a situation where there could be cross heap issues.  It is
 * generally imprudent for application built on GDAL to use this function
 * unless they are sure they will always use the same runtime heap as GDAL.
 *
 * @param pszBaseFilename the filename relative to which to search.
 * @param pszExt the target extension in either upper or lower case.
 * @param papszSiblingFiles the list of files in the same directory as 
 * pszBaseFilename or NULL if they are not known. 
 * @param nFlags special options controlling search.  None defined yet, just 
 * pass 0.
 * 
 * @return an empty string if the target is not found, otherwise the target
 * file with similar path style as the pszBaseFilename. 
 */

CPLString GDALFindAssociatedFile( const char *pszBaseFilename, 
                                  const char *pszExt,
                                  char **papszSiblingFiles, 
                                  int nFlags )

{
    (void) nFlags;

    CPLString osTarget = CPLResetExtension( pszBaseFilename, pszExt );

    if( papszSiblingFiles == NULL )
    {
        VSIStatBufL sStatBuf;

        if( VSIStatExL( osTarget, &sStatBuf, VSI_STAT_EXISTS_FLAG ) != 0 )
        {
            CPLString osAltExt = pszExt;

            if( islower( pszExt[0] ) )
                osAltExt.toupper();
            else
                osAltExt.tolower();

            osTarget = CPLResetExtension( pszBaseFilename, osAltExt );

            if( VSIStatExL( osTarget, &sStatBuf, VSI_STAT_EXISTS_FLAG ) != 0 )
                return "";
        }
    }
    else
    {
        int iSibling = CSLFindString( papszSiblingFiles, 
                                      CPLGetFilename(osTarget) );
        if( iSibling < 0 )
            return "";

        osTarget.resize(osTarget.size() - strlen(papszSiblingFiles[iSibling]));
        osTarget += papszSiblingFiles[iSibling];
    }

    return osTarget;
}
                             
/************************************************************************/
/*                         GDALLoadOziMapFile()                         */
/************************************************************************/

#define MAX_GCP 30
 
int CPL_STDCALL GDALLoadOziMapFile( const char *pszFilename,
                                    double *padfGeoTransform, char **ppszWKT, 
                                    int *pnGCPCount, GDAL_GCP **ppasGCPs )


{
    char	**papszLines;
    int		iLine, nLines=0;
    int	        nCoordinateCount = 0;
    GDAL_GCP    asGCPs[MAX_GCP];
    
    VALIDATE_POINTER1( pszFilename, "GDALLoadOziMapFile", FALSE );
    VALIDATE_POINTER1( padfGeoTransform, "GDALLoadOziMapFile", FALSE );
    VALIDATE_POINTER1( pnGCPCount, "GDALLoadOziMapFile", FALSE );
    VALIDATE_POINTER1( ppasGCPs, "GDALLoadOziMapFile", FALSE );

    papszLines = CSLLoad2( pszFilename, 1000, 200, NULL );

    if ( !papszLines )
        return FALSE;

    nLines = CSLCount( papszLines );

    // Check the OziExplorer Map file signature
    if ( nLines < 5
         || !EQUALN(papszLines[0], "OziExplorer Map Data File Version ", 34) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
        "GDALLoadOziMapFile(): file \"%s\" is not in OziExplorer Map format.",
                  pszFilename );
        CSLDestroy( papszLines );
        return FALSE;
    }

    OGRSpatialReference oSRS;
    OGRErr eErr = OGRERR_NONE;

    /* The Map Scale Factor has been introduced recently on the 6th line */
    /* and is a trick that is used to just change that line without changing */
    /* the rest of the MAP file but providing an imagery that is smaller or larger */
    /* so we have to correct the pixel/line values read in the .MAP file so they */
    /* match the actual imagery dimension. Well, this is a bad summary of what */
    /* is explained at http://tech.groups.yahoo.com/group/OziUsers-L/message/12484 */
    double dfMSF = 1;

    for ( iLine = 5; iLine < nLines; iLine++ )
    {
        if ( EQUALN(papszLines[iLine], "MSF,", 4) )
        {
            dfMSF = atof(papszLines[iLine] + 4);
            if (dfMSF <= 0.01) /* Suspicious values */
            {
                CPLDebug("OZI", "Suspicious MSF value : %s", papszLines[iLine]);
                dfMSF = 1;
            }
        }
    }

    eErr = oSRS.importFromOzi( papszLines );
    if ( eErr == OGRERR_NONE )
    {
        if ( ppszWKT != NULL )
            oSRS.exportToWkt( ppszWKT );
    }

    // Iterate all lines in the MAP-file
    for ( iLine = 5; iLine < nLines; iLine++ )
    {
        char    **papszTok = NULL;

        papszTok = CSLTokenizeString2( papszLines[iLine], ",",
                                       CSLT_ALLOWEMPTYTOKENS
                                       | CSLT_STRIPLEADSPACES
                                       | CSLT_STRIPENDSPACES );

        if ( CSLCount(papszTok) < 12 )
        {
            CSLDestroy(papszTok);
            continue;
        }

        if ( CSLCount(papszTok) >= 17
             && EQUALN(papszTok[0], "Point", 5)
             && !EQUAL(papszTok[2], "")
             && !EQUAL(papszTok[3], "")
             && nCoordinateCount < MAX_GCP )
        {
            int     bReadOk = FALSE;
            double  dfLon = 0., dfLat = 0.;

            if ( !EQUAL(papszTok[6], "")
                 && !EQUAL(papszTok[7], "")
                 && !EQUAL(papszTok[9], "")
                 && !EQUAL(papszTok[10], "") )
            {
                // Set geographical coordinates of the pixels
                dfLon = CPLAtofM(papszTok[9]) + CPLAtofM(papszTok[10]) / 60.0;
                dfLat = CPLAtofM(papszTok[6]) + CPLAtofM(papszTok[7]) / 60.0;
                if ( EQUAL(papszTok[11], "W") )
                    dfLon = -dfLon;
                if ( EQUAL(papszTok[8], "S") )
                    dfLat = -dfLat;

                // Transform from the geographical coordinates into projected
                // coordinates.
                if ( eErr == OGRERR_NONE )
                {
                    OGRSpatialReference *poLatLong = NULL;
                    OGRCoordinateTransformation *poTransform = NULL;

                    poLatLong = oSRS.CloneGeogCS();
                    if ( poLatLong )
                    {
                        poTransform = OGRCreateCoordinateTransformation( poLatLong, &oSRS );
                        if ( poTransform )
                        {
                            bReadOk = poTransform->Transform( 1, &dfLon, &dfLat );
                            delete poTransform;
                        }
                        delete poLatLong;
                    }
                }
            }
            else if ( !EQUAL(papszTok[14], "")
                      && !EQUAL(papszTok[15], "") )
            {
                // Set cartesian coordinates of the pixels.
                dfLon = CPLAtofM(papszTok[14]);
                dfLat = CPLAtofM(papszTok[15]);
                bReadOk = TRUE;

                //if ( EQUAL(papszTok[16], "S") )
                //    dfLat = -dfLat;
            }

            if ( bReadOk )
            {
                GDALInitGCPs( 1, asGCPs + nCoordinateCount );

                // Set pixel/line part
                asGCPs[nCoordinateCount].dfGCPPixel = CPLAtofM(papszTok[2]) / dfMSF;
                asGCPs[nCoordinateCount].dfGCPLine = CPLAtofM(papszTok[3]) / dfMSF;

                asGCPs[nCoordinateCount].dfGCPX = dfLon;
                asGCPs[nCoordinateCount].dfGCPY = dfLat;

                nCoordinateCount++;
            }
        }

        CSLDestroy( papszTok );
    }

    CSLDestroy( papszLines );

    if ( nCoordinateCount == 0 )
    {
        CPLDebug( "GDAL", "GDALLoadOziMapFile(\"%s\") did read no GCPs.", 
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to convert the GCPs into a geotransform definition, if      */
/*      possible.  Otherwise we will need to use them as GCPs.          */
/* -------------------------------------------------------------------- */
    if( !GDALGCPsToGeoTransform( nCoordinateCount, asGCPs, padfGeoTransform, 
                                 CSLTestBoolean(CPLGetConfigOption("OZI_APPROX_GEOTRANSFORM", "NO")) ) )
    {
        if ( pnGCPCount && ppasGCPs )
        {
            CPLDebug( "GDAL", 
                "GDALLoadOziMapFile(%s) found file, wasn't able to derive a\n"
                "first order geotransform.  Using points as GCPs.",
                pszFilename );

            *ppasGCPs = (GDAL_GCP *) 
                CPLCalloc( sizeof(GDAL_GCP),nCoordinateCount );
            memcpy( *ppasGCPs, asGCPs, sizeof(GDAL_GCP) * nCoordinateCount );
            *pnGCPCount = nCoordinateCount;
        }
    }
    else
    {
        GDALDeinitGCPs( nCoordinateCount, asGCPs );
    }
     
    return TRUE;
}

#undef MAX_GCP

/************************************************************************/
/*                       GDALReadOziMapFile()                           */
/************************************************************************/

int CPL_STDCALL GDALReadOziMapFile( const char * pszBaseFilename,
                                    double *padfGeoTransform, char **ppszWKT,
                                    int *pnGCPCount, GDAL_GCP **ppasGCPs )


{
    const char	*pszOzi;
    FILE	*fpOzi;

/* -------------------------------------------------------------------- */
/*      Try lower case, then upper case.                                */
/* -------------------------------------------------------------------- */
    pszOzi = CPLResetExtension( pszBaseFilename, "map" );

    fpOzi = VSIFOpen( pszOzi, "rt" );

    if ( fpOzi == NULL && VSIIsCaseSensitiveFS(pszOzi) )
    {
        pszOzi = CPLResetExtension( pszBaseFilename, "MAP" );
        fpOzi = VSIFOpen( pszOzi, "rt" );
    }
    
    if ( fpOzi == NULL )
        return FALSE;

    VSIFClose( fpOzi );

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    return GDALLoadOziMapFile( pszOzi, padfGeoTransform, ppszWKT,
                               pnGCPCount, ppasGCPs );
}

/************************************************************************/
/*                         GDALLoadTabFile()                            */
/*                                                                      */
/*      Helper function for translator implementators wanting           */
/*      support for MapInfo .tab-files.                                 */
/************************************************************************/

#define MAX_GCP 256
 
int CPL_STDCALL GDALLoadTabFile( const char *pszFilename,
                                 double *padfGeoTransform, char **ppszWKT, 
                                 int *pnGCPCount, GDAL_GCP **ppasGCPs )


{
    char	**papszLines;
    char        **papszTok=NULL;
    int	        bTypeRasterFound = FALSE;
    int		bInsideTableDef = FALSE;
    int		iLine, numLines=0;
    int	        nCoordinateCount = 0;
    GDAL_GCP    asGCPs[MAX_GCP];
    
    papszLines = CSLLoad2( pszFilename, 1000, 200, NULL );

    if ( !papszLines )
        return FALSE;

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
                 && CSLCount(papszTok) > 4
                 && EQUAL(papszTok[4], "Label") 
                 && nCoordinateCount < MAX_GCP )
        {
            GDALInitGCPs( 1, asGCPs + nCoordinateCount );
            
            asGCPs[nCoordinateCount].dfGCPPixel = CPLAtofM(papszTok[2]);
            asGCPs[nCoordinateCount].dfGCPLine = CPLAtofM(papszTok[3]);
            asGCPs[nCoordinateCount].dfGCPX = CPLAtofM(papszTok[0]);
            asGCPs[nCoordinateCount].dfGCPY = CPLAtofM(papszTok[1]);
            if( papszTok[5] != NULL )
            {
                CPLFree( asGCPs[nCoordinateCount].pszId );
                asGCPs[nCoordinateCount].pszId = CPLStrdup(papszTok[5]);
            }

            nCoordinateCount++;
        }
        else if( bTypeRasterFound && bInsideTableDef 
                 && EQUAL(papszTok[0],"CoordSys") 
                 && ppszWKT != NULL )
        {
            OGRSpatialReference oSRS;
            
            if( oSRS.importFromMICoordSys( papszLines[iLine] ) == OGRERR_NONE )
                oSRS.exportToWkt( ppszWKT );
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

    CSLDestroy(papszTok);
    CSLDestroy(papszLines);

    if( nCoordinateCount == 0 )
    {
        CPLDebug( "GDAL", "GDALLoadTabFile(%s) did not get any GCPs.", 
                  pszFilename );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try to convert the GCPs into a geotransform definition, if      */
/*      possible.  Otherwise we will need to use them as GCPs.          */
/* -------------------------------------------------------------------- */
    if( !GDALGCPsToGeoTransform( nCoordinateCount, asGCPs, padfGeoTransform, 
                                 FALSE ) )
    {
        if (pnGCPCount && ppasGCPs)
        {
            CPLDebug( "GDAL", 
                "GDALLoadTabFile(%s) found file, wasn't able to derive a\n"
                "first order geotransform.  Using points as GCPs.",
                pszFilename );

            *ppasGCPs = (GDAL_GCP *) 
                CPLCalloc( sizeof(GDAL_GCP),nCoordinateCount );
            memcpy( *ppasGCPs, asGCPs, sizeof(GDAL_GCP) * nCoordinateCount );
            *pnGCPCount = nCoordinateCount;
        }
    }
    else
    {
        GDALDeinitGCPs( nCoordinateCount, asGCPs );
    }
     
    return TRUE;
}

#undef MAX_GCP

/************************************************************************/
/*                         GDALReadTabFile()                            */
/*                                                                      */
/*      Helper function for translator implementators wanting           */
/*      support for MapInfo .tab-files.                                 */
/************************************************************************/

int CPL_STDCALL GDALReadTabFile( const char * pszBaseFilename, 
                                 double *padfGeoTransform, char **ppszWKT, 
                                 int *pnGCPCount, GDAL_GCP **ppasGCPs )


{
    return GDALReadTabFile2(pszBaseFilename, padfGeoTransform,
                            ppszWKT, pnGCPCount, ppasGCPs,
                            NULL, NULL);
}


int GDALReadTabFile2( const char * pszBaseFilename,
                      double *padfGeoTransform, char **ppszWKT,
                      int *pnGCPCount, GDAL_GCP **ppasGCPs,
                      char** papszSiblingFiles, char** ppszTabFileNameOut )
{
    const char	*pszTAB;
    VSILFILE	*fpTAB;

    if (ppszTabFileNameOut)
        *ppszTabFileNameOut = NULL;

    pszTAB = CPLResetExtension( pszBaseFilename, "tab" );

    if (papszSiblingFiles)
    {
        int iSibling = CSLFindString(papszSiblingFiles, CPLGetFilename(pszTAB));
        if (iSibling >= 0)
        {
            CPLString osTabFilename = pszBaseFilename;
            osTabFilename.resize(strlen(pszBaseFilename) -
                                 strlen(CPLGetFilename(pszBaseFilename)));
            osTabFilename += papszSiblingFiles[iSibling];
            if ( GDALLoadTabFile(osTabFilename, padfGeoTransform, ppszWKT,
                                 pnGCPCount, ppasGCPs ) )
            {
                if (ppszTabFileNameOut)
                    *ppszTabFileNameOut = CPLStrdup(osTabFilename);
                return TRUE;
            }
        }
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try lower case, then upper case.                                */
/* -------------------------------------------------------------------- */

    fpTAB = VSIFOpenL( pszTAB, "rt" );

    if( fpTAB == NULL && VSIIsCaseSensitiveFS(pszTAB) )
    {
        pszTAB = CPLResetExtension( pszBaseFilename, "TAB" );
        fpTAB = VSIFOpenL( pszTAB, "rt" );
    }
    
    if( fpTAB == NULL )
        return FALSE;

    VSIFCloseL( fpTAB );

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    if (GDALLoadTabFile( pszTAB, padfGeoTransform, ppszWKT,
                         pnGCPCount, ppasGCPs ) )
    {
        if (ppszTabFileNameOut)
            *ppszTabFileNameOut = CPLStrdup(pszTAB);
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                         GDALLoadWorldFile()                          */
/************************************************************************/

/**
 * \brief Read ESRI world file. 
 *
 * This function reads an ESRI style world file, and formats a geotransform
 * from its contents.
 *
 * The world file contains an affine transformation with the parameters
 * in a different order than in a geotransform array. 
 *
 * <ul>
 * <li> geotransform[1] : width of pixel
 * <li> geotransform[4] : rotational coefficient, zero for north up images.
 * <li> geotransform[2] : rotational coefficient, zero for north up images.
 * <li> geotransform[5] : height of pixel (but negative)
 * <li> geotransform[0] + 0.5 * geotransform[1] + 0.5 * geotransform[2] : x offset to center of top left pixel.
 * <li> geotransform[3] + 0.5 * geotransform[4] + 0.5 * geotransform[5] : y offset to center of top left pixel.
 * </ul>
 *
 * @param pszFilename the world file name.
 * @param padfGeoTransform the six double array into which the
 * geotransformation should be placed. 
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPL_STDCALL 
GDALLoadWorldFile( const char *pszFilename, double *padfGeoTransform )

{
    char        **papszLines;

    VALIDATE_POINTER1( pszFilename, "GDALLoadWorldFile", FALSE );
    VALIDATE_POINTER1( padfGeoTransform, "GDALLoadWorldFile", FALSE );

    papszLines = CSLLoad2( pszFilename, 100, 100, NULL );

    if ( !papszLines )
        return FALSE;

   double world[6];
    // reads the first 6 non-empty lines
    int nLines = 0;
    int nLinesCount = CSLCount(papszLines);
    for( int i = 0; i < nLinesCount && nLines < 6; ++i )
    {
        CPLString line(papszLines[i]);
        if( line.Trim().empty() )
          continue;

        world[nLines] = CPLAtofM(line);
        ++nLines;
    }
      
    if( nLines == 6 
        && (world[0] != 0.0 || world[2] != 0.0)
        && (world[3] != 0.0 || world[1] != 0.0) )
    {
        padfGeoTransform[0] = world[4];
        padfGeoTransform[1] = world[0];
        padfGeoTransform[2] = world[2];
        padfGeoTransform[3] = world[5];
        padfGeoTransform[4] = world[1];
        padfGeoTransform[5] = world[3];

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
                  "GDALLoadWorldFile(%s) found file, but it was corrupt.",
                  pszFilename );
        CSLDestroy(papszLines);
        return FALSE;
    }
}

/************************************************************************/
/*                         GDALReadWorldFile()                          */
/************************************************************************/

/**
 * \brief Read ESRI world file. 
 *
 * This function reads an ESRI style world file, and formats a geotransform
 * from its contents.  It does the same as GDALLoadWorldFile() function, but
 * it will form the filename for the worldfile from the filename of the raster
 * file referred and the suggested extension.  If no extension is provided,
 * the code will internally try the unix style and windows style world file
 * extensions (eg. for .tif these would be .tfw and .tifw). 
 *
 * The world file contains an affine transformation with the parameters
 * in a different order than in a geotransform array. 
 *
 * <ul>
 * <li> geotransform[1] : width of pixel
 * <li> geotransform[4] : rotational coefficient, zero for north up images.
 * <li> geotransform[2] : rotational coefficient, zero for north up images.
 * <li> geotransform[5] : height of pixel (but negative)
 * <li> geotransform[0] + 0.5 * geotransform[1] + 0.5 * geotransform[2] : x offset to center of top left pixel.
 * <li> geotransform[3] + 0.5 * geotransform[4] + 0.5 * geotransform[5] : y offset to center of top left pixel.
 * </ul>
 *
 * @param pszBaseFilename the target raster file.
 * @param pszExtension the extension to use (ie. ".wld") or NULL to derive it
 * from the pszBaseFilename
 * @param padfGeoTransform the six double array into which the 
 * geotransformation should be placed. 
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPL_STDCALL 
GDALReadWorldFile( const char *pszBaseFilename, const char *pszExtension,
                   double *padfGeoTransform )

{
    return GDALReadWorldFile2(pszBaseFilename, pszExtension,
                              padfGeoTransform, NULL, NULL);
}

int GDALReadWorldFile2( const char *pszBaseFilename, const char *pszExtension,
                        double *padfGeoTransform, char** papszSiblingFiles,
                        char** ppszWorldFileNameOut )
{
    const char  *pszTFW;
    char        szExtUpper[32], szExtLower[32];
    int         i;

    VALIDATE_POINTER1( pszBaseFilename, "GDALReadWorldFile", FALSE );
    VALIDATE_POINTER1( padfGeoTransform, "GDALReadWorldFile", FALSE );

    if (ppszWorldFileNameOut)
        *ppszWorldFileNameOut = NULL;

/* -------------------------------------------------------------------- */
/*      If we aren't given an extension, try both the unix and          */
/*      windows style extensions.                                       */
/* -------------------------------------------------------------------- */
    if( pszExtension == NULL )
    {
        char szDerivedExtension[100];
        std::string  oBaseExt = CPLGetExtension( pszBaseFilename );

        if( oBaseExt.length() < 2 )
            return FALSE;

        // windows version - first + last + 'w'
        szDerivedExtension[0] = oBaseExt[0];
        szDerivedExtension[1] = oBaseExt[oBaseExt.length()-1];
        szDerivedExtension[2] = 'w';
        szDerivedExtension[3] = '\0';
        
        if( GDALReadWorldFile2( pszBaseFilename, szDerivedExtension,
                                padfGeoTransform, papszSiblingFiles,
                                ppszWorldFileNameOut ) )
            return TRUE;

        // unix version - extension + 'w'
        if( oBaseExt.length() > sizeof(szDerivedExtension)-2 )
            return FALSE;

        strcpy( szDerivedExtension, oBaseExt.c_str() );
        strcat( szDerivedExtension, "w" );
        return GDALReadWorldFile2( pszBaseFilename, szDerivedExtension,
                                  padfGeoTransform, papszSiblingFiles,
                                  ppszWorldFileNameOut );
    }

/* -------------------------------------------------------------------- */
/*      Skip the leading period in the extension if there is one.       */
/* -------------------------------------------------------------------- */
    if( *pszExtension == '.' )
        pszExtension++;

/* -------------------------------------------------------------------- */
/*      Generate upper and lower case versions of the extension.        */
/* -------------------------------------------------------------------- */
    CPLStrlcpy( szExtUpper, pszExtension, sizeof(szExtUpper) );
    CPLStrlcpy( szExtLower, pszExtension, sizeof(szExtLower) );

    for( i = 0; szExtUpper[i] != '\0'; i++ )
    {
        szExtUpper[i] = (char) toupper(szExtUpper[i]);
        szExtLower[i] = (char) tolower(szExtLower[i]);
    }

    VSIStatBufL sStatBuf;
    int bGotTFW;

    pszTFW = CPLResetExtension( pszBaseFilename, szExtLower );

    if (papszSiblingFiles)
    {
        int iSibling = CSLFindString(papszSiblingFiles, CPLGetFilename(pszTFW));
        if (iSibling >= 0)
        {
            CPLString osTFWFilename = pszBaseFilename;
            osTFWFilename.resize(strlen(pszBaseFilename) -
                                 strlen(CPLGetFilename(pszBaseFilename)));
            osTFWFilename += papszSiblingFiles[iSibling];
            if (GDALLoadWorldFile( osTFWFilename, padfGeoTransform ))
            {
                if (ppszWorldFileNameOut)
                    *ppszWorldFileNameOut = CPLStrdup(osTFWFilename);
                return TRUE;
            }
        }
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Try lower case, then upper case.                                */
/* -------------------------------------------------------------------- */

    bGotTFW = VSIStatExL( pszTFW, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0;

    if( !bGotTFW  && VSIIsCaseSensitiveFS(pszTFW) )
    {
        pszTFW = CPLResetExtension( pszBaseFilename, szExtUpper );
        bGotTFW = VSIStatExL( pszTFW, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0;
    }
    
    if( !bGotTFW )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    if (GDALLoadWorldFile( pszTFW, padfGeoTransform ))
    {
        if (ppszWorldFileNameOut)
            *ppszWorldFileNameOut = CPLStrdup(pszTFW);
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                         GDALWriteWorldFile()                          */
/*                                                                      */
/*      Helper function for translator implementators wanting           */
/*      support for ESRI world files.                                   */
/************************************************************************/

/**
 * \brief Write ESRI world file. 
 *
 * This function writes an ESRI style world file from the passed geotransform.
 *
 * The world file contains an affine transformation with the parameters
 * in a different order than in a geotransform array. 
 *
 * <ul>
 * <li> geotransform[1] : width of pixel
 * <li> geotransform[4] : rotational coefficient, zero for north up images.
 * <li> geotransform[2] : rotational coefficient, zero for north up images.
 * <li> geotransform[5] : height of pixel (but negative)
 * <li> geotransform[0] + 0.5 * geotransform[1] + 0.5 * geotransform[2] : x offset to center of top left pixel.
 * <li> geotransform[3] + 0.5 * geotransform[4] + 0.5 * geotransform[5] : y offset to center of top left pixel.
 * </ul>
 *
 * @param pszBaseFilename the target raster file.
 * @param pszExtension the extension to use (ie. ".wld"). Must not be NULL
 * @param padfGeoTransform the six double array from which the 
 * geotransformation should be read. 
 *
 * @return TRUE on success or FALSE on failure.
 */

int CPL_STDCALL 
GDALWriteWorldFile( const char * pszBaseFilename, const char *pszExtension,
                    double *padfGeoTransform )

{
    VALIDATE_POINTER1( pszBaseFilename, "GDALWriteWorldFile", FALSE );
    VALIDATE_POINTER1( pszExtension, "GDALWriteWorldFile", FALSE );
    VALIDATE_POINTER1( padfGeoTransform, "GDALWriteWorldFile", FALSE );

/* -------------------------------------------------------------------- */
/*      Prepare the text to write to the file.                          */
/* -------------------------------------------------------------------- */
    CPLString osTFWText;

    osTFWText.Printf( "%.10f\n%.10f\n%.10f\n%.10f\n%.10f\n%.10f\n", 
                      padfGeoTransform[1],
                      padfGeoTransform[4],
                      padfGeoTransform[2],
                      padfGeoTransform[5],
                      padfGeoTransform[0] 
                      + 0.5 * padfGeoTransform[1]
                      + 0.5 * padfGeoTransform[2],
                      padfGeoTransform[3]
                      + 0.5 * padfGeoTransform[4]
                      + 0.5 * padfGeoTransform[5] );

/* -------------------------------------------------------------------- */
/*      Update extention, and write to disk.                            */
/* -------------------------------------------------------------------- */
    const char  *pszTFW;
    VSILFILE    *fpTFW;

    pszTFW = CPLResetExtension( pszBaseFilename, pszExtension );
    fpTFW = VSIFOpenL( pszTFW, "wt" );
    if( fpTFW == NULL )
        return FALSE;

    VSIFWriteL( (void *) osTFWText.c_str(), 1, osTFWText.size(), fpTFW );
    VSIFCloseL( fpTFW );

    return TRUE;
}

/************************************************************************/
/*                          GDALVersionInfo()                           */
/************************************************************************/

/**
 * \brief Get runtime version information.
 *
 * Available pszRequest values:
 * <ul>
 * <li> "VERSION_NUM": Returns GDAL_VERSION_NUM formatted as a string.  ie. "1170"
 *      Note: starting with GDAL 1.10, this string will be longer than 4 characters.
 * <li> "RELEASE_DATE": Returns GDAL_RELEASE_DATE formatted as a string.  
 * ie. "20020416".
 * <li> "RELEASE_NAME": Returns the GDAL_RELEASE_NAME. ie. "1.1.7"
 * <li> "--version": Returns one line version message suitable for use in 
 * response to --version requests.  ie. "GDAL 1.1.7, released 2002/04/16"
 * <li> "LICENCE": Returns the content of the LICENSE.TXT file from the GDAL_DATA directory.
 *      Before GDAL 1.7.0, the returned string was leaking memory but this is now resolved.
 *      So the result should not been freed by the caller.
 * <li> "BUILD_INFO": List of NAME=VALUE pairs separated by newlines with 
 * information on build time options.
 * </ul>
 *
 * @param pszRequest the type of version info desired, as listed above.
 *
 * @return an internal string containing the requested information.
 */

const char * CPL_STDCALL GDALVersionInfo( const char *pszRequest )

{
/* -------------------------------------------------------------------- */
/*      Try to capture as much build information as practical.          */
/* -------------------------------------------------------------------- */
    if( pszRequest != NULL && EQUAL(pszRequest,"BUILD_INFO") ) 
    {
        CPLString osBuildInfo;

#ifdef ESRI_BUILD
        osBuildInfo += "ESRI_BUILD=YES\n";
#endif
#ifdef PAM_ENABLED
        osBuildInfo += "PAM_ENABLED=YES\n";
#endif
#ifdef OGR_ENABLED
        osBuildInfo += "OGR_ENABLED=YES\n";
#endif

        CPLFree(CPLGetTLS(CTLS_VERSIONINFO));
        CPLSetTLS(CTLS_VERSIONINFO, CPLStrdup(osBuildInfo), TRUE );
        return (char *) CPLGetTLS(CTLS_VERSIONINFO);
    }

/* -------------------------------------------------------------------- */
/*      LICENSE is a special case. We try to find and read the          */
/*      LICENSE.TXT file from the GDAL_DATA directory and return it     */
/* -------------------------------------------------------------------- */
    if( pszRequest != NULL && EQUAL(pszRequest,"LICENSE") )
    {
        char* pszResultLicence = (char*) CPLGetTLS( CTLS_VERSIONINFO_LICENCE );
        if( pszResultLicence != NULL )
        {
            return pszResultLicence;
        }

        const char *pszFilename = CPLFindFile( "etc", "LICENSE.TXT" );
        VSILFILE *fp = NULL;
        int  nLength;

        if( pszFilename != NULL )
            fp = VSIFOpenL( pszFilename, "r" );

        if( fp != NULL )
        {
            VSIFSeekL( fp, 0, SEEK_END );
            nLength = (int) VSIFTellL( fp ) + 1;
            VSIFSeekL( fp, SEEK_SET, 0 );

            pszResultLicence = (char *) VSICalloc(1,nLength);
            if (pszResultLicence)
                VSIFReadL( pszResultLicence, 1, nLength-1, fp );

            VSIFCloseL( fp );
        }

        if (!pszResultLicence)
        {
            pszResultLicence = CPLStrdup(
                     "GDAL/OGR is released under the MIT/X license.\n"
                     "The LICENSE.TXT distributed with GDAL/OGR should\n"
                     "contain additional details.\n" );
        }

        CPLSetTLS( CTLS_VERSIONINFO_LICENCE, pszResultLicence, TRUE );
        return pszResultLicence;
    }

/* -------------------------------------------------------------------- */
/*      All other strings are fairly small.                             */
/* -------------------------------------------------------------------- */
    CPLString osVersionInfo;

    if( pszRequest == NULL || EQUAL(pszRequest,"VERSION_NUM") )
        osVersionInfo.Printf( "%d", GDAL_VERSION_NUM );
    else if( EQUAL(pszRequest,"RELEASE_DATE") )
        osVersionInfo.Printf( "%d", GDAL_RELEASE_DATE );
    else if( EQUAL(pszRequest,"RELEASE_NAME") )
        osVersionInfo.Printf( GDAL_RELEASE_NAME );
    else // --version
        osVersionInfo.Printf( "GDAL %s, released %d/%02d/%02d",
                              GDAL_RELEASE_NAME, 
                              GDAL_RELEASE_DATE / 10000, 
                              (GDAL_RELEASE_DATE % 10000) / 100,
                              GDAL_RELEASE_DATE % 100 );

    CPLFree(CPLGetTLS(CTLS_VERSIONINFO)); // clear old value.
    CPLSetTLS(CTLS_VERSIONINFO, CPLStrdup(osVersionInfo), TRUE ); 
    return (char *) CPLGetTLS(CTLS_VERSIONINFO);
}

/************************************************************************/
/*                         GDALCheckVersion()                           */
/************************************************************************/

/** Return TRUE if GDAL library version at runtime matches nVersionMajor.nVersionMinor.

    The purpose of this method is to ensure that calling code will run with the GDAL
    version it is compiled for. It is primarly intented for external plugins.

    @param nVersionMajor Major version to be tested against
    @param nVersionMinor Minor version to be tested against
    @param pszCallingComponentName If not NULL, in case of version mismatch, the method
                                   will issue a failure mentionning the name of
                                   the calling component.

    @return TRUE if GDAL library version at runtime matches nVersionMajor.nVersionMinor, FALSE otherwise.
  */
int CPL_STDCALL GDALCheckVersion( int nVersionMajor, int nVersionMinor,
                                  const char* pszCallingComponentName)
{
    if (nVersionMajor == GDAL_VERSION_MAJOR &&
        nVersionMinor == GDAL_VERSION_MINOR)
        return TRUE;

    if (pszCallingComponentName)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s was compiled against GDAL %d.%d but current library version is %d.%d\n",
                  pszCallingComponentName, nVersionMajor, nVersionMinor,
                  GDAL_VERSION_MAJOR, GDAL_VERSION_MINOR);
    }
    return FALSE;
}

/************************************************************************/
/*                            GDALDecToDMS()                            */
/*                                                                      */
/*      Translate a decimal degrees value to a DMS string with          */
/*      hemisphere.                                                     */
/************************************************************************/

const char * CPL_STDCALL GDALDecToDMS( double dfAngle, const char * pszAxis,
                          int nPrecision )

{
    return CPLDecToDMS( dfAngle, pszAxis, nPrecision );
}

/************************************************************************/
/*                         GDALPackedDMSToDec()                         */
/************************************************************************/

/**
 * \brief Convert a packed DMS value (DDDMMMSSS.SS) into decimal degrees.
 *
 * See CPLPackedDMSToDec().
 */

double CPL_STDCALL GDALPackedDMSToDec( double dfPacked )

{
    return CPLPackedDMSToDec( dfPacked );
}

/************************************************************************/
/*                         GDALDecToPackedDMS()                         */
/************************************************************************/

/**
 * \brief Convert decimal degrees into packed DMS value (DDDMMMSSS.SS).
 *
 * See CPLDecToPackedDMS().
 */

double CPL_STDCALL GDALDecToPackedDMS( double dfDec )

{
    return CPLDecToPackedDMS( dfDec );
}

/************************************************************************/
/*                       GDALGCPsToGeoTransform()                       */
/************************************************************************/

/**
 * \brief Generate Geotransform from GCPs. 
 *
 * Given a set of GCPs perform first order fit as a geotransform. 
 *
 * Due to imprecision in the calculations the fit algorithm will often 
 * return non-zero rotational coefficients even if given perfectly non-rotated
 * inputs.  A special case has been implemented for corner corner coordinates
 * given in TL, TR, BR, BL order.  So when using this to get a geotransform
 * from 4 corner coordinates, pass them in this order. 
 * 
 * @param nGCPCount the number of GCPs being passed in.
 * @param pasGCPs the list of GCP structures. 
 * @param padfGeoTransform the six double array in which the affine 
 * geotransformation will be returned. 
 * @param bApproxOK If FALSE the function will fail if the geotransform is not 
 * essentially an exact fit (within 0.25 pixel) for all GCPs. 
 * 
 * @return TRUE on success or FALSE if there aren't enough points to prepare a
 * geotransform, the pointers are ill-determined or if bApproxOK is FALSE 
 * and the fit is poor.
 */

int CPL_STDCALL 
GDALGCPsToGeoTransform( int nGCPCount, const GDAL_GCP *pasGCPs,
                        double *padfGeoTransform, int bApproxOK )

{
    int    i;

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
/*      Special case of 4 corner coordinates of a non-rotated           */
/*      image.  The points must be in TL-TR-BR-BL order for now.        */
/*      This case helps avoid some imprecision in the general           */
/*      calcuations.                                                    */
/* -------------------------------------------------------------------- */
    if( nGCPCount == 4 
        && pasGCPs[0].dfGCPLine == pasGCPs[1].dfGCPLine
        && pasGCPs[2].dfGCPLine == pasGCPs[3].dfGCPLine
        && pasGCPs[0].dfGCPPixel == pasGCPs[3].dfGCPPixel
        && pasGCPs[1].dfGCPPixel == pasGCPs[2].dfGCPPixel
        && pasGCPs[0].dfGCPLine != pasGCPs[2].dfGCPLine
        && pasGCPs[0].dfGCPPixel != pasGCPs[1].dfGCPPixel 
        && pasGCPs[0].dfGCPY == pasGCPs[1].dfGCPY
        && pasGCPs[2].dfGCPY == pasGCPs[3].dfGCPY
        && pasGCPs[0].dfGCPX == pasGCPs[3].dfGCPX
        && pasGCPs[1].dfGCPX == pasGCPs[2].dfGCPX
        && pasGCPs[0].dfGCPY != pasGCPs[2].dfGCPY
        && pasGCPs[0].dfGCPX != pasGCPs[1].dfGCPX )
    {
        padfGeoTransform[1] = (pasGCPs[1].dfGCPX - pasGCPs[0].dfGCPX)
            / (pasGCPs[1].dfGCPPixel - pasGCPs[0].dfGCPPixel);
        padfGeoTransform[2] = 0.0;
        padfGeoTransform[4] = 0.0;
        padfGeoTransform[5] = (pasGCPs[2].dfGCPY - pasGCPs[1].dfGCPY)
            / (pasGCPs[2].dfGCPLine - pasGCPs[1].dfGCPLine);

        padfGeoTransform[0] = 
            pasGCPs[0].dfGCPX - pasGCPs[0].dfGCPPixel * padfGeoTransform[1];
        padfGeoTransform[3] = 
            pasGCPs[0].dfGCPY - pasGCPs[0].dfGCPLine * padfGeoTransform[5];
        return TRUE;
    }

/* -------------------------------------------------------------------- */
/* In the general case, do a least squares error approximation by       */
/* solving the equation Sum[(A - B*x + C*y - Lon)^2] = minimum		*/
/* -------------------------------------------------------------------- */
	
    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0, sum_yy = 0.0;
    double sum_Lon = 0.0, sum_Lonx = 0.0, sum_Lony = 0.0;
    double sum_Lat = 0.0, sum_Latx = 0.0, sum_Laty = 0.0;
    double divisor;

    for (i = 0; i < nGCPCount; ++i) {
        sum_x += pasGCPs[i].dfGCPPixel;
        sum_y += pasGCPs[i].dfGCPLine;
        sum_xy += pasGCPs[i].dfGCPPixel * pasGCPs[i].dfGCPLine;
        sum_xx += pasGCPs[i].dfGCPPixel * pasGCPs[i].dfGCPPixel;
        sum_yy += pasGCPs[i].dfGCPLine * pasGCPs[i].dfGCPLine;
        sum_Lon += pasGCPs[i].dfGCPX;
        sum_Lonx += pasGCPs[i].dfGCPX * pasGCPs[i].dfGCPPixel;
        sum_Lony += pasGCPs[i].dfGCPX * pasGCPs[i].dfGCPLine;
        sum_Lat += pasGCPs[i].dfGCPY;
        sum_Latx += pasGCPs[i].dfGCPY * pasGCPs[i].dfGCPPixel;
        sum_Laty += pasGCPs[i].dfGCPY * pasGCPs[i].dfGCPLine;
    }

    divisor = nGCPCount * (sum_xx * sum_yy - sum_xy * sum_xy)
        + 2 * sum_x * sum_y * sum_xy - sum_y * sum_y * sum_xx
        - sum_x * sum_x * sum_yy;

/* -------------------------------------------------------------------- */
/*      If the divisor is zero, there is no valid solution.             */
/* -------------------------------------------------------------------- */
    if (divisor == 0.0)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Compute top/left origin.                                        */
/* -------------------------------------------------------------------- */

    padfGeoTransform[0] = (sum_Lon * (sum_xx * sum_yy - sum_xy * sum_xy)
                           + sum_Lonx * (sum_y * sum_xy - sum_x *  sum_yy)
                           + sum_Lony * (sum_x * sum_xy - sum_y * sum_xx))
        / divisor;

    padfGeoTransform[3] = (sum_Lat * (sum_xx * sum_yy - sum_xy * sum_xy)
                           + sum_Latx * (sum_y * sum_xy - sum_x *  sum_yy)
                           + sum_Laty * (sum_x * sum_xy - sum_y * sum_xx)) 
        / divisor;

/* -------------------------------------------------------------------- */
/*      Compute X related coefficients.                                 */
/* -------------------------------------------------------------------- */
    padfGeoTransform[1] = (sum_Lon * (sum_y * sum_xy - sum_x * sum_yy)
                           + sum_Lonx * (nGCPCount * sum_yy - sum_y * sum_y)
                           + sum_Lony * (sum_x * sum_y - sum_xy * nGCPCount))
        / divisor;

    padfGeoTransform[2] = (sum_Lon * (sum_x * sum_xy - sum_y * sum_xx)
                           + sum_Lonx * (sum_x * sum_y - nGCPCount * sum_xy)
                           + sum_Lony * (nGCPCount * sum_xx - sum_x * sum_x))
        / divisor;

/* -------------------------------------------------------------------- */
/*      Compute Y related coefficients.                                 */
/* -------------------------------------------------------------------- */
    padfGeoTransform[4] = (sum_Lat * (sum_y * sum_xy - sum_x * sum_yy)
                           + sum_Latx * (nGCPCount * sum_yy - sum_y * sum_y)
                           + sum_Laty * (sum_x * sum_y - sum_xy * nGCPCount))
        / divisor;

    padfGeoTransform[5] = (sum_Lat * (sum_x * sum_xy - sum_y * sum_xx)
                           + sum_Latx * (sum_x * sum_y - nGCPCount * sum_xy)
                           + sum_Laty * (nGCPCount * sum_xx - sum_x * sum_x))
        / divisor;

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
/*                    GDALGeneralCmdLineProcessor()                     */
/************************************************************************/

/**
 * \brief General utility option processing.
 *
 * This function is intended to provide a variety of generic commandline 
 * options for all GDAL commandline utilities.  It takes care of the following
 * commandline options:
 *  
 *  --version: report version of GDAL in use. 
 *  --build: report build info about GDAL in use.
 *  --license: report GDAL license info.
 *  --formats: report all format drivers configured.
 *  --format [format]: report details of one format driver. 
 *  --optfile filename: expand an option file into the argument list. 
 *  --config key value: set system configuration option. 
 *  --debug [on/off/value]: set debug level.
 *  --mempreload dir: preload directory contents into /vsimem
 *  --pause: Pause for user input (allows time to attach debugger)
 *  --locale [locale]: Install a locale using setlocale() (debugging)
 *  --help-general: report detailed help on general options. 
 *
 * The argument array is replaced "in place" and should be freed with 
 * CSLDestroy() when no longer needed.  The typical usage looks something
 * like the following.  Note that the formats should be registered so that
 * the --formats and --format options will work properly.
 *
 *  int main( int argc, char ** argv )
 *  { 
 *    GDALAllRegister();
 *
 *    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
 *    if( argc < 1 )
 *        exit( -argc );
 *
 * @param nArgc number of values in the argument list.
 * @param ppapszArgv pointer to the argument list array (will be updated in place).
 * @param nOptions unused for now.
 *
 * @return updated nArgc argument count.  Return of 0 requests terminate 
 * without error, return of -1 requests exit with error code.
 */

int CPL_STDCALL 
GDALGeneralCmdLineProcessor( int nArgc, char ***ppapszArgv, int nOptions )

{
    char **papszReturn = NULL;
    int  iArg;
    char **papszArgv = *ppapszArgv;

    (void) nOptions;
    
/* -------------------------------------------------------------------- */
/*      Preserve the program name.                                      */
/* -------------------------------------------------------------------- */
    papszReturn = CSLAddString( papszReturn, papszArgv[0] );

/* ==================================================================== */
/*      Loop over all arguments.                                        */
/* ==================================================================== */
    for( iArg = 1; iArg < nArgc; iArg++ )
    {
/* -------------------------------------------------------------------- */
/*      --version                                                       */
/* -------------------------------------------------------------------- */
        if( EQUAL(papszArgv[iArg],"--version") )
        {
            printf( "%s\n", GDALVersionInfo( "--version" ) );
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --build                                                         */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--build") )
        {
            printf( "%s", GDALVersionInfo( "BUILD_INFO" ) );
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --license                                                       */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--license") )
        {
            printf( "%s\n", GDALVersionInfo( "LICENSE" ) );
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --config                                                        */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--config") )
        {
            if( iArg + 2 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--config option given without a key and value argument." );
                CSLDestroy( papszReturn );
                return -1;
            }

            CPLSetConfigOption( papszArgv[iArg+1], papszArgv[iArg+2] );

            iArg += 2;
        }

/* -------------------------------------------------------------------- */
/*      --mempreload                                                    */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--mempreload") )
        {
            int i;

            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--mempreload option given without directory path.");
                CSLDestroy( papszReturn );
                return -1;
            }
            
            char **papszFiles = CPLReadDir( papszArgv[iArg+1] );
            if( CSLCount(papszFiles) == 0 )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--mempreload given invalid or empty directory.");
                CSLDestroy( papszReturn );
                return -1;
            }
                
            for( i = 0; papszFiles[i] != NULL; i++ )
            {
                CPLString osOldPath, osNewPath;
                VSIStatBufL sStatBuf;
                
                if( EQUAL(papszFiles[i],".") || EQUAL(papszFiles[i],"..") )
                    continue;

                osOldPath = CPLFormFilename( papszArgv[iArg+1], 
                                             papszFiles[i], NULL );
                osNewPath.Printf( "/vsimem/%s", papszFiles[i] );

                if( VSIStatL( osOldPath, &sStatBuf ) != 0
                    || VSI_ISDIR( sStatBuf.st_mode ) )
                {
                    CPLDebug( "VSI", "Skipping preload of %s.", 
                              osOldPath.c_str() );
                    continue;
                }

                CPLDebug( "VSI", "Preloading %s to %s.", 
                          osOldPath.c_str(), osNewPath.c_str() );

                if( CPLCopyFile( osNewPath, osOldPath ) != 0 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "Failed to copy %s to /vsimem", 
                              osOldPath.c_str() );
                    return -1;
                }
            }
            
            CSLDestroy( papszFiles );
            iArg += 1;
        }

/* -------------------------------------------------------------------- */
/*      --debug                                                         */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--debug") )
        {
            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--debug option given without debug level." );
                CSLDestroy( papszReturn );
                return -1;
            }

            CPLSetConfigOption( "CPL_DEBUG", papszArgv[iArg+1] );
            iArg += 1;
        }

/* -------------------------------------------------------------------- */
/*      --optfile                                                       */
/*                                                                      */
/*      Annoyingly the options inserted by --optfile will *not* be      */
/*      processed properly if they are general options.                 */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--optfile") )
        {
            const char *pszLine;
            FILE *fpOptFile;

            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--optfile option given without filename." );
                CSLDestroy( papszReturn );
                return -1;
            }

            fpOptFile = VSIFOpen( papszArgv[iArg+1], "rb" );

            if( fpOptFile == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unable to open optfile '%s'.\n%s",
                          papszArgv[iArg+1], VSIStrerror( errno ) );
                CSLDestroy( papszReturn );
                return -1;
            }
            
            while( (pszLine = CPLReadLine( fpOptFile )) != NULL )
            {
                char **papszTokens;
                int i;

                if( pszLine[0] == '#' || strlen(pszLine) == 0 )
                    continue;

                papszTokens = CSLTokenizeString( pszLine );
                for( i = 0; papszTokens != NULL && papszTokens[i] != NULL; i++)
                    papszReturn = CSLAddString( papszReturn, papszTokens[i] );
                CSLDestroy( papszTokens );
            }

            VSIFClose( fpOptFile );
                
            iArg += 1;
        }

/* -------------------------------------------------------------------- */
/*      --formats                                                       */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg], "--formats") )
        {
            int iDr;

            printf( "Supported Formats:\n" );
            for( iDr = 0; iDr < GDALGetDriverCount(); iDr++ )
            {
                GDALDriverH hDriver = GDALGetDriver(iDr);
                const char *pszRWFlag, *pszVirtualIO;
                
                if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) )
                    pszRWFlag = "rw+";
                else if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, 
                                              NULL ) )
                    pszRWFlag = "rw";
                else
                    pszRWFlag = "ro";
                
                if( GDALGetMetadataItem( hDriver, GDAL_DCAP_VIRTUALIO, NULL) )
                    pszVirtualIO = "v";
                else
                    pszVirtualIO = "";

                printf( "  %s (%s%s): %s\n",
                        GDALGetDriverShortName( hDriver ),
                        pszRWFlag, pszVirtualIO,
                        GDALGetDriverLongName( hDriver ) );
            }

            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --format                                                        */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg], "--format") )
        {
            GDALDriverH hDriver;
            char **papszMD;

            CSLDestroy( papszReturn );

            if( iArg + 1 >= nArgc )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--format option given without a format code." );
                return -1;
            }

            hDriver = GDALGetDriverByName( papszArgv[iArg+1] );
            if( hDriver == NULL )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "--format option given with format '%s', but that format not\n"
                          "recognised.  Use the --formats option to get a list of available formats,\n"
                          "and use the short code (ie. GTiff or HFA) as the format identifier.\n", 
                          papszArgv[iArg+1] );
                return -1;
            }

            printf( "Format Details:\n" );
            printf( "  Short Name: %s\n", GDALGetDriverShortName( hDriver ) );
            printf( "  Long Name: %s\n", GDALGetDriverLongName( hDriver ) );

            papszMD = GDALGetMetadata( hDriver, NULL );

            if( CSLFetchNameValue( papszMD, GDAL_DMD_EXTENSION ) )
                printf( "  Extension: %s\n", 
                        CSLFetchNameValue( papszMD, GDAL_DMD_EXTENSION ) );
            if( CSLFetchNameValue( papszMD, GDAL_DMD_MIMETYPE ) )
                printf( "  Mime Type: %s\n", 
                        CSLFetchNameValue( papszMD, GDAL_DMD_MIMETYPE ) );
            if( CSLFetchNameValue( papszMD, GDAL_DMD_HELPTOPIC ) )
                printf( "  Help Topic: %s\n", 
                        CSLFetchNameValue( papszMD, GDAL_DMD_HELPTOPIC ) );
            
            if( CSLFetchBoolean( papszMD, GDAL_DCAP_CREATE, FALSE ) )
                printf( "  Supports: Create() - Create writeable dataset.\n" );
            if( CSLFetchBoolean( papszMD, GDAL_DCAP_CREATECOPY, FALSE ) )
                printf( "  Supports: CreateCopy() - Create dataset by copying another.\n" );
            if( CSLFetchBoolean( papszMD, GDAL_DCAP_VIRTUALIO, FALSE ) )
                printf( "  Supports: Virtual IO - eg. /vsimem/\n" );
            if( CSLFetchNameValue( papszMD, GDAL_DMD_CREATIONDATATYPES ) )
                printf( "  Creation Datatypes: %s\n",
                        CSLFetchNameValue( papszMD, GDAL_DMD_CREATIONDATATYPES ) );
            if( CSLFetchNameValue( papszMD, GDAL_DMD_CREATIONOPTIONLIST ) )
            {
                CPLXMLNode *psCOL = 
                    CPLParseXMLString( 
                        CSLFetchNameValue( papszMD, 
                                           GDAL_DMD_CREATIONOPTIONLIST ) );
                char *pszFormattedXML = 
                    CPLSerializeXMLTree( psCOL );

                CPLDestroyXMLNode( psCOL );
                
                printf( "\n%s\n", pszFormattedXML );
                CPLFree( pszFormattedXML );
            }
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --help-general                                                  */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--help-general") )
        {
            printf( "Generic GDAL utility command options:\n" );
            printf( "  --version: report version of GDAL in use.\n" );
            printf( "  --license: report GDAL license info.\n" );
            printf( "  --formats: report all configured format drivers.\n" );
            printf( "  --format [format]: details of one format.\n" );
            printf( "  --optfile filename: expand an option file into the argument list.\n" );
            printf( "  --config key value: set system configuration option.\n" );
            printf( "  --debug [on/off/value]: set debug level.\n" );
            printf( "  --pause: wait for user input, time to attach debugger\n" );
            printf( "  --locale [locale]: install locale for debugging (ie. en_US.UTF-8)\n" );
            printf( "  --help-general: report detailed help on general options.\n" );
            CSLDestroy( papszReturn );
            return 0;
        }

/* -------------------------------------------------------------------- */
/*      --locale                                                        */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--locale") && iArg < nArgc-1 )
        {
            setlocale( LC_ALL, papszArgv[++iArg] );
        }

/* -------------------------------------------------------------------- */
/*      --pause                                                         */
/* -------------------------------------------------------------------- */
        else if( EQUAL(papszArgv[iArg],"--pause") )
        {
            printf( "Hit <ENTER> to Continue.\n" );
            CPLReadLine( stdin );
        }

/* -------------------------------------------------------------------- */
/*      carry through unrecognised options.                             */
/* -------------------------------------------------------------------- */
        else
        {
            papszReturn = CSLAddString( papszReturn, papszArgv[iArg] );
        }
    }

    *ppapszArgv = papszReturn;

    return CSLCount( *ppapszArgv );
}


/************************************************************************/
/*                          _FetchDblFromMD()                           */
/************************************************************************/

static int _FetchDblFromMD( char **papszMD, const char *pszKey, 
                            double *padfTarget, int nCount, double dfDefault )

{
    char szFullKey[200];

    sprintf( szFullKey, "%s", pszKey );

    const char *pszValue = CSLFetchNameValue( papszMD, szFullKey );
    int i;
    
    for( i = 0; i < nCount; i++ )
        padfTarget[i] = dfDefault;

    if( pszValue == NULL )
        return FALSE;

    if( nCount == 1 )
    {
        *padfTarget = CPLAtofM( pszValue );
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
        padfTarget[i] = CPLAtofM(papszTokens[i]);

    CSLDestroy( papszTokens );

    return TRUE;
}

/************************************************************************/
/*                         GDALExtractRPCInfo()                         */
/*                                                                      */
/*      Extract RPC info from metadata, and apply to an RPCInfo         */
/*      structure.  The inverse of this function is RPCInfoToMD() in    */
/*      alg/gdal_rpc.cpp (should it be needed).                         */
/************************************************************************/

int CPL_STDCALL GDALExtractRPCInfo( char **papszMD, GDALRPCInfo *psRPC )

{
    if( CSLFetchNameValue( papszMD, "LINE_NUM_COEFF" ) == NULL )
        return FALSE;

    if( CSLFetchNameValue( papszMD, "LINE_NUM_COEFF" ) == NULL 
        || CSLFetchNameValue( papszMD, "LINE_DEN_COEFF" ) == NULL 
        || CSLFetchNameValue( papszMD, "SAMP_NUM_COEFF" ) == NULL 
        || CSLFetchNameValue( papszMD, "SAMP_DEN_COEFF" ) == NULL )
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
/*                     GDALFindAssociatedAuxFile()                      */
/************************************************************************/

GDALDataset *GDALFindAssociatedAuxFile( const char *pszBasename,
                                        GDALAccess eAccess,
                                        GDALDataset *poDependentDS )

{
    const char *pszAuxSuffixLC = "aux";
    const char *pszAuxSuffixUC = "AUX";

    if( EQUAL(CPLGetExtension(pszBasename), pszAuxSuffixLC) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Don't even try to look for an .aux file if we don't have a      */
/*      path of any kind.                                               */
/* -------------------------------------------------------------------- */
    if( strlen(pszBasename) == 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We didn't find that, so try and find a corresponding aux        */
/*      file.  Check that we are the dependent file of the aux          */
/*      file, or if we aren't verify that the dependent file does       */
/*      not exist, likely mean it is us but some sort of renaming       */
/*      has occured.                                                    */
/* -------------------------------------------------------------------- */
    CPLString osJustFile = CPLGetFilename(pszBasename); // without dir
    CPLString osAuxFilename = CPLResetExtension(pszBasename, pszAuxSuffixLC);
    GDALDataset *poODS = NULL;
    GByte abyHeader[32];
    VSILFILE *fp;

    fp = VSIFOpenL( osAuxFilename, "rb" );


    if ( fp == NULL && VSIIsCaseSensitiveFS(osAuxFilename)) 
    {
        // Can't found file with lower case suffix. Try the upper case one.
        osAuxFilename = CPLResetExtension(pszBasename, pszAuxSuffixUC);
        fp = VSIFOpenL( osAuxFilename, "rb" );
    }

    if( fp != NULL )
    {
        if( VSIFReadL( abyHeader, 1, 32, fp ) == 32 &&
            EQUALN((char *) abyHeader,"EHFA_HEADER_TAG",15) )
        {
            /* Avoid causing failure in opening of main file from SWIG bindings */
            /* when auxiliary file cannot be opened (#3269) */
            CPLTurnFailureIntoWarning(TRUE);
            poODS = (GDALDataset *) GDALOpenShared( osAuxFilename, eAccess );
            CPLTurnFailureIntoWarning(FALSE);
        }
        VSIFCloseL( fp );
    }

/* -------------------------------------------------------------------- */
/*      Try replacing extension with .aux                               */
/* -------------------------------------------------------------------- */
    if( poODS != NULL )
    {
        const char *pszDep
            = poODS->GetMetadataItem( "HFA_DEPENDENT_FILE", "HFA" );
        if( pszDep == NULL  )
        {
            CPLDebug( "AUX", 
                      "Found %s but it has no dependent file, ignoring.",
                      osAuxFilename.c_str() );
            GDALClose( poODS );
            poODS = NULL;
        }
        else if( !EQUAL(pszDep,osJustFile) )
        {
            VSIStatBufL sStatBuf;

            if( VSIStatExL( pszDep, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0 )
            {
                CPLDebug( "AUX", "%s is for file %s, not %s, ignoring.",
                          osAuxFilename.c_str(), 
                          pszDep, osJustFile.c_str() );
                GDALClose( poODS );
                poODS = NULL;
            }
            else
            {
                CPLDebug( "AUX", "%s is for file %s, not %s, but since\n"
                          "%s does not exist, we will use .aux file as our own.",
                          osAuxFilename.c_str(), 
                          pszDep, osJustFile.c_str(),
                          pszDep );
            }
        }

/* -------------------------------------------------------------------- */
/*      Confirm that the aux file matches the configuration of the      */
/*      dependent dataset.                                              */
/* -------------------------------------------------------------------- */
        if( poODS != NULL && poDependentDS != NULL
            && (poODS->GetRasterCount() != poDependentDS->GetRasterCount()
                || poODS->GetRasterXSize() != poDependentDS->GetRasterXSize()
                || poODS->GetRasterYSize() != poDependentDS->GetRasterYSize()) )
        {
            CPLDebug( "AUX",
                      "Ignoring aux file %s as its raster configuration\n"
                      "(%dP x %dL x %dB) does not match master file (%dP x %dL x %dB)", 
                      osAuxFilename.c_str(),
                      poODS->GetRasterXSize(), 
                      poODS->GetRasterYSize(),
                      poODS->GetRasterCount(),
                      poDependentDS->GetRasterXSize(), 
                      poDependentDS->GetRasterYSize(),
                      poDependentDS->GetRasterCount() );

            GDALClose( poODS );
            poODS = NULL;
        }
    }
        
/* -------------------------------------------------------------------- */
/*      Try appending .aux to the end of the filename.                  */
/* -------------------------------------------------------------------- */
    if( poODS == NULL )
    {
        osAuxFilename = pszBasename;
        osAuxFilename += ".";
        osAuxFilename += pszAuxSuffixLC;
        fp = VSIFOpenL( osAuxFilename, "rb" );
        if ( fp == NULL && VSIIsCaseSensitiveFS(osAuxFilename) )
        {
            // Can't found file with lower case suffix. Try the upper case one.
            osAuxFilename = pszBasename;
            osAuxFilename += ".";
            osAuxFilename += pszAuxSuffixUC;
            fp = VSIFOpenL( osAuxFilename, "rb" );
        }

        if( fp != NULL )
        {
            if( VSIFReadL( abyHeader, 1, 32, fp ) == 32 &&
                EQUALN((char *) abyHeader,"EHFA_HEADER_TAG",15) )
            {
                /* Avoid causing failure in opening of main file from SWIG bindings */
                /* when auxiliary file cannot be opened (#3269) */
                CPLTurnFailureIntoWarning(TRUE);
                poODS = (GDALDataset *) GDALOpenShared( osAuxFilename, eAccess );
                CPLTurnFailureIntoWarning(FALSE);
            }
            VSIFCloseL( fp );
        }
 
        if( poODS != NULL )
        {
            const char *pszDep
                = poODS->GetMetadataItem( "HFA_DEPENDENT_FILE", "HFA" );
            if( pszDep == NULL  )
            {
                CPLDebug( "AUX", 
                          "Found %s but it has no dependent file, ignoring.",
                          osAuxFilename.c_str() );
                GDALClose( poODS );
                poODS = NULL;
            }
            else if( !EQUAL(pszDep,osJustFile) )
            {
                VSIStatBufL sStatBuf;

                if( VSIStatExL( pszDep, &sStatBuf, VSI_STAT_EXISTS_FLAG ) == 0 )
                {
                    CPLDebug( "AUX", "%s is for file %s, not %s, ignoring.",
                              osAuxFilename.c_str(), 
                              pszDep, osJustFile.c_str() );
                    GDALClose( poODS );
                    poODS = NULL;
                }
                else
                {
                    CPLDebug( "AUX", "%s is for file %s, not %s, but since\n"
                              "%s does not exist, we will use .aux file as our own.",
                              osAuxFilename.c_str(), 
                              pszDep, osJustFile.c_str(),
                              pszDep );
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Confirm that the aux file matches the configuration of the      */
/*      dependent dataset.                                              */
/* -------------------------------------------------------------------- */
    if( poODS != NULL && poDependentDS != NULL
        && (poODS->GetRasterCount() != poDependentDS->GetRasterCount()
            || poODS->GetRasterXSize() != poDependentDS->GetRasterXSize()
            || poODS->GetRasterYSize() != poDependentDS->GetRasterYSize()) )
    {
        CPLDebug( "AUX",
                  "Ignoring aux file %s as its raster configuration\n"
                  "(%dP x %dL x %dB) does not match master file (%dP x %dL x %dB)", 
                  osAuxFilename.c_str(),
                  poODS->GetRasterXSize(), 
                  poODS->GetRasterYSize(),
                  poODS->GetRasterCount(),
                  poDependentDS->GetRasterXSize(), 
                  poDependentDS->GetRasterYSize(),
                  poDependentDS->GetRasterCount() );

        GDALClose( poODS );
        poODS = NULL;
    }

    return poODS;
}

/************************************************************************/
/* -------------------------------------------------------------------- */
/*      The following stubs are present to ensure that older GDAL       */
/*      bridges don't fail with newer libraries.                        */
/* -------------------------------------------------------------------- */
/************************************************************************/

CPL_C_START

void * CPL_STDCALL GDALCreateProjDef( const char * )
{
    CPLDebug( "GDAL", "GDALCreateProjDef no longer supported." );
    return NULL;
}

CPLErr CPL_STDCALL GDALReprojectToLongLat( void *, double *, double * )
{
    CPLDebug( "GDAL", "GDALReprojectToLatLong no longer supported." );
    return CE_Failure;
}

CPLErr CPL_STDCALL GDALReprojectFromLongLat( void *, double *, double * )
{
    CPLDebug( "GDAL", "GDALReprojectFromLatLong no longer supported." );
    return CE_Failure;
}

void CPL_STDCALL GDALDestroyProjDef( void * )

{
    CPLDebug( "GDAL", "GDALDestroyProjDef no longer supported." );
}

CPL_C_END

/************************************************************************/
/* Infrastructure to check that dataset characteristics are valid       */
/************************************************************************/

CPL_C_START

/**
  * \brief Return TRUE if the dataset dimensions are valid.
  *
  * @param nXSize raster width
  * @param nYSize raster height
  *
  * @since GDAL 1.7.0
  */
int GDALCheckDatasetDimensions( int nXSize, int nYSize )
{
    if (nXSize <= 0 || nYSize <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid dataset dimensions : %d x %d", nXSize, nYSize);
        return FALSE;
    }
    return TRUE;
}

/**
  * \brief Return TRUE if the band count is valid.
  *
  * If the configuration option GDAL_MAX_BAND_COUNT is defined,
  * the band count will be compared to the maximum number of band allowed.
  *
  * @param nBands the band count
  * @param bIsZeroAllowed TRUE if band count == 0 is allowed
  *
  * @since GDAL 1.7.0
  */

int GDALCheckBandCount( int nBands, int bIsZeroAllowed )
{
    int nMaxBands = -1;
    const char* pszMaxBandCount = CPLGetConfigOption("GDAL_MAX_BAND_COUNT", NULL);
    if (pszMaxBandCount != NULL)
    {
        nMaxBands = atoi(pszMaxBandCount);
    }
    if (nBands < 0 || (!bIsZeroAllowed && nBands == 0) ||
        (nMaxBands >= 0 && nBands > nMaxBands) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid band count : %d", nBands);
        return FALSE;
    }
    return TRUE;
}

CPL_C_END

