/******************************************************************************
 *
 * Name:     gdalmultidim_array_resampled.cpp
 * Project:  GDAL Core
 * Purpose:  GDALMDArray::GetResampled() implementation
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"
#include "gdal_pam.h"
#include "gdal_pam_multidim.h"
#include "gdal_rasterband.h"
#include "gdal_utils.h"

#include <algorithm>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         GDALMDArrayResampled                         */
/************************************************************************/

class GDALMDArrayResampledDataset;

class GDALMDArrayResampledDatasetRasterBand final : public GDALRasterBand
{
  protected:
    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpaceBuf,
                     GSpacing nLineSpaceBuf,
                     GDALRasterIOExtraArg *psExtraArg) override;

  public:
    explicit GDALMDArrayResampledDatasetRasterBand(
        GDALMDArrayResampledDataset *poDSIn);

    double GetNoDataValue(int *pbHasNoData) override;
};

class GDALMDArrayResampledDataset final : public GDALPamDataset
{
    friend class GDALMDArrayResampled;
    friend class GDALMDArrayResampledDatasetRasterBand;

    std::shared_ptr<GDALMDArray> m_poArray;
    const size_t m_iXDim;
    const size_t m_iYDim;
    GDALGeoTransform m_gt{};
    bool m_bHasGT = false;
    mutable std::shared_ptr<OGRSpatialReference> m_poSRS{};

    std::vector<GUInt64> m_anOffset{};
    std::vector<size_t> m_anCount{};
    std::vector<GPtrDiff_t> m_anStride{};

    std::string m_osFilenameLong{};
    std::string m_osFilenameLat{};

  public:
    GDALMDArrayResampledDataset(const std::shared_ptr<GDALMDArray> &array,
                                size_t iXDim, size_t iYDim)
        : m_poArray(array), m_iXDim(iXDim), m_iYDim(iYDim),
          m_anOffset(m_poArray->GetDimensionCount(), 0),
          m_anCount(m_poArray->GetDimensionCount(), 1),
          m_anStride(m_poArray->GetDimensionCount(), 0)
    {
        const auto &dims(m_poArray->GetDimensions());

        nRasterYSize = static_cast<int>(
            std::min(static_cast<GUInt64>(INT_MAX), dims[iYDim]->GetSize()));
        nRasterXSize = static_cast<int>(
            std::min(static_cast<GUInt64>(INT_MAX), dims[iXDim]->GetSize()));

        m_bHasGT = m_poArray->GuessGeoTransform(m_iXDim, m_iYDim, false, m_gt);

        SetBand(1, new GDALMDArrayResampledDatasetRasterBand(this));
    }

    ~GDALMDArrayResampledDataset() override;

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override
    {
        gt = m_gt;
        return m_bHasGT ? CE_None : CE_Failure;
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        m_poSRS = m_poArray->GetSpatialRef();
        if (m_poSRS)
        {
            m_poSRS.reset(m_poSRS->Clone());
            auto axisMapping = m_poSRS->GetDataAxisToSRSAxisMapping();
            for (auto &m : axisMapping)
            {
                if (m == static_cast<int>(m_iXDim) + 1)
                    m = 1;
                else if (m == static_cast<int>(m_iYDim) + 1)
                    m = 2;
            }
            m_poSRS->SetDataAxisToSRSAxisMapping(axisMapping);
        }
        return m_poSRS.get();
    }

    void SetGeolocationArray(const std::string &osFilenameLong,
                             const std::string &osFilenameLat)
    {
        m_osFilenameLong = osFilenameLong;
        m_osFilenameLat = osFilenameLat;
        CPLStringList aosGeoLoc;
        aosGeoLoc.SetNameValue("LINE_OFFSET", "0");
        aosGeoLoc.SetNameValue("LINE_STEP", "1");
        aosGeoLoc.SetNameValue("PIXEL_OFFSET", "0");
        aosGeoLoc.SetNameValue("PIXEL_STEP", "1");
        aosGeoLoc.SetNameValue("SRS", SRS_WKT_WGS84_LAT_LONG);  // FIXME?
        aosGeoLoc.SetNameValue("X_BAND", "1");
        aosGeoLoc.SetNameValue("X_DATASET", m_osFilenameLong.c_str());
        aosGeoLoc.SetNameValue("Y_BAND", "1");
        aosGeoLoc.SetNameValue("Y_DATASET", m_osFilenameLat.c_str());
        aosGeoLoc.SetNameValue("GEOREFERENCING_CONVENTION", "PIXEL_CENTER");
        SetMetadata(aosGeoLoc.List(), "GEOLOCATION");
    }
};

GDALMDArrayResampledDataset::~GDALMDArrayResampledDataset()
{
    if (!m_osFilenameLong.empty())
        VSIUnlink(m_osFilenameLong.c_str());
    if (!m_osFilenameLat.empty())
        VSIUnlink(m_osFilenameLat.c_str());
}

/************************************************************************/
/*               GDALMDArrayResampledDatasetRasterBand()                */
/************************************************************************/

GDALMDArrayResampledDatasetRasterBand::GDALMDArrayResampledDatasetRasterBand(
    GDALMDArrayResampledDataset *poDSIn)
{
    const auto &poArray(poDSIn->m_poArray);
    const auto blockSize(poArray->GetBlockSize());
    nBlockYSize = (blockSize[poDSIn->m_iYDim])
                      ? static_cast<int>(std::min(static_cast<GUInt64>(INT_MAX),
                                                  blockSize[poDSIn->m_iYDim]))
                      : 1;
    nBlockXSize = blockSize[poDSIn->m_iXDim]
                      ? static_cast<int>(std::min(static_cast<GUInt64>(INT_MAX),
                                                  blockSize[poDSIn->m_iXDim]))
                      : poDSIn->GetRasterXSize();
    eDataType = poArray->GetDataType().GetNumericDataType();
    eAccess = poDSIn->eAccess;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GDALMDArrayResampledDatasetRasterBand::GetNoDataValue(int *pbHasNoData)
{
    auto l_poDS(cpl::down_cast<GDALMDArrayResampledDataset *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    bool bHasNodata = false;
    double dfRes = poArray->GetNoDataValueAsDouble(&bHasNodata);
    if (pbHasNoData)
        *pbHasNoData = bHasNodata;
    return dfRes;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALMDArrayResampledDatasetRasterBand::IReadBlock(int nBlockXOff,
                                                         int nBlockYOff,
                                                         void *pImage)
{
    const int nDTSize(GDALGetDataTypeSizeBytes(eDataType));
    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqXSize = std::min(nRasterXSize - nXOff, nBlockXSize);
    const int nReqYSize = std::min(nRasterYSize - nYOff, nBlockYSize);
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    return IRasterIO(GF_Read, nXOff, nYOff, nReqXSize, nReqYSize, pImage,
                     nReqXSize, nReqYSize, eDataType, nDTSize,
                     static_cast<GSpacing>(nDTSize) * nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALMDArrayResampledDatasetRasterBand::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    GSpacing nPixelSpaceBuf, GSpacing nLineSpaceBuf,
    GDALRasterIOExtraArg *psExtraArg)
{
    auto l_poDS(cpl::down_cast<GDALMDArrayResampledDataset *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));
    if (eRWFlag == GF_Read && nXSize == nBufXSize && nYSize == nBufYSize &&
        nBufferDTSize > 0 && (nPixelSpaceBuf % nBufferDTSize) == 0 &&
        (nLineSpaceBuf % nBufferDTSize) == 0)
    {
        l_poDS->m_anOffset[l_poDS->m_iXDim] = static_cast<GUInt64>(nXOff);
        l_poDS->m_anCount[l_poDS->m_iXDim] = static_cast<size_t>(nXSize);
        l_poDS->m_anStride[l_poDS->m_iXDim] =
            static_cast<GPtrDiff_t>(nPixelSpaceBuf / nBufferDTSize);

        l_poDS->m_anOffset[l_poDS->m_iYDim] = static_cast<GUInt64>(nYOff);
        l_poDS->m_anCount[l_poDS->m_iYDim] = static_cast<size_t>(nYSize);
        l_poDS->m_anStride[l_poDS->m_iYDim] =
            static_cast<GPtrDiff_t>(nLineSpaceBuf / nBufferDTSize);

        return poArray->Read(l_poDS->m_anOffset.data(),
                             l_poDS->m_anCount.data(), nullptr,
                             l_poDS->m_anStride.data(),
                             GDALExtendedDataType::Create(eBufType), pData)
                   ? CE_None
                   : CE_Failure;
    }
    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpaceBuf, nLineSpaceBuf, psExtraArg);
}

class GDALMDArrayResampled final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    std::vector<std::shared_ptr<GDALDimension>> m_apoDims;
    std::vector<GUInt64> m_anBlockSize;
    GDALExtendedDataType m_dt;
    std::shared_ptr<OGRSpatialReference> m_poSRS{};
    std::shared_ptr<GDALMDArray> m_poVarX{};
    std::shared_ptr<GDALMDArray> m_poVarY{};
    std::unique_ptr<GDALMDArrayResampledDataset> m_poParentDS{};
    std::unique_ptr<GDALDataset> m_poReprojectedDS{};

  protected:
    GDALMDArrayResampled(
        const std::shared_ptr<GDALMDArray> &poParent,
        const std::vector<std::shared_ptr<GDALDimension>> &apoDims,
        const std::vector<GUInt64> &anBlockSize)
        : GDALAbstractMDArray(std::string(),
                              "Resampled view of " + poParent->GetFullName()),
          GDALPamMDArray(
              std::string(), "Resampled view of " + poParent->GetFullName(),
              GDALPamMultiDim::GetPAM(poParent), poParent->GetContext()),
          m_poParent(std::move(poParent)), m_apoDims(apoDims),
          m_anBlockSize(anBlockSize), m_dt(m_poParent->GetDataType())
    {
        CPLAssert(apoDims.size() == m_poParent->GetDimensionCount());
        CPLAssert(anBlockSize.size() == m_poParent->GetDimensionCount());
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    static std::shared_ptr<GDALMDArray>
    Create(const std::shared_ptr<GDALMDArray> &poParent,
           const std::vector<std::shared_ptr<GDALDimension>> &apoNewDims,
           GDALRIOResampleAlg resampleAlg,
           const OGRSpatialReference *poTargetSRS, CSLConstList papszOptions);

    ~GDALMDArrayResampled() override
    {
        // First close the warped VRT
        m_poReprojectedDS.reset();
        m_poParentDS.reset();
    }

    bool IsWritable() const override
    {
        return false;
    }

    const std::string &GetFilename() const override
    {
        return m_poParent->GetFilename();
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_apoDims;
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_dt;
    }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        return m_poSRS;
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        return m_anBlockSize;
    }

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override
    {
        return m_poParent->GetAttribute(osName);
    }

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override
    {
        return m_poParent->GetAttributes(papszOptions);
    }

    const std::string &GetUnit() const override
    {
        return m_poParent->GetUnit();
    }

    const void *GetRawNoDataValue() const override
    {
        return m_poParent->GetRawNoDataValue();
    }

    double GetOffset(bool *pbHasOffset,
                     GDALDataType *peStorageType) const override
    {
        return m_poParent->GetOffset(pbHasOffset, peStorageType);
    }

    double GetScale(bool *pbHasScale,
                    GDALDataType *peStorageType) const override
    {
        return m_poParent->GetScale(pbHasScale, peStorageType);
    }
};

/************************************************************************/
/*                    GDALMDArrayResampled::Create()                    */
/************************************************************************/

std::shared_ptr<GDALMDArray> GDALMDArrayResampled::Create(
    const std::shared_ptr<GDALMDArray> &poParent,
    const std::vector<std::shared_ptr<GDALDimension>> &apoNewDimsIn,
    GDALRIOResampleAlg resampleAlg, const OGRSpatialReference *poTargetSRS,
    CSLConstList /* papszOptions */)
{
    const char *pszResampleAlg = "nearest";
    bool unsupported = false;
    switch (resampleAlg)
    {
        case GRIORA_NearestNeighbour:
            pszResampleAlg = "nearest";
            break;
        case GRIORA_Bilinear:
            pszResampleAlg = "bilinear";
            break;
        case GRIORA_Cubic:
            pszResampleAlg = "cubic";
            break;
        case GRIORA_CubicSpline:
            pszResampleAlg = "cubicspline";
            break;
        case GRIORA_Lanczos:
            pszResampleAlg = "lanczos";
            break;
        case GRIORA_Average:
            pszResampleAlg = "average";
            break;
        case GRIORA_Mode:
            pszResampleAlg = "mode";
            break;
        case GRIORA_Gauss:
            unsupported = true;
            break;
        case GRIORA_RESERVED_START:
            unsupported = true;
            break;
        case GRIORA_RESERVED_END:
            unsupported = true;
            break;
        case GRIORA_RMS:
            pszResampleAlg = "rms";
            break;
    }
    if (unsupported)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported resample method for GetResampled()");
        return nullptr;
    }

    if (poParent->GetDimensionCount() < 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetResampled() only supports 2 dimensions or more");
        return nullptr;
    }

    const auto &aoParentDims = poParent->GetDimensions();
    if (apoNewDimsIn.size() != aoParentDims.size())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetResampled(): apoNewDims size should be the same as "
                 "GetDimensionCount()");
        return nullptr;
    }

    std::vector<std::shared_ptr<GDALDimension>> apoNewDims;
    apoNewDims.reserve(apoNewDimsIn.size());

    std::vector<GUInt64> anBlockSize;
    anBlockSize.reserve(apoNewDimsIn.size());
    const auto &anParentBlockSize = poParent->GetBlockSize();

    auto apoParentDims = poParent->GetDimensions();
    // Special case for NASA EMIT datasets
    const bool bYXBandOrder = (apoParentDims.size() == 3 &&
                               apoParentDims[0]->GetName() == "downtrack" &&
                               apoParentDims[1]->GetName() == "crosstrack" &&
                               apoParentDims[2]->GetName() == "bands");

    const size_t iYDimParent =
        bYXBandOrder ? 0 : poParent->GetDimensionCount() - 2;
    const size_t iXDimParent =
        bYXBandOrder ? 1 : poParent->GetDimensionCount() - 1;

    for (unsigned i = 0; i < apoNewDimsIn.size(); ++i)
    {
        if (i == iYDimParent || i == iXDimParent)
            continue;
        if (apoNewDimsIn[i] == nullptr)
        {
            apoNewDims.emplace_back(aoParentDims[i]);
        }
        else if (apoNewDimsIn[i]->GetSize() != aoParentDims[i]->GetSize() ||
                 apoNewDimsIn[i]->GetName() != aoParentDims[i]->GetName())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "GetResampled(): apoNewDims[%u] should be the same "
                     "as its parent",
                     i);
            return nullptr;
        }
        else
        {
            apoNewDims.emplace_back(aoParentDims[i]);
        }
        anBlockSize.emplace_back(anParentBlockSize[i]);
    }

    std::unique_ptr<GDALMDArrayResampledDataset> poParentDS(
        new GDALMDArrayResampledDataset(poParent, iXDimParent, iYDimParent));

    double dfXStart = 0.0;
    double dfXSpacing = 0.0;
    bool gotXSpacing = false;
    auto poNewDimX = apoNewDimsIn[iXDimParent];
    if (poNewDimX)
    {
        if (poNewDimX->GetSize() > static_cast<GUInt64>(INT_MAX))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too big size for X dimension");
            return nullptr;
        }
        auto var = poNewDimX->GetIndexingVariable();
        if (var)
        {
            if (var->GetDimensionCount() != 1 ||
                var->GetDimensions()[0]->GetSize() != poNewDimX->GetSize() ||
                !var->IsRegularlySpaced(dfXStart, dfXSpacing))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "New X dimension should be indexed by a regularly "
                         "spaced variable");
                return nullptr;
            }
            gotXSpacing = true;
        }
    }

    double dfYStart = 0.0;
    double dfYSpacing = 0.0;
    auto poNewDimY = apoNewDimsIn[iYDimParent];
    bool gotYSpacing = false;
    if (poNewDimY)
    {
        if (poNewDimY->GetSize() > static_cast<GUInt64>(INT_MAX))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Too big size for Y dimension");
            return nullptr;
        }
        auto var = poNewDimY->GetIndexingVariable();
        if (var)
        {
            if (var->GetDimensionCount() != 1 ||
                var->GetDimensions()[0]->GetSize() != poNewDimY->GetSize() ||
                !var->IsRegularlySpaced(dfYStart, dfYSpacing))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "New Y dimension should be indexed by a regularly "
                         "spaced variable");
                return nullptr;
            }
            gotYSpacing = true;
        }
    }

    // This limitation could probably be removed
    if ((gotXSpacing && !gotYSpacing) || (!gotXSpacing && gotYSpacing))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Either none of new X or Y dimension should have an indexing "
                 "variable, or both should both should have one.");
        return nullptr;
    }

    std::string osDstWKT;
    if (poTargetSRS)
    {
        char *pszDstWKT = nullptr;
        if (poTargetSRS->exportToWkt(&pszDstWKT) != OGRERR_NONE)
        {
            CPLFree(pszDstWKT);
            return nullptr;
        }
        osDstWKT = pszDstWKT;
        CPLFree(pszDstWKT);
    }

    // Use coordinate variables for geolocation array
    const auto apoCoordinateVars = poParent->GetCoordinateVariables();
    bool useGeolocationArray = false;
    if (apoCoordinateVars.size() >= 2)
    {
        std::shared_ptr<GDALMDArray> poLongVar;
        std::shared_ptr<GDALMDArray> poLatVar;
        for (const auto &poCoordVar : apoCoordinateVars)
        {
            const auto &osName = poCoordVar->GetName();
            const auto poAttr = poCoordVar->GetAttribute("standard_name");
            std::string osStandardName;
            if (poAttr && poAttr->GetDataType().GetClass() == GEDTC_STRING &&
                poAttr->GetDimensionCount() == 0)
            {
                const char *pszStandardName = poAttr->ReadAsString();
                if (pszStandardName)
                    osStandardName = pszStandardName;
            }
            if (osName == "lon" || osName == "longitude" ||
                osName == "Longitude" || osStandardName == "longitude")
            {
                poLongVar = poCoordVar;
            }
            else if (osName == "lat" || osName == "latitude" ||
                     osName == "Latitude" || osStandardName == "latitude")
            {
                poLatVar = poCoordVar;
            }
        }
        if (poLatVar != nullptr && poLongVar != nullptr)
        {
            const auto longDimCount = poLongVar->GetDimensionCount();
            const auto &longDims = poLongVar->GetDimensions();
            const auto latDimCount = poLatVar->GetDimensionCount();
            const auto &latDims = poLatVar->GetDimensions();
            const auto xDimSize = aoParentDims[iXDimParent]->GetSize();
            const auto yDimSize = aoParentDims[iYDimParent]->GetSize();
            if (longDimCount == 1 && longDims[0]->GetSize() == xDimSize &&
                latDimCount == 1 && latDims[0]->GetSize() == yDimSize)
            {
                // Geolocation arrays are 1D, and of consistent size with
                // the variable
                useGeolocationArray = true;
            }
            else if ((longDimCount == 2 ||
                      (longDimCount == 3 && longDims[0]->GetSize() == 1)) &&
                     longDims[longDimCount - 2]->GetSize() == yDimSize &&
                     longDims[longDimCount - 1]->GetSize() == xDimSize &&
                     (latDimCount == 2 ||
                      (latDimCount == 3 && latDims[0]->GetSize() == 1)) &&
                     latDims[latDimCount - 2]->GetSize() == yDimSize &&
                     latDims[latDimCount - 1]->GetSize() == xDimSize)

            {
                // Geolocation arrays are 2D (or 3D with first dimension of
                // size 1, as found in Sentinel 5P products), and of consistent
                // size with the variable
                useGeolocationArray = true;
            }
            else
            {
                CPLDebug(
                    "GDAL",
                    "Longitude and latitude coordinate variables found, "
                    "but their characteristics are not compatible of using "
                    "them as geolocation arrays");
            }
            if (useGeolocationArray)
            {
                CPLDebug("GDAL",
                         "Setting geolocation array from variables %s and %s",
                         poLongVar->GetName().c_str(),
                         poLatVar->GetName().c_str());
                const std::string osFilenameLong =
                    VSIMemGenerateHiddenFilename("longitude.tif");
                const std::string osFilenameLat =
                    VSIMemGenerateHiddenFilename("latitude.tif");
                std::unique_ptr<GDALDataset> poTmpLongDS(
                    longDimCount == 1
                        ? poLongVar->AsClassicDataset(0, 0)
                        : poLongVar->AsClassicDataset(longDimCount - 1,
                                                      longDimCount - 2));
                auto hTIFFLongDS = GDALTranslate(
                    osFilenameLong.c_str(),
                    GDALDataset::ToHandle(poTmpLongDS.get()), nullptr, nullptr);
                std::unique_ptr<GDALDataset> poTmpLatDS(
                    latDimCount == 1 ? poLatVar->AsClassicDataset(0, 0)
                                     : poLatVar->AsClassicDataset(
                                           latDimCount - 1, latDimCount - 2));
                auto hTIFFLatDS = GDALTranslate(
                    osFilenameLat.c_str(),
                    GDALDataset::ToHandle(poTmpLatDS.get()), nullptr, nullptr);
                const bool bError =
                    (hTIFFLatDS == nullptr || hTIFFLongDS == nullptr);
                GDALClose(hTIFFLongDS);
                GDALClose(hTIFFLatDS);
                if (bError)
                {
                    VSIUnlink(osFilenameLong.c_str());
                    VSIUnlink(osFilenameLat.c_str());
                    return nullptr;
                }

                poParentDS->SetGeolocationArray(osFilenameLong, osFilenameLat);
            }
        }
        else
        {
            CPLDebug("GDAL",
                     "Coordinate variables available for %s, but "
                     "longitude and/or latitude variables were not identified",
                     poParent->GetName().c_str());
        }
    }

    // Build gdalwarp arguments
    CPLStringList aosArgv;

    aosArgv.AddString("-of");
    aosArgv.AddString("VRT");

    aosArgv.AddString("-r");
    aosArgv.AddString(pszResampleAlg);

    if (!osDstWKT.empty())
    {
        aosArgv.AddString("-t_srs");
        aosArgv.AddString(osDstWKT.c_str());
    }

    if (useGeolocationArray)
        aosArgv.AddString("-geoloc");

    if (gotXSpacing && gotYSpacing)
    {
        const double dfXMin = dfXStart - dfXSpacing / 2;
        const double dfXMax =
            dfXMin + dfXSpacing * static_cast<double>(poNewDimX->GetSize());
        const double dfYMax = dfYStart - dfYSpacing / 2;
        const double dfYMin =
            dfYMax + dfYSpacing * static_cast<double>(poNewDimY->GetSize());
        aosArgv.AddString("-te");
        aosArgv.AddString(CPLSPrintf("%.17g", dfXMin));
        aosArgv.AddString(CPLSPrintf("%.17g", dfYMin));
        aosArgv.AddString(CPLSPrintf("%.17g", dfXMax));
        aosArgv.AddString(CPLSPrintf("%.17g", dfYMax));
    }

    if (poNewDimX && poNewDimY)
    {
        aosArgv.AddString("-ts");
        aosArgv.AddString(
            CPLSPrintf("%d", static_cast<int>(poNewDimX->GetSize())));
        aosArgv.AddString(
            CPLSPrintf("%d", static_cast<int>(poNewDimY->GetSize())));
    }
    else if (poNewDimX)
    {
        aosArgv.AddString("-ts");
        aosArgv.AddString(
            CPLSPrintf("%d", static_cast<int>(poNewDimX->GetSize())));
        aosArgv.AddString("0");
    }
    else if (poNewDimY)
    {
        aosArgv.AddString("-ts");
        aosArgv.AddString("0");
        aosArgv.AddString(
            CPLSPrintf("%d", static_cast<int>(poNewDimY->GetSize())));
    }

    // Create a warped VRT dataset
    GDALWarpAppOptions *psOptions =
        GDALWarpAppOptionsNew(aosArgv.List(), nullptr);
    GDALDatasetH hSrcDS = GDALDataset::ToHandle(poParentDS.get());
    std::unique_ptr<GDALDataset> poReprojectedDS(GDALDataset::FromHandle(
        GDALWarp("", nullptr, 1, &hSrcDS, psOptions, nullptr)));
    GDALWarpAppOptionsFree(psOptions);
    if (poReprojectedDS == nullptr)
        return nullptr;

    int nBlockXSize;
    int nBlockYSize;
    poReprojectedDS->GetRasterBand(1)->GetBlockSize(&nBlockXSize, &nBlockYSize);
    anBlockSize.emplace_back(nBlockYSize);
    anBlockSize.emplace_back(nBlockXSize);

    GDALGeoTransform gt;
    CPLErr eErr = poReprojectedDS->GetGeoTransform(gt);
    CPLAssert(eErr == CE_None);
    CPL_IGNORE_RET_VAL(eErr);

    auto poDimY = std::make_shared<GDALDimensionWeakIndexingVar>(
        std::string(), "dimY", GDAL_DIM_TYPE_HORIZONTAL_Y, "NORTH",
        poReprojectedDS->GetRasterYSize());
    auto varY = GDALMDArrayRegularlySpaced::Create(
        std::string(), poDimY->GetName(), poDimY, gt.yorig + gt.yscale / 2,
        gt.yscale, 0);
    poDimY->SetIndexingVariable(varY);

    auto poDimX = std::make_shared<GDALDimensionWeakIndexingVar>(
        std::string(), "dimX", GDAL_DIM_TYPE_HORIZONTAL_X, "EAST",
        poReprojectedDS->GetRasterXSize());
    auto varX = GDALMDArrayRegularlySpaced::Create(
        std::string(), poDimX->GetName(), poDimX, gt.xorig + gt.xscale / 2,
        gt.xscale, 0);
    poDimX->SetIndexingVariable(varX);

    apoNewDims.emplace_back(poDimY);
    apoNewDims.emplace_back(poDimX);
    auto newAr(std::shared_ptr<GDALMDArrayResampled>(
        new GDALMDArrayResampled(poParent, apoNewDims, anBlockSize)));
    newAr->SetSelf(newAr);
    if (poTargetSRS)
    {
        newAr->m_poSRS.reset(poTargetSRS->Clone());
    }
    else
    {
        newAr->m_poSRS = poParent->GetSpatialRef();
    }
    newAr->m_poVarX = varX;
    newAr->m_poVarY = varY;
    newAr->m_poReprojectedDS = std::move(poReprojectedDS);
    newAr->m_poParentDS = std::move(poParentDS);

    // If the input array is y,x,band ordered, the above newAr is
    // actually band,y,x ordered as it is more convenient for
    // GDALMDArrayResampled::IRead() implementation. But transpose that
    // array to the order of the input array
    if (bYXBandOrder)
        return newAr->Transpose(std::vector<int>{1, 2, 0});

    return newAr;
}

/************************************************************************/
/*                    GDALMDArrayResampled::IRead()                     */
/************************************************************************/

bool GDALMDArrayResampled::IRead(const GUInt64 *arrayStartIdx,
                                 const size_t *count, const GInt64 *arrayStep,
                                 const GPtrDiff_t *bufferStride,
                                 const GDALExtendedDataType &bufferDataType,
                                 void *pDstBuffer) const
{
    if (bufferDataType.GetClass() != GEDTC_NUMERIC)
        return false;

    struct Stack
    {
        size_t nIters = 0;
        GByte *dst_ptr = nullptr;
        GPtrDiff_t dst_inc_offset = 0;
    };

    const auto nDims = GetDimensionCount();
    std::vector<Stack> stack(nDims + 1);  // +1 to avoid -Wnull-dereference
    const size_t nBufferDTSize = bufferDataType.GetSize();
    for (size_t i = 0; i < nDims; i++)
    {
        stack[i].dst_inc_offset =
            static_cast<GPtrDiff_t>(bufferStride[i] * nBufferDTSize);
    }
    stack[0].dst_ptr = static_cast<GByte *>(pDstBuffer);

    size_t dimIdx = 0;
    const size_t iDimY = nDims - 2;
    const size_t iDimX = nDims - 1;
    // Use an array to avoid a false positive warning from CLang Static
    // Analyzer about flushCaches being never read
    bool flushCaches[] = {false};
    const bool bYXBandOrder =
        m_poParentDS->m_iYDim == 0 && m_poParentDS->m_iXDim == 1;

lbl_next_depth:
    if (dimIdx == iDimY)
    {
        if (flushCaches[0])
        {
            flushCaches[0] = false;
            // When changing of 2D slice, flush GDAL 2D buffers
            m_poParentDS->FlushCache(false);
            m_poReprojectedDS->FlushCache(false);
        }

        if (!GDALMDRasterIOFromBand(m_poReprojectedDS->GetRasterBand(1),
                                    GF_Read, iDimX, iDimY, arrayStartIdx, count,
                                    arrayStep, bufferStride, bufferDataType,
                                    stack[dimIdx].dst_ptr))
        {
            return false;
        }
    }
    else
    {
        stack[dimIdx].nIters = count[dimIdx];
        if (m_poParentDS->m_anOffset[bYXBandOrder ? 2 : dimIdx] !=
            arrayStartIdx[dimIdx])
        {
            flushCaches[0] = true;
        }
        m_poParentDS->m_anOffset[bYXBandOrder ? 2 : dimIdx] =
            arrayStartIdx[dimIdx];
        while (true)
        {
            dimIdx++;
            stack[dimIdx].dst_ptr = stack[dimIdx - 1].dst_ptr;
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if ((--stack[dimIdx].nIters) == 0)
                break;
            flushCaches[0] = true;
            ++m_poParentDS->m_anOffset[bYXBandOrder ? 2 : dimIdx];
            stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    return true;
}

//! @endcond

/************************************************************************/
/*                            GetResampled()                            */
/************************************************************************/

/** Return an array that is a resampled / reprojected view of the current array
 *
 * This is the same as the C function GDALMDArrayGetResampled().
 *
 * Currently this method can only resample along the last 2 dimensions, unless
 * orthorectifying a NASA EMIT dataset.
 *
 * For NASA EMIT datasets, if apoNewDims[] and poTargetSRS is NULL, the
 * geometry lookup table (GLT) is used by default for fast orthorectification.
 *
 * Options available are:
 * <ul>
 * <li>EMIT_ORTHORECTIFICATION=YES/NO: defaults to YES for a NASA EMIT dataset.
 * Can be set to NO to use generic reprojection method.
 * </li>
 * <li>USE_GOOD_WAVELENGTHS=YES/NO: defaults to YES. Only used for EMIT
 * orthorectification to take into account the value of the
 * /sensor_band_parameters/good_wavelengths array to decide if slices of the
 * current array along the band dimension are valid.</li>
 * </ul>
 *
 * @param apoNewDims New dimensions. Its size should be GetDimensionCount().
 *                   apoNewDims[i] can be NULL to let the method automatically
 *                   determine it.
 * @param resampleAlg Resampling algorithm
 * @param poTargetSRS Target SRS, or nullptr
 * @param papszOptions NULL-terminated list of options, or NULL.
 *
 * @return a new array, that holds a reference to the original one, and thus is
 * a view of it (not a copy), or nullptr in case of error.
 *
 * @since 3.4
 */
std::shared_ptr<GDALMDArray> GDALMDArray::GetResampled(
    const std::vector<std::shared_ptr<GDALDimension>> &apoNewDims,
    GDALRIOResampleAlg resampleAlg, const OGRSpatialReference *poTargetSRS,
    CSLConstList papszOptions) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if (!self)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }
    if (GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetResampled() only supports numeric data type");
        return nullptr;
    }

    // Special case for NASA EMIT datasets
    auto apoDims = GetDimensions();
    if (poTargetSRS == nullptr &&
        ((apoDims.size() == 3 && apoDims[0]->GetName() == "downtrack" &&
          apoDims[1]->GetName() == "crosstrack" &&
          apoDims[2]->GetName() == "bands" &&
          (apoNewDims == std::vector<std::shared_ptr<GDALDimension>>(3) ||
           apoNewDims ==
               std::vector<std::shared_ptr<GDALDimension>>{nullptr, nullptr,
                                                           apoDims[2]})) ||
         (apoDims.size() == 2 && apoDims[0]->GetName() == "downtrack" &&
          apoDims[1]->GetName() == "crosstrack" &&
          apoNewDims == std::vector<std::shared_ptr<GDALDimension>>(2))) &&
        CPLTestBool(CSLFetchNameValueDef(papszOptions,
                                         "EMIT_ORTHORECTIFICATION", "YES")))
    {
        auto poRootGroup = GetRootGroup();
        if (poRootGroup)
        {
            auto poAttrGeotransform = poRootGroup->GetAttribute("geotransform");
            auto poLocationGroup = poRootGroup->OpenGroup("location");
            if (poAttrGeotransform &&
                poAttrGeotransform->GetDataType().GetClass() == GEDTC_NUMERIC &&
                poAttrGeotransform->GetDimensionCount() == 1 &&
                poAttrGeotransform->GetDimensionsSize()[0] == 6 &&
                poLocationGroup)
            {
                auto poGLT_X = poLocationGroup->OpenMDArray("glt_x");
                auto poGLT_Y = poLocationGroup->OpenMDArray("glt_y");
                if (poGLT_X && poGLT_X->GetDimensionCount() == 2 &&
                    poGLT_X->GetDimensions()[0]->GetName() == "ortho_y" &&
                    poGLT_X->GetDimensions()[1]->GetName() == "ortho_x" &&
                    poGLT_Y && poGLT_Y->GetDimensionCount() == 2 &&
                    poGLT_Y->GetDimensions()[0]->GetName() == "ortho_y" &&
                    poGLT_Y->GetDimensions()[1]->GetName() == "ortho_x")
                {
                    return CreateGLTOrthorectified(
                        self, poRootGroup, poGLT_X, poGLT_Y,
                        /* nGLTIndexOffset = */ -1,
                        poAttrGeotransform->ReadAsDoubleArray(), papszOptions);
                }
            }
        }
    }

    if (CPLTestBool(CSLFetchNameValueDef(papszOptions,
                                         "EMIT_ORTHORECTIFICATION", "NO")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "EMIT_ORTHORECTIFICATION required, but dataset and/or "
                 "parameters are not compatible with it");
        return nullptr;
    }

    return GDALMDArrayResampled::Create(self, apoNewDims, resampleAlg,
                                        poTargetSRS, papszOptions);
}
