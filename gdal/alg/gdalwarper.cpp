/******************************************************************************
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implementation of high level convenience APIs for warper.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdalwarper.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"

#if (defined(__x86_64) || defined(_M_X64))
#include <emmintrin.h>
#endif

CPL_CVSID("$Id$")

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
/* -------------------------------------------------------------------- */
/*      Setup a reprojection based transformer.                         */
/* -------------------------------------------------------------------- */
    void *hTransformArg =
        GDALCreateGenImgProjTransformer( hSrcDS, pszSrcWKT, hDstDS, pszDstWKT,
                                         TRUE, 1000.0, 0 );

    if( hTransformArg == nullptr )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Create a copy of the user provided options, or a defaulted      */
/*      options structure.                                              */
/* -------------------------------------------------------------------- */
    GDALWarpOptions *psWOptions =
        psOptions == nullptr
        ? GDALCreateWarpOptions()
        : GDALCloneWarpOptions( psOptions );

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
    psWOptions->hSrcDS = hSrcDS;
    psWOptions->hDstDS = hDstDS;

    int nSrcBands = GDALGetRasterCount(hSrcDS);
    {
        GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, nSrcBands );
        if( hBand && GDALGetRasterColorInterpretation(hBand) == GCI_AlphaBand )
        {
            psWOptions->nSrcAlphaBand = nSrcBands;
            nSrcBands --;
        }
    }

    int nDstBands = GDALGetRasterCount(hDstDS);
    {
        GDALRasterBandH hBand = GDALGetRasterBand( hDstDS, nDstBands );
        if( hBand && GDALGetRasterColorInterpretation(hBand) == GCI_AlphaBand )
        {
            psWOptions->nDstAlphaBand = nDstBands;
            nDstBands --;
        }
    }

    GDALWarpInitDefaultBandMapping(
        psWOptions, std::min(nSrcBands, nDstBands));

/* -------------------------------------------------------------------- */
/*      Set source nodata values if the source dataset seems to have    */
/*      any. Same for target nodata values                              */
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < psWOptions->nBandCount; iBand++ )
    {
        GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, iBand+1 );

        int bGotNoData = FALSE;
        double dfNoDataValue = GDALGetRasterNoDataValue( hBand, &bGotNoData );
        if( bGotNoData )
        {
            GDALWarpInitSrcNoDataReal(psWOptions, -1.1e20);
            psWOptions->padfSrcNoDataReal[iBand] = dfNoDataValue;
        }

        // Deal with target band.
        hBand = GDALGetRasterBand( hDstDS, iBand+1 );

        dfNoDataValue = GDALGetRasterNoDataValue( hBand, &bGotNoData );
        if( bGotNoData )
        {
            GDALWarpInitDstNoDataReal(psWOptions, -1.1e20);
            psWOptions->padfDstNoDataReal[iBand] = dfNoDataValue;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set the progress function.                                      */
/* -------------------------------------------------------------------- */
    if( pfnProgress != nullptr )
    {
        psWOptions->pfnProgress = pfnProgress;
        psWOptions->pProgressArg = pProgressArg;
    }

/* -------------------------------------------------------------------- */
/*      Create a warp options based on the options.                     */
/* -------------------------------------------------------------------- */
    GDALWarpOperation oWarper;
    CPLErr eErr = oWarper.Initialize( psWOptions );

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

/** Reproject an image and create the target reprojected image */
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
    if( hDstDriver == nullptr )
    {
        hDstDriver = GDALGetDriverByName( "GTiff" );
        if (hDstDriver == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GDALCreateAndReprojectImage needs GTiff driver");
            return CE_Failure;
        }
    }

    if( pszSrcWKT == nullptr )
        pszSrcWKT = GDALGetProjectionRef( hSrcDS );

    if( pszDstWKT == nullptr )
        pszDstWKT = pszSrcWKT;

/* -------------------------------------------------------------------- */
/*      Create a transformation object from the source to               */
/*      destination coordinate system.                                  */
/* -------------------------------------------------------------------- */
    void *hTransformArg =
        GDALCreateGenImgProjTransformer( hSrcDS, pszSrcWKT, nullptr, pszDstWKT,
                                         TRUE, 1000.0, 0 );

    if( hTransformArg == nullptr )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Get approximate output definition.                              */
/* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6] = {};
    int nPixels = 0;
    int nLines = 0;

    if( GDALSuggestedWarpOutput( hSrcDS,
                                 GDALGenImgProjTransform, hTransformArg,
                                 adfDstGeoTransform, &nPixels, &nLines )
        != CE_None )
        return CE_Failure;

    GDALDestroyGenImgProjTransformer( hTransformArg );

/* -------------------------------------------------------------------- */
/*      Create the output file.                                         */
/* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS =
        GDALCreate( hDstDriver, pszDstFilename, nPixels, nLines,
                    GDALGetRasterCount(hSrcDS),
                    GDALGetRasterDataType(GDALGetRasterBand(hSrcDS,1)),
                    papszCreateOptions );

    if( hDstDS == nullptr )
        return CE_Failure;

/* -------------------------------------------------------------------- */
/*      Write out the projection definition.                            */
/* -------------------------------------------------------------------- */
    GDALSetProjection( hDstDS, pszDstWKT );
    GDALSetGeoTransform( hDstDS, adfDstGeoTransform );

/* -------------------------------------------------------------------- */
/*      Perform the reprojection.                                       */
/* -------------------------------------------------------------------- */
    CPLErr eErr =
        GDALReprojectImage( hSrcDS, pszSrcWKT, hDstDS, pszDstWKT,
                            eResampleAlg, dfWarpMemoryLimit, dfMaxError,
                            pfnProgress, pProgressArg, psOptions );

    GDALClose( hDstDS );

    return eErr;
}

/************************************************************************/
/*                       GDALWarpNoDataMaskerT()                        */
/************************************************************************/

template<class T> static CPLErr GDALWarpNoDataMaskerT( const double *padfNoData,
                                                       size_t nPixels,
                                                       const T *pData ,
                                                       GUInt32 *panValidityMask,
                                                       int* pbOutAllValid )
{
    // Nothing to do if value is out of range.
    if( padfNoData[0] < std::numeric_limits<T>::min() ||
        padfNoData[0] > std::numeric_limits<T>::max() + 0.000001
        || padfNoData[1] != 0.0 )
    {
        *pbOutAllValid = TRUE;
        return CE_None;
    }

    const int nNoData = static_cast<int>(floor(padfNoData[0] + 0.000001));
    int bAllValid = TRUE;
    for( size_t iOffset = 0; iOffset < nPixels; ++iOffset )
    {
        if( pData[iOffset] == nNoData )
        {
            bAllValid = FALSE;
            panValidityMask[iOffset>>5] &= ~(0x01 << (iOffset & 0x1f));
        }
    }
    *pbOutAllValid = bAllValid;

    return CE_None;
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
                      int bMaskIsFloat, void *pValidityMask,
                      int* pbOutAllValid )

{
    const double *padfNoData = static_cast<double*>(pMaskFuncArg);
    GUInt32 *panValidityMask = static_cast<GUInt32 *>(pValidityMask);
    const size_t nPixels = static_cast<size_t>(nXSize) * nYSize;

    *pbOutAllValid = FALSE;

    if( nBandCount != 1 || bMaskIsFloat )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Invalid nBandCount or bMaskIsFloat argument in SourceNoDataMask");
        return CE_Failure;
    }

    switch( eType )
    {
      case GDT_Byte:
          return
              GDALWarpNoDataMaskerT(
                  padfNoData, nPixels,
                  *ppImageData,  // Already a GByte *.
                  panValidityMask, pbOutAllValid );

      case GDT_Int16:
          return
              GDALWarpNoDataMaskerT(
                   padfNoData, nPixels,
                   reinterpret_cast<GInt16*>(*ppImageData),
                   panValidityMask, pbOutAllValid );

      case GDT_UInt16:
          return GDALWarpNoDataMaskerT(
                      padfNoData, nPixels,
                      reinterpret_cast<GUInt16*>(*ppImageData),
                      panValidityMask, pbOutAllValid );

      case GDT_Float32:
      {
          const float fNoData = static_cast<float>(padfNoData[0]);
          const float *pafData = reinterpret_cast<float *>(*ppImageData);
          const bool bIsNoDataNan = CPL_TO_BOOL(CPLIsNan(fNoData));

          // Nothing to do if value is out of range.
          if( padfNoData[1] != 0.0 )
          {
              *pbOutAllValid = TRUE;
              return CE_None;
          }

          int bAllValid = TRUE;
          for( size_t iOffset = 0; iOffset < nPixels; ++iOffset )
          {
              float fVal = pafData[iOffset];
              if( (bIsNoDataNan && CPLIsNan(fVal)) ||
                  (!bIsNoDataNan && ARE_REAL_EQUAL(fVal, fNoData)) )
              {
                  bAllValid = FALSE;
                  panValidityMask[iOffset>>5] &= ~(0x01 << (iOffset & 0x1f));
              }
          }
          *pbOutAllValid = bAllValid;
      }
      break;

      case GDT_Float64:
      {
          const double dfNoData = padfNoData[0];
          const double *padfData =
              reinterpret_cast<double *>(*ppImageData);
          const bool bIsNoDataNan = CPL_TO_BOOL(CPLIsNan(dfNoData));

          // Nothing to do if value is out of range.
          if( padfNoData[1] != 0.0 )
          {
              *pbOutAllValid = TRUE;
              return CE_None;
          }

          int bAllValid = TRUE;
          for( size_t iOffset = 0; iOffset < nPixels; ++iOffset )
          {
              double dfVal = padfData[iOffset];
              if( (bIsNoDataNan && CPLIsNan(dfVal)) ||
                  (!bIsNoDataNan && ARE_REAL_EQUAL(dfVal, dfNoData)) )
              {
                  bAllValid = FALSE;
                  panValidityMask[iOffset>>5] &= ~(0x01 << (iOffset & 0x1f));
              }
          }
          *pbOutAllValid = bAllValid;
      }
      break;

      default:
      {
          const int nWordSize = GDALGetDataTypeSizeBytes(eType);

          const bool bIsNoDataRealNan = CPL_TO_BOOL(CPLIsNan(padfNoData[0]));

          double *padfWrk = static_cast<double *>(
              CPLMalloc(nXSize * sizeof(double) * 2));
          int bAllValid = TRUE;
          for( int iLine = 0; iLine < nYSize; iLine++ )
          {
              GDALCopyWords( (*ppImageData)+nWordSize*iLine*nXSize,
                             eType, nWordSize,
                             padfWrk, GDT_CFloat64, 16, nXSize );

              for( int iPixel = 0; iPixel < nXSize; ++iPixel )
              {
                  if( ((bIsNoDataRealNan && CPLIsNan(padfWrk[iPixel*2])) ||
                       (!bIsNoDataRealNan &&
                         ARE_REAL_EQUAL(padfWrk[iPixel*2], padfNoData[0]))))
                  {
                      size_t iOffset =
                          iPixel + static_cast<size_t>(iLine) * nXSize;

                      bAllValid = FALSE;
                      panValidityMask[iOffset>>5] &=
                          ~(0x01 << (iOffset & 0x1f));
                  }
              }
          }
          *pbOutAllValid = bAllValid;

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
                        int /* nBandCount */,
                        GDALDataType /* eType */,
                        int nXOff, int nYOff, int nXSize, int nYSize,
                        GByte ** /*ppImageData */,
                        int bMaskIsFloat, void *pValidityMask,
                        int* pbOutAllOpaque )

{
    GDALWarpOptions *psWO = static_cast<GDALWarpOptions *>(pMaskFuncArg);
    float *pafMask = static_cast<float *>(pValidityMask);
    *pbOutAllOpaque = FALSE;
    const size_t nPixels = static_cast<size_t>(nXSize) * nYSize;

/* -------------------------------------------------------------------- */
/*      Do some minimal checking.                                       */
/* -------------------------------------------------------------------- */
    if( !bMaskIsFloat )
    {
        CPLAssert( false );
        return CE_Failure;
    }

    if( psWO == nullptr || psWO->nSrcAlphaBand < 1 )
    {
        CPLAssert( false );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the alpha band.                                            */
/* -------------------------------------------------------------------- */
    GDALRasterBandH hAlphaBand = GDALGetRasterBand( psWO->hSrcDS,
                                                    psWO->nSrcAlphaBand );
    if (hAlphaBand == nullptr)
        return CE_Failure;

    // Rescale.
    const float inv_alpha_max = static_cast<float>(1.0 / CPLAtof(
      CSLFetchNameValueDef( psWO->papszWarpOptions, "SRC_ALPHA_MAX", "255" )));
    bool bOutAllOpaque = true;

    size_t iPixel = 0;
    CPLErr eErr;


#if (defined(__x86_64) || defined(_M_X64))
    GDALDataType eDT = GDALGetRasterDataType(hAlphaBand);
    // Make sure that pafMask is at least 8-byte aligned, which should
    // normally be always the case if being a ptr returned by malloc().
    if( (eDT == GDT_Byte || eDT == GDT_UInt16) && CPL_IS_ALIGNED(pafMask, 8) )
    {
        // Read data.
        eErr = GDALRasterIOEx( hAlphaBand, GF_Read,
                               nXOff, nYOff, nXSize, nYSize,
                               pafMask, nXSize, nYSize, eDT,
                               static_cast<GSpacing>(sizeof(int)),
                               static_cast<GSpacing>(sizeof(int)) * nXSize,
                               nullptr );

        if( eErr != CE_None )
            return eErr;

        // Make sure we have the correct alignment before doing SSE
        // On Linux x86_64, the alignment should be always correct due
        // the alignment of malloc() being 16 byte.
        const GUInt32 mask = (eDT == GDT_Byte) ? 0xff : 0xffff;
        if( !CPL_IS_ALIGNED(pafMask, 16) )
        {
            pafMask[iPixel] = (reinterpret_cast<GUInt32*>(pafMask)[iPixel] & mask) *
                                                    inv_alpha_max;
            if( pafMask[iPixel] >= 1.0f )
                pafMask[iPixel] = 1.0f;
            else
                bOutAllOpaque = false;
            iPixel ++;
        }
        CPLAssert( CPL_IS_ALIGNED(pafMask + iPixel, 16) );
        const __m128 xmm_inverse_alpha_max = _mm_load1_ps(&inv_alpha_max);
        const float one_single = 1.0f;
        const __m128 xmm_one = _mm_load1_ps(&one_single);
        const __m128i xmm_i_mask = _mm_set1_epi32 (mask);
        __m128 xmmMaskNonOpaque0 = _mm_setzero_ps();
        __m128 xmmMaskNonOpaque1 = _mm_setzero_ps();
        __m128 xmmMaskNonOpaque2 = _mm_setzero_ps();
        for( ; iPixel + 6*4-1 < nPixels; iPixel+=6*4 )
        {
            __m128 xmm_mask0 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 0) )) );
            __m128 xmm_mask1 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 1) )) );
            __m128 xmm_mask2 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 2) )) );
            __m128 xmm_mask3 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 3) )) );
            __m128 xmm_mask4 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 4) )) );
            __m128 xmm_mask5 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 5) )) );
            xmm_mask0 = _mm_mul_ps(xmm_mask0, xmm_inverse_alpha_max);
            xmm_mask1 = _mm_mul_ps(xmm_mask1, xmm_inverse_alpha_max);
            xmm_mask2 = _mm_mul_ps(xmm_mask2, xmm_inverse_alpha_max);
            xmm_mask3 = _mm_mul_ps(xmm_mask3, xmm_inverse_alpha_max);
            xmm_mask4 = _mm_mul_ps(xmm_mask4, xmm_inverse_alpha_max);
            xmm_mask5 = _mm_mul_ps(xmm_mask5, xmm_inverse_alpha_max);
            xmmMaskNonOpaque0 =
                _mm_or_ps(xmmMaskNonOpaque0, _mm_cmplt_ps(xmm_mask0, xmm_one));
            xmmMaskNonOpaque1 =
                _mm_or_ps(xmmMaskNonOpaque1, _mm_cmplt_ps(xmm_mask1, xmm_one));
            xmmMaskNonOpaque2 =
                _mm_or_ps(xmmMaskNonOpaque2, _mm_cmplt_ps(xmm_mask2, xmm_one));
            xmmMaskNonOpaque0 =
                _mm_or_ps(xmmMaskNonOpaque0, _mm_cmplt_ps(xmm_mask3, xmm_one));
            xmmMaskNonOpaque1 =
                _mm_or_ps(xmmMaskNonOpaque1, _mm_cmplt_ps(xmm_mask4, xmm_one));
            xmmMaskNonOpaque2 =
                _mm_or_ps(xmmMaskNonOpaque2, _mm_cmplt_ps(xmm_mask5, xmm_one));
            xmm_mask0 = _mm_min_ps(xmm_mask0, xmm_one);
            xmm_mask1 = _mm_min_ps(xmm_mask1, xmm_one);
            xmm_mask2 = _mm_min_ps(xmm_mask2, xmm_one);
            xmm_mask3 = _mm_min_ps(xmm_mask3, xmm_one);
            xmm_mask4 = _mm_min_ps(xmm_mask4, xmm_one);
            xmm_mask5 = _mm_min_ps(xmm_mask5, xmm_one);
            _mm_store_ps(pafMask + iPixel + 4 * 0, xmm_mask0);
            _mm_store_ps(pafMask + iPixel + 4 * 1, xmm_mask1);
            _mm_store_ps(pafMask + iPixel + 4 * 2, xmm_mask2);
            _mm_store_ps(pafMask + iPixel + 4 * 3, xmm_mask3);
            _mm_store_ps(pafMask + iPixel + 4 * 4, xmm_mask4);
            _mm_store_ps(pafMask + iPixel + 4 * 5, xmm_mask5);
        }
        if( _mm_movemask_ps(_mm_or_ps(_mm_or_ps(
                  xmmMaskNonOpaque0, xmmMaskNonOpaque1), xmmMaskNonOpaque2))  )
        {
            bOutAllOpaque = false;
        }
        for(; iPixel < nPixels; iPixel++ )
        {
            pafMask[iPixel] = (reinterpret_cast<GUInt32*>(pafMask)[iPixel] & mask) *
                                                      inv_alpha_max;
            if( pafMask[iPixel] >= 1.0f )
                pafMask[iPixel] = 1.0f;
            else
                bOutAllOpaque = false;
        }
    }
    else
#endif
    {
        // Read data.
        eErr = GDALRasterIO( hAlphaBand, GF_Read, nXOff, nYOff, nXSize, nYSize,
                             pafMask, nXSize, nYSize, GDT_Float32, 0, 0 );

        if( eErr != CE_None )
            return eErr;

        // TODO(rouault): Is loop unrolling by hand (r34564) actually helpful?
        for( ; iPixel + 3 < nPixels; iPixel += 4 )
        {
            pafMask[iPixel] = pafMask[iPixel] * inv_alpha_max;
            if( pafMask[iPixel] >= 1.0f )
                pafMask[iPixel] = 1.0f;
            else
                bOutAllOpaque = false;
            pafMask[iPixel+1] = pafMask[iPixel+1] * inv_alpha_max;
            if( pafMask[iPixel+1] >= 1.0f )
                pafMask[iPixel+1] = 1.0f;
            else
                bOutAllOpaque = false;
            pafMask[iPixel+2] = pafMask[iPixel+2] * inv_alpha_max;
            if( pafMask[iPixel+2] >= 1.0f )
                pafMask[iPixel+2] = 1.0f;
            else
                bOutAllOpaque = false;
            pafMask[iPixel+3] = pafMask[iPixel+3] * inv_alpha_max;
            if( pafMask[iPixel+3] >= 1.0f )
                pafMask[iPixel+3] = 1.0f;
            else
                bOutAllOpaque = false;
        }

        for( ; iPixel < nPixels; iPixel++ )
        {
            pafMask[iPixel] = pafMask[iPixel] * inv_alpha_max;
            if( pafMask[iPixel] >= 1.0f )
                pafMask[iPixel] = 1.0f;
            else
                bOutAllOpaque = false;
        }
    }

    *pbOutAllOpaque = bOutAllOpaque;

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
                       int /* nBandCount */,
                       GDALDataType /* eType */,
                       int nXOff, int nYOff, int nXSize, int nYSize,
                       GByte ** /*ppImageData */,
                       int bMaskIsFloat, void *pValidityMask )

{
    GDALWarpOptions *psWO = static_cast<GDALWarpOptions *>(pMaskFuncArg);
    GUInt32  *panMask = static_cast<GUInt32 *>(pValidityMask);

/* -------------------------------------------------------------------- */
/*      Do some minimal checking.                                       */
/* -------------------------------------------------------------------- */
    if( bMaskIsFloat )
    {
        CPLAssert( false );
        return CE_Failure;
    }

    if( psWO == nullptr )
    {
        CPLAssert( false );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate a temporary buffer to read mask byte data into.        */
/* -------------------------------------------------------------------- */
    GByte *pabySrcMask = static_cast<GByte *>(
        VSI_MALLOC2_VERBOSE(nXSize,nYSize));
    if( pabySrcMask == nullptr )
    {
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Fetch our mask band.                                            */
/* -------------------------------------------------------------------- */
    GDALRasterBandH hMaskBand = nullptr;
    GDALRasterBandH hSrcBand =
        GDALGetRasterBand( psWO->hSrcDS, psWO->panSrcBands[0] );
    if( hSrcBand != nullptr )
        hMaskBand = GDALGetMaskBand( hSrcBand );

    if( hMaskBand == nullptr )
    {
        CPLAssert( false );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Read the mask band.                                             */
/* -------------------------------------------------------------------- */
    CPLErr eErr =
        GDALRasterIO( hMaskBand, GF_Read, nXOff, nYOff, nXSize, nYSize,
                      pabySrcMask, nXSize, nYSize, GDT_Byte, 0, 0 );

    if( eErr != CE_None )
    {
        CPLFree( pabySrcMask );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Pack into 1 bit per pixel for validity.                         */
/* -------------------------------------------------------------------- */
    const size_t nPixels = static_cast<size_t>(nXSize) * nYSize;
    for( size_t iPixel = 0; iPixel < nPixels; iPixel++ )
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
                        CPL_UNUSED GDALDataType /* eType */,
                        int nXOff, int nYOff, int nXSize, int nYSize,
                        GByte ** /*ppImageData */,
                        int bMaskIsFloat, void *pValidityMask )
{
/* -------------------------------------------------------------------- */
/*      Do some minimal checking.                                       */
/* -------------------------------------------------------------------- */
    if( !bMaskIsFloat )
    {
        CPLAssert( false );
        return CE_Failure;
    }

    GDALWarpOptions *psWO = static_cast<GDALWarpOptions *>(pMaskFuncArg);
    if( psWO == nullptr || psWO->nDstAlphaBand < 1 )
    {
        CPLAssert( false );
        return CE_Failure;
    }

    float *pafMask = static_cast<float *>(pValidityMask);
    const size_t nPixels = static_cast<size_t>(nXSize) * nYSize;

    GDALRasterBandH hAlphaBand =
        GDALGetRasterBand( psWO->hDstDS, psWO->nDstAlphaBand );
    if (hAlphaBand == nullptr)
        return CE_Failure;

    size_t iPixel = 0;

/* -------------------------------------------------------------------- */
/*      Read alpha case.                                                */
/* -------------------------------------------------------------------- */
    if( nBandCount >= 0 )
    {
        const char *pszInitDest =
            CSLFetchNameValue( psWO->papszWarpOptions, "INIT_DEST" );

        // Special logic for destinations being initialized on-the-fly.
        if( pszInitDest != nullptr )
        {
            memset( pafMask, 0, nPixels * sizeof(float) );
            return CE_None;
        }

        // Rescale.
        const float inv_alpha_max =  static_cast<float>(1.0 / CPLAtof(
            CSLFetchNameValueDef( psWO->papszWarpOptions, "DST_ALPHA_MAX",
                                  "255" ) ));

#if (defined(__x86_64) || defined(_M_X64))
        const GDALDataType eDT = GDALGetRasterDataType(hAlphaBand);
        // Make sure that pafMask is at least 8-byte aligned, which should
        // normally be always the case if being a ptr returned by malloc().
        if( (eDT == GDT_Byte || eDT == GDT_UInt16) &&
            CPL_IS_ALIGNED(pafMask, 8) )
        {
            // Read data.
            const CPLErr eErr =
                GDALRasterIOEx( hAlphaBand, GF_Read,
                                nXOff, nYOff, nXSize, nYSize,
                                pafMask, nXSize, nYSize, eDT,
                                static_cast<GSpacing>(sizeof(int)),
                                static_cast<GSpacing>(sizeof(int)) * nXSize,
                                nullptr );

            if( eErr != CE_None )
                return eErr;

            // Make sure we have the correct alignment before doing SSE
            // On Linux x86_64, the alignment should be always correct due
            // the alignment of malloc() being 16 byte.
            const GUInt32 mask = (eDT == GDT_Byte) ? 0xff : 0xffff;
            if( !CPL_IS_ALIGNED(pafMask, 16) )
            {
                pafMask[iPixel] = (reinterpret_cast<GUInt32*>(pafMask)[iPixel] & mask) *
                                                          inv_alpha_max;
                pafMask[iPixel] = std::min( 1.0f, pafMask[iPixel] );
                iPixel ++;
            }
            CPLAssert( CPL_IS_ALIGNED(pafMask + iPixel, 16) );
            const __m128 xmm_inverse_alpha_max =
                                        _mm_load1_ps(&inv_alpha_max);
            const float one_single = 1.0f;
            const __m128 xmm_one = _mm_load1_ps(&one_single);
            const __m128i xmm_i_mask = _mm_set1_epi32 (mask);
            for( ; iPixel + 31 < nPixels; iPixel+=32 )
            {
                __m128 xmm_mask0 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 0) )) );
                __m128 xmm_mask1 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 1) )) );
                __m128 xmm_mask2 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 2) )) );
                __m128 xmm_mask3 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 3) )) );
                __m128 xmm_mask4 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 4) )) );
                __m128 xmm_mask5 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 5) )) );
                __m128 xmm_mask6 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 6) )) );
                __m128 xmm_mask7 = _mm_cvtepi32_ps( _mm_and_si128(xmm_i_mask,
                    _mm_load_si128( reinterpret_cast<__m128i *>(pafMask + iPixel + 4 * 7) )) );
                xmm_mask0 = _mm_mul_ps(xmm_mask0, xmm_inverse_alpha_max);
                xmm_mask1 = _mm_mul_ps(xmm_mask1, xmm_inverse_alpha_max);
                xmm_mask2 = _mm_mul_ps(xmm_mask2, xmm_inverse_alpha_max);
                xmm_mask3 = _mm_mul_ps(xmm_mask3, xmm_inverse_alpha_max);
                xmm_mask4 = _mm_mul_ps(xmm_mask4, xmm_inverse_alpha_max);
                xmm_mask5 = _mm_mul_ps(xmm_mask5, xmm_inverse_alpha_max);
                xmm_mask6 = _mm_mul_ps(xmm_mask6, xmm_inverse_alpha_max);
                xmm_mask7 = _mm_mul_ps(xmm_mask7, xmm_inverse_alpha_max);
                xmm_mask0 = _mm_min_ps(xmm_mask0, xmm_one);
                xmm_mask1 = _mm_min_ps(xmm_mask1, xmm_one);
                xmm_mask2 = _mm_min_ps(xmm_mask2, xmm_one);
                xmm_mask3 = _mm_min_ps(xmm_mask3, xmm_one);
                xmm_mask4 = _mm_min_ps(xmm_mask4, xmm_one);
                xmm_mask5 = _mm_min_ps(xmm_mask5, xmm_one);
                xmm_mask6 = _mm_min_ps(xmm_mask6, xmm_one);
                xmm_mask7 = _mm_min_ps(xmm_mask7, xmm_one);
                _mm_store_ps(pafMask + iPixel + 4 * 0, xmm_mask0);
                _mm_store_ps(pafMask + iPixel + 4 * 1, xmm_mask1);
                _mm_store_ps(pafMask + iPixel + 4 * 2, xmm_mask2);
                _mm_store_ps(pafMask + iPixel + 4 * 3, xmm_mask3);
                _mm_store_ps(pafMask + iPixel + 4 * 4, xmm_mask4);
                _mm_store_ps(pafMask + iPixel + 4 * 5, xmm_mask5);
                _mm_store_ps(pafMask + iPixel + 4 * 6, xmm_mask6);
                _mm_store_ps(pafMask + iPixel + 4 * 7, xmm_mask7);
            }
            for(; iPixel < nPixels; iPixel++ )
            {
                pafMask[iPixel] = (reinterpret_cast<GUInt32*>(pafMask)[iPixel] & mask) *
                                                        inv_alpha_max;
                pafMask[iPixel] = std::min( 1.0f, pafMask[iPixel] );
            }
        }
        else
#endif
        {
            // Read data.
            const CPLErr eErr =
                GDALRasterIO( hAlphaBand, GF_Read, nXOff, nYOff, nXSize, nYSize,
                              pafMask, nXSize, nYSize, GDT_Float32, 0, 0 );

            if( eErr != CE_None )
                return eErr;

            for(; iPixel < nPixels; iPixel++ )
            {
                pafMask[iPixel] = pafMask[iPixel] * inv_alpha_max;
                pafMask[iPixel] = std::min(1.0f, pafMask[iPixel]);
            }
        }

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Write alpha case.                                               */
/* -------------------------------------------------------------------- */
    else
    {
        GDALDataType eDT = GDALGetRasterDataType(hAlphaBand);
        const float cst_alpha_max = static_cast<float>(CPLAtof(
            CSLFetchNameValueDef( psWO->papszWarpOptions, "DST_ALPHA_MAX",
                                  "255" ) )) +
            (( eDT == GDT_Byte || eDT == GDT_Int16 || eDT == GDT_UInt16 ||
               eDT == GDT_Int32 || eDT == GDT_UInt32 ) ?
                0.1f : 0.0f);

        CPLErr eErr = CE_None;

#if (defined(__x86_64) || defined(_M_X64))
        // Make sure that pafMask is at least 8-byte aligned, which should
        // normally be always the case if being a ptr returned by malloc()
        if( (eDT == GDT_Byte || eDT == GDT_Int16 || eDT == GDT_UInt16) &&
            CPL_IS_ALIGNED(pafMask, 8) )
        {
            // Make sure we have the correct alignment before doing SSE
            // On Linux x86_64, the alignment should be always correct due
            // the alignment of malloc() being 16 byte
            if( !CPL_IS_ALIGNED(pafMask, 16) )
            {
                reinterpret_cast<int*>(pafMask)[iPixel] =
                    static_cast<int>(pafMask[iPixel] * cst_alpha_max);
                iPixel++;
            }
            CPLAssert( CPL_IS_ALIGNED(pafMask + iPixel, 16) );
            const __m128 xmm_alpha_max = _mm_load1_ps(&cst_alpha_max);
            for( ; iPixel + 31 < nPixels; iPixel += 32 )
            {
                __m128 xmm_mask0 = _mm_load_ps(pafMask + iPixel + 4 * 0);
                __m128 xmm_mask1 = _mm_load_ps(pafMask + iPixel + 4 * 1);
                __m128 xmm_mask2 = _mm_load_ps(pafMask + iPixel + 4 * 2);
                __m128 xmm_mask3 = _mm_load_ps(pafMask + iPixel + 4 * 3);
                __m128 xmm_mask4 = _mm_load_ps(pafMask + iPixel + 4 * 4);
                __m128 xmm_mask5 = _mm_load_ps(pafMask + iPixel + 4 * 5);
                __m128 xmm_mask6 = _mm_load_ps(pafMask + iPixel + 4 * 6);
                __m128 xmm_mask7 = _mm_load_ps(pafMask + iPixel + 4 * 7);
                xmm_mask0 = _mm_mul_ps(xmm_mask0, xmm_alpha_max);
                xmm_mask1 = _mm_mul_ps(xmm_mask1, xmm_alpha_max);
                xmm_mask2 = _mm_mul_ps(xmm_mask2, xmm_alpha_max);
                xmm_mask3 = _mm_mul_ps(xmm_mask3, xmm_alpha_max);
                xmm_mask4 = _mm_mul_ps(xmm_mask4, xmm_alpha_max);
                xmm_mask5 = _mm_mul_ps(xmm_mask5, xmm_alpha_max);
                xmm_mask6 = _mm_mul_ps(xmm_mask6, xmm_alpha_max);
                xmm_mask7 = _mm_mul_ps(xmm_mask7, xmm_alpha_max);
                 // Truncate to int.
                _mm_store_si128(reinterpret_cast<__m128i*>(pafMask + iPixel + 4 * 0),
                                _mm_cvttps_epi32(xmm_mask0));
                _mm_store_si128(reinterpret_cast<__m128i*>(pafMask + iPixel + 4 * 1),
                                _mm_cvttps_epi32(xmm_mask1));
                _mm_store_si128(reinterpret_cast<__m128i*>(pafMask + iPixel + 4 * 2),
                                _mm_cvttps_epi32(xmm_mask2));
                _mm_store_si128(reinterpret_cast<__m128i*>(pafMask + iPixel + 4 * 3),
                                _mm_cvttps_epi32(xmm_mask3));
                _mm_store_si128(reinterpret_cast<__m128i*>(pafMask + iPixel + 4 * 4),
                                _mm_cvttps_epi32(xmm_mask4));
                _mm_store_si128(reinterpret_cast<__m128i*>(pafMask + iPixel + 4 * 5),
                                _mm_cvttps_epi32(xmm_mask5));
                _mm_store_si128(reinterpret_cast<__m128i*>(pafMask + iPixel + 4 * 6),
                                _mm_cvttps_epi32(xmm_mask6));
                _mm_store_si128(reinterpret_cast<__m128i*>(pafMask + iPixel + 4 * 7),
                                _mm_cvttps_epi32(xmm_mask7));
            }
            for( ; iPixel < nPixels; iPixel++ )
                reinterpret_cast<int*>(pafMask)[iPixel] =
                  static_cast<int>(pafMask[iPixel] * cst_alpha_max);

            // Write data.
            // Assumes little endianness here.
            eErr = GDALRasterIOEx( hAlphaBand, GF_Write,
                                   nXOff, nYOff, nXSize, nYSize,
                                   pafMask, nXSize, nYSize, eDT,
                                   static_cast<GSpacing>(sizeof(int)),
                                   static_cast<GSpacing>(sizeof(int)) * nXSize,
                                   nullptr );
        }
        else
#endif
        {
            for( ; iPixel + 3 < nPixels; iPixel+=4 )
            {
                pafMask[iPixel+0] = static_cast<float>(
                    static_cast<int>( pafMask[iPixel+0] * cst_alpha_max ));
                pafMask[iPixel+1] = static_cast<float>(
                    static_cast<int>( pafMask[iPixel+1] * cst_alpha_max ));
                pafMask[iPixel+2] = static_cast<float>(
                    static_cast<int>( pafMask[iPixel+2] * cst_alpha_max ));
                pafMask[iPixel+3] = static_cast<float>(
                    static_cast<int>( pafMask[iPixel+3] * cst_alpha_max ));
            }
            for( ; iPixel < nPixels; iPixel++ )
                pafMask[iPixel] = static_cast<float>(
                    static_cast<int>(pafMask[iPixel] * cst_alpha_max ));

            // Write data.

            eErr = GDALRasterIO( hAlphaBand, GF_Write,
                                nXOff, nYOff, nXSize, nYSize,
                                pafMask, nXSize, nYSize, GDT_Float32,
                                0, 0 );
        }
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
 * <ul>
 * <li>INIT_DEST=[value] or INIT_DEST=NO_DATA: This option forces the
 * destination image to be initialized to the indicated value (for all bands)
 * or indicates that it should be initialized to the NO_DATA value in
 * padfDstNoDataReal/padfDstNoDataImag.  If this value isn't set the
 * destination image will be read and overlaid.</li>
 *
 * <li>WRITE_FLUSH=YES/NO: This option forces a flush to disk of data after
 * each chunk is processed.  In some cases this helps ensure a serial
 * writing of the output data otherwise a block of data may be written to disk
 * each time a block of data is read for the input buffer resulting in a lot
 * of extra seeking around the disk, and reduced IO throughput.  The default
 * at this time is NO.</li>
 *
 * <li>SKIP_NOSOURCE=YES/NO: Skip all processing for chunks for which there
 * is no corresponding input data.  This will disable initializing the
 * destination (INIT_DEST) and all other processing, and so should be used
 * carefully.  Mostly useful to short circuit a lot of extra work in mosaicing
 * situations. Starting with GDAL 2.4, gdalwarp will automatically enable this
 * option when it is assumed to be safe to do so.</li>
 *
 * <li>UNIFIED_SRC_NODATA=YES/NO/PARTIAL: This setting determines
 * how to take into account nodata values when there are several input bands.
 * <ul>
 * <li>When YES, all bands are considered as nodata if and only if, all bands
 *     match the corresponding nodata values.
 *     Note: UNIFIED_SRC_NODATA=YES is set by default, when called from gdalwarp /
 *     GDALWarp() with an explicit -srcnodata setting.
 *
 *     Example with nodata values at (1, 2, 3) and target alpha band requested.
 *     <ul>
 *     <li>input pixel = (1, 2, 3) ==> output pixel = (0, 0, 0, 0)</li>
 *     <li>input pixel = (1, 2, 127) ==> output pixel = (1, 2, 127, 255)</li>
 *     </ul>
 * </li>
 * <li>When NO, nodata masking values is considered independently for each band.
 *     A potential target alpha band will always be valid if there are multiple
 *     bands.
 *
 *     Example with nodata values at (1, 2, 3) and target alpha band requested.
 *     <ul>
 *     <li>input pixel = (1, 2, 3) ==> output pixel = (0, 0, 0, 255)</li>
 *     <li>input pixel = (1, 2, 127) ==> output pixel = (0, 0, 127, 255)</li>
 *     </ul>
 *
 *     Note: NO was the default behaviour before GDAL 3.3.2
 * </li>
 * <li>When PARTIAL, or not specified at all (default behavior),
 *     nodata masking values is considered independently for each band.
 *     But, and this is the difference with NO, if for a given pixel, it
 *     evaluates to the nodata value of each band, the target pixel is
 *     considered as globally invalid, which impacts the value of a potential
 *     target alpha band.
 *
 *     Note: PARTIAL is new to GDAL 3.3.2 and should not be used with
 *     earlier versions. The default behavior of GDAL < 3.3.2 was NO.
 *
 *     Example with nodata values at (1, 2, 3) and target alpha band requested.
 *     <ul>
 *     <li>input pixel = (1, 2, 3) ==> output pixel = (0, 0, 0, 0)</li>
 *     <li>input pixel = (1, 2, 127) ==> output pixel = (0, 0, 127, 255)</li>
 *     </ul>
 * </li>
 * </ul>
 * </li>
 *
 * <li>CUTLINE: This may contain the WKT geometry for a cutline.  It will
 * be converted into a geometry by GDALWarpOperation::Initialize() and assigned
 * to the GDALWarpOptions hCutline field. The coordinates must be expressed
 * in source pixel/line coordinates. Note: this is different from the assumptions
 * made for the -cutline option of the gdalwarp utility !</li>
 *
 * <li>CUTLINE_BLEND_DIST: This may be set with a distance in pixels which
 * will be assigned to the dfCutlineBlendDist field in the GDALWarpOptions.</li>
 *
 * <li>CUTLINE_ALL_TOUCHED: This defaults to FALSE, but may be set to TRUE
 * to enable ALL_TOUCHEd mode when rasterizing cutline polygons.  This is
 * useful to ensure that that all pixels overlapping the cutline polygon
 * will be selected, not just those whose center point falls within the
 * polygon.</li>
 *
 * <li>OPTIMIZE_SIZE: This defaults to FALSE, but may be set to TRUE
 * typically when writing to a compressed dataset (GeoTIFF with
 * COMPRESSED creation option set for example) for achieving a smaller
 * file size. This is achieved by writing at once data aligned on full
 * blocks of the target dataset, which avoids partial writes of
 * compressed blocks and lost space when they are rewritten at the end
 * of the file. However sticking to target block size may cause major
 * processing slowdown for some particular reprojections.</li>
 *
 * <li>NUM_THREADS: (GDAL >= 1.10) Can be set to a numeric value or ALL_CPUS to
 * set the number of threads to use to parallelize the computation part of the
 * warping. If not set, computation will be done in a single thread.</li>
 *
 * <li>STREAMABLE_OUTPUT: (GDAL >= 2.0) This defaults to FALSE, but may
 * be set to TRUE typically when writing to a streamed file. The
 * gdalwarp utility automatically sets this option when writing to
 * /vsistdout/ or a named pipe (on Unix).  This option has performance
 * impacts for some reprojections.  Note: band interleaved output is
 * not currently supported by the warping algorithm in a streamable
 * compatible way.</li>
 *
 * <li>SRC_COORD_PRECISION: (GDAL >= 2.0). Advanced setting. This
 * defaults to 0, to indicate that no rounding of computing source
 * image coordinates corresponding to the target image must be
 * done. If greater than 0 (and typically below 1), this value,
 * expressed in pixel, will be used to round computed source image
 * coordinates. The purpose of this option is to make the results of
 * warping with the approximated transformer more reproducible and not
 * sensitive to changes in warping memory size. To achieve that,
 * SRC_COORD_PRECISION must be at least 10 times greater than the
 * error threshold. The higher the SRC_COORD_PRECISION/error_threshold
 * ratio, the higher the performance will be, since exact
 * reprojections must statistically be done with a frequency of
 * 4*error_threshold/SRC_COORD_PRECISION.</li>
 *
 * <li>SRC_ALPHA_MAX: (GDAL >= 2.2). Maximum value for the alpha band of the
 * source dataset. If the value is not set and the alpha band has a NBITS
 * metadata item, it is used to set SRC_ALPHA_MAX = 2^NBITS-1. Otherwise, if the
 * value is not set and the alpha band is of type UInt16 (resp Int16), 65535
 * (resp 32767) is used. Otherwise, 255 is used.</li>
 *
 * <li>DST_ALPHA_MAX: (GDAL >= 2.2). Maximum value for the alpha band of the
 * destination dataset. If the value is not set and the alpha band has a NBITS
 * metadata item, it is used to set DST_ALPHA_MAX = 2^NBITS-1. Otherwise, if the
 * value is not set and the alpha band is of type UInt16 (resp Int16), 65535
 * (resp 32767) is used. Otherwise, 255 is used.</li>
 * </ul>
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
 * <ul>
 *
 * <li>SAMPLE_GRID=YES/NO: Setting this option to YES will force the sampling to
 * include internal points as well as edge points which can be important if
 * the transformation is esoteric inside out, or if large sections of the
 * destination image are not transformable into the source coordinate system.</li>
 *
 * <li>SAMPLE_STEPS: Modifies the density of the sampling grid.  The default
 * number of steps is 21.   Increasing this can increase the computational
 * cost, but improves the accuracy with which the source region is computed.</li>
 *
 * <li>SOURCE_EXTRA: This is a number of extra pixels added around the source
 * window for a given request, and by default it is 1 to take care of rounding
 * error.  Setting this larger will increase the amount of data that needs to
 * be read, but can avoid missing source data.</li>
 * </ul>
 */

/************************************************************************/
/*                       GDALCreateWarpOptions()                        */
/************************************************************************/

/** Create a warp options structure.
 *
 * Must be deallocated with GDALDestroyWarpOptions()
 */
GDALWarpOptions * CPL_STDCALL GDALCreateWarpOptions()

{
    GDALWarpOptions *psOptions = static_cast<GDALWarpOptions *>(
        CPLCalloc(sizeof(GDALWarpOptions), 1));

    psOptions->nBandCount = 0;
    psOptions->eResampleAlg = GRA_NearestNeighbour;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->eWorkingDataType = GDT_Unknown;

    return psOptions;
}

/************************************************************************/
/*                       GDALDestroyWarpOptions()                       */
/************************************************************************/

/** Destroy a warp options structure. */
void CPL_STDCALL GDALDestroyWarpOptions( GDALWarpOptions *psOptions )

{
    if( psOptions == nullptr )
        return;

    CSLDestroy( psOptions->papszWarpOptions );
    CPLFree( psOptions->panSrcBands );
    CPLFree( psOptions->panDstBands );
    CPLFree( psOptions->padfSrcNoDataReal );
    CPLFree( psOptions->padfSrcNoDataImag );
    CPLFree( psOptions->padfDstNoDataReal );
    CPLFree( psOptions->padfDstNoDataImag );
    CPLFree( psOptions->papfnSrcPerBandValidityMaskFunc );
    CPLFree( psOptions->papSrcPerBandValidityMaskFuncArg );

    if( psOptions->hCutline != nullptr )
        OGR_G_DestroyGeometry( reinterpret_cast<OGRGeometryH>(psOptions->hCutline) );

    CPLFree( psOptions );
}

#define COPY_MEM(target,type,count)                                     \
   do { if( (psSrcOptions->target) != nullptr && (count) != 0 )            \
   {                                                                    \
       (psDstOptions->target) = static_cast<type *>(CPLMalloc(sizeof(type)*(count))); \
       memcpy( (psDstOptions->target), (psSrcOptions->target),          \
               sizeof(type) * (count) );                                \
   } \
   else \
       (psDstOptions->target) = nullptr; } while( false )

/************************************************************************/
/*                        GDALCloneWarpOptions()                        */
/************************************************************************/

/** Clone a warp options structure.
 *
 * Must be deallocated with GDALDestroyWarpOptions()
 */
GDALWarpOptions * CPL_STDCALL
GDALCloneWarpOptions( const GDALWarpOptions *psSrcOptions )

{
    GDALWarpOptions *psDstOptions = GDALCreateWarpOptions();

    memcpy( psDstOptions, psSrcOptions, sizeof(GDALWarpOptions) );

    if( psSrcOptions->papszWarpOptions != nullptr )
        psDstOptions->papszWarpOptions =
            CSLDuplicate( psSrcOptions->papszWarpOptions );

    COPY_MEM( panSrcBands, int, psSrcOptions->nBandCount );
    COPY_MEM( panDstBands, int, psSrcOptions->nBandCount );
    COPY_MEM( padfSrcNoDataReal, double, psSrcOptions->nBandCount );
    COPY_MEM( padfSrcNoDataImag, double, psSrcOptions->nBandCount );
    COPY_MEM( padfDstNoDataReal, double, psSrcOptions->nBandCount );
    COPY_MEM( padfDstNoDataImag, double, psSrcOptions->nBandCount );
    // cppcheck-suppress pointerSize
    COPY_MEM( papfnSrcPerBandValidityMaskFunc, GDALMaskFunc,
              psSrcOptions->nBandCount );
    psDstOptions->papSrcPerBandValidityMaskFuncArg = nullptr;

    if( psSrcOptions->hCutline != nullptr )
        psDstOptions->hCutline =
            OGR_G_Clone( reinterpret_cast<OGRGeometryH>(psSrcOptions->hCutline) );
    psDstOptions->dfCutlineBlendDist = psSrcOptions->dfCutlineBlendDist;

    return psDstOptions;
}

namespace
{
    void InitNoData(int nBandCount, double ** ppdNoDataReal, double dDataReal)
    {
        if( nBandCount <= 0 ) { return; }
        if( *ppdNoDataReal != nullptr ) { return; }

        *ppdNoDataReal = static_cast<double *>(
            CPLMalloc(sizeof(double) * nBandCount));

        for( int i = 0; i < nBandCount; ++i)
        {
            (*ppdNoDataReal)[i] = dDataReal;
        }
    }
}


/************************************************************************/
/*                      GDALWarpInitDstNoDataReal()                     */
/************************************************************************/

/**
 * \brief Initialize padfDstNoDataReal with specified value.
 *
 * @param psOptionsIn options to initialize.
 * @param dNoDataReal value to initialize to.
 *
 */
void CPL_STDCALL
GDALWarpInitDstNoDataReal( GDALWarpOptions * psOptionsIn, double dNoDataReal )
{
    VALIDATE_POINTER0(psOptionsIn, "GDALWarpInitDstNoDataReal");
    InitNoData(
        psOptionsIn->nBandCount, &psOptionsIn->padfDstNoDataReal, dNoDataReal);
}


/************************************************************************/
/*                      GDALWarpInitSrcNoDataReal()                     */
/************************************************************************/

/**
 * \brief Initialize padfSrcNoDataReal with specified value.
 *
 * @param psOptionsIn options to initialize.
 * @param dNoDataReal value to initialize to.
 *
 */
void CPL_STDCALL
GDALWarpInitSrcNoDataReal( GDALWarpOptions * psOptionsIn, double dNoDataReal )
{
    VALIDATE_POINTER0(psOptionsIn, "GDALWarpInitSrcNoDataReal");
    InitNoData(
        psOptionsIn->nBandCount, &psOptionsIn->padfSrcNoDataReal, dNoDataReal);
}


/************************************************************************/
/*                      GDALWarpInitNoDataReal()                        */
/************************************************************************/

/**
 * \brief Initialize padfSrcNoDataReal and padfDstNoDataReal with specified value.
 *
 * @param psOptionsIn options to initialize.
 * @param dNoDataReal value to initialize to.
 *
 */
void CPL_STDCALL
GDALWarpInitNoDataReal(GDALWarpOptions * psOptionsIn, double  dNoDataReal)
{
    GDALWarpInitDstNoDataReal(psOptionsIn, dNoDataReal);
    GDALWarpInitSrcNoDataReal(psOptionsIn, dNoDataReal);
}

/************************************************************************/
/*                      GDALWarpInitDstNoDataImag()                     */
/************************************************************************/

/**
 * \brief Initialize padfDstNoDataImag  with specified value.
 *
 * @param psOptionsIn options to initialize.
 * @param dNoDataImag value to initialize to.
 *
 */
void CPL_STDCALL
GDALWarpInitDstNoDataImag( GDALWarpOptions * psOptionsIn, double dNoDataImag )
{
    VALIDATE_POINTER0(psOptionsIn, "GDALWarpInitDstNoDataImag");
    InitNoData(
        psOptionsIn->nBandCount, &psOptionsIn->padfDstNoDataImag, dNoDataImag);
}

/************************************************************************/
/*                      GDALWarpInitSrcNoDataImag()                     */
/************************************************************************/

/**
 * \brief Initialize padfSrcNoDataImag  with specified value.
 *
 * @param psOptionsIn options to initialize.
 * @param dNoDataImag value to initialize to.
 *
 */
void CPL_STDCALL
GDALWarpInitSrcNoDataImag( GDALWarpOptions * psOptionsIn, double dNoDataImag )
{
    VALIDATE_POINTER0(psOptionsIn, "GDALWarpInitSrcNoDataImag");
    InitNoData(
        psOptionsIn->nBandCount, &psOptionsIn->padfSrcNoDataImag, dNoDataImag);
}

/************************************************************************/
/*                      GDALWarpResolveWorkingDataType()                */
/************************************************************************/

/**
 * \brief If the working data type is unknown, this method will determine
 *  a valid working data type to support the data in the src and dest
 *  data sets and any noData values.
 *
 * @param psOptions options to initialize.
 *
 */
void CPL_STDCALL
GDALWarpResolveWorkingDataType( GDALWarpOptions *psOptions )
{
    if( psOptions == nullptr ) { return; }
/* -------------------------------------------------------------------- */
/*      If no working data type was provided, set one now.              */
/*                                                                      */
/*      Ensure that the working data type can encapsulate any value     */
/*      in the target, source, and the no data for either.              */
/* -------------------------------------------------------------------- */
    if( psOptions->eWorkingDataType != GDT_Unknown ) { return; }


    psOptions->eWorkingDataType = GDT_Byte;

    for( int iBand = 0; iBand < psOptions->nBandCount; iBand++ )
    {
        if( psOptions->hDstDS != nullptr)
        {
            GDALRasterBandH hDstBand = GDALGetRasterBand(
                psOptions->hDstDS, psOptions->panDstBands[iBand] );

            if( hDstBand != nullptr )
            {
                psOptions->eWorkingDataType =
                    GDALDataTypeUnion( psOptions->eWorkingDataType,
                                        GDALGetRasterDataType( hDstBand ) );
            }
        }
        else if( psOptions->hSrcDS != nullptr )
        {
            GDALRasterBandH hSrcBand = GDALGetRasterBand(
                psOptions->hSrcDS, psOptions->panSrcBands[iBand] );

            if( hSrcBand != nullptr)
            {
                psOptions->eWorkingDataType =
                    GDALDataTypeUnion( psOptions->eWorkingDataType,
                                        GDALGetRasterDataType( hSrcBand ) );
            }
        }

        if( psOptions->padfSrcNoDataReal != nullptr )
        {
            psOptions->eWorkingDataType = GDALDataTypeUnionWithValue(
                psOptions->eWorkingDataType,
                psOptions->padfSrcNoDataReal[iBand],
                false );
        }

        if( psOptions->padfSrcNoDataImag != nullptr &&
            psOptions->padfSrcNoDataImag[iBand] != 0.0 )
        {
           psOptions->eWorkingDataType = GDALDataTypeUnionWithValue(
                psOptions->eWorkingDataType,
                psOptions->padfSrcNoDataImag[iBand],
                true );
        }

        if( psOptions->padfDstNoDataReal != nullptr )
        {
            psOptions->eWorkingDataType = GDALDataTypeUnionWithValue(
                psOptions->eWorkingDataType,
                psOptions->padfDstNoDataReal[iBand],
                false );
        }

        if( psOptions->padfDstNoDataImag != nullptr &&
            psOptions->padfDstNoDataImag[iBand] != 0.0 )
        {
            psOptions->eWorkingDataType = GDALDataTypeUnionWithValue(
                psOptions->eWorkingDataType,
                psOptions->padfDstNoDataImag[iBand],
                true );
        }
    }
}

/************************************************************************/
/*                      GDALWarpInitDefaultBandMapping()                */
/************************************************************************/

/**
 * \brief Init src and dst band mappings such that Bands[i] = i+1
 *  for nBandCount
 *  Does nothing if psOptionsIn->nBandCount is non-zero.
 *
 * @param psOptionsIn options to initialize.
 * @param nBandCount bands to initialize for.
 *
 */
void CPL_STDCALL
GDALWarpInitDefaultBandMapping( GDALWarpOptions * psOptionsIn, int nBandCount )
{
    if( psOptionsIn->nBandCount != 0 ) { return; }

    psOptionsIn->nBandCount = nBandCount;

    psOptionsIn->panSrcBands = static_cast<int *>(
        CPLMalloc(sizeof(int) * psOptionsIn->nBandCount));
    psOptionsIn->panDstBands = static_cast<int *>(
        CPLMalloc(sizeof(int) * psOptionsIn->nBandCount));

    for( int i = 0; i < psOptionsIn->nBandCount; i++ )
    {
        psOptionsIn->panSrcBands[i] = i+1;
        psOptionsIn->panDstBands[i] = i+1;
    }
}

/************************************************************************/
/*                      GDALSerializeWarpOptions()                      */
/************************************************************************/

CPLXMLNode * CPL_STDCALL
GDALSerializeWarpOptions( const GDALWarpOptions *psWO )

{
/* -------------------------------------------------------------------- */
/*      Create root.                                                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTree =
        CPLCreateXMLNode( nullptr, CXT_Element, "GDALWarpOptions" );

/* -------------------------------------------------------------------- */
/*      WarpMemoryLimit                                                 */
/* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(
        psTree, "WarpMemoryLimit",
        CPLString().Printf("%g", psWO->dfWarpMemoryLimit ) );

/* -------------------------------------------------------------------- */
/*      ResampleAlg                                                     */
/* -------------------------------------------------------------------- */
    const char *pszAlgName = nullptr;

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
    else if( psWO->eResampleAlg == GRA_RMS )
        pszAlgName = "RootMeanSquare";
    else if( psWO->eResampleAlg == GRA_Mode )
        pszAlgName = "Mode";
    else if( psWO->eResampleAlg == GRA_Max )
        pszAlgName = "Maximum";
    else if( psWO->eResampleAlg == GRA_Min )
        pszAlgName = "Minimum";
    else if( psWO->eResampleAlg == GRA_Med )
        pszAlgName = "Median";
    else if( psWO->eResampleAlg == GRA_Q1 )
        pszAlgName = "Quartile1";
    else if( psWO->eResampleAlg == GRA_Q3 )
        pszAlgName = "Quartile3";
    else if( psWO->eResampleAlg == GRA_Sum )
        pszAlgName = "Sum";
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
    for( int iWO = 0; psWO->papszWarpOptions != nullptr
             && psWO->papszWarpOptions[iWO] != nullptr; iWO++ )
    {
        char *pszName = nullptr;
        const char *pszValue =
            CPLParseNameValue( psWO->papszWarpOptions[iWO], &pszName );

        // EXTRA_ELTS is an internal detail that we will recover
        // no need to serialize it.
        // And CUTLINE is also serialized in a special way
        if( pszName != nullptr &&
            !EQUAL(pszName, "EXTRA_ELTS") && !EQUAL(pszName, "CUTLINE") )
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
    if( psWO->hSrcDS != nullptr )
    {
        CPLCreateXMLElementAndValue(
            psTree, "SourceDataset",
            GDALGetDescription( psWO->hSrcDS ) );

        char** papszOpenOptions =
            (static_cast<GDALDataset*>(psWO->hSrcDS))->GetOpenOptions();
        GDALSerializeOpenOptionsToXML(psTree, papszOpenOptions);
    }

    if( psWO->hDstDS != nullptr && strlen(GDALGetDescription(psWO->hDstDS)) != 0 )
    {
        CPLCreateXMLElementAndValue(
            psTree, "DestinationDataset",
            GDALGetDescription( psWO->hDstDS ) );
    }

/* -------------------------------------------------------------------- */
/*      Serialize transformer.                                          */
/* -------------------------------------------------------------------- */
    if( psWO->pfnTransformer != nullptr )
    {
        CPLXMLNode *psTransformerContainer =
            CPLCreateXMLNode( psTree, CXT_Element, "Transformer" );

        CPLXMLNode *psTransformerTree =
            GDALSerializeTransformer( psWO->pfnTransformer,
                                      psWO->pTransformerArg );

        if( psTransformerTree != nullptr )
            CPLAddXMLChild( psTransformerContainer, psTransformerTree );
    }

/* -------------------------------------------------------------------- */
/*      Band count and lists.                                           */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psBandList = nullptr;

    if( psWO->nBandCount != 0 )
        psBandList = CPLCreateXMLNode( psTree, CXT_Element, "BandList" );

    for( int i = 0; i < psWO->nBandCount; i++ )
    {
        CPLXMLNode *psBand;

        psBand = CPLCreateXMLNode( psBandList, CXT_Element, "BandMapping" );
        if( psWO->panSrcBands != nullptr )
            CPLCreateXMLNode(
                CPLCreateXMLNode( psBand, CXT_Attribute, "src" ),
                CXT_Text, CPLString().Printf( "%d", psWO->panSrcBands[i] ) );
        if( psWO->panDstBands != nullptr )
            CPLCreateXMLNode(
                CPLCreateXMLNode( psBand, CXT_Attribute, "dst" ),
                CXT_Text, CPLString().Printf( "%d", psWO->panDstBands[i] ) );

        if( psWO->padfSrcNoDataReal != nullptr )
        {
            if (CPLIsNan(psWO->padfSrcNoDataReal[i]))
                CPLCreateXMLElementAndValue(psBand, "SrcNoDataReal", "nan");
            else
                CPLCreateXMLElementAndValue(
                    psBand, "SrcNoDataReal",
                    CPLString().Printf( "%.16g", psWO->padfSrcNoDataReal[i] ) );
        }

        if( psWO->padfSrcNoDataImag != nullptr )
        {
            if (CPLIsNan(psWO->padfSrcNoDataImag[i]))
                CPLCreateXMLElementAndValue(psBand, "SrcNoDataImag", "nan");
            else
                CPLCreateXMLElementAndValue(
                    psBand, "SrcNoDataImag",
                    CPLString().Printf( "%.16g", psWO->padfSrcNoDataImag[i] ) );
        }
        // Compatibility with GDAL <= 2.2: if we serialize a SrcNoDataReal,
        // it needs a SrcNoDataImag as well
        else if( psWO->padfSrcNoDataReal != nullptr )
        {
            CPLCreateXMLElementAndValue(psBand, "SrcNoDataImag", "0");
        }

        if( psWO->padfDstNoDataReal != nullptr )
        {
            if (CPLIsNan(psWO->padfDstNoDataReal[i]))
                CPLCreateXMLElementAndValue(psBand, "DstNoDataReal", "nan");
            else
                CPLCreateXMLElementAndValue(
                    psBand, "DstNoDataReal",
                    CPLString().Printf( "%.16g", psWO->padfDstNoDataReal[i] ) );
        }

        if( psWO->padfDstNoDataImag != nullptr )
        {
            if (CPLIsNan(psWO->padfDstNoDataImag[i]))
                CPLCreateXMLElementAndValue(psBand, "DstNoDataImag", "nan");
            else
                CPLCreateXMLElementAndValue(
                    psBand, "DstNoDataImag",
                    CPLString().Printf( "%.16g", psWO->padfDstNoDataImag[i] ) );
        }
        // Compatibility with GDAL <= 2.2: if we serialize a DstNoDataReal,
        // it needs a SrcNoDataImag as well
        else if( psWO->padfDstNoDataReal != nullptr )
        {
            CPLCreateXMLElementAndValue(psBand, "DstNoDataImag", "0");
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
    if( psWO->hCutline != nullptr )
    {
        char *pszWKT = nullptr;
        if( OGR_G_ExportToWkt( reinterpret_cast<OGRGeometryH>(psWO->hCutline), &pszWKT )
            == OGRERR_NONE )
        {
            CPLCreateXMLElementAndValue( psTree, "Cutline", pszWKT );
        }
        CPLFree( pszWKT );
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
    if( psTree == nullptr || psTree->eType != CXT_Element
        || !EQUAL(psTree->pszValue, "GDALWarpOptions") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Wrong node, unable to deserialize GDALWarpOptions." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create pre-initialized warp options.                            */
/* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = GDALCreateWarpOptions();

/* -------------------------------------------------------------------- */
/*      Warp memory limit.                                              */
/* -------------------------------------------------------------------- */
    psWO->dfWarpMemoryLimit =
        CPLAtof(CPLGetXMLValue(psTree,"WarpMemoryLimit", "0.0"));

/* -------------------------------------------------------------------- */
/*      resample algorithm                                              */
/* -------------------------------------------------------------------- */
    const char *pszValue =
        CPLGetXMLValue(psTree,"ResampleAlg","Default");

    if( EQUAL(pszValue,"NearestNeighbour") )
        psWO->eResampleAlg = GRA_NearestNeighbour;
    else if( EQUAL(pszValue, "Bilinear") )
        psWO->eResampleAlg = GRA_Bilinear;
    else if( EQUAL(pszValue, "Cubic") )
        psWO->eResampleAlg = GRA_Cubic;
    else if( EQUAL(pszValue, "CubicSpline") )
        psWO->eResampleAlg = GRA_CubicSpline;
    else if( EQUAL(pszValue, "Lanczos") )
        psWO->eResampleAlg = GRA_Lanczos;
    else if( EQUAL(pszValue, "Average") )
        psWO->eResampleAlg = GRA_Average;
    else if( EQUAL(pszValue, "RootMeanSquare") )
        psWO->eResampleAlg = GRA_RMS;
    else if( EQUAL(pszValue, "Mode") )
        psWO->eResampleAlg = GRA_Mode;
    else if( EQUAL(pszValue, "Maximum") )
        psWO->eResampleAlg = GRA_Max;
    else if( EQUAL(pszValue, "Minimum") )
        psWO->eResampleAlg = GRA_Min;
    else if( EQUAL(pszValue, "Median") )
        psWO->eResampleAlg = GRA_Med;
    else if( EQUAL(pszValue, "Quartile1") )
        psWO->eResampleAlg = GRA_Q1;
    else if( EQUAL(pszValue, "Quartile3") )
        psWO->eResampleAlg = GRA_Q3;
    else if( EQUAL(pszValue, "Sum") )
        psWO->eResampleAlg = GRA_Sum;
    else if( EQUAL(pszValue, "Default") )
        /* leave as is */;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unrecognised ResampleAlg value '%s'.",
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
    for( CPLXMLNode *psItem = psTree->psChild;
         psItem != nullptr;
         psItem = psItem->psNext )
    {
        if( psItem->eType == CXT_Element
            && EQUAL(psItem->pszValue, "Option") )
        {
            const char *pszName = CPLGetXMLValue(psItem, "Name", nullptr );
            pszValue = CPLGetXMLValue(psItem, "", nullptr );

            if( pszName != nullptr && pszValue != nullptr )
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
    pszValue = CPLGetXMLValue(psTree,"SourceDataset",nullptr);

    if( pszValue != nullptr )
    {
        CPLConfigOptionSetter oSetter("CPL_ALLOW_VSISTDIN", "NO", true);

        char** papszOpenOptions = GDALDeserializeOpenOptionsFromXML(psTree);
        psWO->hSrcDS = GDALOpenEx(
            pszValue, GDAL_OF_SHARED | GDAL_OF_RASTER | GDAL_OF_VERBOSE_ERROR,
            nullptr,
            papszOpenOptions, nullptr );
        CSLDestroy(papszOpenOptions);
    }

/* -------------------------------------------------------------------- */
/*      Destination Dataset.                                            */
/* -------------------------------------------------------------------- */
    pszValue = CPLGetXMLValue(psTree, "DestinationDataset",nullptr);

    if( pszValue != nullptr )
    {
        psWO->hDstDS = GDALOpenShared( pszValue, GA_Update );
    }

/* -------------------------------------------------------------------- */
/*      First, count band mappings so we can establish the bandcount.   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psBandTree = CPLGetXMLNode( psTree, "BandList" );

    int nBandCount = 0;
    CPLXMLNode *psBand = psBandTree ? psBandTree->psChild : nullptr;
    for( ; psBand != nullptr; psBand = psBand->psNext )
    {
        if( psBand->eType != CXT_Element
            || !EQUAL(psBand->pszValue,"BandMapping") )
            continue;

        nBandCount++;
    }

    GDALWarpInitDefaultBandMapping(psWO, nBandCount);

/* ==================================================================== */
/*      Now actually process each bandmapping.                          */
/* ==================================================================== */
    int iBand = 0;

    psBand = psBandTree ? psBandTree->psChild : nullptr;

    for( ; psBand != nullptr; psBand = psBand->psNext )
    {
        if( psBand->eType != CXT_Element
            || !EQUAL(psBand->pszValue,"BandMapping") )
            continue;

/* -------------------------------------------------------------------- */
/*      Source band                                                     */
/* -------------------------------------------------------------------- */
        pszValue = CPLGetXMLValue(psBand,"src",nullptr);
        if( pszValue != nullptr )
            psWO->panSrcBands[iBand] = atoi(pszValue);

/* -------------------------------------------------------------------- */
/*      Destination band.                                               */
/* -------------------------------------------------------------------- */
        pszValue = CPLGetXMLValue(psBand,"dst",nullptr);
        if( pszValue != nullptr )
            psWO->panDstBands[iBand] = atoi(pszValue);

/* -------------------------------------------------------------------- */
/*      Source nodata.                                                  */
/* -------------------------------------------------------------------- */
        pszValue = CPLGetXMLValue(psBand,"SrcNoDataReal",nullptr);
        if( pszValue != nullptr )
        {
            GDALWarpInitSrcNoDataReal(psWO, -1.1e20);
            psWO->padfSrcNoDataReal[iBand] = CPLAtof(pszValue);
        }

        pszValue = CPLGetXMLValue(psBand,"SrcNoDataImag",nullptr);
        if( pszValue != nullptr )
        {
            GDALWarpInitSrcNoDataImag(psWO, 0);
            psWO->padfSrcNoDataImag[iBand] = CPLAtof(pszValue);
        }

/* -------------------------------------------------------------------- */
/*      Destination nodata.                                             */
/* -------------------------------------------------------------------- */
        pszValue = CPLGetXMLValue(psBand,"DstNoDataReal",nullptr);
        if( pszValue != nullptr )
        {
            GDALWarpInitDstNoDataReal(psWO, -1.1e20);
            psWO->padfDstNoDataReal[iBand] = CPLAtof(pszValue);
        }

        pszValue = CPLGetXMLValue(psBand,"DstNoDataImag",nullptr);
        if( pszValue != nullptr )
        {
            GDALWarpInitDstNoDataImag(psWO, 0);
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
    const char *pszWKT = CPLGetXMLValue( psTree, "Cutline", nullptr );
    if( pszWKT )
    {
        char* pszWKTTemp = const_cast<char*>(pszWKT);
        OGR_G_CreateFromWkt( &pszWKTTemp, nullptr,
                             reinterpret_cast<OGRGeometryH *>(&psWO->hCutline) );
    }

    psWO->dfCutlineBlendDist =
        CPLAtof( CPLGetXMLValue( psTree, "CutlineBlendDist", "0" ) );

/* -------------------------------------------------------------------- */
/*      Transformation.                                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTransformer = CPLGetXMLNode( psTree, "Transformer" );

    if( psTransformer != nullptr && psTransformer->psChild != nullptr )
    {
        GDALDeserializeTransformer( psTransformer->psChild,
                                    &(psWO->pfnTransformer),
                                    &(psWO->pTransformerArg) );
    }

/* -------------------------------------------------------------------- */
/*      If any error has occurred, cleanup else return success.          */
/* -------------------------------------------------------------------- */
    if( CPLGetLastErrorType() != CE_None )
    {
        if ( psWO->pTransformerArg )
        {
            GDALDestroyTransformer( psWO->pTransformerArg );
            psWO->pTransformerArg = nullptr;
        }
        if( psWO->hSrcDS != nullptr )
        {
            GDALClose( psWO->hSrcDS );
            psWO->hSrcDS = nullptr;
        }
        if( psWO->hDstDS != nullptr )
        {
            GDALClose( psWO->hDstDS );
            psWO->hDstDS = nullptr;
        }
        GDALDestroyWarpOptions( psWO );
        return nullptr;
    }

    return psWO;
}
