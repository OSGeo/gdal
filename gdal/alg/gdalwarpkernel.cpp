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
 ****************************************************************************/

#include "gdalwarper.h"
#include "cpl_string.h"
#include "gdalwarpkernel_opencl.h"

CPL_CVSID("$Id$");

static const int anGWKFilterRadius[] =
{
    0,      // Nearest neighbour
    1,      // Bilinear
    2,      // Cubic Convolution
    2,      // Cubic B-Spline
    3       // Lanczos windowed sinc
};

/* Used in gdalwarpoperation.cpp */
int GWKGetFilterRadius(GDALResampleAlg eResampleAlg)
{
    return anGWKFilterRadius[eResampleAlg];
}

#ifdef HAVE_OPENCL
static CPLErr GWKOpenCLCase( GDALWarpKernel * );
#endif

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
    dfXScale = 1.0;
    dfYScale = 1.0;
    dfXFilter = 0.0;
    dfYFilter = 0.0;
    nXRadius = 0;
    nYRadius = 0;
    nFiltInitX = 0;
    nFiltInitY = 0;
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

    // See #2445 and #3079
    if (nSrcXSize <= 0 || nSrcYSize <= 0)
    {
        pfnProgress( dfProgressBase + dfProgressScale,
                      "", pProgress );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Pre-calculate resampling scales and window sizes for filtering. */
/* -------------------------------------------------------------------- */

    dfXScale = (double)nDstXSize / nSrcXSize;
    dfYScale = (double)nDstYSize / nSrcYSize;

    dfXFilter = anGWKFilterRadius[eResample];
    dfYFilter = anGWKFilterRadius[eResample];

    nXRadius = ( dfXScale < 1.0 ) ?
        (int)ceil( dfXFilter / dfXScale ) :(int)dfXFilter;
    nYRadius = ( dfYScale < 1.0 ) ?
        (int)ceil( dfYFilter / dfYScale ) : (int)dfYFilter;

    // Filter window offset depends on the parity of the kernel radius
    nFiltInitX = ((anGWKFilterRadius[eResample] + 1) % 2) - nXRadius;
    nFiltInitY = ((anGWKFilterRadius[eResample] + 1) % 2) - nYRadius;

/* -------------------------------------------------------------------- */
/*      Set up resampling functions.                                    */
/* -------------------------------------------------------------------- */
    if( CSLFetchBoolean( papszWarpOptions, "USE_GENERAL_CASE", FALSE ) )
        return GWKGeneralCase( this );

#if defined(HAVE_OPENCL)
    if((eWorkingDataType == GDT_Byte
        || eWorkingDataType == GDT_CInt16
        || eWorkingDataType == GDT_UInt16
        || eWorkingDataType == GDT_Int16
        || eWorkingDataType == GDT_CFloat32
        || eWorkingDataType == GDT_Float32) &&
       (eResample == GRA_Bilinear
        || eResample == GRA_Cubic
        || eResample == GRA_CubicSpline
        || eResample == GRA_Lanczos) &&
        CSLFetchBoolean( papszWarpOptions, "USE_OPENCL", TRUE ))
    {
        CPLErr eResult = GWKOpenCLCase( this );
        
        // CE_Warning tells us a suitable OpenCL environment was not available
        // so we fall through to other CPU based methods.
        if( eResult != CE_Warning )
            return eResult;
    }
#endif /* defined HAVE_OPENCL */

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
    if ( (size_t)eResample >=
         (sizeof(anGWKFilterRadius) / sizeof(anGWKFilterRadius[0])) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported resampling method %d.", (int) eResample );
        return CE_Failure;
    }

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

    poWK->pafDstDensity[iDstOffset] = (float)
        ( 1.0 - (1.0 - dfDensity) * (1.0 - poWK->pafDstDensity[iDstOffset]) );
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
/*      get the new "to apply" value.  Also compute composite           */
/*      density.                                                        */
/*                                                                      */
/*      We avoid mixing if density is very near one or risk mixing      */
/*      in very extreme nodata values and causing odd results (#1610)   */
/* -------------------------------------------------------------------- */
    if( dfDensity < 0.9999 )
    {
        double dfDstReal, dfDstImag, dfDstDensity = 1.0;

        if( dfDensity < 0.0001 )
            return TRUE;

        if( poWK->pafDstDensity != NULL )
            dfDstDensity = poWK->pafDstDensity[iDstOffset];
        else if( poWK->panDstValid != NULL 
                 && !((poWK->panDstValid[iDstOffset>>5]
                       & (0x01 << (iDstOffset & 0x1f))) ) )
            dfDstDensity = 0.0;

        // It seems like we also ought to be testing panDstValid[] here!

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
/*                                                                      */
/*      Avoid using the destination nodata value for integer datatypes  */
/*      if by chance it is equal to the computed pixel value.           */
/* -------------------------------------------------------------------- */

#define CLAMP(type,minval,maxval) \
    do { \
    if (dfReal < minval) ((type *) pabyDst)[iDstOffset] = (type)minval; \
    else if (dfReal > maxval) ((type *) pabyDst)[iDstOffset] = (type)maxval; \
    else ((type *) pabyDst)[iDstOffset] = (minval < 0) ? (type)floor(dfReal + 0.5) : (type)(dfReal + 0.5);  \
    if (poWK->padfDstNoDataReal != NULL && \
        poWK->padfDstNoDataReal[iBand] == (double)((type *) pabyDst)[iDstOffset]) \
    { \
        if (((type *) pabyDst)[iDstOffset] == minval)  \
            ((type *) pabyDst)[iDstOffset] = (type)(minval + 1); \
        else \
            ((type *) pabyDst)[iDstOffset] --; \
    } } while(0)

    switch( poWK->eWorkingDataType )
    {
      case GDT_Byte:
        CLAMP(GByte, 0.0, 255.0);
        break;

      case GDT_Int16:
        CLAMP(GInt16, -32768.0, 32767.0);
        break;

      case GDT_UInt16:
        CLAMP(GUInt16, 0.0, 65535.0);
        break;

      case GDT_UInt32:
        CLAMP(GUInt32, 0.0, 4294967295.0);
        break;

      case GDT_Int32:
        CLAMP(GInt32, -2147483648.0, 2147483647.0);
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
/*                          GWKGetPixelRow()                            */
/************************************************************************/

static int GWKGetPixelRow( GDALWarpKernel *poWK, int iBand, 
                           int iSrcOffset, int nHalfSrcLen,
                           double adfDensity[],
                           double adfReal[], double adfImag[] )
{
    // We know that nSrcLen is even, so we can *always* unroll loops 2x
    int     nSrcLen = nHalfSrcLen * 2;
    int     bHasValid = FALSE;
    int     i;
    
    // Init the density
    for ( i = 0; i < nSrcLen; i += 2 )
    {
        adfDensity[i] = 1.0;
        adfDensity[i+1] = 1.0;
    }
    
    if ( poWK->panUnifiedSrcValid != NULL )
    {
        for ( i = 0; i < nSrcLen; i += 2 )
        {
            if(poWK->panUnifiedSrcValid[(iSrcOffset+i)>>5]
               & (0x01 << ((iSrcOffset+i) & 0x1f)))
                bHasValid = TRUE;
            else
                adfDensity[i] = 0.0;
            
            if(poWK->panUnifiedSrcValid[(iSrcOffset+i+1)>>5]
               & (0x01 << ((iSrcOffset+i+1) & 0x1f)))
                bHasValid = TRUE;
            else
                adfDensity[i+1] = 0.0;
        }

        // Reset or fail as needed
        if ( bHasValid )
            bHasValid = FALSE;
        else
            return FALSE;
    }
    
    if ( poWK->papanBandSrcValid != NULL
         && poWK->papanBandSrcValid[iBand] != NULL)
    {
        for ( i = 0; i < nSrcLen; i += 2 )
        {
            if(poWK->papanBandSrcValid[iBand][(iSrcOffset+i)>>5]
               & (0x01 << ((iSrcOffset+i) & 0x1f)))
                bHasValid = TRUE;
            else
                adfDensity[i] = 0.0;
            
            if(poWK->papanBandSrcValid[iBand][(iSrcOffset+i+1)>>5]
               & (0x01 << ((iSrcOffset+i+1) & 0x1f)))
                bHasValid = TRUE;
            else
                adfDensity[i+1] = 0.0;
        }
        
        // Reset or fail as needed
        if ( bHasValid )
            bHasValid = FALSE;
        else
            return FALSE;
    }
    
    // Fetch data
    switch( poWK->eWorkingDataType )
    {
        case GDT_Byte:
        {
            GByte* pSrc = (GByte*) poWK->papabySrcImage[iBand];
            pSrc += iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[i];
                adfReal[i+1] = pSrc[i+1];
            }
            memset( adfImag, 0, nSrcLen * sizeof(double) );
            break;
        }

        case GDT_Int16:
        {
            GInt16* pSrc = (GInt16*) poWK->papabySrcImage[iBand];
            pSrc += iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[i];
                adfReal[i+1] = pSrc[i+1];
            }
            memset( adfImag, 0, nSrcLen * sizeof(double) );
            break;
        }

         case GDT_UInt16:
         {
            GUInt16* pSrc = (GUInt16*) poWK->papabySrcImage[iBand];
            pSrc += iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[i];
                adfReal[i+1] = pSrc[i+1];
            }
            memset( adfImag, 0, nSrcLen * sizeof(double) );
            break;
         }

        case GDT_Int32:
        {
            GInt32* pSrc = (GInt32*) poWK->papabySrcImage[iBand];
            pSrc += iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[i];
                adfReal[i+1] = pSrc[i+1];
            }
            memset( adfImag, 0, nSrcLen * sizeof(double) );
            break;
        }

        case GDT_UInt32:
        {
            GUInt32* pSrc = (GUInt32*) poWK->papabySrcImage[iBand];
            pSrc += iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[i];
                adfReal[i+1] = pSrc[i+1];
            }
            memset( adfImag, 0, nSrcLen * sizeof(double) );
            break;
        }

        case GDT_Float32:
        {
            float* pSrc = (float*) poWK->papabySrcImage[iBand];
            pSrc += iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[i];
                adfReal[i+1] = pSrc[i+1];
            }
            memset( adfImag, 0, nSrcLen * sizeof(double) );
            break;
        }

       case GDT_Float64:
       {
            double* pSrc = (double*) poWK->papabySrcImage[iBand];
            pSrc += iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[i];
                adfReal[i+1] = pSrc[i+1];
            }
            memset( adfImag, 0, nSrcLen * sizeof(double) );
            break;
       }

        case GDT_CInt16:
        {
            GInt16* pSrc = (GInt16*) poWK->papabySrcImage[iBand];
            pSrc += 2 * iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[2*i];
                adfImag[i] = pSrc[2*i+1];

                adfReal[i+1] = pSrc[2*i+2];
                adfImag[i+1] = pSrc[2*i+3];
            }
            break;
        }

        case GDT_CInt32:
        {
            GInt32* pSrc = (GInt32*) poWK->papabySrcImage[iBand];
            pSrc += 2 * iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[2*i];
                adfImag[i] = pSrc[2*i+1];

                adfReal[i+1] = pSrc[2*i+2];
                adfImag[i+1] = pSrc[2*i+3];
            }
            break;
        }

        case GDT_CFloat32:
        {
            float* pSrc = (float*) poWK->papabySrcImage[iBand];
            pSrc += 2 * iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[2*i];
                adfImag[i] = pSrc[2*i+1];

                adfReal[i+1] = pSrc[2*i+2];
                adfImag[i+1] = pSrc[2*i+3];
            }
            break;
        }


        case GDT_CFloat64:
        {
            double* pSrc = (double*) poWK->papabySrcImage[iBand];
            pSrc += 2 * iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[2*i];
                adfImag[i] = pSrc[2*i+1];

                adfReal[i+1] = pSrc[2*i+2];
                adfImag[i+1] = pSrc[2*i+3];
            }
            break;
        }

        default:
            CPLAssert(FALSE);
            memset( adfDensity, 0, nSrcLen * sizeof(double) );
            return FALSE;
    }
    
    if( poWK->pafUnifiedSrcDensity == NULL )
    {
        for ( i = 0; i < nSrcLen; i += 2 )
        {
            // Take into account earlier calcs
            if(adfDensity[i] > 0.000000001)
            {
                adfDensity[i] = 1.0;
                bHasValid = TRUE;
            }
            
            if(adfDensity[i+1] > 0.000000001)
            {
                adfDensity[i+1] = 1.0;
                bHasValid = TRUE;
            }
        }
    }
    else
    {
        for ( i = 0; i < nSrcLen; i += 2 )
        {
            if(adfDensity[i] > 0.000000001)
                adfDensity[i] = poWK->pafUnifiedSrcDensity[iSrcOffset+i];
            if(adfDensity[i] > 0.000000001)
                bHasValid = TRUE;
            
            if(adfDensity[i+1] > 0.000000001)
                adfDensity[i+1] = poWK->pafUnifiedSrcDensity[iSrcOffset+i+1];
            if(adfDensity[i+1] > 0.000000001)
                bHasValid = TRUE;
        }
    }
    
    return bHasValid;
}

/************************************************************************/
/*                          GWKGetPixelByte()                           */
/************************************************************************/

static int GWKGetPixelByte( GDALWarpKernel *poWK, int iBand, 
                            int iSrcOffset, double *pdfDensity, 
                            GByte *pbValue )

{
    GByte *pabySrc = poWK->papabySrcImage[iBand];

    if ( ( poWK->panUnifiedSrcValid != NULL
           && !((poWK->panUnifiedSrcValid[iSrcOffset>>5]
                 & (0x01 << (iSrcOffset & 0x1f))) ) )
         || ( poWK->papanBandSrcValid != NULL
              && poWK->papanBandSrcValid[iBand] != NULL
              && !((poWK->papanBandSrcValid[iBand][iSrcOffset>>5]
                    & (0x01 << (iSrcOffset & 0x1f)))) ) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    *pbValue = pabySrc[iSrcOffset];

    if ( poWK->pafUnifiedSrcDensity == NULL )
        *pdfDensity = 1.0;
    else
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];

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

    if ( ( poWK->panUnifiedSrcValid != NULL
           && !((poWK->panUnifiedSrcValid[iSrcOffset>>5]
                 & (0x01 << (iSrcOffset & 0x1f))) ) )
         || ( poWK->papanBandSrcValid != NULL
              && poWK->papanBandSrcValid[iBand] != NULL
              && !((poWK->papanBandSrcValid[iBand][iSrcOffset>>5]
                    & (0x01 << (iSrcOffset & 0x1f)))) ) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    *piValue = pabySrc[iSrcOffset];

    if ( poWK->pafUnifiedSrcDensity == NULL )
        *pdfDensity = 1.0;
    else
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];

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

    if ( ( poWK->panUnifiedSrcValid != NULL
           && !((poWK->panUnifiedSrcValid[iSrcOffset>>5]
                 & (0x01 << (iSrcOffset & 0x1f))) ) )
         || ( poWK->papanBandSrcValid != NULL
              && poWK->papanBandSrcValid[iBand] != NULL
              && !((poWK->papanBandSrcValid[iBand][iSrcOffset>>5]
                    & (0x01 << (iSrcOffset & 0x1f)))) ) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    *pfValue = pabySrc[iSrcOffset];

    if ( poWK->pafUnifiedSrcDensity == NULL )
        *pdfDensity = 1.0;
    else
        *pdfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];

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
    // Save as local variables to avoid following pointers
    int     nSrcXSize = poWK->nSrcXSize;
    int     nSrcYSize = poWK->nSrcYSize;
    
    int     iSrcX = (int) floor(dfSrcX - 0.5);
    int     iSrcY = (int) floor(dfSrcY - 0.5);
    int     iSrcOffset;
    double  dfRatioX = 1.5 - (dfSrcX - iSrcX);
    double  dfRatioY = 1.5 - (dfSrcY - iSrcY);
    double  adfDensity[2], adfReal[2], adfImag[2];
    double  dfAccumulatorReal = 0.0, dfAccumulatorImag = 0.0;
    double  dfAccumulatorDensity = 0.0;
    double  dfAccumulatorDivisor = 0.0;
    int     bShifted = FALSE;

    if (iSrcX == -1)
    {
        iSrcX = 0;
        dfRatioX = 1;
    }
    if (iSrcY == -1)
    {
        iSrcY = 0;
        dfRatioY = 1;
    }
    iSrcOffset = iSrcX + iSrcY * nSrcXSize;

    // Shift so we don't overrun the array
    if( nSrcXSize * nSrcYSize == iSrcOffset + 1
        || nSrcXSize * nSrcYSize == iSrcOffset + nSrcXSize + 1 )
    {
        bShifted = TRUE;
        --iSrcOffset;
    }
    
    // Get pixel row
    if ( iSrcY >= 0 && iSrcY < nSrcYSize
         && iSrcOffset >= 0 && iSrcOffset < nSrcXSize * nSrcYSize
         && GWKGetPixelRow( poWK, iBand, iSrcOffset, 1,
                            adfDensity, adfReal, adfImag ) )
    {
        double dfMult1 = dfRatioX * dfRatioY;
        double dfMult2 = (1.0-dfRatioX) * dfRatioY;

        // Shifting corrected
        if ( bShifted )
        {
            adfReal[0] = adfReal[1];
            adfImag[0] = adfImag[1];
            adfDensity[0] = adfDensity[1];
        }
        
        // Upper Left Pixel
        if ( iSrcX >= 0 && iSrcX < nSrcXSize
             && adfDensity[0] > 0.000000001 )
        {
            dfAccumulatorDivisor += dfMult1;

            dfAccumulatorReal += adfReal[0] * dfMult1;
            dfAccumulatorImag += adfImag[0] * dfMult1;
            dfAccumulatorDensity += adfDensity[0] * dfMult1;
        }
            
        // Upper Right Pixel
        if ( iSrcX+1 >= 0 && iSrcX+1 < nSrcXSize
             && adfDensity[1] > 0.000000001 )
        {
            dfAccumulatorDivisor += dfMult2;

            dfAccumulatorReal += adfReal[1] * dfMult2;
            dfAccumulatorImag += adfImag[1] * dfMult2;
            dfAccumulatorDensity += adfDensity[1] * dfMult2;
        }
    }
        
    // Get pixel row
    if ( iSrcY+1 >= 0 && iSrcY+1 < nSrcYSize
         && iSrcOffset+nSrcXSize >= 0
         && iSrcOffset+nSrcXSize < nSrcXSize * nSrcYSize
         && GWKGetPixelRow( poWK, iBand, iSrcOffset+nSrcXSize, 1,
                           adfDensity, adfReal, adfImag ) )
    {
        double dfMult1 = dfRatioX * (1.0-dfRatioY);
        double dfMult2 = (1.0-dfRatioX) * (1.0-dfRatioY);
        
        // Shifting corrected
        if ( bShifted )
        {
            adfReal[0] = adfReal[1];
            adfImag[0] = adfImag[1];
            adfDensity[0] = adfDensity[1];
        }
        
        // Lower Left Pixel
        if ( iSrcX >= 0 && iSrcX < nSrcXSize
             && adfDensity[0] > 0.000000001 )
        {
            dfAccumulatorDivisor += dfMult1;

            dfAccumulatorReal += adfReal[0] * dfMult1;
            dfAccumulatorImag += adfImag[0] * dfMult1;
            dfAccumulatorDensity += adfDensity[0] * dfMult1;
        }

        // Lower Right Pixel
        if ( iSrcX+1 >= 0 && iSrcX+1 < nSrcXSize
             && adfDensity[1] > 0.000000001 )
        {
            dfAccumulatorDivisor += dfMult2;

            dfAccumulatorReal += adfReal[1] * dfMult2;
            dfAccumulatorImag += adfImag[1] * dfMult2;
            dfAccumulatorDensity += adfDensity[1] * dfMult2;
        }
    }

/* -------------------------------------------------------------------- */
/*      Return result.                                                  */
/* -------------------------------------------------------------------- */
    if ( dfAccumulatorDivisor == 1.0 )
    {
        *pdfReal = dfAccumulatorReal;
        *pdfImag = dfAccumulatorImag;
        *pdfDensity = dfAccumulatorDensity;
        return TRUE;
    }
    else if ( dfAccumulatorDivisor < 0.00001 )
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
   (  (   -f0 +     f1 - f2 + f3) * distance3                       \
    + (2.0*(f0 - f1) + f2 - f3) * distance2                         \
    + (   -f0          + f2     ) * distance1                       \
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
    double  adfValueDens[4], adfValueReal[4], adfValueImag[4];
    double  adfDensity[4], adfReal[4], adfImag[4];
    int     i;

    // Get the bilinear interpolation at the image borders
    if ( iSrcX - 1 < 0 || iSrcX + 2 >= poWK->nSrcXSize
         || iSrcY - 1 < 0 || iSrcY + 2 >= poWK->nSrcYSize )
        return GWKBilinearResample( poWK, iBand, dfSrcX, dfSrcY,
                                    pdfDensity, pdfReal, pdfImag );

    for ( i = -1; i < 3; i++ )
    {
        if ( !GWKGetPixelRow(poWK, iBand, iSrcOffset + i * poWK->nSrcXSize - 1,
                             2, adfDensity, adfReal, adfImag)
             || adfDensity[0] < 0.000000001
             || adfDensity[1] < 0.000000001
             || adfDensity[2] < 0.000000001
             || adfDensity[3] < 0.000000001 )
        {
            return GWKBilinearResample( poWK, iBand, dfSrcX, dfSrcY,
                                       pdfDensity, pdfReal, pdfImag );
        }

        adfValueDens[i + 1] = CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
            adfDensity[0], adfDensity[1], adfDensity[2], adfDensity[3]);
        adfValueReal[i + 1] = CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
            adfReal[0], adfReal[1], adfReal[2], adfReal[3]);
        adfValueImag[i + 1] = CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
            adfImag[0], adfImag[1], adfImag[2], adfImag[3]);
    }

    
/* -------------------------------------------------------------------- */
/*      For now, if we have any pixels missing in the kernel area,      */
/*      we fallback on using bilinear interpolation.  Ideally we        */
/*      should do "weight adjustment" of our results similarly to       */
/*      what is done for the cubic spline and lanc. interpolators.      */
/* -------------------------------------------------------------------- */
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
/*                          GWKLanczosSinc()                            */
/************************************************************************/

/*
 * Lanczos windowed sinc interpolation kernel with radius r.
 *        /
 *        | sinc(x) * sinc(x/r), if |x| < r
 * L(x) = | 1, if x = 0                     ,
 *        | 0, otherwise
 *        \
 *
 * where sinc(x) = sin(PI * x) / (PI * x).
 */

#define GWK_PI 3.14159265358979323846
static double GWKLanczosSinc( double dfX, double dfR )
{
    if ( fabs(dfX) > dfR )
        return 0.0;
    if ( dfX == 0.0 )
        return 1.0;
    
    double dfPIX = GWK_PI * dfX;
    return ( sin(dfPIX) / dfPIX ) * ( sin(dfPIX / dfR) * dfR / dfPIX );
}
#undef GWK_PI

/************************************************************************/
/*                           GWKBSpline()                               */
/************************************************************************/

static double GWKBSpline( double x )
{
    double xp2 = x + 2.0;
    double xp1 = x + 1.0;
    double xm1 = x - 1.0;
    
    // This will most likely be used, so we'll compute it ahead of time to
    // avoid stalling the processor
    double xp2c = xp2 * xp2 * xp2;
    
    // Note that the test is computed only if it is needed
    return (((xp2 > 0.0)?((xp1 > 0.0)?((x > 0.0)?((xm1 > 0.0)?
                                                  -4.0 * xm1*xm1*xm1:0.0) +
                                       6.0 * x*x*x:0.0) +
                          -4.0 * xp1*xp1*xp1:0.0) +
             xp2c:0.0) ) * 0.166666666666666666666;
}


typedef struct
{
    // Space for saved X weights
    double  *padfWeightsX;
    char    *panCalcX;

    // Space for saving a row of pixels
    double  *padfRowDensity;
    double  *padfRowReal;
    double  *padfRowImag;
} GWKResampleWrkStruct;

static GWKResampleWrkStruct* GWKResampleCreateWrkStruct(GDALWarpKernel *poWK)
{
    int     nXDist = ( poWK->nXRadius + 1 ) * 2;

    GWKResampleWrkStruct* psWrkStruct =
            (GWKResampleWrkStruct*)CPLMalloc(sizeof(GWKResampleWrkStruct));

    // Alloc space for saved X weights
    psWrkStruct->padfWeightsX = (double *)CPLCalloc( nXDist, sizeof(double) );
    psWrkStruct->panCalcX = (char *)CPLMalloc( nXDist * sizeof(char) );

    // Alloc space for saving a row of pixels
    psWrkStruct->padfRowDensity = (double *)CPLCalloc( nXDist, sizeof(double) );
    psWrkStruct->padfRowReal = (double *)CPLCalloc( nXDist, sizeof(double) );
    psWrkStruct->padfRowImag = (double *)CPLCalloc( nXDist, sizeof(double) );

    return psWrkStruct;
}

static void GWKResampleDeleteWrkStruct(GWKResampleWrkStruct* psWrkStruct)
{
    CPLFree( psWrkStruct->padfWeightsX );
    CPLFree( psWrkStruct->panCalcX );
    CPLFree( psWrkStruct->padfRowDensity );
    CPLFree( psWrkStruct->padfRowReal );
    CPLFree( psWrkStruct->padfRowImag );
    CPLFree( psWrkStruct );
}

/************************************************************************/
/*                           GWKResample()                              */
/************************************************************************/

static int GWKResample( GDALWarpKernel *poWK, int iBand, 
                        double dfSrcX, double dfSrcY,
                        double *pdfDensity, 
                        double *pdfReal, double *pdfImag,
                        const GWKResampleWrkStruct* psWrkStruct )

{
    // Save as local variables to avoid following pointers in loops
    int     nSrcXSize = poWK->nSrcXSize;
    int     nSrcYSize = poWK->nSrcYSize;

    double  dfAccumulatorReal = 0.0, dfAccumulatorImag = 0.0;
    double  dfAccumulatorDensity = 0.0;
    double  dfAccumulatorWeight = 0.0;
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    int     eResample = poWK->eResample;

    double  dfXScale, dfYScale;
    double  dfXFilter, dfYFilter;
    int     nXRadius, nFiltInitX;
    int     nYRadius, nFiltInitY;

    dfXScale = poWK->dfXScale;
    dfYScale = poWK->dfYScale;
    nXRadius = poWK->nXRadius;
    nYRadius = poWK->nYRadius;
    nFiltInitX = poWK->nFiltInitX;
    nFiltInitY = poWK->nFiltInitY;
    dfXFilter = poWK->dfXFilter;
    dfYFilter = poWK->dfYFilter;

    int     i, j;
    int     nXDist = ( nXRadius + 1 ) * 2;

    // Space for saved X weights
    double  *padfWeightsX = psWrkStruct->padfWeightsX;
    char    *panCalcX = psWrkStruct->panCalcX;

    // Space for saving a row of pixels
    double  *padfRowDensity = psWrkStruct->padfRowDensity;
    double  *padfRowReal = psWrkStruct->padfRowReal;
    double  *padfRowImag = psWrkStruct->padfRowImag;

    // Mark as needing calculation (don't calculate the weights yet,
    // because a mask may render it unnecessary)
    memset( panCalcX, FALSE, nXDist * sizeof(char) );

    // Loop over pixel rows in the kernel
    for ( j = nFiltInitY; j <= nYRadius; ++j )
    {
        int     iRowOffset, nXMin = nFiltInitX, nXMax = nXRadius;
        double  dfWeight1;
        
        // Skip sampling over edge of image
        if ( iSrcY + j < 0 || iSrcY + j >= nSrcYSize )
            continue;

        // Invariant; needs calculation only once per row
        iRowOffset = iSrcOffset + j * nSrcXSize + nFiltInitX;

        // Make sure we don't read before or after the source array.
        if ( iRowOffset < 0 )
        {
            nXMin = nXMin - iRowOffset;
            iRowOffset = 0;
        }
        if ( iRowOffset + nXDist >= nSrcXSize*nSrcYSize )
        {
            nXMax = nSrcXSize*nSrcYSize - iRowOffset + nXMin - 1;
            nXMax -= (nXMax-nXMin+1) % 2;
        }

        // Get pixel values
        if ( !GWKGetPixelRow( poWK, iBand, iRowOffset, (nXMax-nXMin+2)/2,
                              padfRowDensity, padfRowReal, padfRowImag ) )
            continue;

        // Select the resampling algorithm
        if ( eResample == GRA_CubicSpline )
            // Calculate the Y weight
            dfWeight1 = ( dfYScale < 1.0 ) ?
                GWKBSpline(((double)j) * dfYScale) * dfYScale :
                GWKBSpline(((double)j) - dfDeltaY);
        else if ( eResample == GRA_Lanczos )
            dfWeight1 = ( dfYScale < 1.0 ) ?
                GWKLanczosSinc(j * dfYScale, dfYFilter) * dfYScale :
                GWKLanczosSinc(j - dfDeltaY, dfYFilter);
        else
            return FALSE;
        
        // Iterate over pixels in row
        for (i = nXMin; i <= nXMax; ++i )
        {
            double dfWeight2;
            
            // Skip sampling at edge of image OR if pixel has zero density
            if ( iSrcX + i < 0 || iSrcX + i >= nSrcXSize
                 || padfRowDensity[i-nXMin] < 0.000000001 )
                continue;

            // Make or use a cached set of weights for this row
            if ( panCalcX[i-nFiltInitX] )
                // Use saved weight value instead of recomputing it
                dfWeight2 = dfWeight1 * padfWeightsX[i-nFiltInitX];
            else
            {
                // Choose among possible algorithms
                if ( eResample == GRA_CubicSpline )
                    // Calculate & save the X weight
                    padfWeightsX[i-nFiltInitX] = dfWeight2 = (dfXScale < 1.0 ) ?
                        GWKBSpline((double)i * dfXScale) * dfXScale :
                        GWKBSpline(dfDeltaX - (double)i);
                else if ( eResample == GRA_Lanczos )
                    // Calculate & save the X weight
                    padfWeightsX[i-nFiltInitX] = dfWeight2 = (dfXScale < 1.0 ) ?
                        GWKLanczosSinc(i * dfXScale, dfXFilter) * dfXScale :
                        GWKLanczosSinc(i - dfDeltaX, dfXFilter);
                else
                    return FALSE;
                
                dfWeight2 *= dfWeight1;
                panCalcX[i-nFiltInitX] = TRUE;
            }
            
            // Accumulate!
            dfAccumulatorReal += padfRowReal[i-nXMin] * dfWeight2;
            dfAccumulatorImag += padfRowImag[i-nXMin] * dfWeight2;
            dfAccumulatorDensity += padfRowDensity[i-nXMin] * dfWeight2;
            dfAccumulatorWeight += dfWeight2;
        }
    }

    if ( dfAccumulatorWeight < 0.000001 || dfAccumulatorDensity < 0.000001 )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    // Calculate the output taking into account weighting
    if ( dfAccumulatorWeight < 0.99999 || dfAccumulatorWeight > 1.00001 )
    {
        *pdfReal = dfAccumulatorReal / dfAccumulatorWeight;
        *pdfImag = dfAccumulatorImag / dfAccumulatorWeight;
        *pdfDensity = dfAccumulatorDensity / dfAccumulatorWeight;
    }
    else
    {
        *pdfReal = dfAccumulatorReal;
        *pdfImag = dfAccumulatorImag;
        *pdfDensity = dfAccumulatorDensity;
    }
    
    return TRUE;
}

static int GWKCubicSplineResampleNoMasksByte( GDALWarpKernel *poWK, int iBand,
                                              double dfSrcX, double dfSrcY,
                                              GByte *pbValue, double *padfBSpline )

{
    // Commonly used; save locally
    int     nSrcXSize = poWK->nSrcXSize;
    int     nSrcYSize = poWK->nSrcYSize;
    
    double  dfAccumulator = 0.0;
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;

    double  dfXScale = poWK->dfXScale;
    double  dfYScale = poWK->dfYScale;
    int     nXRadius = poWK->nXRadius;
    int     nYRadius = poWK->nYRadius;

    GByte*  pabySrcBand = poWK->papabySrcImage[iBand];
    
    // Politely refusing to process invalid coordinates or obscenely small image
    if ( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize
         || nXRadius > nSrcXSize || nYRadius > nSrcYSize )
        return GWKBilinearResampleNoMasksByte( poWK, iBand, dfSrcX, dfSrcY, pbValue);

    // Loop over all rows in the kernel
    int     j, jC;
    for ( jC = 0, j = 1 - nYRadius; j <= nYRadius; ++j, ++jC )
    {
        int     iSampJ;
        // Calculate the Y weight
        double  dfWeight1 = ( dfYScale < 1.0 ) ?
            GWKBSpline((double)j * dfYScale) * dfYScale :
            GWKBSpline((double)j - dfDeltaY);

        // Flip sampling over edge of image
        if ( iSrcY + j < 0 )
            iSampJ = iSrcOffset - (iSrcY + j) * nSrcXSize;
        else if ( iSrcY + j >= nSrcYSize )
            iSampJ = iSrcOffset + (2*nSrcYSize - 2*iSrcY - j - 1) * nSrcXSize;
        else
            iSampJ = iSrcOffset + j * nSrcXSize;
        
        // Loop over all pixels in the row
        int     i, iC;
        for ( iC = 0, i = 1 - nXRadius; i <= nXRadius; ++i, ++iC )
        {
            int     iSampI;
            double  dfWeight2;
            
            // Flip sampling over edge of image
            if ( iSrcX + i < 0 )
                iSampI = -iSrcX - i;
            else if ( iSrcX + i >= nSrcXSize )
                iSampI = 2*nSrcXSize - 2*iSrcX - i - 1;
            else
                iSampI = i;
            
            // Make a cached set of GWKBSpline values
            if( jC == 0 )
            {
                // Calculate & save the X weight
                dfWeight2 = padfBSpline[iC] = ((dfXScale < 1.0 ) ?
                    GWKBSpline((double)i * dfXScale) * dfXScale :
                    GWKBSpline(dfDeltaX - (double)i));
                dfWeight2 *= dfWeight1;
            }
            else
                dfWeight2 = dfWeight1 * padfBSpline[iC];

            // Retrieve the pixel & accumulate
            dfAccumulator += (double)pabySrcBand[iSampI+iSampJ] * dfWeight2;
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
                                               GInt16 *piValue, double *padfBSpline )

{
    //Save src size to local var
    int     nSrcXSize = poWK->nSrcXSize;
    int     nSrcYSize = poWK->nSrcYSize;
    
    double  dfAccumulator = 0.0;
    int     iSrcX = (int) floor( dfSrcX - 0.5 );
    int     iSrcY = (int) floor( dfSrcY - 0.5 );
    int     iSrcOffset = iSrcX + iSrcY * nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;

    double  dfXScale = poWK->dfXScale;
    double  dfYScale = poWK->dfYScale;
    int     nXRadius = poWK->nXRadius;
    int     nYRadius = poWK->nYRadius;

    // Save band array pointer to local var; cast here instead of later
    GInt16* pabySrcBand = ((GInt16 *)poWK->papabySrcImage[iBand]);

    // Politely refusing to process invalid coordinates or obscenely small image
    if ( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize
         || nXRadius > nSrcXSize || nYRadius > nSrcYSize )
        return GWKBilinearResampleNoMasksShort( poWK, iBand, dfSrcX, dfSrcY, piValue);

    // Loop over all pixels in the kernel
    int     j, jC;
    for ( jC = 0, j = 1 - nYRadius; j <= nYRadius; ++j, ++jC )
    {
        int     iSampJ;
        
        // Calculate the Y weight
        double  dfWeight1 = ( dfYScale < 1.0 ) ?
            GWKBSpline((double)j * dfYScale) * dfYScale :
            GWKBSpline((double)j - dfDeltaY);

        // Flip sampling over edge of image
        if ( iSrcY + j < 0 )
            iSampJ = iSrcOffset - (iSrcY + j) * nSrcXSize;
        else if ( iSrcY + j >= nSrcYSize )
            iSampJ = iSrcOffset + (2*nSrcYSize - 2*iSrcY - j - 1) * nSrcXSize;
        else
            iSampJ = iSrcOffset + j * nSrcXSize;
        
        // Loop over all pixels in row
        int     i, iC;
        for ( iC = 0, i = 1 - nXRadius; i <= nXRadius; ++i, ++iC )
        {
        int     iSampI;
            double  dfWeight2;
            
            // Flip sampling over edge of image
            if ( iSrcX + i < 0 )
                iSampI = -iSrcX - i;
            else if(iSrcX + i >= nSrcXSize)
                iSampI = 2*nSrcXSize - 2*iSrcX - i - 1;
            else
                iSampI = i;
            
            // Make a cached set of GWKBSpline values
            if ( jC == 0 )
            {
                // Calculate & save the X weight
                dfWeight2 = padfBSpline[iC] = ((dfXScale < 1.0 ) ?
                    GWKBSpline((double)i * dfXScale) * dfXScale :
                    GWKBSpline(dfDeltaX - (double)i));
                dfWeight2 *= dfWeight1;
            } else
                dfWeight2 = dfWeight1 * padfBSpline[iC];

            dfAccumulator += (double)pabySrcBand[iSampI + iSampJ] * dfWeight2;
        }
    }

    *piValue = (GInt16)(0.5 + dfAccumulator);
    
    return TRUE;
}

/************************************************************************/
/*                           GWKOpenCLCase()                            */
/*                                                                      */
/*      This is identical to GWKGeneralCase(), but functions via        */
/*      OpenCL. This means we have vector optimization (SSE) and/or     */
/*      GPU optimization depending on our prefs. The code itsef is      */
/*      general and not optimized, but by defining constants we can     */
/*      make some pretty darn good code on the fly.                     */
/************************************************************************/

#if defined(HAVE_OPENCL)
static CPLErr GWKOpenCLCase( GDALWarpKernel *poWK )
{
    int iDstY, iBand;
    int nDstXSize = poWK->nDstXSize, nDstYSize = poWK->nDstYSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;
    int nDstXOff  = poWK->nDstXOff , nDstYOff  = poWK->nDstYOff;
    int nSrcXOff  = poWK->nSrcXOff , nSrcYOff  = poWK->nSrcYOff;
    CPLErr eErr = CE_None;
    struct oclWarper *warper;
    cl_channel_type imageFormat;
    int useImag = FALSE;
    OCLResampAlg resampAlg;
    cl_int err;
    
    switch ( poWK->eWorkingDataType )
    {
      case GDT_Byte:
        imageFormat = CL_UNORM_INT8;
        break;
      case GDT_UInt16:
        imageFormat = CL_UNORM_INT16;
        break;
      case GDT_CInt16:
        useImag = TRUE;
      case GDT_Int16:
        imageFormat = CL_SNORM_INT16;
        break;
      case GDT_CFloat32:
        useImag = TRUE;
      case GDT_Float32:
        imageFormat = CL_FLOAT;
        break;
      default:
        // We don't support higher precision formats
        CPLDebug( "OpenCL",
                  "Unsupported resampling OpenCL data type %d.", 
                  (int) poWK->eWorkingDataType );
        return CE_Warning;
    }
    
    switch (poWK->eResample)
    {
      case GRA_Bilinear:
        resampAlg = OCL_Bilinear;
        break;
      case GRA_Cubic:
        resampAlg = OCL_Cubic;
        break;
      case GRA_CubicSpline:
        resampAlg = OCL_CubicSpline;
        break;
      case GRA_Lanczos:
        resampAlg = OCL_Lanczos;
        break;
      default:
        // We don't support higher precision formats
        CPLDebug( "OpenCL", 
                  "Unsupported resampling OpenCL resampling alg %d.", 
                  (int) poWK->eResample );
        return CE_Warning;
    }
    
    // Using a factor of 2 or 4 seems to have much less rounding error than 3 on the GPU.
    // Then the rounding error can cause strange artifacting under the right conditions.
    warper = GDALWarpKernelOpenCL_createEnv(nSrcXSize, nSrcYSize,
                                            nDstXSize, nDstYSize,
                                            imageFormat, poWK->nBands, 4,
                                            useImag, poWK->papanBandSrcValid != NULL,
                                            poWK->pafDstDensity,
                                            poWK->padfDstNoDataReal,
                                            resampAlg, &err );

    if(err != CL_SUCCESS || warper == NULL)
    {
        eErr = CE_Warning;
        if (warper != NULL)
            goto free_warper;
        return eErr;
    }
    
    CPLDebug( "GDAL", "GDALWarpKernel()::GWKOpenCLCase()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
              nDstXOff, nDstYOff, nDstXSize, nDstYSize );
    
    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        eErr = CE_Failure;
        goto free_warper;
    }
    
    /* ==================================================================== */
    /*      Loop over bands.                                                */
    /* ==================================================================== */
    for( iBand = 0; iBand < poWK->nBands; iBand++ ) {
        if( poWK->papanBandSrcValid != NULL && poWK->papanBandSrcValid[iBand] != NULL) {
            GDALWarpKernelOpenCL_setSrcValid(warper, (int *)poWK->papanBandSrcValid[iBand], iBand);
            if(err != CL_SUCCESS)
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "OpenCL routines reported failure (%d) on line %d.", (int) err, __LINE__ );
                eErr = CE_Failure;
                goto free_warper;
            }
        }
        
        err = GDALWarpKernelOpenCL_setSrcImg(warper, poWK->papabySrcImage[iBand], iBand);
        if(err != CL_SUCCESS)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "OpenCL routines reported failure (%d) on line %d.", (int) err, __LINE__ );
            eErr = CE_Failure;
            goto free_warper;
        }
        
        err = GDALWarpKernelOpenCL_setDstImg(warper, poWK->papabyDstImage[iBand], iBand);
        if(err != CL_SUCCESS)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "OpenCL routines reported failure (%d) on line %d.", (int) err, __LINE__ );
            eErr = CE_Failure;
            goto free_warper;
        }
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
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; ++iDstY )
    {
        int iDstX;
        
        /* ---------------------------------------------------------------- */
        /*      Setup points to transform to source image space.            */
        /* ---------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; ++iDstX )
        {
            padfX[iDstX] = iDstX + 0.5 + nDstXOff;
            padfY[iDstX] = iDstY + 0.5 + nDstYOff;
            padfZ[iDstX] = 0.0;
        }
        
        /* ---------------------------------------------------------------- */
        /*      Transform the points from destination pixel/line coordinates*/
        /*      to source pixel/line coordinates.                           */
        /* ---------------------------------------------------------------- */
        poWK->pfnTransformer( poWK->pTransformerArg, TRUE, nDstXSize, 
                              padfX, padfY, padfZ, pabSuccess );
        
        err = GDALWarpKernelOpenCL_setCoordRow(warper, padfX, padfY,
                                               nSrcXOff, nSrcYOff,
                                               pabSuccess, iDstY);
        if(err != CL_SUCCESS)
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "OpenCL routines reported failure (%d) on line %d.", (int) err, __LINE__ );
            return CE_Failure;
        }
        
        //Update the valid & density masks because we don't do so in the kernel
        for( iDstX = 0; iDstX < nDstXSize && eErr == CE_None; iDstX++ )
        {
            double dfX = padfX[iDstX];
            double dfY = padfY[iDstX];
            int iDstOffset = iDstX + iDstY * nDstXSize;
            
            //See GWKGeneralCase() for appropriate commenting
            if( !pabSuccess[iDstX] || dfX < nSrcXOff || dfY < nSrcYOff )
                continue;
            
            int iSrcX = ((int) dfX) - nSrcXOff;
            int iSrcY = ((int) dfY) - nSrcYOff;
            
            if( iSrcX < 0 || iSrcX >= nSrcXSize || iSrcY < 0 || iSrcY >= nSrcYSize )
                continue;
            
            int iSrcOffset = iSrcX + iSrcY * nSrcXSize;
            double  dfDensity = 1.0;
            
            if( poWK->pafUnifiedSrcDensity != NULL 
                && iSrcX >= 0 && iSrcY >= 0 
                && iSrcX < nSrcXSize && iSrcY < nSrcYSize )
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
            
            GWKOverlayDensity( poWK, iDstOffset, dfDensity );
            
            //Because this is on the bit-wise level, it can't be done well in OpenCL
            if( poWK->panDstValid != NULL )
                poWK->panDstValid[iDstOffset>>5] |= 0x01 << (iDstOffset & 0x1f);
        }
    }
    
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );
    
    err = GDALWarpKernelOpenCL_runResamp(warper,
                                         poWK->pafUnifiedSrcDensity,
                                         poWK->panUnifiedSrcValid,
                                         poWK->pafDstDensity,
                                         poWK->panDstValid,
                                         poWK->dfXScale, poWK->dfYScale,
                                         poWK->dfXFilter, poWK->dfYFilter,
                                         poWK->nXRadius, poWK->nYRadius,
                                         poWK->nFiltInitX, poWK->nFiltInitY);
    
    if(err != CL_SUCCESS)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OpenCL routines reported failure (%d) on line %d.", (int) err, __LINE__ );
        eErr = CE_Failure;
        goto free_warper;
    }
    
    /* ==================================================================== */
    /*      Loop over output lines.                                         */
    /* ==================================================================== */
    for( iDstY = 0; iDstY < nDstYSize && eErr == CE_None; iDstY++ )
    {
        for( iBand = 0; iBand < poWK->nBands; iBand++ )
        {
            int iDstX;
            void *rowReal, *rowImag;
            GByte *pabyDst = poWK->papabyDstImage[iBand];
            
            err = GDALWarpKernelOpenCL_getRow(warper, &rowReal, &rowImag, iDstY, iBand);
            if(err != CL_SUCCESS)
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "OpenCL routines reported failure (%d) on line %d.", (int) err, __LINE__ );
                eErr = CE_Failure;
                goto free_warper;
            }
            
            //Copy the data from the warper to GDAL's memory
            switch ( poWK->eWorkingDataType )
            {
              case GDT_Byte:
                memcpy((void **)&(((GByte *)pabyDst)[iDstY*nDstXSize]),
                       rowReal, sizeof(GByte)*nDstXSize);
                break;
              case GDT_Int16:
                memcpy((void **)&(((GInt16 *)pabyDst)[iDstY*nDstXSize]),
                       rowReal, sizeof(GInt16)*nDstXSize);
                break;
              case GDT_UInt16:
                memcpy((void **)&(((GUInt16 *)pabyDst)[iDstY*nDstXSize]),
                       rowReal, sizeof(GUInt16)*nDstXSize);
                break;
              case GDT_Float32:
                memcpy((void **)&(((float *)pabyDst)[iDstY*nDstXSize]),
                       rowReal, sizeof(float)*nDstXSize);
                break;
              case GDT_CInt16:
              {
                  GInt16 *pabyDstI16 = &(((GInt16 *)pabyDst)[iDstY*nDstXSize]);
                  for (iDstX = 0; iDstX < nDstXSize; iDstX++) {
                      pabyDstI16[iDstX*2  ] = ((GInt16 *)rowReal)[iDstX];
                      pabyDstI16[iDstX*2+1] = ((GInt16 *)rowImag)[iDstX];
                  }
              }
              break;
              case GDT_CFloat32:
              {
                  float *pabyDstF32 = &(((float *)pabyDst)[iDstY*nDstXSize]);
                  for (iDstX = 0; iDstX < nDstXSize; iDstX++) {
                      pabyDstF32[iDstX*2  ] = ((float *)rowReal)[iDstX];
                      pabyDstF32[iDstX*2+1] = ((float *)rowImag)[iDstX];
                  }
              }
              break;
              default:
                // We don't support higher precision formats
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unsupported resampling OpenCL data type %d.", (int) poWK->eWorkingDataType );
                eErr = CE_Failure;
                goto free_warper;
            }
        }
    }
free_warper:
    if((err = GDALWarpKernelOpenCL_deleteEnv(warper)) != CL_SUCCESS)
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "OpenCL routines reported failure (%d) on line %d.", (int) err, __LINE__ );
        return CE_Failure;
    }
    
    return eErr;
}
#endif /* defined(HAVE_OPENCL) */


#define COMPUTE_iSrcOffset(_pabSuccess, _iDstX, _padfX, _padfY, _poWK, _nSrcXSize, _nSrcYSize) \
            if( !_pabSuccess[_iDstX] ) \
                continue; \
\
/* -------------------------------------------------------------------- */ \
/*      Figure out what pixel we want in our source raster, and skip    */ \
/*      further processing if it is well off the source image.          */ \
/* -------------------------------------------------------------------- */ \
            /* We test against the value before casting to avoid the */ \
            /* problem of asymmetric truncation effects around zero.  That is */ \
            /* -0.5 will be 0 when cast to an int. */ \
            if( _padfX[_iDstX] < _poWK->nSrcXOff \
                || _padfY[_iDstX] < _poWK->nSrcYOff ) \
                continue; \
\
            int iSrcX, iSrcY, iSrcOffset;\
\
            iSrcX = ((int) (_padfX[_iDstX] + 1e-10)) - _poWK->nSrcXOff;\
            iSrcY = ((int) (_padfY[_iDstX] + 1e-10)) - _poWK->nSrcYOff;\
\
            /* If operating outside natural projection area, padfX/Y can be */ \
            /* a very huge positive number, that becomes -2147483648 in the */ \
            /* int trucation. So it is necessary to test now for non negativeness. */ \
            if( iSrcX < 0 || iSrcX >= _nSrcXSize || iSrcY < 0 || iSrcY >= _nSrcYSize )\
                continue;\
\
            iSrcOffset = iSrcX + iSrcY * _nSrcXSize;

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
/*      Allocate x,y,z coordinate arrays for transformation ... one     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    int    *pabSuccess;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);

    GWKResampleWrkStruct* psWrkStruct = NULL;
    if (poWK->eResample == GRA_CubicSpline
        || poWK->eResample == GRA_Lanczos )
    {
        psWrkStruct = GWKResampleCreateWrkStruct(poWK);
    }

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

            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
            int bHasFoundDensity = FALSE;
            
            iDstOffset = iDstX + iDstY * nDstXSize;
            for( iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                double dfBandDensity = 0.0;
                double dfValueReal = 0.0;
                double dfValueImag = 0.0;

/* -------------------------------------------------------------------- */
/*      Collect the source value.                                       */
/* -------------------------------------------------------------------- */
                if ( poWK->eResample == GRA_NearestNeighbour ||
                     nSrcXSize == 1 || nSrcYSize == 1)
                {
                    GWKGetPixelValue( poWK, iBand, iSrcOffset,
                                      &dfBandDensity, &dfValueReal, &dfValueImag );
                }
                else if ( poWK->eResample == GRA_Bilinear )
                {
                    GWKBilinearResample( poWK, iBand, 
                                         padfX[iDstX]-poWK->nSrcXOff,
                                         padfY[iDstX]-poWK->nSrcYOff,
                                         &dfBandDensity, 
                                         &dfValueReal, &dfValueImag );
                }
                else if ( poWK->eResample == GRA_Cubic )
                {
                    GWKCubicResample( poWK, iBand, 
                                      padfX[iDstX]-poWK->nSrcXOff,
                                      padfY[iDstX]-poWK->nSrcYOff,
                                      &dfBandDensity, 
                                      &dfValueReal, &dfValueImag );
                }
                else if ( poWK->eResample == GRA_CubicSpline
                          || poWK->eResample == GRA_Lanczos )
                {
                    GWKResample( poWK, iBand, 
                                 padfX[iDstX]-poWK->nSrcXOff,
                                 padfY[iDstX]-poWK->nSrcYOff,
                                 &dfBandDensity, 
                                 &dfValueReal, &dfValueImag, psWrkStruct );
                }


                // If we didn't find any valid inputs skip to next band.
                if ( dfBandDensity < 0.0000000001 )
                    continue;

                bHasFoundDensity = TRUE;

/* -------------------------------------------------------------------- */
/*      We have a computed value from the source.  Now apply it to      */
/*      the destination pixel.                                          */
/* -------------------------------------------------------------------- */
                GWKSetPixelValue( poWK, iBand, iDstOffset,
                                  dfBandDensity,
                                  dfValueReal, dfValueImag );

            }

            if (!bHasFoundDensity)
              continue;

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
    if (psWrkStruct)
        GWKResampleDeleteWrkStruct(psWrkStruct);

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

            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
/*      Case for 8bit input data with bilinear resampling without       */
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
            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
/*      Case for 8bit input data with cubic spline resampling without   */
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

    int     nXRadius = poWK->nXRadius;
    double  *padfBSpline = (double *)CPLCalloc( nXRadius * 2, sizeof(double) );

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
            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
                                                   &poWK->papabyDstImage[iBand][iDstOffset],
                                                   padfBSpline);
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
    CPLFree( padfBSpline );

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

            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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

            iDstOffset = iDstX + iDstY * nDstXSize;

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

            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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

    int     nXRadius = poWK->nXRadius;
    // Make space to save weights
    double  *padfBSpline = (double *)CPLCalloc( nXRadius * 2, sizeof(double) );

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
            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
                                                    &iValue,
                                                    padfBSpline);
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
    CPLFree( padfBSpline );

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

            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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
            iDstOffset = iDstX + iDstY * nDstXSize;

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

            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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

            COMPUTE_iSrcOffset(pabSuccess, iDstX, padfX, padfY, poWK, nSrcXSize, nSrcYSize);

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

            iDstOffset = iDstX + iDstY * nDstXSize;

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

