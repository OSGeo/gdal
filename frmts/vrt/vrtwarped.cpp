/******************************************************************************
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTWarpedRasterBand *and VRTWarpedDataset.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "vrtdataset.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <map>

// Suppress deprecation warning for GDALOpenVerticalShiftGrid and
// GDALApplyVerticalShiftGrid
#ifndef CPL_WARN_DEPRECATED_GDALOpenVerticalShiftGrid
#define CPL_WARN_DEPRECATED_GDALOpenVerticalShiftGrid(x)
#define CPL_WARN_DEPRECATED_GDALApplyVerticalShiftGrid(x)
#endif

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_alg_priv.h"
#include "gdal_priv.h"
#include "gdalwarper.h"
#include "ogr_geometry.h"

/************************************************************************/
/*                      GDALAutoCreateWarpedVRT()                       */
/************************************************************************/

/**
 * Create virtual warped dataset automatically.
 *
 * This function will create a warped virtual file representing the
 * input image warped into the target coordinate system.  A GenImgProj
 * transformation is created to accomplish any required GCP/Geotransform
 * warp and reprojection to the target coordinate system.  The output virtual
 * dataset will be "northup" in the target coordinate system.   The
 * GDALSuggestedWarpOutput() function is used to determine the bounds and
 * resolution of the output virtual file which should be large enough to
 * include all the input image
 *
 * If you want to create an alpha band if the source dataset has none, set
 * psOptionsIn->nDstAlphaBand = GDALGetRasterCount(hSrcDS) + 1.
 *
 * Note that the constructed GDALDatasetH will acquire one or more references
 * to the passed in hSrcDS.  Reference counting semantics on the source
 * dataset should be honoured.  That is, don't just GDALClose() it unless it
 * was opened with GDALOpenShared().
 *
 * It is possible to "transfer" the ownership of the source dataset
 * to the warped dataset in the following way:
 *
 * \code{.c}
 *      GDALDatasetH src_ds = GDALOpen("source.tif");
 *      GDALDatasetH warped_ds = GDALAutoCreateWarpedVRT( src_ds, ... );
 *      GDALReleaseDataset(src_ds); // src_ds is not "owned" fully by warped_ds.
 * Do NOT use GDALClose(src_ds) here
 *      ...
 *      ...
 *      GDALReleaseDataset(warped_ds); // or GDALClose(warped_ds);
 * \endcode
 *
 * Traditonal nested calls are also possible of course:
 *
 * \code{.c}
 *      GDALDatasetH src_ds = GDALOpen("source.tif");
 *      GDALDatasetH warped_ds = GDALAutoCreateWarpedVRT( src_ds, ... );
 *      ...
 *      ...
 *      GDALReleaseDataset(warped_ds); // or GDALClose(warped_ds);
 *      GDALReleaseDataset(src_ds); // or GDALClose(src_ds);
 * \endcode
 *
 * The returned dataset will have no associated filename for itself.  If you
 * want to write the virtual dataset description to a file, use the
 * GDALSetDescription() function (or SetDescription() method) on the dataset
 * to assign a filename before it is closed.
 *
 * @param hSrcDS The source dataset.
 *
 * @param pszSrcWKT The coordinate system of the source image.  If NULL, it
 * will be read from the source image.
 *
 * @param pszDstWKT The coordinate system to convert to.  If NULL no change
 * of coordinate system will take place.
 *
 * @param eResampleAlg One of GRA_NearestNeighbour, GRA_Bilinear, GRA_Cubic,
 * GRA_CubicSpline, GRA_Lanczos, GRA_Average, GRA_RMS or GRA_Mode.
 * Controls the sampling method used.
 *
 * @param dfMaxError Maximum error measured in input pixels that is allowed in
 * approximating the transformation (0.0 for exact calculations).
 *
 * @param psOptionsIn Additional warp options, normally NULL.
 *
 * @return NULL on failure, or a new virtual dataset handle on success.
 */

GDALDatasetH CPL_STDCALL
GDALAutoCreateWarpedVRT(GDALDatasetH hSrcDS, const char *pszSrcWKT,
                        const char *pszDstWKT, GDALResampleAlg eResampleAlg,
                        double dfMaxError, const GDALWarpOptions *psOptionsIn)
{
    return GDALAutoCreateWarpedVRTEx(hSrcDS, pszSrcWKT, pszDstWKT, eResampleAlg,
                                     dfMaxError, psOptionsIn, nullptr);
}

/************************************************************************/
/*                     GDALAutoCreateWarpedVRTEx()                      */
/************************************************************************/

/**
 * Create virtual warped dataset automatically.
 *
 * Compared to GDALAutoCreateWarpedVRT() this function adds one extra
 * argument: options to be passed to GDALCreateGenImgProjTransformer2().
 *
 * @since 3.2
 */

GDALDatasetH CPL_STDCALL GDALAutoCreateWarpedVRTEx(
    GDALDatasetH hSrcDS, const char *pszSrcWKT, const char *pszDstWKT,
    GDALResampleAlg eResampleAlg, double dfMaxError,
    const GDALWarpOptions *psOptionsIn, CSLConstList papszTransformerOptions)
{
    VALIDATE_POINTER1(hSrcDS, "GDALAutoCreateWarpedVRT", nullptr);

    /* -------------------------------------------------------------------- */
    /*      Populate the warp options.                                      */
    /* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = nullptr;
    if (psOptionsIn != nullptr)
        psWO = GDALCloneWarpOptions(psOptionsIn);
    else
        psWO = GDALCreateWarpOptions();

    psWO->eResampleAlg = eResampleAlg;

    psWO->hSrcDS = hSrcDS;

    GDALWarpInitDefaultBandMapping(psWO, GDALGetRasterCount(hSrcDS));

    /* -------------------------------------------------------------------- */
    /*      Setup no data values (if not done in psOptionsIn)               */
    /* -------------------------------------------------------------------- */
    if (psWO->padfSrcNoDataReal == nullptr &&
        psWO->padfDstNoDataReal == nullptr && psWO->nSrcAlphaBand == 0)
    {
        // If none of the provided input nodata values can be represented in the
        // data type of the corresponding source band, ignore them.
        int nCountInvalidSrcNoDataReal = 0;
        for (int i = 0; i < psWO->nBandCount; i++)
        {
            GDALRasterBandH rasterBand =
                GDALGetRasterBand(psWO->hSrcDS, psWO->panSrcBands[i]);

            int hasNoDataValue;
            double noDataValue =
                GDALGetRasterNoDataValue(rasterBand, &hasNoDataValue);

            if (hasNoDataValue &&
                !GDALIsValueExactAs(noDataValue,
                                    GDALGetRasterDataType(rasterBand)))
            {
                nCountInvalidSrcNoDataReal++;
            }
        }

        if (nCountInvalidSrcNoDataReal != psWO->nBandCount)
        {
            for (int i = 0; i < psWO->nBandCount; i++)
            {
                GDALRasterBandH rasterBand =
                    GDALGetRasterBand(psWO->hSrcDS, psWO->panSrcBands[i]);

                int hasNoDataValue;
                double noDataValue =
                    GDALGetRasterNoDataValue(rasterBand, &hasNoDataValue);

                if (hasNoDataValue)
                {
                    // Check if the nodata value is out of range
                    int bClamped = FALSE;
                    int bRounded = FALSE;
                    CPL_IGNORE_RET_VAL(GDALAdjustValueToDataType(
                        GDALGetRasterDataType(rasterBand), noDataValue,
                        &bClamped, &bRounded));
                    if (!bClamped)
                    {
                        GDALWarpInitNoDataReal(psWO, -1e10);
                        if (psWO->padfSrcNoDataReal != nullptr &&
                            psWO->padfDstNoDataReal != nullptr)
                        {
                            psWO->padfSrcNoDataReal[i] = noDataValue;
                            psWO->padfDstNoDataReal[i] = noDataValue;
                        }
                    }
                }
            }
        }

        if (psWO->padfDstNoDataReal != nullptr)
        {
            if (CSLFetchNameValue(psWO->papszWarpOptions, "INIT_DEST") ==
                nullptr)
            {
                psWO->papszWarpOptions = CSLSetNameValue(
                    psWO->papszWarpOptions, "INIT_DEST", "NO_DATA");
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create the transformer.                                         */
    /* -------------------------------------------------------------------- */
    psWO->pfnTransformer = GDALGenImgProjTransform;

    char **papszOptions = nullptr;
    if (pszSrcWKT != nullptr)
        papszOptions = CSLSetNameValue(papszOptions, "SRC_SRS", pszSrcWKT);
    if (pszDstWKT != nullptr)
        papszOptions = CSLSetNameValue(papszOptions, "DST_SRS", pszDstWKT);
    papszOptions = CSLMerge(papszOptions, papszTransformerOptions);
    psWO->pTransformerArg =
        GDALCreateGenImgProjTransformer2(psWO->hSrcDS, nullptr, papszOptions);
    CSLDestroy(papszOptions);

    if (psWO->pTransformerArg == nullptr)
    {
        GDALDestroyWarpOptions(psWO);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Figure out the desired output bounds and resolution.            */
    /* -------------------------------------------------------------------- */
    double adfDstGeoTransform[6] = {0.0};
    int nDstPixels = 0;
    int nDstLines = 0;
    CPLErr eErr = GDALSuggestedWarpOutput(
        hSrcDS, psWO->pfnTransformer, psWO->pTransformerArg, adfDstGeoTransform,
        &nDstPixels, &nDstLines);
    if (eErr != CE_None)
    {
        GDALDestroyTransformer(psWO->pTransformerArg);
        GDALDestroyWarpOptions(psWO);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Update the transformer to include an output geotransform        */
    /*      back to pixel/line coordinates.                                 */
    /*                                                                      */
    /* -------------------------------------------------------------------- */
    GDALSetGenImgProjTransformerDstGeoTransform(psWO->pTransformerArg,
                                                adfDstGeoTransform);

    /* -------------------------------------------------------------------- */
    /*      Do we want to apply an approximating transformation?            */
    /* -------------------------------------------------------------------- */
    if (dfMaxError > 0.0)
    {
        psWO->pTransformerArg = GDALCreateApproxTransformer(
            psWO->pfnTransformer, psWO->pTransformerArg, dfMaxError);
        psWO->pfnTransformer = GDALApproxTransform;
        GDALApproxTransformerOwnsSubtransformer(psWO->pTransformerArg, TRUE);
    }

    /* -------------------------------------------------------------------- */
    /*      Create the VRT file.                                            */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDstDS = GDALCreateWarpedVRT(hSrcDS, nDstPixels, nDstLines,
                                              adfDstGeoTransform, psWO);

    GDALDestroyWarpOptions(psWO);

    if (hDstDS != nullptr)
    {
        if (pszDstWKT != nullptr)
            GDALSetProjection(hDstDS, pszDstWKT);
        else if (pszSrcWKT != nullptr)
            GDALSetProjection(hDstDS, pszSrcWKT);
        else if (GDALGetGCPCount(hSrcDS) > 0)
            GDALSetProjection(hDstDS, GDALGetGCPProjection(hSrcDS));
        else
            GDALSetProjection(hDstDS, GDALGetProjectionRef(hSrcDS));
    }

    return hDstDS;
}

/************************************************************************/
/*                        GDALCreateWarpedVRT()                         */
/************************************************************************/

/**
 * Create virtual warped dataset.
 *
 * This function will create a warped virtual file representing the
 * input image warped based on a provided transformation.  Output bounds
 * and resolution are provided explicitly.
 *
 * If you want to create an alpha band if the source dataset has none, set
 * psOptions->nDstAlphaBand = GDALGetRasterCount(hSrcDS) + 1.
 *
 * Note that the constructed GDALDatasetH will acquire one or more references
 * to the passed in hSrcDS.  Reference counting semantics on the source
 * dataset should be honoured.  That is, don't just GDALClose() it unless it
 * was opened with GDALOpenShared().
 *
 * It is possible to "transfer" the ownership of the source dataset
 * to the warped dataset in the following way:
 *
 * \code{.c}
 *      GDALDatasetH src_ds = GDALOpen("source.tif");
 *      GDALDatasetH warped_ds = GDALAutoCreateWarpedVRT( src_ds, ... );
 *      GDALReleaseDataset(src_ds); // src_ds is not "owned" fully by warped_ds.
 * Do NOT use GDALClose(src_ds) here
 *      ...
 *      ...
 *      GDALReleaseDataset(warped_ds); // or GDALClose(warped_ds);
 * \endcode
 *
 * Traditonal nested calls are also possible of course:
 *
 * \code{.c}
 *      GDALDatasetH src_ds = GDALOpen("source.tif");
 *      GDALDatasetH warped_ds = GDALAutoCreateWarpedVRT( src_ds, ... );
 *      ...
 *      ...
 *      GDALReleaseDataset(warped_ds); // or GDALClose(warped_ds);
 *      GDALReleaseDataset(src_ds); // or GDALClose(src_ds);
 * \endcode
 *
 * The returned dataset will have no associated filename for itself.  If you
 * want to write the virtual dataset description to a file, use the
 * GDALSetDescription() function (or SetDescription() method) on the dataset
 * to assign a filename before it is closed.
 *
 * @param hSrcDS The source dataset.
 *
 * @param nPixels Width of the virtual warped dataset to create
 *
 * @param nLines Height of the virtual warped dataset to create
 *
 * @param padfGeoTransform Geotransform matrix of the virtual warped dataset
 * to create
 *
 * @param psOptions Warp options. Must be different from NULL.
 *
 * @return NULL on failure, or a new virtual dataset handle on success.
 */

GDALDatasetH CPL_STDCALL GDALCreateWarpedVRT(GDALDatasetH hSrcDS, int nPixels,
                                             int nLines,
                                             double *padfGeoTransform,
                                             GDALWarpOptions *psOptions)

{
    VALIDATE_POINTER1(hSrcDS, "GDALCreateWarpedVRT", nullptr);
    VALIDATE_POINTER1(psOptions, "GDALCreateWarpedVRT", nullptr);

    /* -------------------------------------------------------------------- */
    /*      Create the VRTDataset and populate it with bands.               */
    /* -------------------------------------------------------------------- */
    VRTWarpedDataset *poDS = new VRTWarpedDataset(nPixels, nLines);

    // Call this before assigning hDstDS
    GDALWarpResolveWorkingDataType(psOptions);

    psOptions->hDstDS = poDS;
    poDS->SetGeoTransform(padfGeoTransform);

    for (int i = 0; i < psOptions->nBandCount; i++)
    {
        int nDstBand = psOptions->panDstBands[i];
        while (poDS->GetRasterCount() < nDstBand)
        {
            poDS->AddBand(psOptions->eWorkingDataType, nullptr);
        }

        VRTWarpedRasterBand *poBand =
            static_cast<VRTWarpedRasterBand *>(poDS->GetRasterBand(nDstBand));
        GDALRasterBand *poSrcBand = static_cast<GDALRasterBand *>(
            GDALGetRasterBand(hSrcDS, psOptions->panSrcBands[i]));

        poBand->CopyCommonInfoFrom(poSrcBand);
    }

    while (poDS->GetRasterCount() < psOptions->nDstAlphaBand)
    {
        poDS->AddBand(psOptions->eWorkingDataType, nullptr);
    }
    if (psOptions->nDstAlphaBand)
    {
        poDS->GetRasterBand(psOptions->nDstAlphaBand)
            ->SetColorInterpretation(GCI_AlphaBand);
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize the warp on the VRTWarpedDataset.                    */
    /* -------------------------------------------------------------------- */
    const CPLErr eErr = poDS->Initialize(psOptions);
    if (eErr == CE_Failure)
    {
        psOptions->hDstDS = nullptr;
        delete poDS;
        return nullptr;
    }

    return poDS;
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/* ==================================================================== */
/*                          VRTWarpedDataset                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          VRTWarpedDataset()                          */
/************************************************************************/

VRTWarpedDataset::VRTWarpedDataset(int nXSize, int nYSize, int nBlockXSize,
                                   int nBlockYSize)
    : VRTDataset(nXSize, nYSize,
                 nBlockXSize > 0 ? nBlockXSize : std::min(nXSize, 512),
                 nBlockYSize > 0 ? nBlockYSize : std::min(nYSize, 128)),
      m_poWarper(nullptr), m_nSrcOvrLevel(-2)
{
    eAccess = GA_Update;
    DisableReadWriteMutex();
}

/************************************************************************/
/*                         ~VRTWarpedDataset()                          */
/************************************************************************/

VRTWarpedDataset::~VRTWarpedDataset()

{
    VRTWarpedDataset::FlushCache(true);
    VRTWarpedDataset::CloseDependentDatasets();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTWarpedDataset::CloseDependentDatasets()
{
    bool bHasDroppedRef = CPL_TO_BOOL(VRTDataset::CloseDependentDatasets());

    /* -------------------------------------------------------------------- */
    /*      Cleanup overviews.                                              */
    /* -------------------------------------------------------------------- */
    for (auto &poDS : m_apoOverviews)
    {
        if (poDS && poDS->Release())
        {
            bHasDroppedRef = true;
        }
    }

    m_apoOverviews.clear();

    /* -------------------------------------------------------------------- */
    /*      Cleanup warper if one is in effect.                             */
    /* -------------------------------------------------------------------- */
    if (m_poWarper != nullptr)
    {
        const GDALWarpOptions *psWO = m_poWarper->GetOptions();

        /* --------------------------------------------------------------------
         */
        /*      We take care to only call GDALClose() on psWO->hSrcDS if the */
        /*      reference count drops to zero.  This is makes it so that we */
        /*      can operate reference counting semantics more-or-less */
        /*      properly even if the dataset isn't open in shared mode, */
        /*      though we require that the caller also honour the reference */
        /*      counting semantics even though it isn't a shared dataset. */
        /* --------------------------------------------------------------------
         */
        if (psWO != nullptr && psWO->hSrcDS != nullptr)
        {
            if (GDALReleaseDataset(psWO->hSrcDS))
            {
                bHasDroppedRef = true;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      We are responsible for cleaning up the transformer ourselves. */
        /* --------------------------------------------------------------------
         */
        if (psWO != nullptr && psWO->pTransformerArg != nullptr)
            GDALDestroyTransformer(psWO->pTransformerArg);

        delete m_poWarper;
        m_poWarper = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Destroy the raster bands if they exist.                         */
    /* -------------------------------------------------------------------- */
    for (int iBand = 0; iBand < nBands; iBand++)
    {
        delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                         SetSrcOverviewLevel()                        */
/************************************************************************/

CPLErr VRTWarpedDataset::SetMetadataItem(const char *pszName,
                                         const char *pszValue,
                                         const char *pszDomain)

{
    if ((pszDomain == nullptr || EQUAL(pszDomain, "")) &&
        EQUAL(pszName, "SrcOvrLevel"))
    {
        const int nOldValue = m_nSrcOvrLevel;
        if (pszValue == nullptr || EQUAL(pszValue, "AUTO"))
            m_nSrcOvrLevel = -2;
        else if (STARTS_WITH_CI(pszValue, "AUTO-"))
            m_nSrcOvrLevel = -2 - atoi(pszValue + 5);
        else if (EQUAL(pszValue, "NONE"))
            m_nSrcOvrLevel = -1;
        else if (CPLGetValueType(pszValue) == CPL_VALUE_INTEGER)
            m_nSrcOvrLevel = atoi(pszValue);
        if (m_nSrcOvrLevel != nOldValue)
            SetNeedsFlush();
        return CE_None;
    }
    return VRTDataset::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                        VRTWarpedAddOptions()                         */
/************************************************************************/

static char **VRTWarpedAddOptions(char **papszWarpOptions)
{
    /* Avoid errors when adding an alpha band, but source dataset has */
    /* no alpha band (#4571), and generally don't leave our buffer uninitialized
     */
    if (CSLFetchNameValue(papszWarpOptions, "INIT_DEST") == nullptr)
        papszWarpOptions = CSLSetNameValue(papszWarpOptions, "INIT_DEST", "0");

    /* For https://github.com/OSGeo/gdal/issues/1985 */
    if (CSLFetchNameValue(papszWarpOptions,
                          "ERROR_OUT_IF_EMPTY_SOURCE_WINDOW") == nullptr)
    {
        papszWarpOptions = CSLSetNameValue(
            papszWarpOptions, "ERROR_OUT_IF_EMPTY_SOURCE_WINDOW", "FALSE");
    }
    return papszWarpOptions;
}

/************************************************************************/
/*                             Initialize()                             */
/*                                                                      */
/*      Initialize a dataset from passed in warp options.               */
/************************************************************************/

CPLErr VRTWarpedDataset::Initialize(void *psWO)

{
    if (m_poWarper != nullptr)
        delete m_poWarper;

    m_poWarper = new GDALWarpOperation();

    GDALWarpOptions *psWO_Dup =
        GDALCloneWarpOptions(static_cast<GDALWarpOptions *>(psWO));

    psWO_Dup->papszWarpOptions =
        VRTWarpedAddOptions(psWO_Dup->papszWarpOptions);

    CPLErr eErr = m_poWarper->Initialize(psWO_Dup);

    // The act of initializing this warped dataset with this warp options
    // will result in our assuming ownership of a reference to the
    // hSrcDS.

    if (eErr == CE_None &&
        static_cast<GDALWarpOptions *>(psWO)->hSrcDS != nullptr)
    {
        GDALReferenceDataset(psWO_Dup->hSrcDS);
    }

    GDALDestroyWarpOptions(psWO_Dup);

    if (nBands > 1)
    {
        GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }

    return eErr;
}

/************************************************************************/
/*                        GDALWarpCoordRescaler                         */
/************************************************************************/

class GDALWarpCoordRescaler : public OGRCoordinateTransformation
{
    double m_dfRatioX;
    double m_dfRatioY;

  public:
    GDALWarpCoordRescaler(double dfRatioX, double dfRatioY)
        : m_dfRatioX(dfRatioX), m_dfRatioY(dfRatioY)
    {
    }

    virtual ~GDALWarpCoordRescaler()
    {
    }

    virtual const OGRSpatialReference *GetSourceCS() const override
    {
        return nullptr;
    }

    virtual const OGRSpatialReference *GetTargetCS() const override
    {
        return nullptr;
    }

    virtual int Transform(size_t nCount, double *x, double *y, double * /*z*/,
                          double * /*t*/, int *pabSuccess) override
    {
        for (size_t i = 0; i < nCount; i++)
        {
            x[i] *= m_dfRatioX;
            y[i] *= m_dfRatioY;
            if (pabSuccess)
                pabSuccess[i] = TRUE;
        }
        return TRUE;
    }

    virtual OGRCoordinateTransformation *Clone() const override
    {
        return new GDALWarpCoordRescaler(*this);
    }

    virtual OGRCoordinateTransformation *GetInverse() const override
    {
        return nullptr;
    }
};

/************************************************************************/
/*                        RescaleDstGeoTransform()                      */
/************************************************************************/

static void RescaleDstGeoTransform(double adfDstGeoTransform[6],
                                   int nRasterXSize, int nDstPixels,
                                   int nRasterYSize, int nDstLines)
{
    adfDstGeoTransform[1] *= static_cast<double>(nRasterXSize) / nDstPixels;
    adfDstGeoTransform[2] *= static_cast<double>(nRasterXSize) / nDstPixels;
    adfDstGeoTransform[4] *= static_cast<double>(nRasterYSize) / nDstLines;
    adfDstGeoTransform[5] *= static_cast<double>(nRasterYSize) / nDstLines;
}

/************************************************************************/
/*                        GetSrcOverviewLevel()                         */
/************************************************************************/

int VRTWarpedDataset::GetSrcOverviewLevel(int iOvr,
                                          bool &bThisLevelOnlyOut) const
{
    bThisLevelOnlyOut = false;
    if (m_nSrcOvrLevel < -2)
    {
        if (iOvr + m_nSrcOvrLevel + 2 >= 0)
        {
            return iOvr + m_nSrcOvrLevel + 2;
        }
    }
    else if (m_nSrcOvrLevel == -2)
    {
        return iOvr;
    }
    else if (m_nSrcOvrLevel >= 0)
    {
        bThisLevelOnlyOut = true;
        return m_nSrcOvrLevel;
    }
    return -1;
}

/************************************************************************/
/*                            GetOverviewSize()                         */
/************************************************************************/

bool VRTWarpedDataset::GetOverviewSize(GDALDataset *poSrcDS, int iOvr,
                                       int iSrcOvr, int &nOvrXSize,
                                       int &nOvrYSize, double &dfSrcRatioX,
                                       double &dfSrcRatioY) const
{
    auto poSrcOvrBand = iSrcOvr >= 0
                            ? poSrcDS->GetRasterBand(1)->GetOverview(iSrcOvr)
                            : poSrcDS->GetRasterBand(1);
    if (!poSrcOvrBand)
    {
        return false;
    }
    dfSrcRatioX = static_cast<double>(poSrcDS->GetRasterXSize()) /
                  poSrcOvrBand->GetXSize();
    dfSrcRatioY = static_cast<double>(poSrcDS->GetRasterYSize()) /
                  poSrcOvrBand->GetYSize();
    const double dfTargetRatio =
        static_cast<double>(poSrcDS->GetRasterXSize()) /
        poSrcDS->GetRasterBand(1)->GetOverview(iOvr)->GetXSize();

    nOvrXSize = static_cast<int>(nRasterXSize / dfTargetRatio + 0.5);
    nOvrYSize = static_cast<int>(nRasterYSize / dfTargetRatio + 0.5);
    return nOvrXSize >= 1 && nOvrYSize >= 1;
}

/************************************************************************/
/*                        CreateImplicitOverview()                      */
/************************************************************************/

VRTWarpedDataset *VRTWarpedDataset::CreateImplicitOverview(int iOvr) const
{
    if (!m_poWarper)
        return nullptr;
    const GDALWarpOptions *psWO = m_poWarper->GetOptions();
    if (!psWO->hSrcDS || GDALGetRasterCount(psWO->hSrcDS) == 0)
        return nullptr;
    GDALDataset *poSrcDS = GDALDataset::FromHandle(psWO->hSrcDS);
    GDALDataset *poSrcOvrDS = poSrcDS;
    bool bThisLevelOnly = false;
    const int iSrcOvr = GetSrcOverviewLevel(iOvr, bThisLevelOnly);
    if (iSrcOvr >= 0)
    {
        poSrcOvrDS =
            GDALCreateOverviewDataset(poSrcDS, iSrcOvr, bThisLevelOnly);
    }
    if (poSrcOvrDS == nullptr)
        return nullptr;
    if (poSrcOvrDS == poSrcDS)
        poSrcOvrDS->Reference();

    int nDstPixels = 0;
    int nDstLines = 0;
    double dfSrcRatioX = 0;
    double dfSrcRatioY = 0;
    // Figure out the desired output bounds and resolution.
    if (!GetOverviewSize(poSrcDS, iOvr, iSrcOvr, nDstPixels, nDstLines,
                         dfSrcRatioX, dfSrcRatioY))
    {
        poSrcOvrDS->ReleaseRef();
        return nullptr;
    }

    /* --------------------------------------------------------------------
     */
    /*      Create transformer and warping options. */
    /* --------------------------------------------------------------------
     */
    void *pTransformerArg = GDALCreateSimilarTransformer(
        psWO->pTransformerArg, dfSrcRatioX, dfSrcRatioY);
    if (pTransformerArg == nullptr)
    {
        poSrcOvrDS->ReleaseRef();
        return nullptr;
    }

    GDALWarpOptions *psWOOvr = GDALCloneWarpOptions(psWO);
    psWOOvr->hSrcDS = poSrcOvrDS;
    psWOOvr->pfnTransformer = psWO->pfnTransformer;
    psWOOvr->pTransformerArg = pTransformerArg;

    /* --------------------------------------------------------------------
     */
    /*      We need to rescale the potential CUTLINE */
    /* --------------------------------------------------------------------
     */
    if (psWOOvr->hCutline)
    {
        GDALWarpCoordRescaler oRescaler(1.0 / dfSrcRatioX, 1.0 / dfSrcRatioY);
        static_cast<OGRGeometry *>(psWOOvr->hCutline)->transform(&oRescaler);
    }

    /* --------------------------------------------------------------------
     */
    /*      Rescale the output geotransform on the transformer. */
    /* --------------------------------------------------------------------
     */
    double adfDstGeoTransform[6] = {0.0};
    GDALGetTransformerDstGeoTransform(psWOOvr->pTransformerArg,
                                      adfDstGeoTransform);
    RescaleDstGeoTransform(adfDstGeoTransform, nRasterXSize, nDstPixels,
                           nRasterYSize, nDstLines);
    GDALSetTransformerDstGeoTransform(psWOOvr->pTransformerArg,
                                      adfDstGeoTransform);

    /* --------------------------------------------------------------------
     */
    /*      Create the VRT file. */
    /* --------------------------------------------------------------------
     */
    GDALDatasetH hDstDS = GDALCreateWarpedVRT(poSrcOvrDS, nDstPixels, nDstLines,
                                              adfDstGeoTransform, psWOOvr);

    poSrcOvrDS->ReleaseRef();

    GDALDestroyWarpOptions(psWOOvr);

    if (hDstDS == nullptr)
    {
        GDALDestroyTransformer(pTransformerArg);
        return nullptr;
    }

    auto poOvrDS = static_cast<VRTWarpedDataset *>(hDstDS);
    poOvrDS->m_bIsOverview = true;
    return poOvrDS;
}

/************************************************************************/
/*                           GetOverviewCount()                         */
/************************************************************************/

int VRTWarpedDataset::GetOverviewCount() const
{
    if (m_poWarper)
    {
        const GDALWarpOptions *psWO = m_poWarper->GetOptions();
        if (!m_bIsOverview && psWO->hSrcDS && GDALGetRasterCount(psWO->hSrcDS))
        {
            GDALDataset *poSrcDS = GDALDataset::FromHandle(psWO->hSrcDS);
            int nSrcOverviewCount =
                poSrcDS->GetRasterBand(1)->GetOverviewCount();
            int nCount = 0;
            for (int i = 0; i < nSrcOverviewCount; ++i)
            {
                bool bThisLevelOnly = false;
                const int iSrcOvr = GetSrcOverviewLevel(i, bThisLevelOnly);
                if (iSrcOvr >= 0)
                {
                    int nDstPixels = 0;
                    int nDstLines = 0;
                    double dfSrcRatioX = 0;
                    double dfSrcRatioY = 0;
                    if (!GetOverviewSize(poSrcDS, i, iSrcOvr, nDstPixels,
                                         nDstLines, dfSrcRatioX, dfSrcRatioY))
                    {
                        break;
                    }
                }
                ++nCount;
            }
            return nCount;
        }
    }
    return 0;
}

/************************************************************************/
/*                        CreateImplicitOverviews()                     */
/*                                                                      */
/*      For each overview of the source dataset, create an overview     */
/*      in the warped VRT dataset.                                      */
/************************************************************************/

void VRTWarpedDataset::CreateImplicitOverviews()
{
    if (m_bIsOverview)
        return;
    const int nOvrCount = GetOverviewCount();
    if (m_apoOverviews.empty())
        m_apoOverviews.resize(nOvrCount);
    for (int iOvr = 0; iOvr < nOvrCount; iOvr++)
    {
        if (!m_apoOverviews[iOvr])
        {
            m_apoOverviews[iOvr] = CreateImplicitOverview(iOvr);
        }
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **VRTWarpedDataset::GetFileList()
{
    char **papszFileList = GDALDataset::GetFileList();

    if (m_poWarper != nullptr)
    {
        const GDALWarpOptions *psWO = m_poWarper->GetOptions();
        const char *pszFilename = nullptr;

        if (psWO->hSrcDS != nullptr &&
            (pszFilename =
                 static_cast<GDALDataset *>(psWO->hSrcDS)->GetDescription()) !=
                nullptr)
        {
            VSIStatBufL sStat;
            if (VSIStatL(pszFilename, &sStat) == 0)
            {
                papszFileList = CSLAddString(papszFileList, pszFilename);
            }
        }
    }

    return papszFileList;
}

/************************************************************************/
/* ==================================================================== */
/*                    VRTWarpedOverviewTransformer                      */
/* ==================================================================== */
/************************************************************************/

typedef struct
{
    GDALTransformerInfo sTI;

    GDALTransformerFunc pfnBaseTransformer;
    void *pBaseTransformerArg;
    bool bOwnSubtransformer;

    double dfXOverviewFactor;
    double dfYOverviewFactor;
} VWOTInfo;

static void *VRTCreateWarpedOverviewTransformer(
    GDALTransformerFunc pfnBaseTransformer, void *pBaseTransformArg,
    double dfXOverviewFactor, double dfYOverviewFactor);
static void VRTDestroyWarpedOverviewTransformer(void *pTransformArg);

static int VRTWarpedOverviewTransform(void *pTransformArg, int bDstToSrc,
                                      int nPointCount, double *padfX,
                                      double *padfY, double *padfZ,
                                      int *panSuccess);

#if 0   // TODO: Why?
/************************************************************************/
/*                VRTSerializeWarpedOverviewTransformer()               */
/************************************************************************/

static CPLXMLNode *
VRTSerializeWarpedOverviewTransformer( void *pTransformArg )

{
    VWOTInfo *psInfo = static_cast<VWOTInfo *>( pTransformArg );

    CPLXMLNode *psTree
        = CPLCreateXMLNode( NULL, CXT_Element, "WarpedOverviewTransformer" );

    CPLCreateXMLElementAndValue(
        psTree, "XFactor",
        CPLString().Printf("%g",psInfo->dfXOverviewFactor) );
    CPLCreateXMLElementAndValue(
        psTree, "YFactor",
        CPLString().Printf("%g",psInfo->dfYOverviewFactor) );

/* -------------------------------------------------------------------- */
/*      Capture underlying transformer.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psTransformerContainer
        = CPLCreateXMLNode( psTree, CXT_Element, "BaseTransformer" );

    CPLXMLNode *psTransformer
        = GDALSerializeTransformer( psInfo->pfnBaseTransformer,
                                    psInfo->pBaseTransformerArg );
    if( psTransformer != NULL )
        CPLAddXMLChild( psTransformerContainer, psTransformer );

    return psTree;
}

/************************************************************************/
/*           VRTWarpedOverviewTransformerOwnsSubtransformer()           */
/************************************************************************/

static void VRTWarpedOverviewTransformerOwnsSubtransformer( void *pTransformArg,
                                                            bool bOwnFlag )
{
    VWOTInfo *psInfo = static_cast<VWOTInfo *>( pTransformArg );

    psInfo->bOwnSubtransformer = bOwnFlag;
}

/************************************************************************/
/*            VRTDeserializeWarpedOverviewTransformer()                 */
/************************************************************************/

void* VRTDeserializeWarpedOverviewTransformer( CPLXMLNode *psTree )

{
    const double dfXOverviewFactor =
        CPLAtof(CPLGetXMLValue( psTree, "XFactor",  "1" ));
    const double dfYOverviewFactor =
        CPLAtof(CPLGetXMLValue( psTree, "YFactor",  "1" ));
    GDALTransformerFunc pfnBaseTransform = NULL;
    void *pBaseTransformerArg = NULL;

    CPLXMLNode *psContainer = CPLGetXMLNode( psTree, "BaseTransformer" );

    if( psContainer != NULL && psContainer->psChild != NULL )
    {
        GDALDeserializeTransformer( psContainer->psChild,
                                    &pfnBaseTransform,
                                    &pBaseTransformerArg );
    }

    if( pfnBaseTransform == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Cannot get base transform for scaled coord transformer." );
        return NULL;
    }
    else
    {
        void *pApproxCBData =
                       VRTCreateWarpedOverviewTransformer( pfnBaseTransform,
                                                           pBaseTransformerArg,
                                                           dfXOverviewFactor,
                                                           dfYOverviewFactor );
        VRTWarpedOverviewTransformerOwnsSubtransformer( pApproxCBData, true );

        return pApproxCBData;
    }
}
#endif  // TODO: Why disabled?

/************************************************************************/
/*                   VRTCreateWarpedOverviewTransformer()               */
/************************************************************************/

static void *VRTCreateWarpedOverviewTransformer(
    GDALTransformerFunc pfnBaseTransformer, void *pBaseTransformerArg,
    double dfXOverviewFactor, double dfYOverviewFactor)

{
    if (pfnBaseTransformer == nullptr)
        return nullptr;

    VWOTInfo *psSCTInfo = static_cast<VWOTInfo *>(CPLMalloc(sizeof(VWOTInfo)));
    psSCTInfo->pfnBaseTransformer = pfnBaseTransformer;
    psSCTInfo->pBaseTransformerArg = pBaseTransformerArg;
    psSCTInfo->dfXOverviewFactor = dfXOverviewFactor;
    psSCTInfo->dfYOverviewFactor = dfYOverviewFactor;
    psSCTInfo->bOwnSubtransformer = false;

    memcpy(psSCTInfo->sTI.abySignature, GDAL_GTI2_SIGNATURE,
           strlen(GDAL_GTI2_SIGNATURE));
    psSCTInfo->sTI.pszClassName = "VRTWarpedOverviewTransformer";
    psSCTInfo->sTI.pfnTransform = VRTWarpedOverviewTransform;
    psSCTInfo->sTI.pfnCleanup = VRTDestroyWarpedOverviewTransformer;
#if 0
    psSCTInfo->sTI.pfnSerialize = VRTSerializeWarpedOverviewTransformer;
#endif
    return psSCTInfo;
}

/************************************************************************/
/*               VRTDestroyWarpedOverviewTransformer()                  */
/************************************************************************/

static void VRTDestroyWarpedOverviewTransformer(void *pTransformArg)
{
    VWOTInfo *psInfo = static_cast<VWOTInfo *>(pTransformArg);

    if (psInfo->bOwnSubtransformer)
        GDALDestroyTransformer(psInfo->pBaseTransformerArg);

    CPLFree(psInfo);
}

/************************************************************************/
/*                     VRTWarpedOverviewTransform()                     */
/************************************************************************/

static int VRTWarpedOverviewTransform(void *pTransformArg, int bDstToSrc,
                                      int nPointCount, double *padfX,
                                      double *padfY, double *padfZ,
                                      int *panSuccess)

{
    VWOTInfo *psInfo = static_cast<VWOTInfo *>(pTransformArg);

    if (bDstToSrc)
    {
        for (int i = 0; i < nPointCount; i++)
        {
            padfX[i] *= psInfo->dfXOverviewFactor;
            padfY[i] *= psInfo->dfYOverviewFactor;
        }
    }

    const int bSuccess = psInfo->pfnBaseTransformer(
        psInfo->pBaseTransformerArg, bDstToSrc, nPointCount, padfX, padfY,
        padfZ, panSuccess);

    if (!bDstToSrc)
    {
        for (int i = 0; i < nPointCount; i++)
        {
            padfX[i] /= psInfo->dfXOverviewFactor;
            padfY[i] /= psInfo->dfYOverviewFactor;
        }
    }

    return bSuccess;
}

/************************************************************************/
/*                           BuildOverviews()                           */
/*                                                                      */
/*      For overviews, we actually just build a whole new dataset       */
/*      with an extra layer of transformation on the warper used to     */
/*      accomplish downsampling by the desired factor.                  */
/************************************************************************/

CPLErr VRTWarpedDataset::IBuildOverviews(
    const char * /* pszResampling */, int nOverviews,
    const int *panOverviewList, int /* nListBands */,
    const int * /* panBandList */, GDALProgressFunc pfnProgress,
    void *pProgressData, CSLConstList /*papszOptions*/)
{
    if (m_poWarper == nullptr || m_bIsOverview)
        return CE_Failure;

    /* -------------------------------------------------------------------- */
    /*      Initial progress result.                                        */
    /* -------------------------------------------------------------------- */
    if (!pfnProgress(0.0, nullptr, pProgressData))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return CE_Failure;
    }

    CreateImplicitOverviews();

    /* -------------------------------------------------------------------- */
    /*      Establish which of the overview levels we already have, and     */
    /*      which are new.                                                  */
    /* -------------------------------------------------------------------- */
    int nNewOverviews = 0;
    int *panNewOverviewList =
        static_cast<int *>(CPLCalloc(sizeof(int), nOverviews));
    std::vector<bool> abFoundOverviewFactor(nOverviews);
    for (int i = 0; i < nOverviews; i++)
    {
        for (GDALDataset *const poOverview : m_apoOverviews)
        {
            if (poOverview)
            {
                const int nOvFactor = GDALComputeOvFactor(
                    poOverview->GetRasterXSize(), GetRasterXSize(),
                    poOverview->GetRasterYSize(), GetRasterYSize());

                if (nOvFactor == panOverviewList[i] ||
                    nOvFactor == GDALOvLevelAdjust2(panOverviewList[i],
                                                    GetRasterXSize(),
                                                    GetRasterYSize()))
                    abFoundOverviewFactor[i] = true;
            }
        }

        if (!abFoundOverviewFactor[i])
            panNewOverviewList[nNewOverviews++] = panOverviewList[i];
    }

    /* -------------------------------------------------------------------- */
    /*      Create each missing overview (we don't need to do anything      */
    /*      to update existing overviews).                                  */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    for (int i = 0; i < nNewOverviews; i++)
    {
        /* --------------------------------------------------------------------
         */
        /*      What size should this overview be. */
        /* --------------------------------------------------------------------
         */
        const int nOXSize = (GetRasterXSize() + panNewOverviewList[i] - 1) /
                            panNewOverviewList[i];

        const int nOYSize = (GetRasterYSize() + panNewOverviewList[i] - 1) /
                            panNewOverviewList[i];

        /* --------------------------------------------------------------------
         */
        /*      Find the most appropriate base dataset onto which to build the
         */
        /*      new one. The preference will be an overview dataset with a
         * ratio*/
        /*      greater than ours, and which is not using */
        /*      VRTWarpedOverviewTransform, since those ones are slow. The
         * other*/
        /*      ones are based on overviews of the source dataset. */
        /* --------------------------------------------------------------------
         */
        VRTWarpedDataset *poBaseDataset = this;
        for (auto *poOverview : m_apoOverviews)
        {
            if (poOverview && poOverview->GetRasterXSize() > nOXSize &&
                poOverview->m_poWarper->GetOptions()->pfnTransformer !=
                    VRTWarpedOverviewTransform &&
                poOverview->GetRasterXSize() < poBaseDataset->GetRasterXSize())
            {
                poBaseDataset = poOverview;
            }
        }

        /* --------------------------------------------------------------------
         */
        /*      Create the overview dataset. */
        /* --------------------------------------------------------------------
         */
        VRTWarpedDataset *poOverviewDS = new VRTWarpedDataset(nOXSize, nOYSize);

        for (int iBand = 0; iBand < GetRasterCount(); iBand++)
        {
            GDALRasterBand *const poOldBand = GetRasterBand(iBand + 1);
            VRTWarpedRasterBand *const poNewBand = new VRTWarpedRasterBand(
                poOverviewDS, iBand + 1, poOldBand->GetRasterDataType());

            poNewBand->CopyCommonInfoFrom(poOldBand);
            poOverviewDS->SetBand(iBand + 1, poNewBand);
        }

        /* --------------------------------------------------------------------
         */
        /*      Prepare update transformation information that will apply */
        /*      the overview decimation. */
        /* --------------------------------------------------------------------
         */
        GDALWarpOptions *psWO = const_cast<GDALWarpOptions *>(
            poBaseDataset->m_poWarper->GetOptions());

        /* --------------------------------------------------------------------
         */
        /*      Initialize the new dataset with adjusted warp options, and */
        /*      then restore to original condition. */
        /* --------------------------------------------------------------------
         */
        GDALTransformerFunc pfnTransformerBase = psWO->pfnTransformer;
        void *pTransformerBaseArg = psWO->pTransformerArg;

        psWO->pfnTransformer = VRTWarpedOverviewTransform;
        psWO->pTransformerArg = VRTCreateWarpedOverviewTransformer(
            pfnTransformerBase, pTransformerBaseArg,
            poBaseDataset->GetRasterXSize() / static_cast<double>(nOXSize),
            poBaseDataset->GetRasterYSize() / static_cast<double>(nOYSize));

        eErr = poOverviewDS->Initialize(psWO);

        psWO->pfnTransformer = pfnTransformerBase;
        psWO->pTransformerArg = pTransformerBaseArg;

        if (eErr != CE_None)
        {
            delete poOverviewDS;
            break;
        }

        m_apoOverviews.push_back(poOverviewDS);
    }

    CPLFree(panNewOverviewList);

    /* -------------------------------------------------------------------- */
    /*      Progress finished.                                              */
    /* -------------------------------------------------------------------- */
    pfnProgress(1.0, nullptr, pProgressData);

    SetNeedsFlush();

    return eErr;
}

/*! @endcond */

/************************************************************************/
/*                      GDALInitializeWarpedVRT()                       */
/************************************************************************/

/**
 * Set warp info on virtual warped dataset.
 *
 * Initializes all the warping information for a virtual warped dataset.
 *
 * This method is the same as the C++ method VRTWarpedDataset::Initialize().
 *
 * @param hDS dataset previously created with the VRT driver, and a
 * SUBCLASS of "VRTWarpedDataset".
 *
 * @param psWO the warp options to apply.  Note that ownership of the
 * transformation information is taken over by the function though everything
 * else remains the property of the caller.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

CPLErr CPL_STDCALL GDALInitializeWarpedVRT(GDALDatasetH hDS,
                                           GDALWarpOptions *psWO)

{
    VALIDATE_POINTER1(hDS, "GDALInitializeWarpedVRT", CE_Failure);

    return static_cast<VRTWarpedDataset *>(GDALDataset::FromHandle(hDS))
        ->Initialize(psWO);
}

/*! @cond Doxygen_Suppress */

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTWarpedDataset::XMLInit(const CPLXMLNode *psTree,
                                 const char *pszVRTPathIn)

{

    /* -------------------------------------------------------------------- */
    /*      Initialize blocksize before calling sub-init so that the        */
    /*      band initializers can get it from the dataset object when       */
    /*      they are created.                                               */
    /* -------------------------------------------------------------------- */
    m_nBlockXSize = atoi(CPLGetXMLValue(psTree, "BlockXSize", "512"));
    m_nBlockYSize = atoi(CPLGetXMLValue(psTree, "BlockYSize", "128"));

    /* -------------------------------------------------------------------- */
    /*      Initialize all the general VRT stuff.  This will even           */
    /*      create the VRTWarpedRasterBands and initialize them.            */
    /* -------------------------------------------------------------------- */
    {
        const CPLErr eErr = VRTDataset::XMLInit(psTree, pszVRTPathIn);

        if (eErr != CE_None)
            return eErr;
    }

    // Check that band block sizes didn't change the dataset block size.
    for (int i = 1; i <= nBands; i++)
    {
        int nBlockXSize = 0;
        int nBlockYSize = 0;
        GetRasterBand(i)->GetBlockSize(&nBlockXSize, &nBlockYSize);
        if (nBlockXSize != m_nBlockXSize || nBlockYSize != m_nBlockYSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Block size specified on band %d not consistent with "
                     "dataset block size",
                     i);
            return CE_Failure;
        }
    }

    if (nBands > 1)
    {
        GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }

    /* -------------------------------------------------------------------- */
    /*      Find the GDALWarpOptions XML tree.                              */
    /* -------------------------------------------------------------------- */
    const CPLXMLNode *const psOptionsTree =
        CPLGetXMLNode(psTree, "GDALWarpOptions");
    if (psOptionsTree == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Count not find required GDALWarpOptions in XML.");
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Adjust the SourceDataset in the warp options to take into       */
    /*      account that it is relative to the VRT if appropriate.          */
    /* -------------------------------------------------------------------- */
    const bool bRelativeToVRT = CPL_TO_BOOL(atoi(
        CPLGetXMLValue(psOptionsTree, "SourceDataset.relativeToVRT", "0")));

    const char *pszRelativePath =
        CPLGetXMLValue(psOptionsTree, "SourceDataset", "");
    char *pszAbsolutePath = nullptr;

    if (bRelativeToVRT)
        pszAbsolutePath = CPLStrdup(
            CPLProjectRelativeFilenameSafe(pszVRTPathIn, pszRelativePath)
                .c_str());
    else
        pszAbsolutePath = CPLStrdup(pszRelativePath);

    CPLXMLNode *psOptionsTreeCloned = CPLCloneXMLTree(psOptionsTree);
    CPLSetXMLValue(psOptionsTreeCloned, "SourceDataset", pszAbsolutePath);
    CPLFree(pszAbsolutePath);

    /* -------------------------------------------------------------------- */
    /*      And instantiate the warp options, and corresponding warp        */
    /*      operation.                                                      */
    /* -------------------------------------------------------------------- */
    GDALWarpOptions *psWO = GDALDeserializeWarpOptions(psOptionsTreeCloned);
    CPLDestroyXMLNode(psOptionsTreeCloned);
    if (psWO == nullptr)
        return CE_Failure;

    psWO->papszWarpOptions = VRTWarpedAddOptions(psWO->papszWarpOptions);

    eAccess = GA_Update;

    if (psWO->hDstDS != nullptr)
    {
        GDALClose(psWO->hDstDS);
        psWO->hDstDS = nullptr;
    }

    psWO->hDstDS = this;

    /* -------------------------------------------------------------------- */
    /*      Deserialize vertical shift grids.                               */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psIter = psTree->psChild;
    for (; psWO->hSrcDS != nullptr && psIter != nullptr;
         psIter = psIter->psNext)
    {
        if (psIter->eType != CXT_Element ||
            !EQUAL(psIter->pszValue, "VerticalShiftGrids"))
        {
            continue;
        }

        CPLError(CE_Warning, CPLE_AppDefined,
                 "The VerticalShiftGrids in a warped VRT is now deprecated, "
                 "and will no longer be handled in GDAL 4.0");

        const char *pszVGrids = CPLGetXMLValue(psIter, "Grids", nullptr);
        if (pszVGrids)
        {
            int bInverse =
                CSLTestBoolean(CPLGetXMLValue(psIter, "Inverse", "FALSE"));
            double dfToMeterSrc =
                CPLAtof(CPLGetXMLValue(psIter, "ToMeterSrc", "1.0"));
            double dfToMeterDest =
                CPLAtof(CPLGetXMLValue(psIter, "ToMeterDest", "1.0"));
            char **papszOptions = nullptr;
            CPLXMLNode *psIter2 = psIter->psChild;
            for (; psIter2 != nullptr; psIter2 = psIter2->psNext)
            {
                if (psIter2->eType != CXT_Element ||
                    !EQUAL(psIter2->pszValue, "Option"))
                {
                    continue;
                }
                const char *pszName = CPLGetXMLValue(psIter2, "name", nullptr);
                const char *pszValue =
                    CPLGetXMLValue(psIter2, nullptr, nullptr);
                if (pszName && pszValue)
                {
                    papszOptions =
                        CSLSetNameValue(papszOptions, pszName, pszValue);
                }
            }

            int bError = FALSE;
            GDALDatasetH hGridDataset =
                GDALOpenVerticalShiftGrid(pszVGrids, &bError);
            if (bError && hGridDataset == nullptr)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot open %s. Source dataset will no "
                         "be vertically adjusted regarding "
                         "vertical datum",
                         pszVGrids);
            }
            else if (hGridDataset != nullptr)
            {
                // Transform from source vertical datum to WGS84
                GDALDatasetH hTmpDS = GDALApplyVerticalShiftGrid(
                    psWO->hSrcDS, hGridDataset, bInverse, dfToMeterSrc,
                    dfToMeterDest, papszOptions);
                GDALReleaseDataset(hGridDataset);
                if (hTmpDS == nullptr)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Source dataset will no "
                             "be vertically adjusted regarding "
                             "vertical datum %s",
                             pszVGrids);
                }
                else
                {
                    CPLDebug("GDALWARP",
                             "Adjusting source dataset "
                             "with vertical datum using %s",
                             pszVGrids);
                    GDALReleaseDataset(psWO->hSrcDS);
                    psWO->hSrcDS = hTmpDS;
                }
            }

            CSLDestroy(papszOptions);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Instantiate the warp operation.                                 */
    /* -------------------------------------------------------------------- */
    m_poWarper = new GDALWarpOperation();

    const CPLErr eErr = m_poWarper->Initialize(psWO);
    if (eErr != CE_None)
    {
        /* --------------------------------------------------------------------
         */
        /*      We are responsible for cleaning up the transformer ourselves. */
        /* --------------------------------------------------------------------
         */
        if (psWO->pTransformerArg != nullptr)
        {
            GDALDestroyTransformer(psWO->pTransformerArg);
            psWO->pTransformerArg = nullptr;
        }

        if (psWO->hSrcDS != nullptr)
        {
            GDALClose(psWO->hSrcDS);
            psWO->hSrcDS = nullptr;
        }
    }

    GDALDestroyWarpOptions(psWO);
    if (eErr != CE_None)
    {
        delete m_poWarper;
        m_poWarper = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Deserialize SrcOvrLevel                                         */
    /* -------------------------------------------------------------------- */
    const char *pszSrcOvrLevel = CPLGetXMLValue(psTree, "SrcOvrLevel", nullptr);
    if (pszSrcOvrLevel != nullptr)
    {
        SetMetadataItem("SrcOvrLevel", pszSrcOvrLevel);
    }

    /* -------------------------------------------------------------------- */
    /*      Generate overviews, if appropriate.                             */
    /* -------------------------------------------------------------------- */

    // OverviewList is historical, and quite inefficient, since it uses
    // the full resolution source dataset, so only build it afterwards.
    const CPLStringList aosOverviews(
        CSLTokenizeString(CPLGetXMLValue(psTree, "OverviewList", "")));
    if (!aosOverviews.empty())
        CreateImplicitOverviews();

    for (int iOverview = 0; iOverview < aosOverviews.size(); ++iOverview)
    {
        int nOvFactor = atoi(aosOverviews[iOverview]);

        if (nOvFactor > 0)
            BuildOverviews("NEAREST", 1, &nOvFactor, 0, nullptr, nullptr,
                           nullptr,
                           /*papszOptions=*/nullptr);
        else
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Bad value for overview factor : %s",
                     aosOverviews[iOverview]);
    }

    return eErr;
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTWarpedDataset::SerializeToXML(const char *pszVRTPathIn)

{
    CPLXMLNode *psTree = VRTDataset::SerializeToXML(pszVRTPathIn);

    if (psTree == nullptr)
        return psTree;

    /* -------------------------------------------------------------------- */
    /*      Set subclass.                                                   */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLNode(CPLCreateXMLNode(psTree, CXT_Attribute, "subClass"),
                     CXT_Text, "VRTWarpedDataset");

    /* -------------------------------------------------------------------- */
    /*      Serialize the block size.                                       */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLElementAndValue(psTree, "BlockXSize",
                                CPLSPrintf("%d", m_nBlockXSize));
    CPLCreateXMLElementAndValue(psTree, "BlockYSize",
                                CPLSPrintf("%d", m_nBlockYSize));

    /* -------------------------------------------------------------------- */
    /*      Serialize the overview list (only for non implicit overviews)   */
    /* -------------------------------------------------------------------- */
    if (!m_apoOverviews.empty())
    {
        int nSrcDSOvrCount = 0;
        if (m_poWarper != nullptr && m_poWarper->GetOptions() != nullptr &&
            m_poWarper->GetOptions()->hSrcDS != nullptr &&
            GDALGetRasterCount(m_poWarper->GetOptions()->hSrcDS) > 0)
        {
            nSrcDSOvrCount =
                static_cast<GDALDataset *>(m_poWarper->GetOptions()->hSrcDS)
                    ->GetRasterBand(1)
                    ->GetOverviewCount();
        }

        if (static_cast<int>(m_apoOverviews.size()) != nSrcDSOvrCount)
        {
            const size_t nLen = m_apoOverviews.size() * 8 + 10;
            char *pszOverviewList = static_cast<char *>(CPLMalloc(nLen));
            pszOverviewList[0] = '\0';
            for (auto *poOverviewDS : m_apoOverviews)
            {
                if (poOverviewDS)
                {
                    const int nOvFactor = static_cast<int>(
                        0.5 +
                        GetRasterXSize() / static_cast<double>(
                                               poOverviewDS->GetRasterXSize()));

                    snprintf(pszOverviewList + strlen(pszOverviewList),
                             nLen - strlen(pszOverviewList), "%d ", nOvFactor);
                }
            }

            CPLCreateXMLElementAndValue(psTree, "OverviewList",
                                        pszOverviewList);

            CPLFree(pszOverviewList);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Serialize source overview level.                                */
    /* -------------------------------------------------------------------- */
    if (m_nSrcOvrLevel != -2)
    {
        if (m_nSrcOvrLevel < -2)
            CPLCreateXMLElementAndValue(
                psTree, "SrcOvrLevel",
                CPLSPrintf("AUTO%d", m_nSrcOvrLevel + 2));
        else if (m_nSrcOvrLevel == -1)
            CPLCreateXMLElementAndValue(psTree, "SrcOvrLevel", "NONE");
        else
            CPLCreateXMLElementAndValue(psTree, "SrcOvrLevel",
                                        CPLSPrintf("%d", m_nSrcOvrLevel));
    }

    /* ==================================================================== */
    /*      Serialize the warp options.                                     */
    /* ==================================================================== */
    if (m_poWarper != nullptr)
    {
        /* --------------------------------------------------------------------
         */
        /*      We reset the destination dataset name so it doesn't get */
        /*      written out in the serialize warp options. */
        /* --------------------------------------------------------------------
         */
        char *const pszSavedName = CPLStrdup(GetDescription());
        SetDescription("");

        CPLXMLNode *const psWOTree =
            GDALSerializeWarpOptions(m_poWarper->GetOptions());
        CPLAddXMLChild(psTree, psWOTree);

        SetDescription(pszSavedName);
        CPLFree(pszSavedName);

        /* --------------------------------------------------------------------
         */
        /*      We need to consider making the source dataset relative to */
        /*      the VRT file if possible.  Adjust accordingly. */
        /* --------------------------------------------------------------------
         */
        CPLXMLNode *psSDS = CPLGetXMLNode(psWOTree, "SourceDataset");
        int bRelativeToVRT = FALSE;
        VSIStatBufL sStat;

        if (VSIStatExL(psSDS->psChild->pszValue, &sStat,
                       VSI_STAT_EXISTS_FLAG) == 0)
        {
            std::string osVRTFilename = pszVRTPathIn;
            std::string osSourceDataset = psSDS->psChild->pszValue;
            char *pszCurDir = CPLGetCurrentDir();
            if (CPLIsFilenameRelative(osSourceDataset.c_str()) &&
                !CPLIsFilenameRelative(osVRTFilename.c_str()) &&
                pszCurDir != nullptr)
            {
                osSourceDataset = CPLFormFilenameSafe(
                    pszCurDir, osSourceDataset.c_str(), nullptr);
            }
            else if (!CPLIsFilenameRelative(osSourceDataset.c_str()) &&
                     CPLIsFilenameRelative(osVRTFilename.c_str()) &&
                     pszCurDir != nullptr)
            {
                osVRTFilename = CPLFormFilenameSafe(
                    pszCurDir, osVRTFilename.c_str(), nullptr);
            }
            CPLFree(pszCurDir);
            char *pszRelativePath = CPLStrdup(CPLExtractRelativePath(
                osVRTFilename.c_str(), osSourceDataset.c_str(),
                &bRelativeToVRT));

            CPLFree(psSDS->psChild->pszValue);
            psSDS->psChild->pszValue = pszRelativePath;
        }

        CPLCreateXMLNode(
            CPLCreateXMLNode(psSDS, CXT_Attribute, "relativeToVRT"), CXT_Text,
            bRelativeToVRT ? "1" : "0");
    }

    return psTree;
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

void VRTWarpedDataset::GetBlockSize(int *pnBlockXSize, int *pnBlockYSize) const

{
    CPLAssert(nullptr != pnBlockXSize);
    CPLAssert(nullptr != pnBlockYSize);

    *pnBlockXSize = m_nBlockXSize;
    *pnBlockYSize = m_nBlockYSize;
}

/************************************************************************/
/*                            ProcessBlock()                            */
/*                                                                      */
/*      Warp a single requested block, and then push each band of       */
/*      the result into the block cache.                                */
/************************************************************************/

CPLErr VRTWarpedDataset::ProcessBlock(int iBlockX, int iBlockY)

{
    if (m_poWarper == nullptr)
        return CE_Failure;

    int nReqXSize = m_nBlockXSize;
    if (iBlockX * m_nBlockXSize + nReqXSize > nRasterXSize)
        nReqXSize = nRasterXSize - iBlockX * m_nBlockXSize;
    int nReqYSize = m_nBlockYSize;
    if (iBlockY * m_nBlockYSize + nReqYSize > nRasterYSize)
        nReqYSize = nRasterYSize - iBlockY * m_nBlockYSize;

    GByte *pabyDstBuffer = static_cast<GByte *>(
        m_poWarper->CreateDestinationBuffer(nReqXSize, nReqYSize));

    if (pabyDstBuffer == nullptr)
    {
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Warp into this buffer.                                          */
    /* -------------------------------------------------------------------- */

    const GDALWarpOptions *psWO = m_poWarper->GetOptions();
    const CPLErr eErr = m_poWarper->WarpRegionToBuffer(
        iBlockX * m_nBlockXSize, iBlockY * m_nBlockYSize, nReqXSize, nReqYSize,
        pabyDstBuffer, psWO->eWorkingDataType);

    if (eErr != CE_None)
    {
        m_poWarper->DestroyDestinationBuffer(pabyDstBuffer);
        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Copy out into cache blocks for each band.                       */
    /* -------------------------------------------------------------------- */
    const int nWordSize = GDALGetDataTypeSizeBytes(psWO->eWorkingDataType);
    for (int i = 0; i < psWO->nBandCount; i++)
    {
        int nDstBand = psWO->panDstBands[i];
        if (GetRasterCount() < nDstBand)
        {
            continue;
        }

        GDALRasterBand *poBand = GetRasterBand(nDstBand);
        GDALRasterBlock *poBlock =
            poBand->GetLockedBlockRef(iBlockX, iBlockY, TRUE);

        const GByte *pabyDstBandBuffer =
            pabyDstBuffer +
            static_cast<GPtrDiff_t>(i) * nReqXSize * nReqYSize * nWordSize;

        if (poBlock != nullptr)
        {
            if (poBlock->GetDataRef() != nullptr)
            {
                if (nReqXSize == m_nBlockXSize && nReqYSize == m_nBlockYSize)
                {
                    GDALCopyWords64(
                        pabyDstBandBuffer, psWO->eWorkingDataType, nWordSize,
                        poBlock->GetDataRef(), poBlock->GetDataType(),
                        GDALGetDataTypeSizeBytes(poBlock->GetDataType()),
                        static_cast<GPtrDiff_t>(m_nBlockXSize) * m_nBlockYSize);
                }
                else
                {
                    GByte *pabyBlock =
                        static_cast<GByte *>(poBlock->GetDataRef());
                    const int nDTSize =
                        GDALGetDataTypeSizeBytes(poBlock->GetDataType());
                    for (int iY = 0; iY < nReqYSize; iY++)
                    {
                        GDALCopyWords(
                            pabyDstBandBuffer + static_cast<GPtrDiff_t>(iY) *
                                                    nReqXSize * nWordSize,
                            psWO->eWorkingDataType, nWordSize,
                            pabyBlock + static_cast<GPtrDiff_t>(iY) *
                                            m_nBlockXSize * nDTSize,
                            poBlock->GetDataType(), nDTSize, nReqXSize);
                    }
                }
            }

            poBlock->DropLock();
        }
    }

    m_poWarper->DestroyDestinationBuffer(pabyDstBuffer);

    return CE_None;
}

/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/

// Specialized implementation of IRasterIO() that will be faster than
// using the VRTWarpedRasterBand::IReadBlock() method in situations where
// - a large enough chunk of data is requested at once
// - and multi-threaded warping is enabled (it only kicks in if the warped
//   chunk is large enough) and/or when reading the source dataset is
//   multi-threaded (e.g JP2KAK or JP2OpenJPEG driver).
CPLErr VRTWarpedDataset::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
    GSpacing nLineSpace, GSpacing nBandSpace, GDALRasterIOExtraArg *psExtraArg)
{
    const bool bWholeImage = nXOff == 0 && nYOff == 0 &&
                             nXSize == nRasterXSize && nYSize == nRasterYSize;

    if (eRWFlag == GF_Write ||
        // For too small request fall back to the block-based approach to
        // benefit from caching
        (!bWholeImage &&
         (nBufXSize <= m_nBlockXSize || nBufYSize <= m_nBlockYSize)) ||
        // Or if we don't request all bands at once
        nBandCount < nBands ||
        !CPLTestBool(
            CPLGetConfigOption("GDAL_VRT_WARP_USE_DATASET_RASTERIO", "YES")))
    {
        return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                      pData, nBufXSize, nBufYSize, eBufType,
                                      nBandCount, panBandMap, nPixelSpace,
                                      nLineSpace, nBandSpace, psExtraArg);
    }

    // Try overviews for sub-sampled requests
    if (nBufXSize < nXSize || nBufYSize < nYSize)
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);

        if (bTried)
        {
            return eErr;
        }
    }

    if (m_poWarper == nullptr)
        return CE_Failure;

    const GDALWarpOptions *psWO = m_poWarper->GetOptions();

    if (nBufXSize != nXSize || nBufYSize != nYSize)
    {
        if (!bWholeImage || !GDALTransformHasFastClone(psWO->pTransformerArg))
        {
            return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize, eBufType,
                                          nBandCount, panBandMap, nPixelSpace,
                                          nLineSpace, nBandSpace, psExtraArg);
        }

        // Build a temporary dataset taking into account the rescaling
        void *pTransformerArg = GDALCloneTransformer(psWO->pTransformerArg);
        if (pTransformerArg == nullptr)
        {
            return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize, eBufType,
                                          nBandCount, panBandMap, nPixelSpace,
                                          nLineSpace, nBandSpace, psExtraArg);
        }

        GDALWarpOptions *psRescaledWO = GDALCloneWarpOptions(psWO);
        psRescaledWO->hSrcDS = psWO->hSrcDS;
        psRescaledWO->pfnTransformer = psWO->pfnTransformer;
        psRescaledWO->pTransformerArg = pTransformerArg;

        // Rescale the output geotransform on the transformer.
        double adfDstGeoTransform[6] = {0.0};
        GDALGetTransformerDstGeoTransform(psRescaledWO->pTransformerArg,
                                          adfDstGeoTransform);
        RescaleDstGeoTransform(adfDstGeoTransform, nRasterXSize, nBufXSize,
                               nRasterYSize, nBufYSize);
        GDALSetTransformerDstGeoTransform(psRescaledWO->pTransformerArg,
                                          adfDstGeoTransform);

        GDALDatasetH hDstDS =
            GDALCreateWarpedVRT(psWO->hSrcDS, nBufXSize, nBufYSize,
                                adfDstGeoTransform, psRescaledWO);

        GDALDestroyWarpOptions(psRescaledWO);

        if (hDstDS == nullptr)
        {
            // Not supposed to happen in nominal circumstances. Could perhaps
            // happen if some memory allocation error occurred in code called
            // by GDALCreateWarpedVRT()
            GDALDestroyTransformer(pTransformerArg);
            return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize, eBufType,
                                          nBandCount, panBandMap, nPixelSpace,
                                          nLineSpace, nBandSpace, psExtraArg);
        }

        auto poOvrDS = static_cast<VRTWarpedDataset *>(hDstDS);
        poOvrDS->m_bIsOverview = true;

        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);
        CPLErr eErr = poOvrDS->IRasterIO(GF_Read, 0, 0, nBufXSize, nBufYSize,
                                         pData, nBufXSize, nBufYSize, eBufType,
                                         nBandCount, panBandMap, nPixelSpace,
                                         nLineSpace, nBandSpace, &sExtraArg);

        poOvrDS->ReleaseRef();
        return eErr;
    }

    // Build a map from warped output bands to their index
    std::map<int, int> oMapBandToWarpingBandIndex;
    bool bAllBandsIncreasingOrder =
        (psWO->nBandCount == nBands && nBands == nBandCount);
    for (int i = 0; i < psWO->nBandCount; ++i)
    {
        oMapBandToWarpingBandIndex[psWO->panDstBands[i]] = i;
        if (psWO->panDstBands[i] != i + 1 || panBandMap[i] != i + 1)
        {
            bAllBandsIncreasingOrder = false;
        }
    }

    // Check that all requested bands are actually warped output bands.
    for (int i = 0; i < nBandCount; ++i)
    {
        const int nRasterIOBand = panBandMap[i];
        if (oMapBandToWarpingBandIndex.find(nRasterIOBand) ==
            oMapBandToWarpingBandIndex.end())
        {
            // Not sure if that can happen...
            // but if that does, that will likely later fail in ProcessBlock()
            return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                          pData, nBufXSize, nBufYSize, eBufType,
                                          nBandCount, panBandMap, nPixelSpace,
                                          nLineSpace, nBandSpace, psExtraArg);
        }
    }

    int nSrcXOff = 0;
    int nSrcYOff = 0;
    int nSrcXSize = 0;
    int nSrcYSize = 0;
    double dfSrcXExtraSize = 0;
    double dfSrcYExtraSize = 0;
    double dfSrcFillRatio = 0;
    // Find the source window that corresponds to our target window
    if (m_poWarper->ComputeSourceWindow(nXOff, nYOff, nXSize, nYSize, &nSrcXOff,
                                        &nSrcYOff, &nSrcXSize, &nSrcYSize,
                                        &dfSrcXExtraSize, &dfSrcYExtraSize,
                                        &dfSrcFillRatio) != CE_None)
    {
        return CE_Failure;
    }

    GByte *const pabyDst = static_cast<GByte *>(pData);
    const int nWarpDTSize = GDALGetDataTypeSizeBytes(psWO->eWorkingDataType);

    const double dfMemRequired = m_poWarper->GetWorkingMemoryForWindow(
        nSrcXSize, nSrcYSize, nXSize, nYSize);
    // If we need more warp working memory than allowed, we have to use a
    // splitting strategy until we get below the limit.
    if (dfMemRequired > psWO->dfWarpMemoryLimit && nXSize >= 2 && nYSize >= 2)
    {
        CPLDebugOnly("VRT", "VRTWarpedDataset::IRasterIO(): exceeding warp "
                            "memory. Splitting region");

        GDALRasterIOExtraArg sExtraArg;
        INIT_RASTERIO_EXTRA_ARG(sExtraArg);

        bool bOK;
        // Split along the longest dimension
        if (nXSize >= nYSize)
        {
            const int nHalfXSize = nXSize / 2;
            bOK = IRasterIO(GF_Read, nXOff, nYOff, nHalfXSize, nYSize, pabyDst,
                            nHalfXSize, nYSize, eBufType, nBandCount,
                            panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                            &sExtraArg) == CE_None &&
                  IRasterIO(GF_Read, nXOff + nHalfXSize, nYOff,
                            nXSize - nHalfXSize, nYSize,
                            pabyDst + nHalfXSize * nPixelSpace,
                            nXSize - nHalfXSize, nYSize, eBufType, nBandCount,
                            panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                            &sExtraArg) == CE_None;
        }
        else
        {
            const int nHalfYSize = nYSize / 2;
            bOK = IRasterIO(GF_Read, nXOff, nYOff, nXSize, nHalfYSize, pabyDst,
                            nXSize, nHalfYSize, eBufType, nBandCount,
                            panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                            &sExtraArg) == CE_None &&
                  IRasterIO(GF_Read, nXOff, nYOff + nHalfYSize, nXSize,
                            nYSize - nHalfYSize,
                            pabyDst + nHalfYSize * nLineSpace, nXSize,
                            nYSize - nHalfYSize, eBufType, nBandCount,
                            panBandMap, nPixelSpace, nLineSpace, nBandSpace,
                            &sExtraArg) == CE_None;
        }
        return bOK ? CE_None : CE_Failure;
    }

    CPLDebugOnly("VRT",
                 "Using optimized VRTWarpedDataset::IRasterIO() code path");

    // Allocate a warping destination buffer if needed.
    // We can use directly the output buffer pData if:
    // - we request exactly all warped bands, and that there are as many
    //   warped bands as dataset bands (no alpha)
    // - the output buffer data atype is the warping working data type
    // - the output buffer has a band-sequential layout.
    GByte *pabyWarpBuffer;

    if (bAllBandsIncreasingOrder && psWO->eWorkingDataType == eBufType &&
        nPixelSpace == GDALGetDataTypeSizeBytes(eBufType) &&
        nLineSpace == nPixelSpace * nXSize &&
        (nBands == 1 || nBandSpace == nLineSpace * nYSize))
    {
        pabyWarpBuffer = static_cast<GByte *>(pData);
        m_poWarper->InitializeDestinationBuffer(pabyWarpBuffer, nXSize, nYSize);
    }
    else
    {
        pabyWarpBuffer = static_cast<GByte *>(
            m_poWarper->CreateDestinationBuffer(nXSize, nYSize));

        if (pabyWarpBuffer == nullptr)
        {
            return CE_Failure;
        }
    }

    const CPLErr eErr = m_poWarper->WarpRegionToBuffer(
        nXOff, nYOff, nXSize, nYSize, pabyWarpBuffer, psWO->eWorkingDataType,
        nSrcXOff, nSrcYOff, nSrcXSize, nSrcYSize, dfSrcXExtraSize,
        dfSrcYExtraSize);

    if (pabyWarpBuffer != pData)
    {
        if (eErr == CE_None)
        {
            // Copy warping buffer into user destination buffer
            for (int i = 0; i < nBandCount; i++)
            {
                const int nRasterIOBand = panBandMap[i];
                const auto oIterToWarpingBandIndex =
                    oMapBandToWarpingBandIndex.find(nRasterIOBand);
                // cannot happen due to earlier check
                CPLAssert(oIterToWarpingBandIndex !=
                          oMapBandToWarpingBandIndex.end());

                const GByte *const pabyWarpBandBuffer =
                    pabyWarpBuffer +
                    static_cast<GPtrDiff_t>(oIterToWarpingBandIndex->second) *
                        nXSize * nYSize * nWarpDTSize;
                GByte *const pabyDstBand = pabyDst + i * nBandSpace;

                for (int iY = 0; iY < nYSize; iY++)
                {
                    GDALCopyWords(pabyWarpBandBuffer +
                                      static_cast<GPtrDiff_t>(iY) * nXSize *
                                          nWarpDTSize,
                                  psWO->eWorkingDataType, nWarpDTSize,
                                  pabyDstBand + iY * nLineSpace, eBufType,
                                  static_cast<int>(nPixelSpace), nXSize);
                }
            }
        }

        m_poWarper->DestroyDestinationBuffer(pabyWarpBuffer);
    }

    return eErr;
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr VRTWarpedDataset::AddBand(GDALDataType eType, char ** /* papszOptions */)

{
    if (eType == GDT_Unknown || eType == GDT_TypeCount)
    {
        ReportError(CE_Failure, CPLE_IllegalArg,
                    "Illegal GDT_Unknown/GDT_TypeCount argument");
        return CE_Failure;
    }

    SetBand(GetRasterCount() + 1,
            new VRTWarpedRasterBand(this, GetRasterCount() + 1, eType));

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*                        VRTWarpedRasterBand                           */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        VRTWarpedRasterBand()                         */
/************************************************************************/

VRTWarpedRasterBand::VRTWarpedRasterBand(GDALDataset *poDSIn, int nBandIn,
                                         GDALDataType eType)

{
    Initialize(poDSIn->GetRasterXSize(), poDSIn->GetRasterYSize());

    poDS = poDSIn;
    nBand = nBandIn;
    eAccess = GA_Update;

    static_cast<VRTWarpedDataset *>(poDS)->GetBlockSize(&nBlockXSize,
                                                        &nBlockYSize);

    if (eType != GDT_Unknown)
        eDataType = eType;
}

/************************************************************************/
/*                        ~VRTWarpedRasterBand()                        */
/************************************************************************/

VRTWarpedRasterBand::~VRTWarpedRasterBand()

{
    FlushCache(true);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr VRTWarpedRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
                                       void *pImage)

{
    VRTWarpedDataset *poWDS = static_cast<VRTWarpedDataset *>(poDS);
    const GPtrDiff_t nDataBytes =
        static_cast<GPtrDiff_t>(GDALGetDataTypeSizeBytes(eDataType)) *
        nBlockXSize * nBlockYSize;

    GDALRasterBlock *poBlock = GetLockedBlockRef(nBlockXOff, nBlockYOff, TRUE);
    if (poBlock == nullptr)
        return CE_Failure;

    if (poWDS->m_poWarper)
    {
        const GDALWarpOptions *psWO = poWDS->m_poWarper->GetOptions();
        if (nBand == psWO->nDstAlphaBand)
        {
            // For a reader starting by asking on band 1, we should normally
            // not reach here, because ProcessBlock() on band 1 will have
            // populated the block cache for the regular bands and the alpha
            // band.
            // But if there's no source window corresponding to the block,
            // the alpha band block will not be written through RasterIO(),
            // so we nee to initialize it.
            memset(poBlock->GetDataRef(), 0, nDataBytes);
        }
    }

    const CPLErr eErr = poWDS->ProcessBlock(nBlockXOff, nBlockYOff);

    if (eErr == CE_None && pImage != poBlock->GetDataRef())
    {
        memcpy(pImage, poBlock->GetDataRef(), nDataBytes);
    }

    poBlock->DropLock();

    return eErr;
}

/************************************************************************/
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr VRTWarpedRasterBand::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                        void *pImage)

{
    VRTWarpedDataset *poWDS = static_cast<VRTWarpedDataset *>(poDS);

    // This is a bit tricky. In the case we are warping a VRTWarpedDataset
    // with a destination alpha band, IWriteBlock can be called on that alpha
    // band by GDALWarpDstAlphaMasker
    // We don't need to do anything since the data will have hopefully been
    // read from the block cache before if the reader processes all the bands
    // of a same block.
    if (poWDS->m_poWarper->GetOptions()->nDstAlphaBand != nBand)
    {
        /* Otherwise, call the superclass method, that will fail of course */
        return VRTRasterBand::IWriteBlock(nBlockXOff, nBlockYOff, pImage);
    }

    return CE_None;
}

/************************************************************************/
/*                   EmitErrorMessageIfWriteNotSupported()              */
/************************************************************************/

bool VRTWarpedRasterBand::EmitErrorMessageIfWriteNotSupported(
    const char *pszCaller) const
{
    VRTWarpedDataset *poWDS = static_cast<VRTWarpedDataset *>(poDS);
    // Cf comment in IWriteBlock()
    if (poWDS->m_poWarper->GetOptions()->nDstAlphaBand != nBand)
    {
        ReportError(CE_Failure, CPLE_NoWriteAccess,
                    "%s: attempt to write to a VRTWarpedRasterBand.",
                    pszCaller);

        return true;
    }
    return false;
}

/************************************************************************/
/*                       GetBestOverviewLevel()                         */
/************************************************************************/

int VRTWarpedRasterBand::GetBestOverviewLevel(
    int &nXOff, int &nYOff, int &nXSize, int &nYSize, int nBufXSize,
    int nBufYSize, GDALRasterIOExtraArg *psExtraArg) const
{
    VRTWarpedDataset *poWDS = static_cast<VRTWarpedDataset *>(poDS);

    /* -------------------------------------------------------------------- */
    /*      Compute the desired downsampling factor.  It is                 */
    /*      based on the least reduced axis, and represents the number      */
    /*      of source pixels to one destination pixel.                      */
    /* -------------------------------------------------------------------- */
    const double dfDesiredDownsamplingFactor =
        ((nXSize / static_cast<double>(nBufXSize)) <
             (nYSize / static_cast<double>(nBufYSize)) ||
         nBufYSize == 1)
            ? nXSize / static_cast<double>(nBufXSize)
            : nYSize / static_cast<double>(nBufYSize);

    /* -------------------------------------------------------------------- */
    /*      Find the overview level that largest downsampling factor (most  */
    /*      downsampled) that is still less than (or only a little more)    */
    /*      downsampled than the request.                                   */
    /* -------------------------------------------------------------------- */
    const GDALWarpOptions *psWO = poWDS->m_poWarper->GetOptions();
    GDALDataset *poSrcDS = GDALDataset::FromHandle(psWO->hSrcDS);
    const int nOverviewCount = poSrcDS->GetRasterBand(1)->GetOverviewCount();

    int nBestOverviewXSize = 1;
    int nBestOverviewYSize = 1;
    double dfBestDownsamplingFactor = 0;
    int nBestOverviewLevel = -1;

    const char *pszOversampligThreshold =
        CPLGetConfigOption("GDAL_OVERVIEW_OVERSAMPLING_THRESHOLD", nullptr);

    // Cf https://github.com/OSGeo/gdal/pull/9040#issuecomment-1898524693
    // Do not exactly use a oversampling threshold of 1.0 because of numerical
    // instability.
    const auto AdjustThreshold = [](double x)
    {
        constexpr double EPS = 1e-2;
        return x == 1.0 ? x + EPS : x;
    };
    const double dfOversamplingThreshold = AdjustThreshold(
        pszOversampligThreshold ? CPLAtof(pszOversampligThreshold)
        : psExtraArg && psExtraArg->eResampleAlg != GRIORA_NearestNeighbour
            ? 1.0
            : 1.2);
    for (int iOverview = 0; iOverview < nOverviewCount; iOverview++)
    {
        const GDALRasterBand *poSrcOvrBand = this;
        bool bThisLevelOnly = false;
        const int iSrcOvr =
            poWDS->GetSrcOverviewLevel(iOverview, bThisLevelOnly);
        if (iSrcOvr >= 0)
        {
            poSrcOvrBand = poSrcDS->GetRasterBand(1)->GetOverview(iSrcOvr);
        }
        if (poSrcOvrBand == nullptr)
            break;

        int nDstPixels = 0;
        int nDstLines = 0;
        double dfSrcRatioX = 0;
        double dfSrcRatioY = 0;
        if (!poWDS->GetOverviewSize(poSrcDS, iOverview, iSrcOvr, nDstPixels,
                                    nDstLines, dfSrcRatioX, dfSrcRatioY))
        {
            break;
        }

        // Compute downsampling factor of this overview
        const double dfDownsamplingFactor =
            std::min(nRasterXSize / static_cast<double>(nDstPixels),
                     nRasterYSize / static_cast<double>(nDstLines));

        // Is it nearly the requested factor and better (lower) than
        // the current best factor?
        if (dfDownsamplingFactor >=
                dfDesiredDownsamplingFactor * dfOversamplingThreshold ||
            dfDownsamplingFactor <= dfBestDownsamplingFactor)
        {
            continue;
        }

        // Ignore AVERAGE_BIT2GRAYSCALE overviews for RasterIO purposes.
        const char *pszResampling = const_cast<GDALRasterBand *>(poSrcOvrBand)
                                        ->GetMetadataItem("RESAMPLING");

        if (pszResampling != nullptr &&
            STARTS_WITH_CI(pszResampling, "AVERAGE_BIT2"))
            continue;

        // OK, this is our new best overview.
        nBestOverviewXSize = nDstPixels;
        nBestOverviewYSize = nDstLines;
        nBestOverviewLevel = iOverview;
        dfBestDownsamplingFactor = dfDownsamplingFactor;
    }

    /* -------------------------------------------------------------------- */
    /*      If we didn't find an overview that helps us, just return        */
    /*      indicating failure and the full resolution image will be used.  */
    /* -------------------------------------------------------------------- */
    if (nBestOverviewLevel < 0)
        return -1;

    /* -------------------------------------------------------------------- */
    /*      Recompute the source window in terms of the selected            */
    /*      overview.                                                       */
    /* -------------------------------------------------------------------- */
    const double dfXFactor =
        nRasterXSize / static_cast<double>(nBestOverviewXSize);
    const double dfYFactor =
        nRasterYSize / static_cast<double>(nBestOverviewYSize);
    CPLDebug("GDAL", "Selecting overview %d x %d", nBestOverviewXSize,
             nBestOverviewYSize);

    const int nOXOff = std::min(nBestOverviewXSize - 1,
                                static_cast<int>(nXOff / dfXFactor + 0.5));
    const int nOYOff = std::min(nBestOverviewYSize - 1,
                                static_cast<int>(nYOff / dfYFactor + 0.5));
    int nOXSize = std::max(1, static_cast<int>(nXSize / dfXFactor + 0.5));
    int nOYSize = std::max(1, static_cast<int>(nYSize / dfYFactor + 0.5));
    if (nOXOff + nOXSize > nBestOverviewXSize)
        nOXSize = nBestOverviewXSize - nOXOff;
    if (nOYOff + nOYSize > nBestOverviewYSize)
        nOYSize = nBestOverviewYSize - nOYOff;

    if (psExtraArg)
    {
        if (psExtraArg->bFloatingPointWindowValidity)
        {
            psExtraArg->dfXOff /= dfXFactor;
            psExtraArg->dfXSize /= dfXFactor;
            psExtraArg->dfYOff /= dfYFactor;
            psExtraArg->dfYSize /= dfYFactor;
        }
        else if (psExtraArg->eResampleAlg != GRIORA_NearestNeighbour)
        {
            psExtraArg->bFloatingPointWindowValidity = true;
            psExtraArg->dfXOff = nXOff / dfXFactor;
            psExtraArg->dfXSize = nXSize / dfXFactor;
            psExtraArg->dfYOff = nYOff / dfYFactor;
            psExtraArg->dfYSize = nYSize / dfYFactor;
        }
    }

    nXOff = nOXOff;
    nYOff = nOYOff;
    nXSize = nOXSize;
    nYSize = nOYSize;

    return nBestOverviewLevel;
}

/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/

CPLErr VRTWarpedRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                      int nXSize, int nYSize, void *pData,
                                      int nBufXSize, int nBufYSize,
                                      GDALDataType eBufType,
                                      GSpacing nPixelSpace, GSpacing nLineSpace,
                                      GDALRasterIOExtraArg *psExtraArg)
{
    VRTWarpedDataset *poWDS = static_cast<VRTWarpedDataset *>(poDS);
    if (m_nIRasterIOCounter == 0 && poWDS->GetRasterCount() == 1)
    {
        int anBandMap[] = {nBand};
        ++m_nIRasterIOCounter;
        const CPLErr eErr = poWDS->IRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, 1, anBandMap, nPixelSpace, nLineSpace, 0, psExtraArg);
        --m_nIRasterIOCounter;
        return eErr;
    }

    /* ==================================================================== */
    /*      Do we have overviews that would be appropriate to satisfy       */
    /*      this request?                                                   */
    /* ==================================================================== */
    if ((nBufXSize < nXSize || nBufYSize < nYSize) && GetOverviewCount() &&
        eRWFlag == GF_Read)
    {
        GDALRasterIOExtraArg sExtraArg;
        GDALCopyRasterIOExtraArg(&sExtraArg, psExtraArg);

        const int nOverview = GetBestOverviewLevel(
            nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, &sExtraArg);
        if (nOverview >= 0)
        {
            auto poOvrBand = GetOverview(nOverview);
            if (!poOvrBand)
                return CE_Failure;

            return poOvrBand->RasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                       pData, nBufXSize, nBufYSize, eBufType,
                                       nPixelSpace, nLineSpace, &sExtraArg);
        }
    }

    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpace, nLineSpace, psExtraArg);
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTWarpedRasterBand::SerializeToXML(const char *pszVRTPathIn,
                                                bool &bHasWarnedAboutRAMUsage,
                                                size_t &nAccRAMUsage)

{
    CPLXMLNode *const psTree = VRTRasterBand::SerializeToXML(
        pszVRTPathIn, bHasWarnedAboutRAMUsage, nAccRAMUsage);

    /* -------------------------------------------------------------------- */
    /*      Set subclass.                                                   */
    /* -------------------------------------------------------------------- */
    CPLCreateXMLNode(CPLCreateXMLNode(psTree, CXT_Attribute, "subClass"),
                     CXT_Text, "VRTWarpedRasterBand");

    return psTree;
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int VRTWarpedRasterBand::GetOverviewCount()

{
    VRTWarpedDataset *const poWDS = static_cast<VRTWarpedDataset *>(poDS);
    if (poWDS->m_bIsOverview)
        return 0;

    if (poWDS->m_apoOverviews.empty())
    {
        return poWDS->GetOverviewCount();
    }

    return static_cast<int>(poWDS->m_apoOverviews.size());
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *VRTWarpedRasterBand::GetOverview(int iOverview)

{
    VRTWarpedDataset *const poWDS = static_cast<VRTWarpedDataset *>(poDS);

    const int nOvrCount = GetOverviewCount();
    if (iOverview < 0 || iOverview >= nOvrCount)
        return nullptr;

    if (poWDS->m_apoOverviews.empty())
        poWDS->m_apoOverviews.resize(nOvrCount);
    if (!poWDS->m_apoOverviews[iOverview])
        poWDS->m_apoOverviews[iOverview] =
            poWDS->CreateImplicitOverview(iOverview);
    if (!poWDS->m_apoOverviews[iOverview])
        return nullptr;
    return poWDS->m_apoOverviews[iOverview]->GetRasterBand(nBand);
}

/*! @endcond */
