/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implementation of the GDALWarpKernel class.  Implements the actual
 *           image warping for a "chunk" of input and output imagery already
 *           loaded into memory.
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
 ****************************************************************************/

#include "gdalwarper.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

static CPLErr GWKGeneralCase( GDALWarpKernel * );
static CPLErr GWKNearestNoMasksByte( GDALWarpKernel *poWK );
static CPLErr GWKBilinearNoMasksByte( GDALWarpKernel *poWK );
static CPLErr GWKCubicNoMasksByte( GDALWarpKernel *poWK );
static CPLErr GWKCubicSplineNoMasksByte( GDALWarpKernel *poWK );
static CPLErr GWKNearestByte( GDALWarpKernel *poWK );
static CPLErr GWKNearestNoMasksShort( GDALWarpKernel *poWK );
static CPLErr GWKBilinearNoMasksShort( GDALWarpKernel *poWK );
static CPLErr GWKCubicNoMasksShort( GDALWarpKernel *poWK );
static CPLErr GWKCubicSplineNoMasksShort( GDALWarpKernel *poWK );
static CPLErr GWKNearestShort( GDALWarpKernel *poWK );
static CPLErr GWKNearestNoMasksFloat( GDALWarpKernel *poWK );
static CPLErr GWKNearestFloat( GDALWarpKernel *poWK );

/************************************************************************/
/* ==================================================================== */
/*                            GDALWarpKernel                            */
/* ==================================================================== */
/************************************************************************/

/**
 * \class GDALWarpKernel "gdalwarper.h"
 *
 * Low level image warping class.
 *
 * This class is responsible for low level image warping for one
 * "chunk" of imagery.  The class is essentially a structure with all
 * data members public - primarily so that new special-case functions 
 * can be added without changing the class declaration.  
 *
 * Applications are normally intended to interactive with warping facilities
 * through the GDALWarpOperation class, though the GDALWarpKernel can in
 * theory be used directly if great care is taken in setting up the 
 * control data. 
 *
 * <h3>Design Issues</h3>
 *
 * My intention is that PerformWarp() would analyse the setup in terms
 * of the datatype, resampling type, and validity/density mask usage and
 * pick one of many specific implementations of the warping algorithm over
 * a continuim of optimization vs. generality.  At one end there will be a
 * reference general purpose implementation of the algorithm that supports
 * any data type (working internally in double precision complex), all three
 * resampling types, and any or all of the validity/density masks.  At the
 * other end would be highly optimized algorithms for common cases like
 * nearest neighbour resampling on GDT_Byte data with no masks.  
 *
 * The full set of optimized versions have not been decided but we should 
 * expect to have at least:
 *  - One for each resampling algorithm for 8bit data with no masks. 
 *  - One for each resampling algorithm for float data with no masks.
 *  - One for each resampling algorithm for float data with any/all masks
 *    (essentially the generic case for just float data). 
 *  - One for each resampling algorithm for 8bit data with support for
 *    input validity masks (per band or per pixel).  This handles the common 
 *    case of nodata masking.
 *  - One for each resampling algorithm for float data with support for
 *    input validity masks (per band or per pixel).  This handles the common 
 *    case of nodata masking.
 *
 * Some of the specializations would operate on all bands in one pass
 * (especially the ones without masking would do this), while others might
 * process each band individually to reduce code complexity.
 *
 * <h3>Masking Semantics</h3>
 * 
 * A detailed explanation of the semantics of the validity and density masks,
 * and their effects on resampling kernels is needed here. 
 */

/************************************************************************/
/*                     GDALWarpKernel Data Members                      */
/************************************************************************/

/**
 * \var GDALResampleAlg GDALWarpKernel::eResample;
 * 
 * Resampling algorithm.
 *
 * The resampling algorithm to use.  One of GRA_NearestNeighbour, 
 * GRA_Bilinear, or GRA_Cubic. 
 *
 * This field is required. GDT_NearestNeighbour may be used as a default
 * value. 
 */
                                  
/**
 * \var GDALDataType GDALWarpKernel::eWorkingDataType;
 * 
 * Working pixel data type.
 *
 * The datatype of pixels in the source image (papabySrcimage) and
 * destination image (papabyDstImage) buffers.  Note that operations on 
 * some data types (such as GDT_Byte) may be much better optimized than other
 * less common cases. 
 *
 * This field is required.  It may not be GDT_Unknown.
 */
                                  
/**
 * \var int GDALWarpKernel::nBands;
 * 
 * Number of bands.
 *
 * The number of bands (layers) of imagery being warped.  Determines the
 * number of entries in the papabySrcImage, papanBandSrcValid, 
 * and papabyDstImage arrays. 
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nSrcXSize;
 * 
 * Source image width in pixels.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nSrcYSize;
 * 
 * Source image height in pixels.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::papabySrcImage;
 * 
 * Array of source image band data.
 *
 * This is an array of pointers (of size GDALWarpKernel::nBands) pointers
 * to image data.  Each individual band of image data is organized as a single 
 * block of image data in left to right, then bottom to top order.  The actual
 * type of the image data is determined by GDALWarpKernel::eWorkingDataType.
 *
 * To access the the pixel value for the (x=3,y=4) pixel (zero based) of
 * the second band with eWorkingDataType set to GDT_Float32 use code like
 * this:
 *
 * \code 
 *   float dfPixelValue;
 *   int   nBand = 1;  // band indexes are zero based.
 *   int   nPixel = 3; // zero based
 *   int   nLine = 4;  // zero based
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nSrcXSize );
 *   assert( nLine >= 0 && nLine < poKern->nSrcYSize );
 *   assert( nBand >= 0 && nBand < poKern->nBands );
 *   dfPixelValue = ((float *) poKern->papabySrcImage[nBand-1])
 *                                  [nPixel + nLine * poKern->nSrcXSize];
 * \endcode
 *
 * This field is required.
 */

/**
 * \var GUInt32 **GDALWarpKernel::papanBandSrcValid;
 *
 * Per band validity mask for source pixels. 
 *
 * Array of pixel validity mask layers for each source band.   Each of
 * the mask layers is the same size (in pixels) as the source image with
 * one bit per pixel.  Note that it is legal (and common) for this to be
 * NULL indicating that none of the pixels are invalidated, or for some
 * band validity masks to be NULL in which case all pixels of the band are
 * valid.  The following code can be used to test the validity of a particular
 * pixel.
 *
 * \code 
 *   int   bIsValid = TRUE;
 *   int   nBand = 1;  // band indexes are zero based.
 *   int   nPixel = 3; // zero based
 *   int   nLine = 4;  // zero based
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nSrcXSize );
 *   assert( nLine >= 0 && nLine < poKern->nSrcYSize );
 *   assert( nBand >= 0 && nBand < poKern->nBands );
 * 
 *   if( poKern->papanBandSrcValid != NULL
 *       && poKern->papanBandSrcValid[nBand] != NULL )
 *   {
 *       GUInt32 *panBandMask = poKern->papanBandSrcValid[nBand];
 *       int    iPixelOffset = nPixel + nLine * poKern->nSrcXSize;
 * 
 *       bIsValid = panBandMask[iPixelOffset>>5] 
 *                  & (0x01 << (iPixelOffset & 0x1f));
 *   }
 * \endcode
 */

/**
 * \var GUInt32 *GDALWarpKernel::panUnifiedSrcValid;
 *
 * Per pixel validity mask for source pixels. 
 *
 * A single validity mask layer that applies to the pixels of all source
 * bands.  It is accessed similarly to papanBandSrcValid, but without the
 * extra level of band indirection.
 *
 * This pointer may be NULL indicating that all pixels are valid. 
 * 
 * Note that if both panUnifiedSrcValid, and papanBandSrcValid are available,
 * the pixel isn't considered to be valid unless both arrays indicate it is
 * valid.  
 */

/**
 * \var float *GDALWarpKernel::pafUnifiedSrcDensity;
 *
 * Per pixel density mask for source pixels. 
 *
 * A single density mask layer that applies to the pixels of all source
 * bands.  It contains values between 0.0 and 1.0 indicating the degree to 
 * which this pixel should be allowed to contribute to the output result. 
 *
 * This pointer may be NULL indicating that all pixels have a density of 1.0.
 *
 * The density for a pixel may be accessed like this:
 *
 * \code 
 *   float fDensity = 1.0;
 *   int   nPixel = 3; // zero based
 *   int   nLine = 4;  // zero based
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nSrcXSize );
 *   assert( nLine >= 0 && nLine < poKern->nSrcYSize );
 *   if( poKern->pafUnifiedSrcDensity != NULL )
 *     fDensity = poKern->pafUnifiedSrcDensity
 *                                  [nPixel + nLine * poKern->nSrcXSize];
 * \endcode
 */

/**
 * \var int GDALWarpKernel::nDstXSize;
 *
 * Width of destination image in pixels.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nDstYSize;
 *
 * Height of destination image in pixels.
 *
 * This field is required.
 */

/**
 * \var GByte **GDALWarpKernel::papabyDstImage;
 * 
 * Array of destination image band data.
 *
 * This is an array of pointers (of size GDALWarpKernel::nBands) pointers
 * to image data.  Each individual band of image data is organized as a single 
 * block of image data in left to right, then bottom to top order.  The actual
 * type of the image data is determined by GDALWarpKernel::eWorkingDataType.
 *
 * To access the the pixel value for the (x=3,y=4) pixel (zero based) of
 * the second band with eWorkingDataType set to GDT_Float32 use code like
 * this:
 *
 * \code 
 *   float dfPixelValue;
 *   int   nBand = 1;  // band indexes are zero based.
 *   int   nPixel = 3; // zero based
 *   int   nLine = 4;  // zero based
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nDstXSize );
 *   assert( nLine >= 0 && nLine < poKern->nDstYSize );
 *   assert( nBand >= 0 && nBand < poKern->nBands );
 *   dfPixelValue = ((float *) poKern->papabyDstImage[nBand-1])
 *                                  [nPixel + nLine * poKern->nSrcYSize];
 * \endcode
 *
 * This field is required.
 */

/**
 * \var GUInt32 *GDALWarpKernel::panDstValid;
 *
 * Per pixel validity mask for destination pixels. 
 *
 * A single validity mask layer that applies to the pixels of all destination
 * bands.  It is accessed similarly to papanUnitifiedSrcValid, but based
 * on the size of the destination image.
 *
 * This pointer may be NULL indicating that all pixels are valid. 
 */

/**
 * \var float *GDALWarpKernel::pafDstDensity;
 *
 * Per pixel density mask for destination pixels. 
 *
 * A single density mask layer that applies to the pixels of all destination
 * bands.  It contains values between 0.0 and 1.0.
 *
 * This pointer may be NULL indicating that all pixels have a density of 1.0.
 *
 * The density for a pixel may be accessed like this:
 *
 * \code 
 *   float fDensity = 1.0;
 *   int   nPixel = 3; // zero based
 *   int   nLine = 4;  // zero based
 *
 *   assert( nPixel >= 0 && nPixel < poKern->nDstXSize );
 *   assert( nLine >= 0 && nLine < poKern->nDstYSize );
 *   if( poKern->pafDstDensity != NULL )
 *     fDensity = poKern->pafDstDensity[nPixel + nLine * poKern->nDstXSize];
 * \endcode
 */

/**
 * \var int GDALWarpKernel::nSrcXOff;
 *
 * X offset to source pixel coordinates for transformation.
 *
 * See pfnTransformer.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nSrcYOff;
 *
 * Y offset to source pixel coordinates for transformation.
 *
 * See pfnTransformer.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nDstXOff;
 *
 * X offset to destination pixel coordinates for transformation.
 *
 * See pfnTransformer.
 *
 * This field is required.
 */

/**
 * \var int GDALWarpKernel::nDstYOff;
 *
 * Y offset to destination pixel coordinates for transformation.
 *
 * See pfnTransformer.
 *
 * This field is required.
 */

/**
 * \var GDALTransformerFunc GDALWarpKernel::pfnTransformer;
 *
 * Source/destination location transformer.
 *
 * The function to call to transform coordinates between source image 
 * pixel/line coordinates and destination image pixel/line coordinates.  
 * See GDALTransformerFunc() for details of the semantics of this function. 
 *
 * The GDALWarpKern algorithm will only ever use this transformer in 
 * "destination to source" mode (bDstToSrc=TRUE), and will always pass 
 * partial or complete scanlines of points in the destination image as
 * input.  This means, amoung other things, that it is safe to the the
 * approximating transform GDALApproxTransform() as the transformation 
 * function. 
 *
 * Source and destination images may be subsets of a larger overall image.
 * The transformation algorithms will expect and return pixel/line coordinates
 * in terms of this larger image, so coordinates need to be offset by
 * the offsets specified in nSrcXOff, nSrcYOff, nDstXOff, and nDstYOff before
 * passing to pfnTransformer, and after return from it. 
 * 
 * The GDALWarpKernel::pfnTransformerArg value will be passed as the callback
 * data to this function when it is called.
 *
 * This field is required.
 */

/**
 * \var void *GDALWarpKernel::pTransformerArg;
 *
 * Callback data for pfnTransformer.
 *
 * This field may be NULL if not required for the pfnTransformer being used.
 */

/**
 * \var GDALProgressFunc GDALWarpKernel::pfnProgress;
 *
 * The function to call to report progress of the algorithm, and to check
 * for a requested termination of the operation.  It operates according to
 * GDALProgressFunc() semantics. 
 *
 * Generally speaking the progress function will be invoked for each 
 * scanline of the destination buffer that has been processed. 
 *
 * This field may be NULL (internally set to GDALDummyProgress()). 
 */

/**
 * \var void *GDALWarpKernel::pProgress;
 *
 * Callback data for pfnProgress.
 *
 * This field may be NULL if not required for the pfnProgress being used.
 */


/************************************************************************/
/*                           GDALWarpKernel()                           */
/************************************************************************/

GDALWarpKernel::GDALWarpKernel()

{
    eResample = GRA_NearestNeighbour;
    eWorkingDataType = GDT_Unknown;
    nBands = 0;
    nDstXOff = 0;
    nDstYOff = 0;
    nDstXSize = 0;
    nDstYSize = 0;
    nSrcXOff = 0;
    nSrcYOff = 0;
    nSrcXSize = 0;
    nSrcYSize = 0;
    pafDstDensity = NULL;
    pafUnifiedSrcDensity = NULL;
    panDstValid = NULL;
    panUnifiedSrcValid = NULL;
    papabyDstImage = NULL;
    papabySrcImage = NULL;
    papanBandSrcValid = NULL;
    pfnProgress = GDALDummyProgress;
    pProgress = NULL;
    dfProgressBase = 0.0;
    dfProgressScale = 1.0;
    pfnTransformer = NULL;
    pTransformerArg = NULL;
    papszWarpOptions = NULL;
}

/************************************************************************/
/*                          ~GDALWarpKernel()                           */
/************************************************************************/

GDALWarpKernel::~GDALWarpKernel()

{
}

/************************************************************************/
/*                            PerformWarp()                             */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpKernel::PerformWarp();
 * 
 * This method performs the warp described in the GDALWarpKernel.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpKernel::PerformWarp()

{
    CPLErr eErr;

    if( (eErr = Validate()) != CE_None )
        return eErr;

    if( CSLFetchBoolean( papszWarpOptions, "USE_GENERAL_CASE", FALSE ) )
        return GWKGeneralCase( this );

    if( eWorkingDataType == GDT_Byte
        && eResample == GRA_NearestNeighbour
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKNearestNoMasksByte( this );

    if( eWorkingDataType == GDT_Byte
        && eResample == GRA_Bilinear
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKBilinearNoMasksByte( this );

    if( eWorkingDataType == GDT_Byte
        && eResample == GRA_Cubic
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKCubicNoMasksByte( this );

    if( eWorkingDataType == GDT_Byte
        && eResample == GRA_CubicSpline
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKCubicSplineNoMasksByte( this );

    if( eWorkingDataType == GDT_Byte
        && eResample == GRA_NearestNeighbour )
        return GWKNearestByte( this );

    if( (eWorkingDataType == GDT_Int16 || eWorkingDataType == GDT_UInt16)
        && eResample == GRA_NearestNeighbour
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKNearestNoMasksShort( this );

    if( (eWorkingDataType == GDT_Int16 )
        && eResample == GRA_Cubic
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKCubicNoMasksShort( this );

    if( (eWorkingDataType == GDT_Int16 )
        && eResample == GRA_CubicSpline
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKCubicSplineNoMasksShort( this );

    if( (eWorkingDataType == GDT_Int16 )
        && eResample == GRA_Bilinear
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKBilinearNoMasksShort( this );

    if( (eWorkingDataType == GDT_Int16 || eWorkingDataType == GDT_UInt16)
        && eResample == GRA_NearestNeighbour )
        return GWKNearestShort( this );

    if( eWorkingDataType == GDT_Float32
        && eResample == GRA_NearestNeighbour
        && papanBandSrcValid == NULL
        && panUnifiedSrcValid == NULL
        && pafUnifiedSrcDensity == NULL
        && panDstValid == NULL
        && pafDstDensity == NULL )
        return GWKNearestNoMasksFloat( this );

    if( eWorkingDataType == GDT_Float32
        && eResample == GRA_NearestNeighbour )
        return GWKNearestFloat( this );

    return GWKGeneralCase( this );
}
                                  
/************************************************************************/
/*                              Validate()                              */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpKernel::Validate()
 * 
 * Check the settings in the GDALWarpKernel, and issue a CPLError()
 * (and return CE_Failure) if the configuration is considered to be
 * invalid for some reason.  
 *
 * This method will also do some standard defaulting such as setting
 * pfnProgress to GDALDummyProgress() if it is NULL. 
 *
 * @return CE_None on success or CE_Failure if an error is detected.
 */

CPLErr GDALWarpKernel::Validate()

{
    return CE_None;
}

/************************************************************************/
/*                         GWKOverlayDensity()                          */
/*                                                                      */
/*      Compute the final density for the destination pixel.  This      */
/*      is a function of the overlay density (passed in) and the        */
/*      original density.                                               */
/************************************************************************/

static void GWKOverlayDensity( GDALWarpKernel *poWK, int iDstOffset, 
                               double dfDensity )
{
    if( dfDensity < 0.0001 || poWK->pafDstDensity == NULL )
        return;

    poWK->pafDstDensity[iDstOffset] 
        = 1.0 - (1.0-dfDensity) * (1.0-poWK->pafDstDensity[iDstOffset]);
}

/************************************************************************/
/*                          GWKSetPixelValue()                          */
/************************************************************************/

static int GWKSetPixelValue( GDALWarpKernel *poWK, int iBand, 
                             int iDstOffset, double dfDensity, 
                             double dfReal, double dfImag )

{
    GByte *pabyDst = poWK->papabyDstImage[iBand];

/* -------------------------------------------------------------------- */
/*      If the source density is less than 100% we need to fetch the    */
/*      existing destination value, and mix it with the source to       */
/*      get the new "to apply" value.  Also compute composite density.  */
/* -------------------------------------------------------------------- */
    if( dfDensity < 1.0 )
    {
        double dfDstReal, dfDstImag, dfDstDensity = 1.0;

        if( dfDensity < 0.0001 )
            return TRUE;

        if( poWK->pafDstDensity != NULL )
            dfDstDensity = poWK->pafDstDensity[iDstOffset];

        switch( poWK->eWorkingDataType )
        {
          case GDT_Byte:
            dfDstReal = pabyDst[iDstOffset];
            dfDstImag = 0.0;
            break;

          case GDT_Int16:
            dfDstReal = ((GInt16 *) pabyDst)[iDstOffset];
            dfDstImag = 0.0;
            break;

          case GDT_UInt16:
            dfDstReal = ((GUInt16 *) pabyDst)[iDstOffset];
            dfDstImag = 0.0;
            break;
 
          case GDT_Int32:
            dfDstReal = ((GInt32 *) pabyDst)[iDstOffset];
            dfDstImag = 0.0;
            break;
 
          case GDT_UInt32:
            dfDstReal = ((GUInt32 *) pabyDst)[iDstOffset];
            dfDstImag = 0.0;
            break;
 
          case GDT_Float32:
            dfDstReal = ((float *) pabyDst)[iDstOffset];
            dfDstImag = 0.0;
            break;
 
          case GDT_Float64:
            dfDstReal = ((double *) pabyDst)[iDstOffset];
            dfDstImag = 0.0;
            break;
 
          case GDT_CInt16:
            dfDstReal = ((GInt16 *) pabyDst)[iDstOffset*2];
            dfDstImag = ((GInt16 *) pabyDst)[iDstOffset*2+1];
            break;
 
          case GDT_CInt32:
            dfDstReal = ((GInt32 *) pabyDst)[iDstOffset*2];
            dfDstImag = ((GInt32 *) pabyDst)[iDstOffset*2+1];
            break;
 
          case GDT_CFloat32:
            dfDstReal = ((float *) pabyDst)[iDstOffset*2];
            dfDstImag = ((float *) pabyDst)[iDstOffset*2+1];
            break;
 
          case GDT_CFloat64:
            dfDstReal = ((double *) pabyDst)[iDstOffset*2];
            dfDstImag = ((double *) pabyDst)[iDstOffset*2+1];
            break;

          default:
            CPLAssert( FALSE );
            dfDstDensity = 0.0;
            return FALSE;
        }

        // the destination density is really only relative to the portion
        // not occluded by the overlay.
        double dfDstInfluence = (1.0 - dfDensity) * dfDstDensity;

        dfReal = (dfReal * dfDensity + dfDstReal * dfDstInfluence) 
            / (dfDensity + dfDstInfluence);

        dfImag = (dfImag * dfDensity + dfDstImag * dfDstInfluence) 
            / (dfDensity + dfDstInfluence);
    }

/* -------------------------------------------------------------------- */
/*      Actually apply the destination value.                           */
/* -------------------------------------------------------------------- */
    switch( poWK->eWorkingDataType )
    {
      case GDT_Byte:
        if( dfReal < 0.0 )
            pabyDst[iDstOffset] = 0;
        else if( dfReal > 255.0 )
            pabyDst[iDstOffset] = 255;
        else
            pabyDst[iDstOffset] = (GByte) (dfReal+0.5);
        break;

      case GDT_Int16:
        if( dfReal < -32768 )
            ((GInt16 *) pabyDst)[iDstOffset] = -32768;
        else if( dfReal > 32767 )
            ((GInt16 *) pabyDst)[iDstOffset] = 32767;
        else
            ((GInt16 *) pabyDst)[iDstOffset] = (GInt16) floor(dfReal+0.5);
        break;

      case GDT_UInt16:
        if( dfReal < 0 )
            ((GUInt16 *) pabyDst)[iDstOffset] = 0;
        else if( dfReal > 65535 )
            ((GUInt16 *) pabyDst)[iDstOffset] = 65535;
        else
            ((GUInt16 *) pabyDst)[iDstOffset] = (GUInt16) (dfReal+0.5);
        break;

      case GDT_UInt32:
        if( dfReal < 0 )
            ((GUInt32 *) pabyDst)[iDstOffset] = 0;
        else if( dfReal > 4294967295.0 )
            ((GUInt32 *) pabyDst)[iDstOffset] = (GUInt32) 4294967295.0;
        else
            ((GUInt32 *) pabyDst)[iDstOffset] = (GUInt32) (dfReal+0.5);
        break;

      case GDT_Int32:
        if( dfReal < -2147483648.0 )
            ((GInt32 *) pabyDst)[iDstOffset] = 0;
        else if( dfReal > 2147483647.0 )
            ((GInt32 *) pabyDst)[iDstOffset] = 2147483647;
        else
            ((GInt32 *) pabyDst)[iDstOffset] = (GInt32) floor(dfReal+0.5);
        break;

      case GDT_Float32:
        ((float *) pabyDst)[iDstOffset] = (float) dfReal;
        break;

      case GDT_Float64:
        ((double *) pabyDst)[iDstOffset] = dfReal;
        break;

      case GDT_CInt16:
        if( dfReal < -32768 )
            ((GInt16 *) pabyDst)[iDstOffset*2] = -32768;
        else if( dfReal > 32767 )
            ((GInt16 *) pabyDst)[iDstOffset*2] = 32767;
        else
            ((GInt16 *) pabyDst)[iDstOffset*2] = (GInt16) floor(dfReal+0.5);
        if( dfImag < -32768 )
            ((GInt16 *) pabyDst)[iDstOffset*2+1] = -32768;
        else if( dfImag > 32767 )
            ((GInt16 *) pabyDst)[iDstOffset*2+1] = 32767;
        else
            ((GInt16 *) pabyDst)[iDstOffset*2+1] = (GInt16) floor(dfImag+0.5);
        break;

      case GDT_CInt32:
        if( dfReal < -2147483648.0 )
            ((GInt32 *) pabyDst)[iDstOffset*2] = (GInt32) -2147483648.0;
        else if( dfReal > 2147483647.0 )
            ((GInt32 *) pabyDst)[iDstOffset*2] = (GInt32) 2147483647.0;
        else
            ((GInt32 *) pabyDst)[iDstOffset*2] = (GInt32) floor(dfReal+0.5);
        if( dfImag < -2147483648.0 )
            ((GInt32 *) pabyDst)[iDstOffset*2+1] = (GInt32) -2147483648.0;
        else if( dfImag > 2147483647.0 )
            ((GInt32 *) pabyDst)[iDstOffset*2+1] = (GInt32) 2147483647.0;
        else
            ((GInt32 *) pabyDst)[iDstOffset*2+1] = (GInt32) floor(dfImag+0.5);
        break;

      case GDT_CFloat32:
        ((float *) pabyDst)[iDstOffset*2] = (float) dfReal;
        ((float *) pabyDst)[iDstOffset*2+1] = (float) dfImag;
        break;

      case GDT_CFloat64:
        ((double *) pabyDst)[iDstOffset*2] = (double) dfReal;
        ((double *) pabyDst)[iDstOffset*2+1] = (double) dfImag;
        break;

      default:
        return FALSE;
    }

    return TRUE;
}
                             
/************************************************************************/
/*                          GWKGetPixelValue()                          */
/************************************************************************/

static int GWKGetPixelValue( GDALWarpKernel *poWK, int iBand, 
                             int iSrcOffset, double *pdfDensity, 
                             double *pdfReal, double *pdfImag )

{
    GByte *pabySrc = poWK->papabySrcImage[iBand];

    if( poWK->panUnifiedSrcValid != NULL
        && !((poWK->panUnifiedSrcValid[iSrcOffset>>5]
              & (0x01 << (iSrcOffset & 0x1f))) ) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    if( poWK->papanBandSrcValid != NULL
        && poWK->papanBandSrcValid[iBand] != NULL
        && !((poWK->papanBandSrcValid[iBand][iSrcOffset>>5]
              & (0x01 << (iSrcOffset & 0x1f)))) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    switch( poWK->eWorkingDataType )
    {
      case GDT_Byte:
        *pdfReal = pabySrc[iSrcOffset];
        *pdfImag = 0.0;
        break;

      case GDT_Int16:
        *pdfReal = ((GInt16 *) pabySrc)[iSrcOffset];
        *pdfImag = 0.0;
        break;

      case GDT_UInt16:
        *pdfReal = ((GUInt16 *) pabySrc)[iSrcOffset];
        *pdfImag = 0.0;
        break;
 
      case GDT_Int32:
        *pdfReal = ((GInt32 *) pabySrc)[iSrcOffset];
        *pdfImag = 0.0;
        break;
 
      case GDT_UInt32:
        *pdfReal = ((GUInt32 *) pabySrc)[iSrcOffset];
        *pdfImag = 0.0;
        break;
 
      case GDT_Float32:
        *pdfReal = ((float *) pabySrc)[iSrcOffset];
        *pdfImag = 0.0;
        break;
 
      case GDT_Float64:
        *pdfReal = ((double *) pabySrc)[iSrcOffset];
        *pdfImag = 0.0;
        break;
 
      case GDT_CInt16:
        *pdfReal = ((GInt16 *) pabySrc)[iSrcOffset*2];
        *pdfImag = ((GInt16 *) pabySrc)[iSrcOffset*2+1];
        break;
 
      case GDT_CInt32:
        *pdfReal = ((GInt32 *) pabySrc)[iSrcOffset*2];
        *pdfImag = ((GInt32 *) pabySrc)[iSrcOffset*2+1];
        break;
 
      case GDT_CFloat32:
        *pdfReal = ((float *) pabySrc)[iSrcOffset*2];
        *pdfImag = ((float *) pabySrc)[iSrcOffset*2+1];
        break;
 
      case GDT_CFloat64:
        *pdfReal = ((double *) pabySrc)[iSrcOffset*2];
        *pdfImag = ((double *) pabySrc)[iSrcOffset*2+1];
        break;

      default:
        *pdfDensity = 0.0;
        return FALSE;
    }

    if( poWK->pafUnifiedSrcDensity != NULL )
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
    else
        *pdfDensity = 1.0;

    return *pdfDensity != 0.0;
}
                             
/************************************************************************/
/*                          GWKGetPixelByte()                           */
/************************************************************************/

static int GWKGetPixelByte( GDALWarpKernel *poWK, int iBand, 
                            int iSrcOffset, double *pdfDensity, 
                            GByte *pbValue )

{
    GByte *pabySrc = poWK->papabySrcImage[iBand];

    if( poWK->panUnifiedSrcValid != NULL
        && !((poWK->panUnifiedSrcValid[iSrcOffset>>5]
              & (0x01 << (iSrcOffset & 0x1f))) ) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    if( poWK->papanBandSrcValid != NULL
        && poWK->papanBandSrcValid[iBand] != NULL
        && !((poWK->papanBandSrcValid[iBand][iSrcOffset>>5]
              & (0x01 << (iSrcOffset & 0x1f)))) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    *pbValue = pabySrc[iSrcOffset];

    if( poWK->pafUnifiedSrcDensity != NULL )
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
    else
        *pdfDensity = 1.0;

    return *pdfDensity != 0.0;
}

/************************************************************************/
/*                          GWKGetPixelShort()                          */
/************************************************************************/

static int GWKGetPixelShort( GDALWarpKernel *poWK, int iBand, 
                             int iSrcOffset, double *pdfDensity, 
                             GInt16 *piValue )

{
    GInt16 *pabySrc = (GInt16 *)poWK->papabySrcImage[iBand];

    if( poWK->panUnifiedSrcValid != NULL
        && !((poWK->panUnifiedSrcValid[iSrcOffset>>5]
              & (0x01 << (iSrcOffset & 0x1f))) ) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    if( poWK->papanBandSrcValid != NULL
        && poWK->papanBandSrcValid[iBand] != NULL
        && !((poWK->papanBandSrcValid[iBand][iSrcOffset>>5]
              & (0x01 << (iSrcOffset & 0x1f)))) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    *piValue = pabySrc[iSrcOffset];

    if( poWK->pafUnifiedSrcDensity != NULL )
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
    else
        *pdfDensity = 1.0;

    return *pdfDensity != 0.0;
}

/************************************************************************/
/*                          GWKGetPixelFloat()                          */
/************************************************************************/

static int GWKGetPixelFloat( GDALWarpKernel *poWK, int iBand, 
                             int iSrcOffset, double *pdfDensity, 
                             float *pfValue )

{
    float *pabySrc = (float *)poWK->papabySrcImage[iBand];

    if( poWK->panUnifiedSrcValid != NULL
        && !((poWK->panUnifiedSrcValid[iSrcOffset>>5]
              & (0x01 << (iSrcOffset & 0x1f))) ) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    if( poWK->papanBandSrcValid != NULL
        && poWK->papanBandSrcValid[iBand] != NULL
        && !((poWK->papanBandSrcValid[iBand][iSrcOffset>>5]
              & (0x01 << (iSrcOffset & 0x1f)))) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    *pfValue = pabySrc[iSrcOffset];

    if( poWK->pafUnifiedSrcDensity != NULL )
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
    else
        *pdfDensity = 1.0;

    return *pdfDensity != 0.0;
}

/************************************************************************/
/*                        GWKBilinearResample()                         */
/*     Set of bilinear interpolators                                    */
/************************************************************************/

static int GWKBilinearResample( GDALWarpKernel *poWK, int iBand, 
                                double dfSrcX, double dfSrcY,
                                double *pdfDensity, 
                                double *pdfReal, double *pdfImag )

{
    double  dfAccumulatorReal = 0.0, dfAccumulatorImag = 0.0;
    double  dfAccumulatorDensity = 0.0;
    double  dfAccumulatorDivisor = 0.0;

    int     iSrcX = (int) floor(dfSrcX - 0.5);
    int     iSrcY = (int) floor(dfSrcY - 0.5);
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfRatioX = 1.5 - (dfSrcX - iSrcX);
    double  dfRatioY = 1.5 - (dfSrcY - iSrcY);

    // Upper Left Pixel
    if( iSrcX >= 0 && iSrcX < poWK->nSrcXSize
        && iSrcY >= 0 && iSrcY < poWK->nSrcYSize
        && GWKGetPixelValue( poWK, iBand, iSrcOffset, pdfDensity, 
                             pdfReal, pdfImag )
        && *pdfDensity != 0.0 )
    {
        double dfMult = dfRatioX * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulatorReal += *pdfReal * dfMult;
        dfAccumulatorImag += *pdfImag * dfMult;
        dfAccumulatorDensity += *pdfDensity * dfMult;
    }
        
    // Upper Right Pixel
    if( iSrcX+1 >= 0 && iSrcX+1 < poWK->nSrcXSize
        && iSrcY >= 0 && iSrcY < poWK->nSrcYSize
        && GWKGetPixelValue( poWK, iBand, iSrcOffset+1, pdfDensity, 
                             pdfReal, pdfImag )
        && *pdfDensity != 0.0 )
    {
        double dfMult = (1.0-dfRatioX) * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulatorReal += *pdfReal * dfMult;
        dfAccumulatorImag += *pdfImag * dfMult;
        dfAccumulatorDensity += *pdfDensity * dfMult;
    }
        
    // Lower Right Pixel
    if( iSrcX+1 >= 0 && iSrcX+1 < poWK->nSrcXSize
        && iSrcY+1 >= 0 && iSrcY+1 < poWK->nSrcYSize
        && GWKGetPixelValue( poWK, iBand, iSrcOffset+1+poWK->nSrcXSize, 
                             pdfDensity, pdfReal, pdfImag )
        && *pdfDensity != 0.0 )
    {
        double dfMult = (1.0-dfRatioX) * (1.0-dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulatorReal += *pdfReal * dfMult;
        dfAccumulatorImag += *pdfImag * dfMult;
        dfAccumulatorDensity += *pdfDensity * dfMult;
    }
        
    // Lower Left Pixel
    if( iSrcX >= 0 && iSrcX < poWK->nSrcXSize
        && iSrcY+1 >= 0 && iSrcY+1 < poWK->nSrcYSize
        && GWKGetPixelValue( poWK, iBand, iSrcOffset+poWK->nSrcXSize, 
                             pdfDensity, pdfReal, pdfImag )
        && *pdfDensity != 0.0 )
    {
        double dfMult = dfRatioX * (1.0-dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulatorReal += *pdfReal * dfMult;
        dfAccumulatorImag += *pdfImag * dfMult;
        dfAccumulatorDensity += *pdfDensity * dfMult;
    }

/* -------------------------------------------------------------------- */
/*      Return result.                                                  */
/* -------------------------------------------------------------------- */
    if( dfAccumulatorDivisor == 1.0 )
    {
        *pdfReal = dfAccumulatorReal;
        *pdfImag = dfAccumulatorImag;
        *pdfDensity = dfAccumulatorDensity;
        return TRUE;
    }
    else if( dfAccumulatorDivisor < 0.00001 )
    {
        *pdfReal = 0.0;
        *pdfImag = 0.0;
        *pdfDensity = 0.0;
        return FALSE;
    }
    else
    {
        *pdfReal = dfAccumulatorReal / dfAccumulatorDivisor;
        *pdfImag = dfAccumulatorImag / dfAccumulatorDivisor;
        *pdfDensity = dfAccumulatorDensity / dfAccumulatorDivisor;
        return TRUE;
    }
}

static int GWKBilinearResampleNoMasksByte( GDALWarpKernel *poWK, int iBand, 
                                           double dfSrcX, double dfSrcY,
                                           GByte *pbValue )

{
    double  dfAccumulator = 0.0;
    double  dfAccumulatorDivisor = 0.0;

    int     iSrcX = (int) floor(dfSrcX - 0.5);
    int     iSrcY = (int) floor(dfSrcY - 0.5);
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfRatioX = 1.5 - (dfSrcX - iSrcX);
    double  dfRatioY = 1.5 - (dfSrcY - iSrcY);

    // Upper Left Pixel
    if( iSrcX >= 0 && iSrcX < poWK->nSrcXSize
        && iSrcY >= 0 && iSrcY < poWK->nSrcYSize )
    {
        double dfMult = dfRatioX * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)poWK->papabySrcImage[iBand][iSrcOffset] * dfMult;
    }
        
    // Upper Right Pixel
    if( iSrcX+1 >= 0 && iSrcX+1 < poWK->nSrcXSize
        && iSrcY >= 0 && iSrcY < poWK->nSrcYSize )
    {
        double dfMult = (1.0-dfRatioX) * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)poWK->papabySrcImage[iBand][iSrcOffset+1] * dfMult;
    }
        
    // Lower Right Pixel
    if( iSrcX+1 >= 0 && iSrcX+1 < poWK->nSrcXSize
        && iSrcY+1 >= 0 && iSrcY+1 < poWK->nSrcYSize )
    {
        double dfMult = (1.0-dfRatioX) * (1.0-dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)poWK->papabySrcImage[iBand][iSrcOffset+1+poWK->nSrcXSize]
            * dfMult;
    }
        
    // Lower Left Pixel
    if( iSrcX >= 0 && iSrcX < poWK->nSrcXSize
        && iSrcY+1 >= 0 && iSrcY+1 < poWK->nSrcYSize )
    {
        double dfMult = dfRatioX * (1.0-dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)poWK->papabySrcImage[iBand][iSrcOffset+poWK->nSrcXSize]
            * dfMult;
    }

/* -------------------------------------------------------------------- */
/*      Return result.                                                  */
/* -------------------------------------------------------------------- */
    double      dfValue;

    if( dfAccumulatorDivisor < 0.00001 )
    {
        *pbValue = 0;
        return FALSE;
    }
    else if( dfAccumulatorDivisor == 1.0 )
    {
        dfValue = dfAccumulator;
    }
    else
    {
        dfValue = dfAccumulator / dfAccumulatorDivisor;
    }

    if ( dfValue < 0.0 )
        *pbValue = 0;
    else if ( dfValue > 255.0 )
        *pbValue = 255;
    else
        *pbValue = (GByte)(0.5 + dfValue);
    
    return TRUE;
}

static int GWKBilinearResampleNoMasksShort( GDALWarpKernel *poWK, int iBand, 
                                            double dfSrcX, double dfSrcY,
                                            GInt16 *piValue )

{
    double  dfAccumulator = 0.0;
    double  dfAccumulatorDivisor = 0.0;

    int     iSrcX = (int) floor(dfSrcX - 0.5);
    int     iSrcY = (int) floor(dfSrcY - 0.5);
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfRatioX = 1.5 - (dfSrcX - iSrcX);
    double  dfRatioY = 1.5 - (dfSrcY - iSrcY);

    // Upper Left Pixel
    if( iSrcX >= 0 && iSrcX < poWK->nSrcXSize
        && iSrcY >= 0 && iSrcY < poWK->nSrcYSize )
    {
        double dfMult = dfRatioX * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)((GInt16 *)poWK->papabySrcImage[iBand])[iSrcOffset]
            * dfMult;
    }
        
    // Upper Right Pixel
    if( iSrcX+1 >= 0 && iSrcX+1 < poWK->nSrcXSize
        && iSrcY >= 0 && iSrcY < poWK->nSrcYSize )
    {
        double dfMult = (1.0-dfRatioX) * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)((GInt16 *)poWK->papabySrcImage[iBand])[iSrcOffset+1] * dfMult;
    }
        
    // Lower Right Pixel
    if( iSrcX+1 >= 0 && iSrcX+1 < poWK->nSrcXSize
        && iSrcY+1 >= 0 && iSrcY+1 < poWK->nSrcYSize )
    {
        double dfMult = (1.0-dfRatioX) * (1.0-dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)((GInt16 *)poWK->papabySrcImage[iBand])[iSrcOffset+1+poWK->nSrcXSize]
            * dfMult;
    }
        
    // Lower Left Pixel
    if( iSrcX >= 0 && iSrcX < poWK->nSrcXSize
        && iSrcY+1 >= 0 && iSrcY+1 < poWK->nSrcYSize )
    {
        double dfMult = dfRatioX * (1.0-dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)((GInt16 *)poWK->papabySrcImage[iBand])[iSrcOffset+poWK->nSrcXSize]
            * dfMult;
    }

/* -------------------------------------------------------------------- */
/*      Return result.                                                  */
/* -------------------------------------------------------------------- */
    if( dfAccumulatorDivisor == 1.0 )
    {
        *piValue = (GInt16)(0.5 + dfAccumulator);
        return TRUE;
    }
    else if( dfAccumulatorDivisor < 0.00001 )
    {
        *piValue = 0;
        return FALSE;
    }
    else
    {
        *piValue = (GInt16)(0.5 + dfAccumulator / dfAccumulatorDivisor);
        return TRUE;
    }
}

/************************************************************************/
/*                        GWKCubicResample()                            */
/*     Set of bicubic interpolators using cubic convolution.            */
/************************************************************************/

#define CubicConvolution(distance1,distance2,distance3,f0,f1,f2,f3) \
   (  (   -f0 +     f1 - f2 + f3) * distance3 \
    + (2.0*(f0 - f1) + f2 - f3) * distance2 \
    + (   -f0          + f2     ) * distance1 \
    +               f1                         )

static int GWKCubicResample( GDALWarpKernel *poWK, int iBand,
                             double dfSrcX, double dfSrcY,
                             double *pdfDensity,
                             double *pdfReal, double *pdfImag )

{
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    double  dfDeltaX2 = dfDeltaX * dfDeltaX;
    double  dfDeltaY2 = dfDeltaY * dfDeltaY;
    double  dfDeltaX3 = dfDeltaX2 * dfDeltaX;
    double  dfDeltaY3 = dfDeltaY2 * dfDeltaY;
    double  dfDensity0, dfDensity1, dfDensity2, dfDensity3;
    double  dfReal0, dfReal1, dfReal2, dfReal3;
    double  dfImag0, dfImag1, dfImag2, dfImag3;
    double  adfValueDens[4], adfValueReal[4], adfValueImag[4];
    int     i;

    // Get the bilinear interpolation at the image borders
    if ( iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize
         || iSrcY - 1 < 0 || iSrcY + 2 >= poWK->nSrcYSize )
        return GWKBilinearResample( poWK, iBand, dfSrcX, dfSrcY,
                                    pdfDensity, pdfReal, pdfImag );

    for ( i = -1; i < 3; i++ )
    {
        int     iOffset = iSrcOffset + i * poWK->nSrcXSize;

        if ( !GWKGetPixelValue( poWK, iBand, iOffset - 1,
                                &dfDensity0, &dfReal0, &dfImag0 ) )
            return FALSE;

        if ( !GWKGetPixelValue( poWK, iBand, iOffset,
                                &dfDensity1, &dfReal1, &dfImag1 ) )
            return FALSE;

        if ( !GWKGetPixelValue( poWK, iBand, iOffset + 1,
                                &dfDensity2, &dfReal2, &dfImag2 ) )
            return FALSE;

        if ( !GWKGetPixelValue( poWK, iBand, iOffset + 2,
                                &dfDensity3, &dfReal3, &dfImag3 ) )
            return FALSE;

        adfValueDens[i + 1] = CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
                            dfDensity0, dfDensity1, dfDensity2, dfDensity3);
        adfValueReal[i + 1] = CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
                            dfReal0, dfReal1, dfReal2, dfReal3);
        adfValueImag[i + 1] = CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
                        dfImag0, dfImag1, dfImag2, dfImag3);
    }

    *pdfDensity = CubicConvolution(dfDeltaY, dfDeltaY2, dfDeltaY3,
                                   adfValueDens[0], adfValueDens[1],
                                   adfValueDens[2], adfValueDens[3]);
    *pdfReal = CubicConvolution(dfDeltaY, dfDeltaY2, dfDeltaY3,
                                   adfValueReal[0], adfValueReal[1],
                                   adfValueReal[2], adfValueReal[3]);
    *pdfImag = CubicConvolution(dfDeltaY, dfDeltaY2, dfDeltaY3,
                                   adfValueImag[0], adfValueImag[1],
                                   adfValueImag[2], adfValueImag[3]);
    
    return TRUE;
}

static int GWKCubicResampleNoMasksByte( GDALWarpKernel *poWK, int iBand,
                                        double dfSrcX, double dfSrcY,
                                        GByte *pbValue )

{
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    double  dfDeltaX2 = dfDeltaX * dfDeltaX;
    double  dfDeltaY2 = dfDeltaY * dfDeltaY;
    double  dfDeltaX3 = dfDeltaX2 * dfDeltaX;
    double  dfDeltaY3 = dfDeltaY2 * dfDeltaY;
    double  adfValue[4];
    int     i;

    // Get the bilinear interpolation at the image borders
    if ( iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize
         || iSrcY - 1 < 0 || iSrcY + 2 >= poWK->nSrcYSize )
        return GWKBilinearResampleNoMasksByte( poWK, iBand, dfSrcX, dfSrcY,
                                               pbValue);

    for ( i = -1; i < 3; i++ )
    {
        int     iOffset = iSrcOffset + i * poWK->nSrcXSize;

        adfValue[i + 1] = CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
                            (double)poWK->papabySrcImage[iBand][iOffset - 1],
                            (double)poWK->papabySrcImage[iBand][iOffset],
                            (double)poWK->papabySrcImage[iBand][iOffset + 1],
                            (double)poWK->papabySrcImage[iBand][iOffset + 2]);
    }

    double dfValue = CubicConvolution(dfDeltaY, dfDeltaY2, dfDeltaY3,
                        adfValue[0], adfValue[1], adfValue[2], adfValue[3]);

    if ( dfValue < 0.0 )
        *pbValue = 0;
    else if ( dfValue > 255.0 )
        *pbValue = 255;
    else
        *pbValue = (GByte)(0.5 + dfValue);
    
    return TRUE;
}

static int GWKCubicResampleNoMasksShort( GDALWarpKernel *poWK, int iBand,
                                         double dfSrcX, double dfSrcY,
                                         GInt16 *piValue )

{
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    double  dfDeltaX2 = dfDeltaX * dfDeltaX;
    double  dfDeltaY2 = dfDeltaY * dfDeltaY;
    double  dfDeltaX3 = dfDeltaX2 * dfDeltaX;
    double  dfDeltaY3 = dfDeltaY2 * dfDeltaY;
    double  adfValue[4];
    int     i;

    // Get the bilinear interpolation at the image borders
    if ( iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize
         || iSrcY - 1 < 0 || iSrcY + 2 >= poWK->nSrcYSize )
        return GWKBilinearResampleNoMasksShort( poWK, iBand, dfSrcX, dfSrcY,
                                                piValue);

    for ( i = -1; i < 3; i++ )
    {
        int     iOffset = iSrcOffset + i * poWK->nSrcXSize;

        adfValue[i + 1] =CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
                (double)((GInt16 *)poWK->papabySrcImage[iBand])[iOffset - 1],
                (double)((GInt16 *)poWK->papabySrcImage[iBand])[iOffset],
                (double)((GInt16 *)poWK->papabySrcImage[iBand])[iOffset + 1],
                (double)((GInt16 *)poWK->papabySrcImage[iBand])[iOffset + 2]);
    }

    *piValue = (GInt16)CubicConvolution(dfDeltaY, dfDeltaY2, dfDeltaY3,
                        adfValue[0], adfValue[1], adfValue[2], adfValue[3]);
    
    return TRUE;
}

/************************************************************************/
/*                    GWKCubicSplineResample()                          */
/*     Set of bicubic interpolators using B-splines.                    */
/************************************************************************/

#define P(x) (((x) > 0)?(x)*(x)*(x):0)

static double BSpline( double x )
{
    return ( P(x + 2) - 4 * P(x + 1) + 6 * P(x) - 4 * P(x - 1) ) / 6;
}

static int GWKCubicSplineResample( GDALWarpKernel *poWK, int iBand, 
                                   double dfSrcX, double dfSrcY,
                                   double *pdfDensity, 
                                   double *pdfReal, double *pdfImag )

{
    double  dfAccumulatorReal = 0.0, dfAccumulatorImag = 0.0;
    double  dfAccumulatorDensity = 0.0;
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    int     i, j;

    // Get the bilinear interpolation at the image borders
    if ( iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize
         || iSrcY - 1 < 0 || iSrcY + 2 >= poWK->nSrcYSize )
        return GWKBilinearResample( poWK, iBand, dfSrcX, dfSrcY,
                                    pdfDensity, pdfReal, pdfImag );

    for ( i = -1; i < 3; i++ )
    {
        double  dfWeight1 = BSpline((double)i - dfDeltaX);

        for ( j = -1; j < 3; j++ )
        {
            if ( GWKGetPixelValue( poWK, iBand,
                                   iSrcOffset + i + j  * poWK->nSrcXSize,
                                   pdfDensity, pdfReal, pdfImag ) )
            {
                double  dfWeight2 = dfWeight1 * BSpline(dfDeltaY - (double)j);

                dfAccumulatorReal += *pdfReal * dfWeight2;
                dfAccumulatorImag += *pdfImag * dfWeight2;
                dfAccumulatorDensity += *pdfDensity * dfWeight2;
            }
        }
    }
    
    *pdfReal = dfAccumulatorReal;
    *pdfImag = dfAccumulatorImag;
    *pdfDensity = dfAccumulatorDensity;
    
    return TRUE;
}

static int GWKCubicSplineResampleNoMasksByte( GDALWarpKernel *poWK, int iBand,
                                              double dfSrcX, double dfSrcY,
                                              GByte *pbValue )

{
    double  dfAccumulator = 0.0;
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    int     i, j;

    // Get the bilinear interpolation at the image borders
    if ( iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize
         || iSrcY - 1 < 0 || iSrcY + 2 >= poWK->nSrcYSize )
        return GWKBilinearResampleNoMasksByte( poWK, iBand, dfSrcX, dfSrcY,
                                               pbValue);

    for ( i = -1; i < 3; i++ )
    {
        double  dfWeight1 = BSpline((double)i - dfDeltaX);

        for ( j = -1; j < 3; j++ )
        {
            double  dfWeight2 = dfWeight1 * BSpline(dfDeltaY - (double)j);

            dfAccumulator +=
                (double)poWK->papabySrcImage[iBand][iSrcOffset + i + j * poWK->nSrcXSize]
                * dfWeight2;
        }
    }
    
    if ( dfAccumulator < 0.0 )
        *pbValue = 0;
    else if ( dfAccumulator > 255.0 )
        *pbValue = 255;
    else
        *pbValue = (GByte)(0.5 + dfAccumulator);
     
    return TRUE;
}

static int GWKCubicSplineResampleNoMasksShort( GDALWarpKernel *poWK, int iBand,
                                         double dfSrcX, double dfSrcY,
                                         GInt16 *piValue )

{
    double  dfAccumulator = 0.0;
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    int     i, j;

    // Get the bilinear interpolation at the image borders
    if ( iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize
         || iSrcY - 1 < 0 || iSrcY + 2 >= poWK->nSrcYSize )
        return GWKBilinearResampleNoMasksShort( poWK, iBand, dfSrcX, dfSrcY,
                                                piValue);

    for ( i = -1; i < 3; i++ )
    {
        double  dfWeight1 = BSpline((double)i - dfDeltaX);

        for ( j = -1; j < 3; j++ )
        {
            double  dfWeight2 = dfWeight1 * BSpline(dfDeltaY - (double)j);

            dfAccumulator +=
                (double)((GInt16 *)poWK->papabySrcImage[iBand])[iSrcOffset + i + j * poWK->nSrcXSize]
                * dfWeight2;
        }
    }
    
    *piValue = (GInt16)(0.5 + dfAccumulator);
    
    return TRUE;
}

/************************************************************************/
/*                           GWKGeneralCase()                           */
/*                                                                      */
/*      This is the most general case.  It attempts to handle all       */
/*      possible features with relatively little concern for            */
/*      efficiency.                                                     */
/************************************************************************/

static CPLErr GWKGeneralCase( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKGeneralCase()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      How much of a window around our source pixel might we need      */
/*      to collect data from based on the resampling kernel?  Even      */
/*      if the requested central pixel falls off the source image,      */
/*      we may need to collect data if some portion of the              */
/*      resampling kernel could be on-image.                            */
/* -------------------------------------------------------------------- */
    int nResWinSize = 0;

    if( poWK->eResample == GRA_Bilinear )
        nResWinSize = 1;
    
    if( poWK->eResample == GRA_Cubic )
        nResWinSize = 2;

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            int iDstOffset;

            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff - nResWinSize
                || padfY[iDstX] < poWK->nSrcYOff - nResWinSize )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize + nResWinSize 
                || iSrcY >= nSrcYSize + nResWinSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* -------------------------------------------------------------------- */
/*      Don't generate output pixels for which the destination valid    */
/*      mask exists and is already set.                                 */
/* -------------------------------------------------------------------- */
            iDstOffset = iDstX + iDstY * nDstXSize;
            if( poWK->panDstValid != NULL
                && (poWK->panDstValid[iDstOffset>>5]
                    & (0x01 << (iDstOffset & 0x1f))) )
                continue;

/* -------------------------------------------------------------------- */
/*      Do not try to apply transparent/invalid source pixels to the    */
/*      destination.  This currently ignores the multi-pixel input      */
/*      of bilinear and cubic resamples.                                */
/* -------------------------------------------------------------------- */
            double  dfDensity = 1.0;

            if( poWK->pafUnifiedSrcDensity != NULL 
                && iSrcX >= 0 && iSrcY >= 0 
                && iSrcX < nSrcXSize && iSrcY < nSrcYSize )
            {
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                if( dfDensity < 0.00001 )
                    continue;
            }

            if( poWK->panUnifiedSrcValid != NULL
                && iSrcX >= 0 && iSrcY >= 0 
                && iSrcX < nSrcXSize && iSrcY < nSrcYSize 
                && !(poWK->panUnifiedSrcValid[iSrcOffset>>5]
                     & (0x01 << (iSrcOffset & 0x1f))) )
                continue;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            
            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                double dfBandDensity = 0.0;
                double dfValueReal = 0.0;
                double dfValueImag = 0.0;

/* -------------------------------------------------------------------- */
/*      Collect the source value.                                       */
/* -------------------------------------------------------------------- */
                if( poWK->eResample == GRA_NearestNeighbour )
                {
                    GWKGetPixelValue( poWK, iBand, iSrcOffset, &dfBandDensity, 
                                      &dfValueReal, &dfValueImag );
                }
                else if( poWK->eResample == GRA_Bilinear )
                {
                    GWKBilinearResample( poWK, iBand, 
                                         padfX[iDstX]-poWK->nSrcXOff,
                                         padfY[iDstX]-poWK->nSrcYOff,
                                         &dfBandDensity, 
                                         &dfValueReal, &dfValueImag );
                }
                else if( poWK->eResample == GRA_Cubic )
                {
                    GWKCubicResample( poWK, iBand, 
                                      padfX[iDstX]-poWK->nSrcXOff,
                                      padfY[iDstX]-poWK->nSrcYOff,
                                      &dfBandDensity, 
                                      &dfValueReal, &dfValueImag );
                }
                else if( poWK->eResample == GRA_CubicSpline )
                {
                    GWKCubicSplineResample( poWK, iBand, 
                                            padfX[iDstX]-poWK->nSrcXOff,
                                            padfY[iDstX]-poWK->nSrcYOff,
                                            &dfBandDensity, 
                                            &dfValueReal, &dfValueImag );
                }


                // If we didn't find any valid inputs skip to next band.
                if( dfBandDensity == 0.0 )
                    continue;

/* -------------------------------------------------------------------- */
/*      We have a computed value from the source.  Now apply it to      */
/*      the destination pixel.                                          */
/* -------------------------------------------------------------------- */
                GWKSetPixelValue( poWK, iBand, iDstOffset,
                                  dfBandDensity, dfValueReal, dfValueImag );

            }

/* -------------------------------------------------------------------- */
/*      Update destination density/validity masks.                      */
/* -------------------------------------------------------------------- */
            GWKOverlayDensity( poWK, iDstOffset, dfDensity );

            if( poWK->panDstValid != NULL )
            {
                poWK->panDstValid[iDstOffset>>5] |= 
                    0x01 << (iDstOffset & 0x1f);
            }

        } /* Next iDstX */

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                       GWKNearestNoMasksByte()                        */
/*                                                                      */
/*      Case for 8bit input data with nearest neighbour resampling      */
/*      without concerning about masking. Should be as fast as          */
/*      possible for this particular transformation type.               */
/************************************************************************/

static CPLErr GWKNearestNoMasksByte( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKNearestNoMasksByte()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            int iDstOffset;

            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                poWK->papabyDstImage[iBand][iDstOffset] = 
                    poWK->papabySrcImage[iBand][iSrcOffset];
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                       GWKBilinearNoMasksByte()                       */
/*                                                                      */
/*      Case for 8bit input data with cubic resampling without          */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

static CPLErr GWKBilinearNoMasksByte( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKBilinearNoMasksByte()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            int iDstOffset;

            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                GWKBilinearResampleNoMasksByte( poWK, iBand,
                                                padfX[iDstX]-poWK->nSrcXOff,
                                                padfY[iDstX]-poWK->nSrcYOff,
                                                &poWK->papabyDstImage[iBand][iDstOffset] );
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                       GWKCubicNoMasksByte()                          */
/*                                                                      */
/*      Case for 8bit input data with cubic resampling without          */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

static CPLErr GWKCubicNoMasksByte( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKCubicNoMasksByte()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            int iDstOffset;

            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                GWKCubicResampleNoMasksByte( poWK, iBand,
                                             padfX[iDstX]-poWK->nSrcXOff,
                                             padfY[iDstX]-poWK->nSrcYOff,
                                             &poWK->papabyDstImage[iBand][iDstOffset] );
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                   GWKCubicSplineNoMasksByte()                        */
/*                                                                      */
/*      Case for 8bit input data with cubic resampling without          */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

static CPLErr GWKCubicSplineNoMasksByte( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKCubicSplineNoMasksByte()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            int iDstOffset;

            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                GWKCubicSplineResampleNoMasksByte( poWK, iBand,
                                                   padfX[iDstX]-poWK->nSrcXOff,
                                                   padfY[iDstX]-poWK->nSrcYOff,
                                                   &poWK->papabyDstImage[iBand][iDstOffset] );
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                          GWKNearestByte()                            */
/*                                                                      */
/*      Case for 8bit input data with nearest neighbour resampling      */
/*      using valid flags. Should be as fast as possible for this       */
/*      particular transformation type.                                 */
/************************************************************************/

static CPLErr GWKNearestByte( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKNearestByte()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            int iDstOffset;

            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* -------------------------------------------------------------------- */
/*      Don't generate output pixels for which the destination valid    */
/*      mask exists and is already set.                                 */
/*                                                                      */
/*      -- NFW -- FIXME: I think this test is in error.  We should,     */
/*      generally, be pasteing over existing valid data.                */
/* -------------------------------------------------------------------- */
            iDstOffset = iDstX + iDstY * nDstXSize;
            if( poWK->panDstValid != NULL
                && (poWK->panDstValid[iDstOffset>>5]
                    & (0x01 << (iDstOffset & 0x1f))) )
                continue;

/* -------------------------------------------------------------------- */
/*      Do not try to apply invalid source pixels to the dest.          */
/* -------------------------------------------------------------------- */
            if( poWK->panUnifiedSrcValid != NULL
                && !(poWK->panUnifiedSrcValid[iSrcOffset>>5]
                     & (0x01 << (iSrcOffset & 0x1f))) )
                continue;

/* -------------------------------------------------------------------- */
/*      Do not try to apply transparent source pixels to the destination.*/
/* -------------------------------------------------------------------- */
            double  dfDensity = 1.0;

            if( poWK->pafUnifiedSrcDensity != NULL )
            {
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                if( dfDensity < 0.00001 )
                    continue;
            }

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                GByte   bValue = 0;
                double dfBandDensity = 0.0;

/* -------------------------------------------------------------------- */
/*      Collect the source value.                                       */
/* -------------------------------------------------------------------- */
                if ( GWKGetPixelByte( poWK, iBand, iSrcOffset, &dfBandDensity,
                                      &bValue ) )
                {
                    if( dfBandDensity < 1.0 )
                    {
                        if( dfBandDensity == 0.0 )
                            /* do nothing */;
                        else
                        {
                            /* let the general code take care of mixing */
                            GWKSetPixelValue( poWK, iBand, iDstOffset, 
                                              dfBandDensity, (double) bValue, 
                                              0.0 );
                        }
                    }
                    else
                    {
                        poWK->papabyDstImage[iBand][iDstOffset] = bValue;
                    }
                }
            }

/* -------------------------------------------------------------------- */
/*      Mark this pixel valid/opaque in the output.                     */
/* -------------------------------------------------------------------- */
            GWKOverlayDensity( poWK, iDstOffset, dfDensity );

            if( poWK->panDstValid != NULL )
            {
                poWK->panDstValid[iDstOffset>>5] |= 
                    0x01 << (iDstOffset & 0x1f);
            }
        } /* Next iDstX */

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    } /* Next iDstY */

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                    GWKNearestNoMasksShort()                          */
/*                                                                      */
/*      Case for 16bit signed and unsigned integer input data with      */
/*      nearest neighbour resampling without concerning about masking.  */
/*      Should be as fast as possible for this particular               */
/*      transformation type.                                            */
/************************************************************************/

static CPLErr GWKNearestNoMasksShort( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKNearestNoMasksShort()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            int iDstOffset;

            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            
            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                ((GInt16 *)poWK->papabyDstImage[iBand])[iDstOffset] = 
                    ((GInt16 *)poWK->papabySrcImage[iBand])[iSrcOffset];
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                       GWKBilinearNoMasksShort()                      */
/*                                                                      */
/*      Case for 16bit input data with cubic resampling without         */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

static CPLErr GWKBilinearNoMasksShort( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKBilinearNoMasksShort()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            int iDstOffset;

            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                GInt16  iValue = 0;
                GWKBilinearResampleNoMasksShort( poWK, iBand,
                                                 padfX[iDstX]-poWK->nSrcXOff,
                                                 padfY[iDstX]-poWK->nSrcYOff,
                                                 &iValue );
                ((GInt16 *)poWK->papabyDstImage[iBand])[iDstOffset] = iValue;
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                       GWKCubicNoMasksShort()                         */
/*                                                                      */
/*      Case for 16bit input data with cubic resampling without         */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

static CPLErr GWKCubicNoMasksShort( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKCubicNoMasksShort()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            int iDstOffset;

            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                GInt16  iValue = 0;
                GWKCubicResampleNoMasksShort( poWK, iBand,
                                              padfX[iDstX]-poWK->nSrcXOff,
                                              padfY[iDstX]-poWK->nSrcYOff,
                                              &iValue );
                ((GInt16 *)poWK->papabyDstImage[iBand])[iDstOffset] = iValue;
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                    GWKCubicSplineNoMasksShort()                      */
/*                                                                      */
/*      Case for 16bit input data with cubic resampling without         */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

static CPLErr GWKCubicSplineNoMasksShort( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKCubicSplineNoMasksShort()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            int iDstOffset;

            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                GInt16  iValue = 0;
                GWKCubicSplineResampleNoMasksShort( poWK, iBand,
                                                    padfX[iDstX]-poWK->nSrcXOff,
                                                    padfY[iDstX]-poWK->nSrcYOff,
                                                    &iValue );
                ((GInt16 *)poWK->papabyDstImage[iBand])[iDstOffset] = iValue;
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                          GWKNearestShort()                           */
/*                                                                      */
/*      Case for 32bit float input data with nearest neighbour          */
/*      resampling using valid flags. Should be as fast as possible     */
/*      for this particular transformation type.                        */
/************************************************************************/

static CPLErr GWKNearestShort( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKNearestShort()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            int iDstOffset;

            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* -------------------------------------------------------------------- */
/*      Don't generate output pixels for which the destination valid    */
/*      mask exists and is already set.                                 */
/* -------------------------------------------------------------------- */
            iDstOffset = iDstX + iDstY * nDstXSize;
            if( poWK->panDstValid != NULL
                && (poWK->panDstValid[iDstOffset>>5]
                    & (0x01 << (iDstOffset & 0x1f))) )
                continue;

/* -------------------------------------------------------------------- */
/*      Don't generate output pixels for which the source valid         */
/*      mask exists and is invalid.                                     */
/* -------------------------------------------------------------------- */
            if( poWK->panUnifiedSrcValid != NULL
                && !(poWK->panUnifiedSrcValid[iSrcOffset>>5]
                     & (0x01 << (iSrcOffset & 0x1f))) )
                continue;

/* -------------------------------------------------------------------- */
/*      Do not try to apply transparent source pixels to the destination.*/
/* -------------------------------------------------------------------- */
            double  dfDensity = 1.0;

            if( poWK->pafUnifiedSrcDensity != NULL )
            {
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                if( dfDensity < 0.00001 )
                    continue;
            }

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                GInt16  iValue = 0;
                double dfBandDensity = 0.0;

/* -------------------------------------------------------------------- */
/*      Collect the source value.                                       */
/* -------------------------------------------------------------------- */
                if ( GWKGetPixelShort( poWK, iBand, iSrcOffset, &dfBandDensity,
                                       &iValue ) )
                {
                    if( dfBandDensity < 1.0 )
                    {
                        if( dfBandDensity == 0.0 )
                            /* do nothing */;
                        else
                        {
                            /* let the general code take care of mixing */
                            GWKSetPixelValue( poWK, iBand, iDstOffset, 
                                              dfBandDensity, (double) iValue, 
                                              0.0 );
                        }
                    }
                    else
                    {
                        ((GInt16 *)poWK->papabyDstImage[iBand])[iDstOffset] = iValue;
                    }
                }
            }

/* -------------------------------------------------------------------- */
/*      Mark this pixel valid/opaque in the output.                     */
/* -------------------------------------------------------------------- */
            GWKOverlayDensity( poWK, iDstOffset, dfDensity );

            if( poWK->panDstValid != NULL )
            {
                poWK->panDstValid[iDstOffset>>5] |= 
                    0x01 << (iDstOffset & 0x1f);
            }
        } /* Next iDstX */

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    } /* Next iDstY */

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                    GWKNearestNoMasksFloat()                          */
/*                                                                      */
/*      Case for 32bit float input data with nearest neighbour          */
/*      resampling without concerning about masking. Should be as fast  */
/*      as possible for this particular transformation type.            */
/************************************************************************/

static CPLErr GWKNearestNoMasksFloat( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKNearestNoMasksFloat()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            int iDstOffset;

            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;
            
            iDstOffset = iDstX + iDstY * nDstXSize;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                ((float *)poWK->papabyDstImage[iBand])[iDstOffset] = 
                    ((float *)poWK->papabySrcImage[iBand])[iSrcOffset];
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

/************************************************************************/
/*                          GWKNearestFloat()                           */
/*                                                                      */
/*      Case for 32bit float input data with nearest neighbour          */
/*      resampling using valid flags. Should be as fast as possible     */
/*      for this particular transformation type.                        */
/************************************************************************/

static CPLErr GWKNearestFloat( GDALWarpKernel *poWK )

{
    int iDstY;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    CPLErr eErr = CE_None;

    CPLDebug( "GDAL", "GDALWarpKernel()::GWKNearestFloat()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              poWK->nSrcXOff, poWK->nSrcYOff, 
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff, 
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        int iDstX;

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + 0.5 + poWK->nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            int iDstOffset;

            if( !pabSuccess[iDstX] )
                continue;

/* -------------------------------------------------------------------- */
/*      Figure out what pixel we want in our source raster, and skip    */
/*      further processing if it is well off the source image.          */
/* -------------------------------------------------------------------- */
            // We test against the value before casting to avoid the
            // problem of asymmetric truncation effects around zero.  That is
            // -0.5 will be 0 when cast to an int. 
            if( padfX[iDstX] < poWK->nSrcXOff 
                || padfY[iDstX] < poWK->nSrcYOff )
                continue;

            int iSrcX, iSrcY, iSrcOffset;

            iSrcX = ((int) padfX[iDstX]) - poWK->nSrcXOff;
            iSrcY = ((int) padfY[iDstX]) - poWK->nSrcYOff;

            if( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize )
                continue;

            iSrcOffset = iSrcX + iSrcY * nSrcXSize;

/* -------------------------------------------------------------------- */
/*      Don't generate output pixels for which the destination valid    */
/*      mask exists and is already set.                                 */
/* -------------------------------------------------------------------- */
            iDstOffset = iDstX + iDstY * nDstXSize;
            if( poWK->panDstValid != NULL
                && (poWK->panDstValid[iDstOffset>>5]
                    & (0x01 << (iDstOffset & 0x1f))) )
                continue;

/* -------------------------------------------------------------------- */
/*      Do not try to apply invalid source pixels to the dest.          */
/* -------------------------------------------------------------------- */
            if( poWK->panUnifiedSrcValid != NULL
                && !(poWK->panUnifiedSrcValid[iSrcOffset>>5]
                     & (0x01 << (iSrcOffset & 0x1f))) )
                continue;

/* -------------------------------------------------------------------- */
/*      Do not try to apply transparent source pixels to the destination.*/
/* -------------------------------------------------------------------- */
            double  dfDensity = 1.0;

            if( poWK->pafUnifiedSrcDensity != NULL )
            {
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                if( dfDensity < 0.00001 )
                    continue;
            }

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            int iBand;

            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                float   fValue = 0;
                double dfBandDensity = 0.0;

/* -------------------------------------------------------------------- */
/*      Collect the source value.                                       */
/* -------------------------------------------------------------------- */
                if ( GWKGetPixelFloat( poWK, iBand, iSrcOffset, &dfBandDensity,
                                       &fValue ) )
                {
                    if( dfBandDensity < 1.0 )
                    {
                        if( dfBandDensity == 0.0 )
                            /* do nothing */;
                        else
                        {
                            /* let the general code take care of mixing */
                            GWKSetPixelValue( poWK, iBand, iDstOffset, 
                                              dfBandDensity, (double) fValue, 
                                              0.0 );
                        }
                    }
                    else
                    {
                        ((float *)poWK->papabyDstImage[iBand])[iDstOffset] 
                            = fValue;
                    }
                }
            }

/* -------------------------------------------------------------------- */
/*      Mark this pixel valid/opaque in the output.                     */
/* -------------------------------------------------------------------- */
            GWKOverlayDensity( poWK, iDstOffset, dfDensity );

            if( poWK->panDstValid != NULL )
            {
                poWK->panDstValid[iDstOffset>>5] |= 
                    0x01 << (iDstOffset & 0x1f);
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                ((iDstY+1) / (double) nDstYSize), 
                                "", poWK->pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            eErr = CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );

    return eErr;
}

