/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implementation of high level convenience APIs for warper.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.20  2006/08/29 22:38:29  fwarmerdam
 * Added support for saving and load papszWarpOptions.
 *
 * Revision 1.19  2005/04/11 17:20:40  fwarmerdam
 * ensure dstnodata is duplicate in clonewarpoptions: bug 821
 *
 * Revision 1.18  2005/04/04 15:24:16  fwarmerdam
 * added CPL_STDCALL to some functions
 *
 * Revision 1.17  2004/12/31 02:11:20  fwarmerdam
 * Avoid warning.
 *
 * Revision 1.16  2004/10/07 15:50:18  fwarmerdam
 * added preliminary alpha band support
 *
 * Revision 1.15  2004/08/11 21:20:47  warmerda
 * avoid crash if transformer does not serialize
 *
 * Revision 1.14  2004/08/09 14:38:27  warmerda
 * added serialize/deserialize support for warpoptions and transformers
 *
 * Revision 1.13  2004/04/01 18:58:41  warmerda
 * fixed GDALReprojectImage() -- pass on eResampleAlg
 *
 * Revision 1.12  2004/03/28 21:20:39  warmerda
 * fixed initialization of nodata values
 *
 * Revision 1.11  2004/03/19 15:54:42  warmerda
 * Fixed another place where "2" was used instead of 0 as the default
 * polynomial order.
 *
 * Revision 1.10  2004/03/19 05:06:00  warmerda
 * Pass 0 for order to GDALCreateGenImgProjTransformer() from
 * GDALCreateAndReprojectImage() so that the default will be to figure
 * out the best order instead of defaulting to 2nd order.
 *
 * Revision 1.9  2004/02/25 19:51:00  warmerda
 * Fixed nodata check in GDT_Int16 case (from Manuel Massing).
 *
 * Revision 1.8  2003/07/26 17:32:16  warmerda
 * Added info on new warp options.
 *
 * Revision 1.7  2003/05/06 18:31:35  warmerda
 * added WRITE_FLUSH comment
 *
 * Revision 1.6  2003/04/22 19:41:24  dron
 * Fixed problem in GDALWarpNoDataMasker(): missed breaks in switch.
 *
 * Revision 1.5  2003/03/02 05:25:26  warmerda
 * added GDALWarpNoDataMasker
 *
 * Revision 1.4  2003/02/22 02:04:11  warmerda
 * added dfMaxError to reproject function
 *
 * Revision 1.3  2003/02/21 15:41:55  warmerda
 * rename data member
 *
 * Revision 1.2  2003/02/20 21:53:06  warmerda
 * partial implementation
 *
 * Revision 1.1  2003/02/18 17:25:50  warmerda
 * New
 *
 */

#include "gdalwarper.h"
#include "cpl_string.h"
#include "cpl_minixml.h"

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
 * By default all bands are transferred, with no masking or nodata values
 * in effect.  No metadata, projection info, or color tables are transferred 
 * to the output file. 
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
                    double dfWarpMemoryLimit, 
                    double dfMaxError,
                    GDALProgressFunc pfnProgress, void *pProgressArg, 
                    GDALWarpOptions *psOptions )

{
    GDALWarpOptions *psWOptions;

/* -------------------------------------------------------------------- */
/*      Default a few parameters.                                       */
/* -------------------------------------------------------------------- */
    if( pszSrcWKT == NULL )
        pszSrcWKT = GDALGetProjectionRef( hSrcDS );

    if( pszDstWKT == NULL )
        pszDstWKT = pszSrcWKT;

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
/*      any.                                                            */
/* -------------------------------------------------------------------- */
    for( iBand = 0; iBand < psWOptions->nBandCount; iBand++ )
    {
        GDALRasterBandH hBand = GDALGetRasterBand( hSrcDS, iBand+1 );
        int             bGotNoData = FALSE;
        double          dfNoDataValue;
        
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
/* -------------------------------------------------------------------- */
/*      Default a few parameters.                                       */
/* -------------------------------------------------------------------- */
    if( hDstDriver == NULL )
        hDstDriver = GDALGetDriverByName( "GTiff" );

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

          // nothing to do if value is out of range.
          if( padfNoData[1] != 0.0 )
              return CE_None;

          for( iOffset = nXSize*nYSize-1; iOffset >= 0; iOffset-- )
          {
              if( pafData[iOffset] == fNoData )
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

          padfWrk = (double *) CPLMalloc(nXSize * sizeof(double) * 2);
          for( iLine = 0; iLine < nYSize; iLine++ )
          {
              GDALCopyWords( ((GByte *) *ppImageData)+nWordSize*iLine*nXSize, 
                             eType, nWordSize,
                             padfWrk, GDT_CFloat64, 16, nXSize );
              
              for( iPixel = 0; iPixel < nXSize; iPixel++ )
              {
                  if( padfWrk[iPixel*2] == padfNoData[0]
                      && padfWrk[iPixel*2+1] == padfNoData[1] )
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
GDALWarpSrcAlphaMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType, 
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

    eErr = GDALRasterIO( hAlphaBand, GF_Read, nXOff, nYOff, nXSize, nYSize, 
                         pafMask, nXSize, nYSize, GDT_Float32, 0, 0 );

    if( eErr != CE_None )
        return eErr;

/* -------------------------------------------------------------------- */
/*      Rescale from 0-255 to 0.0-1.0.                                  */
/* -------------------------------------------------------------------- */
    for( int iPixel = nXSize * nYSize - 1; iPixel >= 0; iPixel-- )
    {                                    //  (1/255)
        pafMask[iPixel] = pafMask[iPixel] * 0.00392157; 
        pafMask[iPixel] = MIN(1.0,pafMask[iPixel]);
    }

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
GDALWarpDstAlphaMasker( void *pMaskFuncArg, int nBandCount, GDALDataType eType,
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
            pafMask[iPixel] = pafMask[iPixel] * 0.00392157;
            pafMask[iPixel] = MIN(1.0,pafMask[iPixel]);
        }

        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Write alpha case.                                               */
/* -------------------------------------------------------------------- */
    else
    {
        for( iPixel = nXSize * nYSize - 1; iPixel >= 0; iPixel-- )
            pafMask[iPixel] = (int) (pafMask[iPixel] * 255.1);
        
        // Read data.
        eErr = GDALRasterIO( hAlphaBand, GF_Write, 
                             nXOff, nYOff, nXSize, nYSize, 
                             pafMask, nXSize, nYSize, GDT_Float32, 0, 0 );
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
 */

/************************************************************************/
/*                       GDALCreateWarpOptions()                        */
/************************************************************************/

GDALWarpOptions * CPL_STDCALL GDALCreateWarpOptions()

{
    GDALWarpOptions *psOptions;

    psOptions = (GDALWarpOptions *) CPLCalloc(sizeof(GDALWarpOptions),1);

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
    CSLDestroy( psOptions->papszWarpOptions );
    CPLFree( psOptions->panSrcBands );
    CPLFree( psOptions->panDstBands );
    CPLFree( psOptions->padfSrcNoDataReal );
    CPLFree( psOptions->padfSrcNoDataImag );
    CPLFree( psOptions->padfDstNoDataReal );
    CPLFree( psOptions->padfDstNoDataImag );
    CPLFree( psOptions->papfnSrcPerBandValidityMaskFunc );
    CPLFree( psOptions->papSrcPerBandValidityMaskFuncArg );

    CPLFree( psOptions );
}


#define COPY_MEM(target,type,count)					\
   if( (psSrcOptions->target) != NULL && (count) != 0 ) 		\
   { 									\
       (psDstOptions->target) = (type *) CPLMalloc(sizeof(type)*count); \
       memcpy( (psDstOptions->target), (psSrcOptions->target),		\
 	       sizeof(type) * count ); 	        			\
   }

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
        CPLSPrintf("%g", psWO->dfWarpMemoryLimit ) );

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

        CPLXMLNode *psOption = 
            CPLCreateXMLElementAndValue( 
                psTree, "Option", pszValue );

        CPLCreateXMLNode( 
            CPLCreateXMLNode( psOption, CXT_Attribute, "name" ),
            CXT_Text, pszName );
    }

/* -------------------------------------------------------------------- */
/*      Source and Destination Data Source                              */
/* -------------------------------------------------------------------- */
    if( psWO->hSrcDS != NULL )
    {
        CPLCreateXMLElementAndValue( 
            psTree, "SourceDataset", 
            GDALGetDescription( psWO->hSrcDS ) );
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
                CXT_Text, CPLSPrintf( "%d", psWO->panSrcBands[i] ) );
        if( psWO->panDstBands != NULL )
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psBand, CXT_Attribute, "dst" ),
                CXT_Text, CPLSPrintf( "%d", psWO->panDstBands[i] ) );
        
        if( psWO->padfSrcNoDataReal != NULL )
            CPLCreateXMLElementAndValue( 
                psBand, "SrcNoDataReal", 
                CPLSPrintf( "%.16g", psWO->padfSrcNoDataReal[i] ) );

        if( psWO->padfSrcNoDataImag != NULL )
            CPLCreateXMLElementAndValue( 
                psBand, "SrcNoDataImag", 
                CPLSPrintf( "%.16g", psWO->padfSrcNoDataImag[i] ) );

        if( psWO->padfDstNoDataReal != NULL )
            CPLCreateXMLElementAndValue( 
                psBand, "DstNoDataReal", 
                CPLSPrintf( "%.16g", psWO->padfDstNoDataReal[i] ) );

        if( psWO->padfDstNoDataImag != NULL )
            CPLCreateXMLElementAndValue( 
                psBand, "DstNoDataImag", 
                CPLSPrintf( "%.16g", psWO->padfDstNoDataImag[i] ) );
    }

/* -------------------------------------------------------------------- */
/*      Alpha bands.                                                    */
/* -------------------------------------------------------------------- */
    if( psWO->nSrcAlphaBand > 0 )
        CPLCreateXMLElementAndValue( 
            psTree, "SrcAlphaBand", 
            CPLSPrintf( "%d", psWO->nSrcAlphaBand ) );

    if( psWO->nDstAlphaBand > 0 )
        CPLCreateXMLElementAndValue( 
            psTree, "DstAlphaBand", 
            CPLSPrintf( "%d", psWO->nDstAlphaBand ) );

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
        atof(CPLGetXMLValue(psTree,"WarpMemoryLimit","0.0"));

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
        psWO->hSrcDS = GDALOpenShared( pszValue, GA_ReadOnly );

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
    
    for( psBand=psBandTree->psChild; psBand != NULL; psBand = psBand->psNext )
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

    for( psBand=psBandTree->psChild; psBand != NULL; psBand = psBand->psNext )
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

            psWO->padfSrcNoDataReal[iBand] = atof(pszValue);
        }
        
        pszValue = CPLGetXMLValue(psBand,"SrcNoDataImag",NULL);
        if( pszValue != NULL )
        {
            if( psWO->padfSrcNoDataImag == NULL )
                psWO->padfSrcNoDataImag = 
                    (double *) CPLCalloc(sizeof(double),psWO->nBandCount);

            psWO->padfSrcNoDataReal[iBand] = atof(pszValue);
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

            psWO->padfDstNoDataReal[iBand] = atof(pszValue);
        }
        
        pszValue = CPLGetXMLValue(psBand,"DstNoDataImag",NULL);
        if( pszValue != NULL )
        {
            if( psWO->padfDstNoDataImag == NULL )
                psWO->padfDstNoDataImag = 
                    (double *) CPLCalloc(sizeof(double),psWO->nBandCount);

            psWO->padfDstNoDataReal[iBand] = atof(pszValue);
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
        GDALDestroyWarpOptions( psWO );
        return NULL;
    }
    else
        return psWO;
}
