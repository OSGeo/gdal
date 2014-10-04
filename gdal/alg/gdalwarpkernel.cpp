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
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "gdal_alg_priv.h"
#include "cpl_string.h"
#include "gdalwarpkernel_opencl.h"
#include "cpl_atomic_ops.h"
#include "cpl_multiproc.h"
#include <limits>

CPL_CVSID("$Id$");

static const int anGWKFilterRadius[] =
{
    0,      // Nearest neighbour
    1,      // Bilinear
    2,      // Cubic Convolution
    2,      // Cubic B-Spline
    3,      // Lanczos windowed sinc
    0,      // Average
    0,      // Mode
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
static CPLErr GWKAverageOrMode( GDALWarpKernel * );

/************************************************************************/
/*                           GWKJobStruct                               */
/************************************************************************/

typedef struct _GWKJobStruct GWKJobStruct;

struct _GWKJobStruct
{
    void           *hThread;
    GDALWarpKernel *poWK;
    int             iYMin;
    int             iYMax;
    volatile int   *pnCounter;
    volatile int   *pbStop;
    void           *hCond;
    void           *hCondMutex;
    int           (*pfnProgress)(GWKJobStruct* psJob);
    void           *pTransformerArg;
} ;

/************************************************************************/
/*                        GWKProgressThread()                           */
/************************************************************************/

/* Return TRUE if the computation must be interrupted */
static int GWKProgressThread(GWKJobStruct* psJob)
{
    CPLAcquireMutex(psJob->hCondMutex, 1.0);
    (*(psJob->pnCounter)) ++;
    CPLCondSignal(psJob->hCond);
    int bStop = *(psJob->pbStop);
    CPLReleaseMutex(psJob->hCondMutex);

    return bStop;
}

/************************************************************************/
/*                      GWKProgressMonoThread()                         */
/************************************************************************/

/* Return TRUE if the computation must be interrupted */
static int GWKProgressMonoThread(GWKJobStruct* psJob)
{
    GDALWarpKernel *poWK = psJob->poWK;
    int nCounter = ++(*(psJob->pnCounter));
    if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                            (nCounter / (double) psJob->iYMax),
                            "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        *(psJob->pbStop) = TRUE;
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                       GWKGenericMonoThread()                         */
/************************************************************************/

static CPLErr GWKGenericMonoThread( GDALWarpKernel *poWK,
                                    void (*pfnFunc) (void *pUserData) )
{
    volatile int bStop = FALSE;
    volatile int nCounter = 0;

    GWKJobStruct sThreadJob;
    sThreadJob.poWK = poWK;
    sThreadJob.pnCounter = &nCounter;
    sThreadJob.iYMin = 0;
    sThreadJob.iYMax = poWK->nDstYSize;
    sThreadJob.pbStop = &bStop;
    sThreadJob.hCond = NULL;
    sThreadJob.hCondMutex = NULL;
    sThreadJob.hThread = NULL;
    sThreadJob.pfnProgress = GWKProgressMonoThread;
    sThreadJob.pTransformerArg = poWK->pTransformerArg;

    pfnFunc(&sThreadJob);

    return !bStop ? CE_None : CE_Failure;
}

/************************************************************************/
/*                                GWKRun()                              */
/************************************************************************/

static CPLErr GWKRun( GDALWarpKernel *poWK,
                      const char* pszFuncName,
                      void (*pfnFunc) (void *pUserData) )

{
    int nDstYSize = poWK->nDstYSize;

    CPLDebug( "GDAL", "GDALWarpKernel()::%s()\n"
              "Src=%d,%d,%dx%d Dst=%d,%d,%dx%d",
              pszFuncName,
              poWK->nSrcXOff, poWK->nSrcYOff,
              poWK->nSrcXSize, poWK->nSrcYSize,
              poWK->nDstXOff, poWK->nDstYOff,
              poWK->nDstXSize, poWK->nDstYSize );

    if( !poWK->pfnProgress( poWK->dfProgressBase, "", poWK->pProgress ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        return CE_Failure;
    }

    const char* pszWarpThreads = CSLFetchNameValue(poWK->papszWarpOptions, "NUM_THREADS");
    int nThreads;
    if (pszWarpThreads == NULL)
        pszWarpThreads = CPLGetConfigOption("GDAL_NUM_THREADS", "1");
    if (EQUAL(pszWarpThreads, "ALL_CPUS"))
        nThreads = CPLGetNumCPUs();
    else
        nThreads = atoi(pszWarpThreads);
    if (nThreads > 128)
        nThreads = 128;
    if (nThreads >= nDstYSize / 2)
        nThreads = nDstYSize / 2;

    if (nThreads <= 1)
    {
        return GWKGenericMonoThread(poWK, pfnFunc);
    }
    else
    {
        GWKJobStruct* pasThreadJob =
            (GWKJobStruct*)CPLCalloc(sizeof(GWKJobStruct), nThreads);

/* -------------------------------------------------------------------- */
/*      Duplicate pTransformerArg per thread.                           */
/* -------------------------------------------------------------------- */
        int i;
        int bTransformerCloningSuccess = TRUE;

        for(i=0;i<nThreads;i++)
        {
            pasThreadJob[i].pTransformerArg = GDALCloneTransformer(poWK->pTransformerArg);
            if( pasThreadJob[i].pTransformerArg == NULL )
            {
                CPLDebug("WARP", "Cannot deserialize transformer");
                bTransformerCloningSuccess = FALSE;
                break;
            }
        }

        if (!bTransformerCloningSuccess)
        {
            for(i=0;i<nThreads;i++)
            {
                if( pasThreadJob[i].pTransformerArg )
                    GDALDestroyTransformer(pasThreadJob[i].pTransformerArg);
            }
            CPLFree(pasThreadJob);

            CPLDebug("WARP", "Cannot duplicate transformer function. "
                     "Falling back to mono-thread computation");
            return GWKGenericMonoThread(poWK, pfnFunc);
        }

        void* hCond = CPLCreateCond();
        if (hCond == NULL)
        {
            for(i=0;i<nThreads;i++)
            {
                if( pasThreadJob[i].pTransformerArg )
                    GDALDestroyTransformer(pasThreadJob[i].pTransformerArg);
            }
            CPLFree(pasThreadJob);

            CPLDebug("WARP", "Multithreading disabled. "
                     "Falling back to mono-thread computation");
            return GWKGenericMonoThread(poWK, pfnFunc);
        }

        CPLDebug("WARP", "Using %d threads", nThreads);

        void* hCondMutex = CPLCreateMutex(); /* and take implicitely the mutex */

        volatile int bStop = FALSE;
        volatile int nCounter = 0;

/* -------------------------------------------------------------------- */
/*      Lannch worker threads                                           */
/* -------------------------------------------------------------------- */
        for(i=0;i<nThreads;i++)
        {
            pasThreadJob[i].poWK = poWK;
            pasThreadJob[i].pnCounter = &nCounter;
            pasThreadJob[i].iYMin = (int)(((GIntBig)i) * nDstYSize / nThreads);
            pasThreadJob[i].iYMax = (int)(((GIntBig)(i + 1)) * nDstYSize / nThreads);
            pasThreadJob[i].pbStop = &bStop;
            pasThreadJob[i].hCond = hCond;
            pasThreadJob[i].hCondMutex = hCondMutex;
            pasThreadJob[i].pfnProgress = GWKProgressThread;
            pasThreadJob[i].hThread = CPLCreateJoinableThread( pfnFunc,
                                                  (void*) &pasThreadJob[i] );
        }

/* -------------------------------------------------------------------- */
/*      Report progress.                                                */
/* -------------------------------------------------------------------- */
        while(nCounter < nDstYSize)
        {
            CPLCondWait(hCond, hCondMutex);

            if( !poWK->pfnProgress( poWK->dfProgressBase + poWK->dfProgressScale *
                                    (nCounter / (double) nDstYSize),
                                    "", poWK->pProgress ) )
            {
                CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
                bStop = TRUE;
                break;
            }
        }

        /* Release mutex before joining threads, otherwise they will dead-lock */
        /* forever in GWKProgressThread() */
        CPLReleaseMutex(hCondMutex);

/* -------------------------------------------------------------------- */
/*      Wait for all threads to complete and finish.                    */
/* -------------------------------------------------------------------- */
        for(i=0;i<nThreads;i++)
        {
            CPLJoinThread(pasThreadJob[i].hThread);
            GDALDestroyTransformer(pasThreadJob[i].pTransformerArg);
        }

        CPLFree(pasThreadJob);
        CPLDestroyCond(hCond);
        CPLDestroyMutex(hCondMutex);

        return !bStop ? CE_None : CE_Failure;
    }
}

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
 * The resampling algorithm to use.  One of GRA_NearestNeighbour, GRA_Bilinear, 
 * GRA_Cubic, GRA_CubicSpline, GRA_Lanczos, GRA_Average, or GRA_Mode.
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
        if ( !pfnProgress( dfProgressBase + dfProgressScale,
                           "", pProgress ) )
        {
            CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
            return CE_Failure;
        }
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      Pre-calculate resampling scales and window sizes for filtering. */
/* -------------------------------------------------------------------- */

    dfXScale = (double)nDstXSize / nSrcXSize;
    dfYScale = (double)nDstYSize / nSrcYSize;
    if( nSrcXSize >= nDstXSize && nSrcXSize <= nDstXSize + 1 + 2 * anGWKFilterRadius[eResample] )
        dfXScale = 1;
    if( nSrcYSize >= nDstYSize && nSrcYSize <= nDstYSize + 1 + 2 * anGWKFilterRadius[eResample] )
        dfYScale = 1;

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

    if( eResample == GRA_Average )
        return GWKAverageOrMode( this );

    if( eResample == GRA_Mode )
        return GWKAverageOrMode( this );

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
    
    // Safety check for callers that would use GDALWarpKernel without using
    // GDALWarpOperation.
    if( (eResample == GRA_CubicSpline || eResample == GRA_Lanczos) &&
         atoi(CSLFetchNameValueDef(papszWarpOptions, "EXTRA_ELTS", "0") ) != WARP_EXTRA_ELTS )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Source arrays must have WARP_EXTRA_ELTS extra elements at their end. "
                  "See GDALWarpKernel class definition. If this condition is fulfilled, "
                  "define a EXTRA_ELTS=%d warp options", WARP_EXTRA_ELTS);
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

template<class T>
static T GWKClampValueT(double dfValue)
{
    if (dfValue < std::numeric_limits<T>::min())
        return std::numeric_limits<T>::min();
    else if (dfValue > std::numeric_limits<T>::max())
        return std::numeric_limits<T>::max();
    else
        return (std::numeric_limits<T>::min() < 0) ? (T)floor(dfValue + 0.5) : (T)(dfValue + 0.5);
}


template<class T>
static int GWKSetPixelValueRealT( GDALWarpKernel *poWK, int iBand, 
                         int iDstOffset, double dfDensity,
                         T value)
{
    T *pDst = (T*)(poWK->papabyDstImage[iBand]);

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
        double dfDstReal, dfDstDensity = 1.0;

        if( dfDensity < 0.0001 )
            return TRUE;

        if( poWK->pafDstDensity != NULL )
            dfDstDensity = poWK->pafDstDensity[iDstOffset];
        else if( poWK->panDstValid != NULL 
                 && !((poWK->panDstValid[iDstOffset>>5]
                       & (0x01 << (iDstOffset & 0x1f))) ) )
            dfDstDensity = 0.0;

        // It seems like we also ought to be testing panDstValid[] here!

        dfDstReal = pDst[iDstOffset];

        // the destination density is really only relative to the portion
        // not occluded by the overlay.
        double dfDstInfluence = (1.0 - dfDensity) * dfDstDensity;

        double dfReal = (value * dfDensity + dfDstReal * dfDstInfluence) 
                            / (dfDensity + dfDstInfluence);

/* -------------------------------------------------------------------- */
/*      Actually apply the destination value.                           */
/*                                                                      */
/*      Avoid using the destination nodata value for integer datatypes  */
/*      if by chance it is equal to the computed pixel value.           */
/* -------------------------------------------------------------------- */
        pDst[iDstOffset] = GWKClampValueT<T>(dfReal);
    }
    else
        pDst[iDstOffset] = value;

    if (poWK->padfDstNoDataReal != NULL &&
        poWK->padfDstNoDataReal[iBand] == (double)pDst[iDstOffset])
    {
        if (pDst[iDstOffset] == std::numeric_limits<T>::min())
            pDst[iDstOffset] = std::numeric_limits<T>::min() + 1;
        else
            pDst[iDstOffset] --;
    }

    return TRUE;
}


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

/* It is assumed that adfImag[] is set to 0 by caller code for non-complex */
/* data-types. */

static int GWKGetPixelRow( GDALWarpKernel *poWK, int iBand, 
                           int iSrcOffset, int nHalfSrcLen,
                           double* padfDensity,
                           double adfReal[],
                           double* padfImag )
{
    // We know that nSrcLen is even, so we can *always* unroll loops 2x
    int     nSrcLen = nHalfSrcLen * 2;
    int     bHasValid = FALSE;
    int     i;
    
    if( padfDensity != NULL )
    {
        // Init the density
        for ( i = 0; i < nSrcLen; i += 2 )
        {
            padfDensity[i] = 1.0;
            padfDensity[i+1] = 1.0;
        }
        
        if ( poWK->panUnifiedSrcValid != NULL )
        {
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                if(poWK->panUnifiedSrcValid[(iSrcOffset+i)>>5]
                & (0x01 << ((iSrcOffset+i) & 0x1f)))
                    bHasValid = TRUE;
                else
                    padfDensity[i] = 0.0;
                
                if(poWK->panUnifiedSrcValid[(iSrcOffset+i+1)>>5]
                & (0x01 << ((iSrcOffset+i+1) & 0x1f)))
                    bHasValid = TRUE;
                else
                    padfDensity[i+1] = 0.0;
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
                    padfDensity[i] = 0.0;
                
                if(poWK->papanBandSrcValid[iBand][(iSrcOffset+i+1)>>5]
                & (0x01 << ((iSrcOffset+i+1) & 0x1f)))
                    bHasValid = TRUE;
                else
                    padfDensity[i+1] = 0.0;
            }
            
            // Reset or fail as needed
            if ( bHasValid )
                bHasValid = FALSE;
            else
                return FALSE;
        }
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
            break;
       }

        case GDT_CInt16:
        {
            GInt16* pSrc = (GInt16*) poWK->papabySrcImage[iBand];
            pSrc += 2 * iSrcOffset;
            for ( i = 0; i < nSrcLen; i += 2 )
            {
                adfReal[i] = pSrc[2*i];
                padfImag[i] = pSrc[2*i+1];

                adfReal[i+1] = pSrc[2*i+2];
                padfImag[i+1] = pSrc[2*i+3];
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
                padfImag[i] = pSrc[2*i+1];

                adfReal[i+1] = pSrc[2*i+2];
                padfImag[i+1] = pSrc[2*i+3];
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
                padfImag[i] = pSrc[2*i+1];

                adfReal[i+1] = pSrc[2*i+2];
                padfImag[i+1] = pSrc[2*i+3];
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
                padfImag[i] = pSrc[2*i+1];

                adfReal[i+1] = pSrc[2*i+2];
                padfImag[i+1] = pSrc[2*i+3];
            }
            break;
        }

        default:
            CPLAssert(FALSE);
            if( padfDensity )
                memset( padfDensity, 0, nSrcLen * sizeof(double) );
            return FALSE;
    }

    if( padfDensity == NULL )
        return TRUE;

    if( poWK->pafUnifiedSrcDensity == NULL )
    {
        for ( i = 0; i < nSrcLen; i += 2 )
        {
            // Take into account earlier calcs
            if(padfDensity[i] > 0.000000001)
            {
                padfDensity[i] = 1.0;
                bHasValid = TRUE;
            }
            
            if(padfDensity[i+1] > 0.000000001)
            {
                padfDensity[i+1] = 1.0;
                bHasValid = TRUE;
            }
        }
    }
    else
    {
        for ( i = 0; i < nSrcLen; i += 2 )
        {
            if(padfDensity[i] > 0.000000001)
                padfDensity[i] = poWK->pafUnifiedSrcDensity[iSrcOffset+i];
            if(padfDensity[i] > 0.000000001)
                bHasValid = TRUE;
            
            if(padfDensity[i+1] > 0.000000001)
                padfDensity[i+1] = poWK->pafUnifiedSrcDensity[iSrcOffset+i+1];
            if(padfDensity[i+1] > 0.000000001)
                bHasValid = TRUE;
        }
    }
    
    return bHasValid;
}

/************************************************************************/
/*                          GWKGetPixelT()                              */
/************************************************************************/

template<class T>
static int GWKGetPixelT( GDALWarpKernel *poWK, int iBand, 
                         int iSrcOffset, double *pdfDensity, 
                         T *pValue )

{
    T *pSrc = (T *)poWK->papabySrcImage[iBand];

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

    *pValue = pSrc[iSrcOffset];

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
    double  adfDensity[2], adfReal[2], adfImag[2] = {0, 0};
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

template<class T>
static int GWKBilinearResampleNoMasksT( GDALWarpKernel *poWK, int iBand, 
                                        double dfSrcX, double dfSrcY,
                                        T *pValue )

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
            (double)((T *)poWK->papabySrcImage[iBand])[iSrcOffset]
            * dfMult;
    }
        
    // Upper Right Pixel
    if( iSrcX+1 >= 0 && iSrcX+1 < poWK->nSrcXSize
        && iSrcY >= 0 && iSrcY < poWK->nSrcYSize )
    {
        double dfMult = (1.0-dfRatioX) * dfRatioY;

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)((T *)poWK->papabySrcImage[iBand])[iSrcOffset+1] * dfMult;
    }
        
    // Lower Right Pixel
    if( iSrcX+1 >= 0 && iSrcX+1 < poWK->nSrcXSize
        && iSrcY+1 >= 0 && iSrcY+1 < poWK->nSrcYSize )
    {
        double dfMult = (1.0-dfRatioX) * (1.0-dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)((T *)poWK->papabySrcImage[iBand])[iSrcOffset+1+poWK->nSrcXSize]
            * dfMult;
    }
        
    // Lower Left Pixel
    if( iSrcX >= 0 && iSrcX < poWK->nSrcXSize
        && iSrcY+1 >= 0 && iSrcY+1 < poWK->nSrcYSize )
    {
        double dfMult = dfRatioX * (1.0-dfRatioY);

        dfAccumulatorDivisor += dfMult;

        dfAccumulator +=
            (double)((T *)poWK->papabySrcImage[iBand])[iSrcOffset+poWK->nSrcXSize]
            * dfMult;
    }

/* -------------------------------------------------------------------- */
/*      Return result.                                                  */
/* -------------------------------------------------------------------- */
    double      dfValue;

    if( dfAccumulatorDivisor < 0.00001 )
    {
        *pValue = 0;
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

    *pValue = GWKClampValueT<T>(dfValue);

    return TRUE;
}

/************************************************************************/
/*                        GWKCubicResample()                            */
/*     Set of bicubic interpolators using cubic convolution.            */
/************************************************************************/
#define CubicConvolution(distance1,distance2,distance3,f0,f1,f2,f3) \
     (             f1                                               \
      + distance1*0.5*(f2 - f0)                                     \
      + distance2*0.5*(2.0*f0 - 5.0*f1 + 4.0*f2 - f3)               \
      + distance3*0.5*(3.0*(f1 - f2) + f3 - f0))

/* http://en.wikipedia.org/wiki/Bicubic_interpolation with alpha = -0.5 (cubic hermite spline ) */
/* or http://www.cs.utexas.edu/users/fussell/courses/cs384g/lectures/mitchell/Mitchell.pdf with (B,C)=(0,0.5) the Catmull-Rom spline */
/*
W(x) = 1.5|x|^3-2.5|x|^2+1 if |x| <= 1
       -0.5|x|^3+2.5|x|^2-4|x|+2 if 1 < |x| < 2

V[-1]*W(-1.5)+V[0]*W(-0.5)+V[1]*W(0.5)+V[2]*W(1.5)=
-0.0625*V[-1]+0.5625*V[0]+0.5625*V[1]-0.0625*V[2]

V[0]+0.25*(V[1]-V[-1])+0.125*(2V[-1]-5V[0]+4V[1]-V[2])+0.0625*(3*(V[0]-V[1])+V[2]-V[-1])=
0.5625*V[0] +0.5625*V[1]-0.0625*V[-1]-0.0625*V[2]

*/

static int GWKCubicResample( GDALWarpKernel *poWK, int iBand,
                             double dfSrcX, double dfSrcY,
                             double *pdfDensity,
                             double *pdfReal, double *pdfImag )

{
    int     iSrcX = (int) (dfSrcX - 0.5);
    int     iSrcY = (int) (dfSrcY - 0.5);
    int     iSrcOffset = iSrcX + iSrcY * poWK->nSrcXSize;
    double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    double  dfDeltaX2 = dfDeltaX * dfDeltaX;
    double  dfDeltaY2 = dfDeltaY * dfDeltaY;
    double  dfDeltaX3 = dfDeltaX2 * dfDeltaX;
    double  dfDeltaY3 = dfDeltaY2 * dfDeltaY;
    double  adfValueDens[4], adfValueReal[4], adfValueImag[4];
    double  adfDensity[4], adfReal[4], adfImag[4] = {0, 0, 0, 0};
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

template<class T>
static int GWKCubicResampleNoMasksT( GDALWarpKernel *poWK, int iBand,
                                     double dfSrcX, double dfSrcY,
                                     T *pValue )

{
    int     iSrcX = (int) (dfSrcX - 0.5);
    int     iSrcY = (int) (dfSrcY - 0.5);
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
        return GWKBilinearResampleNoMasksT ( poWK, iBand, dfSrcX, dfSrcY,
                                             pValue );

    for ( i = -1; i < 3; i++ )
    {
        int     iOffset = iSrcOffset + i * poWK->nSrcXSize;

        adfValue[i + 1] =CubicConvolution(dfDeltaX, dfDeltaX2, dfDeltaX3,
                (double)((T *)poWK->papabySrcImage[iBand])[iOffset - 1],
                (double)((T *)poWK->papabySrcImage[iBand])[iOffset],
                (double)((T *)poWK->papabySrcImage[iBand])[iOffset + 1],
                (double)((T *)poWK->papabySrcImage[iBand])[iOffset + 2]);
    }

    double dfValue = CubicConvolution(
        dfDeltaY, dfDeltaY2, dfDeltaY3,
        adfValue[0], adfValue[1], adfValue[2], adfValue[3]);

    *pValue = GWKClampValueT<T>(dfValue);

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

static double GWKLanczosSinc( double dfX )
{
    if ( dfX == 0.0 )
        return 1.0;

    const double dfPIX = GWK_PI * dfX;
    const double dfPIXoverR = dfPIX / anGWKFilterRadius[GRA_Lanczos];
    const double dfPIX2overR = dfPIX * dfPIXoverR;
    return sin(dfPIX) * sin(dfPIXoverR) / dfPIX2overR;
}

//#undef GWK_PI

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
/* http://www.cs.utexas.edu/users/fussell/courses/cs384g/lectures/mitchell/Mitchell.pdf with (B,C)=(1,0)
1/6 * ( 3 * |x|^3 -  6 * |x|^2 + 4) |x| < 1
1/6 * ( -|x|^3 + 6 |x|^2  - 12|x| + 8) |x| > 1

-2<x<-1 1/6*( (x+2)^3) = 1/6*(x^3 + 6x^2+12x+8)
-1<x<0  1/6*( -4*(x+1)^3 + (x+2)^3 = 1/6*(-4(x^3+3x^2+3x+1)+x^3 + 6x^2+12x+8) = 1/6 * (-3x^3-6*x^2+4)
*/

/************************************************************************/
/*                       GWKResampleWrkStruct                           */
/************************************************************************/

typedef struct _GWKResampleWrkStruct GWKResampleWrkStruct;

typedef int (*pfnGWKResampleType) ( GDALWarpKernel *poWK, int iBand, 
                                    double dfSrcX, double dfSrcY,
                                    double *pdfDensity, 
                                    double *pdfReal, double *pdfImag,
                                    GWKResampleWrkStruct* psWrkStruct );


struct _GWKResampleWrkStruct
{
    pfnGWKResampleType pfnGWKResample;

    // Space for saved X weights
    double  *padfWeightsX;
    char    *panCalcX;

    double  *padfWeightsY; // only used by GWKResampleOptimizedLanczos
    int      iLastSrcX; // only used by GWKResampleOptimizedLanczos
    int      iLastSrcY; // only used by GWKResampleOptimizedLanczos
    double   dfLastDeltaX; // only used by GWKResampleOptimizedLanczos
    double   dfLastDeltaY; // only used by GWKResampleOptimizedLanczos

    // Space for saving a row of pixels
    double  *padfRowDensity;
    double  *padfRowReal;
    double  *padfRowImag;
};

/************************************************************************/
/*                    GWKResampleCreateWrkStruct()                      */
/************************************************************************/

static int GWKResample( GDALWarpKernel *poWK, int iBand, 
                        double dfSrcX, double dfSrcY,
                        double *pdfDensity, 
                        double *pdfReal, double *pdfImag,
                        GWKResampleWrkStruct* psWrkStruct );

static int GWKResampleOptimizedLanczos( GDALWarpKernel *poWK, int iBand, 
                                        double dfSrcX, double dfSrcY,
                                        double *pdfDensity, 
                                        double *pdfReal, double *pdfImag,
                                        GWKResampleWrkStruct* psWrkStruct );

static GWKResampleWrkStruct* GWKResampleCreateWrkStruct(GDALWarpKernel *poWK)
{
    int     nXDist = ( poWK->nXRadius + 1 ) * 2;
    int     nYDist = ( poWK->nYRadius + 1 ) * 2;

    GWKResampleWrkStruct* psWrkStruct =
            (GWKResampleWrkStruct*)CPLMalloc(sizeof(GWKResampleWrkStruct));

    // Alloc space for saved X weights
    psWrkStruct->padfWeightsX = (double *)CPLCalloc( nXDist, sizeof(double) );
    psWrkStruct->panCalcX = (char *)CPLMalloc( nXDist * sizeof(char) );
    
    psWrkStruct->padfWeightsY = (double *)CPLCalloc( nYDist, sizeof(double) );
    psWrkStruct->iLastSrcX = -10;
    psWrkStruct->iLastSrcY = -10;
    psWrkStruct->dfLastDeltaX = -10;
    psWrkStruct->dfLastDeltaY = -10;

    // Alloc space for saving a row of pixels
    if( poWK->pafUnifiedSrcDensity == NULL &&
        poWK->panUnifiedSrcValid == NULL &&
        poWK->papanBandSrcValid == NULL )
    {
        psWrkStruct->padfRowDensity = NULL;
    }
    else
    {
        psWrkStruct->padfRowDensity = (double *)CPLCalloc( nXDist, sizeof(double) );
    }
    psWrkStruct->padfRowReal = (double *)CPLCalloc( nXDist, sizeof(double) );
    psWrkStruct->padfRowImag = (double *)CPLCalloc( nXDist, sizeof(double) );

    if( poWK->eResample == GRA_Lanczos )
    {
        psWrkStruct->pfnGWKResample = GWKResampleOptimizedLanczos;

        const double dfXScale = poWK->dfXScale;
        if( dfXScale < 1.0 )
        {
            int iMin = poWK->nFiltInitX, iMax = poWK->nXRadius;
            while( iMin * dfXScale < -3.0 )
                iMin ++;
            while( iMax * dfXScale > 3.0 )
                iMax --;

            for(int i = iMin; i <= iMax; ++i)
            {
                psWrkStruct->padfWeightsX[i-poWK->nFiltInitX] =
                    GWKLanczosSinc(i * dfXScale);
            }
        }

        const double dfYScale = poWK->dfYScale;
        if( dfYScale < 1.0 )
        {
            int jMin = poWK->nFiltInitY, jMax = poWK->nYRadius;
            while( jMin * dfYScale < -3.0 )
                jMin ++;
            while( jMax * dfYScale > 3.0 )
                jMax --;

            for(int j = jMin; j <= jMax; ++j)
            {
                psWrkStruct->padfWeightsY[j-poWK->nFiltInitY] =
                    GWKLanczosSinc(j * dfYScale);
            }
        }
    }
    else
        psWrkStruct->pfnGWKResample = GWKResample;

    return psWrkStruct;
}

/************************************************************************/
/*                    GWKResampleDeleteWrkStruct()                      */
/************************************************************************/

static void GWKResampleDeleteWrkStruct(GWKResampleWrkStruct* psWrkStruct)
{
    CPLFree( psWrkStruct->padfWeightsX );
    CPLFree( psWrkStruct->padfWeightsY );
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
                        GWKResampleWrkStruct* psWrkStruct )

{
    // Save as local variables to avoid following pointers in loops
    const int     nSrcXSize = poWK->nSrcXSize;
    const int     nSrcYSize = poWK->nSrcYSize;

    double  dfAccumulatorReal = 0.0, dfAccumulatorImag = 0.0;
    double  dfAccumulatorDensity = 0.0;
    double  dfAccumulatorWeight = 0.0;
    const int     iSrcX = (int) floor( dfSrcX - 0.5 );
    const int     iSrcY = (int) floor( dfSrcY - 0.5 );
    const int     iSrcOffset = iSrcX + iSrcY * nSrcXSize;
    const double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double  dfDeltaY = dfSrcY - 0.5 - iSrcY;
    const int     eResample = poWK->eResample;

    const double  dfXScale = poWK->dfXScale, dfYScale = poWK->dfYScale;

    int     i, j;
    const int     nXDist = ( poWK->nXRadius + 1 ) * 2;

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
    
    CPLAssert( eResample == GRA_CubicSpline || eResample == GRA_Lanczos );

    // Skip sampling over edge of image
    j = poWK->nFiltInitY;
    int jMax= poWK->nYRadius;
    if( iSrcY + j < 0 )
        j = -iSrcY;
    if( iSrcY + jMax >= nSrcYSize )
        jMax = nSrcYSize - iSrcY - 1;
        
    int iMin = poWK->nFiltInitX, iMax = poWK->nXRadius;
    if( iSrcX + iMin < 0 )
        iMin = -iSrcX;
    if( iSrcX + iMax >= nSrcXSize )
        iMax = nSrcXSize - iSrcX - 1;

    const int bXScaleBelow1 = ( dfXScale < 1.0 );
    const int bYScaleBelow1 = ( dfYScale < 1.0 );

    int iRowOffset = iSrcOffset + (j - 1) * nSrcXSize + iMin;

    CPLAssert( eResample == GRA_CubicSpline );

    double dfAccumulatorWeightHorizontal = 0.0;
    double dfAccumulatorWeightVertical = 0.0;

    // Loop over pixel rows in the kernel
    for ( ; j <= jMax; ++j )
    {
        double  dfWeight1;

        iRowOffset += nSrcXSize;

        // Get pixel values
        // We can potentially read extra elements after the "normal" end of the source arrays,
        // but the contract of papabySrcImage[iBand], papanBandSrcValid[iBand],
        // panUnifiedSrcValid and pafUnifiedSrcDensity is to have WARP_EXTRA_ELTS
        // reserved at their end.
        if ( !GWKGetPixelRow( poWK, iBand, iRowOffset, (iMax-iMin+2)/2,
                              padfRowDensity, padfRowReal, padfRowImag ) )
            continue;

        // Select the resampling algorithm
        /* if ( eResample == GRA_CubicSpline ) */
            // Calculate the Y weight
            dfWeight1 = ( bYScaleBelow1 ) ?
                GWKBSpline((j - dfDeltaY) * dfYScale):
                GWKBSpline(j - dfDeltaY);
        /*else
            CPLAssert(0);*/

        // Iterate over pixels in row
        double dfAccumulatorRealLocal = 0.0;
        double dfAccumulatorImagLocal = 0.0;
        double dfAccumulatorDensityLocal = 0.0;

        for (i = iMin; i <= iMax; ++i )
        {
            double dfWeight2;

            // Skip sampling if pixel has zero density
            if ( padfRowDensity != NULL &&
                 padfRowDensity[i-iMin] < 0.000000001 )
                continue;

            // Make or use a cached set of weights for this row
            if ( panCalcX[i-iMin] )
            {
                // Use saved weight value instead of recomputing it
                dfWeight2 = padfWeightsX[i-iMin];
            }
            else
            {
                // Choose among possible algorithms
                /* if ( eResample == GRA_CubicSpline ) */
                    // Calculate & save the X weight
                    padfWeightsX[i-iMin] = dfWeight2 = ( bXScaleBelow1 ) ?
                        GWKBSpline((i - dfDeltaX) * dfXScale):
                        GWKBSpline(i - dfDeltaX);
                /*else
                    CPLAssert(0);*/

                dfAccumulatorWeightHorizontal += dfWeight2;
                panCalcX[i-iMin] = TRUE;
            }

            // Accumulate!
            dfAccumulatorRealLocal += padfRowReal[i-iMin] * dfWeight2;
            dfAccumulatorImagLocal += padfRowImag[i-iMin] * dfWeight2;
            if( padfRowDensity != NULL )
                dfAccumulatorDensityLocal += padfRowDensity[i-iMin] * dfWeight2;
        }
        
        dfAccumulatorReal += dfAccumulatorRealLocal * dfWeight1;
        dfAccumulatorImag += dfAccumulatorImagLocal * dfWeight1;
        dfAccumulatorDensity += dfAccumulatorDensityLocal * dfWeight1;
        dfAccumulatorWeightVertical += dfWeight1;
    }
    
    dfAccumulatorWeight = dfAccumulatorWeightHorizontal * dfAccumulatorWeightVertical;

    if ( dfAccumulatorWeight < 0.000001 ||
         (padfRowDensity != NULL && dfAccumulatorDensity < 0.000001) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    // Calculate the output taking into account weighting
    if ( dfAccumulatorWeight < 0.99999 || dfAccumulatorWeight > 1.00001 )
    {
        *pdfReal = dfAccumulatorReal / dfAccumulatorWeight;
        *pdfImag = dfAccumulatorImag / dfAccumulatorWeight;
        if( padfRowDensity != NULL )
            *pdfDensity = dfAccumulatorDensity / dfAccumulatorWeight;
        else
            *pdfDensity = 1.0;
    }
    else
    {
        *pdfReal = dfAccumulatorReal;
        *pdfImag = dfAccumulatorImag;
        if( padfRowDensity != NULL )
            *pdfDensity = dfAccumulatorDensity;
        else
            *pdfDensity = 1.0;
    }
    
    return TRUE;
}

/************************************************************************/
/*                      GWKResampleOptimizedLanczos()                   */
/************************************************************************/

static int GWKResampleOptimizedLanczos( GDALWarpKernel *poWK, int iBand, 
                        double dfSrcX, double dfSrcY,
                        double *pdfDensity, 
                        double *pdfReal, double *pdfImag,
                        GWKResampleWrkStruct* psWrkStruct )

{
    // Save as local variables to avoid following pointers in loops
    const int     nSrcXSize = poWK->nSrcXSize;
    const int     nSrcYSize = poWK->nSrcYSize;

    double  dfAccumulatorReal = 0.0, dfAccumulatorImag = 0.0;
    double  dfAccumulatorDensity = 0.0;
    double  dfAccumulatorWeight = 0.0;
    const int     iSrcX = (int) floor( dfSrcX - 0.5 );
    const int     iSrcY = (int) floor( dfSrcY - 0.5 );
    const int     iSrcOffset = iSrcX + iSrcY * nSrcXSize;
    const double  dfDeltaX = dfSrcX - 0.5 - iSrcX;
    const double  dfDeltaY = dfSrcY - 0.5 - iSrcY;

    const double  dfXScale = poWK->dfXScale, dfYScale = poWK->dfYScale;

    // Space for saved X weights
    double  *padfWeightsX = psWrkStruct->padfWeightsX;
    double  *padfWeightsY = psWrkStruct->padfWeightsY;

    // Space for saving a row of pixels
    double  *padfRowDensity = psWrkStruct->padfRowDensity;
    double  *padfRowReal = psWrkStruct->padfRowReal;
    double  *padfRowImag = psWrkStruct->padfRowImag;

    // Skip sampling over edge of image
    int jMin = poWK->nFiltInitY, jMax= poWK->nYRadius;
    if( iSrcY + jMin < 0 )
        jMin = -iSrcY;
    if( iSrcY + jMax >= nSrcYSize )
        jMax = nSrcYSize - iSrcY - 1;

    int iMin = poWK->nFiltInitX, iMax = poWK->nXRadius;
    if( iSrcX + iMin < 0 )
        iMin = -iSrcX;
    if( iSrcX + iMax >= nSrcXSize )
        iMax = nSrcXSize - iSrcX - 1;

    if( dfXScale < 1.0 )
    {
        while( iMin * dfXScale < -3.0 )
            iMin ++;
        while( iMax * dfXScale > 3.0 )
            iMax --;
        // padfWeightsX computed in GWKResampleCreateWrkStruct
    }
    else
    {
        while( iMin - dfDeltaX < -3.0 )
            iMin ++;
        while( iMax - dfDeltaX > 3.0 )
            iMax --;

        if( iSrcX != psWrkStruct->iLastSrcX ||
            dfDeltaX != psWrkStruct->dfLastDeltaX )
        {
            // Optimisation of GWKLanczosSinc(i - dfDeltaX) based on the following
            // trigonometric formulas.

    //sin(GWK_PI * (dfBase + k)) = sin(GWK_PI * dfBase) * cos(GWK_PI * k) + cos(GWK_PI * dfBase) * sin(GWK_PI * k)
    //sin(GWK_PI * (dfBase + k)) = dfSinPIBase * cos(GWK_PI * k) + dfCosPIBase * sin(GWK_PI * k)
    //sin(GWK_PI * (dfBase + k)) = dfSinPIBase * cos(GWK_PI * k)
    //sin(GWK_PI * (dfBase + k)) = dfSinPIBase * (((k % 2) == 0) ? 1 : -1)

    //sin(GWK_PI / dfR * (dfBase + k)) = sin(GWK_PI / dfR * dfBase) * cos(GWK_PI / dfR * k) + cos(GWK_PI / dfR * dfBase) * sin(GWK_PI / dfR * k)
    //sin(GWK_PI / dfR * (dfBase + k)) = dfSinPIBaseOverR * cos(GWK_PI / dfR * k) + dfCosPIBaseOverR * sin(GWK_PI / dfR * k)

            double dfSinPIDeltaXOver3 = sin((-GWK_PI / 3) * dfDeltaX);
            double dfSin2PIDeltaXOver3 = dfSinPIDeltaXOver3 * dfSinPIDeltaXOver3;
            /* ok to use sqrt(1-sin^2) since GWK_PI / 3 * dfDeltaX < PI/2 */
            double dfCosPIDeltaXOver3 = sqrt(1 - dfSin2PIDeltaXOver3);
            double dfSinPIDeltaX = (3-4*dfSin2PIDeltaXOver3)*dfSinPIDeltaXOver3;
            const double dfInvPI2Over3 = 3.0 / (GWK_PI * GWK_PI);
            double dfInvPI2Over3xSinPIDeltaX = dfInvPI2Over3 * dfSinPIDeltaX;
            double dfInvPI2Over3xSinPIDeltaXxm0d5SinPIDeltaXOver3 =
                -0.5 * dfInvPI2Over3xSinPIDeltaX * dfSinPIDeltaXOver3;
            const double dfSinPIOver3 = 0.8660254037844386;
            double dfInvPI2Over3xSinPIDeltaXxSinPIOver3xCosPIDeltaXOver3 =
                dfSinPIOver3 * dfInvPI2Over3xSinPIDeltaX * dfCosPIDeltaXOver3;
            double padfCst[] = {
                dfInvPI2Over3xSinPIDeltaX * dfSinPIDeltaXOver3,
                dfInvPI2Over3xSinPIDeltaXxm0d5SinPIDeltaXOver3 -
                        dfInvPI2Over3xSinPIDeltaXxSinPIOver3xCosPIDeltaXOver3,
                dfInvPI2Over3xSinPIDeltaXxm0d5SinPIDeltaXOver3 +
                        dfInvPI2Over3xSinPIDeltaXxSinPIOver3xCosPIDeltaXOver3 };

            for (int i = iMin; i <= iMax; ++i )
            {
                const double dfX = i - dfDeltaX;
                if (dfX == 0.0)
                    padfWeightsX[i-poWK->nFiltInitX] = 1.0;
                else
                    padfWeightsX[i-poWK->nFiltInitX] =
                                            padfCst[(i + 3) % 3] / (dfX * dfX);
                //CPLAssert(fabs(padfWeightsX[i-poWK->nFiltInitX] - GWKLanczosSinc(dfX, 3.0)) < 1e-10);
            }

            psWrkStruct->iLastSrcX = iSrcX;
            psWrkStruct->dfLastDeltaX = dfDeltaX;
        }
    }

    if( dfYScale < 1.0 )
    {
        while( jMin * dfYScale < -3.0 )
            jMin ++;
        while( jMax * dfYScale > 3.0 )
            jMax --;
        // padfWeightsY computed in GWKResampleCreateWrkStruct
    }
    else
    {
        while( jMin - dfDeltaY < -3.0 )
            jMin ++;
        while( jMax - dfDeltaY > 3.0 )
            jMax --;

        if( iSrcY != psWrkStruct->iLastSrcY ||
            dfDeltaY != psWrkStruct->dfLastDeltaY )
        {
            double dfSinPIDeltaYOver3 = sin((-GWK_PI / 3) * dfDeltaY);
            double dfSin2PIDeltaYOver3 = dfSinPIDeltaYOver3 * dfSinPIDeltaYOver3;
            /* ok to use sqrt(1-sin^2) since GWK_PI / 3 * dfDeltaY < PI/2 */
            double dfCosPIDeltaYOver3 = sqrt(1 - dfSin2PIDeltaYOver3);
            double dfSinPIDeltaY = (3-4*dfSin2PIDeltaYOver3)*dfSinPIDeltaYOver3;
            const double dfInvPI2Over3 = 3.0 / (GWK_PI * GWK_PI);
            double dfInvPI2Over3xSinPIDeltaY = dfInvPI2Over3 * dfSinPIDeltaY;
            double dfInvPI2Over3xSinPIDeltaYxm0d5SinPIDeltaYOver3 =
                -0.5 * dfInvPI2Over3xSinPIDeltaY * dfSinPIDeltaYOver3;
            const double dfSinPIOver3 = 0.8660254037844386;
            double dfInvPI2Over3xSinPIDeltaYxSinPIOver3xCosPIDeltaYOver3 =
                dfSinPIOver3 * dfInvPI2Over3xSinPIDeltaY * dfCosPIDeltaYOver3;
            double padfCst[] = {
                dfInvPI2Over3xSinPIDeltaY * dfSinPIDeltaYOver3,
                dfInvPI2Over3xSinPIDeltaYxm0d5SinPIDeltaYOver3 -
                        dfInvPI2Over3xSinPIDeltaYxSinPIOver3xCosPIDeltaYOver3,
                dfInvPI2Over3xSinPIDeltaYxm0d5SinPIDeltaYOver3 +
                        dfInvPI2Over3xSinPIDeltaYxSinPIOver3xCosPIDeltaYOver3 };

            for ( int j = jMin; j <= jMax; ++j )
            {
                const double dfY = j - dfDeltaY;
                if (dfY == 0.0)
                    padfWeightsY[j-poWK->nFiltInitY] = 1.0;
                else
                    padfWeightsY[j-poWK->nFiltInitY] =
                                            padfCst[(j + 3) % 3] / (dfY * dfY);
                //CPLAssert(fabs(padfWeightsY[j-poWK->nFiltInitY] - GWKLanczosSinc(dfY, 3.0)) < 1e-10);
            }

            psWrkStruct->iLastSrcY = iSrcY;
            psWrkStruct->dfLastDeltaY = dfDeltaY;
        }
    }

    int iRowOffset = iSrcOffset + (jMin - 1) * nSrcXSize + iMin;

    // If we have no density information, we can simply compute the
    // accumulated weight.
    if( padfRowDensity == NULL )
    {
        double dfRowAccWeight = 0.0;
        for (int i = iMin; i <= iMax; ++i )
        {
            dfRowAccWeight += padfWeightsX[i-poWK->nFiltInitX];
        }
        double dfColAccWeight = 0.0;
        for ( int j = jMin; j <= jMax; ++j )
        {
            dfColAccWeight += padfWeightsY[j-poWK->nFiltInitY];
        }
        dfAccumulatorWeight = dfRowAccWeight * dfColAccWeight;

        if( !GDALDataTypeIsComplex(poWK->eWorkingDataType) )
            padfRowImag = NULL;
    }

    // Loop over pixel rows in the kernel
    for ( int j = jMin; j <= jMax; ++j )
    {
        double  dfWeight1;

        iRowOffset += nSrcXSize;

        // Get pixel values
        // We can potentially read extra elements after the "normal" end of the source arrays,
        // but the contract of papabySrcImage[iBand], papanBandSrcValid[iBand],
        // panUnifiedSrcValid and pafUnifiedSrcDensity is to have WARP_EXTRA_ELTS
        // reserved at their end.
        if ( !GWKGetPixelRow( poWK, iBand, iRowOffset, (iMax-iMin+2)/2,
                              padfRowDensity, padfRowReal, padfRowImag ) )
            continue;

        dfWeight1 = padfWeightsY[j-poWK->nFiltInitY];

        // Iterate over pixels in row
        if ( padfRowDensity != NULL )
        {
            for (int i = iMin; i <= iMax; ++i )
            {
                double dfWeight2;

                // Skip sampling if pixel has zero density
                if ( padfRowDensity[i - iMin] < 0.000000001 )
                    continue;

                //  Use a cached set of weights for this row
                dfWeight2 = dfWeight1 * padfWeightsX[i- poWK->nFiltInitX];

                // Accumulate!
                dfAccumulatorReal += padfRowReal[i - iMin] * dfWeight2;
                dfAccumulatorImag += padfRowImag[i - iMin] * dfWeight2;
                dfAccumulatorDensity += padfRowDensity[i - iMin] * dfWeight2;
                dfAccumulatorWeight += dfWeight2;
            }
        }
        else if( padfRowImag == NULL )
        {
            double dfRowAccReal = 0.0;
            for (int i = iMin; i <= iMax; ++i )
            {
                double dfWeight2 = padfWeightsX[i- poWK->nFiltInitX];

                // Accumulate!
                dfRowAccReal += padfRowReal[i - iMin] * dfWeight2;
            }

            dfAccumulatorReal += dfRowAccReal * dfWeight1;
        }
        else
        {
            double dfRowAccReal = 0.0;
            double dfRowAccImag = 0.0;
            for (int i = iMin; i <= iMax; ++i )
            {
                double dfWeight2 = padfWeightsX[i- poWK->nFiltInitX];

                // Accumulate!
                dfRowAccReal += padfRowReal[i - iMin] * dfWeight2;
                dfRowAccImag += padfRowImag[i - iMin] * dfWeight2;
            }

            dfAccumulatorReal += dfRowAccReal * dfWeight1;
            dfAccumulatorImag += dfRowAccImag * dfWeight1;
        }
    }

    if ( dfAccumulatorWeight < 0.000001 ||
         (padfRowDensity != NULL && dfAccumulatorDensity < 0.000001) )
    {
        *pdfDensity = 0.0;
        return FALSE;
    }

    // Calculate the output taking into account weighting
    if ( dfAccumulatorWeight < 0.99999 || dfAccumulatorWeight > 1.00001 )
    {
        const double dfInvAcc = 1.0 / dfAccumulatorWeight;
        *pdfReal = dfAccumulatorReal * dfInvAcc;
        *pdfImag = dfAccumulatorImag * dfInvAcc;
        if( padfRowDensity != NULL )
            *pdfDensity = dfAccumulatorDensity * dfInvAcc;
        else
            *pdfDensity = 1.0;
    }
    else
    {
        *pdfReal = dfAccumulatorReal;
        *pdfImag = dfAccumulatorImag;
        if( padfRowDensity != NULL )
            *pdfDensity = dfAccumulatorDensity;
        else
            *pdfDensity = 1.0;
    }
    
    return TRUE;
}

template <class T>
static int GWKCubicSplineResampleNoMasksT( GDALWarpKernel *poWK, int iBand,
                                           double dfSrcX, double dfSrcY,
                                           T *pValue, double *padfBSpline )

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

    T*  pSrcBand = (T*) poWK->papabySrcImage[iBand];
    
    // Politely refusing to process invalid coordinates or obscenely small image
    if ( iSrcX >= nSrcXSize || iSrcY >= nSrcYSize
         || nXRadius > nSrcXSize || nYRadius > nSrcYSize )
        return GWKBilinearResampleNoMasksT( poWK, iBand, dfSrcX, dfSrcY, pValue);

    const int bXScaleBelow1 = ( dfXScale < 1.0 );
    const int bYScaleBelow1 = ( dfYScale < 1.0 );

    // Loop over all rows in the kernel
    double dfAccumulatorWeightHorizontal = 0.0;
    double dfAccumulatorWeightVertical = 0.0;
    
    int iMin = 1 - nXRadius;
    if( iSrcX + iMin < 0 )
        iMin = -iSrcX;
    int iMax = nXRadius;
    if( iSrcX + iMax >= nSrcXSize-1 )
        iMax = nSrcXSize-1 - iSrcX;
    int i, iC;
    for(iC = 0, i = iMin; i <= iMax; ++i, ++iC )
    {
        double dfWeight2 = (bXScaleBelow1) ?
                    GWKBSpline((dfDeltaX - i) * dfXScale):
                    GWKBSpline(dfDeltaX - i);
        padfBSpline[iC] = dfWeight2;
        dfAccumulatorWeightHorizontal += dfWeight2;
    }

    int j = 1 - nYRadius;
    if(  iSrcY + j < 0 )
        j = -iSrcY;
    int jMax = nYRadius;
    if( iSrcY + jMax >= nSrcYSize-1 )
        jMax = nSrcYSize-1 - iSrcY;

    for ( ; j <= jMax; ++j )
    {
        int     iSampJ = iSrcOffset + j * nSrcXSize;

        // Loop over all pixels in the row
        double dfAccumulatorLocal = 0.0;
        for(iC = 0, i = iMin; i <= iMax; ++i, ++iC )
        {
            double  dfWeight2 = padfBSpline[iC];

            // Retrieve the pixel & accumulate
            dfAccumulatorLocal += (double)pSrcBand[i+iSampJ] * dfWeight2;
        }

        // Calculate the Y weight
        double  dfWeight1 = (bYScaleBelow1) ?
            GWKBSpline((j - dfDeltaY) * dfYScale):
            GWKBSpline(j - dfDeltaY);

        dfAccumulator += dfWeight1 * dfAccumulatorLocal;
        dfAccumulatorWeightVertical += dfWeight1;
    }
    
    double dfAccumulatorWeight = dfAccumulatorWeightHorizontal * dfAccumulatorWeightVertical;
    
    if ( dfAccumulatorWeight >= 0.99999 && dfAccumulatorWeight <= 1.00001 )
        *pValue = GWKClampValueT<T>(dfAccumulator);
    else
        *pValue = GWKClampValueT<T>(dfAccumulator / dfAccumulatorWeight);

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
            int iSrcX, iSrcY, CPL_UNUSED iSrcOffset;\
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

static void GWKGeneralCaseThread(void* pData);

static CPLErr GWKGeneralCase( GDALWarpKernel *poWK )
{
    return GWKRun( poWK, "GWKGeneralCase", GWKGeneralCaseThread );
}

static void GWKGeneralCaseThread( void* pData)

{
    GWKJobStruct* psJob = (GWKJobStruct*) pData;
    GDALWarpKernel *poWK = psJob->poWK;
    int iYMin = psJob->iYMin;
    int iYMax = psJob->iYMax;

    int iDstY;
    int nDstXSize = poWK->nDstXSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;

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
    for( iDstY = iYMin; iDstY < iYMax; iDstY++ )
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
        poWK->pfnTransformer( psJob->pTransformerArg, TRUE, nDstXSize,
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

            if( poWK->pafUnifiedSrcDensity != NULL )
            {
                dfDensity = poWK->pafUnifiedSrcDensity[iSrcOffset];
                if( dfDensity < 0.00001 )
                    continue;
            }

            if( poWK->panUnifiedSrcValid != NULL
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
                    psWrkStruct->pfnGWKResample( poWK, iBand, 
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
        if (psJob->pfnProgress(psJob))
            break;
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
}

/************************************************************************/
/*                       GWKNearestNoMasksByte()                        */
/*                                                                      */
/*      Case for 8bit input data with nearest neighbour resampling      */
/*      without concerning about masking. Should be as fast as          */
/*      possible for this particular transformation type.               */
/************************************************************************/

template<class T>
static void GWKNearestNoMasksThread( void* pData )

{
    GWKJobStruct* psJob = (GWKJobStruct*) pData;
    GDALWarpKernel *poWK = psJob->poWK;
    int iYMin = psJob->iYMin;
    int iYMax = psJob->iYMax;

    int iDstY;
    int nDstXSize = poWK->nDstXSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;

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
    for( iDstY = iYMin; iDstY < iYMax; iDstY++ )
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
        poWK->pfnTransformer( psJob->pTransformerArg, TRUE, nDstXSize,
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
                ((T *)poWK->papabyDstImage[iBand])[iDstOffset] = 
                    ((T *)poWK->papabySrcImage[iBand])[iSrcOffset];
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if (psJob->pfnProgress(psJob))
            break;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );
}

static CPLErr GWKNearestNoMasksByte( GDALWarpKernel *poWK )
{
    return GWKRun( poWK, "GWKNearestNoMasksByte", GWKNearestNoMasksThread<GByte> );
}

/************************************************************************/
/*                       GWKBilinearNoMasksByte()                       */
/*                                                                      */
/*      Case for 8bit input data with bilinear resampling without       */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

template<class T>
static void GWKBilinearNoMasksThread( void* pData )

{
    GWKJobStruct* psJob = (GWKJobStruct*) pData;
    GDALWarpKernel *poWK = psJob->poWK;
    int iYMin = psJob->iYMin;
    int iYMax = psJob->iYMax;

    int iDstY;
    int nDstXSize = poWK->nDstXSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;

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
    for( iDstY = iYMin; iDstY < iYMax; iDstY++ )
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
        poWK->pfnTransformer( psJob->pTransformerArg, TRUE, nDstXSize,
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
                T value = 0;
                GWKBilinearResampleNoMasksT( poWK, iBand,
                                             padfX[iDstX]-poWK->nSrcXOff,
                                             padfY[iDstX]-poWK->nSrcYOff,
                                             &value );
                ((T *)poWK->papabyDstImage[iBand])[iDstOffset] = value;
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if (psJob->pfnProgress(psJob))
            break;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );
}

static CPLErr GWKBilinearNoMasksByte( GDALWarpKernel *poWK )
{
    return GWKRun( poWK, "GWKBilinearNoMasksByte", GWKBilinearNoMasksThread<GByte> );
}

/************************************************************************/
/*                       GWKCubicNoMasksByte()                          */
/*                                                                      */
/*      Case for 8bit input data with cubic resampling without          */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

template<class T>
static void GWKCubicNoMasksThread( void* pData )

{
    GWKJobStruct* psJob = (GWKJobStruct*) pData;
    GDALWarpKernel *poWK = psJob->poWK;
    int iYMin = psJob->iYMin;
    int iYMax = psJob->iYMax;

    int iDstY;
    int nDstXSize = poWK->nDstXSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;

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
    for( iDstY = iYMin; iDstY < iYMax; iDstY++ )
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
        poWK->pfnTransformer( psJob->pTransformerArg, TRUE, nDstXSize,
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
                T  value = 0;
                GWKCubicResampleNoMasksT( poWK, iBand,
                                              padfX[iDstX]-poWK->nSrcXOff,
                                              padfY[iDstX]-poWK->nSrcYOff,
                                              &value );
                ((T *)poWK->papabyDstImage[iBand])[iDstOffset] = value;
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if (psJob->pfnProgress(psJob))
            break;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );
}


static CPLErr GWKCubicNoMasksByte( GDALWarpKernel *poWK )
{
    return GWKRun( poWK, "GWKCubicNoMasksByte", GWKCubicNoMasksThread<GByte> );
}

/************************************************************************/
/*                   GWKCubicSplineNoMasksByte()                        */
/*                                                                      */
/*      Case for 8bit input data with cubic spline resampling without   */
/*      concerning about masking. Should be as fast as possible         */
/*      for this particular transformation type.                        */
/************************************************************************/

template<class T>
static void GWKCubicSplineNoMasksThread( void* pData )

{
    GWKJobStruct* psJob = (GWKJobStruct*) pData;
    GDALWarpKernel *poWK = psJob->poWK;
    int iYMin = psJob->iYMin;
    int iYMax = psJob->iYMax;

    int iDstY;
    int nDstXSize = poWK->nDstXSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;

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
    for( iDstY = iYMin; iDstY < iYMax; iDstY++ )
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
        poWK->pfnTransformer( psJob->pTransformerArg, TRUE, nDstXSize,
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
                GWKCubicSplineResampleNoMasksT( poWK, iBand,
                                                   padfX[iDstX]-poWK->nSrcXOff,
                                                   padfY[iDstX]-poWK->nSrcYOff,
                                                   &(((T*)(poWK->papabyDstImage[iBand]))[iDstOffset]),
                                                   padfBSpline);
            }
        }

/* -------------------------------------------------------------------- */
/*      Report progress to the user, and optionally cancel out.         */
/* -------------------------------------------------------------------- */
        if (psJob->pfnProgress(psJob))
            break;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );
    CPLFree( padfBSpline );
}

static CPLErr GWKCubicSplineNoMasksByte( GDALWarpKernel *poWK )
{
    return GWKRun( poWK, "GWKCubicSplineNoMasksByte", GWKCubicSplineNoMasksThread<GByte> );
}

/************************************************************************/
/*                          GWKNearestByte()                            */
/*                                                                      */
/*      Case for 8bit input data with nearest neighbour resampling      */
/*      using valid flags. Should be as fast as possible for this       */
/*      particular transformation type.                                 */
/************************************************************************/

template<class T>
static void GWKNearestThread( void* pData )

{
    GWKJobStruct* psJob = (GWKJobStruct*) pData;
    GDALWarpKernel *poWK = psJob->poWK;
    int iYMin = psJob->iYMin;
    int iYMax = psJob->iYMax;

    int iDstY;
    int nDstXSize = poWK->nDstXSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;

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
    for( iDstY = iYMin; iDstY < iYMax; iDstY++ )
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
        poWK->pfnTransformer( psJob->pTransformerArg, TRUE, nDstXSize,
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
                T   value = 0;
                double dfBandDensity = 0.0;

/* -------------------------------------------------------------------- */
/*      Collect the source value.                                       */
/* -------------------------------------------------------------------- */
                if ( GWKGetPixelT(poWK, iBand, iSrcOffset, &dfBandDensity, &value) )
                {
                    if( dfBandDensity < 1.0 )
                    {
                        if( dfBandDensity == 0.0 )
                            /* do nothing */;
                        else
                        {
                            /* let the general code take care of mixing */
                            GWKSetPixelValueRealT( poWK, iBand, iDstOffset, 
                                          dfBandDensity, value );
                        }
                    }
                    else
                    {
                        ((T *)poWK->papabyDstImage[iBand])[iDstOffset] = value;
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
        if (psJob->pfnProgress(psJob))
            break;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( pabSuccess );
}

static CPLErr GWKNearestByte( GDALWarpKernel *poWK )
{
    return GWKRun( poWK, "GWKNearestByte", GWKNearestThread<GByte> );
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
    return GWKRun( poWK, "GWKNearestNoMasksShort", GWKNearestNoMasksThread<GInt16> );
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
    return GWKRun( poWK, "GWKBilinearNoMasksShort", GWKBilinearNoMasksThread<GInt16> );
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
    return GWKRun( poWK, "GWKCubicNoMasksShort", GWKCubicNoMasksThread<GInt16> );
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
    return GWKRun( poWK, "GWKCubicSplineNoMasksShort", GWKCubicSplineNoMasksThread<GInt16> );
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
    return GWKRun( poWK, "GWKNearestShort", GWKNearestThread<GInt16> );
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
    return GWKRun( poWK, "GWKNearestNoMasksFloat", GWKNearestNoMasksThread<float> );
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
    return GWKRun( poWK, "GWKNearestFloat", GWKNearestThread<float> );
}


/************************************************************************/
/*                           GWKAverageOrMode()                         */
/*                                                                      */
/************************************************************************/

static void GWKAverageOrModeThread(void* pData);

static CPLErr GWKAverageOrMode( GDALWarpKernel *poWK )
{
    return GWKRun( poWK, "GWKAverageOrMode", GWKAverageOrModeThread );
}

// overall logic based on GWKGeneralCaseThread()
static void GWKAverageOrModeThread( void* pData)
{
    GWKJobStruct* psJob = (GWKJobStruct*) pData;
    GDALWarpKernel *poWK = psJob->poWK;
    int iYMin = psJob->iYMin;
    int iYMax = psJob->iYMax;

    int iDstY, iDstX, iSrcX, iSrcY, iDstOffset;
    int nDstXSize = poWK->nDstXSize;
    int nSrcXSize = poWK->nSrcXSize, nSrcYSize = poWK->nSrcYSize;

/* -------------------------------------------------------------------- */
/*      Find out which algorithm to use (small optim.)                  */
/* -------------------------------------------------------------------- */
    int nAlgo = 0;

    // these vars only used with nAlgo == 3
    int *panVals = NULL;
    int nBins = 0, nBinsOffset = 0;

    // only used with nAlgo = 2
    float*   pafVals = NULL;
    int*     panSums = NULL;

    if ( poWK->eResample == GRA_Average ) 
    {
        nAlgo = 1;
    }
    else if( poWK->eResample == GRA_Mode )
    {
        // TODO check color table count > 256
        if ( poWK->eWorkingDataType == GDT_Byte ||
             poWK->eWorkingDataType == GDT_UInt16 ||
             poWK->eWorkingDataType == GDT_Int16 )
        {
            nAlgo = 3;

            /* In the case of a paletted or non-paletted byte band, */
            /* input values are between 0 and 255 */
            if ( poWK->eWorkingDataType == GDT_Byte )
            {
                nBins = 256;
            }
            /* In the case of Int16, input values are between -32768 and 32767 */
            else if ( poWK->eWorkingDataType == GDT_Int16 )
            {
                nBins = 65536;
                nBinsOffset = 32768;
            }
            /* In the case of UInt16, input values are between 0 and 65537 */
            else if ( poWK->eWorkingDataType == GDT_UInt16 )
            {
                nBins = 65536;
            }
            panVals = (int*) VSIMalloc(nBins * sizeof(int));
            if( panVals == NULL )
                return;
        }
        else
        {
            nAlgo = 2;

            if ( nSrcXSize > 0 && nSrcYSize > 0 )
            {
                pafVals = (float*) VSIMalloc3(nSrcXSize, nSrcYSize, sizeof(float));
                panSums = (int*) VSIMalloc3(nSrcXSize, nSrcYSize, sizeof(int));
                if( pafVals == NULL || panSums == NULL )
                {
                    VSIFree(pafVals);
                    VSIFree(panSums);
                    return;
                }
            }
        }
    }
    else
    {
        // other resample algorithms not permitted here
        CPLDebug( "GDAL", "GDALWarpKernel():GWKAverageOrModeThread() ERROR, illegal resample" );
        return;
    }
    CPLDebug( "GDAL", "GDALWarpKernel():GWKAverageOrModeThread() using algo %d", nAlgo );

/* -------------------------------------------------------------------- */
/*      Allocate x,y,z coordinate arrays for transformation ... two     */
/*      scanlines worth of positions.                                   */
/* -------------------------------------------------------------------- */
    double *padfX, *padfY, *padfZ;
    double *padfX2, *padfY2, *padfZ2;
    int    *pabSuccess, *pabSuccess2;

    padfX = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfX2 = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfY2 = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    padfZ2 = (double *) CPLMalloc(sizeof(double) * nDstXSize);
    pabSuccess = (int *) CPLMalloc(sizeof(int) * nDstXSize);
    pabSuccess2 = (int *) CPLMalloc(sizeof(int) * nDstXSize);

/* ==================================================================== */
/*      Loop over output lines.                                         */
/* ==================================================================== */
    for( iDstY = iYMin; iDstY < iYMax; iDstY++ )
    {

/* -------------------------------------------------------------------- */
/*      Setup points to transform to source image space.                */
/* -------------------------------------------------------------------- */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            padfX[iDstX] = iDstX + poWK->nDstXOff;
            padfY[iDstX] = iDstY + poWK->nDstYOff;
            padfZ[iDstX] = 0.0;
            padfX2[iDstX] = iDstX + 1.0 + poWK->nDstXOff;
            padfY2[iDstX] = iDstY + 1.0 + poWK->nDstYOff;
            padfZ2[iDstX] = 0.0;
        }

/* -------------------------------------------------------------------- */
/*      Transform the points from destination pixel/line coordinates    */
/*      to source pixel/line coordinates.                               */
/* -------------------------------------------------------------------- */
        poWK->pfnTransformer( psJob->pTransformerArg, TRUE, nDstXSize,
                              padfX, padfY, padfZ, pabSuccess );
        poWK->pfnTransformer( psJob->pTransformerArg, TRUE, nDstXSize,
                              padfX2, padfY2, padfZ2, pabSuccess2 );

/* ==================================================================== */
/*      Loop over pixels in output scanline.                            */
/* ==================================================================== */
        for( iDstX = 0; iDstX < nDstXSize; iDstX++ )
        {
            int iSrcOffset = 0;
            double  dfDensity = 1.0;
            int bHasFoundDensity = FALSE;

            if( !pabSuccess[iDstX] || !pabSuccess2[iDstX] )
                continue;
            iDstOffset = iDstX + iDstY * nDstXSize;

/* ==================================================================== */
/*      Loop processing each band.                                      */
/* ==================================================================== */
            
            for( int iBand = 0; iBand < poWK->nBands; iBand++ )
            {
                double dfBandDensity = 0.0;
                double dfValueReal = 0.0;
                double dfValueImag = 0.0;
                double dfValueRealTmp = 0.0;
                double dfValueImagTmp = 0.0;

/* -------------------------------------------------------------------- */
/*      Collect the source value.                                       */
/* -------------------------------------------------------------------- */

                double dfTotal = 0;
                int    nCount = 0;  // count of pixels used to compute average/mode
                int    nCount2 = 0; // count of all pixels sampled, including nodata
                int iSrcXMin, iSrcXMax,iSrcYMin,iSrcYMax;

                // compute corners in source crs
                iSrcXMin = MAX( ((int) floor((padfX[iDstX] + 1e-10))) - poWK->nSrcXOff, 0 ); 
                iSrcXMax = MIN( ((int) ceil((padfX2[iDstX] - 1e-10))) - poWK->nSrcXOff, nSrcXSize ); 
                iSrcYMin = MAX( ((int) floor((padfY[iDstX] + 1e-10))) - poWK->nSrcYOff, 0 ); 
                iSrcYMax = MIN( ((int) ceil((padfY2[iDstX] - 1e-10))) - poWK->nSrcYOff, nSrcYSize );
                
                // The transformation might not have preserved ordering of coordinates
                // so do the necessary swapping (#5433)
                // NOTE: this is really an approximative fix. To do something more precise
                // we would for example need to compute the transformation of coordinates
                // in the [iDstX,iDstY]x[iDstX+1,iDstY+1] square back to source coordinates,
                // and take the bounding box of the got source coordinates.
                if( iSrcXMax < iSrcXMin )
                {
                    iSrcXMin = MAX( ((int) floor((padfX2[iDstX] + 1e-10))) - poWK->nSrcXOff, 0 ); 
                    iSrcXMax = MIN( ((int) ceil((padfX[iDstX] - 1e-10))) - poWK->nSrcXOff, nSrcXSize ); 
                }
                if( iSrcYMax < iSrcYMin )
                {
                    iSrcYMin = MAX( ((int) floor((padfY2[iDstX] + 1e-10))) - poWK->nSrcYOff, 0 ); 
                    iSrcYMax = MIN( ((int) ceil((padfY[iDstX] - 1e-10))) - poWK->nSrcYOff, nSrcYSize );
                }
                if( iSrcXMin == iSrcXMax && iSrcXMax < nSrcXSize )
                    iSrcXMax ++;
                if( iSrcYMin == iSrcYMax && iSrcYMax < nSrcYSize )
                    iSrcYMax ++;

                // loop over source lines and pixels - 3 possible algorithms
                
                if ( nAlgo == 1 ) // poWK->eResample == GRA_Average
                {
                    // this code adapted from GDALDownsampleChunk32R_AverageT() in gcore/overview.cpp
                    for( iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++ )
                    {
                        for( iSrcX = iSrcXMin; iSrcX < iSrcXMax; iSrcX++ )
                        {
                            iSrcOffset = iSrcX + iSrcY * nSrcXSize;
                            
                            if( poWK->panUnifiedSrcValid != NULL
                                && !(poWK->panUnifiedSrcValid[iSrcOffset>>5]
                                     & (0x01 << (iSrcOffset & 0x1f))) )
                            {
                                continue;
                            }
                            
                            nCount2++;
                            if ( GWKGetPixelValue( poWK, iBand, iSrcOffset,
                                                   &dfBandDensity, &dfValueRealTmp, &dfValueImagTmp ) && dfBandDensity > 0.0000000001 ) 
                            {
                                nCount++;
                                dfTotal += dfValueRealTmp;
                            }
                        }
                    }
                    
                    if ( nCount > 0 )
                    {                
                        dfValueReal = dfTotal / nCount;
                        dfBandDensity = 1;                
                        bHasFoundDensity = TRUE;
                    }
                                       
                } // GRA_Average
                
                else if ( nAlgo == 2 || nAlgo == 3 ) // poWK->eResample == GRA_Mode
                {
                    // this code adapted from GDALDownsampleChunk32R_Mode() in gcore/overview.cpp

                    if ( nAlgo == 2 ) // int32 or float
                    {
                        /* I'm not sure how much sense it makes to run a majority
                           filter on floating point data, but here it is for the sake
                           of compatability. It won't look right on RGB images by the
                           nature of the filter. */
                        int     iMaxInd = 0, iMaxVal = -1, i = 0;

                        for( iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++ )
                        {
                            for( iSrcX = iSrcXMin; iSrcX < iSrcXMax; iSrcX++ )
                            {
                                iSrcOffset = iSrcX + iSrcY * nSrcXSize;
                                
                                if( poWK->panUnifiedSrcValid != NULL
                                    && !(poWK->panUnifiedSrcValid[iSrcOffset>>5]
                                         & (0x01 << (iSrcOffset & 0x1f))) )
                                    continue;
                                
                                nCount2++;
                                if ( GWKGetPixelValue( poWK, iBand, iSrcOffset,
                                                       &dfBandDensity, &dfValueRealTmp, &dfValueImagTmp ) && dfBandDensity > 0.0000000001 ) 
                                {
                                    nCount++;

                                    float fVal = (float)dfValueRealTmp;
                                    
                                    //Check array for existing entry
                                    for( i = 0; i < iMaxInd; ++i )
                                        if( pafVals[i] == fVal
                                            && ++panSums[i] > panSums[iMaxVal] )
                                        {
                                            iMaxVal = i;
                                            break;
                                        }
                                    
                                    //Add to arr if entry not already there
                                    if( i == iMaxInd )
                                    {
                                        pafVals[iMaxInd] = fVal;
                                        panSums[iMaxInd] = 1;
                                        
                                        if( iMaxVal < 0 )
                                            iMaxVal = iMaxInd;
                                        
                                        ++iMaxInd;
                                    }
                                }
                            }
                        }

                        if( iMaxVal != -1 )
                        {
                            dfValueReal = pafVals[iMaxVal];
                            dfBandDensity = 1;                
                            bHasFoundDensity = TRUE;
                        }
                    }
                    
                    else // byte or int16
                    {
                        int nMaxVal = 0, iMaxInd = -1;

                        memset(panVals, 0, nBins*sizeof(int));
                        
                        for( iSrcY = iSrcYMin; iSrcY < iSrcYMax; iSrcY++ )
                        {
                            for( iSrcX = iSrcXMin; iSrcX < iSrcXMax; iSrcX++ )
                            {
                                iSrcOffset = iSrcX + iSrcY * nSrcXSize;
                                
                                if( poWK->panUnifiedSrcValid != NULL
                                    && !(poWK->panUnifiedSrcValid[iSrcOffset>>5]
                                         & (0x01 << (iSrcOffset & 0x1f))) )
                                    continue;
                                
                                nCount2++;
                                if ( GWKGetPixelValue( poWK, iBand, iSrcOffset,
                                                       &dfBandDensity, &dfValueRealTmp, &dfValueImagTmp ) && dfBandDensity > 0.0000000001 ) 
                                {
                                    nCount++;

                                    int nVal = (int) dfValueRealTmp;
                                    if ( ++panVals[nVal+nBinsOffset] > nMaxVal)
                                    {
                                        //Sum the density
                                        //Is it the most common value so far?
                                        iMaxInd = nVal;
                                        nMaxVal = panVals[nVal+nBinsOffset];
                                    }
                                }
                            }
                        }
                        
                        if( iMaxInd != -1 )
                        {
                            dfValueReal = (float)iMaxInd;
                            dfBandDensity = 1;                
                            bHasFoundDensity = TRUE;                  
                        }
                    }
                    
                } // GRA_Mode

/* -------------------------------------------------------------------- */
/*      We have a computed value from the source.  Now apply it to      */
/*      the destination pixel.                                          */
/* -------------------------------------------------------------------- */
                if ( bHasFoundDensity )
                {
                    // TODO should we compute dfBandDensity in fct of nCount/nCount2 ,
                    // or use as a threshold to set the dest value?
                    // dfBandDensity = (float) nCount / nCount2;
                    // if ( (float) nCount / nCount2 > 0.1 )
                    // or fix gdalwarp crop_to_cutline to crop partially overlapping pixels
                    GWKSetPixelValue( poWK, iBand, iDstOffset,
                                      dfBandDensity,
                                      dfValueReal, dfValueImag );
                }                    
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
        if (psJob->pfnProgress(psJob))
            break;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    CPLFree( padfX );
    CPLFree( padfY );
    CPLFree( padfZ );
    CPLFree( padfX2 );
    CPLFree( padfY2 );
    CPLFree( padfZ2 );
    CPLFree( pabSuccess );
    CPLFree( pabSuccess2 );
    VSIFree( panVals );
    VSIFree(pafVals);
    VSIFree(panSums);
}
