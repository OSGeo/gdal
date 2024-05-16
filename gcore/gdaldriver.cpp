/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALDriver class (and C wrappers)
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1998, 2000, Frank Warmerdam
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_rat.h"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <set>
#include <sys/stat.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_progress.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "ogr_core.h"
#include "ogrsf_frmts.h"

/************************************************************************/
/*                             GDALDriver()                             */
/************************************************************************/

GDALDriver::GDALDriver() = default;

/************************************************************************/
/*                            ~GDALDriver()                             */
/************************************************************************/

GDALDriver::~GDALDriver()

{
    if (pfnUnloadDriver != nullptr)
        pfnUnloadDriver(this);
}

/************************************************************************/
/*                         GDALCreateDriver()                           */
/************************************************************************/

/**
 * \brief Create a GDALDriver.
 *
 * Creates a driver in the GDAL heap.
 */

GDALDriverH CPL_STDCALL GDALCreateDriver()
{
    return new GDALDriver();
}

/************************************************************************/
/*                         GDALDestroyDriver()                          */
/************************************************************************/

/**
 * \brief Destroy a GDALDriver.
 *
 * This is roughly equivalent to deleting the driver, but is guaranteed
 * to take place in the GDAL heap.  It is important this that function
 * not be called on a driver that is registered with the GDALDriverManager.
 *
 * @param hDriver the driver to destroy.
 */

void CPL_STDCALL GDALDestroyDriver(GDALDriverH hDriver)

{
    if (hDriver != nullptr)
        delete GDALDriver::FromHandle(hDriver);
}

/************************************************************************/
/*                               Open()                                 */
/************************************************************************/

//! @cond Doxygen_Suppress

GDALDataset *GDALDriver::Open(GDALOpenInfo *poOpenInfo, bool bSetOpenOptions)
{

    GDALDataset *poDS = nullptr;
    pfnOpen = GetOpenCallback();
    if (pfnOpen != nullptr)
    {
        poDS = pfnOpen(poOpenInfo);
    }
    else if (pfnOpenWithDriverArg != nullptr)
    {
        poDS = pfnOpenWithDriverArg(this, poOpenInfo);
    }

    if (poDS)
    {
        poDS->nOpenFlags = poOpenInfo->nOpenFlags & ~GDAL_OF_FROM_GDALOPEN;

        if (strlen(poDS->GetDescription()) == 0)
            poDS->SetDescription(poOpenInfo->pszFilename);

        if (poDS->poDriver == nullptr)
            poDS->poDriver = this;

        if (poDS->papszOpenOptions == nullptr && bSetOpenOptions)
        {
            poDS->papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);
        }

        if (!(poOpenInfo->nOpenFlags & GDAL_OF_INTERNAL))
        {
            if (CPLGetPID() != GDALGetResponsiblePIDForCurrentThread())
                CPLDebug(
                    "GDAL",
                    "GDALOpen(%s, this=%p) succeeds as "
                    "%s (pid=%d, responsiblePID=%d).",
                    poOpenInfo->pszFilename, poDS, GetDescription(),
                    static_cast<int>(CPLGetPID()),
                    static_cast<int>(GDALGetResponsiblePIDForCurrentThread()));
            else
                CPLDebug("GDAL", "GDALOpen(%s, this=%p) succeeds as %s.",
                         poOpenInfo->pszFilename, poDS, GetDescription());

            poDS->AddToDatasetOpenList();
        }
    }

    return poDS;
}

//! @endcond

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

/**
 * \brief Create a new dataset with this driver.
 *
 * What argument values are legal for particular drivers is driver specific,
 * and there is no way to query in advance to establish legal values.
 *
 * That function will try to validate the creation option list passed to the
 * driver with the GDALValidateCreationOptions() method. This check can be
 * disabled by defining the configuration option
 * GDAL_VALIDATE_CREATION_OPTIONS=NO.
 *
 * After you have finished working with the returned dataset, it is
 * <b>required</b> to close it with GDALClose(). This does not only close the
 * file handle, but also ensures that all the data and metadata has been written
 * to the dataset (GDALFlushCache() is not sufficient for that purpose).
 *
 * In GDAL 2, the arguments nXSize, nYSize and nBands can be passed to 0 when
 * creating a vector-only dataset for a compatible driver.
 *
 * Equivalent of the C function GDALCreate().
 *
 * @param pszFilename the name of the dataset to create.  UTF-8 encoded.
 * @param nXSize width of created raster in pixels.
 * @param nYSize height of created raster in pixels.
 * @param nBands number of bands.
 * @param eType type of raster.
 * @param papszOptions list of driver specific control parameters.
 * The APPEND_SUBDATASET=YES option can be
 * specified to avoid prior destruction of existing dataset.
 *
 * @return NULL on failure, or a new GDALDataset.
 */

GDALDataset *GDALDriver::Create(const char *pszFilename, int nXSize, int nYSize,
                                int nBands, GDALDataType eType,
                                CSLConstList papszOptions)

{
    /* -------------------------------------------------------------------- */
    /*      Does this format support creation.                              */
    /* -------------------------------------------------------------------- */
    pfnCreate = GetCreateCallback();
    if (pfnCreate == nullptr && pfnCreateEx == nullptr &&
        pfnCreateVectorOnly == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALDriver::Create() ... no create method implemented"
                 " for this format.");

        return nullptr;
    }
    /* -------------------------------------------------------------------- */
    /*      Do some rudimentary argument checking.                          */
    /* -------------------------------------------------------------------- */
    if (nBands < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create dataset with %d bands is illegal,"
                 "Must be >= 0.",
                 nBands);
        return nullptr;
    }

    if (GetMetadataItem(GDAL_DCAP_RASTER) != nullptr &&
        GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr &&
        (nXSize < 1 || nYSize < 1))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Attempt to create %dx%d dataset is illegal,"
                 "sizes must be larger than zero.",
                 nXSize, nYSize);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Make sure we cleanup if there is an existing dataset of this    */
    /*      name.  But even if that seems to fail we will continue since    */
    /*      it might just be a corrupt file or something.                   */
    /* -------------------------------------------------------------------- */
    if (!CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false))
    {
        // Someone issuing Create("foo.tif") on a
        // memory driver doesn't expect files with those names to be deleted
        // on a file system...
        // This is somewhat messy. Ideally there should be a way for the
        // driver to overload the default behavior
        if (!EQUAL(GetDescription(), "MEM") &&
            !EQUAL(GetDescription(), "Memory") &&
            // ogr2ogr -f PostgreSQL might reach the Delete method of the
            // PostgisRaster driver which is undesirable
            !EQUAL(GetDescription(), "PostgreSQL"))
        {
            QuietDelete(pszFilename);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Validate creation options.                                      */
    /* -------------------------------------------------------------------- */
    if (CPLTestBool(
            CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "YES")))
        GDALValidateCreationOptions(this, papszOptions);

    /* -------------------------------------------------------------------- */
    /*      Proceed with creation.                                          */
    /* -------------------------------------------------------------------- */
    CPLDebug("GDAL", "GDALDriver::Create(%s,%s,%d,%d,%d,%s,%p)",
             GetDescription(), pszFilename, nXSize, nYSize, nBands,
             GDALGetDataTypeName(eType), papszOptions);

    GDALDataset *poDS = nullptr;
    if (pfnCreateEx != nullptr)
    {
        poDS = pfnCreateEx(this, pszFilename, nXSize, nYSize, nBands, eType,
                           const_cast<char **>(papszOptions));
    }
    else if (pfnCreate != nullptr)
    {
        poDS = pfnCreate(pszFilename, nXSize, nYSize, nBands, eType,
                         const_cast<char **>(papszOptions));
    }
    else if (nBands < 1)
    {
        poDS = pfnCreateVectorOnly(this, pszFilename,
                                   const_cast<char **>(papszOptions));
    }

    if (poDS != nullptr)
    {
        if (poDS->GetDescription() == nullptr ||
            strlen(poDS->GetDescription()) == 0)
            poDS->SetDescription(pszFilename);

        if (poDS->poDriver == nullptr)
            poDS->poDriver = this;

        poDS->AddToDatasetOpenList();
    }

    return poDS;
}

/************************************************************************/
/*                             GDALCreate()                             */
/************************************************************************/

/**
 * \brief Create a new dataset with this driver.
 *
 * @see GDALDriver::Create()
 */

GDALDatasetH CPL_DLL CPL_STDCALL GDALCreate(GDALDriverH hDriver,
                                            const char *pszFilename, int nXSize,
                                            int nYSize, int nBands,
                                            GDALDataType eBandType,
                                            CSLConstList papszOptions)

{
    VALIDATE_POINTER1(hDriver, "GDALCreate", nullptr);

    return GDALDriver::FromHandle(hDriver)->Create(
        pszFilename, nXSize, nYSize, nBands, eBandType, papszOptions);
}

/************************************************************************/
/*                       CreateMultiDimensional()                       */
/************************************************************************/

/**
 * \brief Create a new multidimensional dataset with this driver.
 *
 * Only drivers that advertise the GDAL_DCAP_MULTIDIM_RASTER capability and
 * implement the pfnCreateMultiDimensional method might return a non nullptr
 * GDALDataset.
 *
 * This is the same as the C function GDALCreateMultiDimensional().
 *
 * @param pszFilename  the name of the dataset to create.  UTF-8 encoded.
 * @param papszRootGroupOptions driver specific options regarding the creation
 *                              of the root group. Might be nullptr.
 * @param papszOptions driver specific options regarding the creation
 *                     of the dataset. Might be nullptr.
 * @return a new dataset, or nullptr in case of failure.
 *
 * @since GDAL 3.1
 */

GDALDataset *
GDALDriver::CreateMultiDimensional(const char *pszFilename,
                                   CSLConstList papszRootGroupOptions,
                                   CSLConstList papszOptions)

{
    /* -------------------------------------------------------------------- */
    /*      Does this format support creation.                              */
    /* -------------------------------------------------------------------- */
    pfnCreateMultiDimensional = GetCreateMultiDimensionalCallback();
    if (pfnCreateMultiDimensional == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALDriver::CreateMultiDimensional() ... "
                 "no CreateMultiDimensional method implemented "
                 "for this format.");

        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Validate creation options.                                      */
    /* -------------------------------------------------------------------- */
    if (CPLTestBool(
            CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "YES")))
    {
        const char *pszOptionList =
            GetMetadataItem(GDAL_DMD_MULTIDIM_DATASET_CREATIONOPTIONLIST);
        CPLString osDriver;
        osDriver.Printf("driver %s", GetDescription());
        GDALValidateOptions(pszOptionList, papszOptions, "creation option",
                            osDriver);
    }

    auto poDstDS = pfnCreateMultiDimensional(pszFilename, papszRootGroupOptions,
                                             papszOptions);

    if (poDstDS != nullptr)
    {
        if (poDstDS->GetDescription() == nullptr ||
            strlen(poDstDS->GetDescription()) == 0)
            poDstDS->SetDescription(pszFilename);

        if (poDstDS->poDriver == nullptr)
            poDstDS->poDriver = this;
    }

    return poDstDS;
}

/************************************************************************/
/*                       GDALCreateMultiDimensional()                   */
/************************************************************************/

/** \brief Create a new multidimensional dataset with this driver.
 *
 * This is the same as the C++ method GDALDriver::CreateMultiDimensional().
 */
GDALDatasetH GDALCreateMultiDimensional(GDALDriverH hDriver,
                                        const char *pszName,
                                        CSLConstList papszRootGroupOptions,
                                        CSLConstList papszOptions)
{
    VALIDATE_POINTER1(hDriver, __func__, nullptr);
    VALIDATE_POINTER1(pszName, __func__, nullptr);
    return GDALDataset::ToHandle(
        GDALDriver::FromHandle(hDriver)->CreateMultiDimensional(
            pszName, papszRootGroupOptions, papszOptions));
}

/************************************************************************/
/*                  DefaultCreateCopyMultiDimensional()                 */
/************************************************************************/

//! @cond Doxygen_Suppress

CPLErr GDALDriver::DefaultCreateCopyMultiDimensional(
    GDALDataset *poSrcDS, GDALDataset *poDstDS, bool bStrict,
    CSLConstList papszOptions, GDALProgressFunc pfnProgress,
    void *pProgressData)
{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    auto poSrcRG = poSrcDS->GetRootGroup();
    if (!poSrcRG)
        return CE_Failure;
    auto poDstRG = poDstDS->GetRootGroup();
    if (!poDstRG)
        return CE_Failure;
    GUInt64 nCurCost = 0;
    return poDstRG->CopyFrom(poDstRG, poSrcDS, poSrcRG, bStrict, nCurCost,
                             poSrcRG->GetTotalCopyCost(), pfnProgress,
                             pProgressData, papszOptions)
               ? CE_None
               : CE_Failure;
}

//! @endcond

/************************************************************************/
/*                          DefaultCopyMasks()                          */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDriver::DefaultCopyMasks(GDALDataset *poSrcDS, GDALDataset *poDstDS,
                                    int bStrict)

{
    return DefaultCopyMasks(poSrcDS, poDstDS, bStrict, nullptr, nullptr,
                            nullptr);
}

CPLErr GDALDriver::DefaultCopyMasks(GDALDataset *poSrcDS, GDALDataset *poDstDS,
                                    int bStrict, CSLConstList /*papszOptions*/,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData)

{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
        return CE_None;

    /* -------------------------------------------------------------------- */
    /*      Try to copy mask if it seems appropriate.                       */
    /* -------------------------------------------------------------------- */
    const char *papszOptions[2] = {"COMPRESSED=YES", nullptr};
    CPLErr eErr = CE_None;

    int nTotalBandsWithMask = 0;
    for (int iBand = 0; iBand < nBands; ++iBand)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand + 1);

        int nMaskFlags = poSrcBand->GetMaskFlags();
        if (!(nMaskFlags &
              (GMF_ALL_VALID | GMF_PER_DATASET | GMF_ALPHA | GMF_NODATA)))
        {
            nTotalBandsWithMask++;
        }
    }

    int iBandWithMask = 0;
    for (int iBand = 0; eErr == CE_None && iBand < nBands; ++iBand)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(iBand + 1);

        int nMaskFlags = poSrcBand->GetMaskFlags();
        if (eErr == CE_None && !(nMaskFlags & (GMF_ALL_VALID | GMF_PER_DATASET |
                                               GMF_ALPHA | GMF_NODATA)))
        {
            GDALRasterBand *poDstBand = poDstDS->GetRasterBand(iBand + 1);
            if (poDstBand != nullptr)
            {
                eErr = poDstBand->CreateMaskBand(nMaskFlags);
                if (eErr == CE_None)
                {
                    // coverity[divide_by_zero]
                    void *pScaledData = GDALCreateScaledProgress(
                        double(iBandWithMask) / nTotalBandsWithMask,
                        double(iBandWithMask + 1) / nTotalBandsWithMask,
                        pfnProgress, pProgressData);
                    eErr = GDALRasterBandCopyWholeRaster(
                        poSrcBand->GetMaskBand(), poDstBand->GetMaskBand(),
                        papszOptions, GDALScaledProgress, pScaledData);
                    GDALDestroyScaledProgress(pScaledData);
                }
                else if (!bStrict)
                {
                    eErr = CE_None;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to copy a per-dataset mask if we have one.                  */
    /* -------------------------------------------------------------------- */
    const int nMaskFlags = poSrcDS->GetRasterBand(1)->GetMaskFlags();
    if (eErr == CE_None &&
        !(nMaskFlags & (GMF_ALL_VALID | GMF_ALPHA | GMF_NODATA)) &&
        (nMaskFlags & GMF_PER_DATASET))
    {
        eErr = poDstDS->CreateMaskBand(nMaskFlags);
        if (eErr == CE_None)
        {
            eErr = GDALRasterBandCopyWholeRaster(
                poSrcDS->GetRasterBand(1)->GetMaskBand(),
                poDstDS->GetRasterBand(1)->GetMaskBand(), papszOptions,
                pfnProgress, pProgressData);
        }
        else if (!bStrict)
        {
            eErr = CE_None;
        }
    }

    return eErr;
}

/************************************************************************/
/*                         DefaultCreateCopy()                          */
/************************************************************************/

GDALDataset *GDALDriver::DefaultCreateCopy(const char *pszFilename,
                                           GDALDataset *poSrcDS, int bStrict,
                                           CSLConstList papszOptions,
                                           GDALProgressFunc pfnProgress,
                                           void *pProgressData)

{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    CPLErrorReset();

    /* -------------------------------------------------------------------- */
    /*      Use multidimensional raster API if available.                   */
    /* -------------------------------------------------------------------- */
    auto poSrcGroup = poSrcDS->GetRootGroup();
    if (poSrcGroup != nullptr && GetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER))
    {
        CPLStringList aosDatasetCO;
        for (const char *pszOption : cpl::Iterate(papszOptions))
        {
            if (!STARTS_WITH_CI(pszOption, "ARRAY:"))
                aosDatasetCO.AddString(pszOption);
        }
        auto poDstDS = std::unique_ptr<GDALDataset>(
            CreateMultiDimensional(pszFilename, nullptr, aosDatasetCO.List()));
        if (!poDstDS)
            return nullptr;
        auto poDstGroup = poDstDS->GetRootGroup();
        if (!poDstGroup)
            return nullptr;
        if (DefaultCreateCopyMultiDimensional(
                poSrcDS, poDstDS.get(), CPL_TO_BOOL(bStrict), papszOptions,
                pfnProgress, pProgressData) != CE_None)
            return nullptr;
        return poDstDS.release();
    }

    /* -------------------------------------------------------------------- */
    /*      Validate that we can create the output as requested.            */
    /* -------------------------------------------------------------------- */
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    const int nBands = poSrcDS->GetRasterCount();

    CPLDebug("GDAL", "Using default GDALDriver::CreateCopy implementation.");

    const int nLayerCount = poSrcDS->GetLayerCount();
    if (nBands == 0 && nLayerCount == 0 &&
        GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALDriver::DefaultCreateCopy does not support zero band");
        return nullptr;
    }
    if (poSrcDS->GetDriver() != nullptr &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_RASTER) != nullptr &&
        poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr &&
        GetMetadataItem(GDAL_DCAP_RASTER) == nullptr &&
        GetMetadataItem(GDAL_DCAP_VECTOR) != nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source driver is raster-only whereas output driver is "
                 "vector-only");
        return nullptr;
    }
    else if (poSrcDS->GetDriver() != nullptr &&
             poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_RASTER) ==
                 nullptr &&
             poSrcDS->GetDriver()->GetMetadataItem(GDAL_DCAP_VECTOR) !=
                 nullptr &&
             GetMetadataItem(GDAL_DCAP_RASTER) != nullptr &&
             GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Source driver is vector-only whereas output driver is "
                 "raster-only");
        return nullptr;
    }

    if (!pfnProgress(0.0, nullptr, pProgressData))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Propagate some specific structural metadata as options if it    */
    /*      appears to be supported by the target driver and the caller     */
    /*      didn't provide values.                                          */
    /* -------------------------------------------------------------------- */
    char **papszCreateOptions = CSLDuplicate(papszOptions);
    const char *const apszOptItems[] = {"NBITS", "IMAGE_STRUCTURE", "PIXELTYPE",
                                        "IMAGE_STRUCTURE", nullptr};

    for (int iOptItem = 0; nBands > 0 && apszOptItems[iOptItem] != nullptr;
         iOptItem += 2)
    {
        // does the source have this metadata item on the first band?
        auto poBand = poSrcDS->GetRasterBand(1);
        poBand->EnablePixelTypeSignedByteWarning(false);
        const char *pszValue = poBand->GetMetadataItem(
            apszOptItems[iOptItem], apszOptItems[iOptItem + 1]);
        poBand->EnablePixelTypeSignedByteWarning(true);

        if (pszValue == nullptr)
            continue;

        // Do not override provided value.
        if (CSLFetchNameValue(papszCreateOptions, pszValue) != nullptr)
            continue;

        // Does this appear to be a supported creation option on this driver?
        const char *pszOptionList =
            GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);

        if (pszOptionList == nullptr ||
            strstr(pszOptionList, apszOptItems[iOptItem]) == nullptr)
            continue;

        papszCreateOptions = CSLSetNameValue(papszCreateOptions,
                                             apszOptItems[iOptItem], pszValue);
    }

    /* -------------------------------------------------------------------- */
    /*      Create destination dataset.                                     */
    /* -------------------------------------------------------------------- */
    GDALDataType eType = GDT_Unknown;

    if (nBands > 0)
        eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();
    GDALDataset *poDstDS =
        Create(pszFilename, nXSize, nYSize, nBands, eType, papszCreateOptions);

    CSLDestroy(papszCreateOptions);

    if (poDstDS == nullptr)
        return nullptr;

    int nDstBands = poDstDS->GetRasterCount();
    CPLErr eErr = CE_None;
    if (nDstBands != nBands)
    {
        if (GetMetadataItem(GDAL_DCAP_RASTER) != nullptr)
        {
            // Should not happen for a well-behaved driver.
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Output driver created only %d bands whereas %d were expected",
                nDstBands, nBands);
            eErr = CE_Failure;
        }
        nDstBands = 0;
    }

    /* -------------------------------------------------------------------- */
    /*      Try setting the projection and geotransform if it seems         */
    /*      suitable.                                                       */
    /* -------------------------------------------------------------------- */
    double adfGeoTransform[6] = {};

    if (nDstBands == 0 && !bStrict)
        CPLTurnFailureIntoWarning(true);

    if (eErr == CE_None &&
        poSrcDS->GetGeoTransform(adfGeoTransform) == CE_None
        // TODO(schwehr): The default value check should be a function.
        && (adfGeoTransform[0] != 0.0 || adfGeoTransform[1] != 1.0 ||
            adfGeoTransform[2] != 0.0 || adfGeoTransform[3] != 0.0 ||
            adfGeoTransform[4] != 0.0 || adfGeoTransform[5] != 1.0))
    {
        eErr = poDstDS->SetGeoTransform(adfGeoTransform);
        if (!bStrict)
            eErr = CE_None;
    }

    if (eErr == CE_None)
    {
        const auto poSrcSRS = poSrcDS->GetSpatialRef();
        if (poSrcSRS && !poSrcSRS->IsEmpty())
        {
            eErr = poDstDS->SetSpatialRef(poSrcSRS);
            if (!bStrict)
                eErr = CE_None;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Copy GCPs.                                                      */
    /* -------------------------------------------------------------------- */
    if (poSrcDS->GetGCPCount() > 0 && eErr == CE_None)
    {
        eErr = poDstDS->SetGCPs(poSrcDS->GetGCPCount(), poSrcDS->GetGCPs(),
                                poSrcDS->GetGCPProjection());
        if (!bStrict)
            eErr = CE_None;
    }

    if (nDstBands == 0 && !bStrict)
        CPLTurnFailureIntoWarning(false);

    /* -------------------------------------------------------------------- */
    /*      Copy metadata.                                                  */
    /* -------------------------------------------------------------------- */
    DefaultCopyMetadata(poSrcDS, poDstDS, papszOptions, nullptr);

    /* -------------------------------------------------------------------- */
    /*      Loop copying bands.                                             */
    /* -------------------------------------------------------------------- */
    for (int iBand = 0; eErr == CE_None && iBand < nDstBands; ++iBand)
    {
        GDALRasterBand *const poSrcBand = poSrcDS->GetRasterBand(iBand + 1);
        GDALRasterBand *const poDstBand = poDstDS->GetRasterBand(iBand + 1);

        /* --------------------------------------------------------------------
         */
        /*      Do we need to copy a colortable. */
        /* --------------------------------------------------------------------
         */
        GDALColorTable *const poCT = poSrcBand->GetColorTable();
        if (poCT != nullptr)
            poDstBand->SetColorTable(poCT);

        /* --------------------------------------------------------------------
         */
        /*      Do we need to copy other metadata?  Most of this is */
        /*      non-critical, so lets not bother folks if it fails are we */
        /*      are not in strict mode. */
        /* --------------------------------------------------------------------
         */
        if (!bStrict)
            CPLTurnFailureIntoWarning(true);

        if (strlen(poSrcBand->GetDescription()) > 0)
            poDstBand->SetDescription(poSrcBand->GetDescription());

        if (CSLCount(poSrcBand->GetMetadata()) > 0)
            poDstBand->SetMetadata(poSrcBand->GetMetadata());

        int bSuccess = FALSE;
        double dfValue = poSrcBand->GetOffset(&bSuccess);
        if (bSuccess && dfValue != 0.0)
            poDstBand->SetOffset(dfValue);

        dfValue = poSrcBand->GetScale(&bSuccess);
        if (bSuccess && dfValue != 1.0)
            poDstBand->SetScale(dfValue);

        GDALCopyNoDataValue(poDstBand, poSrcBand);

        if (poSrcBand->GetColorInterpretation() != GCI_Undefined &&
            poSrcBand->GetColorInterpretation() !=
                poDstBand->GetColorInterpretation())
            poDstBand->SetColorInterpretation(
                poSrcBand->GetColorInterpretation());

        char **papszCatNames = poSrcBand->GetCategoryNames();
        if (nullptr != papszCatNames)
            poDstBand->SetCategoryNames(papszCatNames);

        // Only copy RAT if it is of reasonable size to fit in memory
        GDALRasterAttributeTable *poRAT = poSrcBand->GetDefaultRAT();
        if (poRAT != nullptr && static_cast<GIntBig>(poRAT->GetColumnCount()) *
                                        poRAT->GetRowCount() <
                                    1024 * 1024)
        {
            poDstBand->SetDefaultRAT(poRAT);
        }

        if (!bStrict)
        {
            CPLTurnFailureIntoWarning(false);
        }
        else
        {
            eErr = CPLGetLastErrorType();
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Copy image data.                                                */
    /* -------------------------------------------------------------------- */
    if (eErr == CE_None && nDstBands > 0)
        eErr = GDALDatasetCopyWholeRaster(poSrcDS, poDstDS, nullptr,
                                          pfnProgress, pProgressData);

    /* -------------------------------------------------------------------- */
    /*      Should we copy some masks over?                                 */
    /* -------------------------------------------------------------------- */
    if (eErr == CE_None && nDstBands > 0)
        eErr = DefaultCopyMasks(poSrcDS, poDstDS, eErr);

    /* -------------------------------------------------------------------- */
    /*      Copy vector layers                                              */
    /* -------------------------------------------------------------------- */
    if (eErr == CE_None)
    {
        if (nLayerCount > 0 && poDstDS->TestCapability(ODsCCreateLayer))
        {
            for (int iLayer = 0; iLayer < nLayerCount; ++iLayer)
            {
                OGRLayer *poLayer = poSrcDS->GetLayer(iLayer);

                if (poLayer == nullptr)
                    continue;

                poDstDS->CopyLayer(poLayer, poLayer->GetName(), nullptr);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Try to cleanup the output dataset if the translation failed.    */
    /* -------------------------------------------------------------------- */
    if (eErr != CE_None)
    {
        delete poDstDS;
        if (!CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false))
        {
            // Only delete if creating a new file
            Delete(pszFilename);
        }
        return nullptr;
    }
    else
    {
        CPLErrorReset();
    }

    return poDstDS;
}

/************************************************************************/
/*                       DefaultCopyMetadata()                          */
/************************************************************************/

void GDALDriver::DefaultCopyMetadata(GDALDataset *poSrcDS, GDALDataset *poDstDS,
                                     CSLConstList papszOptions,
                                     CSLConstList papszExcludedDomains)
{
    const char *pszCopySrcMDD =
        CSLFetchNameValueDef(papszOptions, "COPY_SRC_MDD", "AUTO");
    char **papszSrcMDD = CSLFetchNameValueMultiple(papszOptions, "SRC_MDD");
    if (EQUAL(pszCopySrcMDD, "AUTO") || CPLTestBool(pszCopySrcMDD) ||
        papszSrcMDD)
    {
        if ((!papszSrcMDD || CSLFindString(papszSrcMDD, "") >= 0 ||
             CSLFindString(papszSrcMDD, "_DEFAULT_") >= 0) &&
            CSLFindString(papszExcludedDomains, "") < 0 &&
            CSLFindString(papszExcludedDomains, "_DEFAULT_") < 0)
        {
            if (poSrcDS->GetMetadata() != nullptr)
                poDstDS->SetMetadata(poSrcDS->GetMetadata());
        }

        /* -------------------------------------------------------------------- */
        /*      Copy transportable special domain metadata.                     */
        /*      It would be nice to copy geolocation, but it is pretty fragile. */
        /* -------------------------------------------------------------------- */
        constexpr const char *apszDefaultDomains[] = {
            "RPC", "xml:XMP", "json:ISIS3", "json:VICAR"};
        for (const char *pszDomain : apszDefaultDomains)
        {
            if ((!papszSrcMDD || CSLFindString(papszSrcMDD, pszDomain) >= 0) &&
                CSLFindString(papszExcludedDomains, pszDomain) < 0)
            {
                char **papszMD = poSrcDS->GetMetadata(pszDomain);
                if (papszMD)
                    poDstDS->SetMetadata(papszMD, pszDomain);
            }
        }

        if ((!EQUAL(pszCopySrcMDD, "AUTO") && CPLTestBool(pszCopySrcMDD)) ||
            papszSrcMDD)
        {
            for (const char *pszDomain :
                 CPLStringList(poSrcDS->GetMetadataDomainList()))
            {
                if (pszDomain[0] != 0 &&
                    (!papszSrcMDD ||
                     CSLFindString(papszSrcMDD, pszDomain) >= 0))
                {
                    bool bCanCopy = true;
                    if (CSLFindString(papszExcludedDomains, pszDomain) >= 0)
                    {
                        bCanCopy = false;
                    }
                    else
                    {
                        for (const char *pszOtherDomain : apszDefaultDomains)
                        {
                            if (EQUAL(pszDomain, pszOtherDomain))
                            {
                                bCanCopy = false;
                                break;
                            }
                        }
                        if (!papszSrcMDD)
                        {
                            constexpr const char *const apszReservedDomains[] =
                                {"IMAGE_STRUCTURE", "DERIVED_SUBDATASETS"};
                            for (const char *pszOtherDomain :
                                 apszReservedDomains)
                            {
                                if (EQUAL(pszDomain, pszOtherDomain))
                                {
                                    bCanCopy = false;
                                    break;
                                }
                            }
                        }
                    }
                    if (bCanCopy)
                    {
                        poDstDS->SetMetadata(poSrcDS->GetMetadata(pszDomain),
                                             pszDomain);
                    }
                }
            }
        }
    }
    CSLDestroy(papszSrcMDD);
}

/************************************************************************/
/*                      QuietDeleteForCreateCopy()                      */
/************************************************************************/

CPLErr GDALDriver::QuietDeleteForCreateCopy(const char *pszFilename,
                                            GDALDataset *poSrcDS)
{
    // Someone issuing CreateCopy("foo.tif") on a
    // memory driver doesn't expect files with those names to be deleted
    // on a file system...
    // This is somewhat messy. Ideally there should be a way for the
    // driver to overload the default behavior
    if (!EQUAL(GetDescription(), "MEM") && !EQUAL(GetDescription(), "Memory") &&
        // Also exclude database formats for which there's no file list
        // and whose opening might be slow (GeoRaster in particular)
        !EQUAL(GetDescription(), "GeoRaster") &&
        !EQUAL(GetDescription(), "PostGISRaster"))
    {
        /* --------------------------------------------------------------------
         */
        /*      Establish list of files of output dataset if it already
         * exists. */
        /* --------------------------------------------------------------------
         */
        std::set<std::string> oSetExistingDestFiles;
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            const char *const apszAllowedDrivers[] = {GetDescription(),
                                                      nullptr};
            auto poExistingOutputDS =
                std::unique_ptr<GDALDataset>(GDALDataset::Open(
                    pszFilename, GDAL_OF_RASTER, apszAllowedDrivers));
            if (poExistingOutputDS)
            {
                for (const char *pszFileInList :
                     CPLStringList(poExistingOutputDS->GetFileList()))
                {
                    oSetExistingDestFiles.insert(
                        CPLString(pszFileInList).replaceAll('\\', '/'));
                }
            }
            CPLPopErrorHandler();
        }

        /* --------------------------------------------------------------------
         */
        /*      Check if the source dataset shares some files with the dest
         * one.*/
        /* --------------------------------------------------------------------
         */
        std::set<std::string> oSetExistingDestFilesFoundInSource;
        if (!oSetExistingDestFiles.empty())
        {
            CPLPushErrorHandler(CPLQuietErrorHandler);
            // We need to reopen in a temporary dataset for the particular
            // case of overwritten a .tif.ovr file from a .tif
            // If we probe the file list of the .tif, it will then open the
            // .tif.ovr !
            const char *const apszAllowedDrivers[] = {
                poSrcDS->GetDriver() ? poSrcDS->GetDriver()->GetDescription()
                                     : nullptr,
                nullptr};
            auto poSrcDSTmp = std::unique_ptr<GDALDataset>(GDALDataset::Open(
                poSrcDS->GetDescription(), GDAL_OF_RASTER, apszAllowedDrivers,
                poSrcDS->papszOpenOptions));
            if (poSrcDSTmp)
            {
                for (const char *pszFileInList :
                     CPLStringList(poSrcDSTmp->GetFileList()))
                {
                    CPLString osFilename(pszFileInList);
                    osFilename.replaceAll('\\', '/');
                    if (oSetExistingDestFiles.find(osFilename) !=
                        oSetExistingDestFiles.end())
                    {
                        oSetExistingDestFilesFoundInSource.insert(osFilename);
                    }
                }
            }
            CPLPopErrorHandler();
        }

        // If the source file(s) and the dest one share some files in
        // common, only remove the files that are *not* in common
        if (!oSetExistingDestFilesFoundInSource.empty())
        {
            for (const std::string &osFilename : oSetExistingDestFiles)
            {
                if (oSetExistingDestFilesFoundInSource.find(osFilename) ==
                    oSetExistingDestFilesFoundInSource.end())
                {
                    VSIUnlink(osFilename.c_str());
                }
            }
        }

        QuietDelete(pszFilename);
    }

    return CE_None;
}

//! @endcond

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

/**
 * \brief Create a copy of a dataset.
 *
 * This method will attempt to create a copy of a raster dataset with the
 * indicated filename, and in this drivers format.  Band number, size,
 * type, projection, geotransform and so forth are all to be copied from
 * the provided template dataset.
 *
 * Note that many sequential write once formats (such as JPEG and PNG) don't
 * implement the Create() method but do implement this CreateCopy() method.
 * If the driver doesn't implement CreateCopy(), but does implement Create()
 * then the default CreateCopy() mechanism built on calling Create() will
 * be used.
 * So to test if CreateCopy() is available, you can test if GDAL_DCAP_CREATECOPY
 * or GDAL_DCAP_CREATE is set in the GDAL metadata.
 *
 * It is intended that CreateCopy() will often be used with a source dataset
 * which is a virtual dataset allowing configuration of band types, and other
 * information without actually duplicating raster data (see the VRT driver).
 * This is what is done by the gdal_translate utility for example.
 *
 * That function will try to validate the creation option list passed to the
 * driver with the GDALValidateCreationOptions() method. This check can be
 * disabled by defining the configuration option
 * GDAL_VALIDATE_CREATION_OPTIONS=NO.
 *
 * This function copy all metadata from the default domain ("")
 *
 * Even is bStrict is TRUE, only the <b>value</b> of the data is equivalent,
 * but the data layout (INTERLEAVE as PIXEL/LINE/BAND) of the dst dataset is
 * controlled by the papszOptions creation options, and may differ from the
 * poSrcDS src dataset.
 * Starting from GDAL 3.5, if no INTERLEAVE and COMPRESS creation option has
 * been specified in papszOptions, and if the driver supports equivalent
 * interleaving as the src dataset, the CreateCopy() will internally add the
 * proper creation option to get the same data interleaving.
 *
 * After you have finished working with the returned dataset, it is
 * <b>required</b> to close it with GDALClose(). This does not only close the
 * file handle, but also ensures that all the data and metadata has been written
 * to the dataset (GDALFlushCache() is not sufficient for that purpose).
 *
 * For multidimensional datasets, papszOptions can contain array creation
 * options, if they are prefixed with "ARRAY:". \see GDALGroup::CopyFrom()
 * documentation for further details regarding such options.
 *
 * @param pszFilename the name for the new dataset.  UTF-8 encoded.
 * @param poSrcDS the dataset being duplicated.
 * @param bStrict TRUE if the copy must be strictly equivalent, or more
 * normally FALSE indicating that the copy may adapt as needed for the
 * output format.
 * @param papszOptions additional format dependent options controlling
 * creation of the output file.
 * The APPEND_SUBDATASET=YES option can be specified to avoid prior destruction
 * of existing dataset.
 * Starting with GDAL 3.8.0, the following options are recognized by the
 * GTiff, COG, VRT, PNG au JPEG drivers:
 * <ul>
 * <li>COPY_SRC_MDD=AUTO/YES/NO: whether metadata domains of the source dataset
 * should be copied to the destination dataset. In the default AUTO mode, only
 * "safe" domains will be copied, which include the default metadata domain
 * (some drivers may include other domains such as IMD, RPC, GEOLOCATION). When
 * setting YES, all domains will be copied (but a few reserved ones like
 * IMAGE_STRUCTURE or DERIVED_SUBDATASETS). When setting NO, no source metadata
 * will be copied.
 * </li>
 *<li>SRC_MDD=domain_name: which source metadata domain should be copied.
 * This option restricts the list of source metadata domains to be copied
 * (it implies COPY_SRC_MDD=YES if it is not set). This option may be specified
 * as many times as they are source domains. The default metadata domain is the
 * empty string "" ("_DEFAULT_") may also be used when empty string is not practical)
 * </li>
 * </ul>
 * @param pfnProgress a function to be used to report progress of the copy.
 * @param pProgressData application data passed into progress function.
 *
 * @return a pointer to the newly created dataset (may be read-only access).
 */

GDALDataset *GDALDriver::CreateCopy(const char *pszFilename,
                                    GDALDataset *poSrcDS, int bStrict,
                                    CSLConstList papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void *pProgressData)

{
    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    const int nBandCount = poSrcDS->GetRasterCount();

    /* -------------------------------------------------------------------- */
    /*      If no INTERLEAVE creation option is given, we will try to add   */
    /*      one that matches the current srcDS interleaving                 */
    /* -------------------------------------------------------------------- */
    char **papszOptionsToDelete = nullptr;
    const char *srcInterleave =
        poSrcDS->GetMetadataItem("INTERLEAVE", "IMAGE_STRUCTURE");
    if (nBandCount > 1 && srcInterleave != nullptr &&
        CSLFetchNameValue(papszOptions, "INTERLEAVE") == nullptr &&
        EQUAL(CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE"), "NONE"))
    {

        // look for INTERLEAVE values of the driver
        char **interleavesCSL = nullptr;
        const char *pszOptionList =
            this->GetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST);
        CPLXMLNode *xmlNode =
            !pszOptionList ? nullptr : CPLParseXMLString(pszOptionList);
        for (CPLXMLNode *child = !xmlNode ? nullptr : xmlNode->psChild;
             child != nullptr; child = child->psNext)
        {
            if ((child->eType == CXT_Element) &&
                EQUAL(child->pszValue, "Option"))
            {
                const char *nameAttribute =
                    CPLGetXMLValue(child, "name", nullptr);
                const bool isInterleaveAttribute =
                    nameAttribute && EQUAL(nameAttribute, "INTERLEAVE");
                if (isInterleaveAttribute)
                {
                    for (CPLXMLNode *optionChild = child->psChild;
                         optionChild != nullptr;
                         optionChild = optionChild->psNext)
                    {
                        if ((optionChild->eType == CXT_Element) &&
                            EQUAL(optionChild->pszValue, "Value"))
                        {
                            CPLXMLNode *optionChildValue = optionChild->psChild;
                            if (optionChildValue &&
                                (optionChildValue->eType == CXT_Text))
                            {
                                interleavesCSL = CSLAddString(
                                    interleavesCSL, optionChildValue->pszValue);
                            }
                        }
                    }
                }
            }
        }
        CPLDestroyXMLNode(xmlNode);

        const char *dstInterleaveBand =
            (CSLFindString(interleavesCSL, "BAND") >= 0)  ? "BAND"
            : (CSLFindString(interleavesCSL, "BSQ") >= 0) ? "BSQ"
                                                          : nullptr;
        const char *dstInterleaveLine =
            (CSLFindString(interleavesCSL, "LINE") >= 0)  ? "LINE"
            : (CSLFindString(interleavesCSL, "BIL") >= 0) ? "BIL"
                                                          : nullptr;
        const char *dstInterleavePixel =
            (CSLFindString(interleavesCSL, "PIXEL") >= 0) ? "PIXEL"
            : (CSLFindString(interleavesCSL, "BIP") >= 0) ? "BIP"
                                                          : nullptr;
        const char *dstInterleave =
            EQUAL(srcInterleave, "BAND")    ? dstInterleaveBand
            : EQUAL(srcInterleave, "LINE")  ? dstInterleaveLine
            : EQUAL(srcInterleave, "PIXEL") ? dstInterleavePixel
                                            : nullptr;
        CSLDestroy(interleavesCSL);

        if (dstInterleave != nullptr)
        {
            papszOptionsToDelete = CSLDuplicate(papszOptions);
            papszOptionsToDelete = CSLSetNameValue(papszOptionsToDelete,
                                                   "INTERLEAVE", dstInterleave);
            papszOptionsToDelete = CSLSetNameValue(
                papszOptionsToDelete, "@INTERLEAVE_ADDED_AUTOMATICALLY", "YES");
            papszOptions = papszOptionsToDelete;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Make sure we cleanup if there is an existing dataset of this    */
    /*      name.  But even if that seems to fail we will continue since    */
    /*      it might just be a corrupt file or something.                   */
    /* -------------------------------------------------------------------- */
    const bool bAppendSubdataset =
        CPLFetchBool(papszOptions, "APPEND_SUBDATASET", false);
    // Note: @QUIET_DELETE_ON_CREATE_COPY is set to NO by the KMLSuperOverlay
    // driver when writing a .kmz file. Also by GDALTranslate() if it has
    // already done a similar job.
    if (!bAppendSubdataset &&
        CPLFetchBool(papszOptions, "@QUIET_DELETE_ON_CREATE_COPY", true))
    {
        QuietDeleteForCreateCopy(pszFilename, poSrcDS);
    }

    int iIdxQuietDeleteOnCreateCopy =
        CSLPartialFindString(papszOptions, "@QUIET_DELETE_ON_CREATE_COPY=");
    if (iIdxQuietDeleteOnCreateCopy >= 0)
    {
        if (papszOptionsToDelete == nullptr)
            papszOptionsToDelete = CSLDuplicate(papszOptions);
        papszOptionsToDelete = CSLRemoveStrings(
            papszOptionsToDelete, iIdxQuietDeleteOnCreateCopy, 1, nullptr);
        papszOptions = papszOptionsToDelete;
    }

    /* -------------------------------------------------------------------- */
    /*      If _INTERNAL_DATASET=YES, the returned dataset will not be      */
    /*      registered in the global list of open datasets.                 */
    /* -------------------------------------------------------------------- */
    const int iIdxInternalDataset =
        CSLPartialFindString(papszOptions, "_INTERNAL_DATASET=");
    bool bInternalDataset = false;
    if (iIdxInternalDataset >= 0)
    {
        bInternalDataset =
            CPLFetchBool(papszOptions, "_INTERNAL_DATASET", false);
        if (papszOptionsToDelete == nullptr)
            papszOptionsToDelete = CSLDuplicate(papszOptions);
        papszOptionsToDelete = CSLRemoveStrings(
            papszOptionsToDelete, iIdxInternalDataset, 1, nullptr);
        papszOptions = papszOptionsToDelete;
    }

    /* -------------------------------------------------------------------- */
    /*      Validate creation options.                                      */
    /* -------------------------------------------------------------------- */
    if (CPLTestBool(
            CPLGetConfigOption("GDAL_VALIDATE_CREATION_OPTIONS", "YES")))
    {
        auto poSrcGroup = poSrcDS->GetRootGroup();
        if (poSrcGroup != nullptr && GetMetadataItem(GDAL_DCAP_MULTIDIM_RASTER))
        {
            CPLStringList aosDatasetCO;
            for (const char *pszOption : cpl::Iterate(papszOptions))
            {
                if (!STARTS_WITH_CI(pszOption, "ARRAY:"))
                    aosDatasetCO.AddString(pszOption);
            }
            GDALValidateCreationOptions(this, aosDatasetCO.List());
        }
        else
        {
            GDALValidateCreationOptions(this, papszOptions);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Advise the source raster that we are going to read it completely */
    /* -------------------------------------------------------------------- */

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    GDALDataType eDT = GDT_Unknown;
    if (nBandCount > 0)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(1);
        if (poSrcBand)
            eDT = poSrcBand->GetRasterDataType();
    }
    poSrcDS->AdviseRead(0, 0, nXSize, nYSize, nXSize, nYSize, eDT, nBandCount,
                        nullptr, nullptr);

    /* -------------------------------------------------------------------- */
    /*      If the format provides a CreateCopy() method use that,          */
    /*      otherwise fallback to the internal implementation using the     */
    /*      Create() method.                                                */
    /* -------------------------------------------------------------------- */
    GDALDataset *poDstDS = nullptr;
    auto l_pfnCreateCopy = GetCreateCopyCallback();
    if (l_pfnCreateCopy != nullptr &&
        !CPLTestBool(CPLGetConfigOption("GDAL_DEFAULT_CREATE_COPY", "NO")))
    {
        poDstDS = l_pfnCreateCopy(pszFilename, poSrcDS, bStrict,
                                  const_cast<char **>(papszOptions),
                                  pfnProgress, pProgressData);
        if (poDstDS != nullptr)
        {
            if (poDstDS->GetDescription() == nullptr ||
                strlen(poDstDS->GetDescription()) == 0)
                poDstDS->SetDescription(pszFilename);

            if (poDstDS->poDriver == nullptr)
                poDstDS->poDriver = this;

            if (!bInternalDataset)
                poDstDS->AddToDatasetOpenList();
        }
    }
    else
    {
        poDstDS = DefaultCreateCopy(pszFilename, poSrcDS, bStrict, papszOptions,
                                    pfnProgress, pProgressData);
    }

    CSLDestroy(papszOptionsToDelete);
    return poDstDS;
}

/************************************************************************/
/*                           GDALCreateCopy()                           */
/************************************************************************/

/**
 * \brief Create a copy of a dataset.
 *
 * @see GDALDriver::CreateCopy()
 */

GDALDatasetH CPL_STDCALL GDALCreateCopy(GDALDriverH hDriver,
                                        const char *pszFilename,
                                        GDALDatasetH hSrcDS, int bStrict,
                                        CSLConstList papszOptions,
                                        GDALProgressFunc pfnProgress,
                                        void *pProgressData)

{
    VALIDATE_POINTER1(hDriver, "GDALCreateCopy", nullptr);
    VALIDATE_POINTER1(hSrcDS, "GDALCreateCopy", nullptr);

    return GDALDriver::FromHandle(hDriver)->CreateCopy(
        pszFilename, GDALDataset::FromHandle(hSrcDS), bStrict, papszOptions,
        pfnProgress, pProgressData);
}

/************************************************************************/
/*                      CanVectorTranslateFrom()                        */
/************************************************************************/

/** Returns whether the driver can translate from a vector dataset,
 * using the arguments passed to GDALVectorTranslate() stored in
 * papszVectorTranslateArguments.
 *
 * This is used to determine if the driver supports the VectorTranslateFrom()
 * operation.
 *
 * @param pszDestName Target dataset name
 * @param poSourceDS  Source dataset
 * @param papszVectorTranslateArguments Non-positional arguments passed to
 *                                      GDALVectorTranslate() (may be nullptr)
 * @param[out] ppapszFailureReasons nullptr, or a pointer to an null-terminated
 * array of strings to record the reason(s) for the impossibility.
 * @return true if VectorTranslateFrom() can be called with the same arguments.
 * @since GDAL 3.8
 */
bool GDALDriver::CanVectorTranslateFrom(
    const char *pszDestName, GDALDataset *poSourceDS,
    CSLConstList papszVectorTranslateArguments, char ***ppapszFailureReasons)

{
    if (ppapszFailureReasons)
    {
        *ppapszFailureReasons = nullptr;
    }

    if (!pfnCanVectorTranslateFrom)
    {
        if (ppapszFailureReasons)
        {
            *ppapszFailureReasons = CSLAddString(
                nullptr,
                "CanVectorTranslateFrom() not implemented for this driver");
        }
        return false;
    }

    char **papszFailureReasons = nullptr;
    bool bRet = pfnCanVectorTranslateFrom(
        pszDestName, poSourceDS, papszVectorTranslateArguments,
        ppapszFailureReasons ? ppapszFailureReasons : &papszFailureReasons);
    if (!ppapszFailureReasons)
    {
        for (const char *pszReason :
             cpl::Iterate(CSLConstList(papszFailureReasons)))
        {
            CPLDebug("GDAL", "%s", pszReason);
        }
        CSLDestroy(papszFailureReasons);
    }
    return bRet;
}

/************************************************************************/
/*                         VectorTranslateFrom()                        */
/************************************************************************/

/** Create a copy of a vector dataset, using the arguments passed to
 * GDALVectorTranslate() stored in papszVectorTranslateArguments.
 *
 * This may be implemented by some drivers that can convert from an existing
 * dataset in an optimized way.
 *
 * This is for example used by the PMTiles to convert from MBTiles.
 *
 * @param pszDestName Target dataset name
 * @param poSourceDS  Source dataset
 * @param papszVectorTranslateArguments Non-positional arguments passed to
 *                                      GDALVectorTranslate() (may be nullptr)
 * @param pfnProgress a function to be used to report progress of the copy.
 * @param pProgressData application data passed into progress function.
 * @return a new dataset in case of success, or nullptr in case of error.
 * @since GDAL 3.8
 */
GDALDataset *GDALDriver::VectorTranslateFrom(
    const char *pszDestName, GDALDataset *poSourceDS,
    CSLConstList papszVectorTranslateArguments, GDALProgressFunc pfnProgress,
    void *pProgressData)

{
    if (!pfnVectorTranslateFrom)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "VectorTranslateFrom() not implemented for this driver");
        return nullptr;
    }

    return pfnVectorTranslateFrom(pszDestName, poSourceDS,
                                  papszVectorTranslateArguments, pfnProgress,
                                  pProgressData);
}

/************************************************************************/
/*                            QuietDelete()                             */
/************************************************************************/

/**
 * \brief Delete dataset if found.
 *
 * This is a helper method primarily used by Create() and
 * CreateCopy() to predelete any dataset of the name soon to be
 * created.  It will attempt to delete the named dataset if
 * one is found, otherwise it does nothing.  An error is only
 * returned if the dataset is found but the delete fails.
 *
 * This is a static method and it doesn't matter what driver instance
 * it is invoked on.  It will attempt to discover the correct driver
 * using Identify().
 *
 * @param pszName the dataset name to try and delete.
 * @param papszAllowedDrivers NULL to consider all candidate drivers, or a NULL
 * terminated list of strings with the driver short names that must be
 * considered. (Note: implemented only starting with GDAL 3.4.1)
 * @return CE_None if the dataset does not exist, or is deleted without issues.
 */

CPLErr GDALDriver::QuietDelete(const char *pszName,
                               CSLConstList papszAllowedDrivers)

{
    VSIStatBufL sStat;
    const bool bExists =
        VSIStatExL(pszName, &sStat,
                   VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG) == 0;

#ifdef S_ISFIFO
    if (bExists && S_ISFIFO(sStat.st_mode))
        return CE_None;
#endif

    if (bExists && VSI_ISDIR(sStat.st_mode))
    {
        // It is not desirable to remove directories quietly.  Necessary to
        // avoid ogr_mitab_12 to destroy file created at ogr_mitab_7.
        return CE_None;
    }

    GDALDriver *poDriver = nullptr;
    if (papszAllowedDrivers)
    {
        GDALOpenInfo oOpenInfo(pszName, GDAL_OF_ALL);
        for (const char *pszDriverName : cpl::Iterate(papszAllowedDrivers))
        {
            GDALDriver *poTmpDriver =
                GDALDriver::FromHandle(GDALGetDriverByName(pszDriverName));
            if (poTmpDriver)
            {
                const bool bIdentifyRes =
                    poTmpDriver->pfnIdentifyEx
                        ? poTmpDriver->pfnIdentifyEx(poTmpDriver, &oOpenInfo) >
                              0
                        : poTmpDriver->pfnIdentify &&
                              poTmpDriver->pfnIdentify(&oOpenInfo) > 0;
                if (bIdentifyRes)
                {
                    poDriver = poTmpDriver;
                    break;
                }
            }
        }
    }
    else
    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
        poDriver = GDALDriver::FromHandle(GDALIdentifyDriver(pszName, nullptr));
    }

    if (poDriver == nullptr)
        return CE_None;

    CPLDebug("GDAL", "QuietDelete(%s) invoking Delete()", pszName);

    poDriver->pfnDelete = poDriver->GetDeleteCallback();
    const bool bQuiet = !bExists && poDriver->pfnDelete == nullptr &&
                        poDriver->pfnDeleteDataSource == nullptr;
    if (bQuiet)
    {
        CPLErrorStateBackuper oErrorStateBackuper(CPLQuietErrorHandler);
        return poDriver->Delete(pszName);
    }
    else
    {
        return poDriver->Delete(pszName);
    }
}

/************************************************************************/
/*                               Delete()                               */
/************************************************************************/

/**
 * \brief Delete named dataset.
 *
 * The driver will attempt to delete the named dataset in a driver specific
 * fashion.  Full featured drivers will delete all associated files,
 * database objects, or whatever is appropriate.  The default behavior when
 * no driver specific behavior is provided is to attempt to delete all the
 * files that are returned by GDALGetFileList() on the dataset handle.
 *
 * It is unwise to have open dataset handles on this dataset when it is
 * deleted.
 *
 * Equivalent of the C function GDALDeleteDataset().
 *
 * @param pszFilename name of dataset to delete.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::Delete(const char *pszFilename)

{
    pfnDelete = GetDeleteCallback();
    if (pfnDelete != nullptr)
        return pfnDelete(pszFilename);
    else if (pfnDeleteDataSource != nullptr)
        return pfnDeleteDataSource(this, pszFilename);

    /* -------------------------------------------------------------------- */
    /*      Collect file list.                                              */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDS = GDALOpenEx(pszFilename, 0, nullptr, nullptr, nullptr);

    if (hDS == nullptr)
    {
        if (CPLGetLastErrorNo() == 0)
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Unable to open %s to obtain file list.", pszFilename);

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList(hDS);

    GDALClose(hDS);
    hDS = nullptr;

    if (CSLCount(papszFileList) == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unable to determine files associated with %s, "
                 "delete fails.",
                 pszFilename);
        CSLDestroy(papszFileList);
        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Delete all files.                                               */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    for (int i = 0; papszFileList[i] != nullptr; ++i)
    {
        if (VSIUnlink(papszFileList[i]) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Deleting %s failed:\n%s",
                     papszFileList[i], VSIStrerror(errno));
            eErr = CE_Failure;
        }
    }

    CSLDestroy(papszFileList);

    return eErr;
}

/************************************************************************/
/*                         GDALDeleteDataset()                          */
/************************************************************************/

/**
 * \brief Delete named dataset.
 *
 * @see GDALDriver::Delete()
 */

CPLErr CPL_STDCALL GDALDeleteDataset(GDALDriverH hDriver,
                                     const char *pszFilename)

{
    if (hDriver == nullptr)
        hDriver = GDALIdentifyDriver(pszFilename, nullptr);

    if (hDriver == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No identifiable driver for %s.",
                 pszFilename);
        return CE_Failure;
    }

    return GDALDriver::FromHandle(hDriver)->Delete(pszFilename);
}

/************************************************************************/
/*                           DefaultRename()                            */
/*                                                                      */
/*      The generic implementation based on the file list used when     */
/*      there is no format specific implementation.                     */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDriver::DefaultRename(const char *pszNewName, const char *pszOldName)

{
    /* -------------------------------------------------------------------- */
    /*      Collect file list.                                              */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDS = GDALOpen(pszOldName, GA_ReadOnly);

    if (hDS == nullptr)
    {
        if (CPLGetLastErrorNo() == 0)
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Unable to open %s to obtain file list.", pszOldName);

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList(hDS);

    GDALClose(hDS);

    if (CSLCount(papszFileList) == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unable to determine files associated with %s,\n"
                 "rename fails.",
                 pszOldName);

        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Produce a list of new filenames that correspond to the old      */
    /*      names.                                                          */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    char **papszNewFileList =
        CPLCorrespondingPaths(pszOldName, pszNewName, papszFileList);

    if (papszNewFileList == nullptr)
        return CE_Failure;

    for (int i = 0; papszFileList[i] != nullptr; ++i)
    {
        if (CPLMoveFile(papszNewFileList[i], papszFileList[i]) != 0)
        {
            eErr = CE_Failure;
            // Try to put the ones we moved back.
            for (--i; i >= 0; i--)
            {
                // Nothing we can do if the moving back doesn't work...
                CPL_IGNORE_RET_VAL(
                    CPLMoveFile(papszFileList[i], papszNewFileList[i]));
            }
            break;
        }
    }

    CSLDestroy(papszNewFileList);
    CSLDestroy(papszFileList);

    return eErr;
}

//! @endcond

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

/**
 * \brief Rename a dataset.
 *
 * Rename a dataset. This may including moving the dataset to a new directory
 * or even a new filesystem.
 *
 * It is unwise to have open dataset handles on this dataset when it is
 * being renamed.
 *
 * Equivalent of the C function GDALRenameDataset().
 *
 * @param pszNewName new name for the dataset.
 * @param pszOldName old name for the dataset.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::Rename(const char *pszNewName, const char *pszOldName)

{
    pfnRename = GetRenameCallback();
    if (pfnRename != nullptr)
        return pfnRename(pszNewName, pszOldName);

    return DefaultRename(pszNewName, pszOldName);
}

/************************************************************************/
/*                         GDALRenameDataset()                          */
/************************************************************************/

/**
 * \brief Rename a dataset.
 *
 * @see GDALDriver::Rename()
 */

CPLErr CPL_STDCALL GDALRenameDataset(GDALDriverH hDriver,
                                     const char *pszNewName,
                                     const char *pszOldName)

{
    if (hDriver == nullptr)
        hDriver = GDALIdentifyDriver(pszOldName, nullptr);

    if (hDriver == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No identifiable driver for %s.",
                 pszOldName);
        return CE_Failure;
    }

    return GDALDriver::FromHandle(hDriver)->Rename(pszNewName, pszOldName);
}

/************************************************************************/
/*                          DefaultCopyFiles()                          */
/*                                                                      */
/*      The default implementation based on file lists used when        */
/*      there is no format specific implementation.                     */
/************************************************************************/

//! @cond Doxygen_Suppress
CPLErr GDALDriver::DefaultCopyFiles(const char *pszNewName,
                                    const char *pszOldName)

{
    /* -------------------------------------------------------------------- */
    /*      Collect file list.                                              */
    /* -------------------------------------------------------------------- */
    GDALDatasetH hDS = GDALOpen(pszOldName, GA_ReadOnly);

    if (hDS == nullptr)
    {
        if (CPLGetLastErrorNo() == 0)
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Unable to open %s to obtain file list.", pszOldName);

        return CE_Failure;
    }

    char **papszFileList = GDALGetFileList(hDS);

    GDALClose(hDS);
    hDS = nullptr;

    if (CSLCount(papszFileList) == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unable to determine files associated with %s,\n"
                 "rename fails.",
                 pszOldName);

        return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Produce a list of new filenames that correspond to the old      */
    /*      names.                                                          */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = CE_None;
    char **papszNewFileList =
        CPLCorrespondingPaths(pszOldName, pszNewName, papszFileList);

    if (papszNewFileList == nullptr)
        return CE_Failure;

    for (int i = 0; papszFileList[i] != nullptr; ++i)
    {
        if (CPLCopyFile(papszNewFileList[i], papszFileList[i]) != 0)
        {
            eErr = CE_Failure;
            // Try to put the ones we moved back.
            for (--i; i >= 0; --i)
                VSIUnlink(papszNewFileList[i]);
            break;
        }
    }

    CSLDestroy(papszNewFileList);
    CSLDestroy(papszFileList);

    return eErr;
}

//! @endcond

/************************************************************************/
/*                             CopyFiles()                              */
/************************************************************************/

/**
 * \brief Copy the files of a dataset.
 *
 * Copy all the files associated with a dataset.
 *
 * Equivalent of the C function GDALCopyDatasetFiles().
 *
 * @param pszNewName new name for the dataset.
 * @param pszOldName old name for the dataset.
 *
 * @return CE_None on success, or CE_Failure if the operation fails.
 */

CPLErr GDALDriver::CopyFiles(const char *pszNewName, const char *pszOldName)

{
    pfnCopyFiles = GetCopyFilesCallback();
    if (pfnCopyFiles != nullptr)
        return pfnCopyFiles(pszNewName, pszOldName);

    return DefaultCopyFiles(pszNewName, pszOldName);
}

/************************************************************************/
/*                        GDALCopyDatasetFiles()                        */
/************************************************************************/

/**
 * \brief Copy the files of a dataset.
 *
 * @see GDALDriver::CopyFiles()
 */

CPLErr CPL_STDCALL GDALCopyDatasetFiles(GDALDriverH hDriver,
                                        const char *pszNewName,
                                        const char *pszOldName)

{
    if (hDriver == nullptr)
        hDriver = GDALIdentifyDriver(pszOldName, nullptr);

    if (hDriver == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No identifiable driver for %s.",
                 pszOldName);
        return CE_Failure;
    }

    return GDALDriver::FromHandle(hDriver)->CopyFiles(pszNewName, pszOldName);
}

/************************************************************************/
/*                       GDALGetDriverShortName()                       */
/************************************************************************/

/**
 * \brief Return the short name of a driver
 *
 * This is the string that can be
 * passed to the GDALGetDriverByName() function.
 *
 * For the GeoTIFF driver, this is "GTiff"
 *
 * @param hDriver the handle of the driver
 * @return the short name of the driver. The
 *         returned string should not be freed and is owned by the driver.
 */

const char *CPL_STDCALL GDALGetDriverShortName(GDALDriverH hDriver)

{
    VALIDATE_POINTER1(hDriver, "GDALGetDriverShortName", nullptr);

    return GDALDriver::FromHandle(hDriver)->GetDescription();
}

/************************************************************************/
/*                       GDALGetDriverLongName()                        */
/************************************************************************/

/**
 * \brief Return the long name of a driver
 *
 * For the GeoTIFF driver, this is "GeoTIFF"
 *
 * @param hDriver the handle of the driver
 * @return the long name of the driver or empty string. The
 *         returned string should not be freed and is owned by the driver.
 */

const char *CPL_STDCALL GDALGetDriverLongName(GDALDriverH hDriver)

{
    VALIDATE_POINTER1(hDriver, "GDALGetDriverLongName", nullptr);

    const char *pszLongName =
        GDALDriver::FromHandle(hDriver)->GetMetadataItem(GDAL_DMD_LONGNAME);

    if (pszLongName == nullptr)
        return "";

    return pszLongName;
}

/************************************************************************/
/*                       GDALGetDriverHelpTopic()                       */
/************************************************************************/

/**
 * \brief Return the URL to the help that describes the driver
 *
 * That URL is relative to the GDAL documentation directory.
 *
 * For the GeoTIFF driver, this is "frmt_gtiff.html"
 *
 * @param hDriver the handle of the driver
 * @return the URL to the help that describes the driver or NULL. The
 *         returned string should not be freed and is owned by the driver.
 */

const char *CPL_STDCALL GDALGetDriverHelpTopic(GDALDriverH hDriver)

{
    VALIDATE_POINTER1(hDriver, "GDALGetDriverHelpTopic", nullptr);

    return GDALDriver::FromHandle(hDriver)->GetMetadataItem(GDAL_DMD_HELPTOPIC);
}

/************************************************************************/
/*                   GDALGetDriverCreationOptionList()                  */
/************************************************************************/

/**
 * \brief Return the list of creation options of the driver
 *
 * Return the list of creation options of the driver used by Create() and
 * CreateCopy() as an XML string
 *
 * @param hDriver the handle of the driver
 * @return an XML string that describes the list of creation options or
 *         empty string. The returned string should not be freed and is
 *         owned by the driver.
 */

const char *CPL_STDCALL GDALGetDriverCreationOptionList(GDALDriverH hDriver)

{
    VALIDATE_POINTER1(hDriver, "GDALGetDriverCreationOptionList", nullptr);

    const char *pszOptionList =
        GDALDriver::FromHandle(hDriver)->GetMetadataItem(
            GDAL_DMD_CREATIONOPTIONLIST);

    if (pszOptionList == nullptr)
        return "";

    return pszOptionList;
}

/************************************************************************/
/*                   GDALValidateCreationOptions()                      */
/************************************************************************/

/**
 * \brief Validate the list of creation options that are handled by a driver
 *
 * This is a helper method primarily used by Create() and
 * CreateCopy() to validate that the passed in list of creation options
 * is compatible with the GDAL_DMD_CREATIONOPTIONLIST metadata item defined
 * by some drivers. @see GDALGetDriverCreationOptionList()
 *
 * If the GDAL_DMD_CREATIONOPTIONLIST metadata item is not defined, this
 * function will return TRUE. Otherwise it will check that the keys and values
 * in the list of creation options are compatible with the capabilities declared
 * by the GDAL_DMD_CREATIONOPTIONLIST metadata item. In case of incompatibility
 * a (non fatal) warning will be emitted and FALSE will be returned.
 *
 * @param hDriver the handle of the driver with whom the lists of creation
 * option must be validated
 * @param papszCreationOptions the list of creation options. An array of
 * strings, whose last element is a NULL pointer
 * @return TRUE if the list of creation options is compatible with the Create()
 *         and CreateCopy() method of the driver, FALSE otherwise.
 */

int CPL_STDCALL GDALValidateCreationOptions(GDALDriverH hDriver,
                                            CSLConstList papszCreationOptions)
{
    VALIDATE_POINTER1(hDriver, "GDALValidateCreationOptions", FALSE);
    const char *pszOptionList =
        GDALDriver::FromHandle(hDriver)->GetMetadataItem(
            GDAL_DMD_CREATIONOPTIONLIST);
    CPLString osDriver;
    osDriver.Printf("driver %s",
                    GDALDriver::FromHandle(hDriver)->GetDescription());
    bool bFoundOptionToRemove = false;
    for (const char *pszCO : cpl::Iterate(papszCreationOptions))
    {
        for (const char *pszExcludedOptions :
             {"APPEND_SUBDATASET", "COPY_SRC_MDD", "SRC_MDD"})
        {
            if (STARTS_WITH_CI(pszCO, pszExcludedOptions) &&
                pszCO[strlen(pszExcludedOptions)] == '=')
            {
                bFoundOptionToRemove = true;
                break;
            }
        }
        if (bFoundOptionToRemove)
            break;
    }
    CSLConstList papszOptionsToValidate = papszCreationOptions;
    char **papszOptionsToFree = nullptr;
    if (bFoundOptionToRemove)
    {
        for (const char *pszCO : cpl::Iterate(papszCreationOptions))
        {
            bool bMatch = false;
            for (const char *pszExcludedOptions :
                 {"APPEND_SUBDATASET", "COPY_SRC_MDD", "SRC_MDD"})
            {
                if (STARTS_WITH_CI(pszCO, pszExcludedOptions) &&
                    pszCO[strlen(pszExcludedOptions)] == '=')
                {
                    bMatch = true;
                    break;
                }
            }
            if (!bMatch)
                papszOptionsToFree = CSLAddString(papszOptionsToFree, pszCO);
        }
        papszOptionsToValidate = papszOptionsToFree;
    }

    const bool bRet = CPL_TO_BOOL(GDALValidateOptions(
        pszOptionList, papszOptionsToValidate, "creation option", osDriver));
    CSLDestroy(papszOptionsToFree);
    return bRet;
}

/************************************************************************/
/*                     GDALValidateOpenOptions()                        */
/************************************************************************/

int GDALValidateOpenOptions(GDALDriverH hDriver,
                            const char *const *papszOpenOptions)
{
    VALIDATE_POINTER1(hDriver, "GDALValidateOpenOptions", FALSE);
    const char *pszOptionList =
        GDALDriver::FromHandle(hDriver)->GetMetadataItem(
            GDAL_DMD_OPENOPTIONLIST);
    CPLString osDriver;
    osDriver.Printf("driver %s",
                    GDALDriver::FromHandle(hDriver)->GetDescription());
    return GDALValidateOptions(pszOptionList, papszOpenOptions, "open option",
                               osDriver);
}

/************************************************************************/
/*                           GDALValidateOptions()                      */
/************************************************************************/

int GDALValidateOptions(const char *pszOptionList,
                        const char *const *papszOptionsToValidate,
                        const char *pszErrorMessageOptionType,
                        const char *pszErrorMessageContainerName)
{
    if (papszOptionsToValidate == nullptr || *papszOptionsToValidate == nullptr)
        return TRUE;
    if (pszOptionList == nullptr)
        return TRUE;

    CPLXMLNode *psNode = CPLParseXMLString(pszOptionList);
    if (psNode == nullptr)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Could not parse %s list of %s. Assuming options are valid.",
                 pszErrorMessageOptionType, pszErrorMessageContainerName);
        return TRUE;
    }

    bool bRet = true;
    while (*papszOptionsToValidate)
    {
        char *pszKey = nullptr;
        const char *pszValue =
            CPLParseNameValue(*papszOptionsToValidate, &pszKey);
        if (pszKey == nullptr)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "%s '%s' is not formatted with the key=value format",
                     pszErrorMessageOptionType, *papszOptionsToValidate);
            bRet = false;

            ++papszOptionsToValidate;
            continue;
        }

        if (EQUAL(pszKey, "VALIDATE_OPEN_OPTIONS"))
        {
            ++papszOptionsToValidate;
            CPLFree(pszKey);
            continue;
        }

        // Must we be forgiving in case of missing option ?
        bool bWarnIfMissingKey = true;
        if (pszKey[0] == '@')
        {
            bWarnIfMissingKey = false;
            memmove(pszKey, pszKey + 1, strlen(pszKey + 1) + 1);
        }

        CPLXMLNode *psChildNode = psNode->psChild;
        while (psChildNode)
        {
            if (EQUAL(psChildNode->pszValue, "OPTION"))
            {
                const char *pszOptionName =
                    CPLGetXMLValue(psChildNode, "name", "");
                /* For option names terminated by wildcard (NITF BLOCKA option
                 * names for example) */
                if (strlen(pszOptionName) > 0 &&
                    pszOptionName[strlen(pszOptionName) - 1] == '*' &&
                    EQUALN(pszOptionName, pszKey, strlen(pszOptionName) - 1))
                {
                    break;
                }

                /* For option names beginning by a wildcard */
                if (pszOptionName[0] == '*' &&
                    strlen(pszKey) > strlen(pszOptionName) &&
                    EQUAL(pszKey + strlen(pszKey) - strlen(pszOptionName + 1),
                          pszOptionName + 1))
                {
                    break;
                }

                // For options names with * in the middle
                const char *pszStarInOptionName = strchr(pszOptionName, '*');
                if (pszStarInOptionName &&
                    pszStarInOptionName != pszOptionName &&
                    pszStarInOptionName !=
                        pszOptionName + strlen(pszOptionName) - 1 &&
                    strlen(pszKey) > static_cast<size_t>(pszStarInOptionName -
                                                         pszOptionName) &&
                    EQUALN(pszKey, pszOptionName,
                           static_cast<size_t>(pszStarInOptionName -
                                               pszOptionName)) &&
                    EQUAL(pszKey +
                              static_cast<size_t>(pszStarInOptionName -
                                                  pszOptionName) +
                              1,
                          pszStarInOptionName + 1))
                {
                    break;
                }

                if (EQUAL(pszOptionName, pszKey))
                {
                    break;
                }
                const char *pszAlias = CPLGetXMLValue(
                    psChildNode, "alias",
                    CPLGetXMLValue(psChildNode, "deprecated_alias", ""));
                if (EQUAL(pszAlias, pszKey))
                {
                    CPLDebug("GDAL",
                             "Using deprecated alias '%s'. New name is '%s'",
                             pszAlias, pszOptionName);
                    break;
                }
            }
            psChildNode = psChildNode->psNext;
        }
        if (psChildNode == nullptr)
        {
            if (bWarnIfMissingKey &&
                (!EQUAL(pszErrorMessageOptionType, "open option") ||
                 CPLFetchBool(papszOptionsToValidate, "VALIDATE_OPEN_OPTIONS",
                              true)))
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "%s does not support %s %s",
                         pszErrorMessageContainerName,
                         pszErrorMessageOptionType, pszKey);
                bRet = false;
            }

            CPLFree(pszKey);
            ++papszOptionsToValidate;
            continue;
        }

#ifdef DEBUG
        CPLXMLNode *psChildSubNode = psChildNode->psChild;
        while (psChildSubNode)
        {
            if (psChildSubNode->eType == CXT_Attribute)
            {
                if (!(EQUAL(psChildSubNode->pszValue, "name") ||
                      EQUAL(psChildSubNode->pszValue, "alias") ||
                      EQUAL(psChildSubNode->pszValue, "deprecated_alias") ||
                      EQUAL(psChildSubNode->pszValue, "alt_config_option") ||
                      EQUAL(psChildSubNode->pszValue, "description") ||
                      EQUAL(psChildSubNode->pszValue, "type") ||
                      EQUAL(psChildSubNode->pszValue, "min") ||
                      EQUAL(psChildSubNode->pszValue, "max") ||
                      EQUAL(psChildSubNode->pszValue, "default") ||
                      EQUAL(psChildSubNode->pszValue, "maxsize") ||
                      EQUAL(psChildSubNode->pszValue, "required") ||
                      EQUAL(psChildSubNode->pszValue, "scope")))
                {
                    /* Driver error */
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "%s : unhandled attribute '%s' for %s %s.",
                             pszErrorMessageContainerName,
                             psChildSubNode->pszValue, pszKey,
                             pszErrorMessageOptionType);
                }
            }
            psChildSubNode = psChildSubNode->psNext;
        }
#endif

        const char *pszType = CPLGetXMLValue(psChildNode, "type", nullptr);
        const char *pszMin = CPLGetXMLValue(psChildNode, "min", nullptr);
        const char *pszMax = CPLGetXMLValue(psChildNode, "max", nullptr);
        if (pszType != nullptr)
        {
            if (EQUAL(pszType, "INT") || EQUAL(pszType, "INTEGER"))
            {
                const char *pszValueIter = pszValue;
                while (*pszValueIter)
                {
                    if (!((*pszValueIter >= '0' && *pszValueIter <= '9') ||
                          *pszValueIter == '+' || *pszValueIter == '-'))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is an unexpected value for %s %s of "
                                 "type int.",
                                 pszValue, pszKey, pszErrorMessageOptionType);
                        bRet = false;
                        break;
                    }
                    ++pszValueIter;
                }
                if (*pszValueIter == '\0')
                {
                    if (pszMin && atoi(pszValue) < atoi(pszMin))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is an unexpected value for %s %s that "
                                 "should be >= %s.",
                                 pszValue, pszKey, pszErrorMessageOptionType,
                                 pszMin);
                        bRet = false;
                    }
                    if (pszMax && atoi(pszValue) > atoi(pszMax))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is an unexpected value for %s %s that "
                                 "should be <= %s.",
                                 pszValue, pszKey, pszErrorMessageOptionType,
                                 pszMax);
                        bRet = false;
                    }
                }
            }
            else if (EQUAL(pszType, "UNSIGNED INT"))
            {
                const char *pszValueIter = pszValue;
                while (*pszValueIter)
                {
                    if (!((*pszValueIter >= '0' && *pszValueIter <= '9') ||
                          *pszValueIter == '+'))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is an unexpected value for %s %s of "
                                 "type unsigned int.",
                                 pszValue, pszKey, pszErrorMessageOptionType);
                        bRet = false;
                        break;
                    }
                    ++pszValueIter;
                }
                if (*pszValueIter == '\0')
                {
                    if (pszMin && atoi(pszValue) < atoi(pszMin))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is an unexpected value for %s %s that "
                                 "should be >= %s.",
                                 pszValue, pszKey, pszErrorMessageOptionType,
                                 pszMin);
                        bRet = false;
                    }
                    if (pszMax && atoi(pszValue) > atoi(pszMax))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is an unexpected value for %s %s that "
                                 "should be <= %s.",
                                 pszValue, pszKey, pszErrorMessageOptionType,
                                 pszMax);
                        bRet = false;
                    }
                }
            }
            else if (EQUAL(pszType, "FLOAT"))
            {
                char *endPtr = nullptr;
                double dfVal = CPLStrtod(pszValue, &endPtr);
                if (!(endPtr == nullptr || *endPtr == '\0'))
                {
                    CPLError(
                        CE_Warning, CPLE_NotSupported,
                        "'%s' is an unexpected value for %s %s of type float.",
                        pszValue, pszKey, pszErrorMessageOptionType);
                    bRet = false;
                }
                else
                {
                    if (pszMin && dfVal < CPLAtof(pszMin))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is an unexpected value for %s %s that "
                                 "should be >= %s.",
                                 pszValue, pszKey, pszErrorMessageOptionType,
                                 pszMin);
                        bRet = false;
                    }
                    if (pszMax && dfVal > CPLAtof(pszMax))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is an unexpected value for %s %s that "
                                 "should be <= %s.",
                                 pszValue, pszKey, pszErrorMessageOptionType,
                                 pszMax);
                        bRet = false;
                    }
                }
            }
            else if (EQUAL(pszType, "BOOLEAN"))
            {
                if (!(EQUAL(pszValue, "ON") || EQUAL(pszValue, "TRUE") ||
                      EQUAL(pszValue, "YES") || EQUAL(pszValue, "OFF") ||
                      EQUAL(pszValue, "FALSE") || EQUAL(pszValue, "NO")))
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s of type "
                             "boolean.",
                             pszValue, pszKey, pszErrorMessageOptionType);
                    bRet = false;
                }
            }
            else if (EQUAL(pszType, "STRING-SELECT"))
            {
                bool bMatchFound = false;
                CPLXMLNode *psStringSelect = psChildNode->psChild;
                while (psStringSelect)
                {
                    if (psStringSelect->eType == CXT_Element &&
                        EQUAL(psStringSelect->pszValue, "Value"))
                    {
                        CPLXMLNode *psOptionNode = psStringSelect->psChild;
                        while (psOptionNode)
                        {
                            if (psOptionNode->eType == CXT_Text &&
                                EQUAL(psOptionNode->pszValue, pszValue))
                            {
                                bMatchFound = true;
                                break;
                            }
                            if (psOptionNode->eType == CXT_Attribute &&
                                (EQUAL(psOptionNode->pszValue, "alias") ||
                                 EQUAL(psOptionNode->pszValue,
                                       "deprecated_alias")) &&
                                EQUAL(psOptionNode->psChild->pszValue,
                                      pszValue))
                            {
                                bMatchFound = true;
                                break;
                            }
                            psOptionNode = psOptionNode->psNext;
                        }
                        if (bMatchFound)
                            break;
                    }
                    psStringSelect = psStringSelect->psNext;
                }
                if (!bMatchFound)
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "'%s' is an unexpected value for %s %s of type "
                             "string-select.",
                             pszValue, pszKey, pszErrorMessageOptionType);
                    bRet = false;
                }
            }
            else if (EQUAL(pszType, "STRING"))
            {
                const char *pszMaxSize =
                    CPLGetXMLValue(psChildNode, "maxsize", nullptr);
                if (pszMaxSize != nullptr)
                {
                    if (static_cast<int>(strlen(pszValue)) > atoi(pszMaxSize))
                    {
                        CPLError(CE_Warning, CPLE_NotSupported,
                                 "'%s' is of size %d, whereas maximum size for "
                                 "%s %s is %d.",
                                 pszValue, static_cast<int>(strlen(pszValue)),
                                 pszKey, pszErrorMessageOptionType,
                                 atoi(pszMaxSize));
                        bRet = false;
                    }
                }
            }
            else
            {
                /* Driver error */
                CPLError(CE_Warning, CPLE_NotSupported,
                         "%s : type '%s' for %s %s is not recognized.",
                         pszErrorMessageContainerName, pszType, pszKey,
                         pszErrorMessageOptionType);
            }
        }
        else
        {
            /* Driver error */
            CPLError(CE_Warning, CPLE_NotSupported, "%s : no type for %s %s.",
                     pszErrorMessageContainerName, pszKey,
                     pszErrorMessageOptionType);
        }
        CPLFree(pszKey);
        ++papszOptionsToValidate;
    }

    CPLDestroyXMLNode(psNode);
    return bRet ? TRUE : FALSE;
}

/************************************************************************/
/*                         GDALIdentifyDriver()                         */
/************************************************************************/

/**
 * \brief Identify the driver that can open a dataset.
 *
 * This function will try to identify the driver that can open the passed file
 * name by invoking the Identify method of each registered GDALDriver in turn.
 * The first driver that successfully identifies the file name will be returned.
 * If all drivers fail then NULL is returned.
 *
 * In order to reduce the need for such searches to touch the operating system
 * file system machinery, it is possible to give an optional list of files.
 * This is the list of all files at the same level in the file system as the
 * target file, including the target file. The filenames will not include any
 * path components, and are essentially just the output of VSIReadDir() on the
 * parent directory. If the target object does not have filesystem semantics
 * then the file list should be NULL.
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param papszFileList an array of strings, whose last element is the NULL
 * pointer.  These strings are filenames that are auxiliary to the main
 * filename. The passed value may be NULL.
 *
 * @return A GDALDriverH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDriver *.
 */

GDALDriverH CPL_STDCALL GDALIdentifyDriver(const char *pszFilename,
                                           CSLConstList papszFileList)

{
    return GDALIdentifyDriverEx(pszFilename, 0, nullptr, papszFileList);
}

/************************************************************************/
/*                         GDALIdentifyDriverEx()                       */
/************************************************************************/

/**
 * \brief Identify the driver that can open a dataset.
 *
 * This function will try to identify the driver that can open the passed file
 * name by invoking the Identify method of each registered GDALDriver in turn.
 * The first driver that successfully identifies the file name will be returned.
 * If all drivers fail then NULL is returned.
 *
 * In order to reduce the need for such searches to touch the operating system
 * file system machinery, it is possible to give an optional list of files.
 * This is the list of all files at the same level in the file system as the
 * target file, including the target file. The filenames will not include any
 * path components, and are essentially just the output of VSIReadDir() on the
 * parent directory. If the target object does not have filesystem semantics
 * then the file list should be NULL.
 *
 * @param pszFilename the name of the file to access.  In the case of
 * exotic drivers this may not refer to a physical file, but instead contain
 * information for the driver on how to access a dataset.
 *
 * @param nIdentifyFlags a combination of GDAL_OF_RASTER for raster drivers
 * or GDAL_OF_VECTOR for vector drivers. If none of the value is specified,
 * both kinds are implied.
 *
 * @param papszAllowedDrivers NULL to consider all candidate drivers, or a NULL
 * terminated list of strings with the driver short names that must be
 * considered.
 *
 * @param papszFileList an array of strings, whose last element is the NULL
 * pointer.  These strings are filenames that are auxiliary to the main
 * filename. The passed value may be NULL.
 *
 * @return A GDALDriverH handle or NULL on failure.  For C++ applications
 * this handle can be cast to a GDALDriver *.
 *
 * @since GDAL 2.2
 */

GDALDriverH CPL_STDCALL GDALIdentifyDriverEx(
    const char *pszFilename, unsigned int nIdentifyFlags,
    const char *const *papszAllowedDrivers, const char *const *papszFileList)
{
    GDALDriverManager *poDM = GetGDALDriverManager();
    CPLAssert(nullptr != poDM);
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly, papszFileList);
    oOpenInfo.papszAllowedDrivers = papszAllowedDrivers;

    CPLErrorStateBackuper oBackuper;
    CPLErrorSetState(CE_None, CPLE_AppDefined, "");

    const int nDriverCount = poDM->GetDriverCount();

    // First pass: only use drivers that have a pfnIdentify implementation.
    std::vector<GDALDriver *> apoSecondPassDrivers;
    for (int iDriver = 0; iDriver < nDriverCount; ++iDriver)
    {
        GDALDriver *poDriver = poDM->GetDriver(iDriver);
        if (papszAllowedDrivers != nullptr &&
            CSLFindString(papszAllowedDrivers,
                          GDALGetDriverShortName(poDriver)) == -1)
        {
            continue;
        }

        VALIDATE_POINTER1(poDriver, "GDALIdentifyDriver", nullptr);

        if (poDriver->pfnIdentify == nullptr &&
            poDriver->pfnIdentifyEx == nullptr)
        {
            continue;
        }

        if (papszAllowedDrivers != nullptr &&
            CSLFindString(papszAllowedDrivers,
                          GDALGetDriverShortName(poDriver)) == -1)
            continue;
        if ((nIdentifyFlags & GDAL_OF_RASTER) != 0 &&
            (nIdentifyFlags & GDAL_OF_VECTOR) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_RASTER) == nullptr)
            continue;
        if ((nIdentifyFlags & GDAL_OF_VECTOR) != 0 &&
            (nIdentifyFlags & GDAL_OF_RASTER) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr)
            continue;

        if (poDriver->pfnIdentifyEx)
        {
            if (poDriver->pfnIdentifyEx(poDriver, &oOpenInfo) > 0)
                return poDriver;
        }
        else
        {
            const int nIdentifyRes = poDriver->pfnIdentify(&oOpenInfo);
            if (nIdentifyRes > 0)
                return poDriver;
            if (nIdentifyRes < 0 &&
                poDriver->GetMetadataItem("IS_NON_LOADED_PLUGIN"))
            {
                // Not loaded plugin
                apoSecondPassDrivers.push_back(poDriver);
            }
        }
    }

    // second pass: try loading plugin drivers
    for (auto poDriver : apoSecondPassDrivers)
    {
        // Force plugin driver loading
        poDriver->GetMetadata();
        if (poDriver->pfnIdentify(&oOpenInfo) > 0)
            return poDriver;
    }

    // third pass: slow method.
    for (int iDriver = 0; iDriver < nDriverCount; ++iDriver)
    {
        GDALDriver *poDriver = poDM->GetDriver(iDriver);
        if (papszAllowedDrivers != nullptr &&
            CSLFindString(papszAllowedDrivers,
                          GDALGetDriverShortName(poDriver)) == -1)
        {
            continue;
        }

        VALIDATE_POINTER1(poDriver, "GDALIdentifyDriver", nullptr);

        if ((nIdentifyFlags & GDAL_OF_RASTER) != 0 &&
            (nIdentifyFlags & GDAL_OF_VECTOR) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_RASTER) == nullptr)
            continue;
        if ((nIdentifyFlags & GDAL_OF_VECTOR) != 0 &&
            (nIdentifyFlags & GDAL_OF_RASTER) == 0 &&
            poDriver->GetMetadataItem(GDAL_DCAP_VECTOR) == nullptr)
            continue;

        if (poDriver->pfnIdentifyEx != nullptr)
        {
            if (poDriver->pfnIdentifyEx(poDriver, &oOpenInfo) == 0)
                continue;
        }
        else if (poDriver->pfnIdentify != nullptr)
        {
            if (poDriver->pfnIdentify(&oOpenInfo) == 0)
                continue;
        }

        GDALDataset *poDS;
        if (poDriver->pfnOpen != nullptr)
        {
            poDS = poDriver->pfnOpen(&oOpenInfo);
            if (poDS != nullptr)
            {
                delete poDS;
                return GDALDriver::ToHandle(poDriver);
            }

            if (CPLGetLastErrorType() != CE_None)
                return nullptr;
        }
        else if (poDriver->pfnOpenWithDriverArg != nullptr)
        {
            poDS = poDriver->pfnOpenWithDriverArg(poDriver, &oOpenInfo);
            if (poDS != nullptr)
            {
                delete poDS;
                return GDALDriver::ToHandle(poDriver);
            }

            if (CPLGetLastErrorType() != CE_None)
                return nullptr;
        }
    }

    return nullptr;
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALDriver::SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain)

{
    if (pszDomain == nullptr || pszDomain[0] == '\0')
    {
        /* Automatically sets GDAL_DMD_EXTENSIONS from GDAL_DMD_EXTENSION */
        if (EQUAL(pszName, GDAL_DMD_EXTENSION) &&
            GDALMajorObject::GetMetadataItem(GDAL_DMD_EXTENSIONS) == nullptr)
        {
            GDALMajorObject::SetMetadataItem(GDAL_DMD_EXTENSIONS, pszValue);
        }
        /* and vice-versa if there is a single extension in GDAL_DMD_EXTENSIONS */
        else if (EQUAL(pszName, GDAL_DMD_EXTENSIONS) &&
                 strchr(pszValue, ' ') == nullptr &&
                 GDALMajorObject::GetMetadataItem(GDAL_DMD_EXTENSION) ==
                     nullptr)
        {
            GDALMajorObject::SetMetadataItem(GDAL_DMD_EXTENSION, pszValue);
        }
    }
    return GDALMajorObject::SetMetadataItem(pszName, pszValue, pszDomain);
}

/************************************************************************/
/*                   DoesDriverHandleExtension()                        */
/************************************************************************/

static bool DoesDriverHandleExtension(GDALDriverH hDriver, const char *pszExt)
{
    bool bRet = false;
    const char *pszDriverExtensions =
        GDALGetMetadataItem(hDriver, GDAL_DMD_EXTENSIONS, nullptr);
    if (pszDriverExtensions)
    {
        const CPLStringList aosTokens(CSLTokenizeString(pszDriverExtensions));
        const int nTokens = aosTokens.size();
        for (int j = 0; j < nTokens; ++j)
        {
            if (EQUAL(pszExt, aosTokens[j]))
            {
                bRet = true;
                break;
            }
        }
    }
    return bRet;
}

/************************************************************************/
/*                  GDALGetOutputDriversForDatasetName()                */
/************************************************************************/

/** Return a list of driver short names that are likely candidates for the
 * provided output file name.
 *
 * @param pszDestDataset Output dataset name (might not exist).
 * @param nFlagRasterVector GDAL_OF_RASTER, GDAL_OF_VECTOR or
 *                          binary-or'ed combination of both
 * @param bSingleMatch Whether a single match is desired, that is to say the
 *                     returned list will contain at most one item, which will
 *                     be the first driver in the order they are registered to
 *                     match the output dataset name. Note that in this mode, if
 *                     nFlagRasterVector==GDAL_OF_RASTER and pszDestDataset has
 *                     no extension, GTiff will be selected.
 * @param bEmitWarning Whether a warning should be emitted when bSingleMatch is
 *                     true and there are more than 2 candidates.
 * @return NULL terminated list of driver short names.
 * To be freed with CSLDestroy()
 * @since 3.9
 */
char **GDALGetOutputDriversForDatasetName(const char *pszDestDataset,
                                          int nFlagRasterVector,
                                          bool bSingleMatch, bool bEmitWarning)
{
    CPLStringList aosDriverNames;

    std::string osExt = CPLGetExtension(pszDestDataset);
    if (EQUAL(osExt.c_str(), "zip"))
    {
        const CPLString osLower(CPLString(pszDestDataset).tolower());
        if (osLower.endsWith(".shp.zip"))
        {
            osExt = "shp.zip";
        }
        else if (osLower.endsWith(".gpkg.zip"))
        {
            osExt = "gpkg.zip";
        }
    }

    const int nDriverCount = GDALGetDriverCount();
    for (int i = 0; i < nDriverCount; i++)
    {
        GDALDriverH hDriver = GDALGetDriver(i);
        bool bOk = false;
        if ((GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATE, nullptr) !=
                 nullptr ||
             GDALGetMetadataItem(hDriver, GDAL_DCAP_CREATECOPY, nullptr) !=
                 nullptr) &&
            (((nFlagRasterVector & GDAL_OF_RASTER) &&
              GDALGetMetadataItem(hDriver, GDAL_DCAP_RASTER, nullptr) !=
                  nullptr) ||
             ((nFlagRasterVector & GDAL_OF_VECTOR) &&
              GDALGetMetadataItem(hDriver, GDAL_DCAP_VECTOR, nullptr) !=
                  nullptr)))
        {
            bOk = true;
        }
        else if (GDALGetMetadataItem(hDriver, GDAL_DCAP_VECTOR_TRANSLATE_FROM,
                                     nullptr) &&
                 (nFlagRasterVector & GDAL_OF_VECTOR) != 0)
        {
            bOk = true;
        }
        if (bOk)
        {
            if (!osExt.empty() &&
                DoesDriverHandleExtension(hDriver, osExt.c_str()))
            {
                aosDriverNames.AddString(GDALGetDriverShortName(hDriver));
            }
            else
            {
                const char *pszPrefix = GDALGetMetadataItem(
                    hDriver, GDAL_DMD_CONNECTION_PREFIX, nullptr);
                if (pszPrefix && STARTS_WITH_CI(pszDestDataset, pszPrefix))
                {
                    aosDriverNames.AddString(GDALGetDriverShortName(hDriver));
                }
            }
        }
    }

    // GMT is registered before netCDF for opening reasons, but we want
    // netCDF to be used by default for output.
    if (EQUAL(osExt.c_str(), "nc") && aosDriverNames.size() == 2 &&
        EQUAL(aosDriverNames[0], "GMT") && EQUAL(aosDriverNames[1], "netCDF"))
    {
        aosDriverNames.Clear();
        aosDriverNames.AddString("netCDF");
        aosDriverNames.AddString("GMT");
    }

    if (bSingleMatch)
    {
        if (nFlagRasterVector == GDAL_OF_RASTER)
        {
            if (aosDriverNames.empty())
            {
                if (osExt.empty())
                {
                    aosDriverNames.AddString("GTiff");
                }
            }
            else if (aosDriverNames.size() >= 2)
            {
                if (bEmitWarning && !(EQUAL(aosDriverNames[0], "GTiff") &&
                                      EQUAL(aosDriverNames[1], "COG")))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Several drivers matching %s extension. Using %s",
                             osExt.c_str(), aosDriverNames[0]);
                }
                const std::string osDrvName = aosDriverNames[0];
                aosDriverNames.Clear();
                aosDriverNames.AddString(osDrvName.c_str());
            }
        }
        else if (aosDriverNames.size() >= 2)
        {
            if (bEmitWarning)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Several drivers matching %s extension. Using %s",
                         osExt.c_str(), aosDriverNames[0]);
            }
            const std::string osDrvName = aosDriverNames[0];
            aosDriverNames.Clear();
            aosDriverNames.AddString(osDrvName.c_str());
        }
    }

    return aosDriverNames.StealList();
}
