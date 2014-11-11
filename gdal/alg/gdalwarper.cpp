/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implementation of high level convenience APIs for warper.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdalwarper.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "ogr_api.h"
#include "gdal_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         GDALReprojectImage()                         */
/************************************************************************/

/**
 * Reproject image.
 *
 * This is a convenience function utilizing the GDALWarpOperation class to
 * reproject an image from a source to a destination.  In particular, this
 * function takes care of establishing the transformation function to
 * implement the reprojection, and will default a variety of other 
 * warp options. 
 *
 * No metadata, projection info, or color tables are transferred
 * to the output file. 
 *
 * Starting with GDAL 2.0, nodata values set on destination dataset are taken
 * into account.
 *
 * @param hSrcDS the source image file. 
 * @param pszSrcWKT the source projection.  If NULL the source projection
 * is read from from hSrcDS.
 * @param hDstDS the destination image file. 
 * @param pszDstWKT the destination projection.  If NULL the destination
 * projection will be read from hDstDS.
 * @param eResampleAlg the type of resampling to use.  
 * @param dfWarpMemoryLimit the amount of memory (in bytes) that the warp
 * API is allowed to use for caching.  This is in addition to the memory
 * already allocated to the GDAL caching (as per GDALSetCacheMax()).  May be
 * 0.0 to use default memory settings.
 * @param dfMaxError maximum error measured in input pixels that is allowed
 * in approximating the transformation (0.0 for exact calculations).
 * @param pfnProgress a GDALProgressFunc() compatible callback function for
 * reporting progress or NULL. 
 * @param pProgressArg argument to be passed to pfnProgress.  May be NULL.
 * @param psOptions warp options, normally NULL.
 *
 * @return CE_None on success or CE_Failure if something goes wrong.
 */

CPLErr CPL_STDCALL
GDALReprojectImage( GDALDatasetH hSrcDS, const char *pszSrcWKT,
                    GDALDatasetH hDstDS, const char *pszDstWKT,
                    GDALResampleAlg eResampleAlg,
                    CPL_UNUSED double dfWarpMemoryLimit,
                    double dfMaxError,
                    GDALProgressFunc pfnProgress, void *pProgressArg,
                    GDALWarpOptions *psOptions )

{
    GDALWarpOptions *psWOptions;

/* -------------------------------------------------------------------- */
/*      Setup a reprojection based transformer.                         */
/* -------------------------------------------------------------------- */
    void *hTransformArg;

    hTransformArg = 
        GDALCreateGenImgProjTransformer( hSrcDS, pszSrcWKT, hDstDS, pszDstWKT, 
                                         TRUE, 1000.0, 0 );

    if( hTransformArg == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Create a copy of the user provided options, or a defaulted      */
/*      options structure.                                              */
/* -------------------------------------------------------------------- */
    if( psOptions == NULL )
        psWOptions = GDALCreateWarpOptions();
    else
        psWOptions = GDALCloneWarpOptions( psOptions );

    psWOptions->eResampleAlg = eResampleAlg;

/* -------------------------------------------------------------------- */
/*      Set transform.                                                  */
/* -------------------------------------------------------------------- */
    if( dfMaxError > 0.0 )
    {
        psWOptions->pTransformerArg = 
            GDALCreateApproxTransformer( GDALGenImgProjTransform, 
                                         hTransformArg, dfMaxError );

        psWOptions->pfnTransformer = GDALApproxTransform;
    }
    else
    {
        psWOptions->pfnTransformer = GDALGenImgProjTransform;
        psWOptions->pTransformerArg = hTransformArg;
    }

/* -------------------------------------------------------------------- */
/*      Set file and band mapping.                                      */
/* -------------------------------------------------------------------- */
    int  iBand;

    psWOptions->hSrcDS = hSrcDS;
    psWOptions->hDstDS = hDstDS;

    if( psWOptions->nBandCount == 0 )
    {
        psWOptions->nBandCount = MIN(GDALGetRasterCount(hSrcDS),
                                     GDALGetRasterCount(hDstDS));
        
        psWOptions->panSrcBands = (int *) 
            CPLMalloc(sizeof(int) * psWOptions->nBandCount);
        psWOptions->panDstBands = (int *) 
            CPLMalloc(sizeof(int) * psWOptions->nBandCount);

        for( iBand = 0; iBand < psWOptions->nBandCount; iBand++ )
        {
            psWOptions->panSrcBands[iBand] = iBand+1;
            psWOptions->panDstBands[iBand] = iBand+1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set source nodata values if the source dataset seems to have    */
/*      any. Same for target nodata values                              */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < psWOptions->nBandCount; iBand++ )
    {
        GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, iBand+1 );
        int             bGotNoData = FALSE;
        double          dfNoDataValue;

        if (GDALGetRasterColorInterpretation(hBand) == GCI_AlphaBand)
        {
            psWOptions->nSrcAlphaBand = iBand + 1;
        }

        dfNoDataValue = GDALGetRasterNoDataValue( hBand, &bGotNoData );
        if( bGotNoData )
        {
            if( psWOptions->padfSrcNoDataReal == NULL )
            {
                int  ii;

                psWOptions->padfSrcNoDataReal = (double *) 
                    CPLMalloc(sizeof(double) * psWOptions->nBandCount);
                psWOptions->padfSrcNoDataImag = (double *) 
                    CPLMalloc(sizeof(double) * psWOptions->nBandCount);

                for( ii = 0; ii < psWOptions->nBandCount; ii++ )
                {
                    psWOptions->padfSrcNoDataReal[ii] = -1.1e20;
                    psWOptions->padfSrcNoDataImag[ii] = 0.0;
                }
            }

            psWOptions->padfSrcNoDataReal[iBand] = dfNoDataValue;
        }

        // Deal with target band
        hBand = GDALGetRasterBand( hDstDS, iBand+1 );
        if (hBand && GDALGetRasterColorInterpretation(hBand) == GCI_AlphaBand)
        {
            psWOptions->nDstAlphaBand = iBand + 1;
        }

        dfNoDataValue = GDALGetRasterNoDataValue( hBand, &bGotNoData );
        if( bGotNoData )
        {
            if( psWOptions->padfDstNoDataReal == NULL )
            {
                int  ii;

                psWOptions->padfDstNoDataReal = (double *) 
                    CPLMalloc(sizeof(double) * psWOptions->nBandCount);
                psWOptions->padfDstNoDataImag = (double *) 
                    CPLMalloc(sizeof(double) * psWOptions->nBandCount);

                for( ii = 0; ii < psWOptions->nBandCount; ii++ )
                {
                    psWOptions->padfDstNoDataReal[ii] = -1.1e20;
                    psWOptions->padfDstNoDataImag[ii] = 0.0;
                }
            }

            psWOptions->padfDstNoDataReal[iBand] = dfNoDataValue;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set the progress function.                                      */
/* -------------------------------------------------------------------- */
    if( pfnProgress != NULL )
    {
        psWOptions->pfnProgress = pfnProgress;
        psWOptions->pProgressArg = pProgressArg;
    }

/* -------------------------------------------------------------------- */
/*      Create a warp options based on the options.                     */
/* -------------------------------------------------------------------- */
    GDALWarpOperation  oWarper;
    CPLErr eErr;

    eErr = oWarper.Initialize( psWOptions );

    if( eErr == CE_None )
        eErr = oWarper.ChunkAndWarpImage( 0, 0, 
                                          GDALGetRasterXSize(hDstDS),
                                          GDALGetRasterYSize(hDstDS) );

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    GDALDestroyGenImgProjTransformer( hTransformArg );

    if( dfMaxError > 0.0 )
        GDALDestroyApproxTransformer( psWOptions->pTransformerArg );
        
    GDALDestroyWarpOptions( psWOptions );

    return eErr;
}

/************************************************************************/
/*                    GDALCreateAndReprojectImage()                     */
/*                                                                      */
/*      This is a "quicky" reprojection API.                            */
/************************************************************************/

CPLErr CPL_STDCALL GDALCreateAndReprojectImage( 
    GDALDatasetH hSrcDS, const char *pszSrcWKT, 
    const char *pszDstFilename, const char *pszDstWKT,
    GDALDriverH hDstDriver, char **papszCreateOptions,
    GDALResampleAlg eResampleAlg, double dfWarpMemoryLimit, double dfMaxError,
    GDALProgressFunc pfnProgress, void *pProgressArg, 
    GDALWarpOptions *psOptions )
    
{
    VALIDATE_POINTER1( hSrcDS, "GDALCreateAndReprojectImage", CE_Failure );

/* -------------------------------------------------------------------- */
/*      Default a few parameters.                                       */
/* -------------------------------------------------------------------- */
    if( hDstDriver == NULL )
    {
        hDstDriver = GDALGetDriverByName( "GTiff" );
        if (hDstDriver == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALCreateAndReprojectImage needs GTiff driver");
            return CE_Failure;
        }
    }

    if( pszSrcWKT == NULL )
        pszSrcWKT = GDALGetProjectionRef( hSrcDS );

    if( pszDstWKT == NULL )
        pszDstWKT = pszSrcWKT;

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    void *hTransformArg;

    hTransformArg = 
        GDALCreateGenImgProjTransformer( hSrcDS, pszSrcWKT, NULL, pszDstWKT, 
                                         TRUE, 1000.0, 0 );

    if( hTransformArg == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Get approximate output definition.                              */
/* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6];
    int    nPixels, nLines;

    if( GDALSuggestedWarpOutput( hSrcDS, 
                                 GDALGenImgProjTransform, hTransformArg, 
                                 adfDstGeoTransform, &nPixels, &nLines )
        != CE_None )
        return CE_Failure;

    GDALDestroyGenImgProjTransformer( hTransformArg );

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS;

    hDstDS = GDALCreate( hDstDriver, pszDstFilename, nPixels, nLines, 
                         GDALGetRasterCount(hSrcDS),
                         GDALGetRasterDataType(GDALGetRasterBand(hSrcDS,1)),
                         papszCreateOptions );

    if( hDstDS == NULL )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Write out the projection definition.                            */
/* -------------------------------------------------------------------- */
    GDALSetProjection( hDstDS, pszDstWKT );
    GDALSetGeoTransform( hDstDS, adfDstGeoTransform );

/* -------------------------------------------------------------------- */
/*      Perform the reprojection.                                       */
/* -------------------------------------------------------------------- */
    CPLErr eErr ;

    eErr = 
        GDALReprojectImage( hSrcDS, pszSrcWKT, hDstDS, pszDstWKT, 
                            eResampleAlg, dfWarpMemoryLimit, dfMaxError,
                            pfnProgress, pProgressArg, psOptions );

    GDALClose( hDstDS );

    return eErr;
}

/************************************************************************/
/*                        GDALWarpNoDataMasker()                        */
/*                                                                      */
/*      GDALMaskFunc for establishing a validity mask for a source      */
/*      band based on a provided NODATA value.                          */
/************************************************************************/

CPLErr 
GDALWarpNoDataMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType, 
                      int /* nXOff */, int /* nYOff */, int nXSize, int nYSize,
                      GByte **ppImageData, 
                      int bMaskIsFloat, void *pValidityMask )

{
    double *padfNoData = (double *) pMaskFuncArg;
    GUInt32 *panValidityMask = (GUInt32 *) pValidityMask;

    if( nBandCount != 1 || bMaskIsFloat )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Invalid nBandCount or bMaskIsFloat argument in SourceNoDataMask" );
        return CE_Failure;
    }

    switch( eType )
    {
      case GDT_Byte:
      {
          int nNoData = (int) padfNoData[0];
          GByte *pabyData = (GByte *) *ppImageData;
          int iOffset;

          // nothing to do if value is out of range.
          if( padfNoData[0] < 0.0 || padfNoData[0] > 255.000001 
              || padfNoData[1] != 0.0 )
              return CE_None;

          for( iOffset = nXSize*nYSize-1; iOffset >= 0; iOffset-- )
          {
              if( pabyData[iOffset] == nNoData )
              {
                  panValidityMask[iOffset>>5] &= ~(0x01 << (iOffset & 0x1f));
              }
          }
      }
      break;
      
      case GDT_Int16:
      {
          int nNoData = (int) padfNoData[0];
          GInt16 *panData = (GInt16 *) *ppImageData;
          int iOffset;

          // nothing to do if value is out of range.
          if( padfNoData[0] < -32768 || padfNoData[0] > 32767
              || padfNoData[1] != 0.0 )
              return CE_None;

          for( iOffset = nXSize*nYSize-1; iOffset >= 0; iOffset-- )
          {
              if( panData[iOffset] == nNoData )
              {
                  panValidityMask[iOffset>>5] &= ~(0x01 << (iOffset & 0x1f));
              }
          }
      }
      break;
      
      case GDT_UInt16:
      {
          int nNoData = (int) padfNoData[0];
          GUInt16 *panData = (GUInt16 *) *ppImageData;
          int iOffset;

          // nothing to do if value is out of range.
          if( padfNoData[0] < 0 || padfNoData[0] > 65535
              || padfNoData[1] != 0.0 )
              return CE_None;

          for( iOffset = nXSize*nYSize-1; iOffset >= 0; iOffset-- )
          {
              if( panData[iOffset] == nNoData )
              {
                  panValidityMask[iOffset>>5] &= ~(0x01 << (iOffset & 0x1f));
              }
          }
      }
      break;
      
      case GDT_Float32:
      {
          float fNoData = (float) padfNoData[0];
          float *pafData = (float *) *ppImageData;
          int iOffset;
          int bIsNoDataNan = CPLIsNan(fNoData);

          // nothing to do if value is out of range.
          if( padfNoData[1] != 0.0 )
              return CE_None;

          for( iOffset = nXSize*nYSize-1; iOffset >= 0; iOffset-- )
          {
              float fVal = pafData[iOffset];
              if( (bIsNoDataNan && CPLIsNan(fVal)) || (!bIsNoDataNan && ARE_REAL_EQUAL(fVal, fNoData)) )
              {
                  panValidityMask[iOffset>>5] &= ~(0x01 << (iOffset & 0x1f));
              }
          }
      }
      break;
      
      case GDT_Float64:
      {
          double dfNoData = padfNoData[0];
          double *padfData = (double *) *ppImageData;
          int iOffset;
          int bIsNoDataNan = CPLIsNan(dfNoData);

          // nothing to do if value is out of range.
          if( padfNoData[1] != 0.0 )
              return CE_None;

          for( iOffset = nXSize*nYSize-1; iOffset >= 0; iOffset-- )
          {
              double dfVal = padfData[iOffset];
              if( (bIsNoDataNan && CPLIsNan(dfVal)) || (!bIsNoDataNan && ARE_REAL_EQUAL(dfVal, dfNoData)) )
              {
                  panValidityMask[iOffset>>5] &= ~(0x01 << (iOffset & 0x1f));
              }
          }
      }
      break;
      
      default:
      {
          double  *padfWrk;
          int     iLine, iPixel;
          int     nWordSize = GDALGetDataTypeSize(eType)/8;
          
          int bIsNoDataRealNan = CPLIsNan(padfNoData[0]);
          int bIsNoDataImagNan = CPLIsNan(padfNoData[1]);

          padfWrk = (double *) CPLMalloc(nXSize * sizeof(double) * 2);
          for( iLine = 0; iLine < nYSize; iLine++ )
          {
              GDALCopyWords( ((GByte *) *ppImageData)+nWordSize*iLine*nXSize, 
                             eType, nWordSize,
                             padfWrk, GDT_CFloat64, 16, nXSize );
              
              for( iPixel = 0; iPixel < nXSize; iPixel++ )
              {
                  if( ((bIsNoDataRealNan && CPLIsNan(padfWrk[iPixel*2])) ||
                       (!bIsNoDataRealNan && ARE_REAL_EQUAL(padfWrk[iPixel*2], padfNoData[0])))
                      && ((bIsNoDataImagNan && CPLIsNan(padfWrk[iPixel*2+1])) ||
                          (!bIsNoDataImagNan && ARE_REAL_EQUAL(padfWrk[iPixel*2+1], padfNoData[1]))) )
                  {
                      int iOffset = iPixel + iLine * nXSize;
                      
                      panValidityMask[iOffset>>5] &=
                          ~(0x01 << (iOffset & 0x1f));
                  }
              }
              
          }

          CPLFree( padfWrk );
      }
      break;
    }

    return CE_None;
}

/************************************************************************/
/*                       GDALWarpSrcAlphaMasker()                       */
/*                                                                      */
/*      GDALMaskFunc for reading source simple 8bit alpha mask          */
/*      information and building a floating point density mask from     */
/*      it.                                                             */
/************************************************************************/

CPLErr 
GDALWarpSrcAlphaMasker( void *pMaskFuncArg,
                        CPL_UNUSED int nBandCount,
                        CPL_UNUSED GDALDataType eType,
                        int nXOff, int nYOff, int nXSize, int nYSize,
                        GByte ** /*ppImageData */,
                        int bMaskIsFloat, void *pValidityMask )

{
    GDALWarpOptions *psWO = (GDALWarpOptions *) pMaskFuncArg;
    float *pafMask = (float *) pValidityMask;

/* -------------------------------------------------------------------- */
/*      Do some minimal checking.                                       */
/* -------------------------------------------------------------------- */
    if( !bMaskIsFloat )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    if( psWO == NULL || psWO->nSrcAlphaBand < 1 )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the alpha band.                                            */
/* -------------------------------------------------------------------- */
    CPLErr eErr;
    GDALRasterBandH hAlphaBand = GDALGetRasterBand( psWO->hSrcDS, 
                                                    psWO->nSrcAlphaBand );
    if (hAlphaBand == NULL)
        return CE_Failure;

    eErr = GDALRasterIO( hAlphaBand, GF_Read, nXOff, nYOff, nXSize, nYSize, 
                         pafMask, nXSize, nYSize, GDT_Float32, 0, 0 );

    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Rescale from 0-255 to 0.0-1.0.                                  */
/* -------------------------------------------------------------------- */
    for( int iPixel = nXSize * nYSize - 1; iPixel >= 0; iPixel-- )
    {                                    //  (1/255)
        pafMask[iPixel] = (float)( pafMask[iPixel] * 0.00392157 );
        pafMask[iPixel] = MIN( 1.0F, pafMask[iPixel] );
    }

    return CE_None;
}

/************************************************************************/
/*                       GDALWarpSrcMaskMasker()                        */
/*                                                                      */
/*      GDALMaskFunc for reading source simple 8bit validity mask       */
/*      information and building a one bit validity mask.               */
/************************************************************************/

CPLErr
GDALWarpSrcMaskMasker( void *pMaskFuncArg,
                       CPL_UNUSED int nBandCount,
                       CPL_UNUSED GDALDataType eType,
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       GByte ** /*ppImageData */,
                       int bMaskIsFloat, void *pValidityMask )

{
    GDALWarpOptions *psWO = (GDALWarpOptions *) pMaskFuncArg;
    GUInt32  *panMask = (GUInt32 *) pValidityMask;

/* -------------------------------------------------------------------- */
/*      Do some minimal checking.                                       */
/* -------------------------------------------------------------------- */
    if( bMaskIsFloat )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    if( psWO == NULL )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer to read mask byte data into.        */
/* -------------------------------------------------------------------- */
    GByte *pabySrcMask;

    pabySrcMask = (GByte *) VSIMalloc2(nXSize,nYSize);
    if( pabySrcMask == NULL )
    {
        CPLError( CE_Failure, CPLE_OutOfMemory,
                  "Failed to allocate pabySrcMask (%dx%d) in GDALWarpSrcMaskMasker()", 
                  nXSize, nYSize );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Fetch our mask band.                                            */
/* -------------------------------------------------------------------- */
    GDALRasterBandH hSrcBand, hMaskBand = NULL;

    hSrcBand = GDALGetRasterBand( psWO->hSrcDS, psWO->panSrcBands[0] );
    if( hSrcBand != NULL )
        hMaskBand = GDALGetMaskBand( hSrcBand );

    if( hMaskBand == NULL )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the mask band.                                             */
/* -------------------------------------------------------------------- */
    CPLErr eErr;

    eErr = GDALRasterIO( hMaskBand, GF_Read, nXOff, nYOff, nXSize, nYSize, 
                         pabySrcMask, nXSize, nYSize, GDT_Byte, 0, 0 );

    if( eErr != CE_None )
    {
        CPLFree( pabySrcMask );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Pack into 1 bit per pixel for validity.                         */
/* -------------------------------------------------------------------- */
    for( int iPixel = nXSize * nYSize - 1; iPixel >= 0; iPixel-- )
    {
        if( pabySrcMask[iPixel] == 0 )
            panMask[iPixel>>5] &= ~(0x01 << (iPixel & 0x1f));
    }

    CPLFree( pabySrcMask );

    return CE_None;
}

/************************************************************************/
/*                       GDALWarpDstAlphaMasker()                       */
/*                                                                      */
/*      GDALMaskFunc for reading or writing the destination simple      */
/*      8bit alpha mask information and building a floating point       */
/*      density mask from it.   Note, writing is distinguished          */
/*      negative bandcount.                                             */
/************************************************************************/

CPLErr
GDALWarpDstAlphaMasker( void *pMaskFuncArg, int nBandCount,
                        CPL_UNUSED GDALDataType eType,
                        int nXOff, int nYOff, int nXSize, int nYSize,
                        GByte ** /*ppImageData */,
                        int bMaskIsFloat, void *pValidityMask )
{
    GDALWarpOptions *psWO = (GDALWarpOptions *) pMaskFuncArg;
    float *pafMask = (float *) pValidityMask;
    int iPixel;
    CPLErr eErr;

/* -------------------------------------------------------------------- */
/*      Do some minimal checking.                                       */
/* -------------------------------------------------------------------- */
    if( !bMaskIsFloat )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    if( psWO == NULL || psWO->nDstAlphaBand < 1 )
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }

    GDALRasterBandH hAlphaBand = 
        GDALGetRasterBand( psWO->hDstDS, psWO->nDstAlphaBand );
    if (hAlphaBand == NULL)
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Read alpha case.						*/
/* -------------------------------------------------------------------- */
    if( nBandCount >= 0 )
    {
        const char *pszInitDest = 
            CSLFetchNameValue( psWO->papszWarpOptions, "INIT_DEST" );

        // Special logic for destinations being initialized on the fly.
        if( pszInitDest != NULL )
        {
            for( iPixel = nXSize * nYSize - 1; iPixel >= 0; iPixel-- )
                pafMask[iPixel] = 0.0;
            return CE_None;
        }

        // Read data.
        eErr = GDALRasterIO( hAlphaBand, GF_Read, nXOff, nYOff, nXSize, nYSize,
                             pafMask, nXSize, nYSize, GDT_Float32, 0, 0 );
        
        if( eErr != CE_None )
            return eErr;

        // rescale.
        for( iPixel = nXSize * nYSize - 1; iPixel >= 0; iPixel-- )
        {
            pafMask[iPixel] = (float) (pafMask[iPixel] * 0.00392157);
            pafMask[iPixel] = MIN( 1.0F, pafMask[iPixel] );
        }

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Write alpha case.                                               */
/* -------------------------------------------------------------------- */
    else
    {
        for( iPixel = nXSize * nYSize - 1; iPixel >= 0; iPixel-- )
            pafMask[iPixel] = (float)(int) ( pafMask[iPixel] * 255.1 );
        
        // Write data.

        /* The VRT warper will pass destination sizes that may exceed */
        /* the size of the raster for the partial blocks at the right */
        /* and bottom of the band. So let's adjust the size */
        int nDstXSize = nXSize;
        if (nXOff + nXSize > GDALGetRasterBandXSize(hAlphaBand))
            nDstXSize = GDALGetRasterBandXSize(hAlphaBand) - nXOff;
        int nDstYSize = nYSize;
        if (nYOff + nYSize > GDALGetRasterBandYSize(hAlphaBand))
            nDstYSize = GDALGetRasterBandYSize(hAlphaBand) - nYOff;

        eErr = GDALRasterIO( hAlphaBand, GF_Write, 
                             nXOff, nYOff, nDstXSize, nDstYSize, 
                             pafMask, nDstXSize, nDstYSize, GDT_Float32,
                             0, sizeof(float) * nXSize );
        return eErr;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALWarpOptions                            */
/* ==================================================================== */
/************************************************************************/

/**
 * \var char **GDALWarpOptions::papszWarpOptions;
 *
 * A string list of additional options controlling the warp operation in
 * name=value format.  A suitable string list can be prepared with 
 * CSLSetNameValue().
 *
 * The following values are currently supported:
 * 
 *  - INIT_DEST=[value] or INIT_DEST=NO_DATA: This option forces the 
 * destination image to be initialized to the indicated value (for all bands)
 * or indicates that it should be initialized to the NO_DATA value in
 * padfDstNoDataReal/padfDstNoDataImag.  If this value isn't set the
 * destination image will be read and overlayed.  
 *
 * - WRITE_FLUSH=YES/NO: This option forces a flush to disk of data after
 * each chunk is processed.  In some cases this helps ensure a serial 
 * writing of the output data otherwise a block of data may be written to disk
 * each time a block of data is read for the input buffer resulting in alot
 * of extra seeking around the disk, and reduced IO throughput.  The default
 * at this time is NO.
 *
 * - SKIP_NOSOURCE=YES/NO: Skip all processing for chunks for which there
 * is no corresponding input data.  This will disable initializing the 
 * destination (INIT_DEST) and all other processing, and so should be used
 * careful.  Mostly useful to short circuit a lot of extra work in mosaicing 
 * situations.
 * 
 * - UNIFIED_SRC_NODATA=YES/[NO]: By default nodata masking values considered
 * independently for each band.  However, sometimes it is desired to treat all
 * bands as nodata if and only if, all bands match the corresponding nodata
 * values.  To get this behavior set this option to YES. 
 *
 * Normally when computing the source raster data to 
 * load to generate a particular output area, the warper samples transforms
 * 21 points along each edge of the destination region back onto the source
 * file, and uses this to compute a bounding window on the source image that
 * is sufficient.  Depending on the transformation in effect, the source 
 * window may be a bit too small, or even missing large areas.  Problem 
 * situations are those where the transformation is very non-linear or 
 * "inside out".  Examples are transforming from WGS84 to Polar Steregraphic
 * for areas around the pole, or transformations where some of the image is
 * untransformable.  The following options provide some additional control
 * to deal with errors in computing the source window:
 * 
 * - SAMPLE_GRID=YES/NO: Setting this option to YES will force the sampling to 
 * include internal points as well as edge points which can be important if
 * the transformation is esoteric inside out, or if large sections of the
 * destination image are not transformable into the source coordinate system.
 *
 * - SAMPLE_STEPS: Modifies the density of the sampling grid.  The default
 * number of steps is 21.   Increasing this can increase the computational
 * cost, but improves the accuracy with which the source region is computed.
 *
 * - SOURCE_EXTRA: This is a number of extra pixels added around the source
 * window for a given request, and by default it is 1 to take care of rounding
 * error.  Setting this larger will incease the amount of data that needs to
 * be read, but can avoid missing source data.  
 *
 * - CUTLINE: This may contain the WKT geometry for a cutline.  It will
 * be converted into a geometry by GDALWarpOperation::Initialize() and assigned
 * to the GDALWarpOptions hCutline field. The coordinates must be expressed
 * in source pixel/line coordinates. Note: this is different from the assumptions
 * made for the -cutline option of the gdalwarp utility !
 *
 * - CUTLINE_BLEND_DIST: This may be set with a distance in pixels which
 * will be assigned to the dfCutlineBlendDist field in the GDALWarpOptions.
 *
 * - CUTLINE_ALL_TOUCHED: This defaults to FALSE, but may be set to TRUE
 * to enable ALL_TOUCHEd mode when rasterizing cutline polygons.  This is
 * useful to ensure that that all pixels overlapping the cutline polygon
 * will be selected, not just those whose center point falls within the 
 * polygon.
 *
 * - OPTIMIZE_SIZE: This defaults to FALSE, but may be set to TRUE when
 * outputing typically to a compressed dataset (GeoTIFF with COMPRESSED creation
 * option set for example) for achieving a smaller file size. This is achieved
 * by writing at once data aligned on full blocks of the target dataset, which
 * avoids partial writes of compressed blocks and lost space when they are rewritten
 * at the end of the file. However sticking to target block size may cause major
 * processing slowdown for some particular reprojections.
 *
 * - NUM_THREADS: (GDAL >= 1.10) Can be set to a numeric value or ALL_CPUS to
 * set the number of threads to use to parallelize the computation part of the
 * warping. If not set, computation will be done in a single thread.
 */

/************************************************************************/
/*                       GDALCreateWarpOptions()                        */
/************************************************************************/

GDALWarpOptions * CPL_STDCALL GDALCreateWarpOptions()

{
    GDALWarpOptions *psOptions;

    psOptions = (GDALWarpOptions *) CPLCalloc(sizeof(GDALWarpOptions),1);

    psOptions->nBandCount = 0;
    psOptions->eResampleAlg = GRA_NearestNeighbour;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->eWorkingDataType = GDT_Unknown;

    return psOptions;
}

/************************************************************************/
/*                       GDALDestroyWarpOptions()                       */
/************************************************************************/

void CPL_STDCALL GDALDestroyWarpOptions( GDALWarpOptions *psOptions )

{
    VALIDATE_POINTER0( psOptions, "GDALDestroyWarpOptions" );

    CSLDestroy( psOptions->papszWarpOptions );
    CPLFree( psOptions->panSrcBands );
    CPLFree( psOptions->panDstBands );
    CPLFree( psOptions->padfSrcNoDataReal );
    CPLFree( psOptions->padfSrcNoDataImag );
    CPLFree( psOptions->padfDstNoDataReal );
    CPLFree( psOptions->padfDstNoDataImag );
    CPLFree( psOptions->papfnSrcPerBandValidityMaskFunc );
    CPLFree( psOptions->papSrcPerBandValidityMaskFuncArg );

    if( psOptions->hCutline != NULL )
        OGR_G_DestroyGeometry( (OGRGeometryH) psOptions->hCutline );

    CPLFree( psOptions );
}


#define COPY_MEM(target,type,count)					\
   do { if( (psSrcOptions->target) != NULL && (count) != 0 ) 		\
   { 									\
       (psDstOptions->target) = (type *) CPLMalloc(sizeof(type)*(count)); \
       memcpy( (psDstOptions->target), (psSrcOptions->target),		\
 	       sizeof(type) * (count) ); 	        			\
   } \
   else \
       (psDstOptions->target) = NULL; } while(0)

/************************************************************************/
/*                        GDALCloneWarpOptions()                        */
/************************************************************************/

GDALWarpOptions * CPL_STDCALL
GDALCloneWarpOptions( const GDALWarpOptions *psSrcOptions )

{
    GDALWarpOptions *psDstOptions = GDALCreateWarpOptions();

    memcpy( psDstOptions, psSrcOptions, sizeof(GDALWarpOptions) );

    if( psSrcOptions->papszWarpOptions != NULL )
        psDstOptions->papszWarpOptions = 
            CSLDuplicate( psSrcOptions->papszWarpOptions );

    COPY_MEM( panSrcBands, int, psSrcOptions->nBandCount );
    COPY_MEM( panDstBands, int, psSrcOptions->nBandCount );
    COPY_MEM( padfSrcNoDataReal, double, psSrcOptions->nBandCount );
    COPY_MEM( padfSrcNoDataImag, double, psSrcOptions->nBandCount );
    COPY_MEM( padfDstNoDataReal, double, psSrcOptions->nBandCount );
    COPY_MEM( padfDstNoDataImag, double, psSrcOptions->nBandCount );
    COPY_MEM( papfnSrcPerBandValidityMaskFunc, GDALMaskFunc, 
              psSrcOptions->nBandCount );
    psDstOptions->papSrcPerBandValidityMaskFuncArg = NULL;

    if( psSrcOptions->hCutline != NULL )
        psDstOptions->hCutline = 
            OGR_G_Clone( (OGRGeometryH) psSrcOptions->hCutline );
    psDstOptions->dfCutlineBlendDist = psSrcOptions->dfCutlineBlendDist;

    return psDstOptions;
}

/************************************************************************/
/*                      GDALSerializeWarpOptions()                      */
/************************************************************************/

CPLXMLNode * CPL_STDCALL 
GDALSerializeWarpOptions( const GDALWarpOptions *psWO )

{
    CPLXMLNode *psTree;

/* -------------------------------------------------------------------- */
/*      Create root.                                                    */
/* -------------------------------------------------------------------- */
    psTree = CPLCreateXMLNode( NULL, CXT_Element, "GDALWarpOptions" );
    
/* -------------------------------------------------------------------- */
/*      WarpMemoryLimit                                                 */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue( 
        psTree, "WarpMemoryLimit", 
        CPLString().Printf("%g", psWO->dfWarpMemoryLimit ) );

/* -------------------------------------------------------------------- */
/*      ResampleAlg                                                     */
/* -------------------------------------------------------------------- */
    const char *pszAlgName;

    if( psWO->eResampleAlg == GRA_NearestNeighbour )
        pszAlgName = "NearestNeighbour";
    else if( psWO->eResampleAlg == GRA_Bilinear )
        pszAlgName = "Bilinear";
    else if( psWO->eResampleAlg == GRA_Cubic )
        pszAlgName = "Cubic";
    else if( psWO->eResampleAlg == GRA_CubicSpline )
        pszAlgName = "CubicSpline";
    else if( psWO->eResampleAlg == GRA_Lanczos )
        pszAlgName = "Lanczos";
    else if( psWO->eResampleAlg == GRA_Average )
        pszAlgName = "Average";
    else if( psWO->eResampleAlg == GRA_Mode )
        pszAlgName = "Mode";
    else
        pszAlgName = "Unknown";

    CPLCreateXMLElementAndValue( 
        psTree, "ResampleAlg", pszAlgName );

/* -------------------------------------------------------------------- */
/*      Working Data Type                                               */
/* -------------------------------------------------------------------- */

    CPLCreateXMLElementAndValue( 
        psTree, "WorkingDataType", 
        GDALGetDataTypeName( psWO->eWorkingDataType ) );

/* -------------------------------------------------------------------- */
/*      Name/value warp options.                                        */
/* -------------------------------------------------------------------- */
    int iWO;

    for( iWO = 0; psWO->papszWarpOptions != NULL 
             && psWO->papszWarpOptions[iWO] != NULL; iWO++ )
    {
        char *pszName = NULL;
        const char *pszValue = 
            CPLParseNameValue( psWO->papszWarpOptions[iWO], &pszName );
            
        /* EXTRA_ELTS is an internal detail that we will recover */
        /* no need to serialize it */
        if( !EQUAL(pszName, "EXTRA_ELTS") )
        {
            CPLXMLNode *psOption = 
                CPLCreateXMLElementAndValue( 
                    psTree, "Option", pszValue );

            CPLCreateXMLNode( 
                CPLCreateXMLNode( psOption, CXT_Attribute, "name" ),
                CXT_Text, pszName );
        }

        CPLFree(pszName);
    }

/* -------------------------------------------------------------------- */
/*      Source and Destination Data Source                              */
/* -------------------------------------------------------------------- */
    if( psWO->hSrcDS != NULL )
    {
        CPLCreateXMLElementAndValue( 
            psTree, "SourceDataset", 
            GDALGetDescription( psWO->hSrcDS ) );
        
        char** papszOpenOptions = ((GDALDataset*)psWO->hSrcDS)->GetOpenOptions();
        GDALSerializeOpenOptionsToXML(psTree, papszOpenOptions);
    }
    
    if( psWO->hDstDS != NULL && strlen(GDALGetDescription(psWO->hDstDS)) != 0 )
    {
        CPLCreateXMLElementAndValue( 
            psTree, "DestinationDataset", 
            GDALGetDescription( psWO->hDstDS ) );
    }
    
/* -------------------------------------------------------------------- */
/*      Serialize transformer.                                          */
/* -------------------------------------------------------------------- */
    if( psWO->pfnTransformer != NULL )
    {
        CPLXMLNode *psTransformerContainer;
        CPLXMLNode *psTransformerTree;

        psTransformerContainer = 
            CPLCreateXMLNode( psTree, CXT_Element, "Transformer" );

        psTransformerTree = 
            GDALSerializeTransformer( psWO->pfnTransformer,
                                      psWO->pTransformerArg );

        if( psTransformerTree != NULL )
            CPLAddXMLChild( psTransformerContainer, psTransformerTree );
    }

/* -------------------------------------------------------------------- */
/*      Band count and lists.                                           */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psBandList = NULL;
    int i;

    if( psWO->nBandCount != 0 )
        psBandList = CPLCreateXMLNode( psTree, CXT_Element, "BandList" );

    for( i = 0; i < psWO->nBandCount; i++ )
    {
        CPLXMLNode *psBand;

        psBand = CPLCreateXMLNode( psBandList, CXT_Element, "BandMapping" );
        if( psWO->panSrcBands != NULL )
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psBand, CXT_Attribute, "src" ),
                CXT_Text, CPLString().Printf( "%d", psWO->panSrcBands[i] ) );
        if( psWO->panDstBands != NULL )
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psBand, CXT_Attribute, "dst" ),
                CXT_Text, CPLString().Printf( "%d", psWO->panDstBands[i] ) );
        
        if( psWO->padfSrcNoDataReal != NULL )
        {
            if (CPLIsNan(psWO->padfSrcNoDataReal[i]))
                CPLCreateXMLElementAndValue(psBand, "SrcNoDataReal", "nan");
            else
                CPLCreateXMLElementAndValue( 
                    psBand, "SrcNoDataReal", 
                    CPLString().Printf( "%.16g", psWO->padfSrcNoDataReal[i] ) );
        }

        if( psWO->padfSrcNoDataImag != NULL )
        {
            if (CPLIsNan(psWO->padfSrcNoDataImag[i]))
                CPLCreateXMLElementAndValue(psBand, "SrcNoDataImag", "nan");
            else
                CPLCreateXMLElementAndValue( 
                    psBand, "SrcNoDataImag", 
                    CPLString().Printf( "%.16g", psWO->padfSrcNoDataImag[i] ) );
        }

        if( psWO->padfDstNoDataReal != NULL )
        {
            if (CPLIsNan(psWO->padfDstNoDataReal[i]))
                CPLCreateXMLElementAndValue(psBand, "DstNoDataReal", "nan");
            else
                CPLCreateXMLElementAndValue( 
                    psBand, "DstNoDataReal", 
                    CPLString().Printf( "%.16g", psWO->padfDstNoDataReal[i] ) );
        }

        if( psWO->padfDstNoDataImag != NULL )
        {
            if (CPLIsNan(psWO->padfDstNoDataImag[i]))
                CPLCreateXMLElementAndValue(psBand, "DstNoDataImag", "nan");
            else
                CPLCreateXMLElementAndValue( 
                    psBand, "DstNoDataImag", 
                    CPLString().Printf( "%.16g", psWO->padfDstNoDataImag[i] ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Alpha bands.                                                    */
/* -------------------------------------------------------------------- */
    if( psWO->nSrcAlphaBand > 0 )
        CPLCreateXMLElementAndValue( 
            psTree, "SrcAlphaBand", 
            CPLString().Printf( "%d", psWO->nSrcAlphaBand ) );

    if( psWO->nDstAlphaBand > 0 )
        CPLCreateXMLElementAndValue( 
            psTree, "DstAlphaBand", 
            CPLString().Printf( "%d", psWO->nDstAlphaBand ) );

/* -------------------------------------------------------------------- */
/*      Cutline.                                                        */
/* -------------------------------------------------------------------- */
    if( psWO->hCutline != NULL )
    {
        char *pszWKT = NULL;
        if( OGR_G_ExportToWkt( (OGRGeometryH) psWO->hCutline, &pszWKT )
            == OGRERR_NONE )
        {
            CPLCreateXMLElementAndValue( psTree, "Cutline", pszWKT );
            CPLFree( pszWKT );
        }
    }

    if( psWO->dfCutlineBlendDist != 0.0 )
        CPLCreateXMLElementAndValue( 
            psTree, "CutlineBlendDist", 
            CPLString().Printf( "%.5g", psWO->dfCutlineBlendDist ) );

    return psTree;
}

/************************************************************************/
/*                     GDALDeserializeWarpOptions()                     */
/************************************************************************/

GDALWarpOptions * CPL_STDCALL GDALDeserializeWarpOptions( CPLXMLNode *psTree )

{
    CPLErrorReset();

/* -------------------------------------------------------------------- */
/*      Verify this is the right kind of object.                        */
/* -------------------------------------------------------------------- */
    if( psTree == NULL || psTree->eType != CXT_Element
        || !EQUAL(psTree->pszValue,"GDALWarpOptions") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Wrong node, unable to deserialize GDALWarpOptions." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create pre-initialized warp options.                            */
/* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = GDALCreateWarpOptions();

/* -------------------------------------------------------------------- */
/*      Warp memory limit.                                              */
/* -------------------------------------------------------------------- */
    psWO->dfWarpMemoryLimit = 
        CPLAtof(CPLGetXMLValue(psTree,"WarpMemoryLimit","0.0"));

/* -------------------------------------------------------------------- */
/*      resample algorithm                                              */
/* -------------------------------------------------------------------- */
    const char *pszValue = 
        CPLGetXMLValue(psTree,"ResampleAlg","Default");

    if( EQUAL(pszValue,"NearestNeighbour") )
        psWO->eResampleAlg = GRA_NearestNeighbour;
    else if( EQUAL(pszValue,"Bilinear") )
        psWO->eResampleAlg = GRA_Bilinear;
    else if( EQUAL(pszValue,"Cubic") )
        psWO->eResampleAlg = GRA_Cubic;
    else if( EQUAL(pszValue,"CubicSpline") )
        psWO->eResampleAlg = GRA_CubicSpline;
    else if( EQUAL(pszValue,"Lanczos") )
        psWO->eResampleAlg = GRA_Lanczos;
    else if( EQUAL(pszValue,"Average") )
        psWO->eResampleAlg = GRA_Average;
    else if( EQUAL(pszValue,"Mode") )
        psWO->eResampleAlg = GRA_Mode;
    else if( EQUAL(pszValue,"Default") )
        /* leave as is */;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unrecognise ResampleAlg value '%s'.",
                  pszValue );
    }

/* -------------------------------------------------------------------- */
/*      Working data type.                                              */
/* -------------------------------------------------------------------- */
    psWO->eWorkingDataType = 
        GDALGetDataTypeByName(
            CPLGetXMLValue(psTree,"WorkingDataType","Unknown"));

/* -------------------------------------------------------------------- */
/*      Name/value warp options.                                        */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psItem; 

    for( psItem = psTree->psChild; psItem != NULL; psItem = psItem->psNext )
    {
        if( psItem->eType == CXT_Element 
            && EQUAL(psItem->pszValue,"Option") )
        {
            const char *pszName = CPLGetXMLValue(psItem, "Name", NULL );
            const char *pszValue = CPLGetXMLValue(psItem, "", NULL );

            if( pszName != NULL && pszValue != NULL )
            {
                psWO->papszWarpOptions = 
                    CSLSetNameValue( psWO->papszWarpOptions, 
                                     pszName, pszValue );
            }
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Source Dataset.                                                 */
/* -------------------------------------------------------------------- */
    pszValue = CPLGetXMLValue(psTree,"SourceDataset",NULL);

    if( pszValue != NULL )
    {
        char** papszOpenOptions = GDALDeserializeOpenOptionsFromXML(psTree);
        psWO->hSrcDS = GDALOpenEx(
                    pszValue, GDAL_OF_SHARED | GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR, NULL,
                    (const char* const* )papszOpenOptions, NULL );
        CSLDestroy(papszOpenOptions);
    }

/* -------------------------------------------------------------------- */
/*      Destination Dataset.                                            */
/* -------------------------------------------------------------------- */
    pszValue = CPLGetXMLValue(psTree,"DestinationDataset",NULL);

    if( pszValue != NULL )
        psWO->hDstDS = GDALOpenShared( pszValue, GA_Update );

/* -------------------------------------------------------------------- */
/*      First, count band mappings so we can establish the bandcount.   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psBandTree = CPLGetXMLNode( psTree, "BandList" );
    CPLXMLNode *psBand = NULL;

    psWO->nBandCount = 0;
    
    if (psBandTree)
        psBand = psBandTree->psChild;
    else
        psBand = NULL;

    for( ; psBand != NULL; psBand = psBand->psNext )
    {
        if( psBand->eType != CXT_Element 
            || !EQUAL(psBand->pszValue,"BandMapping") )
            continue;

        psWO->nBandCount++;
    }

/* ==================================================================== */
/*      Now actually process each bandmapping.                          */
/* ==================================================================== */
    int iBand = 0;

    if (psBandTree)
        psBand = psBandTree->psChild;
    else
        psBand = NULL;

    for( ; psBand != NULL; psBand = psBand->psNext )
    {
        if( psBand->eType != CXT_Element 
            || !EQUAL(psBand->pszValue,"BandMapping") )
            continue;

/* -------------------------------------------------------------------- */
/*      Source band                                                     */
/* -------------------------------------------------------------------- */
        if( psWO->panSrcBands == NULL )
            psWO->panSrcBands = (int *)CPLMalloc(sizeof(int)*psWO->nBandCount);
        
        pszValue = CPLGetXMLValue(psBand,"src",NULL);
        if( pszValue == NULL )
            psWO->panSrcBands[iBand] = iBand+1;
        else
            psWO->panSrcBands[iBand] = atoi(pszValue);
        
/* -------------------------------------------------------------------- */
/*      Destination band.                                               */
/* -------------------------------------------------------------------- */
        pszValue = CPLGetXMLValue(psBand,"dst",NULL);
        if( pszValue != NULL )
        {
            if( psWO->panDstBands == NULL )
                psWO->panDstBands = 
                    (int *) CPLMalloc(sizeof(int)*psWO->nBandCount);

            psWO->panDstBands[iBand] = atoi(pszValue);
        }
        
/* -------------------------------------------------------------------- */
/*      Source nodata.                                                  */
/* -------------------------------------------------------------------- */
        pszValue = CPLGetXMLValue(psBand,"SrcNoDataReal",NULL);
        if( pszValue != NULL )
        {
            if( psWO->padfSrcNoDataReal == NULL )
                psWO->padfSrcNoDataReal = 
                    (double *) CPLCalloc(sizeof(double),psWO->nBandCount);

            psWO->padfSrcNoDataReal[iBand] = CPLAtof(pszValue);
        }
        
        pszValue = CPLGetXMLValue(psBand,"SrcNoDataImag",NULL);
        if( pszValue != NULL )
        {
            if( psWO->padfSrcNoDataImag == NULL )
                psWO->padfSrcNoDataImag = 
                    (double *) CPLCalloc(sizeof(double),psWO->nBandCount);

            psWO->padfSrcNoDataImag[iBand] = CPLAtof(pszValue);
        }
        
/* -------------------------------------------------------------------- */
/*      Destination nodata.                                             */
/* -------------------------------------------------------------------- */
        pszValue = CPLGetXMLValue(psBand,"DstNoDataReal",NULL);
        if( pszValue != NULL )
        {
            if( psWO->padfDstNoDataReal == NULL )
                psWO->padfDstNoDataReal = 
                    (double *) CPLCalloc(sizeof(double),psWO->nBandCount);

            psWO->padfDstNoDataReal[iBand] = CPLAtof(pszValue);
        }
        
        pszValue = CPLGetXMLValue(psBand,"DstNoDataImag",NULL);
        if( pszValue != NULL )
        {
            if( psWO->padfDstNoDataImag == NULL )
                psWO->padfDstNoDataImag = 
                    (double *) CPLCalloc(sizeof(double),psWO->nBandCount);

            psWO->padfDstNoDataImag[iBand] = CPLAtof(pszValue);
        }
        
        iBand++;
    }

/* -------------------------------------------------------------------- */
/*      Alpha bands.                                                    */
/* -------------------------------------------------------------------- */
    psWO->nSrcAlphaBand = 
        atoi( CPLGetXMLValue( psTree, "SrcAlphaBand", "0" ) );
    psWO->nDstAlphaBand = 
        atoi( CPLGetXMLValue( psTree, "DstAlphaBand", "0" ) );

/* -------------------------------------------------------------------- */
/*      Cutline.                                                        */
/* -------------------------------------------------------------------- */
    const char *pszWKT = CPLGetXMLValue( psTree, "Cutline", NULL );
    if( pszWKT )
    {
        OGR_G_CreateFromWkt( (char **) &pszWKT, NULL, 
                             (OGRGeometryH *) (&psWO->hCutline) );
    }

    psWO->dfCutlineBlendDist =
        CPLAtof( CPLGetXMLValue( psTree, "CutlineBlendDist", "0" ) );

/* -------------------------------------------------------------------- */
/*      Transformation.                                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTransformer = CPLGetXMLNode( psTree, "Transformer" );

    if( psTransformer != NULL && psTransformer->psChild != NULL )
    {
        GDALDeserializeTransformer( psTransformer->psChild, 
                                    &(psWO->pfnTransformer),
                                    &(psWO->pTransformerArg) );
    }

/* -------------------------------------------------------------------- */
/*      If any error has occured, cleanup else return success.          */
/* -------------------------------------------------------------------- */
    if( CPLGetLastErrorNo() != CE_None )
    {
        if ( psWO->pTransformerArg )
        {
            GDALDestroyTransformer( psWO->pTransformerArg );
            psWO->pTransformerArg = NULL;
        }
        if( psWO->hSrcDS != NULL )
        {
            GDALClose( psWO->hSrcDS );
            psWO->hSrcDS = NULL;
        }
        if( psWO->hDstDS != NULL )
        {
            GDALClose( psWO->hDstDS );
            psWO->hDstDS = NULL;
        }
        GDALDestroyWarpOptions( psWO );
        return NULL;
    }
    else
        return psWO;
}
