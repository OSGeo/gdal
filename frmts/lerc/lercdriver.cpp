/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  LERC driver
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2026, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "lercdrivercore.h"

#include "cpl_vsi.h"
#include "cpl_vsi_virtual.h"

#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "gdal_priv.h"

#include "Lerc_c_api.h"

#include <limits>

#ifndef LERC_AT_LEAST_VERSION
#define LERC_AT_LEAST_VERSION(maj, min, patch) 0
#endif

namespace gdal
{
class LERCBand;

/************************************************************************/
/*                             LERCDataset                              */
/************************************************************************/

class LERCDataset final : public GDALPamDataset
{
  public:
    LERCDataset() = default;
    ~LERCDataset() override;

    static GDALDataset *Open(GDALOpenInfo *poOpenInfo);

  private:
    friend class LERCBand;
    friend class LERCMaskBand;

    bool m_bUseNoData = true;
    double m_dfNoDataValue = std::numeric_limits<double>::quiet_NaN();
    int m_nLercDataType = 0;
    int m_nDepth = 0;
    int m_nLercBands = 0;
    int m_nMaskCount = 0;
    unsigned m_nBlobSize = 0;
    std::unique_ptr<unsigned char, VSIFreeReleaser> m_pabyBlob{};
    std::unique_ptr<unsigned char, VSIFreeReleaser> m_pabyDecodedImage{};
    std::unique_ptr<unsigned char, VSIFreeReleaser> m_pabyDecodedMask{};

    const unsigned char *GetDecodedImage();
    const unsigned char *GetDecodedMask();
};

LERCDataset::~LERCDataset() = default;

/************************************************************************/
/*                               LERCBand                               */
/************************************************************************/

class LERCBand final : public GDALPamRasterBand
{
  public:
    LERCBand(LERCDataset *poDSIn, int nBandIn, GDALDataType eDT);

    double GetNoDataValue(int *pbHasNoData) override;
    int GetMaskFlags() override;
    GDALRasterBand *GetMaskBand() override;

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override;

  private:
    std::unique_ptr<GDALRasterBand> m_poMaskBand{};
};

/************************************************************************/
/*                             LERCMaskBand                             */
/************************************************************************/

class LERCMaskBand final : public GDALRasterBand
{
  public:
    LERCMaskBand(LERCDataset *poDSIn, int nMaskIdx) : m_nMaskIdx(nMaskIdx)
    {
        poDS = poDSIn;
        nBand = 0;
        eDataType = GDT_Byte;
        nRasterXSize = poDS->GetRasterXSize();
        nRasterYSize = poDS->GetRasterYSize();
        nBlockXSize = nRasterXSize;
        nBlockYSize = 1;
    }

  protected:
    CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void *pData) override;

  private:
    const int m_nMaskIdx;
};

/************************************************************************/
/*                    LERCDataset::GetDecodedImage()                    */
/************************************************************************/

const unsigned char *LERCDataset::GetDecodedImage()
{
    if (!m_pabyBlob)
        return m_pabyDecodedImage.get();

    m_pabyDecodedImage.reset(static_cast<unsigned char *>(VSI_MALLOC_VERBOSE(
        static_cast<size_t>(nRasterXSize) * nRasterYSize * m_nDepth *
        m_nLercBands *
        GDALGetDataTypeSizeBytes(GetRasterBand(1)->GetRasterDataType()))));
    if (m_nMaskCount)
    {
        m_pabyDecodedMask.reset(static_cast<unsigned char *>(VSI_MALLOC_VERBOSE(
            static_cast<size_t>(nRasterXSize) * nRasterYSize * m_nLercBands)));
        if (!m_pabyDecodedMask)
            m_pabyDecodedImage.reset();
    }

    if (m_pabyDecodedImage)
    {
        const lerc_status status = lerc_decode(
            m_pabyBlob.get(), m_nBlobSize,
#if LERC_AT_LEAST_VERSION(3, 0, 0)
            m_nMaskCount,
#endif
            m_pabyDecodedMask.get(), m_nDepth, nRasterXSize, nRasterYSize,
            m_nLercBands, m_nLercDataType, m_pabyDecodedImage.get());
        if (status != 0)
        {
            m_pabyDecodedImage.reset();
            CPLError(CE_Failure, CPLE_AppDefined,
                     "lerc_decode() failed with status %d", status);
        }
    }
    m_pabyBlob.reset();

    return m_pabyDecodedImage.get();
}

/************************************************************************/
/*                    LERCDataset::GetDecodedMask()                     */
/************************************************************************/

const unsigned char *LERCDataset::GetDecodedMask()
{
    GetDecodedImage();
    return m_pabyDecodedMask.get();
}

/************************************************************************/
/*                         LERCBand::LERCBand()                         */
/************************************************************************/

LERCBand::LERCBand(LERCDataset *poDSIn, int nBandIn, GDALDataType eDT)
{
    poDS = poDSIn;
    nBand = nBandIn;
    eDataType = eDT;
    nRasterXSize = poDS->GetRasterXSize();
    nRasterYSize = poDS->GetRasterYSize();
    nBlockXSize = nRasterXSize;
    nBlockYSize = 1;
}

/************************************************************************/
/*                    SetNodataWhereMaskIsInvalid()                     */
/************************************************************************/

template <class T>
static void SetNodataWhereMaskIsInvalid(T *pLine, const unsigned char *pSrcMask,
                                        int nPixels, T nodataValue)
{
    for (int i = 0; i < nPixels; ++i)
    {
        if (!pSrcMask[i])
        {
            pLine[i] = nodataValue;
        }
    }
}

/************************************************************************/
/*                        LERCBand::IReadBlock()                        */
/************************************************************************/

CPLErr LERCBand::IReadBlock(int, int nBlockYOff, void *pData)
{
    auto poGDS = cpl::down_cast<LERCDataset *>(poDS);
    const unsigned char *pabyImage = poGDS->GetDecodedImage();
    if (!pabyImage)
        return CE_Failure;

    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);
    GDALCopyWords64(
        pabyImage + ((nBand - 1) + static_cast<size_t>(nBlockYOff) *
                                       nRasterXSize * poGDS->GetRasterCount()) *
                        nDTSize,
        eDataType, poGDS->GetRasterCount() * nDTSize, pData, eDataType, nDTSize,
        nRasterXSize);

    if (poGDS->m_bUseNoData &&
        (eDataType == GDT_Float32 || eDataType == GDT_Float64) &&
        poGDS->m_nMaskCount > 0)
    {
        const unsigned char *pabyMask = poGDS->GetDecodedMask();
        if (pabyMask)
        {
            const unsigned char *pSrcMask =
                pabyMask +
                static_cast<size_t>(poGDS->m_nMaskCount == 1 ? 0 : nBand - 1) *
                    nRasterXSize * nRasterYSize +
                static_cast<size_t>(nBlockYOff) * nRasterXSize;
            if (eDataType == GDT_Float32)
            {
                SetNodataWhereMaskIsInvalid(
                    static_cast<float *>(pData), pSrcMask, nRasterXSize,
                    static_cast<float>(poGDS->m_dfNoDataValue));
            }
            else
            {
                SetNodataWhereMaskIsInvalid(static_cast<double *>(pData),
                                            pSrcMask, nRasterXSize,
                                            poGDS->m_dfNoDataValue);
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                      LERCBand::GetNoDataValue()                      */
/************************************************************************/

double LERCBand::GetNoDataValue(int *pbHasNoData)
{
    auto poGDS = cpl::down_cast<LERCDataset *>(poDS);
    if (poGDS->m_bUseNoData &&
        (eDataType == GDT_Float32 || eDataType == GDT_Float64) &&
        poGDS->m_nMaskCount > 0)
    {
        if (pbHasNoData)
            *pbHasNoData = true;
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (pbHasNoData)
        *pbHasNoData = false;
    return 0;
}

/************************************************************************/
/*                       LERCBand::GetMaskFlags()                       */
/************************************************************************/

int LERCBand::GetMaskFlags()
{
    auto poGDS = cpl::down_cast<LERCDataset *>(poDS);
    if (poGDS->m_bUseNoData &&
        (eDataType == GDT_Float32 || eDataType == GDT_Float64) &&
        poGDS->m_nMaskCount > 0)
    {
        return GMF_NODATA;
    }
    if (poGDS->m_nMaskCount == 0)
        return GMF_ALL_VALID;
    if (poGDS->m_nMaskCount == 1)
        return GMF_PER_DATASET;
    return 0;
}

/************************************************************************/
/*                       LERCBand::GetMaskBand()                        */
/************************************************************************/

GDALRasterBand *LERCBand::GetMaskBand()
{
    auto poGDS = cpl::down_cast<LERCDataset *>(poDS);
    if (poGDS->m_nMaskCount == 0)
        return GDALPamRasterBand::GetMaskBand();
    if (!m_poMaskBand)
        m_poMaskBand = std::make_unique<LERCMaskBand>(poGDS, nBand - 1);
    return m_poMaskBand.get();
}

/************************************************************************/
/*                      LERCMaskBand::IReadBlock()                      */
/************************************************************************/

CPLErr LERCMaskBand::IReadBlock(int, int nBlockYOff, void *pData)
{
    auto poGDS = cpl::down_cast<LERCDataset *>(poDS);
    const unsigned char *pabyMask = poGDS->GetDecodedMask();
    if (!pabyMask)
        return CE_Failure;

    const unsigned char *pSrc =
        pabyMask +
        static_cast<size_t>(m_nMaskIdx) * nRasterXSize * nRasterYSize +
        static_cast<size_t>(nBlockYOff) * nRasterXSize;
    unsigned char *pDst = static_cast<unsigned char *>(pData);
    for (int i = 0; i < nRasterXSize; ++i)
    {
        pDst[i] = pSrc[i] ? 255 : 0;
    }

    return CE_None;
}

/************************************************************************/
/*                         LERCDataset::Open()                          */
/************************************************************************/

GDALDataset *LERCDataset::Open(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->eAccess == GA_Update || poOpenInfo->fpL == nullptr)
        return nullptr;

    VSIStatBufL sStat;
    if (VSIStatL(poOpenInfo->pszFilename, &sStat) != 0)
        return nullptr;
    if (static_cast<uint64_t>(sStat.st_size) >
        std::numeric_limits<unsigned>::max())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too large file");
        return nullptr;
    }

    const auto nRAMSize = CPLGetUsablePhysicalRAM();
    if (nRAMSize > 0 && sStat.st_size > nRAMSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too large file compared to usable RAM");
        return nullptr;
    }
    const unsigned nBlobSize = static_cast<unsigned>(sStat.st_size);
    std::unique_ptr<unsigned char, VSIFreeReleaser> pabyBlob(
        static_cast<unsigned char *>(VSI_MALLOC_VERBOSE(nBlobSize)));
    if (!pabyBlob)
        return nullptr;
    if (poOpenInfo->fpL->Seek(0, SEEK_SET) != 0 ||
        poOpenInfo->fpL->Read(pabyBlob.get(), nBlobSize) != nBlobSize)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot read file");
        return nullptr;
    }

    unsigned int infoArray[9];
    /* Info returned in infoArray is { version, dataType, nDim/nDepth, nCols,
        nRows, nBands, nValidPixels, blobSize,
        and starting with liblerc 3.0 nRequestedMasks } */

    double arrayRange[3];
    /* Info returned in infoArray is { zMin, zMax, maxZErrUsed } */

    int lerc_ret =
        lerc_getBlobInfo(pabyBlob.get(), nBlobSize, infoArray, arrayRange,
                         CPL_ARRAYSIZE(infoArray), CPL_ARRAYSIZE(arrayRange));
    if (lerc_ret != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "lerc_getBlobInfo() failed");
        return nullptr;
    }

    GDALDataType eDT = GDT_Unknown;
    const unsigned nLercDataType = infoArray[1];
    switch (nLercDataType)
    {
        case 0:
            eDT = GDT_Int8;
            break;
        case 1:
            eDT = GDT_UInt8;
            break;
        case 2:
            eDT = GDT_Int16;
            break;
        case 3:
            eDT = GDT_UInt16;
            break;
        case 4:
            eDT = GDT_Int32;
            break;
        case 5:
            eDT = GDT_UInt32;
            break;
        case 6:
            eDT = GDT_Float32;
            break;
        case 7:
            eDT = GDT_Float64;
            break;
        default:
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Unhandled LERC data type %u", nLercDataType);
            return nullptr;
    }

    const unsigned nDepth = infoArray[2];
    const unsigned nCols = infoArray[3];
    const unsigned nRows = infoArray[4];
    const unsigned nLercBands = infoArray[5];
    if (nCols == 0 || nRows == 0 || nDepth == 0 || nLercBands == 0 ||
        nCols > static_cast<unsigned>(INT_MAX) ||
        nRows > static_cast<unsigned>(INT_MAX) ||
        nDepth > static_cast<unsigned>(INT_MAX) ||
        nLercBands > static_cast<unsigned>(INT_MAX))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Invalid dimensions: cols=%u x rows=%u x depth=%u x bands=%u",
                 nCols, nRows, nDepth, nLercBands);
        return nullptr;
    }
    const int nDTSize = GDALGetDataTypeSizeBytes(eDT);
    // nCols * nRows limitation is due to liblerc assuming it fits on int
    if (nCols > std::numeric_limits<int>::max() / nRows ||
        nDepth > static_cast<unsigned>(std::numeric_limits<int>::max()) /
                     nLercBands ||
        static_cast<size_t>(nCols) * nRows >
            (std::numeric_limits<size_t>::max() / 2) / nDTSize ||
        static_cast<size_t>(nCols) * nRows * nDTSize >
            (std::numeric_limits<size_t>::max() / 2) /
                (static_cast<size_t>(nDepth) * nLercBands) ||
        (nRAMSize > 0 &&
         static_cast<GIntBig>(nCols) * nRows * nDTSize * nDepth * nLercBands >
             nRAMSize - sStat.st_size))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Too large dimensions cols=%u x rows=%u x depth=%u x bands=%u",
                 nCols, nRows, nDepth, nLercBands);
        return nullptr;
    }

    const int nGDALBands =
        static_cast<int>(nDepth) * static_cast<int>(nLercBands);

    if (nLercBands != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "LercBands=%u != 1 not supported", nLercBands);
        return nullptr;
    }

    auto poDS = std::make_unique<LERCDataset>();
    poDS->nRasterXSize = static_cast<int>(nCols);
    poDS->nRasterYSize = static_cast<int>(nRows);
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize) ||
        !GDALCheckBandCount(nGDALBands, /* bIsZeroAllowed =*/false))
    {
        return nullptr;
    }

    poDS->m_nLercDataType = nLercDataType;
    poDS->m_nBlobSize = nBlobSize;
    poDS->m_nDepth = static_cast<int>(nDepth);
    poDS->m_nLercBands = static_cast<int>(nLercBands);
    // Use by WMS driver in MRF/LERC mode
    const char *pszNDV = CSLFetchNameValue(poOpenInfo->papszOpenOptions, "NDV");
    if (pszNDV)
    {
        if (EQUAL(pszNDV, "none"))
            poDS->m_bUseNoData = false;
        else
            poDS->m_dfNoDataValue = CPLAtof(pszNDV);
    }
#if LERC_AT_LEAST_VERSION(3, 0, 0)
    poDS->m_nMaskCount = static_cast<int>(infoArray[8]);
    if (poDS->m_nMaskCount != 0 && poDS->m_nMaskCount != nGDALBands)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Mask count=%d != 0 and != band count=%d not supported",
                 poDS->m_nMaskCount, nGDALBands);
        return nullptr;
    }
#else
    const unsigned nValidPixels = infoArray[6];
    const size_t nTotalPixels = static_cast<size_t>(nCols) * nRows;
    if (nTotalPixels <= std::numeric_limits<unsigned>::max() &&
        nValidPixels < nTotalPixels)
        poDS->m_nMaskCount = 1;
#endif
    poDS->m_pabyBlob = std::move(pabyBlob);

    for (int i = 0; i < nGDALBands; ++i)
    {
        poDS->SetBand(i + 1, std::make_unique<LERCBand>(poDS.get(), i, eDT));
    }

    if (nGDALBands > 1)
    {
        poDS->GDALDataset::SetMetadataItem("INTERLEAVE", "PIXEL",
                                           "IMAGE_STRUCTURE");
    }
    poDS->GDALDataset::SetMetadataItem("COMPRESSION", "LERC",
                                       "IMAGE_STRUCTURE");
    poDS->GDALDataset::SetMetadataItem(
        "MAX_Z_ERROR", CPLSPrintf("%g", arrayRange[2]), "IMAGE_STRUCTURE");

    // Initialize any PAM information.
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML(poOpenInfo->GetSiblingFiles());
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo);

    return poDS.release();
}

}  // namespace gdal

/************************************************************************/
/*                         GDALRegister_LERC()                          */
/************************************************************************/

void GDALRegister_LERC()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    auto poDriver = std::make_unique<GDALDriver>();
    LERCDriverSetCommonMetadata(poDriver.get());

#ifdef LERC_VERSION_MAJOR
#define XSTRINGIFY(X) #X
#define STRINGIFY(X) XSTRINGIFY(X)
    poDriver->SetMetadataItem(
        "LIBLERC_VERSION",
        STRINGIFY(LERC_VERSION_MAJOR) "." STRINGIFY(
            LERC_VERSION_MINOR) "." STRINGIFY(LERC_VERSION_PATCH));
#endif

    poDriver->pfnOpen = gdal::LERCDataset::Open;
    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
