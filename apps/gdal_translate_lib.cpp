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
#include "gdalargumentparser.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <limits>
#include <set>

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

static void AttachMetadata(GDALDatasetH, const CPLStringList &);
static void AttachDomainMetadata(GDALDatasetH, const CPLStringList &);

static void CopyBandInfo(GDALRasterBand *poSrcBand, GDALRasterBand *poDstBand,
                         int bCanCopyStatsMetadata, int bCopyScale,
                         int bCopyNoData, bool bCopyRAT,
                         const GDALTranslateOptions *psOptions);

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
struct GDALTranslateScaleParams
{
    /*! scaling is done only if it is set to TRUE. This is helpful when there is
       a need to scale only certain bands. */
    bool bScale = false;

    /*! set it to TRUE if dfScaleSrcMin and dfScaleSrcMax is set. When it is
       FALSE, the input range is automatically computed from the source data. */
    bool bHaveScaleSrc = false;

    /*! the range of input pixel values which need to be scaled */
    double dfScaleSrcMin = 0;
    double dfScaleSrcMax = 0;

    /*! the range of output pixel values. If
       GDALTranslateScaleParams::dfScaleDstMin and
       GDALTranslateScaleParams::dfScaleDstMax are not set, then the output
        range is 0 to 255. */
    double dfScaleDstMin = 0;
    double dfScaleDstMax = 0;
};

/************************************************************************/
/*                         GDALTranslateOptions                         */
/************************************************************************/

/** Options for use with GDALTranslate(). GDALTranslateOptions* must be
 * allocated and freed with GDALTranslateOptionsNew() and
 * GDALTranslateOptionsFree() respectively.
 */
struct GDALTranslateOptions
{

    /*! output format. Use the short format name. */
    std::string osFormat{};

    /*! allow or suppress progress monitor and other non-error output */
    bool bQuiet = true;

    /*! the progress function to use */
    GDALProgressFunc pfnProgress = GDALDummyProgress;

    /*! pointer to the progress data variable */
    void *pProgressData = nullptr;

    /*! for the output bands to be of the indicated data type */
    GDALDataType eOutputType = GDT_Unknown;

    /*! Used only by parser logic */
    bool bParsedMaskArgument = false;

    MaskMode eMaskMode = MASK_AUTO;

    /*! number of input bands to write to the output file, or to reorder bands
     */
    int nBandCount = 0;

    /*! list of input bands to write to the output file, or to reorder bands.
       The value 1 corresponds to the 1st band. */
    std::vector<int> anBandList{}; /* negative value of panBandList[i] means
                                      mask band of ABS(panBandList[i]) */

    /*! size of the output file. GDALTranslateOptions::nOXSizePixel is in pixels
       and GDALTranslateOptions::nOYSizePixel is in lines. If one of the two
       values is set to 0, its value will be determined from the other one,
       while maintaining the aspect ratio of the source dataset */
    int nOXSizePixel = 0;
    int nOYSizePixel = 0;

    /*! size of the output file. GDALTranslateOptions::dfOXSizePct and
       GDALTranslateOptions::dfOYSizePct are fraction of the input image size.
       The value 100 means 100%. If one of the two values is set to 0, its value
       will be determined from the other one, while maintaining the aspect ratio
       of the source dataset */
    double dfOXSizePct = 0;
    double dfOYSizePct = 0;

    /*! list of creation options to the output format driver */
    CPLStringList aosCreateOptions{};

    /*! subwindow from the source image for copying based on pixel/line location
     */
    std::array<double, 4> adfSrcWin{{0, 0, 0, 0}};

    /*! don't be forgiving of mismatches and lost data when translating to the
     * output format */
    bool bStrict = false;

    /*! apply the scale/offset metadata for the bands to convert scaled values
     * to unscaled values. It is also often necessary to reset the output
     * datatype with GDALTranslateOptions::eOutputType */
    bool bUnscale = false;

    bool bSetScale = false;

    double dfScale = 1;

    bool bSetOffset = false;

    double dfOffset = 0;

    /*! the list of scale parameters for each band. */
    std::vector<GDALTranslateScaleParams> asScaleParams{};

    /*! It is set to TRUE, when scale parameters are specific to each band */
    bool bHasUsedExplicitScaleBand = false;

    /*! to apply non-linear scaling with a power function. It is the list of
       exponents of the power function (must be positive). This option must be
       used with GDALTranslateOptions::asScaleParams. If
        GDALTranslateOptions::adfExponent.size() is 1, it is applied to all
       bands of the output image. */
    std::vector<double> adfExponent{};

    bool bHasUsedExplicitExponentBand = false;

    /*! list of metadata key and value to set on the output dataset if possible. */
    CPLStringList aosMetadataOptions{};

    /*! list of metadata key and value in a domain to set on the output dataset if possible. */
    CPLStringList aosDomainMetadataOptions{};

    /*! override the projection for the output file. The SRS may be any of the
       usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file containing
       the WKT. */
    std::string osOutputSRS{};

    /*! Coordinate epoch of output SRS */
    double dfOutputCoordinateEpoch = 0;

    /*! does not copy source GCP into destination dataset (when TRUE) */
    bool bNoGCP = false;

    /*! number of GCPS to be added to the output dataset */
    int nGCPCount = 0;

    /*! list of GCPs to be added to the output dataset */
    GDAL_GCP *pasGCPs = nullptr;

    /*! assign/override the georeferenced bounds of the output file. This
       assigns georeferenced bounds to the output file, ignoring what would have
       been derived from the source file. So this does not cause reprojection to
       the specified SRS. */
    std::array<double, 4> adfULLR{{0, 0, 0, 0}};

    /*! assign/override the geotransform of the output file. This
       assigns a geotransform to the output file, ignoring what would have
       been derived from the source file. So this does not cause reprojection to
       the specified SRS. */
    std::array<double, 6> adfGT{{0, 0, 0, 0, 0, 0}};

    /*! set a nodata value specified in GDALTranslateOptions::osNoData to the
     * output bands */
    bool bSetNoData = 0;

    /*! avoid setting a nodata value to the output file if one exists for the
     * source file */
    bool bUnsetNoData = 0;

    /*! Assign a specified nodata value to output bands (
       GDALTranslateOptions::bSetNoData option should be set). Note that if the
       input dataset has a nodata value, this does not cause pixel values that
       are equal to that nodata value to be changed to the value specified. */
    std::string osNoData{};

    /*! to expose a dataset with 1 band with a color table as a dataset with
        3 (RGB) or 4 (RGBA) bands. Useful for output drivers such as JPEG,
        JPEG2000, MrSID, ECW that don't support color indexed datasets.
        The 1 value enables to expand a dataset with a color table that only
        contains gray levels to a gray indexed dataset. */
    int nRGBExpand = 0;

    int nMaskBand = 0; /* negative value means mask band of ABS(nMaskBand) */

    /*! force recomputation of statistics */
    bool bStats = false;

    bool bApproxStats = false;

    /*! If this option is set, GDALTranslateOptions::adfSrcWin or
       (GDALTranslateOptions::dfULX, GDALTranslateOptions::dfULY,
       GDALTranslateOptions::dfLRX, GDALTranslateOptions::dfLRY) values that
       falls partially outside the source raster extent will be considered as an
       error. The default behavior is to accept such requests. */
    bool bErrorOnPartiallyOutside = false;

    /*! Same as bErrorOnPartiallyOutside, except that the criterion for
        erroring out is when the request falls completely outside the
        source raster extent. */
    bool bErrorOnCompletelyOutside = false;

    /*! does not copy source RAT into destination dataset (when TRUE) */
    bool bNoRAT = false;

    /*! resampling algorithm
        nearest (default), bilinear, cubic, cubicspline, lanczos, average, mode
     */
    std::string osResampling{};

    /*! target resolution. The values must be expressed in georeferenced units.
        Both must be positive values. This is exclusive with
       GDALTranslateOptions::nOXSizePixel (or
       GDALTranslateOptions::dfOXSizePct), GDALTranslateOptions::nOYSizePixel
        (or GDALTranslateOptions::dfOYSizePct), GDALTranslateOptions::adfULLR,
        and GDALTranslateOptions::adfGT.
     */
    double dfXRes = 0;
    double dfYRes = 0;

    /*! subwindow from the source image for copying (like
       GDALTranslateOptions::adfSrcWin) but with the corners given in
       georeferenced coordinates (by default expressed in the SRS of the
       dataset. Can be changed with osProjSRS) */
    double dfULX = 0;
    double dfULY = 0;
    double dfLRX = 0;
    double dfLRY = 0;

    /*! SRS in which to interpret the coordinates given with
       GDALTranslateOptions::dfULX, GDALTranslateOptions::dfULY,
       GDALTranslateOptions::dfLRX, GDALTranslateOptions::dfLRY. The SRS may be
       any of the usual GDAL/OGR forms, complete WKT, PROJ.4, EPSG:n or a file
       containing the WKT. Note that this does not cause reprojection of the
        dataset to the specified SRS. */
    std::string osProjSRS{};

    int nLimitOutSize = 0;

    // Array of color interpretations per band. Should be a GDALColorInterp
    // value, or -1 if no override.
    std::vector<int> anColorInterp{};

    /*! does not copy source XMP into destination dataset (when TRUE) */
    bool bNoXMP = false;

    /*! overview level of source file to be used */
    int nOvLevel = OVR_LEVEL_AUTO;

    GDALTranslateOptions() = default;
    ~GDALTranslateOptions();
    GDALTranslateOptions *Clone() const;

  private:
    GDALTranslateOptions(const GDALTranslateOptions &) = default;
    GDALTranslateOptions &operator=(const GDALTranslateOptions &) = delete;
};

/************************************************************************/
/*             GDALTranslateOptions::~GDALTranslateOptions()            */
/************************************************************************/

GDALTranslateOptions::~GDALTranslateOptions()
{
    if (nGCPCount)
        GDALDeinitGCPs(nGCPCount, pasGCPs);
    CPLFree(pasGCPs);
}

/************************************************************************/
/*                    GDALTranslateOptions::Clone(()                    */
/************************************************************************/

GDALTranslateOptions *GDALTranslateOptions::Clone() const
{
    GDALTranslateOptions *psOptions = new GDALTranslateOptions(*this);
    if (nGCPCount)
        psOptions->pasGCPs = GDALDuplicateGCPs(nGCPCount, pasGCPs);
    return psOptions;
}

/************************************************************************/
/*                              SrcToDst()                              */
/************************************************************************/

static void SrcToDst(double dfX, double dfY, double dfSrcXOff, double dfSrcYOff,
                     double dfSrcXSize, double dfSrcYSize, double dfDstXOff,
                     double dfDstYOff, double dfDstXSize, double dfDstYSize,
                     double &dfXOut, double &dfYOut)

{
    dfXOut = ((dfX - dfSrcXOff) / dfSrcXSize) * dfDstXSize + dfDstXOff;
    dfYOut = ((dfY - dfSrcYOff) / dfSrcYSize) * dfDstYSize + dfDstYOff;
}

/************************************************************************/
/*                          GetSrcDstWindow()                           */
/************************************************************************/

static bool FixSrcDstWindow(std::array<double, 4> &padfSrcWin,
                            std::array<double, 4> &padfDstWin,
                            int nSrcRasterXSize, int nSrcRasterYSize)

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
    if (dfModifiedSrcXOff < 0)
    {
        dfModifiedSrcXSize += dfModifiedSrcXOff;
        dfModifiedSrcXOff = 0;

        bModifiedX = true;
    }

    if (dfModifiedSrcYOff < 0)
    {
        dfModifiedSrcYSize += dfModifiedSrcYOff;
        dfModifiedSrcYOff = 0;
        bModifiedY = true;
    }

    if (dfModifiedSrcXOff + dfModifiedSrcXSize > nSrcRasterXSize)
    {
        dfModifiedSrcXSize = nSrcRasterXSize - dfModifiedSrcXOff;
        bModifiedX = true;
    }

    if (dfModifiedSrcYOff + dfModifiedSrcYSize > nSrcRasterYSize)
    {
        dfModifiedSrcYSize = nSrcRasterYSize - dfModifiedSrcYOff;
        bModifiedY = true;
    }

    /* -------------------------------------------------------------------- */
    /*      Don't do anything if the requesting region is completely off    */
    /*      the source image.                                               */
    /* -------------------------------------------------------------------- */
    if (dfModifiedSrcXOff >= nSrcRasterXSize ||
        dfModifiedSrcYOff >= nSrcRasterYSize || dfModifiedSrcXSize <= 0 ||
        dfModifiedSrcYSize <= 0)
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
    if (!bModifiedX && !bModifiedY)
        return true;

    /* -------------------------------------------------------------------- */
    /*      Now transform this possibly reduced request back into the       */
    /*      destination buffer coordinates in case the output region is     */
    /*      less than the whole buffer.                                     */
    /* -------------------------------------------------------------------- */
    double dfDstULX, dfDstULY, dfDstLRX, dfDstLRY;

    SrcToDst(dfModifiedSrcXOff, dfModifiedSrcYOff, dfSrcXOff, dfSrcYOff,
             dfSrcXSize, dfSrcYSize, dfDstXOff, dfDstYOff, dfDstXSize,
             dfDstYSize, dfDstULX, dfDstULY);
    SrcToDst(dfModifiedSrcXOff + dfModifiedSrcXSize,
             dfModifiedSrcYOff + dfModifiedSrcYSize, dfSrcXOff, dfSrcYOff,
             dfSrcXSize, dfSrcYSize, dfDstXOff, dfDstYOff, dfDstXSize,
             dfDstYSize, dfDstLRX, dfDstLRY);

    double dfModifiedDstXOff = dfDstXOff;
    double dfModifiedDstYOff = dfDstYOff;
    double dfModifiedDstXSize = dfDstXSize;
    double dfModifiedDstYSize = dfDstYSize;

    if (bModifiedX)
    {
        dfModifiedDstXOff = dfDstULX - dfDstXOff;
        dfModifiedDstXSize = (dfDstLRX - dfDstXOff) - dfModifiedDstXOff;

        dfModifiedDstXOff = std::max(0.0, dfModifiedDstXOff);
        if (dfModifiedDstXOff + dfModifiedDstXSize > dfDstXSize)
            dfModifiedDstXSize = dfDstXSize - dfModifiedDstXOff;
    }

    if (bModifiedY)
    {
        dfModifiedDstYOff = dfDstULY - dfDstYOff;
        dfModifiedDstYSize = (dfDstLRY - dfDstYOff) - dfModifiedDstYOff;

        dfModifiedDstYOff = std::max(0.0, dfModifiedDstYOff);
        if (dfModifiedDstYOff + dfModifiedDstYSize > dfDstYSize)
            dfModifiedDstYSize = dfDstYSize - dfModifiedDstYOff;
    }

    if (dfModifiedDstXSize <= 0.0 || dfModifiedDstYSize <= 0.0)
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
/*                        GDALTranslateFlush()                          */
/************************************************************************/

static GDALDatasetH GDALTranslateFlush(GDALDatasetH hOutDS)
{
    if (hOutDS != nullptr)
    {
        CPLErr eErrBefore = CPLGetLastErrorType();
        GDALFlushCache(hOutDS);
        if (eErrBefore == CE_None && CPLGetLastErrorType() != CE_None)
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

static CPLJSONObject Clone(const CPLJSONObject &obj)
{
    auto serialized = obj.Format(CPLJSONObject::PrettyFormat::Plain);
    CPLJSONDocument oJSONDocument;
    const GByte *pabyData = reinterpret_cast<const GByte *>(serialized.c_str());
    oJSONDocument.LoadMemory(pabyData);
    return oJSONDocument.GetRoot();
}

static void ReworkArray(CPLJSONObject &container, const CPLJSONObject &obj,
                        int nSrcBandCount,
                        const GDALTranslateOptions *psOptions)
{
    auto oArray = obj.ToArray();
    if (oArray.Size() == nSrcBandCount)
    {
        CPLJSONArray oNewArray;
        for (int nBand : psOptions->anBandList)
        {
            const int iSrcIdx = nBand - 1;
            oNewArray.Add(oArray[iSrcIdx]);
        }
        const auto childName(obj.GetName());
        container.Delete(childName);
        container.Add(childName, oNewArray);
    }
}

static CPLString
EditISIS3MetadataForBandChange(const char *pszJSON, int nSrcBandCount,
                               const GDALTranslateOptions *psOptions)
{
    CPLJSONDocument oJSONDocument;
    const GByte *pabyData = reinterpret_cast<const GByte *>(pszJSON);
    if (!oJSONDocument.LoadMemory(pabyData))
    {
        return CPLString();
    }

    auto oRoot = oJSONDocument.GetRoot();
    if (!oRoot.IsValid())
    {
        return CPLString();
    }

    auto oBandBin = oRoot.GetObj("IsisCube/BandBin");
    if (oBandBin.IsValid() && oBandBin.GetType() == CPLJSONObject::Type::Object)
    {
        // Backup original BandBin object
        oRoot.GetObj("IsisCube").Add("OriginalBandBin", Clone(oBandBin));

        // Iterate over BandBin members and reorder/resize its arrays that
        // have the same number of elements than the number of bands of the
        // source dataset.
        for (auto &child : oBandBin.GetChildren())
        {
            if (child.GetType() == CPLJSONObject::Type::Array)
            {
                ReworkArray(oBandBin, child, nSrcBandCount, psOptions);
            }
            else if (child.GetType() == CPLJSONObject::Type::Object)
            {
                auto oValue = child.GetObj("value");
                auto oUnit = child.GetObj("unit");
                if (oValue.GetType() == CPLJSONObject::Type::Array)
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

static double AdjustNoDataValue(double dfInputNoDataValue,
                                GDALRasterBand *poBand,
                                const GDALTranslateOptions *psOptions)
{
    bool bSignedByte = false;
    const char *pszPixelType =
        psOptions->aosCreateOptions.FetchNameValue("PIXELTYPE");
    if (pszPixelType == nullptr && poBand->GetRasterDataType() == GDT_Byte)
    {
        poBand->EnablePixelTypeSignedByteWarning(false);
        pszPixelType = poBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
        poBand->EnablePixelTypeSignedByteWarning(true);
    }
    if (pszPixelType != nullptr && EQUAL(pszPixelType, "SIGNEDBYTE"))
        bSignedByte = true;
    int bClamped = FALSE;
    int bRounded = FALSE;
    double dfVal = 0.0;
    const GDALDataType eBandType = poBand->GetRasterDataType();
    if (bSignedByte)
    {
        if (dfInputNoDataValue < -128.0)
        {
            dfVal = -128.0;
            bClamped = TRUE;
        }
        else if (dfInputNoDataValue > 127.0)
        {
            dfVal = 127.0;
            bClamped = TRUE;
        }
        else
        {
            dfVal = static_cast<int>(floor(dfInputNoDataValue + 0.5));
            if (dfVal != dfInputNoDataValue)
                bRounded = TRUE;
        }
    }
    else
    {
        dfVal = GDALAdjustValueToDataType(eBandType, dfInputNoDataValue,
                                          &bClamped, &bRounded);
    }

    if (bClamped)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "for band %d, nodata value has been clamped "
                 "to %.0f, the original value being out of range.",
                 poBand->GetBand(), dfVal);
    }
    else if (bRounded)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "for band %d, nodata value has been rounded "
                 "to %.0f, %s being an integer datatype.",
                 poBand->GetBand(), dfVal, GDALGetDataTypeName(eBandType));
    }
    return dfVal;
}

/************************************************************************/
/*                             GDALTranslate()                          */
/************************************************************************/

/* clang-format off */
/**
 * Converts raster data between different formats.
 *
 * This is the equivalent of the
 * <a href="/programs/gdal_translate.html">gdal_translate</a> utility.
 *
 * GDALTranslateOptions* must be allocated and freed with
 * GDALTranslateOptionsNew() and GDALTranslateOptionsFree() respectively.
 *
 * @param pszDest the destination dataset path.
 * @param hSrcDataset the source dataset handle.
 * @param psOptionsIn the options struct returned by GDALTranslateOptionsNew()
 * or NULL.
 * @param pbUsageError pointer to a integer output variable to store if any
 * usage error has occurred or NULL.
 * @return the output dataset (new dataset that must be closed using
 * GDALClose()) or NULL in case of error. If the output
 * format is a VRT dataset, then the returned VRT dataset has a reference to
 * hSrcDataset. Hence hSrcDataset should be closed after the returned dataset
 * if using GDALClose().
 * A safer alternative is to use GDALReleaseDataset() instead of using
 * GDALClose(), in which case you can close datasets in any order.
 *
 * @since GDAL 2.1
 */
/* clang-format on */

GDALDatasetH GDALTranslate(const char *pszDest, GDALDatasetH hSrcDataset,
                           const GDALTranslateOptions *psOptionsIn,
                           int *pbUsageError)

{
    CPLErrorReset();
    if (hSrcDataset == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No source dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }
    if (pszDest == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No target dataset specified.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        return nullptr;
    }

    GDALTranslateOptions *psOptions =
        (psOptionsIn) ? psOptionsIn->Clone()
                      : GDALTranslateOptionsNew(nullptr, nullptr);

    GDALDatasetH hOutDS = nullptr;
    bool bGotBounds = false;
    bool bGotGeoTransform = false;

    if (pbUsageError)
        *pbUsageError = FALSE;

    if (psOptions->adfULLR[0] != 0.0 || psOptions->adfULLR[1] != 0.0 ||
        psOptions->adfULLR[2] != 0.0 || psOptions->adfULLR[3] != 0.0)
        bGotBounds = true;

    if (psOptions->adfGT[0] != 0.0 || psOptions->adfGT[1] != 0.0 ||
        psOptions->adfGT[2] != 0.0 || psOptions->adfGT[3] != 0.0 ||
        psOptions->adfGT[4] != 0.0 || psOptions->adfGT[5] != 0.0)
        bGotGeoTransform = true;

    GDALDataset *poSrcDS = GDALDataset::FromHandle(hSrcDataset);
    const char *pszSource = poSrcDS->GetDescription();

    if (strcmp(pszSource, pszDest) == 0 && pszSource[0] != '\0' &&
        poSrcDS->GetDriver() != GDALGetDriverByName("MEM"))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Source and destination datasets must be different.");

        if (pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    CPLString osProjSRS;

    if (!psOptions->osProjSRS.empty())
    {
        OGRSpatialReference oSRS;
        oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

        if (oSRS.SetFromUserInput(psOptions->osProjSRS.c_str()) != OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to process SRS definition: %s",
                     psOptions->osProjSRS.c_str());
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }

        char *pszSRS = nullptr;
        oSRS.exportToWkt(&pszSRS);
        if (pszSRS)
            osProjSRS = pszSRS;
        CPLFree(pszSRS);
    }

    if (!psOptions->osOutputSRS.empty())
    {
        OGRSpatialReference oOutputSRS;
        if (oOutputSRS.SetFromUserInput(psOptions->osOutputSRS.c_str()) !=
            OGRERR_NONE)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to process SRS definition: %s",
                     psOptions->osOutputSRS.c_str());
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Check that incompatible options are not used                    */
    /* -------------------------------------------------------------------- */

    if ((psOptions->nOXSizePixel != 0 || psOptions->dfOXSizePct != 0.0 ||
         psOptions->nOYSizePixel != 0 || psOptions->dfOYSizePct != 0.0) &&
        (psOptions->dfXRes != 0 && psOptions->dfYRes != 0))
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-outsize and -tr options cannot be used at the same time.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }
    if ((bGotBounds | bGotGeoTransform) &&
        (psOptions->dfXRes != 0 && psOptions->dfYRes != 0))
    {
        CPLError(
            CE_Failure, CPLE_IllegalArg,
            "-a_ullr or -a_gt options cannot be used at the same time as -tr.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }
    if (bGotBounds && bGotGeoTransform)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "-a_ullr and -a_gt options cannot be used at the same time.");
        if (pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Collect some information from the source file.                  */
    /* -------------------------------------------------------------------- */
    if (psOptions->adfSrcWin[2] == 0 && psOptions->adfSrcWin[3] == 0)
    {
        psOptions->adfSrcWin[2] = poSrcDS->GetRasterXSize();
        psOptions->adfSrcWin[3] = poSrcDS->GetRasterYSize();
    }

    /* -------------------------------------------------------------------- */
    /*      Build band list to translate                                    */
    /* -------------------------------------------------------------------- */
    bool bAllBandsInOrder = true;

    if (psOptions->anBandList.empty())
    {

        psOptions->nBandCount = poSrcDS->GetRasterCount();
        if ((psOptions->nBandCount == 0) && (psOptions->bStrict))
        {
            // if not strict then the driver can fail if it doesn't support zero
            // bands
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Input file has no bands, and so cannot be translated.");
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }

        psOptions->anBandList.resize(psOptions->nBandCount);
        for (int i = 0; i < psOptions->nBandCount; i++)
            psOptions->anBandList[i] = i + 1;
    }
    else
    {
        for (int i = 0; i < psOptions->nBandCount; i++)
        {
            if (std::abs(psOptions->anBandList[i]) > poSrcDS->GetRasterCount())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Band %d requested, but only bands 1 to %d available.",
                         std::abs(psOptions->anBandList[i]),
                         poSrcDS->GetRasterCount());
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }

            if (psOptions->anBandList[i] != i + 1)
                bAllBandsInOrder = FALSE;
        }

        if (psOptions->nBandCount != poSrcDS->GetRasterCount())
            bAllBandsInOrder = FALSE;
    }

    if (static_cast<int>(psOptions->asScaleParams.size()) >
        psOptions->nBandCount)
    {
        if (!psOptions->bHasUsedExplicitScaleBand)
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "-scale has been specified more times than the number of "
                     "output bands");
        else
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "-scale_XX has been specified with XX greater than the "
                     "number of output bands");
        if (pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if (static_cast<int>(psOptions->adfExponent.size()) > psOptions->nBandCount)
    {
        if (!psOptions->bHasUsedExplicitExponentBand)
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "-exponent has been specified more times than the number "
                     "of output bands");
        else
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "-exponent_XX has been specified with XX greater than the "
                     "number of output bands");
        if (pbUsageError)
            *pbUsageError = TRUE;
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if (!psOptions->bQuiet && (psOptions->bSetScale || psOptions->bSetOffset) &&
        psOptions->bUnscale)
    {
        // Cf https://github.com/OSGeo/gdal/issues/7863
        CPLError(CE_Warning, CPLE_AppDefined,
                 "-a_scale/-a_offset are not applied by -unscale, but are set "
                 "after it, and -unscale uses the original source band "
                 "scale/offset values. "
                 "You may want to use -scale 0 1 %.16g %.16g instead. "
                 "This warning will not appear if -q is specified.",
                 psOptions->dfOffset, psOptions->dfOffset + psOptions->dfScale);
    }

    /* -------------------------------------------------------------------- */
    /*      Compute the source window from the projected source window      */
    /*      if the projected coordinates were provided.  Note that the      */
    /*      projected coordinates are in ulx, uly, lrx, lry format,         */
    /*      while the adfSrcWin is xoff, yoff, xsize, ysize with the        */
    /*      xoff,yoff being the ulx, uly in pixel/line.                     */
    /* -------------------------------------------------------------------- */
    const char *pszProjection = nullptr;

    if (psOptions->dfULX != 0.0 || psOptions->dfULY != 0.0 ||
        psOptions->dfLRX != 0.0 || psOptions->dfLRY != 0.0)
    {
        double adfGeoTransform[6];

        poSrcDS->GetGeoTransform(adfGeoTransform);

        if (adfGeoTransform[1] == 0.0 || adfGeoTransform[5] == 0.0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The -projwin option was used, but the geotransform is "
                     "invalid.");
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
        if (adfGeoTransform[2] != 0.0 || adfGeoTransform[4] != 0.0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "The -projwin option was used, but the geotransform is\n"
                     "rotated.  This configuration is not supported.");
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }

        if (!osProjSRS.empty())
        {
            pszProjection = poSrcDS->GetProjectionRef();
            if (pszProjection != nullptr && strlen(pszProjection) > 0)
            {
                OGRSpatialReference oSRSIn;
                OGRSpatialReference oSRSDS;
                oSRSIn.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                oSRSDS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                oSRSIn.SetFromUserInput(osProjSRS);
                oSRSDS.SetFromUserInput(pszProjection);
                if (!oSRSIn.IsSame(&oSRSDS))
                {
                    OGRCoordinateTransformation *poCT =
                        OGRCreateCoordinateTransformation(&oSRSIn, &oSRSDS);
                    if (!(poCT &&
                          poCT->Transform(1, &psOptions->dfULX,
                                          &psOptions->dfULY) &&
                          poCT->Transform(1, &psOptions->dfLRX,
                                          &psOptions->dfLRY)))
                    {
                        OGRCoordinateTransformation::DestroyCT(poCT);

                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "-projwin_srs ignored since coordinate "
                                 "transformation failed.");
                        GDALTranslateOptionsFree(psOptions);
                        return nullptr;
                    }
                    delete poCT;
                }
            }
            else
            {
                CPLError(CE_None, CPLE_None,
                         "-projwin_srs ignored since the dataset has no "
                         "projection.");
            }
        }

        psOptions->adfSrcWin[0] =
            (psOptions->dfULX - adfGeoTransform[0]) / adfGeoTransform[1];
        psOptions->adfSrcWin[1] =
            (psOptions->dfULY - adfGeoTransform[3]) / adfGeoTransform[5];

        psOptions->adfSrcWin[2] =
            (psOptions->dfLRX - psOptions->dfULX) / adfGeoTransform[1];
        psOptions->adfSrcWin[3] =
            (psOptions->dfLRY - psOptions->dfULY) / adfGeoTransform[5];

        // In case of nearest resampling, round to integer pixels (#6610)
        if (psOptions->osResampling.empty() ||
            EQUALN(psOptions->osResampling.c_str(), "NEAR", 4))
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
    if (psOptions->adfSrcWin[2] <= 0 || psOptions->adfSrcWin[3] <= 0)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Error: %s-srcwin %g %g %g %g has negative width and/or height.",
            (psOptions->dfULX != 0.0 || psOptions->dfULY != 0.0 ||
             psOptions->dfLRX != 0.0 || psOptions->dfLRY != 0.0)
                ? "Computed "
                : "",
            psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
            psOptions->adfSrcWin[2], psOptions->adfSrcWin[3]);
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Verify source window dimensions.                                */
    /* -------------------------------------------------------------------- */
    else if (psOptions->adfSrcWin[0] <= -1 || psOptions->adfSrcWin[1] <= -1 ||
             psOptions->adfSrcWin[0] + psOptions->adfSrcWin[2] >=
                 poSrcDS->GetRasterXSize() + 1 ||
             psOptions->adfSrcWin[1] + psOptions->adfSrcWin[3] >=
                 poSrcDS->GetRasterYSize() + 1)
    {
        const bool bCompletelyOutside =
            psOptions->adfSrcWin[0] + psOptions->adfSrcWin[2] <= 0 ||
            psOptions->adfSrcWin[1] + psOptions->adfSrcWin[3] <= 0 ||
            psOptions->adfSrcWin[0] >= poSrcDS->GetRasterXSize() ||
            psOptions->adfSrcWin[1] >= poSrcDS->GetRasterYSize();
        const bool bIsError =
            psOptions->bErrorOnPartiallyOutside ||
            (bCompletelyOutside && psOptions->bErrorOnCompletelyOutside);
        if (!psOptions->bQuiet || bIsError)
        {
            CPLErr eErr = bIsError ? CE_Failure : CE_Warning;

            CPLError(eErr, CPLE_AppDefined,
                     "%s-srcwin %g %g %g %g falls %s outside raster extent.%s",
                     (psOptions->dfULX != 0.0 || psOptions->dfULY != 0.0 ||
                      psOptions->dfLRX != 0.0 || psOptions->dfLRY != 0.0)
                         ? "Computed "
                         : "",
                     psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                     psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                     bCompletelyOutside ? "completely" : "partially",
                     bIsError ? "" : " Going on however.");
        }
        if (bIsError)
        {
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Find the output driver.                                         */
    /* -------------------------------------------------------------------- */
    if (psOptions->osFormat.empty())
    {
        const std::string osFormat = GetOutputDriverForRaster(pszDest);
        if (osFormat.empty())
        {
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
        psOptions->osFormat = osFormat;
    }

    GDALDriverH hDriver = GDALGetDriverByName(psOptions->osFormat.c_str());
    if (hDriver == nullptr)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Output driver `%s' not recognised.",
                 psOptions->osFormat.c_str());
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
    if (!psOptions->aosCreateOptions.FetchBool("APPEND_SUBDATASET", false))
    {
        if (!EQUAL(psOptions->osFormat.c_str(), "VRT"))
        {
            // Prevent GDALDriver::CreateCopy() from doing that again.
            psOptions->aosCreateOptions.SetNameValue(
                "@QUIET_DELETE_ON_CREATE_COPY", "NO");
        }

        GDALDriver::FromHandle(hDriver)->QuietDeleteForCreateCopy(pszDest,
                                                                  poSrcDS);

        // Make sure to load early overviews, so that on the GTiff driver
        // external .ovr is looked for before it might be created as the
        // output dataset !
        if (poSrcDS->GetRasterCount())
        {
            auto poBand = poSrcDS->GetRasterBand(1);
            if (poBand)
                poBand->GetOverviewCount();
        }
    }

    char **papszDriverMD = GDALGetMetadata(hDriver, nullptr);

    if (!CPLTestBool(
            CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_RASTER, "FALSE")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s driver has no raster capabilities.",
                 psOptions->osFormat.c_str());
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    if (!CPLTestBool(
            CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATE, "FALSE")) &&
        !CPLTestBool(
            CSLFetchNameValueDef(papszDriverMD, GDAL_DCAP_CREATECOPY, "FALSE")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s driver has no creation capabilities.",
                 psOptions->osFormat.c_str());
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      The short form is to CreateCopy().  We use this if the input    */
    /*      matches the whole dataset.  Eventually we should rewrite        */
    /*      this entire program to use virtual datasets to construct a      */
    /*      virtual input source to copy from.                              */
    /* -------------------------------------------------------------------- */

    const bool bKeepResolution =
        psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0 &&
        psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0 &&
        psOptions->dfXRes == 0.0;
    const bool bSpatialArrangementPreserved =
        psOptions->adfSrcWin[0] == 0 && psOptions->adfSrcWin[1] == 0 &&
        psOptions->adfSrcWin[2] == poSrcDS->GetRasterXSize() &&
        psOptions->adfSrcWin[3] == poSrcDS->GetRasterYSize() && bKeepResolution;

    if (psOptions->eOutputType == GDT_Unknown &&
        psOptions->asScaleParams.empty() && psOptions->adfExponent.empty() &&
        !psOptions->bUnscale && !psOptions->bSetScale &&
        !psOptions->bSetOffset && psOptions->aosMetadataOptions.empty() &&
        psOptions->aosDomainMetadataOptions.empty() && bAllBandsInOrder &&
        psOptions->eMaskMode == MASK_AUTO && bSpatialArrangementPreserved &&
        !psOptions->bNoGCP && psOptions->nGCPCount == 0 && !bGotBounds &&
        !bGotGeoTransform && psOptions->osOutputSRS.empty() &&
        psOptions->dfOutputCoordinateEpoch == 0 && !psOptions->bSetNoData &&
        !psOptions->bUnsetNoData && psOptions->nRGBExpand == 0 &&
        !psOptions->bNoRAT && psOptions->anColorInterp.empty() &&
        !psOptions->bNoXMP && psOptions->nOvLevel == OVR_LEVEL_AUTO)
    {

        // For gdal_translate_fuzzer
        if (psOptions->nLimitOutSize > 0)
        {
            vsi_l_offset nRawOutSize =
                static_cast<vsi_l_offset>(poSrcDS->GetRasterXSize()) *
                poSrcDS->GetRasterYSize() * psOptions->nBandCount;
            if (psOptions->nBandCount)
            {
                nRawOutSize *= GDALGetDataTypeSizeBytes(
                    poSrcDS->GetRasterBand(1)->GetRasterDataType());
            }
            if (nRawOutSize >
                static_cast<vsi_l_offset>(psOptions->nLimitOutSize))
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Attempt to create %dx%d dataset is above authorized "
                         "limit.",
                         poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize());
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Compute stats if required. */
        /* --------------------------------------------------------------------
         */

        if (psOptions->bStats && EQUAL(psOptions->osFormat.c_str(), "COG"))
        {
            psOptions->aosCreateOptions.SetNameValue("STATISTICS", "YES");
        }
        else if (psOptions->bStats)
        {
            for (int i = 0; i < poSrcDS->GetRasterCount(); i++)
            {
                double dfMin, dfMax, dfMean, dfStdDev;
                poSrcDS->GetRasterBand(i + 1)->ComputeStatistics(
                    psOptions->bApproxStats, &dfMin, &dfMax, &dfMean, &dfStdDev,
                    GDALDummyProgress, nullptr);
            }
        }

        hOutDS = GDALCreateCopy(
            hDriver, pszDest, GDALDataset::ToHandle(poSrcDS),
            psOptions->bStrict, psOptions->aosCreateOptions.List(),
            psOptions->pfnProgress, psOptions->pProgressData);
        hOutDS = GDALTranslateFlush(hOutDS);

        GDALTranslateOptionsFree(psOptions);
        return hOutDS;
    }

    if (psOptions->aosCreateOptions.FetchNameValue("COPY_SRC_OVERVIEWS"))
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
    if (poSrcDS->GetGeoTransform(adfSrcGeoTransform) == CE_None)
        bHasSrcGeoTransform = true;

    const bool bOutsizeExplicitlySet =
        !(psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0 &&
          psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0);
    if (psOptions->dfXRes != 0.0)
    {
        if (!(bHasSrcGeoTransform && psOptions->nGCPCount == 0 &&
              adfSrcGeoTransform[2] == 0.0 && adfSrcGeoTransform[4] == 0.0))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "The -tr option was used, but there's no geotransform or "
                     "it is\n"
                     "rotated.  This configuration is not supported.");
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
        const double dfOXSize = psOptions->adfSrcWin[2] / psOptions->dfXRes *
                                    adfSrcGeoTransform[1] +
                                0.5;
        const double dfOYSize = psOptions->adfSrcWin[3] / psOptions->dfYRes *
                                    fabs(adfSrcGeoTransform[5]) +
                                0.5;
        if (dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize) ||
            dfOYSize < 1 || !GDALIsValueInRange<int>(dfOXSize))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Invalid output size: %g x %g", dfOXSize, dfOYSize);
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
        nOXSize = static_cast<int>(dfOXSize);
        nOYSize = static_cast<int>(dfOYSize);
    }
    else if (!bOutsizeExplicitlySet)
    {
        double dfOXSize = ceil(psOptions->adfSrcWin[2] - 0.001);
        double dfOYSize = ceil(psOptions->adfSrcWin[3] - 0.001);
        if (dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize) ||
            dfOYSize < 1 || !GDALIsValueInRange<int>(dfOXSize))
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Invalid output size: %g x %g", dfOXSize, dfOYSize);
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
        nOXSize = static_cast<int>(dfOXSize);
        nOYSize = static_cast<int>(dfOYSize);
    }
    else
    {
        if (!(psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0))
        {
            if (psOptions->nOXSizePixel != 0)
                nOXSize = psOptions->nOXSizePixel;
            else
            {
                const double dfOXSize =
                    psOptions->dfOXSizePct / 100 * psOptions->adfSrcWin[2];
                if (dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize))
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid output width: %g", dfOXSize);
                    GDALTranslateOptionsFree(psOptions);
                    return nullptr;
                }
                nOXSize = static_cast<int>(dfOXSize);
            }
        }

        if (!(psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0))
        {
            if (psOptions->nOYSizePixel != 0)
                nOYSize = psOptions->nOYSizePixel;
            else
            {
                const double dfOYSize =
                    psOptions->dfOYSizePct / 100 * psOptions->adfSrcWin[3];
                if (dfOYSize < 1 || !GDALIsValueInRange<int>(dfOYSize))
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "Invalid output height: %g", dfOYSize);
                    GDALTranslateOptionsFree(psOptions);
                    return nullptr;
                }
                nOYSize = static_cast<int>(dfOYSize);
            }
        }

        if (psOptions->nOXSizePixel == 0 && psOptions->dfOXSizePct == 0.0)
        {
            const double dfOXSize = static_cast<double>(nOYSize) *
                                        psOptions->adfSrcWin[2] /
                                        psOptions->adfSrcWin[3] +
                                    0.5;
            if (dfOXSize < 1 || !GDALIsValueInRange<int>(dfOXSize))
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid output width: %g", dfOXSize);
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
            nOXSize = static_cast<int>(dfOXSize);
        }
        else if (psOptions->nOYSizePixel == 0 && psOptions->dfOYSizePct == 0.0)
        {
            const double dfOYSize = static_cast<double>(nOXSize) *
                                        psOptions->adfSrcWin[3] /
                                        psOptions->adfSrcWin[2] +
                                    0.5;
            if (dfOYSize < 1 || !GDALIsValueInRange<int>(dfOYSize))
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Invalid output height: %g", dfOYSize);
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
            nOYSize = static_cast<int>(dfOYSize);
        }
    }

    if (nOXSize <= 0 || nOYSize <= 0)
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Attempt to create %dx%d dataset is illegal.", nOXSize,
                 nOYSize);
        GDALTranslateOptionsFree(psOptions);
        return nullptr;
    }

    // Build overview dataset if -ovr is specified
    GDALDataset *poSrcOvrDS = nullptr;
    GDALDataset *poSrcDSOri = poSrcDS;
    const auto poFirstBand = poSrcDS->GetRasterBand(1);
    const int nOvCount = poFirstBand ? poFirstBand->GetOverviewCount() : 0;
    if (psOptions->nOvLevel < OVR_LEVEL_AUTO && poFirstBand && nOvCount > 0)
    {
        int iOvr = 0;
        for (; iOvr < nOvCount - 1; iOvr++)
        {
            if (poFirstBand->GetOverview(iOvr)->GetXSize() <= nOXSize)
            {
                break;
            }
        }
        iOvr += (psOptions->nOvLevel - OVR_LEVEL_AUTO);
        if (iOvr >= 0)
        {
            CPLDebug("GDAL", "Selecting overview level %d", iOvr);
            poSrcOvrDS = GDALCreateOverviewDataset(poSrcDS, iOvr,
                                                   /* bThisLevelOnly = */ true);
        }
    }
    else if (psOptions->nOvLevel >= OVR_LEVEL_NONE)
    {
        poSrcOvrDS = GDALCreateOverviewDataset(poSrcDS, psOptions->nOvLevel,
                                               /* bThisLevelOnly = */ true);
        if (poSrcOvrDS == nullptr)
        {
            if (!psOptions->bQuiet)
            {
                if (nOvCount > 0)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot get overview level %d. "
                             "Defaulting to level %d.",
                             psOptions->nOvLevel, nOvCount - 1);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Cannot get overview level %d. "
                             "Defaulting to full resolution.",
                             psOptions->nOvLevel);
                }
            }
            if (nOvCount > 0)
                poSrcOvrDS =
                    GDALCreateOverviewDataset(poSrcDS, nOvCount - 1,
                                              /* bThisLevelOnly = */ true);
        }
        if (poSrcOvrDS && psOptions->dfXRes == 0.0 && !bOutsizeExplicitlySet)
        {
            const double dfRatioX =
                static_cast<double>(poSrcDSOri->GetRasterXSize()) /
                poSrcOvrDS->GetRasterXSize();
            const double dfRatioY =
                static_cast<double>(poSrcDSOri->GetRasterYSize()) /
                poSrcOvrDS->GetRasterYSize();
            nOXSize =
                std::max(1, static_cast<int>(ceil(nOXSize / dfRatioX - 0.001)));
            nOYSize =
                std::max(1, static_cast<int>(ceil(nOYSize / dfRatioY - 0.001)));
        }
    }

    if (poSrcOvrDS)
        poSrcDS = poSrcOvrDS;
    else
        poSrcDS->Reference();

    // For gdal_translate_fuzzer
    if (psOptions->nLimitOutSize > 0)
    {
        vsi_l_offset nRawOutSize = static_cast<vsi_l_offset>(nOXSize) * nOYSize;
        if (psOptions->nBandCount)
        {
            if (nRawOutSize > std::numeric_limits<vsi_l_offset>::max() /
                                  psOptions->nBandCount)
            {
                GDALTranslateOptionsFree(psOptions);
                return nullptr;
            }
            nRawOutSize *= psOptions->nBandCount;
            const int nDTSize = GDALGetDataTypeSizeBytes(
                poSrcDS->GetRasterBand(1)->GetRasterDataType());
            if (nDTSize > 0 &&
                nRawOutSize >
                    std::numeric_limits<vsi_l_offset>::max() / nDTSize)
            {
                GDALTranslateOptionsFree(psOptions);
                poSrcDS->Release();
                return nullptr;
            }
            nRawOutSize *= nDTSize;
        }
        if (nRawOutSize > static_cast<vsi_l_offset>(psOptions->nLimitOutSize))
        {
            CPLError(
                CE_Failure, CPLE_IllegalArg,
                "Attempt to create %dx%d dataset is above authorized limit.",
                nOXSize, nOYSize);
            GDALTranslateOptionsFree(psOptions);
            poSrcDS->Release();
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

    if (psOptions->nGCPCount == 0)
    {
        OGRSpatialReference oSRS;
        if (!psOptions->osOutputSRS.empty())
        {
            oSRS.SetFromUserInput(psOptions->osOutputSRS.c_str());
            oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
        else
        {
            const OGRSpatialReference *poSrcSRS = poSrcDS->GetSpatialRef();
            if (poSrcSRS)
                oSRS = *poSrcSRS;
        }
        if (!oSRS.IsEmpty())
        {
            if (psOptions->dfOutputCoordinateEpoch > 0)
                oSRS.SetCoordinateEpoch(psOptions->dfOutputCoordinateEpoch);
            poVDS->SetSpatialRef(&oSRS);
        }
    }

    bool bHasDstGeoTransform = false;
    double adfDstGeoTransform[6] = {};

    if (bGotBounds)
    {
        bHasDstGeoTransform = true;
        adfDstGeoTransform[0] = psOptions->adfULLR[0];
        adfDstGeoTransform[1] =
            (psOptions->adfULLR[2] - psOptions->adfULLR[0]) / nOXSize;
        adfDstGeoTransform[2] = 0.0;
        adfDstGeoTransform[3] = psOptions->adfULLR[1];
        adfDstGeoTransform[4] = 0.0;
        adfDstGeoTransform[5] =
            (psOptions->adfULLR[3] - psOptions->adfULLR[1]) / nOYSize;

        poVDS->SetGeoTransform(adfDstGeoTransform);
    }

    else if (bGotGeoTransform)
    {
        bHasDstGeoTransform = true;
        for (int i = 0; i < 6; i++)
            adfDstGeoTransform[i] = psOptions->adfGT[i];
        poVDS->SetGeoTransform(adfDstGeoTransform);
    }

    else if (bHasSrcGeoTransform && psOptions->nGCPCount == 0)
    {
        bHasDstGeoTransform = true;
        memcpy(adfDstGeoTransform, adfSrcGeoTransform, 6 * sizeof(double));
        adfDstGeoTransform[0] +=
            psOptions->adfSrcWin[0] * adfDstGeoTransform[1] +
            psOptions->adfSrcWin[1] * adfDstGeoTransform[2];
        adfDstGeoTransform[3] +=
            psOptions->adfSrcWin[0] * adfDstGeoTransform[4] +
            psOptions->adfSrcWin[1] * adfDstGeoTransform[5];

        const double dfX = static_cast<double>(nOXSize);
        const double dfY = static_cast<double>(nOYSize);
        adfDstGeoTransform[1] *= psOptions->adfSrcWin[2] / dfX;
        adfDstGeoTransform[2] *= psOptions->adfSrcWin[3] / dfY;
        adfDstGeoTransform[4] *= psOptions->adfSrcWin[2] / dfX;
        adfDstGeoTransform[5] *= psOptions->adfSrcWin[3] / dfY;

        if (psOptions->dfXRes != 0.0)
        {
            adfDstGeoTransform[1] = psOptions->dfXRes;
            adfDstGeoTransform[5] = (adfDstGeoTransform[5] > 0)
                                        ? psOptions->dfYRes
                                        : -psOptions->dfYRes;
        }

        poVDS->SetGeoTransform(adfDstGeoTransform);
    }

    if (psOptions->nGCPCount != 0)
    {
        OGRSpatialReference oSRS;
        if (!psOptions->osOutputSRS.empty())
        {
            oSRS.SetFromUserInput(psOptions->osOutputSRS.c_str());
            oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
        else
        {
            const OGRSpatialReference *poSrcSRS = poSrcDS->GetGCPSpatialRef();
            if (poSrcSRS)
                oSRS = *poSrcSRS;
        }
        poVDS->SetGCPs(psOptions->nGCPCount, psOptions->pasGCPs,
                       !oSRS.IsEmpty() ? &oSRS : nullptr);
    }

    else if (!psOptions->bNoGCP && poSrcDSOri->GetGCPCount() > 0)
    {
        const int nGCPs = poSrcDSOri->GetGCPCount();

        GDAL_GCP *pasGCPs = GDALDuplicateGCPs(nGCPs, poSrcDSOri->GetGCPs());

        for (int i = 0; i < nGCPs; i++)
        {
            pasGCPs[i].dfGCPPixel -= psOptions->adfSrcWin[0];
            pasGCPs[i].dfGCPLine -= psOptions->adfSrcWin[1];
            pasGCPs[i].dfGCPPixel *=
                nOXSize / static_cast<double>(psOptions->adfSrcWin[2]);
            pasGCPs[i].dfGCPLine *=
                nOYSize / static_cast<double>(psOptions->adfSrcWin[3]);
        }

        poVDS->SetGCPs(nGCPs, pasGCPs, poSrcDSOri->GetGCPSpatialRef());

        GDALDeinitGCPs(nGCPs, pasGCPs);
        CPLFree(pasGCPs);
    }

    /* -------------------------------------------------------------------- */
    /*      To make the VRT to look less awkward (but this is optional      */
    /*      in fact), avoid negative values.                                */
    /* -------------------------------------------------------------------- */
    std::array<double, 4> adfDstWin = {0.0, 0.0, static_cast<double>(nOXSize),
                                       static_cast<double>(nOYSize)};

    // When specifying -tr with non-nearest resampling, make sure that the
    // size of target window precisely matches the requested resolution, to
    // avoid any shift.
    if (bHasSrcGeoTransform && bHasDstGeoTransform &&
        psOptions->dfXRes != 0.0 && !psOptions->osResampling.empty() &&
        !EQUALN(psOptions->osResampling.c_str(), "NEAR", 4))
    {
        adfDstWin[2] = psOptions->adfSrcWin[2] * adfSrcGeoTransform[1] /
                       adfDstGeoTransform[1];
        adfDstWin[3] = psOptions->adfSrcWin[3] *
                       fabs(adfSrcGeoTransform[5] / adfDstGeoTransform[5]);
    }

    const std::array<double, 4> adfSrcWinOri(psOptions->adfSrcWin);
    const double dfRatioX =
        poSrcDS->GetRasterXSize() == 0
            ? 1.0
            : static_cast<double>(poSrcDSOri->GetRasterXSize()) /
                  poSrcDS->GetRasterXSize();
    const double dfRatioY =
        poSrcDS->GetRasterYSize() == 0
            ? 1.0
            : static_cast<double>(poSrcDSOri->GetRasterYSize()) /
                  poSrcDS->GetRasterYSize();
    psOptions->adfSrcWin[0] /= dfRatioX;
    psOptions->adfSrcWin[1] /= dfRatioY;
    psOptions->adfSrcWin[2] /= dfRatioX;
    psOptions->adfSrcWin[3] /= dfRatioY;
    FixSrcDstWindow(psOptions->adfSrcWin, adfDstWin, poSrcDS->GetRasterXSize(),
                    poSrcDS->GetRasterYSize());

    /* -------------------------------------------------------------------- */
    /*      Transfer generally applicable metadata.                         */
    /* -------------------------------------------------------------------- */
    char **papszMetadata = CSLDuplicate(poSrcDS->GetMetadata());
    if (!psOptions->asScaleParams.empty() || psOptions->bUnscale ||
        psOptions->eOutputType != GDT_Unknown)
    {
        /* Remove TIFFTAG_MINSAMPLEVALUE and TIFFTAG_MAXSAMPLEVALUE */
        /* if the data range may change because of options */
        char **papszIter = papszMetadata;
        while (papszIter && *papszIter)
        {
            if (STARTS_WITH_CI(*papszIter, "TIFFTAG_MINSAMPLEVALUE=") ||
                STARTS_WITH_CI(*papszIter, "TIFFTAG_MAXSAMPLEVALUE="))
            {
                CPLFree(*papszIter);
                memmove(papszIter, papszIter + 1,
                        sizeof(char *) * (CSLCount(papszIter + 1) + 1));
            }
            else
                papszIter++;
        }
    }

    // Remove NITF_BLOCKA_ stuff if georeferencing is changed
    if (!(psOptions->adfSrcWin[0] == 0 && psOptions->adfSrcWin[1] == 0 &&
          psOptions->adfSrcWin[2] == poSrcDS->GetRasterXSize() &&
          psOptions->adfSrcWin[3] == poSrcDS->GetRasterYSize() &&
          psOptions->nGCPCount == 0 && !bGotBounds && !bGotGeoTransform))
    {
        char **papszIter = papszMetadata;
        while (papszIter && *papszIter)
        {
            if (STARTS_WITH_CI(*papszIter, "NITF_BLOCKA_"))
            {
                CPLFree(*papszIter);
                memmove(papszIter, papszIter + 1,
                        sizeof(char *) * (CSLCount(papszIter + 1) + 1));
            }
            else
                papszIter++;
        }
    }

    {
        char **papszIter = papszMetadata;
        while (papszIter && *papszIter)
        {
            // Do not preserve the CACHE_PATH from the WMS driver
            if (STARTS_WITH_CI(*papszIter, "CACHE_PATH="))
            {
                CPLFree(*papszIter);
                memmove(papszIter, papszIter + 1,
                        sizeof(char *) * (CSLCount(papszIter + 1) + 1));
            }
            else
                papszIter++;
        }
    }

    poVDS->SetMetadata(papszMetadata);
    CSLDestroy(papszMetadata);
    AttachMetadata(GDALDataset::ToHandle(poVDS), psOptions->aosMetadataOptions);

    AttachDomainMetadata(GDALDataset::ToHandle(poVDS),
                         psOptions->aosDomainMetadataOptions);

    const char *pszInterleave =
        poSrcDS->GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE");
    if (pszInterleave)
        poVDS->SetMetadataItem("INTERLEAVE", pszInterleave, "IMAGE_STRUCTURE");

    {
        const char *pszCompression =
            poSrcDS->GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE");
        if (pszCompression)
        {
            poVDS->SetMetadataItem("COMPRESSION", pszCompression,
                                   "IMAGE_STRUCTURE");
        }
    }

    /* ISIS3 metadata preservation */
    char **papszMD_ISIS3 = poSrcDS->GetMetadata("json:ISIS3");
    if (papszMD_ISIS3 != nullptr)
    {
        if (!bAllBandsInOrder)
        {
            CPLString osJSON = EditISIS3MetadataForBandChange(
                papszMD_ISIS3[0], poSrcDS->GetRasterCount(), psOptions);
            if (!osJSON.empty())
            {
                char *apszMD[] = {&osJSON[0], nullptr};
                poVDS->SetMetadata(apszMD, "json:ISIS3");
            }
        }
        else
        {
            poVDS->SetMetadata(papszMD_ISIS3, "json:ISIS3");
        }
    }

    // PDS4 -> PDS4 special case
    if (EQUAL(psOptions->osFormat.c_str(), "PDS4"))
    {
        char **papszMD_PDS4 = poSrcDS->GetMetadata("xml:PDS4");
        if (papszMD_PDS4 != nullptr)
            poVDS->SetMetadata(papszMD_PDS4, "xml:PDS4");
    }

    // VICAR -> VICAR special case
    if (EQUAL(psOptions->osFormat.c_str(), "VICAR"))
    {
        char **papszMD_VICAR = poSrcDS->GetMetadata("json:VICAR");
        if (papszMD_VICAR != nullptr)
            poVDS->SetMetadata(papszMD_VICAR, "json:VICAR");
    }

    // Copy XMP metadata
    if (!psOptions->bNoXMP)
    {
        char **papszXMP = poSrcDS->GetMetadata("xml:XMP");
        if (papszXMP != nullptr && *papszXMP != nullptr)
        {
            poVDS->SetMetadata(papszXMP, "xml:XMP");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Transfer metadata that remains valid if the spatial             */
    /*      arrangement of the data is unaltered.                           */
    /* -------------------------------------------------------------------- */
    if (bSpatialArrangementPreserved)
    {
        char **papszMD = poSrcDS->GetMetadata("RPC");
        if (papszMD != nullptr)
            poVDS->SetMetadata(papszMD, "RPC");

        papszMD = poSrcDS->GetMetadata("GEOLOCATION");
        if (papszMD != nullptr)
            poVDS->SetMetadata(papszMD, "GEOLOCATION");
    }
    else
    {
        char **papszMD = poSrcDSOri->GetMetadata("RPC");
        if (papszMD != nullptr)
        {
            papszMD = CSLDuplicate(papszMD);

            double dfSAMP_OFF =
                CPLAtof(CSLFetchNameValueDef(papszMD, "SAMP_OFF", "0"));
            double dfLINE_OFF =
                CPLAtof(CSLFetchNameValueDef(papszMD, "LINE_OFF", "0"));
            double dfSAMP_SCALE =
                CPLAtof(CSLFetchNameValueDef(papszMD, "SAMP_SCALE", "1"));
            double dfLINE_SCALE =
                CPLAtof(CSLFetchNameValueDef(papszMD, "LINE_SCALE", "1"));

            dfSAMP_OFF -= adfSrcWinOri[0];
            dfLINE_OFF -= adfSrcWinOri[1];

            const double df2 = adfSrcWinOri[2];
            const double df3 = adfSrcWinOri[3];
            dfSAMP_OFF *= nOXSize / df2;
            dfLINE_OFF *= nOYSize / df3;
            dfSAMP_SCALE *= nOXSize / df2;
            dfLINE_SCALE *= nOYSize / df3;

            CPLString osField;
            osField.Printf("%.15g", dfLINE_OFF);
            papszMD = CSLSetNameValue(papszMD, "LINE_OFF", osField);

            osField.Printf("%.15g", dfSAMP_OFF);
            papszMD = CSLSetNameValue(papszMD, "SAMP_OFF", osField);

            osField.Printf("%.15g", dfLINE_SCALE);
            papszMD = CSLSetNameValue(papszMD, "LINE_SCALE", osField);

            osField.Printf("%.15g", dfSAMP_SCALE);
            papszMD = CSLSetNameValue(papszMD, "SAMP_SCALE", osField);

            poVDS->SetMetadata(papszMD, "RPC");
            CSLDestroy(papszMD);
        }
    }

    const int nSrcBandCount = psOptions->nBandCount;

    if (psOptions->nRGBExpand != 0)
    {
        GDALRasterBand *poSrcBand =
            poSrcDS->GetRasterBand(std::abs(psOptions->anBandList[0]));
        if (psOptions->anBandList[0] < 0)
            poSrcBand = poSrcBand->GetMaskBand();
        GDALColorTable *poColorTable = poSrcBand->GetColorTable();
        if (poColorTable == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Error : band %d has no color table",
                     std::abs(psOptions->anBandList[0]));
            GDALClose(poVDS);
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }

        /* Check that the color table only contains gray levels */
        /* when using -expand gray */
        if (psOptions->nRGBExpand == 1)
        {
            int nColorCount = poColorTable->GetColorEntryCount();
            for (int nColor = 0; nColor < nColorCount; nColor++)
            {
                const GDALColorEntry *poEntry =
                    poColorTable->GetColorEntry(nColor);
                if (poEntry->c1 != poEntry->c2 || poEntry->c1 != poEntry->c3)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Warning : color table contains non gray levels "
                             "colors");
                    break;
                }
            }
        }

        if (psOptions->nBandCount == 1)
        {
            psOptions->nBandCount = psOptions->nRGBExpand;
        }
        else if (psOptions->nBandCount == 2 &&
                 (psOptions->nRGBExpand == 3 || psOptions->nRGBExpand == 4))
        {
            psOptions->nBandCount = psOptions->nRGBExpand;
        }
        else
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "Error : invalid use of -expand option.");
            GDALClose(poVDS);
            GDALTranslateOptionsFree(psOptions);
            return nullptr;
        }
    }

    // Can be set to TRUE in the band loop too
    bool bFilterOutStatsMetadata =
        !psOptions->asScaleParams.empty() || psOptions->bUnscale ||
        !bSpatialArrangementPreserved || psOptions->nRGBExpand != 0;

    if (static_cast<int>(psOptions->anColorInterp.size()) >
        psOptions->nBandCount)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "More bands defined in -colorinterp than output bands");
    }

    /* ==================================================================== */
    /*      Process all bands.                                              */
    /* ==================================================================== */
    GDALDataType eOutputType = psOptions->eOutputType;

    for (int i = 0; i < psOptions->nBandCount; i++)
    {
        int nComponent = 0;
        int nSrcBand = 0;

        if (psOptions->nRGBExpand != 0)
        {
            if (nSrcBandCount == 2 && psOptions->nRGBExpand == 4 && i == 3)
                nSrcBand = psOptions->anBandList[1];
            else
            {
                nSrcBand = psOptions->anBandList[0];
                nComponent = i + 1;
            }
        }
        else
        {
            nSrcBand = psOptions->anBandList[i];
        }

        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(std::abs(nSrcBand));

        /* --------------------------------------------------------------------
         */
        /*      Select output data type to match source. */
        /* --------------------------------------------------------------------
         */
        GDALRasterBand *poRealSrcBand =
            (nSrcBand < 0) ? poSrcBand->GetMaskBand() : poSrcBand;
        GDALDataType eBandType;
        if (eOutputType == GDT_Unknown)
        {
            eBandType = poRealSrcBand->GetRasterDataType();
            if (eBandType != GDT_Byte && psOptions->nRGBExpand != 0)
            {
                // Use case of https://github.com/OSGeo/gdal/issues/9402
                if (const auto poColorTable = poRealSrcBand->GetColorTable())
                {
                    bool bIn0To255Range = true;
                    const int nColorCount = poColorTable->GetColorEntryCount();
                    for (int nColor = 0; nColor < nColorCount; nColor++)
                    {
                        const GDALColorEntry *poEntry =
                            poColorTable->GetColorEntry(nColor);
                        if (poEntry->c1 > 255 || poEntry->c2 > 255 ||
                            poEntry->c3 > 255 || poEntry->c4 > 255)
                        {
                            bIn0To255Range = false;
                            break;
                        }
                    }
                    if (bIn0To255Range)
                    {
                        if (!psOptions->bQuiet)
                        {
                            CPLError(CE_Warning, CPLE_AppDefined,
                                     "Using Byte output data type due to range "
                                     "of values in color table");
                        }
                        eBandType = GDT_Byte;
                    }
                }
                eOutputType = eBandType;
            }
        }
        else
        {
            eBandType = eOutputType;

            // Check that we can copy existing statistics
            GDALDataType eSrcBandType = poRealSrcBand->GetRasterDataType();
            const char *pszMin =
                poRealSrcBand->GetMetadataItem("STATISTICS_MINIMUM");
            const char *pszMax =
                poRealSrcBand->GetMetadataItem("STATISTICS_MAXIMUM");
            if (!bFilterOutStatsMetadata && eBandType != eSrcBandType &&
                pszMin != nullptr && pszMax != nullptr)
            {
                const bool bSrcIsInteger =
                    CPL_TO_BOOL(GDALDataTypeIsInteger(eSrcBandType) &&
                                !GDALDataTypeIsComplex(eSrcBandType));
                const bool bDstIsInteger =
                    CPL_TO_BOOL(GDALDataTypeIsInteger(eBandType) &&
                                !GDALDataTypeIsComplex(eBandType));
                if (bSrcIsInteger && bDstIsInteger)
                {
                    std::int64_t nDstMin = 0;
                    std::uint64_t nDstMax = 0;
                    switch (eBandType)
                    {
                        case GDT_Byte:
                            nDstMin = std::numeric_limits<std::uint8_t>::min();
                            nDstMax = std::numeric_limits<std::uint8_t>::max();
                            break;
                        case GDT_Int8:
                            nDstMin = std::numeric_limits<std::int8_t>::min();
                            nDstMax = std::numeric_limits<std::int8_t>::max();
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
                        if (nMin < nDstMin || nMax > nDstMax)
                            bFilterOutStatsMetadata = true;
                    }
                    catch (const std::exception &)
                    {
                    }
                }
                // Float64 is large enough to hold all integer <= 32 bit or
                // float32 values there might be other OK cases, but ere on safe
                // side for now
                else if (!((bSrcIsInteger || eSrcBandType == GDT_Float32) &&
                           eBandType == GDT_Float64))
                {
                    bFilterOutStatsMetadata = true;
                }
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Create this band. */
        /* --------------------------------------------------------------------
         */
        CPLStringList aosAddBandOptions;
        int nSrcBlockXSize, nSrcBlockYSize;
        poSrcBand->GetBlockSize(&nSrcBlockXSize, &nSrcBlockYSize);
        if (bKeepResolution &&
            (fmod(psOptions->adfSrcWin[0], nSrcBlockXSize)) == 0 &&
            (fmod(psOptions->adfSrcWin[1], nSrcBlockYSize)) == 0)
        {
            aosAddBandOptions.SetNameValue("BLOCKXSIZE",
                                           CPLSPrintf("%d", nSrcBlockXSize));
            aosAddBandOptions.SetNameValue("BLOCKYSIZE",
                                           CPLSPrintf("%d", nSrcBlockYSize));
        }
        const char *pszBlockXSize =
            psOptions->aosCreateOptions.FetchNameValue("BLOCKXSIZE");
        if (pszBlockXSize)
            aosAddBandOptions.SetNameValue("BLOCKXSIZE", pszBlockXSize);
        const char *pszBlockYSize =
            psOptions->aosCreateOptions.FetchNameValue("BLOCKYSIZE");
        if (pszBlockYSize)
            aosAddBandOptions.SetNameValue("BLOCKYSIZE", pszBlockYSize);
        poVDS->AddBand(eBandType, aosAddBandOptions.List());
        VRTSourcedRasterBand *poVRTBand =
            static_cast<VRTSourcedRasterBand *>(poVDS->GetRasterBand(i + 1));

        if (nSrcBand < 0)
        {
            poVRTBand->AddMaskBandSource(
                poSrcBand, psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                psOptions->adfSrcWin[2], psOptions->adfSrcWin[3], adfDstWin[0],
                adfDstWin[1], adfDstWin[2], adfDstWin[3]);

            // Color interpretation override
            if (!psOptions->anColorInterp.empty())
            {
                if (i < static_cast<int>(psOptions->anColorInterp.size()) &&
                    psOptions->anColorInterp[i] >= 0)
                {
                    poVRTBand->SetColorInterpretation(
                        static_cast<GDALColorInterp>(
                            psOptions->anColorInterp[i]));
                }
            }

            continue;
        }

        // Preserve NBITS if no option change values
        const char *pszNBits =
            poSrcBand->GetMetadataItem("NBITS", "IMAGE_STRUCTURE");
        if (pszNBits && psOptions->nRGBExpand == 0 &&
            psOptions->asScaleParams.empty() && !psOptions->bUnscale &&
            psOptions->eOutputType == GDT_Unknown &&
            psOptions->osResampling.empty())
        {
            poVRTBand->SetMetadataItem("NBITS", pszNBits, "IMAGE_STRUCTURE");
        }

        // Preserve PIXELTYPE if no option change values
        if (poSrcBand->GetRasterDataType() == GDT_Byte &&
            psOptions->nRGBExpand == 0 && psOptions->asScaleParams.empty() &&
            !psOptions->bUnscale && psOptions->eOutputType == GDT_Unknown &&
            psOptions->osResampling.empty())
        {
            poSrcBand->EnablePixelTypeSignedByteWarning(false);
            const char *pszPixelType =
                poSrcBand->GetMetadataItem("PIXELTYPE", "IMAGE_STRUCTURE");
            poSrcBand->EnablePixelTypeSignedByteWarning(true);
            if (pszPixelType)
            {
                poVRTBand->SetMetadataItem("PIXELTYPE", pszPixelType,
                                           "IMAGE_STRUCTURE");
            }
        }

        const char *pszCompression =
            poSrcBand->GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE");
        if (pszCompression)
        {
            poVRTBand->SetMetadataItem("COMPRESSION", pszCompression,
                                       "IMAGE_STRUCTURE");
        }

        /* --------------------------------------------------------------------
         */
        /*      Do we need to collect scaling information? */
        /* --------------------------------------------------------------------
         */
        double dfScale = 1.0;
        double dfOffset = 0.0;
        bool bScale = false;
        bool bHaveScaleSrc = false;
        double dfScaleSrcMin = 0.0;
        double dfScaleSrcMax = 0.0;
        double dfScaleDstMin = 0.0;
        double dfScaleDstMax = 0.0;
        bool bExponentScaling = false;
        double dfExponent = 0.0;

        if (i < static_cast<int>(psOptions->asScaleParams.size()) &&
            psOptions->asScaleParams[i].bScale)
        {
            bScale = psOptions->asScaleParams[i].bScale;
            bHaveScaleSrc = psOptions->asScaleParams[i].bHaveScaleSrc;
            dfScaleSrcMin = psOptions->asScaleParams[i].dfScaleSrcMin;
            dfScaleSrcMax = psOptions->asScaleParams[i].dfScaleSrcMax;
            dfScaleDstMin = psOptions->asScaleParams[i].dfScaleDstMin;
            dfScaleDstMax = psOptions->asScaleParams[i].dfScaleDstMax;
        }
        else if (psOptions->asScaleParams.size() == 1 &&
                 !psOptions->bHasUsedExplicitScaleBand)
        {
            bScale = psOptions->asScaleParams[0].bScale;
            bHaveScaleSrc = psOptions->asScaleParams[0].bHaveScaleSrc;
            dfScaleSrcMin = psOptions->asScaleParams[0].dfScaleSrcMin;
            dfScaleSrcMax = psOptions->asScaleParams[0].dfScaleSrcMax;
            dfScaleDstMin = psOptions->asScaleParams[0].dfScaleDstMin;
            dfScaleDstMax = psOptions->asScaleParams[0].dfScaleDstMax;
        }

        if (i < static_cast<int>(psOptions->adfExponent.size()) &&
            psOptions->adfExponent[i] != 0.0)
        {
            bExponentScaling = TRUE;
            dfExponent = psOptions->adfExponent[i];
        }
        else if (psOptions->adfExponent.size() == 1 &&
                 !psOptions->bHasUsedExplicitExponentBand)
        {
            bExponentScaling = TRUE;
            dfExponent = psOptions->adfExponent[0];
        }

        if (bExponentScaling && !bScale)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "For band %d, -scale should be specified when -exponent "
                     "is specified.",
                     i + 1);
            if (pbUsageError)
                *pbUsageError = TRUE;
            GDALTranslateOptionsFree(psOptions);
            delete poVDS;
            poSrcDS->Release();
            return nullptr;
        }

        if (bScale && !bHaveScaleSrc)
        {
            double adfCMinMax[2] = {};
            GDALComputeRasterMinMax(poSrcBand, TRUE, adfCMinMax);
            dfScaleSrcMin = adfCMinMax[0];
            dfScaleSrcMax = adfCMinMax[1];
        }

        if (bScale)
        {
            /* To avoid a divide by zero */
            if (dfScaleSrcMax == dfScaleSrcMin)
                dfScaleSrcMax += 0.1;

            // Can still occur for very big values
            if (dfScaleSrcMax == dfScaleSrcMin)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "-scale cannot be applied due to source "
                         "minimum and maximum being equal");
                GDALTranslateOptionsFree(psOptions);
                delete poVDS;
                poSrcDS->Release();
                return nullptr;
            }

            if (!bExponentScaling)
            {
                dfScale = (dfScaleDstMax - dfScaleDstMin) /
                          (dfScaleSrcMax - dfScaleSrcMin);
                dfOffset = -1 * dfScaleSrcMin * dfScale + dfScaleDstMin;
            }
        }

        if (psOptions->bUnscale)
        {
            dfScale = poSrcBand->GetScale();
            dfOffset = poSrcBand->GetOffset();
        }

        /* --------------------------------------------------------------------
         */
        /*      Create a simple or complex data source depending on the */
        /*      translation type required. */
        /* --------------------------------------------------------------------
         */
        VRTSimpleSource *poSimpleSource = nullptr;
        if (psOptions->bUnscale || bScale ||
            (psOptions->nRGBExpand != 0 && i < psOptions->nRGBExpand))
        {
            VRTComplexSource *poSource = new VRTComplexSource();

            /* --------------------------------------------------------------------
             */
            /*      Set complex parameters. */
            /* --------------------------------------------------------------------
             */

            if (dfOffset != 0.0 || dfScale != 1.0)
            {
                poSource->SetLinearScaling(dfOffset, dfScale);
            }
            else if (bExponentScaling)
            {
                poSource->SetPowerScaling(dfExponent, dfScaleSrcMin,
                                          dfScaleSrcMax, dfScaleDstMin,
                                          dfScaleDstMax);
            }

            poSource->SetColorTableComponent(nComponent);

            int bSuccess;
            double dfNoData = poSrcBand->GetNoDataValue(&bSuccess);
            if (bSuccess)
            {
                poSource->SetNoDataValue(dfNoData);
            }

            poSimpleSource = poSource;
        }
        else
        {
            poSimpleSource = new VRTSimpleSource();
        }

        poSimpleSource->SetResampling(psOptions->osResampling.empty()
                                          ? nullptr
                                          : psOptions->osResampling.c_str());
        poVRTBand->ConfigureSource(
            poSimpleSource, poSrcBand, FALSE, psOptions->adfSrcWin[0],
            psOptions->adfSrcWin[1], psOptions->adfSrcWin[2],
            psOptions->adfSrcWin[3], adfDstWin[0], adfDstWin[1], adfDstWin[2],
            adfDstWin[3]);

        poVRTBand->AddSource(poSimpleSource);

        /* --------------------------------------------------------------------
         */
        /*      In case of color table translate, we only set the color */
        /*      interpretation other info copied by CopyBandInfo are */
        /*      not relevant in RGB expansion. */
        /* --------------------------------------------------------------------
         */
        if (psOptions->nRGBExpand == 1)
        {
            poVRTBand->SetColorInterpretation(GCI_GrayIndex);
        }
        else if (psOptions->nRGBExpand != 0 && i < psOptions->nRGBExpand)
        {
            poVRTBand->SetColorInterpretation(
                static_cast<GDALColorInterp>(GCI_RedBand + i));
        }

        /* --------------------------------------------------------------------
         */
        /*      copy over some other information of interest. */
        /* --------------------------------------------------------------------
         */
        else
        {
            CopyBandInfo(poSrcBand, poVRTBand,
                         !psOptions->bStats && !bFilterOutStatsMetadata,
                         !psOptions->bUnscale && !psOptions->bSetScale &&
                             !psOptions->bSetOffset,
                         !psOptions->bSetNoData && !psOptions->bUnsetNoData,
                         !psOptions->bNoRAT, psOptions);
            if (psOptions->asScaleParams.empty() &&
                psOptions->adfExponent.empty() &&
                EQUAL(psOptions->osFormat.c_str(), "GRIB"))
            {
                char **papszMD_GRIB = poSrcBand->GetMetadata("GRIB");
                if (papszMD_GRIB != nullptr)
                    poVRTBand->SetMetadata(papszMD_GRIB, "GRIB");
            }
        }

        // Color interpretation override
        if (!psOptions->anColorInterp.empty())
        {
            if (i < static_cast<int>(psOptions->anColorInterp.size()) &&
                psOptions->anColorInterp[i] >= 0)
            {
                poVRTBand->SetColorInterpretation(
                    static_cast<GDALColorInterp>(psOptions->anColorInterp[i]));
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Set a forcible nodata value? */
        /* --------------------------------------------------------------------
         */
        if (psOptions->bSetNoData)
        {
            if (poVRTBand->GetRasterDataType() == GDT_Int64)
            {
                if (psOptions->osNoData.find('.') != std::string::npos ||
                    CPLGetValueType(psOptions->osNoData.c_str()) ==
                        CPL_VALUE_STRING)
                {
                    const double dfNoData =
                        CPLAtof(psOptions->osNoData.c_str());
                    if (GDALIsValueExactAs<int64_t>(dfNoData))
                    {
                        poVRTBand->SetNoDataValueAsInt64(
                            static_cast<int64_t>(dfNoData));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot set nodata value %s on a Int64 band",
                                 psOptions->osNoData.c_str());
                    }
                }
                else
                {
                    errno = 0;
                    const auto val =
                        std::strtoll(psOptions->osNoData.c_str(), nullptr, 10);
                    if (errno == 0)
                    {
                        poVRTBand->SetNoDataValueAsInt64(
                            static_cast<int64_t>(val));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot set nodata value %s on a Int64 band",
                                 psOptions->osNoData.c_str());
                    }
                }
            }
            else if (poVRTBand->GetRasterDataType() == GDT_UInt64)
            {
                if (psOptions->osNoData.find('.') != std::string::npos ||
                    CPLGetValueType(psOptions->osNoData.c_str()) ==
                        CPL_VALUE_STRING)
                {
                    const double dfNoData =
                        CPLAtof(psOptions->osNoData.c_str());
                    if (GDALIsValueExactAs<uint64_t>(dfNoData))
                    {
                        poVRTBand->SetNoDataValueAsUInt64(
                            static_cast<uint64_t>(dfNoData));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot set nodata value %s on a UInt64 band",
                                 psOptions->osNoData.c_str());
                    }
                }
                else
                {
                    errno = 0;
                    const auto val =
                        std::strtoull(psOptions->osNoData.c_str(), nullptr, 10);
                    if (errno == 0)
                    {
                        poVRTBand->SetNoDataValueAsUInt64(
                            static_cast<uint64_t>(val));
                    }
                    else
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Cannot set nodata value %s on a UInt64 band",
                                 psOptions->osNoData.c_str());
                    }
                }
            }
            else
            {
                const double dfVal = AdjustNoDataValue(
                    CPLAtof(psOptions->osNoData.c_str()), poVRTBand, psOptions);
                poVRTBand->SetNoDataValue(dfVal);
            }
        }

        if (psOptions->bSetScale)
            poVRTBand->SetScale(psOptions->dfScale);

        if (psOptions->bSetOffset)
            poVRTBand->SetOffset(psOptions->dfOffset);

        if (psOptions->eMaskMode == MASK_AUTO &&
            (poSrcDS->GetRasterBand(1)->GetMaskFlags() & GMF_PER_DATASET) ==
                0 &&
            (poSrcBand->GetMaskFlags() & (GMF_ALL_VALID | GMF_NODATA)) == 0)
        {
            if (poVRTBand->CreateMaskBand(poSrcBand->GetMaskFlags()) == CE_None)
            {
                VRTSourcedRasterBand *hMaskVRTBand =
                    cpl::down_cast<VRTSourcedRasterBand *>(
                        poVRTBand->GetMaskBand());
                hMaskVRTBand->AddMaskBandSource(
                    poSrcBand, psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                    psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                    adfDstWin[0], adfDstWin[1], adfDstWin[2], adfDstWin[3]);
            }
        }
    }

    if (psOptions->eMaskMode == MASK_USER)
    {
        GDALRasterBand *poSrcBand =
            poSrcDS->GetRasterBand(std::abs(psOptions->nMaskBand));
        if (poSrcBand && poVDS->CreateMaskBand(GMF_PER_DATASET) == CE_None)
        {
            VRTSourcedRasterBand *hMaskVRTBand =
                static_cast<VRTSourcedRasterBand *>(GDALGetMaskBand(
                    GDALGetRasterBand(static_cast<GDALDataset *>(poVDS), 1)));
            if (psOptions->nMaskBand > 0)
                hMaskVRTBand->AddSimpleSource(
                    poSrcBand, psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                    psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                    adfDstWin[0], adfDstWin[1], adfDstWin[2], adfDstWin[3]);
            else
                hMaskVRTBand->AddMaskBandSource(
                    poSrcBand, psOptions->adfSrcWin[0], psOptions->adfSrcWin[1],
                    psOptions->adfSrcWin[2], psOptions->adfSrcWin[3],
                    adfDstWin[0], adfDstWin[1], adfDstWin[2], adfDstWin[3]);
        }
    }
    else if (psOptions->eMaskMode == MASK_AUTO && nSrcBandCount > 0 &&
             poSrcDS->GetRasterBand(1)->GetMaskFlags() == GMF_PER_DATASET)
    {
        if (poVDS->CreateMaskBand(GMF_PER_DATASET) == CE_None)
        {
            VRTSourcedRasterBand *hMaskVRTBand =
                static_cast<VRTSourcedRasterBand *>(GDALGetMaskBand(
                    GDALGetRasterBand(static_cast<GDALDataset *>(poVDS), 1)));
            hMaskVRTBand->AddMaskBandSource(
                poSrcDS->GetRasterBand(1), psOptions->adfSrcWin[0],
                psOptions->adfSrcWin[1], psOptions->adfSrcWin[2],
                psOptions->adfSrcWin[3], adfDstWin[0], adfDstWin[1],
                adfDstWin[2], adfDstWin[3]);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Compute stats if required.                                      */
    /* -------------------------------------------------------------------- */
    if (psOptions->bStats && EQUAL(psOptions->osFormat.c_str(), "COG"))
    {
        psOptions->aosCreateOptions.SetNameValue("STATISTICS", "YES");
    }
    else if (psOptions->bStats)
    {
        for (int i = 0; i < poVDS->GetRasterCount(); i++)
        {
            double dfMin, dfMax, dfMean, dfStdDev;
            poVDS->GetRasterBand(i + 1)->ComputeStatistics(
                psOptions->bApproxStats, &dfMin, &dfMax, &dfMean, &dfStdDev,
                GDALDummyProgress, nullptr);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Write to the output file using CopyCreate().                    */
    /* -------------------------------------------------------------------- */
    if (EQUAL(psOptions->osFormat.c_str(), "VRT") &&
        (psOptions->aosCreateOptions.empty() ||
         (psOptions->aosCreateOptions.size() == 2 &&
          psOptions->aosCreateOptions.FetchNameValue("BLOCKXSIZE") &&
          psOptions->aosCreateOptions.FetchNameValue("BLOCKYSIZE"))))
    {
        poVDS->SetDescription(pszDest);
        hOutDS = GDALDataset::ToHandle(poVDS);
        if (!EQUAL(pszDest, ""))
        {
            hOutDS = GDALTranslateFlush(hOutDS);
        }
    }
    else
    {
        hOutDS = GDALCreateCopy(
            hDriver, pszDest, GDALDataset::ToHandle(poVDS), psOptions->bStrict,
            psOptions->aosCreateOptions.List(), psOptions->pfnProgress,
            psOptions->pProgressData);
        hOutDS = GDALTranslateFlush(hOutDS);

        GDALClose(poVDS);
    }

    poSrcDS->Release();

    GDALTranslateOptionsFree(psOptions);
    return hOutDS;
}

/************************************************************************/
/*                           AttachMetadata()                           */
/************************************************************************/

static void AttachMetadata(GDALDatasetH hDS,
                           const CPLStringList &aosMetadataOptions)

{
    for (const auto &[pszKey, pszValue] :
         cpl::IterateNameValue(aosMetadataOptions))
    {
        GDALSetMetadataItem(hDS, pszKey, pszValue, nullptr);
    }
}

/************************************************************************/
/*                           AttachDomainMetadata()                     */
/************************************************************************/

static void AttachDomainMetadata(GDALDatasetH hDS,
                                 const CPLStringList &aosDomainMetadataOptions)

{
    for (const char *pszStr : aosDomainMetadataOptions)
    {

        char *pszKey = nullptr;
        char *pszDomain = nullptr;

        // parse the DOMAIN:KEY=value, Remainder is KEY=value
        const char *pszRemainder =
            CPLParseNameValueSep(pszStr, &pszDomain, ':');

        if (pszDomain && pszRemainder)
        {

            const char *pszValue =
                CPLParseNameValueSep(pszRemainder, &pszKey, '=');
            if (pszKey && pszValue)
            {
                GDALSetMetadataItem(hDS, pszKey, pszValue, pszDomain);
            }
        }
        CPLFree(pszKey);

        CPLFree(pszDomain);
    }
}

/************************************************************************/
/*                           CopyBandInfo()                            */
/************************************************************************/

/* A bit of a clone of VRTRasterBand::CopyCommonInfoFrom(), but we need */
/* more and more custom behavior in the context of gdal_translate ... */

static void CopyBandInfo(GDALRasterBand *poSrcBand, GDALRasterBand *poDstBand,
                         int bCanCopyStatsMetadata, int bCopyScale,
                         int bCopyNoData, bool bCopyRAT,
                         const GDALTranslateOptions *psOptions)

{

    if (bCanCopyStatsMetadata)
    {
        poDstBand->SetMetadata(poSrcBand->GetMetadata());
        if (bCopyRAT)
        {
            poDstBand->SetDefaultRAT(poSrcBand->GetDefaultRAT());
        }
    }
    else
    {
        char **papszMetadata = poSrcBand->GetMetadata();
        char **papszMetadataNew = nullptr;
        for (int i = 0; papszMetadata != nullptr && papszMetadata[i] != nullptr;
             i++)
        {
            if (!STARTS_WITH(papszMetadata[i], "STATISTICS_"))
                papszMetadataNew =
                    CSLAddString(papszMetadataNew, papszMetadata[i]);
        }
        poDstBand->SetMetadata(papszMetadataNew);
        CSLDestroy(papszMetadataNew);

        // we need to strip histogram data from the source RAT
        if (poSrcBand->GetDefaultRAT() && bCopyRAT)
        {
            GDALRasterAttributeTable *poNewRAT =
                poSrcBand->GetDefaultRAT()->Clone();

            // strip histogram data (as defined by the source RAT)
            poNewRAT->RemoveStatistics();
            if (poNewRAT->GetColumnCount())
            {
                poDstBand->SetDefaultRAT(poNewRAT);
            }
            // since SetDefaultRAT copies the RAT data we need to delete our
            // original
            delete poNewRAT;
        }
    }

    poDstBand->SetColorTable(poSrcBand->GetColorTable());
    poDstBand->SetColorInterpretation(poSrcBand->GetColorInterpretation());
    if (strlen(poSrcBand->GetDescription()) > 0)
        poDstBand->SetDescription(poSrcBand->GetDescription());

    if (bCopyNoData)
    {
        if (poSrcBand->GetRasterDataType() != GDT_Int64 &&
            poSrcBand->GetRasterDataType() != GDT_UInt64 &&
            poDstBand->GetRasterDataType() != GDT_Int64 &&
            poDstBand->GetRasterDataType() != GDT_UInt64)
        {
            int bSuccess = FALSE;
            double dfNoData = poSrcBand->GetNoDataValue(&bSuccess);
            if (bSuccess)
            {
                const double dfVal =
                    AdjustNoDataValue(dfNoData, poDstBand, psOptions);
                poDstBand->SetNoDataValue(dfVal);
            }
        }
        else
        {
            GDALCopyNoDataValue(poDstBand, poSrcBand);
        }
    }

    if (bCopyScale)
    {
        poDstBand->SetOffset(poSrcBand->GetOffset());
        poDstBand->SetScale(poSrcBand->GetScale());
    }

    poDstBand->SetCategoryNames(poSrcBand->GetCategoryNames());

    // Copy unit only if the range of pixel values is not modified
    if (bCanCopyStatsMetadata && bCopyScale &&
        !EQUAL(poSrcBand->GetUnitType(), ""))
        poDstBand->SetUnitType(poSrcBand->GetUnitType());
}

/************************************************************************/
/*                             GetColorInterp()                         */
/************************************************************************/

static int GetColorInterp(const char *pszStr)
{
    if (EQUAL(pszStr, "red"))
        return GCI_RedBand;
    if (EQUAL(pszStr, "green"))
        return GCI_GreenBand;
    if (EQUAL(pszStr, "blue"))
        return GCI_BlueBand;
    if (EQUAL(pszStr, "alpha"))
        return GCI_AlphaBand;
    if (EQUAL(pszStr, "gray") || EQUAL(pszStr, "grey"))
        return GCI_GrayIndex;
    if (EQUAL(pszStr, "undefined"))
        return GCI_Undefined;
    CPLError(CE_Warning, CPLE_NotSupported,
             "Unsupported color interpretation: %s", pszStr);
    return -1;
}

/************************************************************************/
/*                     GDALTranslateOptionsGetParser()                  */
/************************************************************************/

static std::unique_ptr<GDALArgumentParser>
GDALTranslateOptionsGetParser(GDALTranslateOptions *psOptions,
                              GDALTranslateOptionsForBinary *psOptionsForBinary)
{
    auto argParser = std::make_unique<GDALArgumentParser>(
        "gdal_translate", /* bForBinary=*/psOptionsForBinary != nullptr);

    argParser->add_description(
        _("Convert raster data between different formats, with potential "
          "subsetting, resampling, and rescaling pixels in the process."));

    argParser->add_epilog(_("For more details, consult "
                            "https://gdal.org/programs/gdal_translate.html"));

    argParser->add_output_type_argument(psOptions->eOutputType);

    argParser->add_argument("-if")
        .append()
        .metavar("<format>")
        .action(
            [psOptionsForBinary](const std::string &s)
            {
                if (psOptionsForBinary)
                {
                    if (GDALGetDriverByName(s.c_str()) == nullptr)
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "%s is not a recognized driver", s.c_str());
                    }
                    psOptionsForBinary->aosAllowedInputDrivers.AddString(
                        s.c_str());
                }
            })
        .help(_("Format/driver name(s) to try when opening the input file."));

    argParser->add_output_format_argument(psOptions->osFormat);

    // Written that way so that in library mode, users can still use the -q
    // switch, even if it has no effect
    argParser->add_quiet_argument(
        psOptionsForBinary ? &(psOptionsForBinary->bQuiet) : nullptr);

    argParser->add_argument("-b")
        .append()
        .metavar("<band>")
        .action(
            [psOptions](const std::string &s)
            {
                const char *pszBand = s.c_str();
                bool bMask = false;
                if (EQUAL(pszBand, "mask"))
                    pszBand = "mask,1";
                if (STARTS_WITH_CI(pszBand, "mask,"))
                {
                    bMask = true;
                    pszBand += 5;
                    /* If we use the source mask band as a regular band */
                    /* don't create a target mask band by default */
                    if (!psOptions->bParsedMaskArgument)
                        psOptions->eMaskMode = MASK_DISABLED;
                }
                const int nBand = atoi(pszBand);
                if (nBand < 1)
                {
                    throw std::invalid_argument(CPLSPrintf(
                        "Unrecognizable band number (%s).", s.c_str()));
                }

                psOptions->nBandCount++;
                psOptions->anBandList.emplace_back(nBand * (bMask ? -1 : 1));
            })
        .help(_("Select input band(s)"));

    argParser->add_argument("-mask")
        .metavar("<mask>")
        .action(
            [psOptions](const std::string &s)
            {
                psOptions->bParsedMaskArgument = true;
                const char *pszBand = s.c_str();
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
                    if (nBand < 1)
                    {
                        throw std::invalid_argument(CPLSPrintf(
                            "Unrecognizable band number (%s).", s.c_str()));
                    }

                    psOptions->eMaskMode = MASK_USER;
                    psOptions->nMaskBand = nBand;
                    if (bMask)
                        psOptions->nMaskBand *= -1;
                }
            })
        .help(_("Select an input band to create output dataset mask band"));

    argParser->add_argument("-expand")
        .metavar("gray|rgb|rgba")
        .action(
            [psOptions](const std::string &s)
            {
                if (EQUAL(s.c_str(), "gray"))
                    psOptions->nRGBExpand = 1;
                else if (EQUAL(s.c_str(), "rgb"))
                    psOptions->nRGBExpand = 3;
                else if (EQUAL(s.c_str(), "rgba"))
                    psOptions->nRGBExpand = 4;
                else
                {
                    throw std::invalid_argument(CPLSPrintf(
                        "Value %s unsupported. Only gray, rgb or rgba are "
                        "supported.",
                        s.c_str()));
                }
            })
        .help(_("To expose a dataset with 1 band with a color table as a "
                "dataset with 3 (RGB) or 4 (RGBA) bands."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-strict")
            .store_into(psOptions->bStrict)
            .help(_("Enable strict mode"));

        group.add_argument("-not_strict")
            .flag()
            .action([psOptions](const std::string &)
                    { psOptions->bStrict = false; })
            .help(_("Disable strict mode"));
    }

    argParser->add_argument("-outsize")
        .metavar("<xsize[%]|0> <ysize[%]|0>")
        .nargs(2)
        .help(_("Set the size of the output file."));

    argParser->add_argument("-tr")
        .metavar("<xres> <yes>")
        .nargs(2)
        .scan<'g', double>()
        .help(_("Set target resolution."));

    argParser->add_argument("-ovr")
        .metavar("<level>|AUTO|AUTO-<n>|NONE")
        .action(
            [psOptions](const std::string &s)
            {
                const char *pszOvLevel = s.c_str();
                if (EQUAL(pszOvLevel, "AUTO"))
                    psOptions->nOvLevel = OVR_LEVEL_AUTO;
                else if (STARTS_WITH_CI(pszOvLevel, "AUTO-"))
                    psOptions->nOvLevel =
                        OVR_LEVEL_AUTO - atoi(pszOvLevel + strlen("AUTO-"));
                else if (EQUAL(pszOvLevel, "NONE"))
                    psOptions->nOvLevel = OVR_LEVEL_NONE;
                else if (CPLGetValueType(pszOvLevel) == CPL_VALUE_INTEGER)
                    psOptions->nOvLevel = atoi(pszOvLevel);
                else
                {
                    throw std::invalid_argument(CPLSPrintf(
                        "Invalid value '%s' for -ovr option", pszOvLevel));
                }
            })
        .help(_("Specify which overview level of source file must be used"));

    if (psOptionsForBinary)
    {
        argParser->add_argument("-sds")
            .store_into(psOptionsForBinary->bCopySubDatasets)
            .help(_("Copy subdatasets"));
    }

    argParser->add_argument("-r")
        .metavar("nearest,bilinear,cubic,cubicspline,lanczos,average,mode")
        .store_into(psOptions->osResampling)
        .help(_("Resampling algorithm."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-scale")
            .metavar("[<src_min> <src_max> [<dst_min> <dst_max>]]")
            //.nargs(0, 4)
            .append()
            .scan<'g', double>()
            .help(_("Rescale the input pixels values from the range src_min to "
                    "src_max to the range dst_min to dst_max."));

        group.add_argument("-scale_X")
            .metavar("[<src_min> <src_max> [<dst_min> <dst_max>]]")
            //.nargs(0, 4)
            .append()
            .scan<'g', double>()
            .help(_("Rescale the input pixels values for band X."));

        group.add_argument("-unscale")
            .store_into(psOptions->bUnscale)
            .help(_("Apply the scale/offset metadata for the bands to convert "
                    "scaled values to unscaled values."));
    }

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-exponent")
            .metavar("<value>")
            .scan<'g', double>()
            .help(_(
                "Exponent to apply non-linear scaling with a power function"));

        group.add_argument("-exponent_X")
            .append()
            .metavar("<value>")
            .scan<'g', double>()
            .help(
                _("Exponent to apply non-linear scaling with a power function, "
                  "for band X"));
    }

    argParser->add_argument("-srcwin")
        .metavar("<xoff> <yoff> <xsize> <ysize>")
        .nargs(4)
        .scan<'g', double>()
        .help(_("Selects a subwindow from the source image based on pixel/line "
                "location."));

    argParser->add_argument("-projwin")
        .metavar("<ulx> <uly> <lrx> <lry>")
        .nargs(4)
        .scan<'g', double>()
        .help(_("Selects a subwindow from the source image based on "
                "georeferenced coordinates."));

    argParser->add_argument("-projwin_srs")
        .metavar("<srs_def>")
        .store_into(psOptions->osProjSRS)
        .help(_("Specifies the SRS in which to interpret the coordinates given "
                "with -projwin."));

    argParser->add_argument("-epo")
        .flag()
        .action(
            [psOptions](const std::string &)
            {
                psOptions->bErrorOnPartiallyOutside = true;
                psOptions->bErrorOnCompletelyOutside = true;
            })
        .help(_("Error when Partially Outside."));

    argParser->add_argument("-eco")
        .store_into(psOptions->bErrorOnCompletelyOutside)
        .help(_("Error when Completely Outside."));

    argParser->add_argument("-a_srs")
        .metavar("<srs_def>")
        .store_into(psOptions->osOutputSRS)
        .help(_("Override the projection for the output file."));

    argParser->add_argument("-a_coord_epoch")
        .metavar("<epoch>")
        .store_into(psOptions->dfOutputCoordinateEpoch)
        .help(_("Assign a coordinate epoch."));

    argParser->add_argument("-a_ullr")
        .metavar("<ulx> <uly> <lrx> <lry>")
        .nargs(4)
        .scan<'g', double>()
        .help(
            _("Assign/override the georeferenced bounds of the output file."));

    argParser->add_argument("-a_nodata")
        .metavar("<value>|none")
        .help(_("Assign a specified nodata value to output bands."));

    argParser->add_argument("-a_gt")
        .metavar("<gt(0)> <gt(1)> <gt(2)> <gt(3)> <gt(4)> <gt(5)>")
        .nargs(6)
        .scan<'g', double>()
        .help(_("Assign/override the geotransform of the output file."));

    argParser->add_argument("-a_scale")
        .metavar("<value>")
        .store_into(psOptions->dfScale)
        .help(_("Set band scaling value."));

    argParser->add_argument("-a_offset")
        .metavar("<value>")
        .store_into(psOptions->dfOffset)
        .help(_("Set band offset value."));

    argParser->add_argument("-nogcp")
        .store_into(psOptions->bNoGCP)
        .help(_("Do not copy the GCPs in the source dataset to the output "
                "dataset."));

    argParser->add_argument("-gcp")
        .metavar("<pixel> <line> <easting> <northing> [<elevation>]")
        .nargs(4, 5)
        .append()
        .scan<'g', double>()
        .help(
            _("Add the indicated ground control point to the output dataset."));

    argParser->add_argument("-colorinterp")
        .metavar("{red|green|blue|alpha|gray|undefined},...")
        .action(
            [psOptions](const std::string &s)
            {
                CPLStringList aosList(CSLTokenizeString2(s.c_str(), ",", 0));
                psOptions->anColorInterp.resize(aosList.size());
                for (int j = 0; j < aosList.size(); j++)
                {
                    psOptions->anColorInterp[j] = GetColorInterp(aosList[j]);
                }
            })
        .help(_("Override the color interpretation of all specified bands."));

    argParser->add_argument("-colorinterp_X")
        .append()
        .metavar("{red|green|blue|alpha|gray|undefined}")
        .help(_("Override the color interpretation of band X."));

    {
        auto &group = argParser->add_mutually_exclusive_group();
        group.add_argument("-stats")
            .flag()
            .action(
                [psOptions](const std::string &)
                {
                    psOptions->bStats = true;
                    psOptions->bApproxStats = false;
                })
            .help(_("Force (re)computation of statistics."));

        group.add_argument("-approx_stats")
            .flag()
            .action(
                [psOptions](const std::string &)
                {
                    psOptions->bStats = true;
                    psOptions->bApproxStats = true;
                })
            .help(_("Force (re)computation of approximate statistics."));
    }

    argParser->add_argument("-norat")
        .store_into(psOptions->bNoRAT)
        .help(_("Do not copy source RAT into destination dataset."));

    argParser->add_argument("-noxmp")
        .store_into(psOptions->bNoXMP)
        .help(_("Do not copy the XMP metadata into destination dataset."));

    argParser->add_creation_options_argument(psOptions->aosCreateOptions);

    argParser->add_metadata_item_options_argument(
        psOptions->aosMetadataOptions);

    argParser->add_argument("-dmo")
        .metavar("<DOMAIN>:<KEY>=<VALUE>")
        .append()
        .action([psOptions](const std::string &s)
                { psOptions->aosDomainMetadataOptions.AddString(s.c_str()); })
        .help(_("Passes a metadata key and value in specified domain to set on "
                "the output dataset if possible."));

    argParser->add_open_options_argument(
        psOptionsForBinary ? &(psOptionsForBinary->aosOpenOptions) : nullptr);

    // Undocumented option used by gdal_translate_fuzzer
    argParser->add_argument("-limit_outsize")
        .hidden()
        .store_into(psOptions->nLimitOutSize);

    if (psOptionsForBinary)
    {
        argParser->add_argument("input_file")
            .metavar("<input_file>")
            .store_into(psOptionsForBinary->osSource)
            .help(_("Input file."));

        argParser->add_argument("output_file")
            .metavar("<output_file>")
            .store_into(psOptionsForBinary->osDest)
            .help(_("Output file."));
    }

    return argParser;
}

/************************************************************************/
/*                      GDALTranslateGetParserUsage()                   */
/************************************************************************/

std::string GDALTranslateGetParserUsage()
{
    try
    {
        GDALTranslateOptions sOptions;
        GDALTranslateOptionsForBinary sOptionsForBinary;
        auto argParser =
            GDALTranslateOptionsGetParser(&sOptions, &sOptionsForBinary);
        return argParser->usage();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Unexpected exception: %s",
                 err.what());
        return std::string();
    }
}

/************************************************************************/
/*                             GDALTranslateOptionsNew()                */
/************************************************************************/

/**
 * Allocates a GDALTranslateOptions struct.
 *
 * @param papszArgv NULL terminated list of options (potentially including
 * filename and open options too), or NULL. The accepted options are the ones of
 * the <a href="/programs/gdal_translate.html">gdal_translate</a> utility.
 * @param psOptionsForBinary (output) may be NULL (and should generally be
 * NULL), otherwise (gdal_translate_bin.cpp use case) must be allocated with
 *                           GDALTranslateOptionsForBinaryNew() prior to this
 * function. Will be filled with potentially present filename, open options,...
 * @return pointer to the allocated GDALTranslateOptions struct. Must be freed
 * with GDALTranslateOptionsFree().
 *
 * @since GDAL 2.1
 */

GDALTranslateOptions *
GDALTranslateOptionsNew(char **papszArgv,
                        GDALTranslateOptionsForBinary *psOptionsForBinary)
{
    auto psOptions = std::make_unique<GDALTranslateOptions>();

    /* -------------------------------------------------------------------- */
    /*      Pre-processing for custom syntax that ArgumentParser does not   */
    /*      support.                                                        */
    /* -------------------------------------------------------------------- */

    CPLStringList aosArgv;
    const int argc = CSLCount(papszArgv);
    for (int i = 0; i < argc && papszArgv != nullptr && papszArgv[i] != nullptr;
         i++)
    {
        if (i + 4 < argc && EQUAL(papszArgv[i], "-gcp"))
        {
            /* -gcp pixel line easting northing [elev] */
            psOptions->nGCPCount++;
            psOptions->pasGCPs = static_cast<GDAL_GCP *>(CPLRealloc(
                psOptions->pasGCPs, sizeof(GDAL_GCP) * psOptions->nGCPCount));
            GDALInitGCPs(1, psOptions->pasGCPs + psOptions->nGCPCount - 1);

            psOptions->pasGCPs[psOptions->nGCPCount - 1].dfGCPPixel =
                CPLAtofM(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount - 1].dfGCPLine =
                CPLAtofM(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount - 1].dfGCPX =
                CPLAtofM(papszArgv[++i]);
            psOptions->pasGCPs[psOptions->nGCPCount - 1].dfGCPY =
                CPLAtofM(papszArgv[++i]);

            char *endptr = nullptr;
            if (papszArgv[i + 1] != nullptr &&
                (CPLStrtod(papszArgv[i + 1], &endptr) != 0.0 ||
                 papszArgv[i + 1][0] == '0'))
            {
                /* Check that last argument is really a number and not a
                 * filename */
                /* looking like a number (see ticket #863) */
                if (endptr && *endptr == 0)
                    psOptions->pasGCPs[psOptions->nGCPCount - 1].dfGCPZ =
                        CPLAtofM(papszArgv[++i]);
            }

            /* should set id and info? */
        }

        else if (EQUAL(papszArgv[i], "-scale") ||
                 STARTS_WITH_CI(papszArgv[i], "-scale_"))
        {
            int nIndex = 0;
            if (STARTS_WITH_CI(papszArgv[i], "-scale_"))
            {
                if (!psOptions->bHasUsedExplicitScaleBand &&
                    !psOptions->asScaleParams.empty())
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Cannot mix -scale and -scale_XX syntax");
                    return nullptr;
                }
                psOptions->bHasUsedExplicitScaleBand = true;
                nIndex = atoi(papszArgv[i] + 7);
                if (nIndex <= 0 || nIndex > 65535)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Invalid parameter name: %s", papszArgv[i]);
                    return nullptr;
                }
                nIndex--;
            }
            else
            {
                if (psOptions->bHasUsedExplicitScaleBand)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Cannot mix -scale and -scale_XX syntax");
                    return nullptr;
                }
                nIndex = static_cast<int>(psOptions->asScaleParams.size());
            }

            if (nIndex >= static_cast<int>(psOptions->asScaleParams.size()))
            {
                psOptions->asScaleParams.resize(nIndex + 1);
            }
            psOptions->asScaleParams[nIndex].bScale = true;
            psOptions->asScaleParams[nIndex].bHaveScaleSrc = false;
            if (i < argc - 2 && ArgIsNumeric(papszArgv[i + 1]))
            {
                psOptions->asScaleParams[nIndex].bHaveScaleSrc = true;
                psOptions->asScaleParams[nIndex].dfScaleSrcMin =
                    CPLAtofM(papszArgv[i + 1]);
                psOptions->asScaleParams[nIndex].dfScaleSrcMax =
                    CPLAtofM(papszArgv[i + 2]);
                i += 2;
            }
            if (i < argc - 2 &&
                psOptions->asScaleParams[nIndex].bHaveScaleSrc &&
                ArgIsNumeric(papszArgv[i + 1]))
            {
                psOptions->asScaleParams[nIndex].dfScaleDstMin =
                    CPLAtofM(papszArgv[i + 1]);
                psOptions->asScaleParams[nIndex].dfScaleDstMax =
                    CPLAtofM(papszArgv[i + 2]);
                i += 2;
            }
            else
            {
                psOptions->asScaleParams[nIndex].dfScaleDstMin = 0.0;
                psOptions->asScaleParams[nIndex].dfScaleDstMax = 255.0;
            }
        }

        else if ((EQUAL(papszArgv[i], "-exponent") ||
                  STARTS_WITH_CI(papszArgv[i], "-exponent_")) &&
                 papszArgv[i + 1])
        {
            int nIndex = 0;
            if (STARTS_WITH_CI(papszArgv[i], "-exponent_"))
            {
                if (!psOptions->bHasUsedExplicitExponentBand &&
                    !psOptions->adfExponent.empty())
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Cannot mix -exponent and -exponent_XX syntax");
                    return nullptr;
                }
                psOptions->bHasUsedExplicitExponentBand = true;
                nIndex = atoi(papszArgv[i] + 10);
                if (nIndex <= 0 || nIndex > 65535)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Invalid parameter name: %s", papszArgv[i]);
                    return nullptr;
                }
                nIndex--;
            }
            else
            {
                if (psOptions->bHasUsedExplicitExponentBand)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Cannot mix -exponent and -exponent_XX syntax");
                    return nullptr;
                }
                nIndex = static_cast<int>(psOptions->adfExponent.size());
            }

            if (nIndex >= static_cast<int>(psOptions->adfExponent.size()))
            {
                psOptions->adfExponent.resize(nIndex + 1);
            }
            double dfExponent = CPLAtofM(papszArgv[++i]);
            psOptions->adfExponent[nIndex] = dfExponent;
        }

        else if (STARTS_WITH_CI(papszArgv[i], "-colorinterp_") &&
                 papszArgv[i + 1])
        {
            int nIndex = atoi(papszArgv[i] + strlen("-colorinterp_"));
            if (nIndex <= 0 || nIndex > 65535)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Invalid parameter name: %s", papszArgv[i]);
                return nullptr;
            }
            nIndex--;

            if (nIndex >= static_cast<int>(psOptions->anColorInterp.size()))
            {
                psOptions->anColorInterp.resize(nIndex + 1, -1);
            }
            ++i;
            psOptions->anColorInterp[nIndex] = GetColorInterp(papszArgv[i]);
        }

        // argparser will be confused if the value of a string argument
        // starts with a negative sign.
        else if (EQUAL(papszArgv[i], "-a_nodata") && papszArgv[i + 1])
        {
            ++i;
            const std::string s = papszArgv[i];
            if (EQUAL(s.c_str(), "none"))
            {
                psOptions->bUnsetNoData = true;
            }
            else
            {
                psOptions->bSetNoData = true;
                psOptions->osNoData = s;
            }
        }

        else
        {
            aosArgv.AddString(papszArgv[i]);
        }
    }

    try
    {

        auto argParser =
            GDALTranslateOptionsGetParser(psOptions.get(), psOptionsForBinary);

        argParser->parse_args_without_binary_name(aosArgv.List());

        psOptions->bSetScale = argParser->is_used("-a_scale");
        psOptions->bSetOffset = argParser->is_used("-a_offset");

        if (auto adfULLR = argParser->present<std::vector<double>>("-a_ullr"))
        {
            CPLAssert(psOptions->adfULLR.size() == adfULLR->size());
            for (size_t i = 0; i < adfULLR->size(); ++i)
                psOptions->adfULLR[i] = (*adfULLR)[i];
        }

        if (auto adfGT = argParser->present<std::vector<double>>("-a_gt"))
        {
            CPLAssert(psOptions->adfGT.size() == adfGT->size());
            for (size_t i = 0; i < adfGT->size(); ++i)
                psOptions->adfGT[i] = (*adfGT)[i];
        }

        bool bOutsizeExplicitlySet = false;
        if (auto aosOutSize =
                argParser->present<std::vector<std::string>>("-outsize"))
        {
            if ((*aosOutSize)[0].back() == '%')
                psOptions->dfOXSizePct = CPLAtofM((*aosOutSize)[0].c_str());
            else
                psOptions->nOXSizePixel = atoi((*aosOutSize)[0].c_str());

            if ((*aosOutSize)[1].back() == '%')
                psOptions->dfOYSizePct = CPLAtofM((*aosOutSize)[1].c_str());
            else
                psOptions->nOYSizePixel = atoi((*aosOutSize)[1].c_str());
            bOutsizeExplicitlySet = true;
        }

        if (auto adfTargetRes = argParser->present<std::vector<double>>("-tr"))
        {
            psOptions->dfXRes = (*adfTargetRes)[0];
            psOptions->dfYRes = fabs((*adfTargetRes)[1]);
            if (psOptions->dfXRes == 0 || psOptions->dfYRes == 0)
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "Wrong value for -tr parameters.");
                return nullptr;
            }
        }

        if (auto adfSrcWin = argParser->present<std::vector<double>>("-srcwin"))
        {
            for (int i = 0; i < 4; ++i)
                psOptions->adfSrcWin[i] = (*adfSrcWin)[i];
        }

        if (auto adfProjWin =
                argParser->present<std::vector<double>>("-projwin"))
        {
            psOptions->dfULX = (*adfProjWin)[0];
            psOptions->dfULY = (*adfProjWin)[1];
            psOptions->dfLRX = (*adfProjWin)[2];
            psOptions->dfLRY = (*adfProjWin)[3];
        }

        if (psOptions->nGCPCount > 0 && psOptions->bNoGCP)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "-nogcp and -gcp cannot be used as the same time");
            return nullptr;
        }

        if (bOutsizeExplicitlySet && psOptions->nOXSizePixel == 0 &&
            psOptions->dfOXSizePct == 0.0 && psOptions->nOYSizePixel == 0 &&
            psOptions->dfOYSizePct == 0.0)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "-outsize %d %d invalid.",
                     psOptions->nOXSizePixel, psOptions->nOYSizePixel);
            return nullptr;
        }

        if (!psOptions->asScaleParams.empty() && psOptions->bUnscale)
        {
            CPLError(CE_Failure, CPLE_IllegalArg,
                     "-scale and -unscale cannot be used as the same time");
            return nullptr;
        }

        if (psOptionsForBinary)
        {
            if (!psOptions->osFormat.empty())
                psOptionsForBinary->osFormat = psOptions->osFormat;
        }

        return psOptions.release();
    }
    catch (const std::exception &err)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", err.what());
        return nullptr;
    }
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
    delete psOptions;
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

void GDALTranslateOptionsSetProgress(GDALTranslateOptions *psOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    psOptions->pfnProgress = pfnProgress;
    psOptions->pProgressData = pProgressData;
    if (pfnProgress == GDALTermProgress)
        psOptions->bQuiet = false;
}
