/******************************************************************************
 *
 * Name:     gdalmultidim_bridge_classic.cpp
 * Project:  GDAL Core
 * Purpose:  Bridge from GDALMDArray to GDALRasterBand/GDALDataset
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

#include <algorithm>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                        GDALDatasetFromArray()                        */
/************************************************************************/

class GDALDatasetFromArray;

namespace
{
struct MetadataItem
{
    std::shared_ptr<GDALAbstractMDArray> poArray{};
    std::string osName{};
    std::string osDefinition{};
    bool bDefinitionUsesPctForG = false;
};

struct BandImageryMetadata
{
    std::shared_ptr<GDALAbstractMDArray> poCentralWavelengthArray{};
    double dfCentralWavelengthToMicrometer = 1.0;
    std::shared_ptr<GDALAbstractMDArray> poFWHMArray{};
    double dfFWHMToMicrometer = 1.0;
};

}  // namespace

class GDALRasterBandFromArray final : public GDALPamRasterBand
{
    std::vector<GUInt64> m_anOffset{};
    std::vector<size_t> m_anCount{};
    std::vector<GPtrDiff_t> m_anStride{};

  protected:
    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IWriteBlock(int, int, void *) override;
    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, GSpacing nPixelSpaceBuf,
                     GSpacing nLineSpaceBuf,
                     GDALRasterIOExtraArg *psExtraArg) override;

  public:
    explicit GDALRasterBandFromArray(
        GDALDatasetFromArray *poDSIn,
        const std::vector<GUInt64> &anOtherDimCoord,
        const std::vector<std::vector<MetadataItem>>
            &aoBandParameterMetadataItems,
        const std::vector<BandImageryMetadata> &aoBandImageryMetadata,
        double dfDelay, time_t nStartTime, bool &bHasWarned);

    double GetNoDataValue(int *pbHasNoData) override;
    int64_t GetNoDataValueAsInt64(int *pbHasNoData) override;
    uint64_t GetNoDataValueAsUInt64(int *pbHasNoData) override;
    double GetOffset(int *pbHasOffset) override;
    double GetScale(int *pbHasScale) override;
    const char *GetUnitType() override;
    GDALColorInterp GetColorInterpretation() override;
    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int idx) override;
    CPLErr AdviseRead(int nXOff, int nYOff, int nXSize, int nYSize,
                      int nBufXSize, int nBufYSize, GDALDataType eBufType,
                      CSLConstList papszOptions) override;
};

class GDALDatasetFromArray final : public GDALPamDataset
{
    friend class GDALRasterBandFromArray;

    std::shared_ptr<GDALMDArray> m_poArray;
    const size_t m_iXDim;
    const size_t m_iYDim;
    const CPLStringList m_aosOptions;
    GDALGeoTransform m_gt{};
    bool m_bHasGT = false;
    mutable std::shared_ptr<OGRSpatialReference> m_poSRS{};
    GDALMultiDomainMetadata m_oMDD{};
    std::string m_osOvrFilename{};
    bool m_bOverviewsDiscovered = false;
    bool m_bPixelInterleaved = false;
    std::vector<std::unique_ptr<GDALDataset, GDALDatasetUniquePtrReleaser>>
        m_apoOverviews{};
    std::vector<GUInt64> m_anOffset{};
    std::vector<size_t> m_anCount{};
    std::vector<GPtrDiff_t> m_anStride{};

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount,
                     BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;

  public:
    GDALDatasetFromArray(const std::shared_ptr<GDALMDArray> &array,
                         size_t iXDim, size_t iYDim,
                         const CPLStringList &aosOptions)
        : m_poArray(array), m_iXDim(iXDim), m_iYDim(iYDim),
          m_aosOptions(aosOptions)
    {
        const auto nDimCount = m_poArray->GetDimensionCount();
        m_anOffset.resize(nDimCount);
        m_anCount.resize(nDimCount, 1);
        m_anStride.resize(nDimCount);
    }

    static std::unique_ptr<GDALDatasetFromArray>
    Create(const std::shared_ptr<GDALMDArray> &array, size_t iXDim,
           size_t iYDim, const std::shared_ptr<GDALGroup> &poRootGroup,
           CSLConstList papszOptions);

    ~GDALDatasetFromArray() override;

    CPLErr Close(GDALProgressFunc = nullptr, void * = nullptr) override
    {
        CPLErr eErr = CE_None;
        if (nOpenFlags != OPEN_FLAGS_CLOSED)
        {
            if (GDALDatasetFromArray::FlushCache(/*bAtClosing=*/true) !=
                CE_None)
                eErr = CE_Failure;
            m_poArray.reset();
        }
        return eErr;
    }

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override
    {
        gt = m_gt;
        return m_bHasGT ? CE_None : CE_Failure;
    }

    const OGRSpatialReference *GetSpatialRef() const override
    {
        if (m_poArray->GetDimensionCount() < 2)
            return nullptr;
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

    CPLErr SetMetadata(CSLConstList papszMetadata,
                       const char *pszDomain) override
    {
        return m_oMDD.SetMetadata(papszMetadata, pszDomain);
    }

    CSLConstList GetMetadata(const char *pszDomain) override
    {
        return m_oMDD.GetMetadata(pszDomain);
    }

    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain) override
    {
        const std::string &osFilename = m_poArray->GetFilename();
        if (!osFilename.empty() && pszName && EQUAL(pszName, "OVERVIEW_FILE") &&
            pszDomain && EQUAL(pszDomain, "OVERVIEWS"))
        {
            if (m_osOvrFilename.empty())
            {
                // Legacy strategy (pre GDAL 3.13)
                std::string osOvrFilename = osFilename;
                osOvrFilename += '.';
                for (char ch : m_poArray->GetName())
                {
                    if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= 'a' && ch <= 'z') || ch == '_')
                    {
                        osOvrFilename += ch;
                    }
                    else
                    {
                        osOvrFilename += '_';
                    }
                }
                osOvrFilename += ".ovr";
                VSIStatBufL sStatBuf;
                if (VSIStatL(osOvrFilename.c_str(), &sStatBuf) == 0)
                {
                    m_osOvrFilename = std::move(osOvrFilename);
                }
                else
                {
                    auto poPAM = GDALPamMultiDim::GetPAM(m_poArray);
                    if (!poPAM)
                        poPAM = std::make_shared<GDALPamMultiDim>(osFilename);
                    m_osOvrFilename = poPAM->GetOverviewFilename(
                        m_poArray->GetFullName(), m_poArray->GetContext());
                }

                if (!m_osOvrFilename.empty())
                    oOvManager.Initialize(this, ":::VIRTUAL:::");
            }
            return !m_osOvrFilename.empty() ? m_osOvrFilename.c_str() : nullptr;
        }
        return m_oMDD.GetMetadataItem(pszName, pszDomain);
    }

    bool HasRegisteredPAMOverviewFile()
    {
        return GetMetadataItem("OVERVIEW_FILE", "OVERVIEWS") != nullptr;
    }

    CPLErr IBuildOverviews(const char *pszResampling, int nOverviews,
                           const int *panOverviewList, int nListBands,
                           const int *panBandList, GDALProgressFunc pfnProgress,
                           void *pProgressData,
                           CSLConstList papszOptions) override
    {
        // Try the multidimensional array path. Use quiet handler to
        // suppress the "not supported" error from the base class stub.
        bool bNotSupported = false;
        std::string osErrMsg;
        CPLErr eSavedClass = CE_None;
        int nSavedNo = CPLE_None;
        {
            CPLErrorHandlerPusher oQuiet(CPLQuietErrorHandler);
            CPLErr eErr = m_poArray->BuildOverviews(
                pszResampling, nOverviews, panOverviewList, pfnProgress,
                pProgressData, papszOptions);
            if (eErr == CE_None)
            {
                m_bOverviewsDiscovered = false;
                m_apoOverviews.clear();
                return CE_None;
            }
            nSavedNo = CPLGetLastErrorNo();
            eSavedClass = CPLGetLastErrorType();
            osErrMsg = CPLGetLastErrorMsg();
            bNotSupported = (nSavedNo == CPLE_NotSupported);
        }
        if (!bNotSupported)
        {
            // Re-emit the error that was suppressed by the quiet handler.
            CPLError(eSavedClass, nSavedNo, "%s", osErrMsg.c_str());
            return CE_Failure;
        }
        // Driver doesn't implement BuildOverviews - fall back to
        // default path (e.g. external .ovr file).
        CPLErrorReset();

        if (!HasRegisteredPAMOverviewFile())
        {
            const std::string &osFilename = m_poArray->GetFilename();
            if (osFilename.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "No filename associated with array %s",
                         m_poArray->GetFullName().c_str());
                return CE_Failure;
            }
            auto poPAM = GDALPamMultiDim::GetPAM(m_poArray);
            if (!poPAM)
                poPAM = std::make_shared<GDALPamMultiDim>(osFilename);
            m_osOvrFilename = poPAM->GenerateOverviewFilename(
                m_poArray->GetFullName(), m_poArray->GetContext());
            if (m_osOvrFilename.empty())
                return CE_Failure;
            oOvManager.Initialize(this, ":::VIRTUAL:::");
        }

        return GDALDataset::IBuildOverviews(
            pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
            pfnProgress, pProgressData, papszOptions);
    }

    void DiscoverOverviews()
    {
        if (!m_bOverviewsDiscovered)
        {
            m_bOverviewsDiscovered = true;
            if (const int nOverviews = m_poArray->GetOverviewCount())
            {
                if (auto poRootGroup = m_poArray->GetRootGroup())
                {
                    const size_t nDims = m_poArray->GetDimensionCount();
                    CPLStringList aosOptions(m_aosOptions);
                    aosOptions.SetNameValue("LOAD_PAM", "NO");
                    for (int iOvr = 0; iOvr < nOverviews; ++iOvr)
                    {
                        if (auto poOvrArray = m_poArray->GetOverview(iOvr))
                        {
                            if (poOvrArray->GetDimensionCount() == nDims &&
                                poOvrArray->GetDataType() ==
                                    m_poArray->GetDataType())
                            {
                                auto poOvrDS =
                                    Create(poOvrArray, m_iXDim, m_iYDim,
                                           poRootGroup, aosOptions);
                                if (poOvrDS)
                                {
                                    m_apoOverviews.push_back(
                                        std::unique_ptr<
                                            GDALDataset,
                                            GDALDatasetUniquePtrReleaser>(
                                            poOvrDS.release()));
                                }
                            }
                        }
                    }
                }
            }
        }
    }
};

GDALDatasetFromArray::~GDALDatasetFromArray()
{
    GDALDatasetFromArray::Close();
}

/************************************************************************/
/*                      GDALRasterBandFromArray()                       */
/************************************************************************/

GDALRasterBandFromArray::GDALRasterBandFromArray(
    GDALDatasetFromArray *poDSIn, const std::vector<GUInt64> &anOtherDimCoord,
    const std::vector<std::vector<MetadataItem>> &aoBandParameterMetadataItems,
    const std::vector<BandImageryMetadata> &aoBandImageryMetadata,
    double dfDelay, time_t nStartTime, bool &bHasWarned)
{
    const auto &poArray(poDSIn->m_poArray);
    const auto &dims(poArray->GetDimensions());
    const auto nDimCount(dims.size());
    const auto blockSize(poArray->GetBlockSize());

    nBlockYSize = (nDimCount >= 2 && blockSize[poDSIn->m_iYDim])
                      ? static_cast<int>(std::min(static_cast<GUInt64>(INT_MAX),
                                                  blockSize[poDSIn->m_iYDim]))
                      : 1;
    nBlockXSize = blockSize[poDSIn->m_iXDim]
                      ? static_cast<int>(std::min(static_cast<GUInt64>(INT_MAX),
                                                  blockSize[poDSIn->m_iXDim]))
                      : poDSIn->GetRasterXSize();

    eDataType = poArray->GetDataType().GetNumericDataType();
    const int nDTSize = GDALGetDataTypeSizeBytes(eDataType);

    if (nDTSize > 0)
    {
        // If the above computed block size exceeds INT_MAX or 1/100th of the
        // maximum allowed size for the block cache, divide its shape by two,
        // along the largest dimension. Only do that while there are at least
        // one dimension with 2 pixels.
        while (
            (nBlockXSize >= 2 || nBlockYSize >= 2) &&
            (nBlockXSize > INT_MAX / nBlockYSize / nDTSize ||
             (nBlockXSize > GDALGetCacheMax64() / 100 / nBlockYSize / nDTSize)))
        {
            if (nBlockXSize > nBlockYSize)
                nBlockXSize /= 2;
            else
                nBlockYSize /= 2;
        }
    }

    eAccess = poDSIn->eAccess;
    m_anOffset.resize(nDimCount);
    m_anCount.resize(nDimCount, 1);
    m_anStride.resize(nDimCount);
    for (size_t i = 0, j = 0; i < nDimCount; ++i)
    {
        if (i != poDSIn->m_iXDim && !(nDimCount >= 2 && i == poDSIn->m_iYDim))
        {
            std::string dimName(dims[i]->GetName());
            GUInt64 nIndex = anOtherDimCoord[j];
            // Detect subset_{orig_dim_name}_{start}_{incr}_{size} names of
            // subsetted dimensions as generated by GetView()
            if (STARTS_WITH(dimName.c_str(), "subset_"))
            {
                CPLStringList aosTokens(
                    CSLTokenizeString2(dimName.c_str(), "_", 0));
                if (aosTokens.size() == 5)
                {
                    dimName = aosTokens[1];
                    const auto nStartDim = static_cast<GUInt64>(CPLScanUIntBig(
                        aosTokens[2], static_cast<int>(strlen(aosTokens[2]))));
                    const auto nIncrDim = CPLAtoGIntBig(aosTokens[3]);
                    nIndex = nIncrDim > 0 ? nStartDim + nIndex * nIncrDim
                                          : nStartDim - (nIndex * -nIncrDim);
                }
            }
            if (nDimCount != 3 || dimName != "Band")
            {
                SetMetadataItem(
                    CPLSPrintf("DIM_%s_INDEX", dimName.c_str()),
                    CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(nIndex)));
            }

            auto indexingVar = dims[i]->GetIndexingVariable();

            // If the indexing variable is also listed in band parameter arrays,
            // then don't use our default formatting
            if (indexingVar)
            {
                for (const auto &oItem : aoBandParameterMetadataItems[j])
                {
                    if (oItem.poArray->GetFullName() ==
                        indexingVar->GetFullName())
                    {
                        indexingVar.reset();
                        break;
                    }
                }
            }

            if (indexingVar && indexingVar->GetDimensionCount() == 1 &&
                indexingVar->GetDimensions()[0]->GetSize() ==
                    dims[i]->GetSize())
            {
                if (dfDelay >= 0 && time(nullptr) - nStartTime > dfDelay)
                {
                    if (!bHasWarned)
                    {
                        CPLError(
                            CE_Warning, CPLE_AppDefined,
                            "Maximum delay to load band metadata from "
                            "dimension indexing variables has expired. "
                            "Increase the value of the "
                            "LOAD_EXTRA_DIM_METADATA_DELAY "
                            "option of GDALMDArray::AsClassicDataset() "
                            "(also accessible as the "
                            "GDAL_LOAD_EXTRA_DIM_METADATA_DELAY "
                            "configuration option), "
                            "or set it to 'unlimited' for unlimited delay. ");
                        bHasWarned = true;
                    }
                }
                else
                {
                    size_t nCount = 1;
                    const auto &dt(indexingVar->GetDataType());
                    std::vector<GByte> abyTmp(dt.GetSize());
                    if (indexingVar->Read(&(anOtherDimCoord[j]), &nCount,
                                          nullptr, nullptr, dt, &abyTmp[0]))
                    {
                        char *pszTmp = nullptr;
                        GDALExtendedDataType::CopyValue(
                            &abyTmp[0], dt, &pszTmp,
                            GDALExtendedDataType::CreateString());
                        dt.FreeDynamicMemory(abyTmp.data());
                        if (pszTmp)
                        {
                            SetMetadataItem(
                                CPLSPrintf("DIM_%s_VALUE", dimName.c_str()),
                                pszTmp);
                            CPLFree(pszTmp);
                        }

                        const auto &unit(indexingVar->GetUnit());
                        if (!unit.empty())
                        {
                            SetMetadataItem(
                                CPLSPrintf("DIM_%s_UNIT", dimName.c_str()),
                                unit.c_str());
                        }
                    }
                }
            }

            for (const auto &oItem : aoBandParameterMetadataItems[j])
            {
                CPLString osVal;

                size_t nCount = 1;
                const auto &dt(oItem.poArray->GetDataType());
                if (oItem.bDefinitionUsesPctForG)
                {
                    // There is one and only one %[x][.y]f|g in osDefinition
                    std::vector<GByte> abyTmp(dt.GetSize());
                    if (oItem.poArray->Read(&(anOtherDimCoord[j]), &nCount,
                                            nullptr, nullptr, dt, &abyTmp[0]))
                    {
                        double dfVal = 0;
                        GDALExtendedDataType::CopyValue(
                            &abyTmp[0], dt, &dfVal,
                            GDALExtendedDataType::Create(GDT_Float64));
                        osVal.Printf(oItem.osDefinition.c_str(), dfVal);
                        dt.FreeDynamicMemory(abyTmp.data());
                    }
                }
                else
                {
                    // There should be zero or one %s in osDefinition
                    char *pszValue = nullptr;
                    if (dt.GetClass() == GEDTC_STRING)
                    {
                        CPL_IGNORE_RET_VAL(oItem.poArray->Read(
                            &(anOtherDimCoord[j]), &nCount, nullptr, nullptr,
                            dt, &pszValue));
                    }
                    else
                    {
                        std::vector<GByte> abyTmp(dt.GetSize());
                        if (oItem.poArray->Read(&(anOtherDimCoord[j]), &nCount,
                                                nullptr, nullptr, dt,
                                                &abyTmp[0]))
                        {
                            GDALExtendedDataType::CopyValue(
                                &abyTmp[0], dt, &pszValue,
                                GDALExtendedDataType::CreateString());
                        }
                    }

                    if (pszValue)
                    {
                        osVal.Printf(oItem.osDefinition.c_str(), pszValue);
                        CPLFree(pszValue);
                    }
                }
                if (!osVal.empty())
                    SetMetadataItem(oItem.osName.c_str(), osVal);
            }

            if (aoBandImageryMetadata[j].poCentralWavelengthArray)
            {
                auto &poCentralWavelengthArray =
                    aoBandImageryMetadata[j].poCentralWavelengthArray;
                size_t nCount = 1;
                const auto &dt(poCentralWavelengthArray->GetDataType());
                std::vector<GByte> abyTmp(dt.GetSize());
                if (poCentralWavelengthArray->Read(&(anOtherDimCoord[j]),
                                                   &nCount, nullptr, nullptr,
                                                   dt, &abyTmp[0]))
                {
                    double dfVal = 0;
                    GDALExtendedDataType::CopyValue(
                        &abyTmp[0], dt, &dfVal,
                        GDALExtendedDataType::Create(GDT_Float64));
                    dt.FreeDynamicMemory(abyTmp.data());
                    SetMetadataItem(
                        "CENTRAL_WAVELENGTH_UM",
                        CPLSPrintf(
                            "%g", dfVal * aoBandImageryMetadata[j]
                                              .dfCentralWavelengthToMicrometer),
                        "IMAGERY");
                }
            }

            if (aoBandImageryMetadata[j].poFWHMArray)
            {
                auto &poFWHMArray = aoBandImageryMetadata[j].poFWHMArray;
                size_t nCount = 1;
                const auto &dt(poFWHMArray->GetDataType());
                std::vector<GByte> abyTmp(dt.GetSize());
                if (poFWHMArray->Read(&(anOtherDimCoord[j]), &nCount, nullptr,
                                      nullptr, dt, &abyTmp[0]))
                {
                    double dfVal = 0;
                    GDALExtendedDataType::CopyValue(
                        &abyTmp[0], dt, &dfVal,
                        GDALExtendedDataType::Create(GDT_Float64));
                    dt.FreeDynamicMemory(abyTmp.data());
                    SetMetadataItem(
                        "FWHM_UM",
                        CPLSPrintf("%g", dfVal * aoBandImageryMetadata[j]
                                                     .dfFWHMToMicrometer),
                        "IMAGERY");
                }
            }

            m_anOffset[i] = anOtherDimCoord[j];
            j++;
        }
    }
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double GDALRasterBandFromArray::GetNoDataValue(int *pbHasNoData)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    bool bHasNodata = false;
    const auto res = poArray->GetNoDataValueAsDouble(&bHasNodata);
    if (pbHasNoData)
        *pbHasNoData = bHasNodata;
    return res;
}

/************************************************************************/
/*                       GetNoDataValueAsInt64()                        */
/************************************************************************/

int64_t GDALRasterBandFromArray::GetNoDataValueAsInt64(int *pbHasNoData)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    bool bHasNodata = false;
    const auto nodata = poArray->GetNoDataValueAsInt64(&bHasNodata);
    if (pbHasNoData)
        *pbHasNoData = bHasNodata;
    return nodata;
}

/************************************************************************/
/*                       GetNoDataValueAsUInt64()                       */
/************************************************************************/

uint64_t GDALRasterBandFromArray::GetNoDataValueAsUInt64(int *pbHasNoData)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    bool bHasNodata = false;
    const auto nodata = poArray->GetNoDataValueAsUInt64(&bHasNodata);
    if (pbHasNoData)
        *pbHasNoData = bHasNodata;
    return nodata;
}

/************************************************************************/
/*                             GetOffset()                              */
/************************************************************************/

double GDALRasterBandFromArray::GetOffset(int *pbHasOffset)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    bool bHasValue = false;
    double dfRes = poArray->GetOffset(&bHasValue);
    if (pbHasOffset)
        *pbHasOffset = bHasValue;
    return dfRes;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char *GDALRasterBandFromArray::GetUnitType()
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    return poArray->GetUnit().c_str();
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double GDALRasterBandFromArray::GetScale(int *pbHasScale)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    bool bHasValue = false;
    double dfRes = poArray->GetScale(&bHasValue);
    if (pbHasScale)
        *pbHasScale = bHasValue;
    return dfRes;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr GDALRasterBandFromArray::IReadBlock(int nBlockXOff, int nBlockYOff,
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
/*                            IWriteBlock()                             */
/************************************************************************/

CPLErr GDALRasterBandFromArray::IWriteBlock(int nBlockXOff, int nBlockYOff,
                                            void *pImage)
{
    const int nDTSize(GDALGetDataTypeSizeBytes(eDataType));
    const int nXOff = nBlockXOff * nBlockXSize;
    const int nYOff = nBlockYOff * nBlockYSize;
    const int nReqXSize = std::min(nRasterXSize - nXOff, nBlockXSize);
    const int nReqYSize = std::min(nRasterYSize - nYOff, nBlockYSize);
    GDALRasterIOExtraArg sExtraArg;
    INIT_RASTERIO_EXTRA_ARG(sExtraArg);
    return IRasterIO(GF_Write, nXOff, nYOff, nReqXSize, nReqYSize, pImage,
                     nReqXSize, nReqYSize, eDataType, nDTSize,
                     static_cast<GSpacing>(nDTSize) * nBlockXSize, &sExtraArg);
}

/************************************************************************/
/*                             AdviseRead()                             */
/************************************************************************/

CPLErr GDALRasterBandFromArray::AdviseRead(int nXOff, int nYOff, int nXSize,
                                           int nYSize, int nBufXSize,
                                           int nBufYSize,
                                           GDALDataType /*eBufType*/,
                                           CSLConstList papszOptions)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    int bStopProcessing = FALSE;
    const CPLErr eErr = l_poDS->ValidateRasterIOOrAdviseReadParameters(
        "AdviseRead()", &bStopProcessing, nXOff, nYOff, nXSize, nYSize,
        nBufXSize, nBufYSize, 1, &nBand);
    if (eErr != CE_None || bStopProcessing)
        return eErr;

    const auto &poArray(l_poDS->m_poArray);
    std::vector<GUInt64> anArrayStartIdx = m_anOffset;
    std::vector<size_t> anCount = m_anCount;
    anArrayStartIdx[l_poDS->m_iXDim] = nXOff;
    anCount[l_poDS->m_iXDim] = nXSize;
    if (poArray->GetDimensionCount() >= 2)
    {
        anArrayStartIdx[l_poDS->m_iYDim] = nYOff;
        anCount[l_poDS->m_iYDim] = nYSize;
    }
    return poArray->AdviseRead(anArrayStartIdx.data(), anCount.data(),
                               papszOptions)
               ? CE_None
               : CE_Failure;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALRasterBandFromArray::IRasterIO(GDALRWFlag eRWFlag, int nXOff,
                                          int nYOff, int nXSize, int nYSize,
                                          void *pData, int nBufXSize,
                                          int nBufYSize, GDALDataType eBufType,
                                          GSpacing nPixelSpaceBuf,
                                          GSpacing nLineSpaceBuf,
                                          GDALRasterIOExtraArg *psExtraArg)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));
    // If reading/writing at full resolution and with proper stride, go
    // directly to the array, but, for performance reasons,
    // only if exactly on chunk boundaries, otherwise go through the block cache.
    if (nXSize == nBufXSize && nYSize == nBufYSize && nBufferDTSize > 0 &&
        (nPixelSpaceBuf % nBufferDTSize) == 0 &&
        (nLineSpaceBuf % nBufferDTSize) == 0 && (nXOff % nBlockXSize) == 0 &&
        (nYOff % nBlockYSize) == 0 &&
        ((nXSize % nBlockXSize) == 0 || nXOff + nXSize == nRasterXSize) &&
        ((nYSize % nBlockYSize) == 0 || nYOff + nYSize == nRasterYSize))
    {
        m_anOffset[l_poDS->m_iXDim] = static_cast<GUInt64>(nXOff);
        m_anCount[l_poDS->m_iXDim] = static_cast<size_t>(nXSize);
        m_anStride[l_poDS->m_iXDim] =
            static_cast<GPtrDiff_t>(nPixelSpaceBuf / nBufferDTSize);
        if (poArray->GetDimensionCount() >= 2)
        {
            m_anOffset[l_poDS->m_iYDim] = static_cast<GUInt64>(nYOff);
            m_anCount[l_poDS->m_iYDim] = static_cast<size_t>(nYSize);
            m_anStride[l_poDS->m_iYDim] =
                static_cast<GPtrDiff_t>(nLineSpaceBuf / nBufferDTSize);
        }
        if (eRWFlag == GF_Read)
        {
            return poArray->Read(m_anOffset.data(), m_anCount.data(), nullptr,
                                 m_anStride.data(),
                                 GDALExtendedDataType::Create(eBufType), pData)
                       ? CE_None
                       : CE_Failure;
        }
        else
        {
            return poArray->Write(m_anOffset.data(), m_anCount.data(), nullptr,
                                  m_anStride.data(),
                                  GDALExtendedDataType::Create(eBufType), pData)
                       ? CE_None
                       : CE_Failure;
        }
    }
    // For unaligned reads, give the array a chance to pre-populate its
    // internal chunk cache (e.g. Zarr v3 sharded batches I/O via
    // PreloadShardedBlocks). The block cache loop below then hits the
    // already-decompressed chunks instead of issuing individual reads.
    // Backends that don't override AdviseRead() return true (no-op).
    if (eRWFlag == GF_Read)
    {
        AdviseRead(nXOff, nYOff, nXSize, nYSize, nBufXSize, nBufYSize, eBufType,
                   nullptr);
    }
    return GDALRasterBand::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                     pData, nBufXSize, nBufYSize, eBufType,
                                     nPixelSpaceBuf, nLineSpaceBuf, psExtraArg);
}

/************************************************************************/
/*                        IsContiguousSequence()                        */
/************************************************************************/

static bool IsContiguousSequence(int nBandCount, const int *panBandMap)
{
    for (int i = 1; i < nBandCount; ++i)
    {
        if (panBandMap[i] != panBandMap[i - 1] + 1)
            return false;
    }
    return true;
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr GDALDatasetFromArray::IRasterIO(
    GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize, int nYSize,
    void *pData, int nBufXSize, int nBufYSize, GDALDataType eBufType,
    int nBandCount, BANDMAP_TYPE panBandMap, GSpacing nPixelSpaceBuf,
    GSpacing nLineSpaceBuf, GSpacing nBandSpaceBuf,
    GDALRasterIOExtraArg *psExtraArg)
{
    const int nBufferDTSize(GDALGetDataTypeSizeBytes(eBufType));
    int nBlockXSize, nBlockYSize;
    papoBands[0]->GetBlockSize(&nBlockXSize, &nBlockYSize);

    // If reading/writing at full resolution and with proper stride, go
    // directly to the array, but, for performance reasons,
    // only if exactly on chunk boundaries, or with a pixel interleaved dataset,
    // otherwise go through the block cache.
    if (m_poArray->GetDimensionCount() == 3 && nXSize == nBufXSize &&
        nYSize == nBufYSize && nBufferDTSize > 0 && nBandCount == nBands &&
        (nPixelSpaceBuf % nBufferDTSize) == 0 &&
        (nLineSpaceBuf % nBufferDTSize) == 0 &&
        (m_bPixelInterleaved ||
         ((nXOff % nBlockXSize) == 0 && (nYOff % nBlockYSize) == 0 &&
          ((nXSize % nBlockXSize) == 0 || nXOff + nXSize == nRasterXSize) &&
          ((nYSize % nBlockYSize) == 0 || nYOff + nYSize == nRasterYSize))) &&
        IsContiguousSequence(nBandCount, panBandMap))
    {
        m_anOffset[m_iXDim] = static_cast<GUInt64>(nXOff);
        m_anCount[m_iXDim] = static_cast<size_t>(nXSize);
        m_anStride[m_iXDim] =
            static_cast<GPtrDiff_t>(nPixelSpaceBuf / nBufferDTSize);

        m_anOffset[m_iYDim] = static_cast<GUInt64>(nYOff);
        m_anCount[m_iYDim] = static_cast<size_t>(nYSize);
        m_anStride[m_iYDim] =
            static_cast<GPtrDiff_t>(nLineSpaceBuf / nBufferDTSize);

        size_t iBandDim = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            if (i != m_iXDim && i != m_iYDim)
            {
                iBandDim = i;
                break;
            }
        }

        m_anOffset[iBandDim] = panBandMap[0] - 1;
        m_anCount[iBandDim] = nBandCount;
        m_anStride[iBandDim] =
            static_cast<GPtrDiff_t>(nBandSpaceBuf / nBufferDTSize);

        if (eRWFlag == GF_Read)
        {
            return m_poArray->Read(m_anOffset.data(), m_anCount.data(), nullptr,
                                   m_anStride.data(),
                                   GDALExtendedDataType::Create(eBufType),
                                   pData)
                       ? CE_None
                       : CE_Failure;
        }
        else
        {
            return m_poArray->Write(m_anOffset.data(), m_anCount.data(),
                                    nullptr, m_anStride.data(),
                                    GDALExtendedDataType::Create(eBufType),
                                    pData)
                       ? CE_None
                       : CE_Failure;
        }
    }

    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize, pData,
                                  nBufXSize, nBufYSize, eBufType, nBandCount,
                                  panBandMap, nPixelSpaceBuf, nLineSpaceBuf,
                                  nBandSpaceBuf, psExtraArg);
}

/************************************************************************/
/*                       GetColorInterpretation()                       */
/************************************************************************/

GDALColorInterp GDALRasterBandFromArray::GetColorInterpretation()
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    const auto &poArray(l_poDS->m_poArray);
    auto poAttr = poArray->GetAttribute("COLOR_INTERPRETATION");
    if (poAttr && poAttr->GetDataType().GetClass() == GEDTC_STRING)
    {
        bool bOK = false;
        GUInt64 nStartIndex = 0;
        if (poArray->GetDimensionCount() == 2 &&
            poAttr->GetDimensionCount() == 0)
        {
            bOK = true;
        }
        else if (poArray->GetDimensionCount() == 3)
        {
            uint64_t nExtraDimSamples = 1;
            const auto &apoDims = poArray->GetDimensions();
            for (size_t i = 0; i < apoDims.size(); ++i)
            {
                if (i != l_poDS->m_iXDim && i != l_poDS->m_iYDim)
                    nExtraDimSamples *= apoDims[i]->GetSize();
            }
            if (poAttr->GetDimensionsSize() ==
                std::vector<GUInt64>{static_cast<GUInt64>(nExtraDimSamples)})
            {
                bOK = true;
            }
            nStartIndex = nBand - 1;
        }
        if (bOK)
        {
            const auto oStringDT = GDALExtendedDataType::CreateString();
            const size_t nCount = 1;
            const GInt64 arrayStep = 1;
            const GPtrDiff_t bufferStride = 1;
            char *pszValue = nullptr;
            poAttr->Read(&nStartIndex, &nCount, &arrayStep, &bufferStride,
                         oStringDT, &pszValue);
            if (pszValue)
            {
                const auto eColorInterp =
                    GDALGetColorInterpretationByName(pszValue);
                CPLFree(pszValue);
                return eColorInterp;
            }
        }
    }
    return GCI_Undefined;
}

/************************************************************************/
/*             GDALRasterBandFromArray::GetOverviewCount()              */
/************************************************************************/

int GDALRasterBandFromArray::GetOverviewCount()
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    if (l_poDS->HasRegisteredPAMOverviewFile())
    {
        const int nPAMCount = GDALPamRasterBand::GetOverviewCount();
        if (nPAMCount)
            return nPAMCount;
    }
    l_poDS->DiscoverOverviews();
    return static_cast<int>(l_poDS->m_apoOverviews.size());
}

/************************************************************************/
/*                GDALRasterBandFromArray::GetOverview()                */
/************************************************************************/

GDALRasterBand *GDALRasterBandFromArray::GetOverview(int idx)
{
    auto l_poDS(cpl::down_cast<GDALDatasetFromArray *>(poDS));
    if (l_poDS->HasRegisteredPAMOverviewFile())
    {
        const int nPAMCount = GDALPamRasterBand::GetOverviewCount();
        if (nPAMCount)
            return GDALPamRasterBand::GetOverview(idx);
    }
    l_poDS->DiscoverOverviews();
    if (idx < 0 || static_cast<size_t>(idx) >= l_poDS->m_apoOverviews.size())
    {
        return nullptr;
    }
    return l_poDS->m_apoOverviews[idx]->GetRasterBand(nBand);
}

/************************************************************************/
/*                    GDALDatasetFromArray::Create()                    */
/************************************************************************/

std::unique_ptr<GDALDatasetFromArray> GDALDatasetFromArray::Create(
    const std::shared_ptr<GDALMDArray> &array, size_t iXDim, size_t iYDim,
    const std::shared_ptr<GDALGroup> &poRootGroup, CSLConstList papszOptions)

{
    const auto nDimCount(array->GetDimensionCount());
    if (nDimCount == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported number of dimensions");
        return nullptr;
    }
    if (array->GetDataType().GetClass() != GEDTC_NUMERIC ||
        array->GetDataType().GetNumericDataType() == GDT_Unknown)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only arrays with numeric data types "
                 "can be exposed as classic GDALDataset");
        return nullptr;
    }
    if (iXDim >= nDimCount || iYDim >= nDimCount ||
        (nDimCount >= 2 && iXDim == iYDim))
    {
        CPLError(CE_Failure, CPLE_NotSupported, "Invalid iXDim and/or iYDim");
        return nullptr;
    }
    GUInt64 nTotalBands = 1;
    const auto &dims(array->GetDimensions());
    for (size_t i = 0; i < nDimCount; ++i)
    {
        if (i != iXDim && !(nDimCount >= 2 && i == iYDim))
        {
            if (dims[i]->GetSize() > 65536 / nTotalBands)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too many bands. Operate on a sliced view");
                return nullptr;
            }
            nTotalBands *= dims[i]->GetSize();
        }
    }

    std::map<std::string, size_t> oMapArrayDimNameToExtraDimIdx;
    std::vector<size_t> oMapArrayExtraDimIdxToOriginalIdx;
    for (size_t i = 0, j = 0; i < nDimCount; ++i)
    {
        if (i != iXDim && !(nDimCount >= 2 && i == iYDim))
        {
            oMapArrayDimNameToExtraDimIdx[dims[i]->GetName()] = j;
            oMapArrayExtraDimIdxToOriginalIdx.push_back(i);
            ++j;
        }
    }

    const size_t nNewDimCount = nDimCount >= 2 ? nDimCount - 2 : 0;

    const char *pszBandMetadata =
        CSLFetchNameValue(papszOptions, "BAND_METADATA");
    std::vector<std::vector<MetadataItem>> aoBandParameterMetadataItems(
        nNewDimCount);
    if (pszBandMetadata)
    {
        if (!poRootGroup)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Root group should be provided when BAND_METADATA is set");
            return nullptr;
        }
        CPLJSONDocument oDoc;
        if (!oDoc.LoadMemory(pszBandMetadata))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid JSON content for BAND_METADATA");
            return nullptr;
        }
        auto oRoot = oDoc.GetRoot();
        if (oRoot.GetType() != CPLJSONObject::Type::Array)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Value of BAND_METADATA should be an array");
            return nullptr;
        }

        auto oArray = oRoot.ToArray();
        for (int j = 0; j < oArray.Size(); ++j)
        {
            const auto oJsonItem = oArray[j];
            MetadataItem oItem;
            size_t iExtraDimIdx = 0;

            const auto osBandArrayFullname = oJsonItem.GetString("array");
            const auto osBandAttributeName = oJsonItem.GetString("attribute");
            std::shared_ptr<GDALMDArray> poArray;
            std::shared_ptr<GDALAttribute> poAttribute;
            if (osBandArrayFullname.empty() && osBandAttributeName.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "BAND_METADATA[%d][\"array\"] or "
                         "BAND_METADATA[%d][\"attribute\"] is missing",
                         j, j);
                return nullptr;
            }
            else if (!osBandArrayFullname.empty() &&
                     !osBandAttributeName.empty())
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "BAND_METADATA[%d][\"array\"] and "
                    "BAND_METADATA[%d][\"attribute\"] are mutually exclusive",
                    j, j);
                return nullptr;
            }
            else if (!osBandArrayFullname.empty())
            {
                poArray =
                    poRootGroup->OpenMDArrayFromFullname(osBandArrayFullname);
                if (!poArray)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Array %s cannot be found",
                             osBandArrayFullname.c_str());
                    return nullptr;
                }
                if (poArray->GetDimensionCount() != 1)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Array %s is not a 1D array",
                             osBandArrayFullname.c_str());
                    return nullptr;
                }
                const auto &osAuxArrayDimName =
                    poArray->GetDimensions()[0]->GetName();
                auto oIter =
                    oMapArrayDimNameToExtraDimIdx.find(osAuxArrayDimName);
                if (oIter == oMapArrayDimNameToExtraDimIdx.end())
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Dimension %s of array %s is not a non-X/Y dimension "
                        "of array %s",
                        osAuxArrayDimName.c_str(), osBandArrayFullname.c_str(),
                        array->GetName().c_str());
                    return nullptr;
                }
                iExtraDimIdx = oIter->second;
                CPLAssert(iExtraDimIdx < nNewDimCount);
            }
            else
            {
                CPLAssert(!osBandAttributeName.empty());
                poAttribute = !osBandAttributeName.empty() &&
                                      osBandAttributeName[0] == '/'
                                  ? poRootGroup->OpenAttributeFromFullname(
                                        osBandAttributeName)
                                  : array->GetAttribute(osBandAttributeName);
                if (!poAttribute)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Attribute %s cannot be found",
                             osBandAttributeName.c_str());
                    return nullptr;
                }
                const auto aoAttrDims = poAttribute->GetDimensionsSize();
                if (aoAttrDims.size() != 1)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Attribute %s is not a 1D array",
                             osBandAttributeName.c_str());
                    return nullptr;
                }
                bool found = false;
                for (const auto &iter : oMapArrayDimNameToExtraDimIdx)
                {
                    if (dims[oMapArrayExtraDimIdxToOriginalIdx[iter.second]]
                            ->GetSize() == aoAttrDims[0])
                    {
                        if (found)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Several dimensions of %s have the same "
                                     "size as attribute %s. Cannot infer which "
                                     "one to bind to!",
                                     array->GetName().c_str(),
                                     osBandAttributeName.c_str());
                            return nullptr;
                        }
                        found = true;
                        iExtraDimIdx = iter.second;
                    }
                }
                if (!found)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "No dimension of %s has the same size as attribute %s",
                        array->GetName().c_str(), osBandAttributeName.c_str());
                    return nullptr;
                }
            }

            oItem.osName = oJsonItem.GetString("item_name");
            if (oItem.osName.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "BAND_METADATA[%d][\"item_name\"] is missing", j);
                return nullptr;
            }

            const auto osDefinition = oJsonItem.GetString("item_value", "%s");

            // Check correctness of definition
            bool bFirstNumericFormatter = true;
            std::string osModDefinition;
            bool bDefinitionUsesPctForG = false;
            for (size_t k = 0; k < osDefinition.size(); ++k)
            {
                if (osDefinition[k] == '%')
                {
                    osModDefinition += osDefinition[k];
                    if (k + 1 == osDefinition.size())
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Value of "
                                 "BAND_METADATA[%d][\"item_value\"] = "
                                 "%s is invalid at offset %d",
                                 j, osDefinition.c_str(), int(k));
                        return nullptr;
                    }
                    ++k;
                    if (osDefinition[k] == '%')
                    {
                        osModDefinition += osDefinition[k];
                        continue;
                    }
                    if (!bFirstNumericFormatter)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Value of "
                                 "BAND_METADATA[%d][\"item_value\"] = %s is "
                                 "invalid at offset %d: %%[x][.y]f|g or %%s "
                                 "formatters should be specified at most once",
                                 j, osDefinition.c_str(), int(k));
                        return nullptr;
                    }
                    bFirstNumericFormatter = false;
                    for (; k < osDefinition.size(); ++k)
                    {
                        osModDefinition += osDefinition[k];
                        if (!((osDefinition[k] >= '0' &&
                               osDefinition[k] <= '9') ||
                              osDefinition[k] == '.'))
                            break;
                    }
                    if (k == osDefinition.size() ||
                        (osDefinition[k] != 'f' && osDefinition[k] != 'g' &&
                         osDefinition[k] != 's'))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Value of "
                                 "BAND_METADATA[%d][\"item_value\"] = "
                                 "%s is invalid at offset %d: only "
                                 "%%[x][.y]f|g or %%s formatters are accepted",
                                 j, osDefinition.c_str(), int(k));
                        return nullptr;
                    }
                    bDefinitionUsesPctForG =
                        (osDefinition[k] == 'f' || osDefinition[k] == 'g');
                    if (bDefinitionUsesPctForG)
                    {
                        if (poArray &&
                            poArray->GetDataType().GetClass() != GEDTC_NUMERIC)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Data type of %s array is not numeric",
                                     poArray->GetName().c_str());
                            return nullptr;
                        }
                        else if (poAttribute &&
                                 poAttribute->GetDataType().GetClass() !=
                                     GEDTC_NUMERIC)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Data type of %s attribute is not numeric",
                                     poAttribute->GetFullName().c_str());
                            return nullptr;
                        }
                    }
                }
                else if (osDefinition[k] == '$' &&
                         k + 1 < osDefinition.size() &&
                         osDefinition[k + 1] == '{')
                {
                    const auto nPos = osDefinition.find('}', k);
                    if (nPos == std::string::npos)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Value of "
                                 "BAND_METADATA[%d][\"item_value\"] = "
                                 "%s is invalid at offset %d",
                                 j, osDefinition.c_str(), int(k));
                        return nullptr;
                    }
                    const auto osAttrName =
                        osDefinition.substr(k + 2, nPos - (k + 2));
                    std::shared_ptr<GDALAttribute> poAttr;
                    if (poArray && !osAttrName.empty() && osAttrName[0] != '/')
                    {
                        poAttr = poArray->GetAttribute(osAttrName);
                        if (!poAttr)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Value of "
                                "BAND_METADATA[%d][\"item_value\"] = "
                                "%s is invalid: %s is not an attribute of %s",
                                j, osDefinition.c_str(), osAttrName.c_str(),
                                poArray->GetName().c_str());
                            return nullptr;
                        }
                    }
                    else
                    {
                        poAttr =
                            poRootGroup->OpenAttributeFromFullname(osAttrName);
                        if (!poAttr)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Value of "
                                     "BAND_METADATA[%d][\"item_value\"] = "
                                     "%s is invalid: %s is not an attribute",
                                     j, osDefinition.c_str(),
                                     osAttrName.c_str());
                            return nullptr;
                        }
                    }
                    k = nPos;
                    const char *pszValue = poAttr->ReadAsString();
                    if (!pszValue)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot get value of attribute %s as a "
                                 "string",
                                 osAttrName.c_str());
                        return nullptr;
                    }
                    osModDefinition += pszValue;
                }
                else
                {
                    osModDefinition += osDefinition[k];
                }
            }

            if (poArray)
                oItem.poArray = std::move(poArray);
            else
                oItem.poArray = std::move(poAttribute);
            oItem.osDefinition = std::move(osModDefinition);
            oItem.bDefinitionUsesPctForG = bDefinitionUsesPctForG;

            aoBandParameterMetadataItems[iExtraDimIdx].emplace_back(
                std::move(oItem));
        }
    }

    std::vector<BandImageryMetadata> aoBandImageryMetadata(nNewDimCount);
    const char *pszBandImageryMetadata =
        CSLFetchNameValue(papszOptions, "BAND_IMAGERY_METADATA");
    if (pszBandImageryMetadata)
    {
        if (!poRootGroup)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Root group should be provided when BAND_IMAGERY_METADATA "
                     "is set");
            return nullptr;
        }
        CPLJSONDocument oDoc;
        if (!oDoc.LoadMemory(pszBandImageryMetadata))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid JSON content for BAND_IMAGERY_METADATA");
            return nullptr;
        }
        auto oRoot = oDoc.GetRoot();
        if (oRoot.GetType() != CPLJSONObject::Type::Object)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Value of BAND_IMAGERY_METADATA should be an object");
            return nullptr;
        }
        for (const auto &oJsonItem : oRoot.GetChildren())
        {
            if (oJsonItem.GetName() == "CENTRAL_WAVELENGTH_UM" ||
                oJsonItem.GetName() == "FWHM_UM")
            {
                const auto osBandArrayFullname = oJsonItem.GetString("array");
                const auto osBandAttributeName =
                    oJsonItem.GetString("attribute");
                std::shared_ptr<GDALMDArray> poArray;
                std::shared_ptr<GDALAttribute> poAttribute;
                size_t iExtraDimIdx = 0;
                if (osBandArrayFullname.empty() && osBandAttributeName.empty())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "BAND_IMAGERY_METADATA[\"%s\"][\"array\"] or "
                             "BAND_IMAGERY_METADATA[\"%s\"][\"attribute\"] is "
                             "missing",
                             oJsonItem.GetName().c_str(),
                             oJsonItem.GetName().c_str());
                    return nullptr;
                }
                else if (!osBandArrayFullname.empty() &&
                         !osBandAttributeName.empty())
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "BAND_IMAGERY_METADATA[\"%s\"][\"array\"] and "
                             "BAND_IMAGERY_METADATA[\"%s\"][\"attribute\"] are "
                             "mutually exclusive",
                             oJsonItem.GetName().c_str(),
                             oJsonItem.GetName().c_str());
                    return nullptr;
                }
                else if (!osBandArrayFullname.empty())
                {
                    poArray = poRootGroup->OpenMDArrayFromFullname(
                        osBandArrayFullname);
                    if (!poArray)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Array %s cannot be found",
                                 osBandArrayFullname.c_str());
                        return nullptr;
                    }
                    if (poArray->GetDimensionCount() != 1)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Array %s is not a 1D array",
                                 osBandArrayFullname.c_str());
                        return nullptr;
                    }
                    const auto &osAuxArrayDimName =
                        poArray->GetDimensions()[0]->GetName();
                    auto oIter =
                        oMapArrayDimNameToExtraDimIdx.find(osAuxArrayDimName);
                    if (oIter == oMapArrayDimNameToExtraDimIdx.end())
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Dimension \"%s\" of array \"%s\" is not a "
                                 "non-X/Y dimension of array \"%s\"",
                                 osAuxArrayDimName.c_str(),
                                 osBandArrayFullname.c_str(),
                                 array->GetName().c_str());
                        return nullptr;
                    }
                    iExtraDimIdx = oIter->second;
                    CPLAssert(iExtraDimIdx < nNewDimCount);
                }
                else
                {
                    poAttribute =
                        !osBandAttributeName.empty() &&
                                osBandAttributeName[0] == '/'
                            ? poRootGroup->OpenAttributeFromFullname(
                                  osBandAttributeName)
                            : array->GetAttribute(osBandAttributeName);
                    if (!poAttribute)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Attribute %s cannot be found",
                                 osBandAttributeName.c_str());
                        return nullptr;
                    }
                    const auto aoAttrDims = poAttribute->GetDimensionsSize();
                    if (aoAttrDims.size() != 1)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Attribute %s is not a 1D array",
                                 osBandAttributeName.c_str());
                        return nullptr;
                    }
                    bool found = false;
                    for (const auto &iter : oMapArrayDimNameToExtraDimIdx)
                    {
                        if (dims[oMapArrayExtraDimIdxToOriginalIdx[iter.second]]
                                ->GetSize() == aoAttrDims[0])
                        {
                            if (found)
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "Several dimensions of %s have the "
                                         "same size as attribute %s. Cannot "
                                         "infer which one to bind to!",
                                         array->GetName().c_str(),
                                         osBandAttributeName.c_str());
                                return nullptr;
                            }
                            found = true;
                            iExtraDimIdx = iter.second;
                        }
                    }
                    if (!found)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "No dimension of %s has the same size as "
                                 "attribute %s",
                                 array->GetName().c_str(),
                                 osBandAttributeName.c_str());
                        return nullptr;
                    }
                }

                std::string osUnit = oJsonItem.GetString("unit", "um");
                if (STARTS_WITH(osUnit.c_str(), "${"))
                {
                    if (osUnit.back() != '}')
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Value of "
                                 "BAND_IMAGERY_METADATA[\"%s\"][\"unit\"] = "
                                 "%s is invalid",
                                 oJsonItem.GetName().c_str(), osUnit.c_str());
                        return nullptr;
                    }
                    const auto osAttrName = osUnit.substr(2, osUnit.size() - 3);
                    std::shared_ptr<GDALAttribute> poAttr;
                    if (poArray && !osAttrName.empty() && osAttrName[0] != '/')
                    {
                        poAttr = poArray->GetAttribute(osAttrName);
                        if (!poAttr)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Value of "
                                "BAND_IMAGERY_METADATA[\"%s\"][\"unit\"] = "
                                "%s is invalid: %s is not an attribute of %s",
                                oJsonItem.GetName().c_str(), osUnit.c_str(),
                                osAttrName.c_str(),
                                osBandArrayFullname.c_str());
                            return nullptr;
                        }
                    }
                    else
                    {
                        poAttr =
                            poRootGroup->OpenAttributeFromFullname(osAttrName);
                        if (!poAttr)
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "Value of "
                                "BAND_IMAGERY_METADATA[\"%s\"][\"unit\"] = "
                                "%s is invalid: %s is not an attribute",
                                oJsonItem.GetName().c_str(), osUnit.c_str(),
                                osAttrName.c_str());
                            return nullptr;
                        }
                    }

                    const char *pszValue = poAttr->ReadAsString();
                    if (!pszValue)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot get value of attribute %s of %s as a "
                                 "string",
                                 osAttrName.c_str(),
                                 osBandArrayFullname.c_str());
                        return nullptr;
                    }
                    osUnit = pszValue;
                }
                double dfConvToUM = 1.0;
                if (osUnit == "nm" || osUnit == "nanometre" ||
                    osUnit == "nanometres" || osUnit == "nanometer" ||
                    osUnit == "nanometers")
                {
                    dfConvToUM = 1e-3;
                }
                else if (!(osUnit == "um" || osUnit == "micrometre" ||
                           osUnit == "micrometres" || osUnit == "micrometer" ||
                           osUnit == "micrometers"))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unhandled value for "
                             "BAND_IMAGERY_METADATA[\"%s\"][\"unit\"] = %s",
                             oJsonItem.GetName().c_str(), osUnit.c_str());
                    return nullptr;
                }

                BandImageryMetadata &item = aoBandImageryMetadata[iExtraDimIdx];

                std::shared_ptr<GDALAbstractMDArray> abstractArray;
                if (poArray)
                    abstractArray = std::move(poArray);
                else
                    abstractArray = std::move(poAttribute);
                if (oJsonItem.GetName() == "CENTRAL_WAVELENGTH_UM")
                {
                    item.poCentralWavelengthArray = std::move(abstractArray);
                    item.dfCentralWavelengthToMicrometer = dfConvToUM;
                }
                else
                {
                    item.poFWHMArray = std::move(abstractArray);
                    item.dfFWHMToMicrometer = dfConvToUM;
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Ignored member \"%s\" in BAND_IMAGERY_METADATA",
                         oJsonItem.GetName().c_str());
            }
        }
    }

    if ((nDimCount >= 2 &&
         dims[iYDim]->GetSize() > static_cast<uint64_t>(INT_MAX)) ||
        dims[iXDim]->GetSize() > static_cast<uint64_t>(INT_MAX))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array is too large to be exposed as a GDAL dataset");
        return nullptr;
    }

    auto poDS = std::make_unique<GDALDatasetFromArray>(
        array, iXDim, iYDim, CPLStringList(papszOptions));

    poDS->eAccess = array->IsWritable() ? GA_Update : GA_ReadOnly;

    poDS->nRasterYSize =
        nDimCount < 2 ? 1 : static_cast<int>(dims[iYDim]->GetSize());

    poDS->nRasterXSize = static_cast<int>(dims[iXDim]->GetSize());

    std::vector<GUInt64> anOtherDimCoord(nNewDimCount);
    std::vector<GUInt64> anStackIters(nDimCount);
    std::vector<size_t> anMapNewToOld(nNewDimCount);
    for (size_t i = 0, j = 0; i < nDimCount; ++i)
    {
        if (i != iXDim && !(nDimCount >= 2 && i == iYDim))
        {
            anMapNewToOld[j] = i;
            j++;
        }
    }

    poDS->m_bHasGT = array->GuessGeoTransform(iXDim, iYDim, false, poDS->m_gt);

    const auto attrs(array->GetAttributes());
    for (const auto &attr : attrs)
    {
        if (attr->GetName() == "spatial:registration")
        {
            // From https://github.com/zarr-conventions/spatial
            const char *pszValue = attr->ReadAsString();
            if (pszValue && strcmp(pszValue, "pixel") == 0)
                poDS->m_oMDD.SetMetadataItem(GDALMD_AREA_OR_POINT,
                                             GDALMD_AOP_AREA);
            else if (pszValue && strcmp(pszValue, "node") == 0)
                poDS->m_oMDD.SetMetadataItem(GDALMD_AREA_OR_POINT,
                                             GDALMD_AOP_POINT);
            else if (pszValue)
                poDS->m_oMDD.SetMetadataItem(attr->GetName().c_str(), pszValue);
        }
        else if (attr->GetName() == "gdal:geotransform")
        {
            // From Zarr driver
            const auto doubleArray = attr->ReadAsDoubleArray();
            if (doubleArray.size() == 6)
            {
                poDS->m_bHasGT = true;
                poDS->m_gt = GDALGeoTransform(doubleArray.data());
            }
        }
        else if (attr->GetName() != "COLOR_INTERPRETATION")
        {
            auto stringArray = attr->ReadAsStringArray();
            std::string val;
            if (stringArray.size() > 1)
            {
                val += '{';
            }
            for (int i = 0; i < stringArray.size(); ++i)
            {
                if (i > 0)
                    val += ',';
                val += stringArray[i];
            }
            if (stringArray.size() > 1)
            {
                val += '}';
            }
            poDS->m_oMDD.SetMetadataItem(attr->GetName().c_str(), val.c_str());
        }
    }

    const char *pszDelay = CSLFetchNameValueDef(
        papszOptions, "LOAD_EXTRA_DIM_METADATA_DELAY",
        CPLGetConfigOption("GDAL_LOAD_EXTRA_DIM_METADATA_DELAY", "5"));
    const double dfDelay =
        EQUAL(pszDelay, "unlimited") ? -1 : CPLAtof(pszDelay);
    const auto nStartTime = time(nullptr);
    bool bHasWarned = false;
    // Instantiate bands by iterating over non-XY variables
    size_t iDim = 0;
    int nCurBand = 1;
lbl_next_depth:
    if (iDim < nNewDimCount)
    {
        anStackIters[iDim] = dims[anMapNewToOld[iDim]]->GetSize();
        anOtherDimCoord[iDim] = 0;
        while (true)
        {
            ++iDim;
            goto lbl_next_depth;
        lbl_return_to_caller:
            --iDim;
            --anStackIters[iDim];
            if (anStackIters[iDim] == 0)
                break;
            ++anOtherDimCoord[iDim];
        }
    }
    else
    {
        poDS->SetBand(nCurBand,
                      std::make_unique<GDALRasterBandFromArray>(
                          poDS.get(), anOtherDimCoord,
                          aoBandParameterMetadataItems, aoBandImageryMetadata,
                          dfDelay, nStartTime, bHasWarned));
        ++nCurBand;
    }
    if (iDim > 0)
        goto lbl_return_to_caller;

    if (nDimCount == 3 && iXDim <= 1 && iYDim <= 1 &&
        poDS->GetRasterCount() > 1)
    {
        poDS->m_bPixelInterleaved = true;
        poDS->m_oMDD.SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }

    if (!array->GetFilename().empty() &&
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "LOAD_PAM", "YES")))
    {
        poDS->SetPhysicalFilename(array->GetFilename().c_str());
        std::string osDerivedDatasetName(
            CPLSPrintf("AsClassicDataset(%d,%d) view of %s", int(iXDim),
                       int(iYDim), array->GetFullName().c_str()));
        if (!array->GetContext().empty())
        {
            osDerivedDatasetName += " with context ";
            osDerivedDatasetName += array->GetContext();
        }
        poDS->SetDerivedDatasetName(osDerivedDatasetName.c_str());
        poDS->TryLoadXML();

        for (const auto &[pszKey, pszValue] :
             cpl::IterateNameValue(static_cast<CSLConstList>(
                 poDS->GDALPamDataset::GetMetadata())))
        {
            poDS->m_oMDD.SetMetadataItem(pszKey, pszValue);
        }
    }

    return poDS;
}

//! @endcond

/************************************************************************/
/*                          AsClassicDataset()                          */
/************************************************************************/

/** Return a view of this array as a "classic" GDALDataset (ie 2D)
 *
 * In the case of > 2D arrays, additional dimensions will be represented as
 * raster bands.
 *
 * The "reverse" method is GDALRasterBand::AsMDArray().
 *
 * This is the same as the C function GDALMDArrayAsClassicDataset().
 *
 * @param iXDim Index of the dimension that will be used as the X/width axis.
 * @param iYDim Index of the dimension that will be used as the Y/height axis.
 *              Ignored if the dimension count is 1.
 * @param poRootGroup (Added in GDAL 3.8) Root group. Used with the BAND_METADATA
 *                    and BAND_IMAGERY_METADATA option.
 * @param papszOptions (Added in GDAL 3.8) Null-terminated list of options, or
 *                     nullptr. Current supported options are:
 *                     <ul>
 *                     <li>BAND_METADATA: JSON serialized array defining which
 *                         arrays of the poRootGroup, indexed by non-X and Y
 *                         dimensions, should be mapped as band metadata items.
 *                         Each array item should be an object with the
 *                         following members:
 *                         - "array": full name of a band parameter array.
 *                           Such array must be a one
 *                           dimensional array, and its dimension must be one of
 *                           the dimensions of the array on which the method is
 *                           called (excluding the X and Y dimensions).
 *                         - "attribute": name relative to *this array or full
 *                           name of a single dimension numeric array whose size
 *                           must be one of the dimensions of *this array
 *                           (excluding the X and Y dimensions).
 *                           "array" and "attribute" are mutually exclusive.
 *                         - "item_name": band metadata item name
 *                         - "item_value": (optional) String, where "%[x][.y]f",
 *                           "%[x][.y]g" or "%s" printf-like formatting can be
 *                           used to format the corresponding value of the
 *                           parameter array. The percentage character should be
 *                           repeated: "%%"
 *                           "${attribute_name}" can also be used to include the
 *                           value of an attribute for "array" when set and if
 *                           not starting with '/'. Otherwise if starting with
 *                           '/', it is the full path to the attribute.
 *
 *                           If "item_value" is not provided, a default formatting
 *                           of the value will be applied.
 *
 *                         Example:
 *                         [
 *                            {
 *                              "array": "/sensor_band_parameters/wavelengths",
 *                              "item_name": "WAVELENGTH",
 *                              "item_value": "%.1f ${units}"
 *                            },
 *                            {
 *                              "array": "/sensor_band_parameters/fwhm",
 *                              "item_name": "FWHM"
 *                            },
 *                            {
 *                              "array": "/sensor_band_parameters/fwhm",
 *                              "item_name": "FWHM_UNIT",
 *                              "item_value": "${units}"
 *                            }
 *                         ]
 *
 *                         Example for Planet Labs Tanager radiance products:
 *                         [
 *                            {
 *                              "attribute": "center_wavelengths",
 *                              "item_name": "WAVELENGTH",
 *                              "item_value": "%.1f ${center_wavelengths_units}"
 *                            }
 *                         ]
 *
 *                     </li>
 *                     <li>BAND_IMAGERY_METADATA: (GDAL >= 3.11)
 *                         JSON serialized object defining which arrays of the
 *                         poRootGroup, indexed by non-X and Y dimensions,
 *                         should be mapped as band metadata items in the
 *                         band IMAGERY domain.
 *                         The object currently accepts 2 members:
 *                         - "CENTRAL_WAVELENGTH_UM": Central Wavelength in
 *                           micrometers.
 *                         - "FWHM_UM": Full-width half-maximum
 *                           in micrometers.
 *                         The value of each member should be an object with the
 *                         following members:
 *                         - "array": full name of a band parameter array.
 *                           Such array must be a one dimensional array, and its
 *                           dimension must be one of the dimensions of the
 *                           array on which the method is called
 *                           (excluding the X and Y dimensions).
 *                         - "attribute": name relative to *this array or full
 *                           name of a single dimension numeric array whose size
 *                           must be one of the dimensions of *this array
 *                           (excluding the X and Y dimensions).
 *                           "array" and "attribute" are mutually exclusive,
 *                           and one of them is required.
 *                         - "unit": (optional) unit of the values pointed in
 *                           the above array.
 *                           Can be a literal string or a string of the form
 *                           "${attribute_name}" to point to an attribute for
 *                           "array" when set and if no starting
 *                           with '/'. Otherwise if starting with '/', it is
 *                           the full path to the attribute.
 *                           Accepted values are "um", "micrometer"
 *                           (with UK vs US spelling, singular or plural), "nm",
 *                           "nanometer" (with UK vs US spelling, singular or
 *                           plural)
 *                           If not provided, micrometer is assumed.
 *
 *                         Example for EMIT datasets:
 *                         {
 *                            "CENTRAL_WAVELENGTH_UM": {
 *                                "array": "/sensor_band_parameters/wavelengths",
 *                                "unit": "${units}"
 *                            },
 *                            "FWHM_UM": {
 *                                "array": "/sensor_band_parameters/fwhm",
 *                                "unit": "${units}"
 *                            }
 *                         }
 *
 *                         Example for Planet Labs Tanager radiance products:
 *                         {
 *                            "CENTRAL_WAVELENGTH_UM": {
 *                              "attribute": "center_wavelengths",
 *                              "unit": "${center_wavelengths_units}"
 *                            },
 *                            "FWHM_UM": {
 *                              "attribute": "fwhm",
 *                              "unit": "${fwhm_units}"
 *                            }
 *                         }
 *
 *                     </li>
 *                     <li>LOAD_EXTRA_DIM_METADATA_DELAY: Maximum delay in
 *                         seconds allowed to set the DIM_{dimname}_VALUE band
 *                         metadata items from the indexing variable of the
 *                         dimensions.
 *                         Default value is 5. 'unlimited' can be used to mean
 *                         unlimited delay. Can also be defined globally with
 *                         the GDAL_LOAD_EXTRA_DIM_METADATA_DELAY configuration
 *                         option.</li>
 *                     </ul>
 * @return a new GDALDataset that must be freed with GDALClose(), or nullptr
 */
GDALDataset *
GDALMDArray::AsClassicDataset(size_t iXDim, size_t iYDim,
                              const std::shared_ptr<GDALGroup> &poRootGroup,
                              CSLConstList papszOptions) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if (!self)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }
    return GDALDatasetFromArray::Create(self, iXDim, iYDim, poRootGroup,
                                        papszOptions)
        .release();
}
