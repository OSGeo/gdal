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
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  2003/02/18 17:25:50  warmerda
 * New
 *
 */

#include "gdalwarper.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

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
 *       bIsValid = panBandMask[iPixelOffset>>32] 
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
/*                            PerformWarp()                             */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpKernel::PerformWarp();
 * 
 * This method performs the warp described in the GDALWarpKernel.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */
                                  
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
