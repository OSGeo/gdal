/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Image Translator Program
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2015, Faza Mahamood
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
#include "gdal_utils.h"
#include "gdal_utils_priv.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <limits>

#include "commonutils.h"
#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
#include "gdal_vrt.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "vrtdataset.h"

CPL_CVSID("$Id$")

static int ArgIsNumeric( const char * );
static void AttachMetadata( GDALDatasetH, char ** );
static void CopyBandInfo( GDALRasterBand * poSrcBand, GDALRasterBand * poDstBand,
                            int bCanCopyStatsMetadata, int bCopyScale, int bCopyNoData );

typedef enum
{
    MASK_DISABLED,
    MASK_AUTO,
    MASK_USER
} MaskMode;

/************************************************************************/
/*                         GDALTranslateScaleParams                     */
/************************************************************************/

/** scaling parameters for use in GDALTranslateOptions.
 */
typedef struct
{
    /*! scaling is done only if it is set to TRUE. This is helpful when there is a need to
        scale only certain bands. */
    int     bScale;

    /*! set it to TRUE if dfScaleSrcMin and dfScaleSrcMax is set. When it is FALSE, the
        input range is automatically computed from the source data. */
    bool    bHaveScaleSrc;

    /*! the range of input pixel values which need to be scaled */
    double dfScaleSrcMin;
    double dfScaleSrcMax;

    /*! the range of output pixel values. If GDALTranslateScaleParams::dfScaleDstMin
        and GDALTranslateScaleParams::dfScaleDstMax are not set, then the output
        range is 0 to 255. */
    double dfScaleDstMin;
    double dfScaleDstMax;
} GDALTranslateScaleParams;

/************************************************************************/
/*                         GDALTranslateOptions                         */
/************************************************************************/

/** Options for use with GDALTranslate(). GDALTranslateOptions* must be allocated
 * and freed with GDALTranslateOptionsNew() and GDALTranslateOptionsFree() respectively.
 */
struct GDALTranslateOptions
{

    /*! output format. The default is GeoTIFF(GTiff). Use the short format name. */
    char *pszFormat;

    /*! allow or suppress progress monitor and other non-error output */
    bool bQuiet;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress;

    /*! pointer to the progress data variable */
    void *pProgressData;

    /*! for the output bands to be of the indicated data type */
    GDALDataType eOutputType;

    MaskMode eMaskMode;

    /*! number of input bands to write to the output file, or to reorder bands */
    int nBandCount;

    /*! list of input bands to write to the output file, or to reorder bands. The
        value 1 corresponds to the 1st band. */
    int *panBandList; /* negative value of panBandList[i] means mask band of ABS(panBandList[i]) */

    /*! size of the output file. GDALTranslateOptions::nOXSizePixel is in pixels and
        GDALTranslateOptions::nOYSizePixel is in lines. If one of the two values is
        set to 0, its value will be determined from the other one, while maintaining
        the aspect ratio of the source dataset */
    int nOXSizePixel;
    int nOYSizePixel;

    /*! size of the output file. GDALTranslateOptions::dfOXSizePct and GDALTranslateOptions::dfOYSizePct
        are fraction of the input image size. The value 100 means 100%. If one of the two values is set
        to 0, its value will be determined from the other one, while maintaining the aspect ratio of the
        source dataset */
    double dfOXSizePct;
    double dfOYSizePct;

    /*! list of creation options to the output format driver */
    char **papszCreateOptions;

    /*! subwindow from the source image for copying based on pixel/line location */
    double adfSrcWin[4];

    /*! don't be forgiving of mismatches and lost data when translating to the output format */
    bool bStrict;

    /*! apply the scale/offset metadata for the bands to convert scaled values to unscaled values.
     *  It is also often necessary to reset the output datatype with GDALTranslateOptions::eOutputType */
    bool bUnscale;

    /*! the size of pasScaleParams */
    int nScaleRepeat;

    /*! the list of scale parameters for each band. */
    GDALTranslateScaleParams *pasScaleParams;

    /*! It is set to TRUE, when scale parameters are specific to each band */
    bool bHasUsedExplicitScaleBand;

    /*! the size of the list padfExponent */
    int nExponentRepeat;

    /*! to apply non-linear scaling with a power function. It is the list of exponents of the power
        function (must be positive). This option must be used with GDALTranslateOptions::pasScaleParams. If
        GDALTranslateOptions::nExponentRepeat is 1, it is applied to all bands of the output image. */
    double *padfExponent;

    bool bHasUsedExplicitExponentBand;

    /*! list of metadata key and value to set on the output dataset if possible.
     *  GDALTranslateOptionsSetMetadataOptions() and GDALTranslateOptionsAddMetadataOptions()
     *  should be used */
    char **papszMetadataOptions;

    /*! override the projection for the output file. The SRS may be any of the usual
        GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file containing the WKT. */
    char *pszOutputSRS;

    /*! number of GCPS to be added to the output dataset */
    int nGCPCount;

    /*! list of GCPs to be added to the output dataset */
    GDAL_GCP *pasGCPs;

    /*! assign/override the georeferenced bounds of the output file. This assigns
        georeferenced bounds to the output file, ignoring what would have been
        derived from the source file. So this does not cause reprojection to the
        specified SRS. */
    double adfULLR[4];

    /*! set a nodata value specified in GDALTranslateOptions::dfNoDataReal to the output bands */
    bool bSetNoData;

    /*! avoid setting a nodata value to the output file if one exists for the source file */
    bool bUnsetNoData;

    /*! Assign a specified nodata value to output bands ( GDALTranslateOptions::bSetNoData option
        should be set). Note that if the input dataset has a nodata value, this does not cause
        pixel values that are equal to that nodata value to be changed to the value specified. */
    double dfNoDataReal;

    /*! to expose a dataset with 1 band with a color table as a dataset with
        3 (RGB) or 4 (RGBA) bands. Useful for output drivers such as JPEG,
        JPEG2000, MrSID, ECW that don't support color indexed datasets.
        The 1 value enables to expand a dataset with a color table that only
        contains gray levels to a gray indexed dataset. */
    int nRGBExpand;

    int nMaskBand; /* negative value means mask band of ABS(nMaskBand) */

    /*! force recomputation of statistics */
    bool bStats;

    bool bApproxStats;

    /*! If this option is set, GDALTranslateOptions::adfSrcWin or (GDALTranslateOptions::dfULX,
        GDALTranslateOptions::dfULY, GDALTranslateOptions::dfLRX, GDALTranslateOptions::dfLRY)
        values that falls partially outside the source raster extent will be considered
        as an error. The default behaviour is to accept such requests. */
    bool bErrorOnPartiallyOutside;

    /*! Same as bErrorOnPartiallyOutside, except that the criterion for
        erroring out is when the request falls completely outside the
        source raster extent. */
    bool bErrorOnCompletelyOutside;

    /*! does not copy source RAT into destination dataset (when TRUE) */
    bool bNoRAT;

    /*! resampling algorithm
        nearest (default), bilinear, cubic, cubicspline, lanczos, average, mode */
    char *pszResampling;

    /*! target resolution. The values must be expressed in georeferenced units.
        Both must be positive values. This is exclusive with GDALTranslateOptions::nOXSizePixel
        (or GDALTranslateOptions::dfOXSizePct), GDALTranslateOptions::nOYSizePixel
        (or GDALTranslateOptions::dfOYSizePct) and GDALTranslateOptions::adfULLR */
    double dfXRes;
    double dfYRes;

    /*! subwindow from the source image for copying (like GDALTranslateOptions::adfSrcWin)
        but with the corners given in georeferenced coordinates (by default
        expressed in the SRS of the dataset. Can be changed with
        pszProjSRS) */
    double dfULX;
    double dfULY;
    double dfLRX;
    double dfLRY;

    /*! SRS in which to interpret the coordinates given with GDALTranslateOptions::dfULX,
        GDALTranslateOptions::dfULY, GDALTranslateOptions::dfLRX, GDALTranslateOptions::dfLRY.
        The SRS may be any of the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or
        a file containing the WKT. Note that this does not cause reprojection of the
        dataset to the specified SRS. */
    char *pszProjSRS;

    int nLimitOutSize;
};

/************************************************************************/
/*                              SrcToDst()                              */
/************************************************************************/

static void SrcToDst( double dfX, double dfY,
                      double dfSrcXOff, double dfSrcYOff,
                      double dfSrcXSize, double dfSrcYSize,
                      double dfDstXOff, double dfDstYOff,
                      double dfDstXSize, double dfDstYSize,
                      double &dfXOut, double &dfYOut )

{
    dfXOut = ((dfX - dfSrcXOff) / dfSrcXSize) * dfDstXSize + dfDstXOff;
    dfYOut = ((dfY - dfSrcYOff) / dfSrcYSize) * dfDstYSize + dfDstYOff;
}

/************************************************************************/
/*                          GetSrcDstWindow()                           */
/************************************************************************/

static bool FixSrcDstWindow( double* padfSrcWin, double* padfDstWin,
                             int nSrcRasterXSize,
                             int nSrcRasterYSize )

{
    const double dfSrcXOff = padfSrcWin[0];
    const double dfSrcYOff = padfSrcWin[1];
    const double dfSrcXSize = padfSrcWin[2];
    const double dfSrcYSize = padfSrcWin[3];

    const double dfDstXOff = padfDstWin[0];
    const double dfDstYOff = padfDstWin[1];
    const double dfDstXSize = padfDstWin[2];
    const double dfDstYSize = padfDstWin[3];

    bool bModifiedX = false;
    bool bModifiedY = false;

    double dfModifiedSrcXOff = dfSrcXOff;
    double dfModifiedSrcYOff = dfSrcYOff;

    double dfModifiedSrcXSize = dfSrcXSize;
    double dfModifiedSrcYSize = dfSrcYSize;

/* -------------------------------------------------------------------- */
/*      Clamp within the bounds of the available source data.           */
/* -------------------------------------------------------------------- */
    if( dfModifiedSrcXOff < 0 )
    {
        dfModifiedSrcXSize += dfModifiedSrcXOff;
        dfModifiedSrcXOff = 0;

        bModifiedX = true;
    }

    if( dfModifiedSrcYOff < 0 )
    {
        dfModifiedSrcYSize += dfModifiedSrcYOff;
        dfModifiedSrcYOff = 0;
        bModifiedY = true;
    }

    if( dfModifiedSrcXOff + dfModifiedSrcXSize > nSrcRasterXSize )
    {
        dfModifiedSrcXSize = nSrcRasterXSize - dfModifiedSrcXOff;
        bModifiedX = true;
    }

    if( dfModifiedSrcYOff + dfModifiedSrcYSize > nSrcRasterYSize )
    {
        dfModifiedSrcYSize = nSrcRasterYSize - dfModifiedSrcYOff;
        bModifiedY = true;
    }

/* -------------------------------------------------------------------- */
/*      Don't do anything if the requesting region is completely off    */
/*      the source image.                                               */
/* -------------------------------------------------------------------- */
    if( dfModifiedSrcXOff >= nSrcRasterXSize
        || dfModifiedSrcYOff >= nSrcRasterYSize
        || dfModifiedSrcXSize <= 0 || dfModifiedSrcYSize <= 0 )
    {
        return false;
    }

    padfSrcWin[0] = dfModifiedSrcXOff;
    padfSrcWin[1] = dfModifiedSrcYOff;
    padfSrcWin[2] = dfModifiedSrcXSize;
    padfSrcWin[3] = dfModifiedSrcYSize;

/* -------------------------------------------------------------------- */
/*      If we haven't had to modify the source rectangle, then the      */
/*      destination rectangle must be the whole region.                 */
/* -------------------------------------------------------------------- */
    if( !bModifiedX && !bModifiedY )
        return true;

/* -------------------------------------------------------------------- */
/*      Now transform this possibly reduced request back into the       */
/*      destination buffer coordinates in case the output region is     */
/*      less than the whole buffer.                                     */
/* -------------------------------------------------------------------- */
    double dfDstULX, dfDstULY, dfDstLRX, dfDstLRY;

    SrcToDst( dfModifiedSrcXOff, dfModifiedSrcYOff,
              dfSrcXOff, dfSrcYOff,
              dfSrcXSize, dfSrcYSize,
              dfDstXOff, dfDstYOff,
              dfDstXSize, dfDstYSize,
              dfDstULX, dfDstULY );
    SrcToDst( dfModifiedSrcXOff + dfModifiedSrcXSize, dfModifiedSrcYOff + dfModifiedSrcYSize,
              dfSrcXOff, dfSrcYOff,
              dfSrcXSize, dfSrcYSize,
              dfDstXOff, dfDstYOff,
              dfDstXSize, dfDstYSize,
              dfDstLRX, dfDstLRY );

    double dfModifiedDstXOff = dfDstXOff;
    double dfModifiedDstYOff = dfDstYOff;
    double dfModifiedDstXSize = dfDstXSize;
    double dfModifiedDstYSize = dfDstYSize;

    if( bModifiedX )
    {
        dfModifiedDstXOff = dfDstULX - dfDstXOff;
        dfModifiedDstXSize = (dfDstLRX - dfDstXOff) - dfModifiedDstXOff;

        dfModifiedDstXOff = std::max(0.0, dfModifiedDstXOff);
        if( dfModifiedDstXOff + dfModifiedDstXSize > dfDstXSize )
            dfModifiedDstXSize = dfDstXSize - dfModifiedDstXOff;
    }

    if( bModifiedY )
    {
        dfModifiedDstYOff = dfDstULY - dfDstYOff;
        dfModifiedDstYSize = (dfDstLRY - dfDstYOff) - dfModifiedDstYOff;

        dfModifiedDstYOff = std::max(0.0, dfModifiedDstYOff);
        if( dfModifiedDstYOff + dfModifiedDstYSize > dfDstYSize )
            dfModifiedDstYSize = dfDstYSize - dfModifiedDstYOff;
    }

    if( dfModifiedDstXSize <= 0.0 || dfModifiedDstYSize <= 0.0 )
    {
        return false;
    }

    padfDstWin[0] = dfModifiedDstXOff;
    padfDstWin[1] = dfModifiedDstYOff;
    padfDstWin[2] = dfModifiedDstXSize;
    padfDstWin[3] = dfModifiedDstYSize;

    return true;
}

/************************************************************************/
/*                          GDALTranslateOptionsClone()                 */
/************************************************************************/

static
GDALTranslateOptions* GDALTranslateOptionsClone(const GDALTranslateOptions *psOptionsIn)
{
    GDALTranslateOptions* psOptions = static_cast<GDALTranslateOptions*>(
        CPLMalloc(sizeof(GDALTranslateOptions)));
    memcpy(psOptions, psOptionsIn, sizeof(GDALTranslateOptions));
    psOptions->pszFormat = CPLStrdup(psOptionsIn->pszFormat);
    if( psOptionsIn->panBandList )
    {
        psOptions->panBandList =
            static_cast<int *>(CPLMalloc(sizeof(int) * psOptions->nBandCount));
        memcpy(psOptions->panBandList, psOptionsIn->panBandList,
               sizeof(int) * psOptions->nBandCount);
    }
    psOptions->papszCreateOptions = CSLDuplicate(psOptionsIn->papszCreateOptions);
    if( psOptionsIn->pasScaleParams )
    {
        psOptions->pasScaleParams = static_cast<GDALTranslateScaleParams *>(
            CPLMalloc(sizeof(GDALTranslateScaleParams) *
                      psOptions->nScaleRepeat));
        memcpy(psOptions->pasScaleParams, psOptionsIn->pasScaleParams,
               sizeof(GDALTranslateScaleParams) * psOptions->nScaleRepeat);
    }
    if( psOptionsIn->padfExponent )
    {
        psOptions->padfExponent = static_cast<double *>(
            CPLMalloc(sizeof(double) * psOptions->nExponentRepeat));
        memcpy(psOptions->padfExponent, psOptionsIn->padfExponent,
               sizeof(double) * psOptions->nExponentRepeat);
    }
    psOptions->papszMetadataOptions = CSLDuplicate(psOptionsIn->papszMetadataOptions);
    if( psOptionsIn->pszOutputSRS ) psOptions->pszOutputSRS = CPLStrdup(psOptionsIn->pszOutputSRS);
    if( psOptionsIn->nGCPCount )
        psOptions->pasGCPs = GDALDuplicateGCPs( psOptionsIn->nGCPCount, psOptionsIn->pasGCPs );
    if( psOptionsIn->pszResampling ) psOptions->pszResampling = CPLStrdup(psOptionsIn->pszResampling);
    if( psOptionsIn->pszProjSRS ) psOptions->pszProjSRS = CPLStrdup(psOptionsIn->pszProjSRS);
    return psOptions;
}

/************************************************************************/
/*                        GDALTranslateFlush()                          */
/************************************************************************/

static GDALDatasetH GDALTranslateFlush(GDALDatasetH hOutDS)
{
    if( hOutDS != NULL )
    {
        CPLErr eErrBefore = CPLGetLastErrorType();
        GDALFlushCache( hOutDS );
        if (eErrBefore == CE_None &&
            CPLGetLastErrorType() != CE_None)
        {
            GDALClose(hOutDS);
            hOutDS = NULL;
        }
    }
    return hOutDS;
}

/************************************************************************/
/*                             GDALTranslate()                          */
/************************************************************************/

/**
 * Converts raster data between different formats.
 *
 * This is the equivalent of the <a href="gdal_translate.html">gdal_translate</a> utility.
 *
 * GDALTranslateOptions* must be allocated and freed with GDALTranslateOptionsNew()
 * and GDALTranslateOptionsFree() respectively.
 *
 * @param pszDest the destination dataset path.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALTranslateOptionsNew() or NULL.
 * @param pbUsageError the pointer to int variable to determine any usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALTranslate( const char *pszDest, GDALDatasetH hSrcDataset,
                            const GDALTranslateOptions *psOptionsIn, int *pbUsageError )

{
    CPLErrorReset();
    if( hSrcDataset == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No source dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }
    if( pszDest == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return NULL;
    }

    GDALTranslateOptions* psOptions =
        (psOptionsIn) ? GDALTranslateOptionsClone(psOptionsIn) :
                        GDALTranslateOptionsNew(NULL, NULL);

    GDALDatasetH hOutDS = NULL;
    bool bGotBounds = false;

    if(pbUsageError)
        *pbUsageError = FALSE;

    if(psOptions->adfULLR[0] != 0.0 || psOptions->adfULLR[1] != 0.0 || psOptions->adfULLR[2] != 0.0 || psOptions->adfULLR[3] != 0.0)
        bGotBounds = true;

    const char *pszSource = GDALGetDescription(hSrcDataset);

    if( strcmp(pszSource, pszDest) == 0 && pszSource[0] != '\0' &&
        GDALGetDatasetDriver(hSrcDataset) != GDALGetDriverByName("MEM") )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "Source and destination datasets must be different.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

    CPLString osProjSRS;

    if(psOptions->pszProjSRS != NULL)
    {
        OGRSpatialReference oSRS;

        if( oSRS.SetFromUserInput( psOptions->pszProjSRS ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                      psOptions->pszProjSRS );
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }

        char* pszSRS = NULL;
        oSRS.exportToWkt( &pszSRS );
        if( pszSRS )
            osProjSRS = pszSRS;
        CPLFree( pszSRS );
    }

    if(psOptions->pszOutputSRS != NULL)
    {
        OGRSpatialReference oOutputSRS;

        if( oOutputSRS.SetFromUserInput( psOptions->pszOutputSRS ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                      psOptions->pszOutputSRS );
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }

        char* pszSRS = NULL;
        oOutputSRS.exportToWkt( &pszSRS );
        CPLFree( psOptions->pszOutputSRS );
        psOptions->pszOutputSRS = CPLStrdup( pszSRS );
        CPLFree( pszSRS );
    }

/* -------------------------------------------------------------------- */
/*      Check that incompatible options are not used                    */
/* -------------------------------------------------------------------- */

    if( (psOptions->nOXSizePixel != 0 || psOptions->dfOXSizePct != 0.0 || psOptions->nOYSizePixel != 0 ||
         psOptions->dfOYSizePct != 0.0) && (psOptions->dfXRes != 0 && psOptions->dfYRes != 0) )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "-outsize and -tr options cannot be used at the same time.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }
    if( bGotBounds &&  (psOptions->dfXRes != 0 && psOptions->dfYRes != 0) )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "-a_ullr and -tr options cannot be used at the same time.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Collect some information from the source file.                  */
/* -------------------------------------------------------------------- */
    const int nRasterXSize = GDALGetRasterXSize( hSrcDataset );
    const int nRasterYSize = GDALGetRasterYSize( hSrcDataset );

    if( psOptions->adfSrcWin[2] == 0 && psOptions->adfSrcWin[3] == 0 )
    {
        psOptions->adfSrcWin[2] = nRasterXSize;
        psOptions->adfSrcWin[3] = nRasterYSize;
    }

/* -------------------------------------------------------------------- */
/*      Build band list to translate                                    */
/* -------------------------------------------------------------------- */
    bool bAllBandsInOrder = true;

    if( psOptions->panBandList == NULL )
    {
        psOptions->nBandCount = GDALGetRasterCount( hSrcDataset );
        if( psOptions->nBandCount == 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Input file has no bands, and so cannot be translated." );
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }

        psOptions->panBandList = static_cast<int *>(
            CPLMalloc(sizeof(int) * psOptions->nBandCount));
        for( int i = 0; i < psOptions->nBandCount; i++ )
            psOptions->panBandList[i] = i+1;
    }
    else
    {
        for( int i = 0; i < psOptions->nBandCount; i++ )
        {
            if( std::abs(psOptions->panBandList[i]) >
                GDALGetRasterCount(hSrcDataset) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Band %d requested, but only bands 1 to %d available.",
                         std::abs(psOptions->panBandList[i]),
                         GDALGetRasterCount(hSrcDataset) );
                GDALTranslateOptionsFree(psOptions);
                return NULL;
            }

            if( psOptions->panBandList[i] != i+1 )
                bAllBandsInOrder = FALSE;
        }

        if( psOptions->nBandCount != GDALGetRasterCount( hSrcDataset ) )
            bAllBandsInOrder = FALSE;
    }

    if( psOptions->nScaleRepeat > psOptions->nBandCount )
    {
        if( !psOptions->bHasUsedExplicitScaleBand )
            CPLError( CE_Failure, CPLE_IllegalArg, "-scale has been specified more times than the number of output bands");
        else
            CPLError( CE_Failure, CPLE_IllegalArg, "-scale_XX has been specified with XX greater than the number of output bands");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

    if( psOptions->nExponentRepeat > psOptions->nBandCount )
    {
        if( !psOptions->bHasUsedExplicitExponentBand )
            CPLError( CE_Failure, CPLE_IllegalArg, "-exponent has been specified more times than the number of output bands");
        else
            CPLError( CE_Failure, CPLE_IllegalArg, "-exponent_XX has been specified with XX greater than the number of output bands");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Compute the source window from the projected source window      */
/*      if the projected coordinates were provided.  Note that the      */
/*      projected coordinates are in ulx, uly, lrx, lry format,         */
/*      while the adfSrcWin is xoff, yoff, xsize, ysize with the        */
/*      xoff,yoff being the ulx, uly in pixel/line.                     */
/* -------------------------------------------------------------------- */
    const char *pszProjection = NULL;

    if( psOptions->dfULX != 0.0 || psOptions->dfULY != 0.0
        || psOptions->dfLRX != 0.0 || psOptions->dfLRY != 0.0 )
    {
        double adfGeoTransform[6];

        GDALGetGeoTransform( hSrcDataset, adfGeoTransform );

        if( adfGeoTransform[1] == 0.0 || adfGeoTransform[5] == 0.0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                     "The -projwin option was used, but the geotransform is "
                     "invalid." );
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
        if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                     "The -projwin option was used, but the geotransform is\n"
                     "rotated.  This configuration is not supported." );
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }

        if( !osProjSRS.empty() )
        {
            pszProjection = GDALGetProjectionRef( hSrcDataset );
            if( pszProjection != NULL && strlen(pszProjection) > 0 )
            {
                OGRSpatialReference oSRSIn;
                OGRSpatialReference oSRSDS;
                oSRSIn.SetFromUserInput(osProjSRS);
                oSRSDS.SetFromUserInput(pszProjection);
                if( !oSRSIn.IsSame(&oSRSDS) )
                {
                    OGRCoordinateTransformation* poCT = OGRCreateCoordinateTransformation(&oSRSIn, &oSRSDS);
                    if( !(poCT &&
                        poCT->Transform(1, &psOptions->dfULX, &psOptions->dfULY) &&
                        poCT->Transform(1, &psOptions->dfLRX, &psOptions->dfLRY)) )
                    {
                        OGRCoordinateTransformation::DestroyCT(poCT);

                        CPLError( CE_Failure, CPLE_AppDefined, "-projwin_srs ignored since coordinate transformation failed.");
                        GDALTranslateOptionsFree(psOptions);
                        return NULL;
                    }
                    delete poCT;
                }
            }
            else
            {
                CPLError( CE_None, CPLE_None, "-projwin_srs ignored since the dataset has no projection.");
            }
        }

        psOptions->adfSrcWin[0] = (psOptions->dfULX - adfGeoTransform[0]) / adfGeoTransform[1];
        psOptions->adfSrcWin[1] = (psOptions->dfULY - adfGeoTransform[3]) / adfGeoTransform[5];

        psOptions->adfSrcWin[2] = (psOptions->dfLRX - psOptions->dfULX) / adfGeoTransform[1];
        psOptions->adfSrcWin[3] = (psOptions->dfLRY - psOptions->dfULY) / adfGeoTransform[5];

        // In case of nearest resampling, round to integer pixels (#6610)
        if( psOptions->pszResampling == NULL ||
            EQUALN(psOptions->pszResampling, "NEAR", 4) )
        {
            psOptions->adfSrcWin[0] = floor(psOptions->adfSrcWin[0] + 0.001);
            psOptions->adfSrcWin[1] = floor(psOptions->adfSrcWin[1] + 0.001);
            psOptions->adfSrcWin[2] = floor(psOptions->adfSrcWin[2] + 0.5);
            psOptions->adfSrcWin[3] = floor(psOptions->adfSrcWin[3] + 0.5);
        }

        /*if( !bQuiet )
            fprintf( stdout,
                     "Computed -srcwin %g %g %g %g from projected window.\n",
                     adfSrcWin[0],
                     adfSrcWin[1],
                     adfSrcWin[2],
                     adfSrcWin[3] ); */
    }

/* -------------------------------------------------------------------- */
/*      Verify source window dimensions.                                */
/* -------------------------------------------------------------------- */
    if( psOptions->adfSrcWin[2] <= 0 || psOptions->adfSrcWin[3] <= 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                 "Error: %s-srcwin %g %g %g %g has negative width and/or height.",
                 ( psOptions->dfULX != 0.0 || psOptions->dfULY != 0.0 || psOptions->dfLRX != 0.0 || psOptions->dfLRY != 0.0 ) ? "Computed " : "",
                 psOptions->adfSrcWin[0],
                 psOptions->adfSrcWin[1],
                 psOptions->adfSrcWin[2],
                 psOptions->adfSrcWin[3] );
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Verify source window dimensions.                                */
/* -------------------------------------------------------------------- */
    else if( psOptions->adfSrcWin[0] <= -1 || psOptions->adfSrcWin[1] <= -1
        || psOptions->adfSrcWin[0] + psOptions->adfSrcWin[2] >= GDALGetRasterXSize(hSrcDataset) + 1
        || psOptions->adfSrcWin[1] + psOptions->adfSrcWin[3] >= GDALGetRasterYSize(hSrcDataset) + 1 )
    {
        const bool bCompletelyOutside =
            psOptions->adfSrcWin[0] + psOptions->adfSrcWin[2] <= 0 ||
            psOptions->adfSrcWin[1] + psOptions->adfSrcWin[3] <= 0 ||
            psOptions->adfSrcWin[0] >= GDALGetRasterXSize(hSrcDataset) ||
            psOptions->adfSrcWin[1] >= GDALGetRasterYSize(hSrcDataset);
        const bool bIsError =
            psOptions->bErrorOnPartiallyOutside ||
            (bCompletelyOutside && psOptions->bErrorOnCompletelyOutside);
        if( !psOptions->bQuiet || bIsError )
        {
            CPLErr eErr = bIsError ? CE_Failure : CE_Warning;

            CPLError( eErr, CPLE_AppDefined,
                 "%s-srcwin %g %g %g %g falls %s outside raster extent.%s",
                 ( psOptions->dfULX != 0.0 || psOptions->dfULY != 0.0 || psOptions->dfLRX != 0.0 || psOptions->dfLRY != 0.0 ) ? "Computed " : "",
                 psOptions->adfSrcWin[0],
                 psOptions->adfSrcWin[1],
                 psOptions->adfSrcWin[2],
                 psOptions->adfSrcWin[3],
                 bCompletelyOutside ? "completely" : "partially",
                 bIsError ? "" : " Going on however." );
        }
        if( bIsError )
        {
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    GDALDriverH hDriver = GDALGetDriverByName(psOptions->pszFormat);
    if( hDriver == NULL )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "Output driver `%s' not recognised.",
                  psOptions->pszFormat);
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

    char** papszDriverMD = GDALGetMetadata(hDriver, NULL);

    if( !CPLTestBool( CSLFetchNameValueDef(papszDriverMD,
                                           GDAL_DCAP_RASTER, "FALSE") ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s driver has no raster capabilities.",
                  psOptions->pszFormat );
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

    if( !CPLTestBool( CSLFetchNameValueDef(papszDriverMD,
                                          GDAL_DCAP_CREATE, "FALSE") ) &&
        !CPLTestBool( CSLFetchNameValueDef(papszDriverMD,
                                          GDAL_DCAP_CREATECOPY, "FALSE") ))
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s driver has no creation capabilities.",
                  psOptions->pszFormat );
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      The short form is to CreateCopy().  We use this if the input    */
/*      matches the whole dataset.  Eventually we should rewrite        */
/*      this entire program to use virtual datasets to construct a      */
/*      virtual input source to copy from.                              */
/* -------------------------------------------------------------------- */

    const bool bSpatialArrangementPreserved =
        psOptions->adfSrcWin[0] == 0 && psOptions->adfSrcWin[1] == 0 &&
        psOptions->adfSrcWin[2] == GDALGetRasterXSize(hSrcDataset) &&
        psOptions->adfSrcWin[3] == GDALGetRasterYSize(hSrcDataset) &&
        psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0 &&
        psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0 &&
        psOptions->dfXRes == 0.0;

    if( psOptions->eOutputType == GDT_Unknown
        && psOptions->nScaleRepeat == 0 && psOptions->nExponentRepeat == 0 && !psOptions->bUnscale
        && CSLCount(psOptions->papszMetadataOptions) == 0 && bAllBandsInOrder
        && psOptions->eMaskMode == MASK_AUTO
        && bSpatialArrangementPreserved
        && psOptions->nGCPCount == 0 && !bGotBounds
        && psOptions->pszOutputSRS == NULL && !psOptions->bSetNoData && !psOptions->bUnsetNoData
        && psOptions->nRGBExpand == 0 && !psOptions->bStats && !psOptions->bNoRAT )
    {

        // For gdal_translate_fuzzer
        if( psOptions->nLimitOutSize > 0 )
        {
            vsi_l_offset nRawOutSize =
               static_cast<vsi_l_offset>(GDALGetRasterXSize(hSrcDataset)) *
                GDALGetRasterYSize(hSrcDataset) *
                psOptions->nBandCount;
            if( psOptions->nBandCount )
            {
                nRawOutSize *= GDALGetDataTypeSizeBytes(
                    ((GDALDataset *) hSrcDataset)->GetRasterBand(1)->GetRasterDataType() );
            }
            if( nRawOutSize > static_cast<vsi_l_offset>(psOptions->nLimitOutSize) )
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                          "Attempt to create %dx%d dataset is above authorized limit.",
                          GDALGetRasterXSize(hSrcDataset),
                          GDALGetRasterYSize(hSrcDataset) );
                GDALTranslateOptionsFree(psOptions);
                return NULL;
            }
        }


        hOutDS = GDALCreateCopy( hDriver, pszDest, hSrcDataset,
                                 psOptions->bStrict, psOptions->papszCreateOptions,
                                 psOptions->pfnProgress, psOptions->pProgressData );
        hOutDS = GDALTranslateFlush(hOutDS);

        GDALTranslateOptionsFree(psOptions);
        return hOutDS;
    }

/* -------------------------------------------------------------------- */
/*      Establish some parameters.                                      */
/* -------------------------------------------------------------------- */
    int nOXSize = 0;
    int nOYSize = 0;

    double adfGeoTransform[6] = {};
    if( psOptions->dfXRes != 0.0 )
    {
        if( !(GDALGetGeoTransform( hSrcDataset, adfGeoTransform ) == CE_None &&
              psOptions->nGCPCount == 0 &&
              adfGeoTransform[2] == 0.0 && adfGeoTransform[4] == 0.0) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                     "The -tr option was used, but there's no geotransform or it is\n"
                     "rotated.  This configuration is not supported." );
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
        const double dfOXSize =
            psOptions->adfSrcWin[2] /
            psOptions->dfXRes * adfGeoTransform[1] + 0.5;
        const double dfOYSize =
            psOptions->adfSrcWin[3] /
            psOptions->dfYRes * fabs(adfGeoTransform[5]) + 0.5;
        if( dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize) ||
            dfOYSize < 1 || !GDALIsValueInRange<int>(dfOXSize) )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Invalid output size: %g x %g",
                     dfOXSize, dfOYSize);
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
        nOXSize = static_cast<int>(dfOXSize);
        nOYSize = static_cast<int>(dfOYSize);
    }
    else if( psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0 && psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0)
    {
        double dfOXSize = ceil(psOptions->adfSrcWin[2]-0.001);
        double dfOYSize = ceil(psOptions->adfSrcWin[3]-0.001);
        if( dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize) ||
            dfOYSize < 1 || !GDALIsValueInRange<int>(dfOXSize) )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Invalid output size: %g x %g",
                     dfOXSize, dfOYSize);
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
        nOXSize = static_cast<int>(dfOXSize);
        nOYSize = static_cast<int>(dfOYSize);
    }
    else
    {
        if( !(psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0) )
        {
            if(psOptions->nOXSizePixel != 0)
                nOXSize = psOptions->nOXSizePixel;
            else
            {
                const double dfOXSize =
                    psOptions->dfOXSizePct / 100 * psOptions->adfSrcWin[2];
                if( dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize) )
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                            "Invalid output width: %g",
                            dfOXSize);
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }
                nOXSize = static_cast<int>(dfOXSize);
            }
        }

        if( !(psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0) )
        {
            if(psOptions->nOYSizePixel != 0)
                nOYSize = psOptions->nOYSizePixel;
            else
            {
                const double dfOYSize =
                    psOptions->dfOYSizePct / 100 * psOptions->adfSrcWin[3];
                if( dfOYSize < 1 || !GDALIsValueInRange<int>(dfOYSize) )
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                            "Invalid output height: %g",
                            dfOYSize);
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }
                nOYSize = static_cast<int>(dfOYSize);
            }
        }

        if( psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0 )
        {
            double dfOXSize = (double)nOYSize * psOptions->adfSrcWin[2] / psOptions->adfSrcWin[3] + 0.5;
            if( dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                        "Invalid output width: %g",
                        dfOXSize);
                GDALTranslateOptionsFree(psOptions);
                return NULL;
            }
            nOXSize = static_cast<int>(dfOXSize);
        }
        else if( psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0 )
        {
            double dfOYSize = (double)nOXSize * psOptions->adfSrcWin[3] / psOptions->adfSrcWin[2] + 0.5;
            if( dfOYSize < 1 || !GDALIsValueInRange<int>(dfOYSize) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                        "Invalid output height: %g",
                        dfOYSize);
                GDALTranslateOptionsFree(psOptions);
                return NULL;
            }
            nOYSize = static_cast<int>(dfOYSize);
        }
    }

    if( nOXSize <= 0 || nOYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "Attempt to create %dx%d dataset is illegal.", nOXSize, nOYSize);
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

    // For gdal_translate_fuzzer
    if( psOptions->nLimitOutSize > 0 )
    {
        vsi_l_offset nRawOutSize = static_cast<vsi_l_offset>(nOXSize) * nOYSize *
                                psOptions->nBandCount;
        if( psOptions->nBandCount )
        {
            nRawOutSize *= GDALGetDataTypeSizeBytes(
                ((GDALDataset *) hSrcDataset)->GetRasterBand(1)->GetRasterDataType() );
        }
        if( nRawOutSize > static_cast<vsi_l_offset>(psOptions->nLimitOutSize) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "Attempt to create %dx%d dataset is above authorized limit.",
                      nOXSize, nOYSize);
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
    }


/* ==================================================================== */
/*      Create a virtual dataset.                                       */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Make a virtual clone.                                           */
/* -------------------------------------------------------------------- */
    VRTDataset *poVDS = (VRTDataset *) VRTCreate(nOXSize, nOYSize);

    if( psOptions->nGCPCount == 0 )
    {
        if( psOptions->pszOutputSRS != NULL )
        {
            poVDS->SetProjection( psOptions->pszOutputSRS );
        }
        else
        {
            pszProjection = GDALGetProjectionRef( hSrcDataset );
            if( pszProjection != NULL && strlen(pszProjection) > 0 )
                poVDS->SetProjection( pszProjection );
        }
    }

    if( bGotBounds )
    {
        adfGeoTransform[0] = psOptions->adfULLR[0];
        adfGeoTransform[1] = (psOptions->adfULLR[2] - psOptions->adfULLR[0]) / nOXSize;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = psOptions->adfULLR[1];
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = (psOptions->adfULLR[3] - psOptions->adfULLR[1]) / nOYSize;

        poVDS->SetGeoTransform( adfGeoTransform );
    }

    else if( GDALGetGeoTransform( hSrcDataset, adfGeoTransform ) == CE_None
        && psOptions->nGCPCount == 0 )
    {
        adfGeoTransform[0] += psOptions->adfSrcWin[0] * adfGeoTransform[1]
            + psOptions->adfSrcWin[1] * adfGeoTransform[2];
        adfGeoTransform[3] += psOptions->adfSrcWin[0] * adfGeoTransform[4]
            + psOptions->adfSrcWin[1] * adfGeoTransform[5];

        adfGeoTransform[1] *= psOptions->adfSrcWin[2] / (double) nOXSize;
        adfGeoTransform[2] *= psOptions->adfSrcWin[3] / (double) nOYSize;
        adfGeoTransform[4] *= psOptions->adfSrcWin[2] / (double) nOXSize;
        adfGeoTransform[5] *= psOptions->adfSrcWin[3] / (double) nOYSize;

        if( psOptions->dfXRes != 0.0 )
        {
            adfGeoTransform[1] = psOptions->dfXRes;
            adfGeoTransform[5] = (adfGeoTransform[5] > 0) ? psOptions->dfYRes : -psOptions->dfYRes;
        }

        poVDS->SetGeoTransform( adfGeoTransform );
    }

    if( psOptions->nGCPCount != 0 )
    {
        const char *pszGCPProjection = psOptions->pszOutputSRS;

        if( pszGCPProjection == NULL )
            pszGCPProjection = GDALGetGCPProjection( hSrcDataset );
        if( pszGCPProjection == NULL )
            pszGCPProjection = "";

        poVDS->SetGCPs( psOptions->nGCPCount, psOptions->pasGCPs, pszGCPProjection );
    }

    else if( GDALGetGCPCount( hSrcDataset ) > 0 )
    {
        const int nGCPs = GDALGetGCPCount(hSrcDataset);

        GDAL_GCP *pasGCPs = GDALDuplicateGCPs(nGCPs, GDALGetGCPs(hSrcDataset));

        for( int i = 0; i < nGCPs; i++ )
        {
            pasGCPs[i].dfGCPPixel -= psOptions->adfSrcWin[0];
            pasGCPs[i].dfGCPLine  -= psOptions->adfSrcWin[1];
            pasGCPs[i].dfGCPPixel *= (nOXSize / (double) psOptions->adfSrcWin[2] );
            pasGCPs[i].dfGCPLine  *= (nOYSize / (double) psOptions->adfSrcWin[3] );
        }

        poVDS->SetGCPs( nGCPs, pasGCPs,
                        GDALGetGCPProjection( hSrcDataset ) );

        GDALDeinitGCPs( nGCPs, pasGCPs );
        CPLFree( pasGCPs );
    }

/* -------------------------------------------------------------------- */
/*      To make the VRT to look less awkward (but this is optional      */
/*      in fact), avoid negative values.                                */
/* -------------------------------------------------------------------- */
    double adfDstWin[4] =
        {0.0, 0.0, static_cast<double>(nOXSize), static_cast<double>(nOYSize)};

    FixSrcDstWindow( psOptions->adfSrcWin, adfDstWin,
                     GDALGetRasterXSize(hSrcDataset),
                     GDALGetRasterYSize(hSrcDataset) );

/* -------------------------------------------------------------------- */
/*      Transfer generally applicable metadata.                         */
/* -------------------------------------------------------------------- */
    GDALDataset* poSrcDS = reinterpret_cast<GDALDataset*>(hSrcDataset);
    char** papszMetadata = CSLDuplicate(poSrcDS->GetMetadata());
    if ( psOptions->nScaleRepeat > 0 || psOptions->bUnscale || psOptions->eOutputType != GDT_Unknown )
    {
        /* Remove TIFFTAG_MINSAMPLEVALUE and TIFFTAG_MAXSAMPLEVALUE */
        /* if the data range may change because of options */
        char** papszIter = papszMetadata;
        while(papszIter && *papszIter)
        {
            if (STARTS_WITH_CI(*papszIter, "TIFFTAG_MINSAMPLEVALUE=") ||
                STARTS_WITH_CI(*papszIter, "TIFFTAG_MAXSAMPLEVALUE="))
            {
                CPLFree(*papszIter);
                memmove(papszIter, papszIter+1, sizeof(char*) * (CSLCount(papszIter+1)+1));
            }
            else
                papszIter++;
        }
    }
    poVDS->SetMetadata( papszMetadata );
    CSLDestroy( papszMetadata );
    AttachMetadata( (GDALDatasetH) poVDS, psOptions->papszMetadataOptions );

    const char* pszInterleave = GDALGetMetadataItem(hSrcDataset, "INTERLEAVE", "IMAGE_STRUCTURE");
    if (pszInterleave)
        poVDS->SetMetadataItem("INTERLEAVE", pszInterleave, "IMAGE_STRUCTURE");

    /* ISIS3 -> ISIS3 special case */
    if( EQUAL(psOptions->pszFormat, "ISIS3") )
    {
        char** papszMD_ISIS3 = poSrcDS->GetMetadata("json:ISIS3");
        if( papszMD_ISIS3 != NULL)
            poVDS->SetMetadata( papszMD_ISIS3, "json:ISIS3" );
    }

/* -------------------------------------------------------------------- */
/*      Transfer metadata that remains valid if the spatial             */
/*      arrangement of the data is unaltered.                           */
/* -------------------------------------------------------------------- */
    if( bSpatialArrangementPreserved )
    {
        char **papszMD = poSrcDS->GetMetadata("RPC");
        if( papszMD != NULL )
            poVDS->SetMetadata( papszMD, "RPC" );

        papszMD = poSrcDS->GetMetadata("GEOLOCATION");
        if( papszMD != NULL )
            poVDS->SetMetadata( papszMD, "GEOLOCATION" );
    }
    else
    {
        char **papszMD = poSrcDS->GetMetadata("RPC");
        if( papszMD != NULL )
        {
            papszMD = CSLDuplicate(papszMD);

            double dfSAMP_OFF = CPLAtof(CSLFetchNameValueDef(papszMD, "SAMP_OFF", "0"));
            double dfLINE_OFF = CPLAtof(CSLFetchNameValueDef(papszMD, "LINE_OFF", "0"));
            double dfSAMP_SCALE = CPLAtof(CSLFetchNameValueDef(papszMD, "SAMP_SCALE", "1"));
            double dfLINE_SCALE = CPLAtof(CSLFetchNameValueDef(papszMD, "LINE_SCALE", "1"));

            dfSAMP_OFF -= psOptions->adfSrcWin[0];
            dfLINE_OFF -= psOptions->adfSrcWin[1];
            dfSAMP_OFF *= (nOXSize / (double) psOptions->adfSrcWin[2] );
            dfLINE_OFF *= (nOYSize / (double) psOptions->adfSrcWin[3] );
            dfSAMP_SCALE *= (nOXSize / (double) psOptions->adfSrcWin[2] );
            dfLINE_SCALE *= (nOYSize / (double) psOptions->adfSrcWin[3] );

            CPLString osField;
            osField.Printf( "%.15g", dfLINE_OFF );
            papszMD = CSLSetNameValue( papszMD, "LINE_OFF", osField );

            osField.Printf( "%.15g", dfSAMP_OFF );
            papszMD = CSLSetNameValue( papszMD, "SAMP_OFF", osField );

            osField.Printf( "%.15g", dfLINE_SCALE );
            papszMD = CSLSetNameValue( papszMD, "LINE_SCALE", osField );

            osField.Printf( "%.15g", dfSAMP_SCALE );
            papszMD = CSLSetNameValue( papszMD, "SAMP_SCALE", osField );

            poVDS->SetMetadata( papszMD, "RPC" );
            CSLDestroy(papszMD);
        }
    }

    const int nSrcBandCount = psOptions->nBandCount;

    if (psOptions->nRGBExpand != 0)
    {
        GDALRasterBand *poSrcBand =
            ((GDALDataset *) hSrcDataset)->
                GetRasterBand(std::abs(psOptions->panBandList[0]));
        if (psOptions->panBandList[0] < 0)
            poSrcBand = poSrcBand->GetMaskBand();
        GDALColorTable* poColorTable = poSrcBand->GetColorTable();
        if (poColorTable == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error : band %d has no color table",
                     std::abs(psOptions->panBandList[0]));
            GDALClose(poVDS);
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }

        /* Check that the color table only contains gray levels */
        /* when using -expand gray */
        if (psOptions->nRGBExpand == 1)
        {
            int nColorCount = poColorTable->GetColorEntryCount();
            for( int nColor = 0; nColor < nColorCount; nColor++ )
            {
                const GDALColorEntry* poEntry = poColorTable->GetColorEntry(nColor);
                if (poEntry->c1 != poEntry->c2 || poEntry->c1 != poEntry->c3)
                {
                    CPLError( CE_Warning, CPLE_AppDefined, "Warning : color table contains non gray levels colors");
                    break;
                }
            }
        }

        if (psOptions->nBandCount == 1)
        {
            psOptions->nBandCount = psOptions->nRGBExpand;
        }
        else if (psOptions->nBandCount == 2 && (psOptions->nRGBExpand == 3 || psOptions->nRGBExpand == 4))
        {
            psOptions->nBandCount = psOptions->nRGBExpand;
        }
        else
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "Error : invalid use of -expand option.");
            GDALClose(poVDS);
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
    }

    // Can be set to TRUE in the band loop too
    int bFilterOutStatsMetadata =
        (psOptions->nScaleRepeat > 0 || psOptions->bUnscale || !bSpatialArrangementPreserved || psOptions->nRGBExpand != 0);

/* ==================================================================== */
/*      Process all bands.                                              */
/* ==================================================================== */
    for( int i = 0; i < psOptions->nBandCount; i++ )
    {
        int nComponent = 0;
        int nSrcBand = 0;

        if (psOptions->nRGBExpand != 0)
        {
            if (nSrcBandCount == 2 && psOptions->nRGBExpand == 4 && i == 3)
                nSrcBand = psOptions->panBandList[1];
            else
            {
                nSrcBand = psOptions->panBandList[0];
                nComponent = i + 1;
            }
        }
        else
        {
            nSrcBand = psOptions->panBandList[i];
        }

        GDALRasterBand *poSrcBand =
            ((GDALDataset *) hSrcDataset)->GetRasterBand(std::abs(nSrcBand));

/* -------------------------------------------------------------------- */
/*      Select output data type to match source.                        */
/* -------------------------------------------------------------------- */
        GDALRasterBand* poRealSrcBand =
                (nSrcBand < 0) ? poSrcBand->GetMaskBand(): poSrcBand;
        GDALDataType eBandType;
        if( psOptions->eOutputType == GDT_Unknown )
        {
            eBandType = poRealSrcBand->GetRasterDataType();
        }
        else
        {
            eBandType = psOptions->eOutputType;

            // Check that we can copy existing statistics
            GDALDataType eSrcBandType = poRealSrcBand->GetRasterDataType();
            const char* pszMin = poRealSrcBand->GetMetadataItem("STATISTICS_MINIMUM");
            const char* pszMax = poRealSrcBand->GetMetadataItem("STATISTICS_MAXIMUM");
            if( !bFilterOutStatsMetadata && eBandType != eSrcBandType &&
                pszMin != NULL && pszMax != NULL )
            {
                const bool bSrcIsInteger =
                    eSrcBandType == GDT_Byte ||
                    eSrcBandType == GDT_Int16 ||
                    eSrcBandType == GDT_UInt16 ||
                    eSrcBandType == GDT_Int32 ||
                    eSrcBandType == GDT_UInt32;
                const bool bDstIsInteger =
                    eBandType == GDT_Byte ||
                    eBandType == GDT_Int16 ||
                    eBandType == GDT_UInt16 ||
                    eBandType == GDT_Int32 ||
                    eBandType == GDT_UInt32;
                if( bSrcIsInteger && bDstIsInteger )
                {
                    GInt32 nDstMin = 0;
                    GUInt32 nDstMax = 0;
                    switch( eBandType )
                    {
                        case GDT_Byte:
                            nDstMin = 0;
                            nDstMax = 255;
                            break;
                        case GDT_UInt16:
                            nDstMin = 0;
                            nDstMax = 65535;
                            break;
                        case GDT_Int16:
                            nDstMin = -32768;
                            nDstMax = 32767;
                            break;
                        case GDT_UInt32:
                            nDstMin = 0;
                            nDstMax = 0xFFFFFFFFU;
                            break;
                        case GDT_Int32:
                            nDstMin = 0x80000000;
                            nDstMax = 0x7FFFFFFF;
                            break;
                        default:
                            CPLAssert(false);
                            break;
                    }

                    GInt32 nMin = atoi(pszMin);
                    GUInt32 nMax = (GUInt32)strtoul(pszMax, NULL, 10);
                    if( nMin < nDstMin || nMax > nDstMax )
                        bFilterOutStatsMetadata = TRUE;
                }
                // Float64 is large enough to hold all integer <= 32 bit or float32 values
                // there might be other OK cases, but ere on safe side for now
                else if( !((bSrcIsInteger || eSrcBandType == GDT_Float32) && eBandType == GDT_Float64) )
                {
                    bFilterOutStatsMetadata = TRUE;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Create this band.                                               */
/* -------------------------------------------------------------------- */
        poVDS->AddBand( eBandType, NULL );
        VRTSourcedRasterBand *poVRTBand =
            (VRTSourcedRasterBand *) poVDS->GetRasterBand( i+1 );
        if (nSrcBand < 0)
        {
            poVRTBand->AddMaskBandSource(poSrcBand,
                                         psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                                         psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                                         adfDstWin[0], adfDstWin[1],
                                         adfDstWin[2], adfDstWin[3]);
            continue;
        }

        // Preserve NBITS if no option change values
        const char* pszNBits = poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
        if( pszNBits && psOptions->nRGBExpand == 0 && psOptions->nScaleRepeat == 0 &&
            !psOptions->bUnscale && psOptions->eOutputType == GDT_Unknown && psOptions->pszResampling == NULL )
        {
            poVRTBand->SetMetadataItem("NBITS", pszNBits, "IMAGE_STRUCTURE");
        }

        // Preserve PIXELTYPE if no option change values
        const char* pszPixelType = poSrcBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        if( pszPixelType && psOptions->nRGBExpand == 0 && psOptions->nScaleRepeat == 0 &&
            !psOptions->bUnscale && psOptions->eOutputType == GDT_Unknown && psOptions->pszResampling == NULL )
        {
            poVRTBand->SetMetadataItem("PIXELTYPE", pszPixelType, "IMAGE_STRUCTURE");
        }

/* -------------------------------------------------------------------- */
/*      Do we need to collect scaling information?                      */
/* -------------------------------------------------------------------- */
        double dfScale = 1.0;
        double dfOffset = 0.0;
        int bScale = FALSE;
        bool bHaveScaleSrc = false;
        double dfScaleSrcMin = 0.0;
        double dfScaleSrcMax = 0.0;
        double dfScaleDstMin = 0.0;
        double dfScaleDstMax = 0.0;
        bool bExponentScaling = false;
        double dfExponent = 0.0;

        // TODO(schwehr): Is bScale a bool?
        if( i < psOptions->nScaleRepeat && psOptions->pasScaleParams[i].bScale )
        {
            bScale = psOptions->pasScaleParams[i].bScale;
            bHaveScaleSrc = psOptions->pasScaleParams[i].bHaveScaleSrc;
            dfScaleSrcMin = psOptions->pasScaleParams[i].dfScaleSrcMin;
            dfScaleSrcMax = psOptions->pasScaleParams[i].dfScaleSrcMax;
            dfScaleDstMin = psOptions->pasScaleParams[i].dfScaleDstMin;
            dfScaleDstMax = psOptions->pasScaleParams[i].dfScaleDstMax;
        }
        else if( psOptions->nScaleRepeat == 1 && !psOptions->bHasUsedExplicitScaleBand )
        {
            bScale = psOptions->pasScaleParams[0].bScale;
            bHaveScaleSrc = psOptions->pasScaleParams[0].bHaveScaleSrc;
            dfScaleSrcMin = psOptions->pasScaleParams[0].dfScaleSrcMin;
            dfScaleSrcMax = psOptions->pasScaleParams[0].dfScaleSrcMax;
            dfScaleDstMin = psOptions->pasScaleParams[0].dfScaleDstMin;
            dfScaleDstMax = psOptions->pasScaleParams[0].dfScaleDstMax;
        }

        if( i < psOptions->nExponentRepeat && psOptions->padfExponent[i] != 0.0 )
        {
            bExponentScaling = TRUE;
            dfExponent = psOptions->padfExponent[i];
        }
        else if( psOptions->nExponentRepeat == 1 && !psOptions->bHasUsedExplicitExponentBand )
        {
            bExponentScaling = TRUE;
            dfExponent = psOptions->padfExponent[0];
        }

        if( bExponentScaling && !bScale )
        {
            CPLError( CE_Failure, CPLE_IllegalArg, "For band %d, -scale should be specified when -exponent is specified.", i + 1);
            if(pbUsageError)
                *pbUsageError = TRUE;
            GDALTranslateOptionsFree(psOptions);
            delete poVDS;
            return NULL;
        }

        if( bScale && !bHaveScaleSrc )
        {
            double adfCMinMax[2] = {};
            GDALComputeRasterMinMax( poSrcBand, TRUE, adfCMinMax );
            dfScaleSrcMin = adfCMinMax[0];
            dfScaleSrcMax = adfCMinMax[1];
        }

        if( bScale )
        {
            /* To avoid a divide by zero */
            if( dfScaleSrcMax == dfScaleSrcMin )
                dfScaleSrcMax += 0.1;

            // Can still occur for very big values
            if( dfScaleSrcMax == dfScaleSrcMin )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "-scale cannot be applied due to source "
                          "minimum and maximum being equal" );
                GDALTranslateOptionsFree(psOptions);
                delete poVDS;
                return NULL;
            }

            if( !bExponentScaling )
            {
                dfScale = (dfScaleDstMax - dfScaleDstMin)
                    / (dfScaleSrcMax - dfScaleSrcMin);
                dfOffset = -1 * dfScaleSrcMin * dfScale + dfScaleDstMin;
            }
        }

        if( psOptions->bUnscale )
        {
            dfScale = poSrcBand->GetScale();
            dfOffset = poSrcBand->GetOffset();
        }

/* -------------------------------------------------------------------- */
/*      Create a simple or complex data source depending on the         */
/*      translation type required.                                      */
/* -------------------------------------------------------------------- */
        VRTSimpleSource* poSimpleSource;
        if( psOptions->bUnscale || bScale || (psOptions->nRGBExpand != 0 && i < psOptions->nRGBExpand) )
        {
            VRTComplexSource* poSource = new VRTComplexSource();

        /* -------------------------------------------------------------------- */
        /*      Set complex parameters.                                         */
        /* -------------------------------------------------------------------- */

            if( dfOffset != 0.0 || dfScale != 1.0 )
            {
                poSource->SetLinearScaling(dfOffset, dfScale);
            }
            else if( bExponentScaling )
            {
                poSource->SetPowerScaling(dfExponent,
                                          dfScaleSrcMin,
                                          dfScaleSrcMax,
                                          dfScaleDstMin,
                                          dfScaleDstMax);
            }

            poSource->SetColorTableComponent(nComponent);

            int bSuccess;
            double dfNoData = poSrcBand->GetNoDataValue( &bSuccess );
            if ( bSuccess )
            {
                poSource->SetNoDataValue(dfNoData);
            }

            poSimpleSource = poSource;
        }
        else
        {
            poSimpleSource = new VRTSimpleSource();
        }

        poSimpleSource->SetResampling(psOptions->pszResampling);
        poVRTBand->ConfigureSource( poSimpleSource,
                                    poSrcBand,
                                    FALSE,
                                    psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                                    psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                                    adfDstWin[0], adfDstWin[1],
                                    adfDstWin[2], adfDstWin[3] );

        poVRTBand->AddSource( poSimpleSource );

/* -------------------------------------------------------------------- */
/*      In case of color table translate, we only set the color         */
/*      interpretation other info copied by CopyBandInfo are            */
/*      not relevant in RGB expansion.                                  */
/* -------------------------------------------------------------------- */
        if (psOptions->nRGBExpand == 1)
        {
            poVRTBand->SetColorInterpretation( GCI_GrayIndex );
        }
        else if (psOptions->nRGBExpand != 0 && i < psOptions->nRGBExpand)
        {
            poVRTBand->SetColorInterpretation( (GDALColorInterp) (GCI_RedBand + i) );
        }

/* -------------------------------------------------------------------- */
/*      copy over some other information of interest.                   */
/* -------------------------------------------------------------------- */
        else
        {
            CopyBandInfo( poSrcBand, poVRTBand,
                          !psOptions->bStats && !bFilterOutStatsMetadata,
                          !psOptions->bUnscale,
                          !psOptions->bSetNoData && !psOptions->bUnsetNoData );
        }

/* -------------------------------------------------------------------- */
/*      Set a forcible nodata value?                                    */
/* -------------------------------------------------------------------- */
        if( psOptions->bSetNoData )
        {
            bool bSignedByte = false;
            pszPixelType = CSLFetchNameValue( psOptions->papszCreateOptions, "PIXELTYPE" );
            if( pszPixelType == NULL )
            {
                pszPixelType = poVRTBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
            }
            if( pszPixelType != NULL && EQUAL(pszPixelType, "SIGNEDBYTE") )
                bSignedByte = true;
            int bClamped = FALSE;
            int bRounded = FALSE;
            double dfVal;
            if( eBandType == GDT_Float32 && CPLIsInf(psOptions->dfNoDataReal) )
            {
                dfVal = std::numeric_limits<float>::infinity();
                if( psOptions->dfNoDataReal < 0 )
                    dfVal = -dfVal;
            }
            else if( bSignedByte )
            {
                if( psOptions->dfNoDataReal < -128 )
                {
                    dfVal = -128;
                    bClamped = TRUE;
                }
                else if( psOptions->dfNoDataReal > 127 )
                {
                    dfVal = 127;
                    bClamped = TRUE;
                }
                else
                {
                    dfVal = static_cast<int>(floor(psOptions->dfNoDataReal + 0.5));
                    if( dfVal != psOptions->dfNoDataReal )
                        bRounded = TRUE;
                }
            }
            else
            {
                dfVal = GDALAdjustValueToDataType(eBandType,
                                                     psOptions->dfNoDataReal,
                                                     &bClamped, &bRounded );
            }

            if (bClamped)
            {
                CPLError( CE_Warning, CPLE_AppDefined, "for band %d, nodata value has been clamped "
                       "to %.0f, the original value being out of range.",
                       i + 1, dfVal);
            }
            else if(bRounded)
            {
                CPLError( CE_Warning, CPLE_AppDefined, "for band %d, nodata value has been rounded "
                       "to %.0f, %s being an integer datatype.",
                       i + 1, dfVal,
                       GDALGetDataTypeName(eBandType));
            }

            poVRTBand->SetNoDataValue( dfVal );
        }

        if (psOptions->eMaskMode == MASK_AUTO &&
            (GDALGetMaskFlags(GDALGetRasterBand(hSrcDataset, 1)) & GMF_PER_DATASET) == 0 &&
            (poSrcBand->GetMaskFlags() & (GMF_ALL_VALID | GMF_NODATA)) == 0)
        {
            if (poVRTBand->CreateMaskBand(poSrcBand->GetMaskFlags()) == CE_None)
            {
                VRTSourcedRasterBand* hMaskVRTBand =
                    (VRTSourcedRasterBand*)poVRTBand->GetMaskBand();
                hMaskVRTBand->AddMaskBandSource(poSrcBand,
                                        psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                                        psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                                        adfDstWin[0], adfDstWin[1],
                                        adfDstWin[2], adfDstWin[3] );
            }
        }
    }

    if (psOptions->eMaskMode == MASK_USER)
    {
        GDALRasterBand *poSrcBand =
            (GDALRasterBand*)GDALGetRasterBand(hSrcDataset,
                                               std::abs(psOptions->nMaskBand));
        if (poSrcBand && poVDS->CreateMaskBand(GMF_PER_DATASET) == CE_None)
        {
            VRTSourcedRasterBand* hMaskVRTBand = (VRTSourcedRasterBand*)
                GDALGetMaskBand(GDALGetRasterBand((GDALDatasetH)poVDS, 1));
            if (psOptions->nMaskBand > 0)
                hMaskVRTBand->AddSimpleSource(poSrcBand,
                                        psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                                        psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                                        adfDstWin[0], adfDstWin[1],
                                        adfDstWin[2], adfDstWin[3] );
            else
                hMaskVRTBand->AddMaskBandSource(poSrcBand,
                                        psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                                        psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                                        adfDstWin[0], adfDstWin[1],
                                        adfDstWin[2], adfDstWin[3] );
        }
    }
    else
    if (psOptions->eMaskMode == MASK_AUTO && nSrcBandCount > 0 &&
        GDALGetMaskFlags(GDALGetRasterBand(hSrcDataset, 1)) == GMF_PER_DATASET)
    {
        if (poVDS->CreateMaskBand(GMF_PER_DATASET) == CE_None)
        {
            VRTSourcedRasterBand* hMaskVRTBand = (VRTSourcedRasterBand*)
                GDALGetMaskBand(GDALGetRasterBand((GDALDatasetH)poVDS, 1));
            hMaskVRTBand->AddMaskBandSource((GDALRasterBand*)GDALGetRasterBand(hSrcDataset, 1),
                                        psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                                        psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                                        adfDstWin[0], adfDstWin[1],
                                        adfDstWin[2], adfDstWin[3] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Compute stats if required.                                      */
/* -------------------------------------------------------------------- */
    if (psOptions->bStats)
    {
        for( int i = 0; i < poVDS->GetRasterCount(); i++ )
        {
            double dfMin, dfMax, dfMean, dfStdDev;
            poVDS->GetRasterBand(i+1)->ComputeStatistics( psOptions->bApproxStats,
                    &dfMin, &dfMax, &dfMean, &dfStdDev, GDALDummyProgress, NULL );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write to the output file using CopyCreate().                    */
/* -------------------------------------------------------------------- */
    if( EQUAL(psOptions->pszFormat, "VRT") &&
        psOptions->papszCreateOptions == NULL )
    {
        poVDS->SetDescription(pszDest);
        hOutDS = (GDALDatasetH) poVDS ;
        if( !EQUAL(pszDest, "") )
        {
            hOutDS = GDALTranslateFlush(hOutDS);
        }
    }
    else
    {
        hOutDS = GDALCreateCopy( hDriver, pszDest, (GDALDatasetH) poVDS,
                                psOptions->bStrict, psOptions->papszCreateOptions,
                                psOptions->pfnProgress, psOptions->pProgressData );
        hOutDS = GDALTranslateFlush(hOutDS);

        GDALClose(poVDS);
    }

    GDALTranslateOptionsFree(psOptions);
    return hOutDS;
}

/************************************************************************/
/*                           AttachMetadata()                           */
/************************************************************************/

static void AttachMetadata( GDALDatasetH hDS, char **papszMetadataOptions )

{
    const int nCount = CSLCount(papszMetadataOptions);

    for( int i = 0; i < nCount; i++ )
    {
        char *pszKey = NULL;
        const char *pszValue =
            CPLParseNameValue(papszMetadataOptions[i], &pszKey);
        if( pszKey && pszValue )
        {
            GDALSetMetadataItem(hDS,pszKey,pszValue,NULL);
        }
        CPLFree( pszKey );
    }
}

/************************************************************************/
/*                           CopyBandInfo()                            */
/************************************************************************/

/* A bit of a clone of VRTRasterBand::CopyCommonInfoFrom(), but we need */
/* more and more custom behaviour in the context of gdal_translate ... */

static void CopyBandInfo( GDALRasterBand * poSrcBand, GDALRasterBand * poDstBand,
                          int bCanCopyStatsMetadata, int bCopyScale, int bCopyNoData )

{

    if (bCanCopyStatsMetadata)
    {
        poDstBand->SetMetadata( poSrcBand->GetMetadata() );
    }
    else
    {
        char** papszMetadata = poSrcBand->GetMetadata();
        char** papszMetadataNew = NULL;
        for( int i = 0; papszMetadata != NULL && papszMetadata[i] != NULL; i++ )
        {
            if (!STARTS_WITH(papszMetadata[i], "STATISTICS_"))
                papszMetadataNew = CSLAddString(papszMetadataNew, papszMetadata[i]);
        }
        poDstBand->SetMetadata( papszMetadataNew );
        CSLDestroy(papszMetadataNew);
    }

    poDstBand->SetColorTable( poSrcBand->GetColorTable() );
    poDstBand->SetColorInterpretation(poSrcBand->GetColorInterpretation());
    if( strlen(poSrcBand->GetDescription()) > 0 )
        poDstBand->SetDescription( poSrcBand->GetDescription() );

    if (bCopyNoData)
    {
        int bSuccess = FALSE;
        double dfNoData = poSrcBand->GetNoDataValue(&bSuccess);
        if( bSuccess )
            poDstBand->SetNoDataValue( dfNoData );
    }

    if (bCopyScale)
    {
        poDstBand->SetOffset( poSrcBand->GetOffset() );
        poDstBand->SetScale( poSrcBand->GetScale() );
    }

    poDstBand->SetCategoryNames( poSrcBand->GetCategoryNames() );

    // Copy unit only if the range of pixel values is not modified
    if( bCanCopyStatsMetadata && bCopyScale && !EQUAL(poSrcBand->GetUnitType(),"") )
        poDstBand->SetUnitType( poSrcBand->GetUnitType() );
}

/************************************************************************/
/*                            ArgIsNumeric()                            */
/************************************************************************/

int ArgIsNumeric( const char *pszArg )

{
    return CPLGetValueType(pszArg) != CPL_VALUE_STRING;
}

/************************************************************************/
/*                             GDALTranslateOptionsNew()                */
/************************************************************************/

/**
 * Allocates a GDALTranslateOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="gdal_translate.html">gdal_translate</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be NULL),
 *                           otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALTranslateOptionsForBinaryNew() prior to this function. Will be
 *                           filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALTranslateOptions struct. Must be freed with GDALTranslateOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALTranslateOptions *GDALTranslateOptionsNew(char** papszArgv, GDALTranslateOptionsForBinary* psOptionsForBinary)
{
    GDALTranslateOptions *psOptions = static_cast<GDALTranslateOptions *>(
        CPLCalloc( 1, sizeof(GDALTranslateOptions)));

    psOptions->pszFormat = CPLStrdup("GTiff");
    psOptions->bQuiet = true;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = NULL;
    psOptions->eOutputType = GDT_Unknown;
    psOptions->eMaskMode = MASK_AUTO;
    psOptions->nBandCount = 0;
    psOptions->panBandList = NULL;
    psOptions->nOXSizePixel = 0;
    psOptions->nOYSizePixel = 0;
    psOptions->dfOXSizePct = 0.0;
    psOptions->dfOYSizePct = 0.0;
    psOptions->adfSrcWin[0] = 0;
    psOptions->adfSrcWin[1] = 0;
    psOptions->adfSrcWin[2] = 0;
    psOptions->adfSrcWin[3] = 0;
    psOptions->bStrict = false;
    psOptions->bUnscale = false;
    psOptions->nScaleRepeat = 0;
    psOptions->pasScaleParams = NULL;
    psOptions->bHasUsedExplicitScaleBand = false;
    psOptions->nExponentRepeat = 0;
    psOptions->padfExponent = NULL;
    psOptions->bHasUsedExplicitExponentBand = false;
    psOptions->dfULX = 0.0;
    psOptions->dfULY = 0.0;
    psOptions->dfLRX = 0.0;
    psOptions->dfLRY = 0.0;
    psOptions->pszOutputSRS = NULL;
    psOptions->nGCPCount = 0;
    psOptions->pasGCPs = NULL;
    psOptions->adfULLR[0] = 0;
    psOptions->adfULLR[1] = 0;
    psOptions->adfULLR[2] = 0;
    psOptions->adfULLR[3] = 0;
    psOptions->bSetNoData = false;
    psOptions->bUnsetNoData = false;
    psOptions->dfNoDataReal = 0.0;
    psOptions->nRGBExpand = 0;
    psOptions->nMaskBand = 0;
    psOptions->bStats = false;
    psOptions->bApproxStats = false;
    psOptions->bErrorOnPartiallyOutside = false;
    psOptions->bErrorOnCompletelyOutside = false;
    psOptions->bNoRAT = false;
    psOptions->pszResampling = NULL;
    psOptions->dfXRes = 0.0;
    psOptions->dfYRes = 0.0;
    psOptions->pszProjSRS = NULL;
    psOptions->nLimitOutSize = 0;

    bool bParsedMaskArgument = false;
    bool bOutsideExplicitlySet = false;
    bool bGotSourceFilename = false;
    bool bGotDestFilename = false;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);
    for( int i = 0; papszArgv != NULL && i < argc; i++ )
    {
        if( i < argc-1 && (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) )
        {
            ++i;
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[i]);
            if( psOptionsForBinary )
            {
                psOptionsForBinary->bFormatExplicitlySet = TRUE;
            }
        }

        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = true;
        }

        else if( EQUAL(papszArgv[i],"-ot") && papszArgv[i+1] )
        {
            int iType;

            for( iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName((GDALDataType)iType) != NULL
                    && EQUAL(GDALGetDataTypeName((GDALDataType)iType),
                             papszArgv[i+1]) )
                {
                    psOptions->eOutputType = (GDALDataType) iType;
                }
            }

            if( psOptions->eOutputType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unknown output pixel type: %s.", papszArgv[i+1] );
                GDALTranslateOptionsFree(psOptions);
                return NULL;
            }
            i++;
        }
        else if( EQUAL(papszArgv[i],"-b") && papszArgv[i+1] )
        {
            const char* pszBand = papszArgv[i+1];
            bool bMask = false;
            if (EQUAL(pszBand, "mask"))
                pszBand = "mask,1";
            if (STARTS_WITH_CI(pszBand, "mask,"))
            {
                bMask = true;
                pszBand += 5;
                /* If we use the source mask band as a regular band */
                /* don't create a target mask band by default */
                if( !bParsedMaskArgument )
                    psOptions->eMaskMode = MASK_DISABLED;
            }
            const int nBand = atoi(pszBand);
            if( nBand < 1 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,"Unrecognizable band number (%s).", papszArgv[i+1] );
                GDALTranslateOptionsFree(psOptions);
                return NULL;
            }
            i++;

            psOptions->nBandCount++;
            psOptions->panBandList = static_cast<int *>(
                CPLRealloc(psOptions->panBandList,
                           sizeof(int) * psOptions->nBandCount));
            psOptions->panBandList[psOptions->nBandCount-1] = nBand;
            if (bMask)
                psOptions->panBandList[psOptions->nBandCount-1] *= -1;
        }
        else if( EQUAL(papszArgv[i],"-mask") &&  papszArgv[i+1] )
        {
            bParsedMaskArgument = true;
            const char* pszBand = papszArgv[i+1];
            if (EQUAL(pszBand, "none"))
            {
                psOptions->eMaskMode = MASK_DISABLED;
            }
            else if (EQUAL(pszBand, "auto"))
            {
                psOptions->eMaskMode = MASK_AUTO;
            }
            else
            {
                bool bMask = false;
                if (EQUAL(pszBand, "mask"))
                    pszBand = "mask,1";
                if (STARTS_WITH_CI(pszBand, "mask,"))
                {
                    bMask = true;
                    pszBand += 5;
                }
                const int nBand = atoi(pszBand);
                if( nBand < 1 )
                {
                    CPLError(CE_Failure, CPLE_NotSupported,"Unrecognizable band number (%s).", papszArgv[i+1] );
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }

                psOptions->eMaskMode = MASK_USER;
                psOptions->nMaskBand = nBand;
                if (bMask)
                    psOptions->nMaskBand *= -1;
            }
            i ++;
        }
        else if( EQUAL(papszArgv[i],"-not_strict")  )
            psOptions->bStrict = false;

        else if( EQUAL(papszArgv[i],"-strict")  )
            psOptions->bStrict = true;

        else if( EQUAL(papszArgv[i],"-sds")  )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bCopySubDatasets = TRUE;
        }
        else if( i + 4 < argc && EQUAL(papszArgv[i],"-gcp") )
        {
            char* endptr = NULL;
            /* -gcp pixel line easting northing [elev] */

            psOptions->nGCPCount++;
            psOptions->pasGCPs = static_cast<GDAL_GCP *>(
                CPLRealloc(psOptions->pasGCPs,
                           sizeof(GDAL_GCP) * psOptions->nGCPCount));
            GDALInitGCPs( 1, psOptions->pasGCPs + psOptions->nGCPCount - 1 );

            psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPPixel = CPLAtofM(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPLine = CPLAtofM(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPX = CPLAtofM(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPY = CPLAtofM(papszArgv[++i]);
            if( papszArgv[i+1] != NULL
                && (CPLStrtod(papszArgv[i+1], &endptr) != 0.0 || papszArgv[i+1][0] == '0') )
            {
                /* Check that last argument is really a number and not a filename */
                /* looking like a number (see ticket #863) */
                if (endptr && *endptr == 0)
                    psOptions->pasGCPs[psOptions->nGCPCount-1].dfGCPZ = CPLAtofM(papszArgv[++i]);
            }

            /* should set id and info? */
        }

        else if( EQUAL(papszArgv[i],"-a_nodata") && papszArgv[i+1] )
        {
            if (EQUAL(papszArgv[i+1], "none"))
            {
                psOptions->bUnsetNoData = true;
            }
            else
            {
                psOptions->bSetNoData = true;
                psOptions->dfNoDataReal = CPLAtofM(papszArgv[i+1]);
            }
            i += 1;
        }

        else if( i + 4 < argc && EQUAL(papszArgv[i],"-a_ullr") )
        {
            psOptions->adfULLR[0] = CPLAtofM(papszArgv[i+1]);
            psOptions->adfULLR[1] = CPLAtofM(papszArgv[i+2]);
            psOptions->adfULLR[2] = CPLAtofM(papszArgv[i+3]);
            psOptions->adfULLR[3] = CPLAtofM(papszArgv[i+4]);

            i += 4;
        }

        else if( EQUAL(papszArgv[i],"-co") && papszArgv[i+1] )
        {
            psOptions->papszCreateOptions = CSLAddString( psOptions->papszCreateOptions, papszArgv[++i] );
        }

        else if( EQUAL(papszArgv[i],"-scale") || STARTS_WITH_CI(papszArgv[i], "-scale_") )
        {
            int nIndex = 0;
            if( STARTS_WITH_CI(papszArgv[i], "-scale_") )
            {
                if( !psOptions->bHasUsedExplicitScaleBand && psOptions->nScaleRepeat != 0 )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Cannot mix -scale and -scale_XX syntax");
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }
                psOptions->bHasUsedExplicitScaleBand = true;
                nIndex = atoi(papszArgv[i] + 7);
                if( nIndex <= 0 || nIndex > 65535 )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Invalid parameter name: %s", papszArgv[i]);
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }
                nIndex --;
            }
            else
            {
                if( psOptions->bHasUsedExplicitScaleBand )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Cannot mix -scale and -scale_XX syntax");
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }
                nIndex = psOptions->nScaleRepeat;
            }

            if( nIndex >= psOptions->nScaleRepeat )
            {
                psOptions->pasScaleParams =
                    static_cast<GDALTranslateScaleParams*>(
                        CPLRealloc(psOptions->pasScaleParams,
                                   (nIndex + 1) *
                                   sizeof(GDALTranslateScaleParams)));
                memset(psOptions->pasScaleParams + psOptions->nScaleRepeat, 0,
                        sizeof(GDALTranslateScaleParams) * (nIndex - psOptions->nScaleRepeat + 1));
                psOptions->nScaleRepeat = nIndex + 1;
            }
            psOptions->pasScaleParams[nIndex].bScale = TRUE;
            psOptions->pasScaleParams[nIndex].bHaveScaleSrc = false;
            if( i < argc-2 && ArgIsNumeric(papszArgv[i+1]) )
            {
                psOptions->pasScaleParams[nIndex].bHaveScaleSrc = true;
                psOptions->pasScaleParams[nIndex].dfScaleSrcMin = CPLAtofM(papszArgv[i+1]);
                psOptions->pasScaleParams[nIndex].dfScaleSrcMax = CPLAtofM(papszArgv[i+2]);
                i += 2;
            }
            if( i < argc-2 && psOptions->pasScaleParams[nIndex].bHaveScaleSrc && ArgIsNumeric(papszArgv[i+1]) )
            {
                psOptions->pasScaleParams[nIndex].dfScaleDstMin = CPLAtofM(papszArgv[i+1]);
                psOptions->pasScaleParams[nIndex].dfScaleDstMax = CPLAtofM(papszArgv[i+2]);
                i += 2;
            }
            else
            {
                psOptions->pasScaleParams[nIndex].dfScaleDstMin = 0.0;
                psOptions->pasScaleParams[nIndex].dfScaleDstMax = 255.999;
            }
        }

        else if( (EQUAL(papszArgv[i],"-exponent") || STARTS_WITH_CI(papszArgv[i], "-exponent_")) &&
                 papszArgv[i+1] )
        {
            int nIndex = 0;
            if( STARTS_WITH_CI(papszArgv[i], "-exponent_") )
            {
                if( !psOptions->bHasUsedExplicitExponentBand && psOptions->nExponentRepeat != 0 )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Cannot mix -exponent and -exponent_XX syntax");
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }
                psOptions->bHasUsedExplicitExponentBand = true;
                nIndex = atoi(papszArgv[i] + 10);
                if( nIndex <= 0 || nIndex > 65535 )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Invalid parameter name: %s", papszArgv[i]);
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }
                nIndex --;
            }
            else
            {
                if( psOptions->bHasUsedExplicitExponentBand )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Cannot mix -exponent and -exponent_XX syntax");
                    GDALTranslateOptionsFree(psOptions);
                    return NULL;
                }
                nIndex = psOptions->nExponentRepeat;
            }

            if( nIndex >= psOptions->nExponentRepeat )
            {
              psOptions->padfExponent = static_cast<double *>(
                  CPLRealloc(psOptions->padfExponent,
                             (nIndex + 1) * sizeof(double)));
                if( nIndex > psOptions->nExponentRepeat )
                    memset(psOptions->padfExponent + psOptions->nExponentRepeat, 0,
                        sizeof(double) * (nIndex - psOptions->nExponentRepeat));
                psOptions->nExponentRepeat = nIndex + 1;
            }
            double dfExponent = CPLAtofM(papszArgv[++i]);
            psOptions->padfExponent[nIndex] = dfExponent;
        }

        else if( EQUAL(papszArgv[i], "-unscale") )
        {
            psOptions->bUnscale = true;
        }

        else if( EQUAL(papszArgv[i],"-mo") && papszArgv[i+1] )
        {
            psOptions->papszMetadataOptions = CSLAddString( psOptions->papszMetadataOptions,
                                                 papszArgv[++i] );
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-outsize") && papszArgv[i+1] != NULL )
        {
            ++i;
            if( papszArgv[i][0] != '\0' &&
                papszArgv[i][strlen(papszArgv[i])-1] == '%' )
                psOptions->dfOXSizePct = CPLAtofM(papszArgv[i]);
            else
                psOptions->nOXSizePixel = atoi(papszArgv[i]);
            ++i;
            if( papszArgv[i][0] != '\0' &&
                papszArgv[i][strlen(papszArgv[i])-1] == '%' )
                psOptions->dfOYSizePct = CPLAtofM(papszArgv[i]);
            else
                psOptions->nOYSizePixel = atoi(papszArgv[i]);
            bOutsideExplicitlySet = true;
        }

        else if( i+2 < argc && EQUAL(papszArgv[i],"-tr") )
        {
            psOptions->dfXRes = CPLAtofM(papszArgv[++i]);
            psOptions->dfYRes = fabs(CPLAtofM(papszArgv[++i]));
            if( psOptions->dfXRes == 0 || psOptions->dfYRes == 0 )
            {
                CPLError(CE_Failure, CPLE_IllegalArg, "Wrong value for -tr parameters.");
                GDALTranslateOptionsFree(psOptions);
                return NULL;
            }
        }

        else if( i+4 < argc && EQUAL(papszArgv[i],"-srcwin") )
        {
            psOptions->adfSrcWin[0] = CPLAtof(papszArgv[++i]);
            psOptions->adfSrcWin[1] = CPLAtof(papszArgv[++i]);
            psOptions->adfSrcWin[2] = CPLAtof(papszArgv[++i]);
            psOptions->adfSrcWin[3] = CPLAtof(papszArgv[++i]);
        }

        else if( i+4 < argc && EQUAL(papszArgv[i],"-projwin") )
        {
            psOptions->dfULX = CPLAtofM(papszArgv[++i]);
            psOptions->dfULY = CPLAtofM(papszArgv[++i]);
            psOptions->dfLRX = CPLAtofM(papszArgv[++i]);
            psOptions->dfLRY = CPLAtofM(papszArgv[++i]);
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-projwin_srs") )
        {
            CPLFree(psOptions->pszProjSRS);
            psOptions->pszProjSRS = CPLStrdup(papszArgv[i+1]);
            i++;
        }

        else if( EQUAL(papszArgv[i],"-epo") )
        {
            psOptions->bErrorOnPartiallyOutside = true;
            psOptions->bErrorOnCompletelyOutside = true;
        }

        else  if( EQUAL(papszArgv[i],"-eco") )
        {
            psOptions->bErrorOnCompletelyOutside = true;
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-a_srs") )
        {
            CPLFree(psOptions->pszOutputSRS);
            psOptions->pszOutputSRS = CPLStrdup(papszArgv[i+1]);
            i++;
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-expand") && papszArgv[i+1] != NULL )
        {
            i++;
            if (EQUAL(papszArgv[i], "gray"))
                psOptions->nRGBExpand = 1;
            else if (EQUAL(papszArgv[i], "rgb"))
                psOptions->nRGBExpand = 3;
            else if (EQUAL(papszArgv[i], "rgba"))
                psOptions->nRGBExpand = 4;
            else
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Value %s unsupported. Only gray, rgb or rgba are supported.",
                          papszArgv[i] );
                GDALTranslateOptionsFree(psOptions);
                return NULL;
            }
        }

        else if( EQUAL(papszArgv[i], "-stats") )
        {
            psOptions->bStats = true;
            psOptions->bApproxStats = false;
        }
        else if( EQUAL(papszArgv[i], "-approx_stats") )
        {
            psOptions->bStats = true;
            psOptions->bApproxStats = true;
        }
        else if( EQUAL(papszArgv[i], "-norat") )
        {
            psOptions->bNoRAT = true;
        }
        else if( i+1 < argc && EQUAL(papszArgv[i], "-oo") )
        {
            i++;
            if( psOptionsForBinary )
            {
                psOptionsForBinary->papszOpenOptions =
                    CSLAddString( psOptionsForBinary->papszOpenOptions,
                                                papszArgv[i] );
            }
        }
        else if( i+1 < argc && EQUAL(papszArgv[i],"-r") )
        {
            CPLFree(psOptions->pszResampling);
            psOptions->pszResampling = CPLStrdup(papszArgv[++i]);
        }

        // Undocumented option used by gdal_translate_fuzzer
        else if( i+1 < argc && EQUAL(papszArgv[i],"-limit_outsize") )
        {
            psOptions->nLimitOutSize = atoi(papszArgv[i+1]);
            i++;
        }

        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
        else if( !bGotSourceFilename )
        {
            bGotSourceFilename = true;
            if( psOptionsForBinary )
                psOptionsForBinary->pszSource = CPLStrdup(papszArgv[i]);
        }
        else if( !bGotDestFilename )
        {
            bGotDestFilename = true;
            if( psOptionsForBinary )
                psOptionsForBinary->pszDest = CPLStrdup(papszArgv[i]);
        }
        else
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too many command options '%s'", papszArgv[i]);
            GDALTranslateOptionsFree(psOptions);
            return NULL;
        }
    }

    if( bOutsideExplicitlySet &&
        psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0 &&
        psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-outsize %d %d invalid.", psOptions->nOXSizePixel, psOptions->nOYSizePixel);
        GDALTranslateOptionsFree(psOptions);
        return NULL;
    }

    if( psOptionsForBinary )
    {
        psOptionsForBinary->pszFormat = CPLStrdup(psOptions->pszFormat);
    }

    return psOptions;
}

/************************************************************************/
/*                        GDALTranslateOptionsFree()                    */
/************************************************************************/

/**
 * Frees the GDALTranslateOptions struct.
 *
 * @param psOptions the options struct for GDALTranslate().
 *
 * @since GDAL 2.1
 */

void GDALTranslateOptionsFree(GDALTranslateOptions *psOptions)
{
    if( psOptions == NULL ) return;

    CPLFree(psOptions->pszFormat);
    CPLFree(psOptions->panBandList);
    CSLDestroy(psOptions->papszCreateOptions);
    CPLFree(psOptions->pasScaleParams);
    CPLFree(psOptions->padfExponent);
    CSLDestroy(psOptions->papszMetadataOptions);
    CPLFree(psOptions->pszOutputSRS);
    if( psOptions->nGCPCount )
        GDALDeinitGCPs(psOptions->nGCPCount, psOptions->pasGCPs);
    CPLFree(psOptions->pasGCPs);
    CPLFree(psOptions->pszResampling);
    CPLFree(psOptions->pszProjSRS);

    CPLFree(psOptions);
}

/************************************************************************/
/*                 GDALTranslateOptionsSetProgress()                    */
/************************************************************************/

/**
 * Set a progress function.
 *
 * @param psOptions the options struct for GDALTranslate().
 * @param pfnProgress the progress callback.
 * @param pProgressData the user data for the progress callback.
 *
 * @since GDAL 2.1
 */

void GDALTranslateOptionsSetProgress( GDALTranslateOptions *psOptions,
                                      GDALProgressFunc pfnProgress, void *pProgressData )
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
    if( pfnProgress == GDALTermProgress )
        psOptions->bQuiet = false;
}
