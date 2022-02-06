/******************************************************************************
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL Image Translator Program
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2002, Frank Warmerdam
 * Copyright (c) 2007-2015, Even Rouault <even dot rouault at spatialys.com>
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
#include "cpl_json.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_priv_templates.hpp"
#include "gdal_rat.h"
#include "gdal_vrt.h"
#include "ogr_core.h"
#include "ogr_spatialref.h"
#include "vrtdataset.h"

CPL_CVSID("$Id$")

static int ArgIsNumeric( const char * );
static void AttachMetadata( GDALDatasetH, char ** );
static void CopyBandInfo( GDALRasterBand * poSrcBand, GDALRasterBand * poDstBand,
                          int bCanCopyStatsMetadata, int bCopyScale, int bCopyNoData, bool bCopyRAT,
                          const GDALTranslateOptions* psOptions );

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

    /*! output format. Use the short format name. */
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

    bool bSetScale;

    double dfScale;

    bool bSetOffset;

    double dfOffset;

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

    /*! Coordinate epoch of output SRS */
    double dfOutputCoordinateEpoch;

    /*! does not copy source GCP into destination dataset (when TRUE) */
    bool bNoGCP;

    /*! number of GCPS to be added to the output dataset */
    int nGCPCount;

    /*! list of GCPs to be added to the output dataset */
    GDAL_GCP *pasGCPs;

    /*! assign/override the georeferenced bounds of the output file. This assigns
        georeferenced bounds to the output file, ignoring what would have been
        derived from the source file. So this does not cause reprojection to the
        specified SRS. */
    double adfULLR[4];

    /*! set a nodata value specified in GDALTranslateOptions::szNoData to the output bands */
    bool bSetNoData;

    /*! avoid setting a nodata value to the output file if one exists for the source file */
    bool bUnsetNoData;

    /*! Assign a specified nodata value to output bands ( GDALTranslateOptions::bSetNoData option
        should be set). Note that if the input dataset has a nodata value, this does not cause
        pixel values that are equal to that nodata value to be changed to the value specified. */
    char szNoData[32];

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
        as an error. The default behavior is to accept such requests. */
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

    // Array of color interpretations per band. Should be a GDALColorInterp
    // value, or -1 if no override.
    int nColorInterpSize;
    int* panColorInterp;

    /*! does not copy source XMP into destination dataset (when TRUE) */
    bool bNoXMP;
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
    if( psOptionsIn->pszFormat ) psOptions->pszFormat = CPLStrdup(psOptionsIn->pszFormat);
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
    if( psOptionsIn->panColorInterp )
    {
        psOptions->panColorInterp =
            static_cast<int *>(CPLMalloc(sizeof(int) * psOptions->nColorInterpSize));
        memcpy(psOptions->panColorInterp, psOptionsIn->panColorInterp,
               sizeof(int) * psOptions->nColorInterpSize);
    }
    return psOptions;
}

/************************************************************************/
/*                        GDALTranslateFlush()                          */
/************************************************************************/

static GDALDatasetH GDALTranslateFlush(GDALDatasetH hOutDS)
{
    if( hOutDS != nullptr )
    {
        CPLErr eErrBefore = CPLGetLastErrorType();
        GDALFlushCache( hOutDS );
        if (eErrBefore == CE_None &&
            CPLGetLastErrorType() != CE_None)
        {
            GDALClose(hOutDS);
            hOutDS = nullptr;
        }
    }
    return hOutDS;
}

/************************************************************************/
/*                    EditISIS3MetadataForBandChange()                  */
/************************************************************************/

static CPLJSONObject Clone(const CPLJSONObject& obj)
{
    auto serialized = obj.Format(CPLJSONObject::PrettyFormat::Plain);
    CPLJSONDocument oJSONDocument;
    const GByte *pabyData = reinterpret_cast<const GByte *>(serialized.c_str());
    oJSONDocument.LoadMemory( pabyData );
    return oJSONDocument.GetRoot();
}

static void ReworkArray(CPLJSONObject& container, const CPLJSONObject& obj,
                        int nSrcBandCount,
                        const GDALTranslateOptions *psOptions)
{
    auto oArray = obj.ToArray();
    if( oArray.Size() == nSrcBandCount )
    {
        CPLJSONArray oNewArray;
        for( int i = 0; i < psOptions->nBandCount; i++ )
        {
            const int iSrcIdx = psOptions->panBandList[i]-1;
            oNewArray.Add(oArray[iSrcIdx]);
        }
        const auto childName(obj.GetName());
        container.Delete(childName);
        container.Add(childName, oNewArray);
    }
}

static CPLString EditISIS3MetadataForBandChange(const char* pszJSON,
                                                int nSrcBandCount,
                                                const GDALTranslateOptions *psOptions)
{
    CPLJSONDocument oJSONDocument;
    const GByte *pabyData = reinterpret_cast<const GByte *>(pszJSON);
    if( !oJSONDocument.LoadMemory( pabyData ) )
    {
        return CPLString();
    }

    auto oRoot = oJSONDocument.GetRoot();
    if( !oRoot.IsValid() )
    {
        return CPLString();
    }

    auto oBandBin = oRoot.GetObj( "IsisCube/BandBin" );
    if( oBandBin.IsValid() && oBandBin.GetType() == CPLJSONObject::Type::Object )
    {
        // Backup original BandBin object
        oRoot.GetObj("IsisCube").Add("OriginalBandBin", Clone(oBandBin));

        // Iterate over BandBin members and reorder/resize its arrays that
        // have the same number of elements than the number of bands of the
        // source dataset.
        for( auto& child: oBandBin.GetChildren() )
        {
            if( child.GetType() == CPLJSONObject::Type::Array )
            {
                ReworkArray(oBandBin, child, nSrcBandCount, psOptions);
            }
            else if( child.GetType() == CPLJSONObject::Type::Object )
            {
                auto oValue = child.GetObj("value");
                auto oUnit = child.GetObj("unit");
                if( oValue.GetType() == CPLJSONObject::Type::Array )
                {
                    ReworkArray(child, oValue, nSrcBandCount, psOptions);
                }
            }
        }
    }

    return oRoot.Format(CPLJSONObject::PrettyFormat::Pretty);
}

/************************************************************************/
/*                       AdjustNoDataValue()                            */
/************************************************************************/

static double AdjustNoDataValue( double dfInputNoDataValue,
                                 GDALRasterBand* poBand,
                                 const GDALTranslateOptions *psOptions )
{
    bool bSignedByte = false;
    const char* pszPixelType = CSLFetchNameValue( psOptions->papszCreateOptions, "PIXELTYPE" );
    if( pszPixelType == nullptr )
    {
        pszPixelType = poBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
    }
    if( pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE") )
        bSignedByte = true;
    int bClamped = FALSE;
    int bRounded = FALSE;
    double dfVal = 0.0;
    const GDALDataType eBandType = poBand->GetRasterDataType();
    if( bSignedByte )
    {
        if( dfInputNoDataValue < -128.0 )
        {
            dfVal = -128.0;
            bClamped = TRUE;
        }
        else if( dfInputNoDataValue > 127.0 )
        {
            dfVal = 127.0;
            bClamped = TRUE;
        }
        else
        {
            dfVal = static_cast<int>(floor(dfInputNoDataValue + 0.5));
            if( dfVal != dfInputNoDataValue )
                bRounded = TRUE;
        }
    }
    else
    {
        dfVal = GDALAdjustValueToDataType(eBandType,
                                                dfInputNoDataValue,
                                                &bClamped, &bRounded );
    }

    if (bClamped)
    {
        CPLError( CE_Warning, CPLE_AppDefined, "for band %d, nodata value has been clamped "
                "to %.0f, the original value being out of range.",
                poBand->GetBand(), dfVal);
    }
    else if(bRounded)
    {
        CPLError( CE_Warning, CPLE_AppDefined, "for band %d, nodata value has been rounded "
                "to %.0f, %s being an integer datatype.",
                poBand->GetBand(), dfVal,
                GDALGetDataTypeName(eBandType));
    }
    return dfVal;
}

/************************************************************************/
/*                             GDALTranslate()                          */
/************************************************************************/

/**
 * Converts raster data between different formats.
 *
 * This is the equivalent of the <a href="/programs/gdal_translate.html">gdal_translate</a> utility.
 *
 * GDALTranslateOptions* must be allocated and freed with GDALTranslateOptionsNew()
 * and GDALTranslateOptionsFree() respectively.
 *
 * @param pszDest the destination dataset path.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALTranslateOptionsNew() or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using GDALClose()) or NULL in case of error.
 *
 * @since GDAL 2.1
 */

GDALDatasetH GDALTranslate( const char *pszDest, GDALDatasetH hSrcDataset,
                            const GDALTranslateOptions *psOptionsIn, int *pbUsageError )

{
    CPLErrorReset();
    if( hSrcDataset == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No source dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if( pszDest == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if(pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALTranslateOptions* psOptions =
        (psOptionsIn) ? GDALTranslateOptionsClone(psOptionsIn) :
                        GDALTranslateOptionsNew(nullptr, nullptr);

    GDALDatasetH hOutDS = nullptr;
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
        return nullptr;
    }

    CPLString osProjSRS;

    if(psOptions->pszProjSRS != nullptr)
    {
        OGRSpatialReference oSRS;
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        if( oSRS.SetFromUserInput( psOptions->pszProjSRS ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                      psOptions->pszProjSRS );
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }

        char* pszSRS = nullptr;
        oSRS.exportToWkt( &pszSRS );
        if( pszSRS )
            osProjSRS = pszSRS;
        CPLFree( pszSRS );
    }

    if(psOptions->pszOutputSRS != nullptr)
    {
        OGRSpatialReference oOutputSRS;
        oOutputSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        if( oOutputSRS.SetFromUserInput( psOptions->pszOutputSRS ) != OGRERR_NONE )
        {
            CPLError( CE_Failure, CPLE_AppDefined, "Failed to process SRS definition: %s",
                      psOptions->pszOutputSRS );
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }

        char* pszSRS = nullptr;
        {
            CPLErrorStateBackuper oErrorStateBackuper;
            CPLErrorHandlerPusher oErrorHandler(CPLQuietErrorHandler);
            if( oOutputSRS.exportToWkt( &pszSRS ) != OGRERR_NONE )
            {
                CPLFree(pszSRS);
                pszSRS = nullptr;
                const char* const apszOptions[] = { "FORMAT=WKT2", nullptr };
                oOutputSRS.exportToWkt( &pszSRS, apszOptions );
            }
        }
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
        return nullptr;
    }
    if( bGotBounds &&  (psOptions->dfXRes != 0 && psOptions->dfYRes != 0) )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "-a_ullr and -tr options cannot be used at the same time.");
        if(pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
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

    if( psOptions->panBandList == nullptr )
    {

        psOptions->nBandCount = GDALGetRasterCount( hSrcDataset );
        if( ( psOptions->nBandCount == 0 ) && (psOptions->bStrict ) )
        {
            // if not strict then the driver can fail if it doesn't support zero bands
            CPLError( CE_Failure, CPLE_AppDefined, "Input file has no bands, and so cannot be translated." );
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
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
                return nullptr;
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
        return nullptr;
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
        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Compute the source window from the projected source window      */
/*      if the projected coordinates were provided.  Note that the      */
/*      projected coordinates are in ulx, uly, lrx, lry format,         */
/*      while the adfSrcWin is xoff, yoff, xsize, ysize with the        */
/*      xoff,yoff being the ulx, uly in pixel/line.                     */
/* -------------------------------------------------------------------- */
    const char *pszProjection = nullptr;

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
            return nullptr;
        }
        if( adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                     "The -projwin option was used, but the geotransform is\n"
                     "rotated.  This configuration is not supported." );
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }

        if( !osProjSRS.empty() )
        {
            pszProjection = GDALGetProjectionRef( hSrcDataset );
            if( pszProjection != nullptr && strlen(pszProjection) > 0 )
            {
                OGRSpatialReference oSRSIn;
                OGRSpatialReference oSRSDS;
                oSRSIn.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                oSRSDS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
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
                        return nullptr;
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
        if( psOptions->pszResampling == nullptr ||
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
        return nullptr;
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
            return nullptr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Find the output driver.                                         */
/* -------------------------------------------------------------------- */
    if( psOptions->pszFormat == nullptr )
    {
        CPLString osFormat = GetOutputDriverForRaster(pszDest);
        if( osFormat.empty() )
        {
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
        psOptions->pszFormat = CPLStrdup(osFormat);
    }

    GDALDriverH hDriver = GDALGetDriverByName(psOptions->pszFormat);
    if( hDriver == nullptr )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "Output driver `%s' not recognised.",
                  psOptions->pszFormat);
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Make sure we cleanup if there is an existing dataset of this    */
/*      name.  But even if that seems to fail we will continue since    */
/*      it might just be a corrupt file or something.                   */
/*      This is needed for                                              */
/*      gdal_translate foo.tif foo.tif.ovr -outsize 50% 50%             */
/* -------------------------------------------------------------------- */
    if( !CPLFetchBool(psOptions->papszCreateOptions, "APPEND_SUBDATASET", false) )
    {
        // Someone issuing Create("foo.tif") on a
        // memory driver doesn't expect files with those names to be deleted
        // on a file system...
        // This is somewhat messy. Ideally there should be a way for the
        // driver to overload the default behavior
        if( !EQUAL(psOptions->pszFormat, "MEM") &&
            !EQUAL(psOptions->pszFormat, "Memory") )
        {
            GDALDriver::FromHandle(hDriver)->QuietDelete( pszDest );
        }
        // Make sure to load early overviews, so that on the GTiff driver
        // external .ovr is looked for before it might be created as the
        // output dataset !
        if( GDALGetRasterCount( hSrcDataset ) )
        {
            auto hBand = GDALGetRasterBand(hSrcDataset, 1);
            if( hBand )
                GDALGetOverviewCount(hBand);
        }
    }

    char** papszDriverMD = GDALGetMetadata(hDriver, nullptr);

    if( !CPLTestBool( CSLFetchNameValueDef(papszDriverMD,
                                           GDAL_DCAP_RASTER, "FALSE") ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s driver has no raster capabilities.",
                  psOptions->pszFormat );
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
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
        return nullptr;
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
        && !psOptions->bSetScale && !psOptions->bSetOffset
        && CSLCount(psOptions->papszMetadataOptions) == 0 && bAllBandsInOrder
        && psOptions->eMaskMode == MASK_AUTO
        && bSpatialArrangementPreserved
        && !psOptions->bNoGCP
        && psOptions->nGCPCount == 0 && !bGotBounds
        && psOptions->pszOutputSRS == nullptr
        && psOptions->dfOutputCoordinateEpoch == 0
        && !psOptions->bSetNoData && !psOptions->bUnsetNoData
        && psOptions->nRGBExpand == 0 && !psOptions->bNoRAT
        && psOptions->panColorInterp == nullptr
        && !psOptions->bNoXMP )
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
                    static_cast<GDALDataset *>(hSrcDataset)->GetRasterBand(1)->GetRasterDataType() );
            }
            if( nRawOutSize > static_cast<vsi_l_offset>(psOptions->nLimitOutSize) )
            {
                CPLError( CE_Failure, CPLE_IllegalArg,
                          "Attempt to create %dx%d dataset is above authorized limit.",
                          GDALGetRasterXSize(hSrcDataset),
                          GDALGetRasterYSize(hSrcDataset) );
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
        }

/* -------------------------------------------------------------------- */
/*      Compute stats if required.                                      */
/* -------------------------------------------------------------------- */
        if (psOptions->bStats)
        {
            GDALDataset* poSrcDS = GDALDataset::FromHandle(hSrcDataset);
            for( int i = 0; i < poSrcDS->GetRasterCount(); i++ )
            {
                double dfMin, dfMax, dfMean, dfStdDev;
                poSrcDS->GetRasterBand(i+1)->ComputeStatistics(
                    psOptions->bApproxStats,
                    &dfMin, &dfMax, &dfMean, &dfStdDev,
                    GDALDummyProgress, nullptr );
            }
        }

        hOutDS = GDALCreateCopy( hDriver, pszDest, hSrcDataset,
                                 psOptions->bStrict, psOptions->papszCreateOptions,
                                 psOptions->pfnProgress, psOptions->pProgressData );
        hOutDS = GDALTranslateFlush(hOutDS);

        GDALTranslateOptionsFree(psOptions);
        return hOutDS;
    }

    if( CSLFetchNameValue(psOptions->papszCreateOptions, "COPY_SRC_OVERVIEWS") )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "General options of gdal_translate make the "
                 "COPY_SRC_OVERVIEWS creation option ineffective as they hide "
                 "the overviews");
    }

/* -------------------------------------------------------------------- */
/*      Establish some parameters.                                      */
/* -------------------------------------------------------------------- */
    int nOXSize = 0;
    int nOYSize = 0;

    bool bHasSrcGeoTransform = false;
    double adfSrcGeoTransform[6] = {};
    if( GDALGetGeoTransform( hSrcDataset, adfSrcGeoTransform ) == CE_None )
        bHasSrcGeoTransform = true;

    if( psOptions->dfXRes != 0.0 )
    {
        if( !(bHasSrcGeoTransform &&
              psOptions->nGCPCount == 0 &&
              adfSrcGeoTransform[2] == 0.0 && adfSrcGeoTransform[4] == 0.0) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                     "The -tr option was used, but there's no geotransform or it is\n"
                     "rotated.  This configuration is not supported." );
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
        const double dfOXSize =
            psOptions->adfSrcWin[2] /
            psOptions->dfXRes * adfSrcGeoTransform[1] + 0.5;
        const double dfOYSize =
            psOptions->adfSrcWin[3] /
            psOptions->dfYRes * fabs(adfSrcGeoTransform[5]) + 0.5;
        if( dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize) ||
            dfOYSize < 1 || !GDALIsValueInRange<int>(dfOXSize) )
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Invalid output size: %g x %g",
                     dfOXSize, dfOYSize);
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
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
            return nullptr;
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
                    return nullptr;
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
                    return nullptr;
                }
                nOYSize = static_cast<int>(dfOYSize);
            }
        }

        if( psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0 )
        {
            const double dfOXSize =
                static_cast<double>(nOYSize) * psOptions->adfSrcWin[2] /
                psOptions->adfSrcWin[3] + 0.5;
            if( dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                        "Invalid output width: %g",
                        dfOXSize);
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
            nOXSize = static_cast<int>(dfOXSize);
        }
        else if( psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0 )
        {
            const double dfOYSize =
                static_cast<double>(nOXSize) * psOptions->adfSrcWin[3] /
                psOptions->adfSrcWin[2] + 0.5;
            if( dfOYSize < 1 || !GDALIsValueInRange<int>(dfOYSize) )
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                        "Invalid output height: %g",
                        dfOYSize);
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
            nOYSize = static_cast<int>(dfOYSize);
        }
    }

    if( nOXSize <= 0 || nOYSize <= 0 )
    {
        CPLError( CE_Failure, CPLE_IllegalArg, "Attempt to create %dx%d dataset is illegal.", nOXSize, nOYSize);
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    // For gdal_translate_fuzzer
    if( psOptions->nLimitOutSize > 0 )
    {
        vsi_l_offset nRawOutSize = static_cast<vsi_l_offset>(nOXSize) * nOYSize;
        if( psOptions->nBandCount )
        {
            if( nRawOutSize > std::numeric_limits<vsi_l_offset>::max() / psOptions->nBandCount )
            {
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
            nRawOutSize *= psOptions->nBandCount;
            const int nDTSize = GDALGetDataTypeSizeBytes(
                static_cast<GDALDataset *>(hSrcDataset)->GetRasterBand(1)->GetRasterDataType() );
            if( nDTSize > 0 &&
                nRawOutSize > std::numeric_limits<vsi_l_offset>::max() / nDTSize )
            {
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
            nRawOutSize *= nDTSize;
        }
        if( nRawOutSize > static_cast<vsi_l_offset>(psOptions->nLimitOutSize) )
        {
            CPLError( CE_Failure, CPLE_IllegalArg,
                      "Attempt to create %dx%d dataset is above authorized limit.",
                      nOXSize, nOYSize);
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }


/* ==================================================================== */
/*      Create a virtual dataset.                                       */
/* ==================================================================== */

/* -------------------------------------------------------------------- */
/*      Make a virtual clone.                                           */
/* -------------------------------------------------------------------- */
    VRTDataset *poVDS = static_cast<VRTDataset *>(VRTCreate(nOXSize, nOYSize));

    if( psOptions->nGCPCount == 0 )
    {
        OGRSpatialReference oSRS;
        if( psOptions->pszOutputSRS != nullptr )
        {
            oSRS.SetFromUserInput( psOptions->pszOutputSRS );
            oSRS.SetAxisMappingStrategy( OAMS_TRADITIONAL_GIS_ORDER );
        }
        else
        {
            const OGRSpatialReference* poSrcSRS = GDALDataset::FromHandle(hSrcDataset)->GetSpatialRef();
            if( poSrcSRS )
                oSRS = *poSrcSRS;
        }
        if( !oSRS.IsEmpty() )
        {
            if( psOptions->dfOutputCoordinateEpoch > 0 )
                oSRS.SetCoordinateEpoch(psOptions->dfOutputCoordinateEpoch);
            poVDS->SetSpatialRef( &oSRS );
        }
    }

    bool bHasDstGeoTransform = false;
    double adfDstGeoTransform[6] = {};

    if( bGotBounds )
    {
        bHasDstGeoTransform = true;
        adfDstGeoTransform[0] = psOptions->adfULLR[0];
        adfDstGeoTransform[1] = (psOptions->adfULLR[2] - psOptions->adfULLR[0]) / nOXSize;
        adfDstGeoTransform[2] = 0.0;
        adfDstGeoTransform[3] = psOptions->adfULLR[1];
        adfDstGeoTransform[4] = 0.0;
        adfDstGeoTransform[5] = (psOptions->adfULLR[3] - psOptions->adfULLR[1]) / nOYSize;

        poVDS->SetGeoTransform( adfDstGeoTransform );
    }

    else if( bHasSrcGeoTransform && psOptions->nGCPCount == 0 )
    {
        bHasDstGeoTransform = true;
        memcpy( adfDstGeoTransform, adfSrcGeoTransform, 6 * sizeof(double) );
        adfDstGeoTransform[0] += psOptions->adfSrcWin[0] * adfDstGeoTransform[1]
            + psOptions->adfSrcWin[1] * adfDstGeoTransform[2];
        adfDstGeoTransform[3] += psOptions->adfSrcWin[0] * adfDstGeoTransform[4]
            + psOptions->adfSrcWin[1] * adfDstGeoTransform[5];

        const double dfX = static_cast<double>(nOXSize);
        const double dfY = static_cast<double>(nOYSize);
        adfDstGeoTransform[1] *= psOptions->adfSrcWin[2] / dfX;
        adfDstGeoTransform[2] *= psOptions->adfSrcWin[3] / dfY;
        adfDstGeoTransform[4] *= psOptions->adfSrcWin[2] / dfX;
        adfDstGeoTransform[5] *= psOptions->adfSrcWin[3] / dfY;

        if( psOptions->dfXRes != 0.0 )
        {
            adfDstGeoTransform[1] = psOptions->dfXRes;
            adfDstGeoTransform[5] = (adfDstGeoTransform[5] > 0) ? psOptions->dfYRes : -psOptions->dfYRes;
        }

        poVDS->SetGeoTransform( adfDstGeoTransform );
    }

    if( psOptions->nGCPCount != 0 )
    {
        const char *pszGCPProjection = psOptions->pszOutputSRS;

        if( pszGCPProjection == nullptr )
            pszGCPProjection = GDALGetGCPProjection( hSrcDataset );
        if( pszGCPProjection == nullptr )
            pszGCPProjection = "";

        poVDS->SetGCPs( psOptions->nGCPCount, psOptions->pasGCPs, pszGCPProjection );
    }

    else if( !psOptions->bNoGCP && GDALGetGCPCount( hSrcDataset ) > 0 )
    {
        const int nGCPs = GDALGetGCPCount(hSrcDataset);

        GDAL_GCP *pasGCPs = GDALDuplicateGCPs(nGCPs, GDALGetGCPs(hSrcDataset));

        for( int i = 0; i < nGCPs; i++ )
        {
            pasGCPs[i].dfGCPPixel -= psOptions->adfSrcWin[0];
            pasGCPs[i].dfGCPLine  -= psOptions->adfSrcWin[1];
            pasGCPs[i].dfGCPPixel *=
                nOXSize / static_cast<double>(psOptions->adfSrcWin[2]);
            pasGCPs[i].dfGCPLine  *=
                nOYSize / static_cast<double>(psOptions->adfSrcWin[3]);
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

    // When specifying -tr with non-nearest resampling, make sure that the
    // size of target window precisely matches the requested resolution, to
    // avoid any shift.
    if( bHasSrcGeoTransform && bHasDstGeoTransform &&
        psOptions->dfXRes != 0.0 &&
        psOptions->pszResampling != nullptr &&
        !EQUALN(psOptions->pszResampling, "NEAR", 4) )
    {
        adfDstWin[2] = psOptions->adfSrcWin[2] * adfSrcGeoTransform[1] / adfDstGeoTransform[1];
        adfDstWin[3] = psOptions->adfSrcWin[3] * fabs(adfSrcGeoTransform[5] / adfDstGeoTransform[5]);
    }

    double adfSrcWinOri[4];
    static_assert(sizeof(adfSrcWinOri) == sizeof(psOptions->adfSrcWin),
                  "inconsistent adfSrcWin size");
    memcpy(adfSrcWinOri, psOptions->adfSrcWin, sizeof(psOptions->adfSrcWin));
    FixSrcDstWindow( psOptions->adfSrcWin, adfDstWin,
                     GDALGetRasterXSize(hSrcDataset),
                     GDALGetRasterYSize(hSrcDataset) );

/* -------------------------------------------------------------------- */
/*      Transfer generally applicable metadata.                         */
/* -------------------------------------------------------------------- */
    GDALDataset* poSrcDS = GDALDataset::FromHandle(hSrcDataset);
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

    // Remove NITF_BLOCKA_ stuff if georeferencing is changed
    if( !(psOptions->adfSrcWin[0] == 0 && psOptions->adfSrcWin[1] == 0 &&
          psOptions->adfSrcWin[2] == GDALGetRasterXSize(hSrcDataset) &&
          psOptions->adfSrcWin[3] == GDALGetRasterYSize(hSrcDataset) &&
          psOptions->nGCPCount == 0 && !bGotBounds) )
    {
        char** papszIter = papszMetadata;
        while(papszIter && *papszIter)
        {
            if (STARTS_WITH_CI(*papszIter, "NITF_BLOCKA_"))
            {
                CPLFree(*papszIter);
                memmove(papszIter, papszIter+1, sizeof(char*) * (CSLCount(papszIter+1)+1));
            }
            else
                papszIter++;
        }
    }

    {
        char** papszIter = papszMetadata;
        while(papszIter && *papszIter)
        {
            // Do not preserve the CACHE_PATH from the WMS driver
            if (STARTS_WITH_CI(*papszIter, "CACHE_PATH="))
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
    AttachMetadata( static_cast<GDALDatasetH>(poVDS), psOptions->papszMetadataOptions );

    const char* pszInterleave = GDALGetMetadataItem(hSrcDataset, "INTERLEAVE", "IMAGE_STRUCTURE");
    if (pszInterleave)
        poVDS->SetMetadataItem("INTERLEAVE", pszInterleave, "IMAGE_STRUCTURE");

    {
        const char* pszCompression = poSrcDS->GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE");
        if( pszCompression )
        {
            poVDS->SetMetadataItem("COMPRESSION", pszCompression, "IMAGE_STRUCTURE");
        }
    }

    /* ISIS3 metadata preservation */
    char** papszMD_ISIS3 = poSrcDS->GetMetadata("json:ISIS3");
    if( papszMD_ISIS3 != nullptr)
    {
        if( !bAllBandsInOrder )
        {
            CPLString osJSON = EditISIS3MetadataForBandChange(
                papszMD_ISIS3[0], GDALGetRasterCount( hSrcDataset ), psOptions);
            if( !osJSON.empty() )
            {
                char* apszMD[] = { &osJSON[0], nullptr };
                poVDS->SetMetadata( apszMD, "json:ISIS3" );
            }
        }
        else
        {
            poVDS->SetMetadata( papszMD_ISIS3, "json:ISIS3" );
        }
    }

    // PDS4 -> PDS4 special case
    if( EQUAL(psOptions->pszFormat, "PDS4") )
    {
        char** papszMD_PDS4 = poSrcDS->GetMetadata("xml:PDS4");
        if( papszMD_PDS4 != nullptr)
            poVDS->SetMetadata( papszMD_PDS4, "xml:PDS4" );
    }

    // VICAR -> VICAR special case
    if( EQUAL(psOptions->pszFormat, "VICAR") )
    {
        char** papszMD_VICAR = poSrcDS->GetMetadata("json:VICAR");
        if( papszMD_VICAR != nullptr)
            poVDS->SetMetadata( papszMD_VICAR, "json:VICAR" );
    }

    // Copy XMP metadata
    if( !psOptions->bNoXMP )
    {
        char** papszXMP = poSrcDS->GetMetadata("xml:XMP");
        if (papszXMP != nullptr && *papszXMP != nullptr)
        {
            poVDS->SetMetadata(papszXMP, "xml:XMP");
        }
    }


/* -------------------------------------------------------------------- */
/*      Transfer metadata that remains valid if the spatial             */
/*      arrangement of the data is unaltered.                           */
/* -------------------------------------------------------------------- */
    if( bSpatialArrangementPreserved )
    {
        char **papszMD = poSrcDS->GetMetadata("RPC");
        if( papszMD != nullptr )
            poVDS->SetMetadata( papszMD, "RPC" );

        papszMD = poSrcDS->GetMetadata("GEOLOCATION");
        if( papszMD != nullptr )
            poVDS->SetMetadata( papszMD, "GEOLOCATION" );
    }
    else
    {
        char **papszMD = poSrcDS->GetMetadata("RPC");
        if( papszMD != nullptr )
        {
            papszMD = CSLDuplicate(papszMD);

            double dfSAMP_OFF = CPLAtof(CSLFetchNameValueDef(papszMD, "SAMP_OFF", "0"));
            double dfLINE_OFF = CPLAtof(CSLFetchNameValueDef(papszMD, "LINE_OFF", "0"));
            double dfSAMP_SCALE = CPLAtof(CSLFetchNameValueDef(papszMD, "SAMP_SCALE", "1"));
            double dfLINE_SCALE = CPLAtof(CSLFetchNameValueDef(papszMD, "LINE_SCALE", "1"));

            dfSAMP_OFF -= adfSrcWinOri[0];
            dfLINE_OFF -= adfSrcWinOri[1];

            const double df2 = adfSrcWinOri[2];
            const double df3 = adfSrcWinOri[3];
            dfSAMP_OFF *= nOXSize / df2;
            dfLINE_OFF *= nOYSize / df3;
            dfSAMP_SCALE *= nOXSize / df2;
            dfLINE_SCALE *= nOYSize / df3;

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
            static_cast<GDALDataset*>(hSrcDataset)->
                GetRasterBand(std::abs(psOptions->panBandList[0]));
        if (psOptions->panBandList[0] < 0)
            poSrcBand = poSrcBand->GetMaskBand();
        GDALColorTable* poColorTable = poSrcBand->GetColorTable();
        if (poColorTable == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error : band %d has no color table",
                     std::abs(psOptions->panBandList[0]));
            GDALClose(poVDS);
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
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
            return nullptr;
        }
    }

    // Can be set to TRUE in the band loop too
    bool bFilterOutStatsMetadata =
        psOptions->nScaleRepeat > 0 || psOptions->bUnscale ||
        !bSpatialArrangementPreserved || psOptions->nRGBExpand != 0;

    if( psOptions->nColorInterpSize > psOptions->nBandCount )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "More bands defined in -colorinterp than output bands");
    }

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
            static_cast<GDALDataset*>(hSrcDataset)->GetRasterBand(std::abs(nSrcBand));

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
                pszMin != nullptr && pszMax != nullptr )
            {
                const bool bSrcIsInteger =
                    CPL_TO_BOOL(GDALDataTypeIsInteger(eSrcBandType) &&
                                !GDALDataTypeIsComplex(eSrcBandType));
                const bool bDstIsInteger =
                    CPL_TO_BOOL(GDALDataTypeIsInteger(eBandType) &&
                                !GDALDataTypeIsComplex(eBandType));
                if( bSrcIsInteger && bDstIsInteger )
                {
                    std::int64_t nDstMin = 0;
                    std::uint64_t nDstMax = 0;
                    switch( eBandType )
                    {
                        case GDT_Byte:
                            nDstMin = std::numeric_limits<std::uint8_t>::min();
                            nDstMax = std::numeric_limits<std::uint8_t>::max();
                            break;
                        case GDT_UInt16:
                            nDstMin = std::numeric_limits<std::uint16_t>::min();
                            nDstMax = std::numeric_limits<std::uint16_t>::max();
                            break;
                        case GDT_Int16:
                            nDstMin = std::numeric_limits<std::int16_t>::min();
                            nDstMax = std::numeric_limits<std::int16_t>::max();
                            break;
                        case GDT_UInt32:
                            nDstMin = std::numeric_limits<std::uint32_t>::min();
                            nDstMax = std::numeric_limits<std::uint32_t>::max();
                            break;
                        case GDT_Int32:
                            nDstMin = std::numeric_limits<std::int32_t>::min();
                            nDstMax = std::numeric_limits<std::int32_t>::max();
                            break;
                        case GDT_UInt64:
                            nDstMin = std::numeric_limits<std::uint64_t>::min();
                            nDstMax = std::numeric_limits<std::uint64_t>::max();
                            break;
                        case GDT_Int64:
                            nDstMin = std::numeric_limits<std::int64_t>::min();
                            nDstMax = std::numeric_limits<std::int64_t>::max();
                            break;
                        default:
                            CPLAssert(false);
                            break;
                    }

                    try
                    {
                        const auto nMin = std::stoll(pszMin);
                        const auto nMax = std::stoull(pszMax);
                        if( nMin < nDstMin || nMax > nDstMax )
                            bFilterOutStatsMetadata = true;
                    }
                    catch( const std::exception& )
                    {
                    }
                }
                // Float64 is large enough to hold all integer <= 32 bit or float32 values
                // there might be other OK cases, but ere on safe side for now
                else if( !((bSrcIsInteger || eSrcBandType == GDT_Float32) && eBandType == GDT_Float64) )
                {
                    bFilterOutStatsMetadata = true;
                }
            }
        }

/* -------------------------------------------------------------------- */
/*      Create this band.                                               */
/* -------------------------------------------------------------------- */
        CPLStringList aosAddBandOptions;
        if( bSpatialArrangementPreserved )
        {
            int nSrcBlockXSize, nSrcBlockYSize;
            poSrcBand->GetBlockSize(&nSrcBlockXSize, &nSrcBlockYSize);
            aosAddBandOptions.SetNameValue("BLOCKXSIZE", CPLSPrintf("%d", nSrcBlockXSize));
            aosAddBandOptions.SetNameValue("BLOCKYSIZE", CPLSPrintf("%d", nSrcBlockYSize));
        }
        poVDS->AddBand( eBandType, aosAddBandOptions.List() );
        VRTSourcedRasterBand *poVRTBand =
            static_cast<VRTSourcedRasterBand *>(poVDS->GetRasterBand( i+1 ));
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
            !psOptions->bUnscale && psOptions->eOutputType == GDT_Unknown && psOptions->pszResampling == nullptr )
        {
            poVRTBand->SetMetadataItem("NBITS", pszNBits, "IMAGE_STRUCTURE");
        }

        // Preserve PIXELTYPE if no option change values
        const char* pszPixelType = poSrcBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        if( pszPixelType && psOptions->nRGBExpand == 0 && psOptions->nScaleRepeat == 0 &&
            !psOptions->bUnscale && psOptions->eOutputType == GDT_Unknown && psOptions->pszResampling == nullptr )
        {
            poVRTBand->SetMetadataItem("PIXELTYPE", pszPixelType, "IMAGE_STRUCTURE");
        }

        const char* pszCompression = poSrcBand->GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE");
        if( pszCompression )
        {
            poVRTBand->SetMetadataItem("COMPRESSION", pszCompression, "IMAGE_STRUCTURE");
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
            return nullptr;
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
                return nullptr;
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
        VRTSimpleSource* poSimpleSource = nullptr;
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
            poVRTBand->SetColorInterpretation( static_cast<GDALColorInterp>(GCI_RedBand + i) );
        }

/* -------------------------------------------------------------------- */
/*      copy over some other information of interest.                   */
/* -------------------------------------------------------------------- */
        else
        {
            CopyBandInfo( poSrcBand, poVRTBand,
                          !psOptions->bStats && !bFilterOutStatsMetadata,
                          !psOptions->bUnscale && !psOptions->bSetScale &&
                            !psOptions->bSetOffset,
                          !psOptions->bSetNoData && !psOptions->bUnsetNoData,
                          !psOptions->bNoRAT,
                          psOptions );
            if( psOptions->nScaleRepeat == 0 &&
                psOptions->nExponentRepeat == 0 &&
                EQUAL(psOptions->pszFormat, "GRIB") )
            {
                char** papszMD_GRIB = poSrcBand->GetMetadata("GRIB");
                if( papszMD_GRIB != nullptr)
                    poVRTBand->SetMetadata( papszMD_GRIB, "GRIB" );
            }
        }

        // Color interpretation override
        if( psOptions->panColorInterp )
        {
            if( i < psOptions->nColorInterpSize &&
                psOptions->panColorInterp[i] >= 0 )
            {
                poVRTBand->SetColorInterpretation(
                    static_cast<GDALColorInterp>(psOptions->panColorInterp[i]));
            }
        }

/* -------------------------------------------------------------------- */
/*      Set a forcible nodata value?                                    */
/* -------------------------------------------------------------------- */
        if( psOptions->bSetNoData )
        {
            if( poVRTBand->GetRasterDataType() == GDT_Int64 )
            {
                if( strchr(psOptions->szNoData, '.') ||
                    CPLGetValueType(psOptions->szNoData) == CPL_VALUE_STRING )
                {
                    const double dfNoData = CPLAtof(psOptions->szNoData);
                    if( dfNoData >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                        dfNoData <= static_cast<double>(std::numeric_limits<int64_t>::max()) &&
                        dfNoData == static_cast<double>(static_cast<int64_t>(dfNoData)) )
                    {
                        poVRTBand->SetNoDataValueAsInt64(
                            static_cast<int64_t>(dfNoData));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot set nodata value %s on a Int64 band",
                                 psOptions->szNoData);
                    }
                }
                else
                {
                    errno = 0;
                    const auto val = std::strtoll(psOptions->szNoData, nullptr, 10);
                    if( errno == 0 )
                    {
                        poVRTBand->SetNoDataValueAsInt64(static_cast<int64_t>(val));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot set nodata value %s on a Int64 band",
                                 psOptions->szNoData);
                    }
                }
            }
            else if( poVRTBand->GetRasterDataType() == GDT_UInt64 )
            {
                if( strchr(psOptions->szNoData, '.') ||
                    CPLGetValueType(psOptions->szNoData) == CPL_VALUE_STRING )
                {
                    const double dfNoData = CPLAtof(psOptions->szNoData);
                    if( dfNoData >= static_cast<double>(std::numeric_limits<uint64_t>::min()) &&
                        dfNoData <= static_cast<double>(std::numeric_limits<uint64_t>::max()) &&
                        dfNoData == static_cast<double>(static_cast<uint64_t>(dfNoData)) )
                    {
                        poVRTBand->SetNoDataValueAsUInt64(
                            static_cast<uint64_t>(dfNoData));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot set nodata value %s on a UInt64 band",
                                 psOptions->szNoData);
                    }
                }
                else
                {
                    errno = 0;
                    const auto val = std::strtoull(psOptions->szNoData, nullptr, 10);
                    if( errno == 0 )
                    {
                        poVRTBand->SetNoDataValueAsUInt64(static_cast<uint64_t>(val));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot set nodata value %s on a UInt64 band",
                                 psOptions->szNoData);
                    }
                }
            }
            else
            {
                const double dfVal = AdjustNoDataValue(
                    CPLAtof(psOptions->szNoData), poVRTBand, psOptions);
                poVRTBand->SetNoDataValue( dfVal );
            }
        }

        if( psOptions->bSetScale )
            poVRTBand->SetScale( psOptions->dfScale );

        if( psOptions->bSetOffset )
            poVRTBand->SetOffset( psOptions->dfOffset );

        if (psOptions->eMaskMode == MASK_AUTO &&
            (GDALGetMaskFlags(GDALGetRasterBand(hSrcDataset, 1)) & GMF_PER_DATASET) == 0 &&
            (poSrcBand->GetMaskFlags() & (GMF_ALL_VALID | GMF_NODATA)) == 0)
        {
            if (poVRTBand->CreateMaskBand(poSrcBand->GetMaskFlags()) == CE_None)
            {
                VRTSourcedRasterBand* hMaskVRTBand =
                    cpl::down_cast<VRTSourcedRasterBand*>(poVRTBand->GetMaskBand());
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
            static_cast<GDALRasterBand*>(GDALGetRasterBand(hSrcDataset,
                                               std::abs(psOptions->nMaskBand)));
        if (poSrcBand && poVDS->CreateMaskBand(GMF_PER_DATASET) == CE_None)
        {
            VRTSourcedRasterBand* hMaskVRTBand = static_cast<VRTSourcedRasterBand*>(
                GDALGetMaskBand(GDALGetRasterBand(static_cast<GDALDataset*>(poVDS), 1)));
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
            VRTSourcedRasterBand* hMaskVRTBand = static_cast<VRTSourcedRasterBand*>(
                GDALGetMaskBand(GDALGetRasterBand(static_cast<GDALDataset*>(poVDS), 1)));
            hMaskVRTBand->AddMaskBandSource(static_cast<GDALRasterBand*>(GDALGetRasterBand(hSrcDataset, 1)),
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
                    &dfMin, &dfMax, &dfMean, &dfStdDev, GDALDummyProgress, nullptr );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write to the output file using CopyCreate().                    */
/* -------------------------------------------------------------------- */
    if( EQUAL(psOptions->pszFormat, "VRT") &&
        psOptions->papszCreateOptions == nullptr )
    {
        poVDS->SetDescription(pszDest);
        hOutDS = static_cast<GDALDatasetH>(poVDS);
        if( !EQUAL(pszDest, "") )
        {
            hOutDS = GDALTranslateFlush(hOutDS);
        }
    }
    else
    {
        hOutDS = GDALCreateCopy( hDriver, pszDest, static_cast<GDALDatasetH>(poVDS),
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
        char *pszKey = nullptr;
        const char *pszValue =
            CPLParseNameValue(papszMetadataOptions[i], &pszKey);
        if( pszKey && pszValue )
        {
            GDALSetMetadataItem(hDS,pszKey,pszValue,nullptr);
        }
        CPLFree( pszKey );
    }
}

/************************************************************************/
/*                           CopyBandInfo()                            */
/************************************************************************/

/* A bit of a clone of VRTRasterBand::CopyCommonInfoFrom(), but we need */
/* more and more custom behavior in the context of gdal_translate ... */

static void CopyBandInfo( GDALRasterBand * poSrcBand, GDALRasterBand * poDstBand,
                          int bCanCopyStatsMetadata, int bCopyScale, int bCopyNoData, bool bCopyRAT,
                          const GDALTranslateOptions *psOptions )

{

    if (bCanCopyStatsMetadata)
    {
        poDstBand->SetMetadata( poSrcBand->GetMetadata() );
        if (bCopyRAT)
        {
            poDstBand->SetDefaultRAT( poSrcBand->GetDefaultRAT() );
        }
    }
    else
    {
        char** papszMetadata = poSrcBand->GetMetadata();
        char** papszMetadataNew = nullptr;
        for( int i = 0; papszMetadata != nullptr && papszMetadata[i] != nullptr; i++ )
        {
            if (!STARTS_WITH(papszMetadata[i], "STATISTICS_"))
                papszMetadataNew = CSLAddString(papszMetadataNew, papszMetadata[i]);
        }
        poDstBand->SetMetadata( papszMetadataNew );
        CSLDestroy(papszMetadataNew);

        // we need to strip histogram data from the source RAT
        if (poSrcBand->GetDefaultRAT() && bCopyRAT)
        {
            GDALRasterAttributeTable *poNewRAT = poSrcBand->GetDefaultRAT()->Clone();

            // strip histogram data (as defined by the source RAT)
            poNewRAT->RemoveStatistics();
            if( poNewRAT->GetColumnCount() )
            {
                poDstBand->SetDefaultRAT( poNewRAT );
            }
            // since SetDefaultRAT copies the RAT data we need to delete our original
            delete poNewRAT;
        }
    }

    poDstBand->SetColorTable( poSrcBand->GetColorTable() );
    poDstBand->SetColorInterpretation(poSrcBand->GetColorInterpretation());
    if( strlen(poSrcBand->GetDescription()) > 0 )
        poDstBand->SetDescription( poSrcBand->GetDescription() );

    if (bCopyNoData)
    {
        if( poSrcBand->GetRasterDataType() != GDT_Int64 &&
            poSrcBand->GetRasterDataType() != GDT_UInt64 &&
            poDstBand->GetRasterDataType() != GDT_Int64 &&
            poDstBand->GetRasterDataType() != GDT_UInt64 )
        {
            int bSuccess = FALSE;
            double dfNoData = poSrcBand->GetNoDataValue(&bSuccess);
            if( bSuccess )
            {
                const double dfVal = AdjustNoDataValue(
                    dfNoData, poDstBand, psOptions);
                poDstBand->SetNoDataValue( dfVal );
            }
        }
        else
        {
            GDALCopyNoDataValue(poDstBand, poSrcBand);
        }
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
/*                             GetColorInterp()                         */
/************************************************************************/

static int GetColorInterp( const char* pszStr )
{
    if( EQUAL(pszStr, "red") )
        return GCI_RedBand;
    if( EQUAL(pszStr, "green") )
        return GCI_GreenBand;
    if( EQUAL(pszStr, "blue") )
        return GCI_BlueBand;
    if( EQUAL(pszStr, "alpha") )
        return GCI_AlphaBand;
    if( EQUAL(pszStr, "gray") || EQUAL(pszStr, "grey") )
        return GCI_GrayIndex;
    if( EQUAL(pszStr, "undefined") )
        return GCI_Undefined;
    CPLError(CE_Warning, CPLE_NotSupported,
             "Unsupported color interpretation: %s", pszStr);
    return -1;
}

/************************************************************************/
/*                             GDALTranslateOptionsNew()                */
/************************************************************************/

/**
 * Allocates a GDALTranslateOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including filename and open options too), or NULL.
 *                  The accepted options are the ones of the <a href="/programs/gdal_translate.html">gdal_translate</a> utility.
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

    psOptions->pszFormat = nullptr;
    psOptions->bQuiet = true;
    psOptions->pfnProgress = GDALDummyProgress;
    psOptions->pProgressData = nullptr;
    psOptions->eOutputType = GDT_Unknown;
    psOptions->eMaskMode = MASK_AUTO;
    psOptions->nBandCount = 0;
    psOptions->panBandList = nullptr;
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
    psOptions->bSetScale = false;
    psOptions->dfScale = 1.0;
    psOptions->bSetOffset = false;
    psOptions->dfOffset = 0.0;
    psOptions->nScaleRepeat = 0;
    psOptions->pasScaleParams = nullptr;
    psOptions->bHasUsedExplicitScaleBand = false;
    psOptions->nExponentRepeat = 0;
    psOptions->padfExponent = nullptr;
    psOptions->bHasUsedExplicitExponentBand = false;
    psOptions->dfULX = 0.0;
    psOptions->dfULY = 0.0;
    psOptions->dfLRX = 0.0;
    psOptions->dfLRY = 0.0;
    psOptions->pszOutputSRS = nullptr;
    psOptions->bNoGCP = false;
    psOptions->nGCPCount = 0;
    psOptions->pasGCPs = nullptr;
    psOptions->adfULLR[0] = 0;
    psOptions->adfULLR[1] = 0;
    psOptions->adfULLR[2] = 0;
    psOptions->adfULLR[3] = 0;
    psOptions->bSetNoData = false;
    psOptions->bUnsetNoData = false;
    psOptions->szNoData[0] = 0;
    psOptions->nRGBExpand = 0;
    psOptions->nMaskBand = 0;
    psOptions->bStats = false;
    psOptions->bApproxStats = false;
    psOptions->bErrorOnPartiallyOutside = false;
    psOptions->bErrorOnCompletelyOutside = false;
    psOptions->bNoRAT = false;
    psOptions->pszResampling = nullptr;
    psOptions->dfXRes = 0.0;
    psOptions->dfYRes = 0.0;
    psOptions->pszProjSRS = nullptr;
    psOptions->nLimitOutSize = 0;
    psOptions->bNoXMP = false;

    bool bParsedMaskArgument = false;
    bool bOutsideExplicitlySet = false;
    bool bGotSourceFilename = false;
    bool bGotDestFilename = false;

/* -------------------------------------------------------------------- */
/*      Handle command line arguments.                                  */
/* -------------------------------------------------------------------- */
    const int argc = CSLCount(papszArgv);
    for( int i = 0; i < argc && papszArgv != nullptr && papszArgv[i] != nullptr; i++ )
    {
        if( i < argc-1 && (EQUAL(papszArgv[i],"-of") || EQUAL(papszArgv[i],"-f")) )
        {
            ++i;
            CPLFree(psOptions->pszFormat);
            psOptions->pszFormat = CPLStrdup(papszArgv[i]);
        }

        else if( EQUAL(papszArgv[i],"-q") || EQUAL(papszArgv[i],"-quiet") )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bQuiet = true;
        }

        else if( EQUAL(papszArgv[i],"-ot") && papszArgv[i+1] )
        {
            for( int iType = 1; iType < GDT_TypeCount; iType++ )
            {
                if( GDALGetDataTypeName(static_cast<GDALDataType>(iType)) != nullptr
                    && EQUAL(GDALGetDataTypeName(static_cast<GDALDataType>(iType)),
                             papszArgv[i+1]) )
                {
                    psOptions->eOutputType = static_cast<GDALDataType>(iType);
                }
            }

            if( psOptions->eOutputType == GDT_Unknown )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unknown output pixel type: %s.", papszArgv[i+1] );
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
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
                return nullptr;
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
                    return nullptr;
                }

                psOptions->eMaskMode = MASK_USER;
                psOptions->nMaskBand = nBand;
                if (bMask)
                    psOptions->nMaskBand *= -1;
            }
            i ++;
        }
        else if( EQUAL(papszArgv[i],"-not_strict") )
        {
            psOptions->bStrict = false;

        }
        else if( EQUAL(papszArgv[i],"-strict")  )
        {
            psOptions->bStrict = true;
        }
        else if( EQUAL(papszArgv[i],"-sds")  )
        {
            if( psOptionsForBinary )
                psOptionsForBinary->bCopySubDatasets = TRUE;
        }
        else if (EQUAL(papszArgv[i], "-nogcp"))
        {
            psOptions->bNoGCP = true;
        }
        else if( i + 4 < argc && EQUAL(papszArgv[i],"-gcp") )
        {
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

            char* endptr = nullptr;
            if( papszArgv[i+1] != nullptr
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
                snprintf(psOptions->szNoData, sizeof(psOptions->szNoData),
                         "%s", papszArgv[i+1]);
            }
            i += 1;
        }

        else if( EQUAL(papszArgv[i],"-a_scale") && papszArgv[i+1] )
        {
            psOptions->bSetScale = true;
            psOptions->dfScale = CPLAtofM(papszArgv[i+1]);
            i += 1;
        }

        else if( EQUAL(papszArgv[i],"-a_offset") && papszArgv[i+1] )
        {
            psOptions->bSetOffset = true;
            psOptions->dfOffset = CPLAtofM(papszArgv[i+1]);
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
                    return nullptr;
                }
                psOptions->bHasUsedExplicitScaleBand = true;
                nIndex = atoi(papszArgv[i] + 7);
                if( nIndex <= 0 || nIndex > 65535 )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Invalid parameter name: %s", papszArgv[i]);
                    GDALTranslateOptionsFree(psOptions);
                    return nullptr;
                }
                nIndex --;
            }
            else
            {
                if( psOptions->bHasUsedExplicitScaleBand )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Cannot mix -scale and -scale_XX syntax");
                    GDALTranslateOptionsFree(psOptions);
                    return nullptr;
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
                    return nullptr;
                }
                psOptions->bHasUsedExplicitExponentBand = true;
                nIndex = atoi(papszArgv[i] + 10);
                if( nIndex <= 0 || nIndex > 65535 )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Invalid parameter name: %s", papszArgv[i]);
                    GDALTranslateOptionsFree(psOptions);
                    return nullptr;
                }
                nIndex --;
            }
            else
            {
                if( psOptions->bHasUsedExplicitExponentBand )
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "Cannot mix -exponent and -exponent_XX syntax");
                    GDALTranslateOptionsFree(psOptions);
                    return nullptr;
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

        else if( i+2 < argc && EQUAL(papszArgv[i],"-outsize") && papszArgv[i+1] != nullptr )
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
                return nullptr;
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

        else if( i+1 < argc && EQUAL(papszArgv[i],"-a_coord_epoch") )
        {
            psOptions->dfOutputCoordinateEpoch = CPLAtofM(papszArgv[i+1]);
            i++;
        }

        else if( i+1 < argc && EQUAL(papszArgv[i],"-expand") && papszArgv[i+1] != nullptr )
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
                return nullptr;
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

        else if( EQUAL(papszArgv[i],"-colorinterp") && papszArgv[i+1] )
        {
            ++i;
            CPLStringList aosList(CSLTokenizeString2(papszArgv[i], ",", 0));
            psOptions->nColorInterpSize = aosList.size();
            psOptions->panColorInterp = static_cast<int *>(
                  CPLRealloc(psOptions->panColorInterp,
                             psOptions->nColorInterpSize * sizeof(int)));
            for( int j = 0; j < aosList.size(); j++ )
            {
                psOptions->panColorInterp[j] = GetColorInterp(aosList[j]);
            }
        }

        else if( STARTS_WITH_CI(papszArgv[i], "-colorinterp_") && papszArgv[i+1] )
        {
            int nIndex = atoi(papszArgv[i] + strlen("-colorinterp_"));
            if( nIndex <= 0 || nIndex > 65535 )
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Invalid parameter name: %s", papszArgv[i]);
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
            nIndex --;

            if( nIndex >= psOptions->nColorInterpSize )
            {
                psOptions->panColorInterp = static_cast<int *>(
                    CPLRealloc(psOptions->panColorInterp,
                                (nIndex + 1) * sizeof(int)));
                if( nIndex > psOptions->nColorInterpSize )
                {
                    memset(psOptions->panColorInterp +
                                psOptions->nColorInterpSize,
                           0xFF, // -1
                           sizeof(int) * (nIndex - psOptions->nColorInterpSize));
                }
                psOptions->nColorInterpSize = nIndex + 1;
            }
            ++i;
            psOptions->panColorInterp[nIndex] = GetColorInterp(papszArgv[i]);
        }


        // Undocumented option used by gdal_translate_fuzzer
        else if( i+1 < argc && EQUAL(papszArgv[i],"-limit_outsize") )
        {
            psOptions->nLimitOutSize = atoi(papszArgv[i+1]);
            i++;
        }

        else if( i+1 < argc && EQUAL(papszArgv[i], "-if") )
        {
            i++;
            if( psOptionsForBinary )
            {
                if( GDALGetDriverByName(papszArgv[i]) == nullptr )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "%s is not a recognized driver", papszArgv[i]);
                }
                psOptionsForBinary->papszAllowInputDrivers = CSLAddString(
                    psOptionsForBinary->papszAllowInputDrivers, papszArgv[i] );
            }
        }

        else if (EQUAL(papszArgv[i], "-noxmp"))
        {
            psOptions->bNoXMP = true;
        }


        else if( papszArgv[i][0] == '-' )
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unknown option name '%s'", papszArgv[i]);
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
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
            return nullptr;
        }
    }

    if (psOptions->nGCPCount > 0 && psOptions->bNoGCP)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-nogcp and -gcp cannot be used as the same time" );
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if( bOutsideExplicitlySet &&
        psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0 &&
        psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "-outsize %d %d invalid.", psOptions->nOXSizePixel, psOptions->nOYSizePixel);
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if( psOptionsForBinary )
    {
        if( psOptions->pszFormat )
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
    if( psOptions == nullptr ) return;

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
    CPLFree(psOptions->panColorInterp);

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
