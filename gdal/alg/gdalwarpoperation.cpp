/******************************************************************************
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implementation of the GDALWarpOperation class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_config.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_core.h"

CPL_CVSID("$Id$")

struct _GDALWarpChunk {
    int dx, dy, dsx, dsy;
    int sx, sy, ssx, ssy;
    int sExtraSx, sExtraSy;
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALWarpOperation                           */
/* ==================================================================== */
/************************************************************************/

/**
 * \class GDALWarpOperation "gdalwarper.h"
 *
 * High level image warping class.

<h2>Warper Design</h2>

The overall GDAL high performance image warper is split into a few components.

 - The transformation between input and output file coordinates is handled
via GDALTransformerFunc() implementations such as the one returned by
GDALCreateGenImgProjTransformer().  The transformers are ultimately responsible
for translating pixel/line locations on the destination image to pixel/line
locations on the source image.

 - In order to handle images too large to hold in RAM, the warper needs to
segment large images.  This is the responsibility of the GDALWarpOperation
class.  The GDALWarpOperation::ChunkAndWarpImage() invokes
GDALWarpOperation::WarpRegion() on chunks of output and input image that
are small enough to hold in the amount of memory allowed by the application.
This process is described in greater detail in the <b>Image Chunking</b>
section.

 - The GDALWarpOperation::WarpRegion() function creates and loads an output
image buffer, and then calls WarpRegionToBuffer().

 - GDALWarpOperation::WarpRegionToBuffer() is responsible for loading the
source imagery corresponding to a particular output region, and generating
masks and density masks from the source and destination imagery using
the generator functions found in the GDALWarpOptions structure.  Binds this
all into an instance of GDALWarpKernel on which the
GDALWarpKernel::PerformWarp() method is called.

 - GDALWarpKernel does the actual image warping, but is given an input image
and an output image to operate on.  The GDALWarpKernel does no IO, and in
fact knows nothing about GDAL.  It invokes the transformation function to
get sample locations, builds output values based on the resampling algorithm
in use.  It also takes any validity and density masks into account during
this operation.

<h3>Chunk Size Selection</h3>

The GDALWarpOptions ChunkAndWarpImage() method is responsible for invoking
the WarpRegion() method on appropriate sized output chunks such that the
memory required for the output image buffer, input image buffer and any
required density and validity buffers is less than or equal to the application
defined maximum memory available for use.

It checks the memory required by walking the edges of the output region,
transforming the locations back into source pixel/line coordinates and
establishing a bounding rectangle of source imagery that would be required
for the output area.  This is actually accomplished by the private
GDALWarpOperation::ComputeSourceWindow() method.

Then memory requirements are used by totaling the memory required for all
output bands, input bands, validity masks and density masks.  If this is
greater than the GDALWarpOptions::dfWarpMemoryLimit then the destination
region is divided in two (splitting the longest dimension), and
ChunkAndWarpImage() recursively invoked on each destination subregion.

<h3>Validity and Density Masks Generation</h3>

Fill in ways in which the validity and density masks may be generated here.
Note that detailed semantics of the masks should be found in
GDALWarpKernel.
*/

/************************************************************************/
/*                         GDALWarpOperation()                          */
/************************************************************************/

GDALWarpOperation::GDALWarpOperation() :
    psOptions(NULL),
    hIOMutex(NULL),
    hWarpMutex(NULL),
    nChunkListCount(0),
    nChunkListMax(0),
    pasChunkList(NULL),
    bReportTimings(FALSE),
    nLastTimeReported(0),
    psThreadData(NULL)
{}

/************************************************************************/
/*                         ~GDALWarpOperation()                         */
/************************************************************************/

GDALWarpOperation::~GDALWarpOperation()

{
    WipeOptions();

    if( hIOMutex != NULL )
    {
        CPLDestroyMutex( hIOMutex );
        CPLDestroyMutex( hWarpMutex );
    }

    WipeChunkList();
    if( psThreadData )
        GWKThreadsEnd(psThreadData);
}

/************************************************************************/
/*                             GetOptions()                             */
/************************************************************************/

/** Return warp options */
const GDALWarpOptions *GDALWarpOperation::GetOptions()

{
    return psOptions;
}

/************************************************************************/
/*                            WipeOptions()                             */
/************************************************************************/

void GDALWarpOperation::WipeOptions()

{
    if( psOptions != NULL )
    {
        GDALDestroyWarpOptions( psOptions );
        psOptions = NULL;
    }
}

/************************************************************************/
/*                          ValidateOptions()                           */
/************************************************************************/

int GDALWarpOperation::ValidateOptions()

{
    if( psOptions == NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "no options currently initialized." );
        return FALSE;
    }

    if( psOptions->dfWarpMemoryLimit < 100000.0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "dfWarpMemoryLimit=%g is unreasonably small.",
                  psOptions->dfWarpMemoryLimit );
        return FALSE;
    }

    if( psOptions->eResampleAlg != GRA_NearestNeighbour
        && psOptions->eResampleAlg != GRA_Bilinear
        && psOptions->eResampleAlg != GRA_Cubic
        && psOptions->eResampleAlg != GRA_CubicSpline
        && psOptions->eResampleAlg != GRA_Lanczos
        && psOptions->eResampleAlg != GRA_Average
        && psOptions->eResampleAlg != GRA_Mode
        && psOptions->eResampleAlg != GRA_Max
        && psOptions->eResampleAlg != GRA_Min
        && psOptions->eResampleAlg != GRA_Med
        && psOptions->eResampleAlg != GRA_Q1
        && psOptions->eResampleAlg != GRA_Q3)
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "eResampleArg=%d is not a supported value.",
                  psOptions->eResampleAlg );
        return FALSE;
    }

    if( static_cast<int>(psOptions->eWorkingDataType) < 1 ||
        static_cast<int>(psOptions->eWorkingDataType) >= GDT_TypeCount )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "eWorkingDataType=%d is not a supported value.",
                  psOptions->eWorkingDataType );
        return FALSE;
    }

    if( psOptions->hSrcDS == NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "hSrcDS is not set." );
        return FALSE;
    }

    if( psOptions->nBandCount == 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "nBandCount=0, no bands configured!" );
        return FALSE;
    }

    if( psOptions->panSrcBands == NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "panSrcBands is NULL." );
        return FALSE;
    }

    if( psOptions->hDstDS != NULL && psOptions->panDstBands == NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "panDstBands is NULL." );
        return FALSE;
    }

    for( int iBand = 0; iBand < psOptions->nBandCount; iBand++ )
    {
        if( psOptions->panSrcBands[iBand] < 1
            || psOptions->panSrcBands[iBand]
            > GDALGetRasterCount( psOptions->hSrcDS ) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "panSrcBands[%d] = %d ... out of range for dataset.",
                      iBand, psOptions->panSrcBands[iBand] );
            return FALSE;
        }
        if( psOptions->hDstDS != NULL
            && (psOptions->panDstBands[iBand] < 1
                || psOptions->panDstBands[iBand]
                > GDALGetRasterCount( psOptions->hDstDS ) ) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "panDstBands[%d] = %d ... out of range for dataset.",
                      iBand, psOptions->panDstBands[iBand] );
            return FALSE;
        }

        if( psOptions->hDstDS != NULL
            && GDALGetRasterAccess(
                GDALGetRasterBand(psOptions->hDstDS,
                                  psOptions->panDstBands[iBand]))
            == GA_ReadOnly )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "Destination band %d appears to be read-only.",
                      psOptions->panDstBands[iBand] );
            return FALSE;
        }
    }

    if( psOptions->nBandCount == 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "nBandCount=0, no bands configured!" );
        return FALSE;
    }

    if( psOptions->pfnProgress == NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "pfnProgress is NULL." );
        return FALSE;
    }

    if( psOptions->pfnTransformer == NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "pfnTransformer is NULL." );
        return FALSE;
    }

    if( CSLFetchNameValue( psOptions->papszWarpOptions,
                           "SAMPLE_STEPS" ) != NULL )
    {
        if( atoi(CSLFetchNameValue( psOptions->papszWarpOptions,
                                    "SAMPLE_STEPS" )) < 2 )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "GDALWarpOptions.Validate(): "
                      "SAMPLE_STEPS warp option has illegal value." );
            return FALSE;
        }
    }

    if( psOptions->nSrcAlphaBand > 0)
    {
        if( psOptions->hSrcDS == NULL ||
            psOptions->nSrcAlphaBand > GDALGetRasterCount(psOptions->hSrcDS) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "nSrcAlphaBand = %d ... out of range for dataset.",
                      psOptions->nSrcAlphaBand );
            return FALSE;
        }
    }

    if( psOptions->nDstAlphaBand > 0)
    {
        if( psOptions->hDstDS == NULL ||
            psOptions->nDstAlphaBand > GDALGetRasterCount(psOptions->hDstDS) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "nDstAlphaBand = %d ... out of range for dataset.",
                      psOptions->nDstAlphaBand );
            return FALSE;
        }
    }

    if( psOptions->nSrcAlphaBand > 0
        && psOptions->pfnSrcDensityMaskFunc != NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "pfnSrcDensityMaskFunc provided as well as a SrcAlphaBand." );
        return FALSE;
    }

    if( psOptions->nDstAlphaBand > 0
        && psOptions->pfnDstDensityMaskFunc != NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg,
                  "GDALWarpOptions.Validate(): "
                  "pfnDstDensityMaskFunc provided as well as a DstAlphaBand." );
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                            SetAlphaMax()                             */
/************************************************************************/

static void SetAlphaMax( GDALWarpOptions* psOptions,
                         GDALRasterBandH hBand,
                         const char* pszKey )
{
    const char* pszNBits =
        GDALGetMetadataItem(hBand, "NBITS", "IMAGE_STRUCTURE");
    const char *pszAlphaMax = NULL;
    if( pszNBits )
    {
        pszAlphaMax = CPLSPrintf("%u", (1U << atoi(pszNBits)) - 1U);
    }
    else if( GDALGetRasterDataType( hBand ) == GDT_Int16 )
    {
        pszAlphaMax = "32767";
    }
    else if( GDALGetRasterDataType( hBand ) == GDT_UInt16 )
    {
        pszAlphaMax = "65535";
    }

    if( pszAlphaMax != NULL )
        psOptions->papszWarpOptions = CSLSetNameValue(
            psOptions->papszWarpOptions, pszKey, pszAlphaMax);
    else
        CPLDebug("WARP", "SetAlphaMax: AlphaMax not set.");
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpOperation::Initialize( const GDALWarpOptions * );
 *
 * This method initializes the GDALWarpOperation's concept of the warp
 * options in effect.  It creates an internal copy of the GDALWarpOptions
 * structure and defaults a variety of additional fields in the internal
 * copy if not set in the provides warp options.
 *
 * Defaulting operations include:
 *  - If the nBandCount is 0, it will be set to the number of bands in the
 *    source image (which must match the output image) and the panSrcBands
 *    and panDstBands will be populated.
 *
 * @param psNewOptions input set of warp options.  These are copied and may
 * be destroyed after this call by the application.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpOperation::Initialize( const GDALWarpOptions *psNewOptions )

{
/* -------------------------------------------------------------------- */
/*      Copy the passed in options.                                     */
/* -------------------------------------------------------------------- */
    if( psOptions != NULL )
        WipeOptions();

    psOptions = GDALCloneWarpOptions( psNewOptions );
    psOptions->papszWarpOptions = CSLSetNameValue(psOptions->papszWarpOptions,
        "EXTRA_ELTS", CPLSPrintf("%d", WARP_EXTRA_ELTS));

/* -------------------------------------------------------------------- */
/*      Default band mapping if missing.                                */
/* -------------------------------------------------------------------- */
    if( psOptions->nBandCount == 0
        && psOptions->hSrcDS != NULL
        && psOptions->hDstDS != NULL
        && GDALGetRasterCount( psOptions->hSrcDS )
        == GDALGetRasterCount( psOptions->hDstDS ) )
    {
        GDALWarpInitDefaultBandMapping( 
            psOptions, GDALGetRasterCount( psOptions->hSrcDS ) );
    }

    GDALWarpResolveWorkingDataType(psOptions);

/* -------------------------------------------------------------------- */
/*      Default memory available.                                       */
/*                                                                      */
/*      For now we default to 64MB of RAM, but eventually we should     */
/*      try various schemes to query physical RAM.  This can            */
/*      certainly be done on Win32 and Linux.                           */
/* -------------------------------------------------------------------- */
    if( psOptions->dfWarpMemoryLimit == 0.0 )
    {
        psOptions->dfWarpMemoryLimit = 64.0 * 1024*1024;
    }

/* -------------------------------------------------------------------- */
/*      Are we doing timings?                                           */
/* -------------------------------------------------------------------- */
    bReportTimings = CPLFetchBool( psOptions->papszWarpOptions,
                                   "REPORT_TIMINGS", false );

/* -------------------------------------------------------------------- */
/*      Support creating cutline from text warpoption.                  */
/* -------------------------------------------------------------------- */
    const char *pszCutlineWKT =
        CSLFetchNameValue( psOptions->papszWarpOptions, "CUTLINE" );

    CPLErr eErr = CE_None;
    if( pszCutlineWKT && psOptions->hCutline == NULL )
    {
        if( OGR_G_CreateFromWkt( (char **) &pszCutlineWKT, NULL,
                                 (OGRGeometryH *) &(psOptions->hCutline) )
            != OGRERR_NONE )
        {
            eErr = CE_Failure;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to parse CUTLINE geometry wkt." );
        }
    }
    const char *pszBD = CSLFetchNameValue( psOptions->papszWarpOptions,
                                            "CUTLINE_BLEND_DIST" );
    if( pszBD )
        psOptions->dfCutlineBlendDist = CPLAtof(pszBD);

/* -------------------------------------------------------------------- */
/*      Set SRC_ALPHA_MAX if not provided.                              */
/* -------------------------------------------------------------------- */
    if( psOptions->hSrcDS != NULL &&
        psOptions->nSrcAlphaBand > 0 &&
        psOptions->nSrcAlphaBand <= GDALGetRasterCount(psOptions->hSrcDS) &&
        CSLFetchNameValue( psOptions->papszWarpOptions,
                           "SRC_ALPHA_MAX" ) == NULL )
    {
        GDALRasterBandH hSrcAlphaBand = GDALGetRasterBand(
                          psOptions->hSrcDS, psOptions->nSrcAlphaBand);
        SetAlphaMax( psOptions, hSrcAlphaBand, "SRC_ALPHA_MAX" );
    }

/* -------------------------------------------------------------------- */
/*      Set DST_ALPHA_MAX if not provided.                              */
/* -------------------------------------------------------------------- */
    if( psOptions->hDstDS != NULL &&
        psOptions->nDstAlphaBand > 0 &&
        psOptions->nDstAlphaBand <= GDALGetRasterCount(psOptions->hDstDS) &&
        CSLFetchNameValue( psOptions->papszWarpOptions,
                           "DST_ALPHA_MAX" ) == NULL )
    {
        GDALRasterBandH hDstAlphaBand = GDALGetRasterBand(
            psOptions->hDstDS, psOptions->nDstAlphaBand);
        SetAlphaMax( psOptions, hDstAlphaBand, "DST_ALPHA_MAX" );
    }

/* -------------------------------------------------------------------- */
/*      If the options don't validate, then wipe them.                  */
/* -------------------------------------------------------------------- */
    if( !ValidateOptions() )
        eErr = CE_Failure;

    if( eErr != CE_None )
    {
        WipeOptions();
    }
    else
    {
        psThreadData = GWKThreadsCreate(psOptions->papszWarpOptions,
                                        psOptions->pfnTransformer,
                                        psOptions->pTransformerArg);
        if( psThreadData == NULL )
            eErr = CE_Failure;
    }

    return eErr;
}

/**
 * \fn void* GDALWarpOperation::CreateDestinationBuffer(
            int nDstXSize, int nDstYSize, int *pbInitialized);
 *
 * This method creates a destination buffer for use with WarpRegionToBuffer.
 * The output is initialized based on the INIT_DEST settings.
 *
 * @param nDstXSize Width of output window on destination buffer to be produced.
 * @param nDstYSize Height of output window on destination buffer to be produced.
 * @param pbInitialized Filled with boolean indicating if the buffer was initialized.
 *
 * @return Buffer capable for use as a warp operation output destination
 */
void* GDALWarpOperation::CreateDestinationBuffer(
    int nDstXSize, int nDstYSize, int *pbInitialized)
{

/* -------------------------------------------------------------------- */
/*      Allocate block of memory large enough to hold all the bands     */
/*      for this block.                                                 */
/* -------------------------------------------------------------------- */
    const int nWordSize = GDALGetDataTypeSizeBytes(psOptions->eWorkingDataType);
    const int nBandSize = nWordSize * nDstXSize * nDstYSize;

    if( nDstXSize > INT_MAX / nDstYSize ||
        nDstXSize * nDstYSize > INT_MAX / (nWordSize * psOptions->nBandCount) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Integer overflow : nDstXSize=%d, nDstYSize=%d",
                  nDstXSize, nDstYSize);
        return NULL;
    }

    void *pDstBuffer = VSI_MALLOC_VERBOSE( nBandSize * psOptions->nBandCount );
    if( pDstBuffer == NULL )
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Initialize if requested in the options                                 */
/* -------------------------------------------------------------------- */
    const char *pszInitDest = CSLFetchNameValue( psOptions->papszWarpOptions,
                                                 "INIT_DEST" );

    if( pszInitDest == NULL || EQUAL(pszInitDest, "") ) 
    {
        if( pbInitialized != NULL )
        {
            *pbInitialized = FALSE;
        } 
        
        return pDstBuffer; 
    }


    if( pbInitialized != NULL )
    {
        *pbInitialized = TRUE;
    }

    char **papszInitValues =
            CSLTokenizeStringComplex( pszInitDest, ",", FALSE, FALSE );
    const int nInitCount = CSLCount(papszInitValues);

    for( int iBand = 0; iBand < psOptions->nBandCount; iBand++ )
    {
        double adfInitRealImag[2] = { 0.0, 0.0 };
        const char *pszBandInit =
            papszInitValues[std::min(iBand, nInitCount - 1)];

        if( EQUAL(pszBandInit, "NO_DATA")
            && psOptions->padfDstNoDataReal != NULL )
        {
            adfInitRealImag[0] = psOptions->padfDstNoDataReal[iBand];
            if( psOptions->padfDstNoDataImag != NULL )
            {
                adfInitRealImag[1] = psOptions->padfDstNoDataImag[iBand];
            }
        }
        else
        {
            CPLStringToComplex( pszBandInit,
                                adfInitRealImag + 0, adfInitRealImag + 1);
        }

        GByte *pBandData =
            static_cast<GByte *>(pDstBuffer) + iBand * nBandSize;

        if( psOptions->eWorkingDataType == GDT_Byte )
        {
            memset( pBandData,
                    std::max(
                        0, std::min(255,
                                    static_cast<int>(adfInitRealImag[0]))),
                    nBandSize);
        }
        else if( !CPLIsNan(adfInitRealImag[0]) && adfInitRealImag[0] == 0.0 &&
                 !CPLIsNan(adfInitRealImag[1]) && adfInitRealImag[1] == 0.0 )
        {
            memset( pBandData, 0, nBandSize );
        }
        else if( !CPLIsNan(adfInitRealImag[1]) && adfInitRealImag[1] == 0.0 )
        {
            GDALCopyWords( &adfInitRealImag, GDT_Float64, 0,
                            pBandData, psOptions->eWorkingDataType,
                            nWordSize,
                            nDstXSize * nDstYSize );
        }
        else
        {
            GDALCopyWords( &adfInitRealImag, GDT_CFloat64, 0,
                            pBandData, psOptions->eWorkingDataType,
                            nWordSize,
                            nDstXSize * nDstYSize );
        }
    }

    CSLDestroy( papszInitValues );

    return pDstBuffer;
}


/**
 * \fn void GDALWarpOperation::DestroyDestinationBuffer( void *pDstBuffer )
 *
 * This method destroys a buffer previously retrieved from CreateDestinationBuffer
 *
 * @param pDstBuffer destination buffer to be destroyed
 *
 */
void GDALWarpOperation::DestroyDestinationBuffer( void *pDstBuffer )
{
    VSIFree( pDstBuffer );
}

/************************************************************************/
/*                         GDALCreateWarpOperation()                    */
/************************************************************************/

/**
 * @see GDALWarpOperation::Initialize()
 */

GDALWarpOperationH GDALCreateWarpOperation(
    const GDALWarpOptions *psNewOptions )
{
    GDALWarpOperation *poOperation = new GDALWarpOperation;
    if( poOperation->Initialize( psNewOptions ) != CE_None )
    {
        delete poOperation;
        return NULL;
    }

    return reinterpret_cast<GDALWarpOperationH>(poOperation);
}

/************************************************************************/
/*                         GDALDestroyWarpOperation()                   */
/************************************************************************/

/**
 * @see GDALWarpOperation::~GDALWarpOperation()
 */

void GDALDestroyWarpOperation( GDALWarpOperationH hOperation )
{
    if( hOperation )
        delete static_cast<GDALWarpOperation *>(hOperation);
}

/************************************************************************/
/*                         ChunkAndWarpImage()                          */
/************************************************************************/

static int OrderWarpChunk(const void* _a, const void *_b)
{
    const GDALWarpChunk* a = static_cast<const GDALWarpChunk *>(_a);
    const GDALWarpChunk* b = static_cast<const GDALWarpChunk *>(_b);
    if( a->dy < b->dy )
        return -1;
    else if( a->dy > b->dy )
        return 1;
    else if( a->dx < b->dx )
        return -1;
    else if( a->dx > b->dx )
        return 1;
    else
        return 0;
}

/**
 * \fn CPLErr GDALWarpOperation::ChunkAndWarpImage(
                int nDstXOff, int nDstYOff,  int nDstXSize, int nDstYSize );
 *
 * This method does a complete warp of the source image to the destination
 * image for the indicated region with the current warp options in effect.
 * Progress is reported to the installed progress monitor, if any.
 *
 * This function will subdivide the region and recursively call itself
 * until the total memory required to process a region chunk will all fit
 * in the memory pool defined by GDALWarpOptions::dfWarpMemoryLimit.
 *
 * Once an appropriate region is selected GDALWarpOperation::WarpRegion()
 * is invoked to do the actual work.
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpOperation::ChunkAndWarpImage(
    int nDstXOff, int nDstYOff,  int nDstXSize, int nDstYSize )

{
/* -------------------------------------------------------------------- */
/*      Collect the list of chunks to operate on.                       */
/* -------------------------------------------------------------------- */
    WipeChunkList();
    CollectChunkList( nDstXOff, nDstYOff, nDstXSize, nDstYSize );

    // Sort chucks from top to bottom, and for equal y, from left to right.
    // TODO(schwehr): Use std::sort.
    if( pasChunkList )
        qsort(pasChunkList, nChunkListCount, sizeof(GDALWarpChunk),
              OrderWarpChunk);

/* -------------------------------------------------------------------- */
/*      Total up output pixels to process.                              */
/* -------------------------------------------------------------------- */
    double dfTotalPixels = 0.0;

    for( int iChunk = 0;
         pasChunkList != NULL && iChunk < nChunkListCount;
         iChunk++ )
    {
        GDALWarpChunk *pasThisChunk = pasChunkList + iChunk;
        const double dfChunkPixels =
          pasThisChunk->dsx * static_cast<double>(pasThisChunk->dsy);

        dfTotalPixels += dfChunkPixels;
    }

/* -------------------------------------------------------------------- */
/*      Process them one at a time, updating the progress               */
/*      information for each region.                                    */
/* -------------------------------------------------------------------- */
    double dfPixelsProcessed=0.0;

    for( int iChunk = 0;
         pasChunkList != NULL && iChunk < nChunkListCount;
         iChunk++ )
    {
        GDALWarpChunk *pasThisChunk = pasChunkList + iChunk;
        const double dfChunkPixels =
            pasThisChunk->dsx * static_cast<double>(pasThisChunk->dsy);

        const double dfProgressBase = dfPixelsProcessed / dfTotalPixels;
        const double dfProgressScale = dfChunkPixels / dfTotalPixels;

        CPLErr eErr =
            WarpRegion(pasThisChunk->dx, pasThisChunk->dy,
                       pasThisChunk->dsx, pasThisChunk->dsy,
                       pasThisChunk->sx, pasThisChunk->sy,
                       pasThisChunk->ssx, pasThisChunk->ssy,
                       pasThisChunk->sExtraSx, pasThisChunk->sExtraSy,
                       dfProgressBase, dfProgressScale);

        if( eErr != CE_None )
            return eErr;

        dfPixelsProcessed += dfChunkPixels;
    }

    WipeChunkList();

    psOptions->pfnProgress( 1.00001, "", psOptions->pProgressArg );

    return CE_None;
}

/************************************************************************/
/*                         GDALChunkAndWarpImage()                      */
/************************************************************************/

/**
 * @see GDALWarpOperation::ChunkAndWarpImage()
 */

CPLErr GDALChunkAndWarpImage( GDALWarpOperationH hOperation,
    int nDstXOff, int nDstYOff,  int nDstXSize, int nDstYSize )
{
    VALIDATE_POINTER1( hOperation, "GDALChunkAndWarpImage", CE_Failure );

    return reinterpret_cast<GDALWarpOperation *>(hOperation) ->
        ChunkAndWarpImage( nDstXOff, nDstYOff, nDstXSize, nDstYSize );
}

/************************************************************************/
/*                          ChunkThreadMain()                           */
/************************************************************************/

typedef struct
{
    GDALWarpOperation *poOperation;
    GDALWarpChunk     *pasChunkInfo;
    CPLJoinableThread *hThreadHandle;
    CPLErr             eErr;
    double             dfProgressBase;
    double             dfProgressScale;
    CPLMutex          *hIOMutex;

    CPLMutex          *hCondMutex;
    volatile int       bIOMutexTaken;
    CPLCond           *hCond;
} ChunkThreadData;

static void ChunkThreadMain( void *pThreadData )

{
    volatile ChunkThreadData* psData = (volatile ChunkThreadData*) pThreadData;

    GDALWarpChunk *pasChunkInfo = psData->pasChunkInfo;

/* -------------------------------------------------------------------- */
/*      Acquire IO mutex.                                               */
/* -------------------------------------------------------------------- */
    if( !CPLAcquireMutex( psData->hIOMutex, 600.0 ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Failed to acquire IOMutex in WarpRegion()." );
        psData->eErr = CE_Failure;
    }
    else
    {
        if( psData->hCond != NULL )
        {
            CPLAcquireMutex( psData->hCondMutex, 1.0 );
            psData->bIOMutexTaken = TRUE;
            CPLCondSignal(psData->hCond);
            CPLReleaseMutex( psData->hCondMutex );
        }

        psData->eErr = psData->poOperation->WarpRegion(
                                    pasChunkInfo->dx, pasChunkInfo->dy,
                                    pasChunkInfo->dsx, pasChunkInfo->dsy,
                                    pasChunkInfo->sx, pasChunkInfo->sy,
                                    pasChunkInfo->ssx, pasChunkInfo->ssy,
                                    pasChunkInfo->sExtraSx,
                                    pasChunkInfo->sExtraSy,
                                    psData->dfProgressBase,
                                    psData->dfProgressScale);

    /* -------------------------------------------------------------------- */
    /*      Release the IO mutex.                                           */
    /* -------------------------------------------------------------------- */
        CPLReleaseMutex( psData->hIOMutex );
    }
}

/************************************************************************/
/*                         ChunkAndWarpMulti()                          */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpOperation::ChunkAndWarpMulti(
                int nDstXOff, int nDstYOff,  int nDstXSize, int nDstYSize );
 *
 * This method does a complete warp of the source image to the destination
 * image for the indicated region with the current warp options in effect.
 * Progress is reported to the installed progress monitor, if any.
 *
 * Externally this method operates the same as ChunkAndWarpImage(), but
 * internally this method uses multiple threads to interleave input/output
 * for one region while the processing is being done for another.
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpOperation::ChunkAndWarpMulti(
    int nDstXOff, int nDstYOff,  int nDstXSize, int nDstYSize )

{
    hIOMutex = CPLCreateMutex();
    hWarpMutex = CPLCreateMutex();

    CPLReleaseMutex( hIOMutex );
    CPLReleaseMutex( hWarpMutex );

    CPLCond* hCond = CPLCreateCond();
    CPLMutex* hCondMutex = CPLCreateMutex();
    CPLReleaseMutex(hCondMutex);

/* -------------------------------------------------------------------- */
/*      Collect the list of chunks to operate on.                       */
/* -------------------------------------------------------------------- */
    WipeChunkList();
    CollectChunkList( nDstXOff, nDstYOff, nDstXSize, nDstYSize );

    // Sort chucks from top to bottom, and for equal y, from left to right.
    // TODO(schwehr): Use std::sort.
    if( pasChunkList )
        qsort(pasChunkList, nChunkListCount, sizeof(GDALWarpChunk),
              OrderWarpChunk);

/* -------------------------------------------------------------------- */
/*      Process them one at a time, updating the progress               */
/*      information for each region.                                    */
/* -------------------------------------------------------------------- */
    ChunkThreadData volatile asThreadData[2] = {};
    memset((void*)&asThreadData, 0, sizeof(asThreadData));
    asThreadData[0].poOperation = this;
    asThreadData[0].hIOMutex = hIOMutex;
    asThreadData[1].poOperation = this;
    asThreadData[1].hIOMutex = hIOMutex;

    double dfPixelsProcessed = 0.0;
    double dfTotalPixels = nDstXSize*(double)nDstYSize;

    CPLErr eErr = CE_None;
    for( int iChunk = 0; iChunk < nChunkListCount+1; iChunk++ )
    {
        int iThread = iChunk % 2;

/* -------------------------------------------------------------------- */
/*      Launch thread for this chunk.                                   */
/* -------------------------------------------------------------------- */
        if( pasChunkList != NULL && iChunk < nChunkListCount )
        {
            GDALWarpChunk *pasThisChunk = pasChunkList + iChunk;
            const double dfChunkPixels =
                pasThisChunk->dsx * static_cast<double>(pasThisChunk->dsy);

            asThreadData[iThread].dfProgressBase =
                dfPixelsProcessed / dfTotalPixels;
            asThreadData[iThread].dfProgressScale =
                dfChunkPixels / dfTotalPixels;

            dfPixelsProcessed += dfChunkPixels;

            asThreadData[iThread].pasChunkInfo = pasThisChunk;

            if( iChunk == 0 )
            {
                asThreadData[iThread].hCond = hCond;
                asThreadData[iThread].hCondMutex = hCondMutex;
            }
            else
            {
                asThreadData[iThread].hCond = NULL;
                asThreadData[iThread].hCondMutex = NULL;
            }
            asThreadData[iThread].bIOMutexTaken = FALSE;

            CPLDebug( "GDAL", "Start chunk %d.", iChunk );
            asThreadData[iThread].hThreadHandle = CPLCreateJoinableThread(
                ChunkThreadMain,
                const_cast<ChunkThreadData *>(&asThreadData[iThread]));
            if( asThreadData[iThread].hThreadHandle == NULL )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "CPLCreateJoinableThread() failed in ChunkAndWarpMulti()");
                eErr = CE_Failure;
                break;
            }

            // Wait that the first thread has acquired the IO mutex before
            // proceeding.  This will ensure that the first thread will run
            // before the second one.
            if( iChunk == 0 )
            {
                CPLAcquireMutex(hCondMutex, 1.0);
                while( asThreadData[iThread].bIOMutexTaken == FALSE )
                    CPLCondWait(hCond, hCondMutex);
                CPLReleaseMutex(hCondMutex);
            }
        }

/* -------------------------------------------------------------------- */
/*      Wait for previous chunks thread to complete.                    */
/* -------------------------------------------------------------------- */
        if( iChunk > 0 )
        {
            iThread = (iChunk-1) % 2;

            // Wait for thread to finish.
            CPLJoinThread(asThreadData[iThread].hThreadHandle);
            asThreadData[iThread].hThreadHandle = NULL;

            CPLDebug( "GDAL", "Finished chunk %d.", iChunk-1 );

            eErr = asThreadData[iThread].eErr;

            if( eErr != CE_None )
                break;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Wait for all threads to complete.                               */
    /* -------------------------------------------------------------------- */
    for( int iThread = 0; iThread < 2; iThread++ )
    {
        if( asThreadData[iThread].hThreadHandle )
            CPLJoinThread(asThreadData[iThread].hThreadHandle);
    }

    CPLDestroyCond(hCond);
    CPLDestroyMutex(hCondMutex);

    WipeChunkList();

    return eErr;
}

/************************************************************************/
/*                         GDALChunkAndWarpMulti()                      */
/************************************************************************/

/**
 * @see GDALWarpOperation::ChunkAndWarpMulti()
 */

CPLErr GDALChunkAndWarpMulti( GDALWarpOperationH hOperation,
    int nDstXOff, int nDstYOff,  int nDstXSize, int nDstYSize )
{
    VALIDATE_POINTER1( hOperation, "GDALChunkAndWarpMulti", CE_Failure );

    return reinterpret_cast<GDALWarpOperation *>(hOperation)->
        ChunkAndWarpMulti( nDstXOff, nDstYOff, nDstXSize, nDstYSize );
}

/************************************************************************/
/*                           WipeChunkList()                            */
/************************************************************************/

void GDALWarpOperation::WipeChunkList()

{
    CPLFree( pasChunkList );
    pasChunkList = NULL;
    nChunkListCount = 0;
    nChunkListMax = 0;
}

/************************************************************************/
/*                          CollectChunkList()                          */
/************************************************************************/

CPLErr GDALWarpOperation::CollectChunkList(
    int nDstXOff, int nDstYOff,  int nDstXSize, int nDstYSize )

{
/* -------------------------------------------------------------------- */
/*      Compute the bounds of the input area corresponding to the       */
/*      output area.                                                    */
/* -------------------------------------------------------------------- */
    int nSrcXOff = 0;
    int nSrcYOff = 0;
    int nSrcXSize = 0;
    int nSrcYSize = 0;
    int nSrcXExtraSize = 0;
    int nSrcYExtraSize = 0;
    double dfSrcFillRatio = 0.0;
    CPLErr eErr =
        ComputeSourceWindow(nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                            &nSrcXOff, &nSrcYOff, &nSrcXSize, &nSrcYSize,
                            &nSrcXExtraSize, &nSrcYExtraSize, &dfSrcFillRatio);

    if( eErr != CE_None )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Unable to compute source region for "
                 "output window %d,%d,%d,%d, skipping.",
                 nDstXOff, nDstYOff, nDstXSize, nDstYSize);
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      If we are allowed to drop no-source regions, do so now if       */
/*      appropriate.                                                    */
/* -------------------------------------------------------------------- */
    if( (nSrcXSize == 0 || nSrcYSize == 0)
        && CPLFetchBool( psOptions->papszWarpOptions, "SKIP_NOSOURCE", false ))
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Based on the types of masks in use, how many bits will each     */
/*      source pixel cost us?                                           */
/* -------------------------------------------------------------------- */
    int nSrcPixelCostInBits =
        GDALGetDataTypeSize( psOptions->eWorkingDataType )
        * psOptions->nBandCount;

    if( psOptions->pfnSrcDensityMaskFunc != NULL )
        nSrcPixelCostInBits += 32;  // Float mask?

    GDALRasterBandH hSrcBand = NULL;
    if( psOptions->nBandCount > 0 )
        hSrcBand = GDALGetRasterBand(psOptions->hSrcDS,
                                     psOptions->panSrcBands[0]);

    if( psOptions->nSrcAlphaBand > 0 || psOptions->hCutline != NULL )
        nSrcPixelCostInBits += 32;  // UnifiedSrcDensity float mask.
    else if( hSrcBand != NULL &&
             (GDALGetMaskFlags(hSrcBand) & GMF_PER_DATASET) )
        nSrcPixelCostInBits += 1;  // UnifiedSrcValid bit mask.

    if( psOptions->papfnSrcPerBandValidityMaskFunc != NULL
        || psOptions->padfSrcNoDataReal != NULL )
        nSrcPixelCostInBits += psOptions->nBandCount;  // Bit/band mask.

    if( psOptions->pfnSrcValidityMaskFunc != NULL )
        nSrcPixelCostInBits += 1;  // Bit mask.

/* -------------------------------------------------------------------- */
/*      What about the cost for the destination.                        */
/* -------------------------------------------------------------------- */
    int nDstPixelCostInBits =
        GDALGetDataTypeSize( psOptions->eWorkingDataType )
        * psOptions->nBandCount;

    if( psOptions->pfnDstDensityMaskFunc != NULL )
        nDstPixelCostInBits += 32;

    if( psOptions->padfDstNoDataReal != NULL
        || psOptions->pfnDstValidityMaskFunc != NULL )
        nDstPixelCostInBits += psOptions->nBandCount;

    if( psOptions->nDstAlphaBand > 0 )
        nDstPixelCostInBits += 32;  // DstDensity float mask.

/* -------------------------------------------------------------------- */
/*      Does the cost of the current rectangle exceed our memory        */
/*      limit? If so, split the destination along the longest           */
/*      dimension and recurse.                                          */
/* -------------------------------------------------------------------- */
    double dfTotalMemoryUse =
      (static_cast<double>(nSrcPixelCostInBits) * nSrcXSize * nSrcYSize +
       static_cast<double>(nDstPixelCostInBits) * nDstXSize * nDstYSize) / 8.0;

    int nBlockXSize = 1;
    int nBlockYSize = 1;
    if( psOptions->hDstDS )
    {
        GDALGetBlockSize(GDALGetRasterBand(psOptions->hDstDS, 1),
                         &nBlockXSize, &nBlockYSize);
    }

    // If size of working buffers need exceed the allow limit, then divide
    // the target area
    // Do it also if the "fill ratio" of the source is too low (#3120), but
    // only if there's at least some source pixel intersecting. The
    // SRC_FILL_RATIO_HEURISTICS warping option is undocumented and only here
    // in case the heuristics would cause issues.
#if DEBUG_VERBOSE
    CPLDebug("WARP",
             "dst=(%d,%d,%d,%d) src=(%d,%d,%d,%d) srcfillratio=%.18g",
             nDstXOff, nDstYOff, nDstXSize, nDstYSize,
             nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize, dfSrcFillRatio);
#endif
    if( (dfTotalMemoryUse > psOptions->dfWarpMemoryLimit &&
         (nDstXSize > 2 || nDstYSize > 2)) ||
        (dfSrcFillRatio > 0 && dfSrcFillRatio < 0.5 &&
         (nDstXSize > 100 || nDstYSize > 100) &&
         CPLFetchBool( psOptions->papszWarpOptions, "SRC_FILL_RATIO_HEURISTICS",
                       true )) )
    {
        int bStreamableOutput =
            CPLFetchBool( psOptions->papszWarpOptions, "STREAMABLE_OUTPUT",
                          false );
        const bool bOptimizeSize =
            !bStreamableOutput &&
            CPLFetchBool( psOptions->papszWarpOptions, "OPTIMIZE_SIZE", false );

        // If the region width is greater than the region height,
        // cut in half in the width. When we want to optimize the size
        // of a compressed output dataset, do this only if each half part
        // is at least as wide as the block width.
        bool bHasDivided = false;
        CPLErr eErr2 = CE_None;
        if( nDstXSize > nDstYSize &&
            ((!bOptimizeSize && !bStreamableOutput) ||
             (bOptimizeSize &&
              (nDstXSize / 2 >= nBlockXSize || nDstYSize == 1)) ||
             (bStreamableOutput &&
              nDstXSize / 2 >= nBlockXSize &&
              nDstYSize == nBlockYSize)) )
        {
            bHasDivided = true;
            int nChunk1 = nDstXSize / 2;

            // In the optimize size case, try to stick on target block
            // boundaries.
            if( (bOptimizeSize || bStreamableOutput) && nChunk1 > nBlockXSize )
                nChunk1 = (nChunk1 / nBlockXSize) * nBlockXSize;

            int nChunk2 = nDstXSize - nChunk1;

            eErr = CollectChunkList( nDstXOff, nDstYOff,
                                     nChunk1, nDstYSize );

            eErr2 = CollectChunkList( nDstXOff+nChunk1, nDstYOff,
                                      nChunk2, nDstYSize );
        }
        else if( !(bStreamableOutput && nDstYSize / 2 < nBlockYSize) )
        {
            bHasDivided = true;
            int nChunk1 = nDstYSize / 2;

            // In the optimize size case, try to stick on target block
            // boundaries.
            if( (bOptimizeSize || bStreamableOutput) && nChunk1 > nBlockYSize )
                nChunk1 = (nChunk1 / nBlockYSize) * nBlockYSize;

            const int nChunk2 = nDstYSize - nChunk1;

            eErr = CollectChunkList( nDstXOff, nDstYOff,
                                     nDstXSize, nChunk1 );

            eErr2 = CollectChunkList( nDstXOff, nDstYOff+nChunk1,
                                      nDstXSize, nChunk2 );
        }

        if( bHasDivided )
        {
            if( eErr == CE_None )
                return eErr2;
            else
                return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      OK, everything fits, so add to the chunk list.                  */
/* -------------------------------------------------------------------- */
    if( nChunkListCount == nChunkListMax )
    {
        nChunkListMax = nChunkListMax * 2 + 1;
        pasChunkList = static_cast<GDALWarpChunk *>(
            CPLRealloc(pasChunkList, sizeof(GDALWarpChunk) * nChunkListMax));
    }

    pasChunkList[nChunkListCount].dx = nDstXOff;
    pasChunkList[nChunkListCount].dy = nDstYOff;
    pasChunkList[nChunkListCount].dsx = nDstXSize;
    pasChunkList[nChunkListCount].dsy = nDstYSize;
    pasChunkList[nChunkListCount].sx = nSrcXOff;
    pasChunkList[nChunkListCount].sy = nSrcYOff;
    pasChunkList[nChunkListCount].ssx = nSrcXSize;
    pasChunkList[nChunkListCount].ssy = nSrcYSize;
    pasChunkList[nChunkListCount].sExtraSx = nSrcXExtraSize;
    pasChunkList[nChunkListCount].sExtraSy = nSrcYExtraSize;

    nChunkListCount++;

    return CE_None;
}

/************************************************************************/
/*                             WarpRegion()                             */
/************************************************************************/

/**
 * This method requests the indicated region of the output file be generated.
 *
 * Note that WarpRegion() will produce the requested area in one low level warp
 * operation without verifying that this does not exceed the stated memory
 * limits for the warp operation.  Applications should take care not to call
 * WarpRegion() on too large a region!  This function
 * is normally called by ChunkAndWarpImage(), the normal entry point for
 * applications.  Use it instead if staying within memory constraints is
 * desired.
 *
 * Progress is reported from dfProgressBase to dfProgressBase + dfProgressScale
 * for the indicated region.
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 * @param nSrcXOff source window X offset (computed if window all zero)
 * @param nSrcYOff source window Y offset (computed if window all zero)
 * @param nSrcXSize source window X size (computed if window all zero)
 * @param nSrcYSize source window Y size (computed if window all zero)
 * @param dfProgressBase minimum progress value reported
 * @param dfProgressScale value such as dfProgressBase + dfProgressScale is the
 *                        maximum progress value reported
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpOperation::WarpRegion( int nDstXOff, int nDstYOff,
                                      int nDstXSize, int nDstYSize,
                                      int nSrcXOff, int nSrcYOff,
                                      int nSrcXSize, int nSrcYSize,
                                      double dfProgressBase,
                                      double dfProgressScale )
{
    return WarpRegion(nDstXOff, nDstYOff,
                      nDstXSize, nDstYSize,
                      nSrcXOff, nSrcYOff,
                      nSrcXSize, nSrcYSize,
                      0, 0,
                      dfProgressBase, dfProgressScale);
}

/**
 * This method requests the indicated region of the output file be generated.
 *
 * Note that WarpRegion() will produce the requested area in one low level warp
 * operation without verifying that this does not exceed the stated memory
 * limits for the warp operation.  Applications should take care not to call
 * WarpRegion() on too large a region!  This function
 * is normally called by ChunkAndWarpImage(), the normal entry point for
 * applications.  Use it instead if staying within memory constraints is
 * desired.
 *
 * Progress is reported from dfProgressBase to dfProgressBase + dfProgressScale
 * for the indicated region.
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 * @param nSrcXOff source window X offset (computed if window all zero)
 * @param nSrcYOff source window Y offset (computed if window all zero)
 * @param nSrcXSize source window X size (computed if window all zero)
 * @param nSrcYSize source window Y size (computed if window all zero)
 * @param nSrcXExtraSize Extra pixels (included in nSrcXSize) reserved
 * for filter window. Should be ignored in scale computation
 * @param nSrcYExtraSize Extra pixels (included in nSrcYSize) reserved
 * for filter window. Should be ignored in scale computation
 * @param dfProgressBase minimum progress value reported
 * @param dfProgressScale value such as dfProgressBase + dfProgressScale is the
 *                        maximum progress value reported
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpOperation::WarpRegion( int nDstXOff, int nDstYOff,
                                      int nDstXSize, int nDstYSize,
                                      int nSrcXOff, int nSrcYOff,
                                      int nSrcXSize, int nSrcYSize,
                                      int nSrcXExtraSize, int nSrcYExtraSize,
                                      double dfProgressBase,
                                      double dfProgressScale)

{
    ReportTiming( NULL );

/* -------------------------------------------------------------------- */
/*      Allocate the output buffer.                                     */
/* -------------------------------------------------------------------- */
    int bDstBufferInitialized = FALSE;
    void *pDstBuffer = CreateDestinationBuffer(nDstXSize, nDstYSize, &bDstBufferInitialized); 
    if( pDstBuffer == NULL )
    {
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      If we aren't doing fixed initialization of the output buffer    */
/*      then read it from disk so we can overlay on existing imagery.   */
/* -------------------------------------------------------------------- */
    GDALDataset* poDstDS = reinterpret_cast<GDALDataset*>(psOptions->hDstDS);
    if( !bDstBufferInitialized )
    {
        CPLErr eErr = CE_None;
        if( psOptions->nBandCount == 1 )
        {
            // Particular case to simplify the stack a bit.
            // TODO(rouault): Need an explanation of what and why r34502 helps.
            eErr = poDstDS->GetRasterBand(psOptions->panDstBands[0])->RasterIO(
                GF_Read,
                nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                pDstBuffer, nDstXSize, nDstYSize,
                psOptions->eWorkingDataType,
                0, 0, NULL);
        }
        else
        {
            eErr = poDstDS->RasterIO(
                GF_Read,
                nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                pDstBuffer, nDstXSize, nDstYSize,
                psOptions->eWorkingDataType,
                psOptions->nBandCount,
                psOptions->panDstBands,
                0, 0, 0, NULL);
        }

        if( eErr != CE_None )
        {
            DestroyDestinationBuffer(pDstBuffer);
            return eErr;
        }

        ReportTiming( "Output buffer read" );
    }

/* -------------------------------------------------------------------- */
/*      Perform the warp.                                               */
/* -------------------------------------------------------------------- */
    CPLErr eErr =
        WarpRegionToBuffer(nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                           pDstBuffer, psOptions->eWorkingDataType,
                           nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                           nSrcXExtraSize, nSrcYExtraSize,
                           dfProgressBase, dfProgressScale);

/* -------------------------------------------------------------------- */
/*      Write the output data back to disk if all went well.            */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
    {
        if( psOptions->nBandCount == 1 )
        {
            // Particular case to simplify the stack a bit.
            eErr = poDstDS->GetRasterBand(psOptions->panDstBands[0])->RasterIO(
                  GF_Write,
                  nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                  pDstBuffer, nDstXSize, nDstYSize,
                  psOptions->eWorkingDataType,
                  0, 0, NULL );
        }
        else
        {
            eErr = poDstDS->RasterIO( GF_Write,
                                    nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                    pDstBuffer, nDstXSize, nDstYSize,
                                    psOptions->eWorkingDataType,
                                    psOptions->nBandCount,
                                    psOptions->panDstBands,
                                    0, 0, 0, NULL );
        }

        if( eErr == CE_None &&
            CPLFetchBool( psOptions->papszWarpOptions, "WRITE_FLUSH", false ) )
        {
            const CPLErr eOldErr = CPLGetLastErrorType();
            const CPLString osLastErrMsg = CPLGetLastErrorMsg();
            GDALFlushCache( psOptions->hDstDS );
            const CPLErr eNewErr = CPLGetLastErrorType();
            if( eNewErr != eOldErr ||
                osLastErrMsg.compare(CPLGetLastErrorMsg()) != 0 )
                eErr = CE_Failure;
        }
        ReportTiming( "Output buffer write" );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup and return.                                             */
/* -------------------------------------------------------------------- */
    DestroyDestinationBuffer( pDstBuffer );

    return eErr;
}

/************************************************************************/
/*                             GDALWarpRegion()                         */
/************************************************************************/

/**
 * @see GDALWarpOperation::WarpRegion()
 */

CPLErr GDALWarpRegion( GDALWarpOperationH hOperation,
                       int nDstXOff, int nDstYOff,
                       int nDstXSize, int nDstYSize,
                       int nSrcXOff, int nSrcYOff,
                       int nSrcXSize, int nSrcYSize )

{
    VALIDATE_POINTER1( hOperation, "GDALWarpRegion", CE_Failure );

    return reinterpret_cast<GDALWarpOperation *>(hOperation)->
        WarpRegion( nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                    nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize);
}

/************************************************************************/
/*                            WarpRegionToBuffer()                      */
/************************************************************************/

/**
 * This method requests that a particular window of the output dataset
 * be warped and the result put into the provided data buffer.  The output
 * dataset doesn't even really have to exist to use this method as long as
 * the transformation function in the GDALWarpOptions is setup to map to
 * a virtual pixel/line space.
 *
 * This method will do the whole region in one chunk, so be wary of the
 * amount of memory that might be used.
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 * @param pDataBuf the data buffer to place result in, of type eBufDataType.
 * @param eBufDataType the type of the output data buffer.  For now this
 * must match GDALWarpOptions::eWorkingDataType.
 * @param nSrcXOff source window X offset (computed if window all zero)
 * @param nSrcYOff source window Y offset (computed if window all zero)
 * @param nSrcXSize source window X size (computed if window all zero)
 * @param nSrcYSize source window Y size (computed if window all zero)
 * @param dfProgressBase minimum progress value reported
 * @param dfProgressScale value such as dfProgressBase + dfProgressScale is the
 *                        maximum progress value reported
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpOperation::WarpRegionToBuffer(
    int nDstXOff, int nDstYOff, int nDstXSize, int nDstYSize,
    void *pDataBuf, GDALDataType eBufDataType,
    int nSrcXOff, int nSrcYOff, int nSrcXSize, int nSrcYSize,
    double dfProgressBase, double dfProgressScale)
{
    return WarpRegionToBuffer(nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                              pDataBuf, eBufDataType,
                              nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize, 0, 0,
                              dfProgressBase, dfProgressScale);
}

/**
 * This method requests that a particular window of the output dataset
 * be warped and the result put into the provided data buffer.  The output
 * dataset doesn't even really have to exist to use this method as long as
 * the transformation function in the GDALWarpOptions is setup to map to
 * a virtual pixel/line space.
 *
 * This method will do the whole region in one chunk, so be wary of the
 * amount of memory that might be used.
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 * @param pDataBuf the data buffer to place result in, of type eBufDataType.
 * @param eBufDataType the type of the output data buffer.  For now this
 * must match GDALWarpOptions::eWorkingDataType.
 * @param nSrcXOff source window X offset (computed if window all zero)
 * @param nSrcYOff source window Y offset (computed if window all zero)
 * @param nSrcXSize source window X size (computed if window all zero)
 * @param nSrcYSize source window Y size (computed if window all zero)
 * @param nSrcXExtraSize Extra pixels (included in nSrcXSize) reserved
 * for filter window. Should be ignored in scale computation
 * @param nSrcYExtraSize Extra pixels (included in nSrcYSize) reserved
 * for filter window. Should be ignored in scale computation
 * @param dfProgressBase minimum progress value reported
 * @param dfProgressScale value such as dfProgressBase + dfProgressScale is the
 *                        maximum progress value reported
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr GDALWarpOperation::WarpRegionToBuffer(
    int nDstXOff, int nDstYOff, int nDstXSize, int nDstYSize,
    void *pDataBuf,
    // Only in a CPLAssert.
    CPL_UNUSED GDALDataType eBufDataType,
    int nSrcXOff, int nSrcYOff, int nSrcXSize, int nSrcYSize,
    int nSrcXExtraSize, int nSrcYExtraSize,
    double dfProgressBase, double dfProgressScale)

{
    const int nWordSize = GDALGetDataTypeSizeBytes(psOptions->eWorkingDataType);

    CPLAssert( eBufDataType == psOptions->eWorkingDataType );

/* -------------------------------------------------------------------- */
/*      If not given a corresponding source window compute one now.     */
/* -------------------------------------------------------------------- */
    if( nSrcXSize == 0 && nSrcYSize == 0 )
    {
        // TODO: This taking of the warp mutex is suboptimal. We could get rid
        // of it, but that would require making sure ComputeSourceWindow()
        // uses a different pTransformerArg than the warp kernel.
        if( hWarpMutex != NULL && !CPLAcquireMutex( hWarpMutex, 600.0 ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to acquire WarpMutex in WarpRegion()." );
            return CE_Failure;
        }
        const CPLErr eErr =
            ComputeSourceWindow( nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                                 &nSrcXOff, &nSrcYOff,
                                 &nSrcXSize, &nSrcYSize,
                                 &nSrcXExtraSize, &nSrcYExtraSize, NULL );
        if( hWarpMutex != NULL )
            CPLReleaseMutex( hWarpMutex );
        if( eErr != CE_None )
            return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Prepare a WarpKernel object to match this operation.            */
/* -------------------------------------------------------------------- */
    GDALWarpKernel oWK;

    oWK.eResample = psOptions->eResampleAlg;
    oWK.nBands = psOptions->nBandCount;
    oWK.eWorkingDataType = psOptions->eWorkingDataType;

    oWK.pfnTransformer = psOptions->pfnTransformer;
    oWK.pTransformerArg = psOptions->pTransformerArg;

    oWK.pfnProgress = psOptions->pfnProgress;
    oWK.pProgress = psOptions->pProgressArg;
    oWK.dfProgressBase = dfProgressBase;
    oWK.dfProgressScale = dfProgressScale;

    oWK.papszWarpOptions = psOptions->papszWarpOptions;
    oWK.psThreadData = psThreadData;

    oWK.padfDstNoDataReal = psOptions->padfDstNoDataReal;

/* -------------------------------------------------------------------- */
/*      Setup the source buffer.                                        */
/*                                                                      */
/*      Eventually we may need to take advantage of pixel               */
/*      interleaved reading here.                                       */
/* -------------------------------------------------------------------- */
    oWK.nSrcXOff = nSrcXOff;
    oWK.nSrcYOff = nSrcYOff;
    oWK.nSrcXSize = nSrcXSize;
    oWK.nSrcYSize = nSrcYSize;
    oWK.nSrcXExtraSize = nSrcXExtraSize;
    oWK.nSrcYExtraSize = nSrcYExtraSize;

    if( nSrcXSize != 0 && nSrcYSize != 0 &&
        (nSrcXSize > INT_MAX / nSrcYSize ||
         nSrcXSize * nSrcYSize >
         INT_MAX / (nWordSize * psOptions->nBandCount) - WARP_EXTRA_ELTS) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Integer overflow : nSrcXSize=%d, nSrcYSize=%d",
                  nSrcXSize, nSrcYSize);
        return CE_Failure;
    }

    oWK.papabySrcImage = static_cast<GByte **>(
        CPLCalloc(sizeof(GByte*), psOptions->nBandCount));
    oWK.papabySrcImage[0] = static_cast<GByte *>(
        VSI_MALLOC_VERBOSE(nWordSize * (nSrcXSize * nSrcYSize + WARP_EXTRA_ELTS)
                           * psOptions->nBandCount));

    CPLErr eErr =
        nSrcXSize != 0 && nSrcYSize != 0 && oWK.papabySrcImage[0] == NULL
        ? CE_Failure
        : CE_None;


    for( int i = 0; i < psOptions->nBandCount && eErr == CE_None; i++ )
        oWK.papabySrcImage[i] = ((GByte *) oWK.papabySrcImage[0])
            + nWordSize * (nSrcXSize * nSrcYSize + WARP_EXTRA_ELTS) * i;

    if( eErr == CE_None && nSrcXSize > 0 && nSrcYSize > 0 )
    {
        GDALDataset* poSrcDS =
            reinterpret_cast<GDALDataset*>(psOptions->hSrcDS);
        if( psOptions->nBandCount == 1 )
        {
            // Particular case to simplify the stack a bit.
            eErr = poSrcDS->GetRasterBand(psOptions->panDstBands[0])->RasterIO(
                                  GF_Read,
                                  nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                                  oWK.papabySrcImage[0], nSrcXSize, nSrcYSize,
                                  psOptions->eWorkingDataType,
                                  0, 0, NULL );
        }
        else
        {
            eErr = poSrcDS->RasterIO( GF_Read,
                  nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize,
                  oWK.papabySrcImage[0], nSrcXSize, nSrcYSize,
                  psOptions->eWorkingDataType,
                  psOptions->nBandCount, psOptions->panSrcBands,
                  0, 0, nWordSize * (nSrcXSize * nSrcYSize + WARP_EXTRA_ELTS),
                  NULL );
        }
    }

    ReportTiming( "Input buffer read" );

/* -------------------------------------------------------------------- */
/*      Initialize destination buffer.                                  */
/* -------------------------------------------------------------------- */
    oWK.nDstXOff = nDstXOff;
    oWK.nDstYOff = nDstYOff;
    oWK.nDstXSize = nDstXSize;
    oWK.nDstYSize = nDstYSize;

    oWK.papabyDstImage = reinterpret_cast<GByte **>(
        CPLCalloc(sizeof(GByte*), psOptions->nBandCount));

    int i1 = 0;  // Used after for.
    for( ; i1 < psOptions->nBandCount && eErr == CE_None; i1++ )
    {
        oWK.papabyDstImage[i1] = static_cast<GByte *>(pDataBuf)
            + i1 * nDstXSize * nDstYSize * nWordSize;
    }

/* -------------------------------------------------------------------- */
/*      Eventually we need handling for a whole bunch of the            */
/*      validity and density masks here.                                */
/* -------------------------------------------------------------------- */

    // TODO

/* -------------------------------------------------------------------- */
/*      Generate a source density mask if we have a source alpha band   */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psOptions->nSrcAlphaBand > 0 &&
        nSrcXSize > 0 && nSrcYSize > 0 )
    {
        CPLAssert( oWK.pafUnifiedSrcDensity == NULL );

        eErr = CreateKernelMask( &oWK, 0, "UnifiedSrcDensity" );

        if( eErr == CE_None )
        {
            int bOutAllOpaque = FALSE;
            eErr =
                GDALWarpSrcAlphaMasker( psOptions,
                                        psOptions->nBandCount,
                                        psOptions->eWorkingDataType,
                                        oWK.nSrcXOff, oWK.nSrcYOff,
                                        oWK.nSrcXSize, oWK.nSrcYSize,
                                        oWK.papabySrcImage,
                                        TRUE, oWK.pafUnifiedSrcDensity,
                                        &bOutAllOpaque );
            if( bOutAllOpaque )
            {
#if DEBUG_VERBOSE
                CPLDebug("WARP",
                         "No need for a source density mask as all values "
                         "are opaque");
#endif
                CPLFree(oWK.pafUnifiedSrcDensity);
                oWK.pafUnifiedSrcDensity = NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Generate a source density mask if we have a source cutline.     */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psOptions->hCutline != NULL  &&
        nSrcXSize > 0 && nSrcYSize > 0 )
    {
        if( oWK.pafUnifiedSrcDensity == NULL )
        {
            eErr = CreateKernelMask( &oWK, 0, "UnifiedSrcDensity" );

            if( eErr == CE_None )
            {
                for( int j = 0; j < oWK.nSrcXSize * oWK.nSrcYSize; j++ )
                    oWK.pafUnifiedSrcDensity[j] = 1.0;
            }
        }

        if( eErr == CE_None )
            eErr =
                GDALWarpCutlineMasker( psOptions,
                                       psOptions->nBandCount,
                                       psOptions->eWorkingDataType,
                                       oWK.nSrcXOff, oWK.nSrcYOff,
                                       oWK.nSrcXSize, oWK.nSrcYSize,
                                       oWK.papabySrcImage,
                                       TRUE, oWK.pafUnifiedSrcDensity );
    }

/* -------------------------------------------------------------------- */
/*      Generate a destination density mask if we have a destination    */
/*      alpha band.                                                     */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psOptions->nDstAlphaBand > 0 )
    {
        CPLAssert( oWK.pafDstDensity == NULL );

        eErr = CreateKernelMask( &oWK, i1, "DstDensity" );

        if( eErr == CE_None )
            eErr =
                GDALWarpDstAlphaMasker( psOptions,
                                        psOptions->nBandCount,
                                        psOptions->eWorkingDataType,
                                        oWK.nDstXOff, oWK.nDstYOff,
                                        oWK.nDstXSize, oWK.nDstYSize,
                                        oWK.papabyDstImage,
                                        TRUE, oWK.pafDstDensity );
    }

/* -------------------------------------------------------------------- */
/*      If we have source nodata values create the validity mask.       */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psOptions->padfSrcNoDataReal != NULL &&
        nSrcXSize > 0 && nSrcYSize > 0 )
    {
        CPLAssert( oWK.papanBandSrcValid == NULL );

        bool bAllBandsAllValid = true;
        int i2 = 0;  // Used after for.
        for( ; i2 < psOptions->nBandCount && eErr == CE_None; i2++ )
        {
            eErr = CreateKernelMask( &oWK, i2, "BandSrcValid" );
            if( eErr == CE_None )
            {
                double adfNoData[2] =
                {
                    psOptions->padfSrcNoDataReal[i2],
                    psOptions->padfSrcNoDataImag != NULL ? psOptions->padfSrcNoDataImag[i2] : 0.0
                };

                int bAllValid = FALSE;
                eErr =
                    GDALWarpNoDataMasker( adfNoData, 1,
                                          psOptions->eWorkingDataType,
                                          oWK.nSrcXOff, oWK.nSrcYOff,
                                          oWK.nSrcXSize, oWK.nSrcYSize,
                                          &(oWK.papabySrcImage[i2]),
                                          FALSE, oWK.papanBandSrcValid[i2],
                                          &bAllValid );
                if( !bAllValid )
                    bAllBandsAllValid = false;
            }
        }

        // Optimization: if all pixels in all bands are valid,
        // we don't need a mask.
        if( bAllBandsAllValid )
        {
#if DEBUG_VERBOSE
            CPLDebug(
                "WARP",
                "No need for a source nodata mask as all values are valid");
#endif
            for( int k = 0; k < oWK.nBands; k++ )
                CPLFree( oWK.papanBandSrcValid[k] );
            CPLFree( oWK.papanBandSrcValid );
            oWK.papanBandSrcValid = NULL;
        }


/* -------------------------------------------------------------------- */
/*      If there's just a single band, then transfer                    */
/*      papanBandSrcValid[0] as panUnifiedSrcValid.                     */
/* -------------------------------------------------------------------- */
        if( oWK.papanBandSrcValid != NULL && psOptions->nBandCount == 1 )
        {
            oWK.panUnifiedSrcValid = oWK.papanBandSrcValid[0];
            CPLFree( oWK.papanBandSrcValid );
            oWK.papanBandSrcValid = NULL;
        }

/* -------------------------------------------------------------------- */
/*      If UNIFIED_SRC_NODATA is set, then compute a unified input      */
/*      pixel mask if and only if all bands nodata is true.  That       */
/*      is, we only treat a pixel as nodata if all bands match their    */
/*      respective nodata values.                                       */
/* -------------------------------------------------------------------- */
        else if( oWK.papanBandSrcValid != NULL &&
            CPLFetchBool( psOptions->papszWarpOptions, "UNIFIED_SRC_NODATA",
                          false )
            && eErr == CE_None )
        {
            const int nBytesInMask = (oWK.nSrcXSize * oWK.nSrcYSize + 31) / 8;

            eErr = CreateKernelMask( &oWK, i2, "UnifiedSrcValid" );

            if( eErr == CE_None )
            {
                memset( oWK.panUnifiedSrcValid, 0, nBytesInMask );

                for( int k = 0; k < psOptions->nBandCount; k++ )
                {
                    for( int iWord = nBytesInMask/4 - 1; iWord >= 0; iWord-- )
                        oWK.panUnifiedSrcValid[iWord] |=
                            oWK.papanBandSrcValid[k][iWord];
                    CPLFree( oWK.papanBandSrcValid[k] );
                    oWK.papanBandSrcValid[k] = NULL;
                }

                CPLFree( oWK.papanBandSrcValid );
                oWK.papanBandSrcValid = NULL;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Generate a source validity mask if we have a source mask for    */
/*      the whole input dataset (and didn't already treat it as         */
/*      alpha band).                                                    */
/* -------------------------------------------------------------------- */
    GDALRasterBandH hSrcBand =
        psOptions->nBandCount < 1
        ? NULL
        : GDALGetRasterBand(psOptions->hSrcDS, psOptions->panSrcBands[0]);

    if( eErr == CE_None
        && oWK.pafUnifiedSrcDensity == NULL
        && oWK.panUnifiedSrcValid == NULL
        && psOptions->nSrcAlphaBand <= 0
        && (GDALGetMaskFlags(hSrcBand) & GMF_PER_DATASET)
        // Need to double check for -nosrcalpha case.
        && !(GDALGetMaskFlags(hSrcBand) & GMF_ALPHA)
        && nSrcXSize > 0 && nSrcYSize > 0 )

    {
        eErr = CreateKernelMask( &oWK, 0, "UnifiedSrcValid" );

        if( eErr == CE_None )
            eErr =
                GDALWarpSrcMaskMasker( psOptions,
                                       psOptions->nBandCount,
                                       psOptions->eWorkingDataType,
                                       oWK.nSrcXOff, oWK.nSrcYOff,
                                       oWK.nSrcXSize, oWK.nSrcYSize,
                                       oWK.papabySrcImage,
                                       FALSE, oWK.panUnifiedSrcValid );
    }

/* -------------------------------------------------------------------- */
/*      If we have destination nodata values create the                 */
/*      validity mask.  We set the DstValid for any pixel that we       */
/*      do no have valid data in *any* of the source bands.             */
/*                                                                      */
/*      Note that we don't support any concept of unified nodata on     */
/*      the destination image.  At some point that should be added      */
/*      and then this logic will be significantly different.            */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psOptions->padfDstNoDataReal != NULL )
    {
        CPLAssert( oWK.panDstValid == NULL );

        const int nMaskWords = (oWK.nDstXSize * oWK.nDstYSize + 31)/32;

        eErr = CreateKernelMask( &oWK, 0, "DstValid" );
        GUInt32 *panBandMask =
            eErr == CE_None
            ? static_cast<GUInt32 *>(CPLMalloc(nMaskWords * 4))
            : NULL;

        if( eErr == CE_None && panBandMask != NULL )
        {
            for( int iBand = 0; iBand < psOptions->nBandCount; iBand++ )
            {
                memset( panBandMask, 0xff, nMaskWords * 4 );

                double adfNoData[2] =
                {
                    psOptions->padfDstNoDataReal[iBand],
                    psOptions->padfDstNoDataImag != NULL ? psOptions->padfDstNoDataImag[iBand] : 0.0
                };

                int bAllValid = FALSE;
                eErr =
                    GDALWarpNoDataMasker( adfNoData, 1,
                                          psOptions->eWorkingDataType,
                                          oWK.nDstXOff, oWK.nDstYOff,
                                          oWK.nDstXSize, oWK.nDstYSize,
                                          oWK.papabyDstImage + iBand,
                                          FALSE, panBandMask,
                                          &bAllValid );

                // Optimization: if there's a single band and all pixels are
                // valid then we don't need a mask.
                if( bAllValid && psOptions->nBandCount == 1 )
                {
#if DEBUG_VERBOSE
                    CPLDebug("WARP",
                             "No need for a destination nodata mask as "
                             "all values are valid");
#endif
                    CPLFree(oWK.panDstValid);
                    oWK.panDstValid = NULL;
                    break;
                }

                for( int iWord = nMaskWords - 1; iWord >= 0; iWord-- )
                    oWK.panDstValid[iWord] |= panBandMask[iWord];
            }
            CPLFree( panBandMask );
        }
    }

/* -------------------------------------------------------------------- */
/*      Release IO Mutex, and acquire warper mutex.                     */
/* -------------------------------------------------------------------- */
    if( hIOMutex != NULL )
    {
        CPLReleaseMutex( hIOMutex );
        if( !CPLAcquireMutex( hWarpMutex, 600.0 ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to acquire WarpMutex in WarpRegion()." );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Optional application provided prewarp chunk processor.          */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psOptions->pfnPreWarpChunkProcessor != NULL )
        eErr = psOptions->pfnPreWarpChunkProcessor(
            &oWK, psOptions->pPreWarpProcessorArg );

/* -------------------------------------------------------------------- */
/*      Perform the warp.                                               */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None )
    {
        eErr = oWK.PerformWarp();
        ReportTiming( "In memory warp operation" );
    }

/* -------------------------------------------------------------------- */
/*      Optional application provided postwarp chunk processor.         */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psOptions->pfnPostWarpChunkProcessor != NULL )
        eErr = psOptions->pfnPostWarpChunkProcessor(
            &oWK, psOptions->pPostWarpProcessorArg );

/* -------------------------------------------------------------------- */
/*      Release Warp Mutex, and acquire io mutex.                       */
/* -------------------------------------------------------------------- */
    if( hIOMutex != NULL )
    {
        CPLReleaseMutex( hWarpMutex );
        if( !CPLAcquireMutex( hIOMutex, 600.0 ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to acquire IOMutex in WarpRegion()." );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Write destination alpha if available.                           */
/* -------------------------------------------------------------------- */
    if( eErr == CE_None && psOptions->nDstAlphaBand > 0 )
    {
        eErr =
            GDALWarpDstAlphaMasker( psOptions,
                                    -psOptions->nBandCount,
                                    psOptions->eWorkingDataType,
                                    oWK.nDstXOff, oWK.nDstYOff,
                                    oWK.nDstXSize, oWK.nDstYSize,
                                    oWK.papabyDstImage,
                                    TRUE, oWK.pafDstDensity );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    CPLFree( oWK.papabySrcImage[0] );
    CPLFree( oWK.papabySrcImage );
    CPLFree( oWK.papabyDstImage );

    if( oWK.papanBandSrcValid != NULL )
    {
        for( int i = 0; i < oWK.nBands; i++ )
            CPLFree( oWK.papanBandSrcValid[i] );
        CPLFree( oWK.papanBandSrcValid );
    }
    CPLFree( oWK.panUnifiedSrcValid );
    CPLFree( oWK.pafUnifiedSrcDensity );
    CPLFree( oWK.panDstValid );
    CPLFree( oWK.pafDstDensity );

    return eErr;
}

/************************************************************************/
/*                            GDALWarpRegionToBuffer()                  */
/************************************************************************/

/**
 * @see GDALWarpOperation::WarpRegionToBuffer()
 */

CPLErr GDALWarpRegionToBuffer( GDALWarpOperationH hOperation,
    int nDstXOff, int nDstYOff, int nDstXSize, int nDstYSize,
    void *pDataBuf, GDALDataType eBufDataType,
    int nSrcXOff, int nSrcYOff, int nSrcXSize, int nSrcYSize )

{
    VALIDATE_POINTER1( hOperation, "GDALWarpRegionToBuffer", CE_Failure );

    return reinterpret_cast<GDALWarpOperation *>(hOperation )->
        WarpRegionToBuffer( nDstXOff, nDstYOff, nDstXSize, nDstYSize,
                            pDataBuf, eBufDataType,
                            nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize );
}

/************************************************************************/
/*                          CreateKernelMask()                          */
/*                                                                      */
/*      If mask does not yet exist, create it.  Supported types are     */
/*      the name of the variable in question.  That is                  */
/*      "BandSrcValid", "UnifiedSrcValid", "UnifiedSrcDensity",         */
/*      "DstValid", and "DstDensity".                                   */
/************************************************************************/

CPLErr GDALWarpOperation::CreateKernelMask( GDALWarpKernel *poKernel,
                                            int iBand, const char *pszType )

{
    void **ppMask = NULL;
    int nXSize = 0;
    int nYSize = 0;
    int nBitsPerPixel = 0;
    int nDefault = 0;
    int  nExtraElts = 0;
    bool bDoMemset = true;

/* -------------------------------------------------------------------- */
/*      Get particulars of mask to be updated.                          */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszType, "BandSrcValid") )
    {
        if( poKernel->papanBandSrcValid == NULL )
            poKernel->papanBandSrcValid = static_cast<GUInt32 **>(
                CPLCalloc(sizeof(void*), poKernel->nBands));

        ppMask =
            reinterpret_cast<void **>(&(poKernel->papanBandSrcValid[iBand]));
        nExtraElts = WARP_EXTRA_ELTS;
        nXSize = poKernel->nSrcXSize;
        nYSize = poKernel->nSrcYSize;
        nBitsPerPixel = 1;
        nDefault = 0xff;
    }
    else if( EQUAL(pszType, "UnifiedSrcValid") )
    {
        ppMask = reinterpret_cast<void **>(&(poKernel->panUnifiedSrcValid));
        nExtraElts = WARP_EXTRA_ELTS;
        nXSize = poKernel->nSrcXSize;
        nYSize = poKernel->nSrcYSize;
        nBitsPerPixel = 1;
        nDefault = 0xff;
    }
    else if( EQUAL(pszType, "UnifiedSrcDensity") )
    {
        ppMask = reinterpret_cast<void **>(&(poKernel->pafUnifiedSrcDensity));
        nExtraElts = WARP_EXTRA_ELTS;
        nXSize = poKernel->nSrcXSize;
        nYSize = poKernel->nSrcYSize;
        nBitsPerPixel = 32;
        nDefault = 0;
        bDoMemset = false;
    }
    else if( EQUAL(pszType, "DstValid") )
    {
        ppMask = reinterpret_cast<void **>(&(poKernel->panDstValid));
        nXSize = poKernel->nDstXSize;
        nYSize = poKernel->nDstYSize;
        nBitsPerPixel = 1;
        nDefault = 0;
    }
    else if( EQUAL(pszType, "DstDensity") )
    {
        ppMask = reinterpret_cast<void **>(&(poKernel->pafDstDensity));
        nXSize = poKernel->nDstXSize;
        nYSize = poKernel->nDstYSize;
        nBitsPerPixel = 32;
        nDefault = 0;
        bDoMemset = false;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Internal error in CreateKernelMask(%s).",
                 pszType);
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Allocate if needed.                                             */
/* -------------------------------------------------------------------- */
    if( *ppMask == NULL )
    {
        const GIntBig nBytes =
          nBitsPerPixel == 32
          ? (static_cast<GIntBig>(nXSize) * nYSize + nExtraElts) * 4
          : (static_cast<GIntBig>(nXSize) * nYSize + nExtraElts + 31) / 8;

        const size_t nByteSize_t = static_cast<size_t>(nBytes);
#if SIZEOF_VOIDP != 8
        if( static_cast<GIntBig>(nByteSize_t) != nBytes )
        {
            CPLError(
                CE_Failure, CPLE_OutOfMemory,
                "Cannot allocate " CPL_FRMT_GIB " bytes",
                nBytes );
            return CE_Failure;
        }
#endif

        *ppMask = VSI_MALLOC_VERBOSE( nByteSize_t );

        if( *ppMask == NULL )
        {
            return CE_Failure;
        }

        if( bDoMemset )
            memset( *ppMask, nDefault, nByteSize_t );
    }

    return CE_None;
}

/************************************************************************/
/*                        ComputeSourceWindow()                         */
/************************************************************************/

CPLErr GDALWarpOperation::ComputeSourceWindow(
    int nDstXOff, int nDstYOff,
    int nDstXSize, int nDstYSize,
    int *pnSrcXOff, int *pnSrcYOff,
    int *pnSrcXSize, int *pnSrcYSize,
    int *pnSrcXExtraSize, int *pnSrcYExtraSize,
    double *pdfSrcFillRatio )

{
/* -------------------------------------------------------------------- */
/*      Figure out whether we just want to do the usual "along the      */
/*      edge" sampling, or using a grid.  The grid usage is             */
/*      important in some weird "inside out" cases like WGS84 to        */
/*      polar stereographic around the pole.   Also figure out the      */
/*      sampling rate.                                                  */
/* -------------------------------------------------------------------- */
    int nSampleMax = 0;
    int nStepCount = 21;
    int *pabSuccess = NULL;
    double *padfX = NULL;
    double *padfY = NULL;
    double *padfZ = NULL;
    int nSamplePoints = 0;
    double dfRatio = 0.0;

    if( CSLFetchNameValue( psOptions->papszWarpOptions,
                           "SAMPLE_STEPS" ) != NULL )
    {
        nStepCount =
            atoi(CSLFetchNameValue( psOptions->papszWarpOptions,
                                    "SAMPLE_STEPS" ));
        nStepCount = std::max(2, nStepCount);
    }

    const double dfStepSize = 1.0 / (nStepCount - 1);

    bool bUseGrid =
        CPLFetchBool(psOptions->papszWarpOptions, "SAMPLE_GRID", false);

  TryAgainWithGrid:
    nSamplePoints = 0;
    if( bUseGrid )
    {
        if( nStepCount > INT_MAX / nStepCount )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Too many steps : %d", nStepCount);
            return CE_Failure;
        }
        nSampleMax = nStepCount * nStepCount;
    }
    else
    {
        if( nStepCount > INT_MAX / 4 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Too many steps : %d", nStepCount);
            return CE_Failure;
        }
        nSampleMax = nStepCount * 4;
    }

    pabSuccess =
        static_cast<int *>(VSI_MALLOC2_VERBOSE(sizeof(int), nSampleMax));
    padfX = static_cast<double *>(VSI_MALLOC2_VERBOSE(sizeof(double) * 3,
                                                      nSampleMax));
    if( pabSuccess == NULL || padfX == NULL )
    {
        CPLFree( padfX );
        CPLFree( pabSuccess );
        return CE_Failure;
    }
    padfY = padfX + nSampleMax;
    padfZ = padfX + nSampleMax * 2;

/* -------------------------------------------------------------------- */
/*      Setup sample points on a grid pattern throughout the area.      */
/* -------------------------------------------------------------------- */
    if( bUseGrid )
    {
        for( double dfRatioY = 0.0;
             dfRatioY <= 1.0 + dfStepSize*0.5;
             dfRatioY += dfStepSize )
        {
            for( dfRatio = 0.0;
                 dfRatio <= 1.0 + dfStepSize*0.5;
                 dfRatio += dfStepSize )
            {
                padfX[nSamplePoints]   = dfRatio * nDstXSize + nDstXOff;
                padfY[nSamplePoints]   = dfRatioY * nDstYSize + nDstYOff;
                padfZ[nSamplePoints++] = 0.0;
            }
        }
    }
 /* -------------------------------------------------------------------- */
 /*      Setup sample points all around the edge of the output raster.   */
 /* -------------------------------------------------------------------- */
    else
    {
        for( dfRatio = 0.0;
             dfRatio <= 1.0 + dfStepSize*0.5;
             dfRatio += dfStepSize )
        {
            // Along top
            padfX[nSamplePoints]   = dfRatio * nDstXSize + nDstXOff;
            padfY[nSamplePoints]   = nDstYOff;
            padfZ[nSamplePoints++] = 0.0;

            // Along bottom
            padfX[nSamplePoints]   = dfRatio * nDstXSize + nDstXOff;
            padfY[nSamplePoints]   = nDstYOff + nDstYSize;
            padfZ[nSamplePoints++] = 0.0;

            // Along left
            padfX[nSamplePoints]   = nDstXOff;
            padfY[nSamplePoints]   = dfRatio * nDstYSize + nDstYOff;
            padfZ[nSamplePoints++] = 0.0;

            // Along right
            padfX[nSamplePoints]   = nDstXSize + nDstXOff;
            padfY[nSamplePoints]   = dfRatio * nDstYSize + nDstYOff;
            padfZ[nSamplePoints++] = 0.0;
        }
    }

    CPLAssert( nSamplePoints == nSampleMax );

/* -------------------------------------------------------------------- */
/*      Transform them to the input pixel coordinate space              */
/* -------------------------------------------------------------------- */
    if( !psOptions->pfnTransformer( psOptions->pTransformerArg,
                                    TRUE, nSamplePoints,
                                    padfX, padfY, padfZ, pabSuccess ) )
    {
        CPLFree( padfX );
        CPLFree( pabSuccess );

        CPLError( CE_Failure, CPLE_AppDefined,
                  "GDALWarperOperation::ComputeSourceWindow() failed because "
                  "the pfnTransformer failed." );
        return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Collect the bounds, ignoring any failed points.                 */
/* -------------------------------------------------------------------- */
    double dfMinXOut = 0.0;
    double dfMinYOut = 0.0;
    double dfMaxXOut = 0.0;
    double dfMaxYOut = 0.0;
    bool bGotInitialPoint = false;
    int nFailedCount = 0;

    for( int i = 0; i < nSamplePoints; i++ )
    {
        if( !pabSuccess[i] )
        {
            nFailedCount++;
            continue;
        }

        // If this happens this is likely the symptom of a bug somewhere.
        if( CPLIsNan(padfX[i]) || CPLIsNan(padfY[i]) )
        {
            static bool bNanCoordFound = false;
            if( !bNanCoordFound )
            {
                CPLDebug("WARP", "NaN coordinate found.");
                bNanCoordFound = true;
            }
            nFailedCount++;
            continue;
        }

        if( !bGotInitialPoint )
        {
            bGotInitialPoint = true;
            dfMinXOut = padfX[i];
            dfMaxXOut = padfX[i];
            dfMinYOut = padfY[i];
            dfMaxYOut = padfY[i];
        }
        else
        {
            dfMinXOut = std::min(dfMinXOut, padfX[i]);
            dfMinYOut = std::min(dfMinYOut, padfY[i]);
            dfMaxXOut = std::max(dfMaxXOut, padfX[i]);
            dfMaxYOut = std::max(dfMaxYOut, padfY[i]);
        }
    }

    CPLFree( padfX );
    CPLFree( pabSuccess );

/* -------------------------------------------------------------------- */
/*      If we got any failures when not using a grid, we should         */
/*      really go back and try again with the grid.  Sorry for the      */
/*      goto.                                                           */
/* -------------------------------------------------------------------- */
    if( !bUseGrid && nFailedCount > 0 )
    {
        bUseGrid = true;
        goto TryAgainWithGrid;
    }

/* -------------------------------------------------------------------- */
/*      If we get hardly any points (or none) transforming, we give     */
/*      up.                                                             */
/* -------------------------------------------------------------------- */
    if( nFailedCount > nSamplePoints - 5 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Too many points (%d out of %d) failed to transform, "
                  "unable to compute output bounds.",
                  nFailedCount, nSamplePoints );
        return CE_Failure;
    }

    if( nFailedCount > 0 )
        CPLDebug( "GDAL",
                  "GDALWarpOperation::ComputeSourceWindow() %d out of %d "
                  "points failed to transform.",
                  nFailedCount, nSamplePoints );

/* -------------------------------------------------------------------- */
/*   Early exit to avoid crazy values to cause a huge nResWinSize that  */
/*   would result in a result window wrongly covering the whole raster. */
/* -------------------------------------------------------------------- */
    const int nRasterXSize = GDALGetRasterXSize(psOptions->hSrcDS);
    const int nRasterYSize = GDALGetRasterYSize(psOptions->hSrcDS);
    if( dfMinXOut > nRasterXSize ||
        dfMaxXOut < 0 ||
        dfMinYOut > nRasterYSize ||
        dfMaxYOut < 0 )
    {
        *pnSrcXOff = 0;
        *pnSrcYOff = 0;
        *pnSrcXSize = 0;
        *pnSrcYSize = 0;
        if( pnSrcXExtraSize )
            *pnSrcXExtraSize = 0;
        if( pnSrcYExtraSize )
            *pnSrcYExtraSize = 0;
        if( pdfSrcFillRatio )
            *pdfSrcFillRatio = 0.0;
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      How much of a window around our source pixel might we need      */
/*      to collect data from based on the resampling kernel?  Even      */
/*      if the requested central pixel falls off the source image,      */
/*      we may need to collect data if some portion of the              */
/*      resampling kernel could be on-image.                            */
/* -------------------------------------------------------------------- */
    int nResWinSize = GWKGetFilterRadius(psOptions->eResampleAlg);

    // Take scaling into account.
    const double dfXScale =
        static_cast<double>(nDstXSize) / (dfMaxXOut - dfMinXOut);
    const double dfYScale =
        static_cast<double>(nDstYSize) / (dfMaxYOut - dfMinYOut);
    const int nXRadius = dfXScale < 1.0 ?
        static_cast<int>(ceil( nResWinSize / dfXScale )) : nResWinSize;
    const int nYRadius = dfYScale < 1.0 ?
        static_cast<int>(ceil( nResWinSize / dfYScale )) : nResWinSize;
    nResWinSize = std::max(nXRadius, nYRadius);

/* -------------------------------------------------------------------- */
/*      Allow addition of extra sample pixels to source window to       */
/*      avoid missing pixels due to sampling error.  In fact,           */
/*      fallback to adding a bit to the window if any points failed     */
/*      to transform.                                                   */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( psOptions->papszWarpOptions,
                           "SOURCE_EXTRA" ) != NULL )
    {
        nResWinSize += atoi(
            CSLFetchNameValue( psOptions->papszWarpOptions, "SOURCE_EXTRA" ));
    }
    else if( nFailedCount > 0 )
    {
        nResWinSize += 10;
    }

/* -------------------------------------------------------------------- */
/*      return bounds.                                                  */
/* -------------------------------------------------------------------- */
#if DEBUG_VERBOSE
    CPLDebug(
        "WARP",
        "dst=(%d,%d,%d,%d) raw src=(minx=%.8g,miny=%.8g,maxx=%.8g,maxy=%.8g)",
        nDstXOff, nDstYOff, nDstXSize, nDstYSize,
        dfMinXOut, dfMinYOut, dfMaxXOut, dfMaxYOut);
#endif
    const int nMinXOutClamped =
        dfMinXOut > INT_MAX ? INT_MAX :
        dfMinXOut >= 0.0 ? static_cast<int>(dfMinXOut) : 0;
    *pnSrcXOff = nMinXOutClamped;
    const int nMinYOutClamped =
        dfMinYOut > INT_MAX ? INT_MAX :
        dfMinYOut >= 0.0 ? static_cast<int>(dfMinYOut) : 0;
    *pnSrcYOff = nMinYOutClamped;
    *pnSrcXOff = std::min(*pnSrcXOff, nRasterXSize);
    *pnSrcYOff = std::min(*pnSrcYOff, nRasterYSize);

    double dfCeilMaxXOut = ceil(dfMaxXOut);
    if( dfCeilMaxXOut > INT_MAX )
        dfCeilMaxXOut = INT_MAX;
    double dfCeilMaxYOut = ceil(dfMaxYOut);
    if( dfCeilMaxYOut > INT_MAX )
        dfCeilMaxYOut = INT_MAX;

    int nSrcXSizeRaw = std::min(nRasterXSize - *pnSrcXOff,
                                static_cast<int>(dfCeilMaxXOut) - *pnSrcXOff);
    int nSrcYSizeRaw = std::min(nRasterYSize - *pnSrcYOff,
                                static_cast<int>(dfCeilMaxYOut) - *pnSrcYOff);
    nSrcXSizeRaw = std::max(0, nSrcXSizeRaw);
    nSrcYSizeRaw = std::max(0, nSrcYSizeRaw);

    *pnSrcXOff = std::max(0, nMinXOutClamped - nResWinSize);
    *pnSrcYOff = std::max(0, nMinYOutClamped - nResWinSize);
    *pnSrcXOff = std::min(*pnSrcXOff, nRasterXSize);
    *pnSrcYOff = std::min(*pnSrcYOff, nRasterYSize);

    *pnSrcXSize =
        std::min(nRasterXSize - *pnSrcXOff,
                 static_cast<int>(dfCeilMaxXOut) - *pnSrcXOff + nResWinSize);
    *pnSrcYSize =
        std::min(nRasterYSize - *pnSrcYOff,
                 static_cast<int>(dfCeilMaxYOut) - *pnSrcYOff + nResWinSize);
    *pnSrcXSize = std::max(0, *pnSrcXSize);
    *pnSrcYSize = std::max(0, *pnSrcYSize);

    if( pnSrcXExtraSize )
        *pnSrcXExtraSize = *pnSrcXSize - nSrcXSizeRaw;
    if( pnSrcYExtraSize )
        *pnSrcYExtraSize = *pnSrcYSize - nSrcYSizeRaw;

    // Computed the ratio of the clamped source raster window size over
    // the unclamped source raster window size.
    if( pdfSrcFillRatio )
        *pdfSrcFillRatio =
            *pnSrcXSize * *pnSrcYSize /
            std::max(1.0,
                     (dfMaxXOut - dfMinXOut + 2 * nResWinSize) *
                     (dfMaxYOut - dfMinYOut + 2 * nResWinSize));

    return CE_None;
}

/************************************************************************/
/*                            ReportTiming()                            */
/************************************************************************/

void GDALWarpOperation::ReportTiming( const char * pszMessage )

{
    if( !bReportTimings )
        return;

    const unsigned long nNewTime = VSITime(NULL);

    if( pszMessage != NULL )
    {
        CPLDebug( "WARP_TIMING", "%s: %lds",
                  pszMessage, static_cast<long>(nNewTime - nLastTimeReported) );
    }

    nLastTimeReported = nNewTime;
}
