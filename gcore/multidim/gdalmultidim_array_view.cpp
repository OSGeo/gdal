/******************************************************************************
 *
 * Name:     gdalmultidim_array_view.cpp
 * Project:  GDAL Core
 * Purpose:  GDALMDArray::GetView() implementation
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_multidim.h"
#include "gdal_pam_multidim.h"
#include "ogr_spatialref.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                          GDALSlicedMDArray                           */
/************************************************************************/

class GDALSlicedMDArray final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};
    std::vector<size_t> m_mapDimIdxToParentDimIdx{};  // of size m_dims.size()
    std::vector<std::shared_ptr<GDALMDArray>> m_apoNewIndexingVariables{};
    std::vector<Range>
        m_parentRanges{};  // of size m_poParent->GetDimensionCount()

    mutable std::vector<GUInt64> m_parentStart;
    mutable std::vector<size_t> m_parentCount;
    mutable std::vector<GInt64> m_parentStep;
    mutable std::vector<GPtrDiff_t> m_parentStride;

    void PrepareParentArrays(const GUInt64 *arrayStartIdx, const size_t *count,
                             const GInt64 *arrayStep,
                             const GPtrDiff_t *bufferStride) const;

  protected:
    explicit GDALSlicedMDArray(
        const std::shared_ptr<GDALMDArray> &poParent,
        const std::string &viewExpr,
        std::vector<std::shared_ptr<GDALDimension>> &&dims,
        std::vector<size_t> &&mapDimIdxToParentDimIdx,
        std::vector<std::shared_ptr<GDALMDArray>> &&apoNewIndexingVariables,
        std::vector<Range> &&parentRanges)
        : GDALAbstractMDArray(std::string(), "Sliced view of " +
                                                 poParent->GetFullName() +
                                                 " (" + viewExpr + ")"),
          GDALPamMDArray(std::string(),
                         "Sliced view of " + poParent->GetFullName() + " (" +
                             viewExpr + ")",
                         GDALPamMultiDim::GetPAM(poParent),
                         poParent->GetContext()),
          m_poParent(std::move(poParent)), m_dims(std::move(dims)),
          m_mapDimIdxToParentDimIdx(std::move(mapDimIdxToParentDimIdx)),
          m_apoNewIndexingVariables(std::move(apoNewIndexingVariables)),
          m_parentRanges(std::move(parentRanges)),
          m_parentStart(m_poParent->GetDimensionCount()),
          m_parentCount(m_poParent->GetDimensionCount(), 1),
          m_parentStep(m_poParent->GetDimensionCount()),
          m_parentStride(m_poParent->GetDimensionCount())
    {
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

    bool IWrite(const GUInt64 *arrayStartIdx, const size_t *count,
                const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
                const GDALExtendedDataType &bufferDataType,
                const void *pSrcBuffer) override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override;

  public:
    static std::shared_ptr<GDALSlicedMDArray>
    Create(const std::shared_ptr<GDALMDArray> &poParent,
           const std::string &viewExpr,
           std::vector<std::shared_ptr<GDALDimension>> &&dims,
           std::vector<size_t> &&mapDimIdxToParentDimIdx,
           std::vector<std::shared_ptr<GDALMDArray>> &&apoNewIndexingVariables,
           std::vector<Range> &&parentRanges)
    {
        CPLAssert(dims.size() == mapDimIdxToParentDimIdx.size());
        CPLAssert(parentRanges.size() == poParent->GetDimensionCount());

        auto newAr(std::shared_ptr<GDALSlicedMDArray>(new GDALSlicedMDArray(
            poParent, viewExpr, std::move(dims),
            std::move(mapDimIdxToParentDimIdx),
            std::move(apoNewIndexingVariables), std::move(parentRanges))));
        newAr->SetSelf(newAr);
        return newAr;
    }

    bool IsWritable() const override
    {
        return m_poParent->IsWritable();
    }

    const std::string &GetFilename() const override
    {
        return m_poParent->GetFilename();
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_dims;
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_poParent->GetDataType();
    }

    const std::string &GetUnit() const override
    {
        return m_poParent->GetUnit();
    }

    // bool SetUnit(const std::string& osUnit) override  { return
    // m_poParent->SetUnit(osUnit); }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        auto poSrcSRS = m_poParent->GetSpatialRef();
        if (!poSrcSRS)
            return nullptr;
        auto srcMapping = poSrcSRS->GetDataAxisToSRSAxisMapping();
        std::vector<int> dstMapping;
        for (int srcAxis : srcMapping)
        {
            bool bFound = false;
            for (size_t i = 0; i < m_mapDimIdxToParentDimIdx.size(); i++)
            {
                if (static_cast<int>(m_mapDimIdxToParentDimIdx[i]) ==
                    srcAxis - 1)
                {
                    dstMapping.push_back(static_cast<int>(i) + 1);
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                dstMapping.push_back(0);
            }
        }
        auto poClone(std::shared_ptr<OGRSpatialReference>(poSrcSRS->Clone()));
        poClone->SetDataAxisToSRSAxisMapping(dstMapping);
        return poClone;
    }

    const void *GetRawNoDataValue() const override
    {
        return m_poParent->GetRawNoDataValue();
    }

    // bool SetRawNoDataValue(const void* pRawNoData) override { return
    // m_poParent->SetRawNoDataValue(pRawNoData); }

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

    // bool SetOffset(double dfOffset) override { return
    // m_poParent->SetOffset(dfOffset); }

    // bool SetScale(double dfScale) override { return
    // m_poParent->SetScale(dfScale); }

    std::vector<GUInt64> GetBlockSize() const override
    {
        std::vector<GUInt64> ret(GetDimensionCount());
        const auto parentBlockSize(m_poParent->GetBlockSize());
        for (size_t i = 0; i < m_mapDimIdxToParentDimIdx.size(); ++i)
        {
            const auto iOldAxis = m_mapDimIdxToParentDimIdx[i];
            if (iOldAxis != static_cast<size_t>(-1))
            {
                ret[i] = parentBlockSize[iOldAxis];
            }
        }
        return ret;
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
};

/************************************************************************/
/*                        PrepareParentArrays()                         */
/************************************************************************/

void GDALSlicedMDArray::PrepareParentArrays(
    const GUInt64 *arrayStartIdx, const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride) const
{
    const size_t nParentDimCount = m_parentRanges.size();
    for (size_t i = 0; i < nParentDimCount; i++)
    {
        // For dimensions in parent that have no existence in sliced array
        m_parentStart[i] = m_parentRanges[i].m_nStartIdx;
    }

    for (size_t i = 0; i < m_dims.size(); i++)
    {
        const auto iParent = m_mapDimIdxToParentDimIdx[i];
        if (iParent != static_cast<size_t>(-1))
        {
            m_parentStart[iParent] =
                m_parentRanges[iParent].m_nIncr >= 0
                    ? m_parentRanges[iParent].m_nStartIdx +
                          arrayStartIdx[i] * m_parentRanges[iParent].m_nIncr
                    : m_parentRanges[iParent].m_nStartIdx -
                          arrayStartIdx[i] *
                              static_cast<GUInt64>(
                                  -m_parentRanges[iParent].m_nIncr);
            m_parentCount[iParent] = count[i];
            if (arrayStep)
            {
                m_parentStep[iParent] =
                    count[i] == 1 ? 1 :
                                  // other checks should have ensured this does
                        // not overflow
                        arrayStep[i] * m_parentRanges[iParent].m_nIncr;
            }
            if (bufferStride)
            {
                m_parentStride[iParent] = bufferStride[i];
            }
        }
    }
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool GDALSlicedMDArray::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                              const GInt64 *arrayStep,
                              const GPtrDiff_t *bufferStride,
                              const GDALExtendedDataType &bufferDataType,
                              void *pDstBuffer) const
{
    PrepareParentArrays(arrayStartIdx, count, arrayStep, bufferStride);
    return m_poParent->Read(m_parentStart.data(), m_parentCount.data(),
                            m_parentStep.data(), m_parentStride.data(),
                            bufferDataType, pDstBuffer);
}

/************************************************************************/
/*                               IWrite()                               */
/************************************************************************/

bool GDALSlicedMDArray::IWrite(const GUInt64 *arrayStartIdx,
                               const size_t *count, const GInt64 *arrayStep,
                               const GPtrDiff_t *bufferStride,
                               const GDALExtendedDataType &bufferDataType,
                               const void *pSrcBuffer)
{
    PrepareParentArrays(arrayStartIdx, count, arrayStep, bufferStride);
    return m_poParent->Write(m_parentStart.data(), m_parentCount.data(),
                             m_parentStep.data(), m_parentStride.data(),
                             bufferDataType, pSrcBuffer);
}

/************************************************************************/
/*                            IAdviseRead()                             */
/************************************************************************/

bool GDALSlicedMDArray::IAdviseRead(const GUInt64 *arrayStartIdx,
                                    const size_t *count,
                                    CSLConstList papszOptions) const
{
    PrepareParentArrays(arrayStartIdx, count, nullptr, nullptr);
    return m_poParent->AdviseRead(m_parentStart.data(), m_parentCount.data(),
                                  papszOptions);
}

/************************************************************************/
/*                         CreateSlicedArray()                          */
/************************************************************************/

static std::shared_ptr<GDALMDArray>
CreateSlicedArray(const std::shared_ptr<GDALMDArray> &self,
                  const std::string &viewExpr, const std::string &activeSlice,
                  bool bRenameDimensions,
                  std::vector<GDALMDArray::ViewSpec> &viewSpecs)
{
    const auto &srcDims(self->GetDimensions());
    if (srcDims.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot slice a 0-d array");
        return nullptr;
    }

    CPLStringList aosTokens(CSLTokenizeString2(activeSlice.c_str(), ",", 0));
    const auto nTokens = static_cast<size_t>(aosTokens.size());

    std::vector<std::shared_ptr<GDALDimension>> newDims;
    std::vector<size_t> mapDimIdxToParentDimIdx;
    std::vector<GDALSlicedMDArray::Range> parentRanges;
    newDims.reserve(nTokens);
    mapDimIdxToParentDimIdx.reserve(nTokens);
    parentRanges.reserve(nTokens);

    bool bGotEllipsis = false;
    size_t nCurSrcDim = 0;
    std::vector<std::shared_ptr<GDALMDArray>> apoNewIndexingVariables;
    for (size_t i = 0; i < nTokens; i++)
    {
        const char *pszIdxSpec = aosTokens[i];
        if (EQUAL(pszIdxSpec, "..."))
        {
            if (bGotEllipsis)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Only one single ellipsis is supported");
                return nullptr;
            }
            bGotEllipsis = true;
            const auto nSubstitutionCount = srcDims.size() - (nTokens - 1);
            for (size_t j = 0; j < nSubstitutionCount; j++, nCurSrcDim++)
            {
                parentRanges.emplace_back(0, 1);
                newDims.push_back(srcDims[nCurSrcDim]);
                mapDimIdxToParentDimIdx.push_back(nCurSrcDim);
            }
            continue;
        }
        else if (EQUAL(pszIdxSpec, "newaxis") ||
                 EQUAL(pszIdxSpec, "np.newaxis"))
        {
            newDims.push_back(std::make_shared<GDALDimension>(
                std::string(), "newaxis", std::string(), std::string(), 1));
            mapDimIdxToParentDimIdx.push_back(static_cast<size_t>(-1));
            continue;
        }
        else if (CPLGetValueType(pszIdxSpec) == CPL_VALUE_INTEGER)
        {
            if (nCurSrcDim >= srcDims.size())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too many values in %s",
                         activeSlice.c_str());
                return nullptr;
            }

            auto nVal = CPLAtoGIntBig(pszIdxSpec);
            GUInt64 nDimSize = srcDims[nCurSrcDim]->GetSize();
            if ((nVal >= 0 && static_cast<GUInt64>(nVal) >= nDimSize) ||
                (nVal < 0 && nDimSize < static_cast<GUInt64>(-nVal)))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Index " CPL_FRMT_GIB " is out of bounds", nVal);
                return nullptr;
            }
            if (nVal < 0)
                nVal += nDimSize;
            parentRanges.emplace_back(nVal, 0);
        }
        else
        {
            if (nCurSrcDim >= srcDims.size())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too many values in %s",
                         activeSlice.c_str());
                return nullptr;
            }

            CPLStringList aosRangeTokens(
                CSLTokenizeString2(pszIdxSpec, ":", CSLT_ALLOWEMPTYTOKENS));
            int nRangeTokens = aosRangeTokens.size();
            if (nRangeTokens > 3)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Too many : in %s",
                         pszIdxSpec);
                return nullptr;
            }
            if (nRangeTokens <= 1)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid value %s",
                         pszIdxSpec);
                return nullptr;
            }
            const char *pszStart = aosRangeTokens[0];
            const char *pszEnd = aosRangeTokens[1];
            const char *pszInc = (nRangeTokens == 3) ? aosRangeTokens[2] : "";
            GDALSlicedMDArray::Range range;
            const GUInt64 nDimSize(srcDims[nCurSrcDim]->GetSize());
            range.m_nIncr = EQUAL(pszInc, "") ? 1 : CPLAtoGIntBig(pszInc);
            if (range.m_nIncr == 0 ||
                range.m_nIncr == std::numeric_limits<GInt64>::min())
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid increment");
                return nullptr;
            }
            auto startIdx(CPLAtoGIntBig(pszStart));
            if (startIdx < 0)
            {
                if (nDimSize < static_cast<GUInt64>(-startIdx))
                    startIdx = 0;
                else
                    startIdx = nDimSize + startIdx;
            }
            const bool bPosIncr = range.m_nIncr > 0;
            range.m_nStartIdx = startIdx;
            range.m_nStartIdx = EQUAL(pszStart, "")
                                    ? (bPosIncr ? 0 : nDimSize - 1)
                                    : range.m_nStartIdx;
            if (range.m_nStartIdx >= nDimSize - 1)
                range.m_nStartIdx = nDimSize - 1;
            auto endIdx(CPLAtoGIntBig(pszEnd));
            if (endIdx < 0)
            {
                const auto positiveEndIdx = static_cast<GUInt64>(-endIdx);
                if (nDimSize < positiveEndIdx)
                    endIdx = 0;
                else
                    endIdx = nDimSize - positiveEndIdx;
            }
            GUInt64 nEndIdx = endIdx;
            nEndIdx = EQUAL(pszEnd, "") ? (!bPosIncr ? 0 : nDimSize) : nEndIdx;
            if (pszStart[0] || pszEnd[0])
            {
                if ((bPosIncr && range.m_nStartIdx >= nEndIdx) ||
                    (!bPosIncr && range.m_nStartIdx <= nEndIdx))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Output dimension of size 0 is not allowed");
                    return nullptr;
                }
            }
            int inc = (EQUAL(pszEnd, "") && !bPosIncr) ? 1 : 0;
            const auto nAbsIncr = std::abs(range.m_nIncr);
            const GUInt64 newSize =
                (pszStart[0] == 0 && pszEnd[0] == 0 &&
                 range.m_nStartIdx == nEndIdx)
                    ? 1
                : bPosIncr
                    ? cpl::div_round_up(nEndIdx - range.m_nStartIdx, nAbsIncr)
                    : cpl::div_round_up(inc + range.m_nStartIdx - nEndIdx,
                                        nAbsIncr);
            const auto &poSrcDim = srcDims[nCurSrcDim];
            if (range.m_nStartIdx == 0 && range.m_nIncr == 1 &&
                newSize == poSrcDim->GetSize())
            {
                newDims.push_back(poSrcDim);
            }
            else
            {
                std::string osNewDimName(poSrcDim->GetName());
                if (bRenameDimensions)
                {
                    osNewDimName =
                        "subset_" + poSrcDim->GetName() +
                        CPLSPrintf("_" CPL_FRMT_GUIB "_" CPL_FRMT_GIB
                                   "_" CPL_FRMT_GUIB,
                                   static_cast<GUIntBig>(range.m_nStartIdx),
                                   static_cast<GIntBig>(range.m_nIncr),
                                   static_cast<GUIntBig>(newSize));
                }
                auto poNewDim = std::make_shared<GDALDimensionWeakIndexingVar>(
                    std::string(), osNewDimName, poSrcDim->GetType(),
                    range.m_nIncr > 0 ? poSrcDim->GetDirection()
                                      : std::string(),
                    newSize);
                auto poSrcIndexingVar = poSrcDim->GetIndexingVariable();
                if (poSrcIndexingVar &&
                    poSrcIndexingVar->GetDimensionCount() == 1 &&
                    poSrcIndexingVar->GetDimensions()[0] == poSrcDim)
                {
                    std::vector<std::shared_ptr<GDALDimension>>
                        indexingVarNewDims{poNewDim};
                    std::vector<size_t> indexingVarMapDimIdxToParentDimIdx{0};
                    std::vector<std::shared_ptr<GDALMDArray>>
                        indexingVarNewIndexingVar;
                    std::vector<GDALSlicedMDArray::Range>
                        indexingVarParentRanges{range};
                    auto poNewIndexingVar = GDALSlicedMDArray::Create(
                        poSrcIndexingVar, pszIdxSpec,
                        std::move(indexingVarNewDims),
                        std::move(indexingVarMapDimIdxToParentDimIdx),
                        std::move(indexingVarNewIndexingVar),
                        std::move(indexingVarParentRanges));
                    poNewDim->SetIndexingVariable(poNewIndexingVar);
                    apoNewIndexingVariables.push_back(
                        std::move(poNewIndexingVar));
                }
                newDims.push_back(std::move(poNewDim));
            }
            mapDimIdxToParentDimIdx.push_back(nCurSrcDim);
            parentRanges.emplace_back(range);
        }

        nCurSrcDim++;
    }
    for (; nCurSrcDim < srcDims.size(); nCurSrcDim++)
    {
        parentRanges.emplace_back(0, 1);
        newDims.push_back(srcDims[nCurSrcDim]);
        mapDimIdxToParentDimIdx.push_back(nCurSrcDim);
    }

    GDALMDArray::ViewSpec viewSpec;
    viewSpec.m_mapDimIdxToParentDimIdx = mapDimIdxToParentDimIdx;
    viewSpec.m_parentRanges = parentRanges;
    viewSpecs.emplace_back(std::move(viewSpec));

    return GDALSlicedMDArray::Create(
        self, viewExpr, std::move(newDims), std::move(mapDimIdxToParentDimIdx),
        std::move(apoNewIndexingVariables), std::move(parentRanges));
}

/************************************************************************/
/*                       GDALExtractFieldMDArray                        */
/************************************************************************/

class GDALExtractFieldMDArray final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    GDALExtendedDataType m_dt;
    std::string m_srcCompName;
    mutable std::vector<GByte> m_pabyNoData{};

  protected:
    GDALExtractFieldMDArray(const std::shared_ptr<GDALMDArray> &poParent,
                            const std::string &fieldName,
                            const std::unique_ptr<GDALEDTComponent> &srcComp)
        : GDALAbstractMDArray(std::string(), "Extract field " + fieldName +
                                                 " of " +
                                                 poParent->GetFullName()),
          GDALPamMDArray(
              std::string(),
              "Extract field " + fieldName + " of " + poParent->GetFullName(),
              GDALPamMultiDim::GetPAM(poParent), poParent->GetContext()),
          m_poParent(poParent), m_dt(srcComp->GetType()),
          m_srcCompName(srcComp->GetName())
    {
        m_pabyNoData.resize(m_dt.GetSize());
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

    bool IAdviseRead(const GUInt64 *arrayStartIdx, const size_t *count,
                     CSLConstList papszOptions) const override
    {
        return m_poParent->AdviseRead(arrayStartIdx, count, papszOptions);
    }

  public:
    static std::shared_ptr<GDALExtractFieldMDArray>
    Create(const std::shared_ptr<GDALMDArray> &poParent,
           const std::string &fieldName,
           const std::unique_ptr<GDALEDTComponent> &srcComp)
    {
        auto newAr(std::shared_ptr<GDALExtractFieldMDArray>(
            new GDALExtractFieldMDArray(poParent, fieldName, srcComp)));
        newAr->SetSelf(newAr);
        return newAr;
    }

    ~GDALExtractFieldMDArray() override
    {
        m_dt.FreeDynamicMemory(&m_pabyNoData[0]);
    }

    bool IsWritable() const override
    {
        return m_poParent->IsWritable();
    }

    const std::string &GetFilename() const override
    {
        return m_poParent->GetFilename();
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_poParent->GetDimensions();
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_dt;
    }

    const std::string &GetUnit() const override
    {
        return m_poParent->GetUnit();
    }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        return m_poParent->GetSpatialRef();
    }

    const void *GetRawNoDataValue() const override
    {
        const void *parentNoData = m_poParent->GetRawNoDataValue();
        if (parentNoData == nullptr)
            return nullptr;

        m_dt.FreeDynamicMemory(&m_pabyNoData[0]);
        memset(&m_pabyNoData[0], 0, m_dt.GetSize());

        std::vector<std::unique_ptr<GDALEDTComponent>> comps;
        comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
            new GDALEDTComponent(m_srcCompName, 0, m_dt)));
        auto tmpDT(GDALExtendedDataType::Create(std::string(), m_dt.GetSize(),
                                                std::move(comps)));

        GDALExtendedDataType::CopyValue(parentNoData, m_poParent->GetDataType(),
                                        &m_pabyNoData[0], tmpDT);

        return &m_pabyNoData[0];
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

    std::vector<GUInt64> GetBlockSize() const override
    {
        return m_poParent->GetBlockSize();
    }
};

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool GDALExtractFieldMDArray::IRead(const GUInt64 *arrayStartIdx,
                                    const size_t *count,
                                    const GInt64 *arrayStep,
                                    const GPtrDiff_t *bufferStride,
                                    const GDALExtendedDataType &bufferDataType,
                                    void *pDstBuffer) const
{
    std::vector<std::unique_ptr<GDALEDTComponent>> comps;
    comps.emplace_back(std::unique_ptr<GDALEDTComponent>(
        new GDALEDTComponent(m_srcCompName, 0, bufferDataType)));
    auto tmpDT(GDALExtendedDataType::Create(
        std::string(), bufferDataType.GetSize(), std::move(comps)));

    return m_poParent->Read(arrayStartIdx, count, arrayStep, bufferStride,
                            tmpDT, pDstBuffer);
}

/************************************************************************/
/*                    CreateFieldNameExtractArray()                     */
/************************************************************************/

static std::shared_ptr<GDALMDArray>
CreateFieldNameExtractArray(const std::shared_ptr<GDALMDArray> &self,
                            const std::string &fieldName)
{
    CPLAssert(self->GetDataType().GetClass() == GEDTC_COMPOUND);
    const std::unique_ptr<GDALEDTComponent> *srcComp = nullptr;
    for (const auto &comp : self->GetDataType().GetComponents())
    {
        if (comp->GetName() == fieldName)
        {
            srcComp = &comp;
            break;
        }
    }
    if (srcComp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find field %s",
                 fieldName.c_str());
        return nullptr;
    }
    return GDALExtractFieldMDArray::Create(self, fieldName, *srcComp);
}

//! @endcond

/************************************************************************/
/*                              GetView()                               */
/************************************************************************/

// clang-format off
/** Return a view of the array using slicing or field access.
 *
 * The slice expression uses the same syntax as NumPy basic slicing and
 * indexing. See
 * https://www.numpy.org/devdocs/reference/arrays.indexing.html#basic-slicing-and-indexing
 * Or it can use field access by name. See
 * https://www.numpy.org/devdocs/reference/arrays.indexing.html#field-access
 *
 * Multiple [] bracket elements can be concatenated, with a slice expression
 * or field name inside each.
 *
 * For basic slicing and indexing, inside each [] bracket element, a list of
 * indexes that apply to successive source dimensions, can be specified, using
 * integer indexing (e.g. 1), range indexing (start:stop:step), ellipsis (...)
 * or newaxis, using a comma separator.
 *
 * Examples with a 2-dimensional array whose content is [[0,1,2,3],[4,5,6,7]].
 * <ul>
 * <li>GetView("[1][2]"): returns a 0-dimensional/scalar array with the value
 *     at index 1 in the first dimension, and index 2 in the second dimension
 *     from the source array. That is 5</li>
 * <li>GetView("[1]")->GetView("[2]"): same as above. Above is actually
 * implemented internally doing this intermediate slicing approach.</li>
 * <li>GetView("[1,2]"): same as above, but a bit more performant.</li>
 * <li>GetView("[1]"): returns a 1-dimensional array, sliced at index 1 in the
 *     first dimension. That is [4,5,6,7].</li>
 * <li>GetView("[:,2]"): returns a 1-dimensional array, sliced at index 2 in the
 *     second dimension. That is [2,6].</li>
 * <li>GetView("[:,2:3:]"): returns a 2-dimensional array, sliced at index 2 in
 * the second dimension. That is [[2],[6]].</li>
 * <li>GetView("[::,2]"): Same as
 * above.</li> <li>GetView("[...,2]"): same as above, in that case, since the
 * ellipsis only expands to one dimension here.</li>
 * <li>GetView("[:,::2]"):
 * returns a 2-dimensional array, with even-indexed elements of the second
 * dimension. That is [[0,2],[4,6]].</li>
 * <li>GetView("[:,1::2]"): returns a
 * 2-dimensional array, with odd-indexed elements of the second dimension. That
 * is [[1,3],[5,7]].</li>
 * <li>GetView("[:,1:3:]"): returns a 2-dimensional
 * array, with elements of the second dimension with index in the range [1,3[.
 * That is [[1,2],[5,6]].</li>
 * <li>GetView("[::-1,:]"): returns a 2-dimensional
 * array, with the values in first dimension reversed. That is
 * [[4,5,6,7],[0,1,2,3]].</li>
 * <li>GetView("[newaxis,...]"): returns a
 * 3-dimensional array, with an additional dimension of size 1 put at the
 * beginning. That is [[[0,1,2,3],[4,5,6,7]]].</li>
 * </ul>
 *
 * One difference with NumPy behavior is that ranges that would result in
 * zero elements are not allowed (dimensions of size 0 not being allowed in the
 * GDAL multidimensional model).
 *
 * For field access, the syntax to use is ["field_name"] or ['field_name'].
 * Multiple field specification is not supported currently.
 *
 * Both type of access can be combined, e.g. GetView("[1]['field_name']")
 *
 * \note When using the GDAL Python bindings, natural Python syntax can be
 * used. That is ar[0,::,1]["foo"] will be internally translated to
 * ar.GetView("[0,::,1]['foo']")
 * \note When using the C++ API and integer indexing only, you may use the
 * at(idx0, idx1, ...) method.
 *
 * The returned array holds a reference to the original one, and thus is
 * a view of it (not a copy). If the content of the original array changes,
 * the content of the view array too. When using basic slicing and indexing,
 * the view can be written if the underlying array is writable.
 *
 * This is the same as the C function GDALMDArrayGetView()
 *
 * @param viewExpr Expression expressing basic slicing and indexing, or field
 * access.
 * @return a new array, that holds a reference to the original one, and thus is
 * a view of it (not a copy), or nullptr in case of error.
 */
// clang-format on

std::shared_ptr<GDALMDArray>
GDALMDArray::GetView(const std::string &viewExpr) const
{
    std::vector<ViewSpec> viewSpecs;
    return GetView(viewExpr, true, viewSpecs);
}

//! @cond Doxygen_Suppress
std::shared_ptr<GDALMDArray>
GDALMDArray::GetView(const std::string &viewExpr, bool bRenameDimensions,
                     std::vector<ViewSpec> &viewSpecs) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if (!self)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }
    std::string curExpr(viewExpr);
    while (true)
    {
        if (curExpr.empty() || curExpr[0] != '[')
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Slice string should start with ['");
            return nullptr;
        }

        std::string fieldName;
        size_t endExpr;
        if (curExpr.size() > 2 && (curExpr[1] == '"' || curExpr[1] == '\''))
        {
            if (self->GetDataType().GetClass() != GEDTC_COMPOUND)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field access not allowed on non-compound data type");
                return nullptr;
            }
            size_t idx = 2;
            for (; idx < curExpr.size(); idx++)
            {
                const char ch = curExpr[idx];
                if (ch == curExpr[1])
                    break;
                if (ch == '\\' && idx + 1 < curExpr.size())
                {
                    fieldName += curExpr[idx + 1];
                    idx++;
                }
                else
                {
                    fieldName += ch;
                }
            }
            if (idx + 1 >= curExpr.size() || curExpr[idx + 1] != ']')
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid field access specification");
                return nullptr;
            }
            endExpr = idx + 1;
        }
        else
        {
            endExpr = curExpr.find(']');
        }
        if (endExpr == std::string::npos)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Missing ]'");
            return nullptr;
        }
        if (endExpr == 1)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "[] not allowed");
            return nullptr;
        }
        std::string activeSlice(curExpr.substr(1, endExpr - 1));

        if (!fieldName.empty())
        {
            ViewSpec viewSpec;
            viewSpec.m_osFieldName = fieldName;
            viewSpecs.emplace_back(std::move(viewSpec));
        }

        auto newArray = !fieldName.empty()
                            ? CreateFieldNameExtractArray(self, fieldName)
                            : CreateSlicedArray(self, viewExpr, activeSlice,
                                                bRenameDimensions, viewSpecs);

        if (endExpr == curExpr.size() - 1)
        {
            return newArray;
        }
        self = std::move(newArray);
        curExpr = curExpr.substr(endExpr + 1);
    }
}

//! @endcond

std::shared_ptr<GDALMDArray>
GDALMDArray::GetView(const std::vector<GUInt64> &indices) const
{
    std::string osExpr("[");
    bool bFirst = true;
    for (const auto &idx : indices)
    {
        if (!bFirst)
            osExpr += ',';
        bFirst = false;
        osExpr += CPLSPrintf(CPL_FRMT_GUIB, static_cast<GUIntBig>(idx));
    }
    return GetView(osExpr + ']');
}

/************************************************************************/
/*                              operator[]                              */
/************************************************************************/

/** Return a view of the array using field access
 *
 * Equivalent of GetView("['fieldName']")
 *
 * \note When operating on a shared_ptr, use (*array)["fieldName"] syntax.
 */
std::shared_ptr<GDALMDArray>
GDALMDArray::operator[](const std::string &fieldName) const
{
    return GetView(CPLSPrintf("['%s']", CPLString(fieldName)
                                            .replaceAll('\\', "\\\\")
                                            .replaceAll('\'', "\\\'")
                                            .c_str()));
}
