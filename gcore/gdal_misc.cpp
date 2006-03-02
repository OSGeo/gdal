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
 * Revision 1.77  2006/03/02 11:33:31  dron
 * Unused variable in GDALFindAssociatedAuxFile() removed.
 *
 * Revision 1.76  2006/01/23 15:23:45  fwarmerdam
 * Modified GDALTermProgress to output "." on 2.5% intervals except
 * integral ones.
 *
 * Revision 1.75  2005/12/06 21:51:01  fwarmerdam
 * before calling GDALOpen on a .aux file, verify it is HFA
 *
 * Revision 1.74  2005/10/07 13:31:26  dron
 * Allow pixel width/height to be zero when rotation is present
 * (GDALReadWorldFile).
 *
 * Revision 1.73  2005/09/26 15:52:03  fwarmerdam
 * centralized .aux opening logic
 *
 * Revision 1.72  2005/09/11 21:07:54  fwarmerdam
 * Changed worldfile reading to use large file API.
 *
 * Revision 1.71  2005/09/11 19:16:31  fwarmerdam
 * Use CPLString for safer string manipulation.
 *
 * Revision 1.70  2005/07/15 13:28:00  fwarmerdam
 * Fixed another big raster int overflow problem in GDALGetRasterSampleOverview
 *
 * Revision 1.69  2005/07/13 17:21:10  fwarmerdam
 * Avoiding 32bit overflow in GDALGetRasterSampleOverview().
 *
 * Revision 1.68  2005/07/08 18:59:54  fwarmerdam
 * Don't use thread local storage for GDALTermProgress() last complete, or
 * the GDALVersionInfo() static buffer.
 *
 * Revision 1.67  2005/05/23 06:44:31  fwarmerdam
 * Updated for locking of block refs
 *
 * Revision 1.66  2005/04/04 15:24:48  fwarmerdam
 * Most C entry points now CPL_STDCALL
 *
 * Revision 1.65  2005/03/21 16:12:08  fwarmerdam
 * Trimmed log.
 *
 * Revision 1.64  2005/03/21 16:11:32  fwarmerdam
 * Added special case for simple 4 corners, non-rotated case in
 * GDALGCPsToGeoTransform().
 *
 * Revision 1.63  2005/02/10 04:30:29  fwarmerdam
 * added support for YCbCr color space
 *
 * Revision 1.62  2004/11/23 19:54:07  fwarmerdam
 * Fixed initialization bug in szDerivedExtension in GDALReadWorldFile().
 *
 * Revision 1.61  2004/10/26 15:59:06  fwarmerdam
 * Added rw+ for formats with Create().
 *
 * Revision 1.60  2004/08/09 14:40:41  warmerda
 * return name for GDT_Unknown
 *
 * Revision 1.59  2004/05/28 16:05:25  warmerda
 * add default ext handling and docs for  GDALReadWorldFile
 *
 * Revision 1.58  2004/05/25 16:58:59  warmerda
 * Try to return an error if we don't find GCPs in TAB file.
 *
 * Revision 1.57  2004/04/21 15:48:47  warmerda
 * Reimplement GDALGetGeoTransformFromGCPs to do best fit - Eric Donges
 *
 * Revision 1.56  2004/04/02 18:42:48  warmerda
 * Added rw/ro flag in --formats list.
 *
 * Revision 1.55  2004/04/02 18:01:35  warmerda
 * Finished docs for commandline processor function.
 *
 * Revision 1.54  2004/04/02 17:58:29  warmerda
 * added --optfile support in general commandline processor
 *
 * Revision 1.53  2004/04/02 17:32:40  warmerda
 * added GDALGeneralCmdLineProcessor()
 *
 * Revision 1.52  2004/02/25 09:03:15  dron
 * Added GDALPackedDMSToDec() and GDALDecToPackedDMS() functions.
 *
 * Revision 1.51  2004/02/18 14:59:55  dron
 * Properly determine pixel offset in last tiles in GDALGetRandomRasterSample().
 *
 * Revision 1.50  2004/01/18 16:43:37  dron
 * Added GDALGetDataTypeByName() function.
 */

#include "gdal_priv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include <ctype.h>
#include <string>

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
 * Get data type by symbolic name.
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

void CPL_STDCALL 
GDALComputeRasterMinMax( GDALRasterBandH hBand, int bApproxOK, 
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
        
        poBlock = poBand->GetLockedBlockRef( iXBlock, iYBlock );
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

        poBlock->DropLock();
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

int CPL_STDCALL GDALDummyProgress( double, const char *, void * )

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
 * Cleanup scaled progress handle.
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
            fprintf( stdout, "%d", (int) floor(dfComplete*100) );
            fflush( stdout );
        }
    }
    else if( floor(dfLastComplete*40) != floor(dfComplete*40) )
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

GDALRasterBandH CPL_STDCALL 
GDALGetRasterSampleOverview( GDALRasterBandH hBand, 
                             int nDesiredSamples )

{
    double dfBestSamples; 
    GDALRasterBandH hBestBand = hBand;

    dfBestSamples = GDALGetRasterBandXSize(hBand) 
        * (double) GDALGetRasterBandYSize(hBand);

    for( int iOverview = 0; 
         iOverview < GDALGetOverviewCount( hBand );
         iOverview++ )
    {
        GDALRasterBandH hOBand = GDALGetOverview( hBand, iOverview );
        double    dfOSamples;

        dfOSamples = GDALGetRasterBandXSize(hOBand) 
            * (double) GDALGetRasterBandYSize(hOBand);

        if( dfOSamples < dfBestSamples && dfOSamples > nDesiredSamples )
        {
            dfBestSamples = dfOSamples;
            hBestBand = hOBand;
        }
    }

    return hBestBand;
}

/************************************************************************/
/*                     GDALGetRandomRasterSample()                      */
/************************************************************************/

int CPL_STDCALL 
GDALGetRandomRasterSample( GDALRasterBandH hBand, int nSamples, 
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

        poBlock = poBand->GetLockedBlockRef( iXBlock, iYBlock );
        if( poBlock == NULL )
            continue;

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
/*                         GDALReadTabFile()                            */
/*                                                                      */
/*      Helper function for translator implementators wanting           */
/*      support for MapInfo .tab-files.                                 */
/************************************************************************/

#define MAX_GCP 256
 
int CPL_STDCALL GDALReadTabFile( const char * pszBaseFilename, 
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
                 && CSLCount(papszTok) > 4
                 && EQUAL(papszTok[4], "Label") 
                 && nCoordinateCount < MAX_GCP )
        {
            GDALInitGCPs( 1, asGCPs + nCoordinateCount );
            
            asGCPs[nCoordinateCount].dfGCPPixel = atof(papszTok[2]);
            asGCPs[nCoordinateCount].dfGCPLine = atof(papszTok[3]);
            asGCPs[nCoordinateCount].dfGCPX = atof(papszTok[0]);
            asGCPs[nCoordinateCount].dfGCPY = atof(papszTok[1]);
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

    CSLDestroy(papszTok);
    CSLDestroy(papszLines);

    if( nCoordinateCount == 0 )
    {
        CPLDebug( "GDAL", "GDALReadTabFile(%s) did not get any GCPs.", 
                  pszTAB );
        return FALSE;
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
     
    return TRUE;
}

/************************************************************************/
/*                         GDALReadWorldFile()                          */
/************************************************************************/

/**
 * Read ESRI world file. 
 *
 * This function reads an ESRI style world file, and formats a geotransform
 * from it's contents.  It will form the filename for the worldfile from the
 * filename of the raster file referred and the suggested extension.  If no
 * extension is provided, the code will internally try the unix style and
 * windows style world file extensions (eg. for .tif these would be .tfw and 
 * .tifw). 
 *
 * The world file contains an affine transformation with the parameters
 * in a different order than in a geotransform array. 
 *
 *  geotransform[1] - width of pixel
 *  geotransform[4] - rotational coefficient, zero for north up images.
 *  geotransform[2] - rotational coefficient, zero for north up images.
 *  geotransform[5] - height of pixel (but negative)
 *  geotransform[0] - x offset to center of top left pixel.
 *  geotrasnform[3] - y offset to center of top left pixel.
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
GDALReadWorldFile( const char * pszBaseFilename, const char *pszExtension,
                   double *padfGeoTransform )

{
    const char  *pszTFW;
    char        szExtUpper[32], szExtLower[32];
    int         i;
    char        **papszLines;

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
        
        if( GDALReadWorldFile( pszBaseFilename, szDerivedExtension, 
                               padfGeoTransform ) )
            return TRUE;

        // unix version - extension + 'w'
        if( oBaseExt.length() > sizeof(szDerivedExtension)-2 )
            return FALSE;

        strcpy( szDerivedExtension, oBaseExt.c_str() );
        strcat( szDerivedExtension, "w" );
        return GDALReadWorldFile( pszBaseFilename, szDerivedExtension, 
                                  padfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Skip the leading period in the extension if there is one.       */
/* -------------------------------------------------------------------- */
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
    VSIStatBufL sStatBuf;
    int bGotTFW;

    pszTFW = CPLResetExtension( pszBaseFilename, szExtLower );

    bGotTFW = VSIStatL( pszTFW, &sStatBuf ) == 0;

#ifndef WIN32
    if( !bGotTFW )
    {
        pszTFW = CPLResetExtension( pszBaseFilename, szExtUpper );
        bGotTFW = VSIStatL( pszTFW, &sStatBuf ) == 0;
    }
#endif
    
    if( !bGotTFW )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      We found the file, now load and parse it.                       */
/* -------------------------------------------------------------------- */
    papszLines = CSLLoad( pszTFW );

    if( CSLCount(papszLines) >= 6 
        && (atof(papszLines[0]) != 0.0 || atof(papszLines[2]) != 0.0)
        && (atof(papszLines[3]) != 0.0 || atof(papszLines[1]) != 0.0) )
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

int CPL_STDCALL 
GDALWriteWorldFile( const char * pszBaseFilename, const char *pszExtension,
                    double *padfGeoTransform )

{

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
    FILE    *fpTFW;

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

const char * CPL_STDCALL GDALVersionInfo( const char *pszRequest )

{
    // NOTE: There is a slight risk of a multithreaded race condition if
    // one thread is in the process of sprintf()ing into this buffer while
    // another is using it but that seems pretty low risk.  All threads
    // want the same value in the buffer.

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

const char * CPL_STDCALL GDALDecToDMS( double dfAngle, const char * pszAxis,
                          int nPrecision )

{
    return CPLDecToDMS( dfAngle, pszAxis, nPrecision );
}

/************************************************************************/
/*                         GDALPackedDMSToDec()                         */
/************************************************************************/

/**
 * Convert a packed DMS value (DDDMMMSSS.SS) into decimal degrees.
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
 * Convert decimal degrees into packed DMS value (DDDMMMSSS.SS).
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
 * Generate Geotransform from GCPs. 
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
 * General utility option processing.
 *
 * This function is intended to provide a variety of generic commandline 
 * options for all GDAL commandline utilities.  It takes care of the following
 * commandline options:
 *  
 *  --version: report version of GDAL in use. 
 *  --formats: report all format drivers configured.
 *  --format [format]: report details of one format driver. 
 *  --optfile filename: expand an option file into the argument list. 
 *  --config key value: set system configuration option. 
 *  --debug [on/off/value]: set debug level.
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
 * @param Pointer to the argument list array (will be updated in place). 
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
                const char *pszRWFlag;
                
                if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATE, NULL ) )
                    pszRWFlag = "rw+";
                else if( GDALGetMetadataItem( hDriver, GDAL_DCAP_CREATECOPY, 
                                              NULL ) )
                    pszRWFlag = "rw";
                else
                    pszRWFlag = "ro";
                
                printf( "  %s (%s): %s\n",
                        GDALGetDriverShortName( hDriver ),
                        pszRWFlag,
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
            
            if( CSLFetchNameValue( papszMD, GDAL_DCAP_CREATE ) )
                printf( "  Supports: Create() - Create writeable dataset.\n" );
            if( CSLFetchNameValue( papszMD, GDAL_DCAP_CREATECOPY ) )
                printf( "  Supports: CreateCopy() - Create dataset by copying another.\n" );
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
            printf( "  --formats: report all configured format drivers.\n" );
            printf( "  --format [format]: details of one format.\n" );
            printf( "  --optfile filename: expand an option file into the argument list.\n" );
            printf( "  --config key value: set system configuration option.\n" );
            printf( "  --debug [on/off/value]: set debug level.\n" );
            printf( "  --help-general: report detailed help on general options.\n" );
            CSLDestroy( papszReturn );
            return 0;
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

int CPL_STDCALL GDALExtractRPCInfo( char **papszMD, GDALRPCInfo *psRPC )

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
/*                     GDALFindAssociatedAuxFile()                      */
/************************************************************************/

GDALDataset *GDALFindAssociatedAuxFile( const char *pszBasename,
                                        GDALAccess eAccess )

{
    if( EQUAL(CPLGetExtension(pszBasename),"aux") )
        return NULL;

/* -------------------------------------------------------------------- */
/*      We didn't find that, so try and find a corresponding aux        */
/*      file.  Check that we are the dependent file of the aux          */
/*      file.                                                           */
/* -------------------------------------------------------------------- */
    CPLString oAuxFilename = CPLResetExtension(pszBasename,"aux");
    CPLString oJustFile = CPLGetFilename(pszBasename); // without dir
    GDALDataset *poODS = NULL;
    GByte abyHeader[32];
    FILE *fp;

    fp = VSIFOpenL( oAuxFilename, "rb" );
    if( fp != NULL )
    {
       VSIFReadL( abyHeader, 1, 32, fp );
       if( EQUALN((char *) abyHeader,"EHFA_HEADER_TAG",15) )
          poODS =  (GDALDataset *) GDALOpenShared( oAuxFilename, eAccess );
       VSIFCloseL( fp );
    }

    if( poODS != NULL )
    {
        const char *pszDep
            = poODS->GetMetadataItem( "HFA_DEPENDENT_FILE", "HFA" );
        if( pszDep == NULL || !EQUAL(pszDep,oJustFile) )
        {
            GDALClose( poODS );
            poODS = NULL;
        }
    }
        
    if( poODS == NULL )
    {
        oAuxFilename = pszBasename;
        oAuxFilename += ".aux";
        fp = VSIFOpenL( oAuxFilename, "rb" );
        if( fp != NULL )
        {
           VSIFReadL( abyHeader, 1, 32, fp );
           if( EQUALN((char *) abyHeader,"EHFA_HEADER_TAG",15) )
              poODS =  (GDALDataset *) GDALOpenShared( oAuxFilename, eAccess );
           VSIFCloseL( fp );
        }
 
        if( poODS != NULL )
        {
            const char *pszDep
                = poODS->GetMetadataItem( "HFA_DEPENDENT_FILE", "HFA" );
            if( pszDep == NULL || !EQUAL(pszDep,oJustFile) )
            {
                GDALClose( poODS );
                poODS = NULL;
            }
        }
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
