/******************************************************************************
 * Name:     gdalmultidim_subsetdimension.cpp
 * Project:  GDAL Core
 * Purpose:  GDALGroup::SubsetDimensionFromSelection() implementation
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even.rouault at spatialys.com>
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

#include "gdal_priv.h"
#include "gdal_pam.h"

#include <algorithm>

/************************************************************************/
/*                           GetParentName()                            */
/************************************************************************/

static std::string GetParentName(const std::string &osPath)
{
    if (osPath == "/" || osPath.rfind('/') == 0)
        return "/";
    return osPath.substr(0, osPath.rfind('/'));
}

/************************************************************************/
/*                   GDALSubsetGroupSharedResources                     */
/************************************************************************/

struct GDALSubsetGroupSharedResources
{
    std::shared_ptr<GDALGroup> m_poRootGroup{};  // may be nullptr
    std::string m_osDimFullName{};
    std::vector<int> m_anMapNewDimToOldDim{};
    std::string m_osSelection{};
    std::shared_ptr<GDALDimension> m_poNewDim{};
    std::shared_ptr<GDALMDArray> m_poNewIndexingVar{};
};

/************************************************************************/
/*                          CreateContext()                             */
/************************************************************************/

static std::string
CreateContext(const std::string &osParentContext,
              const std::shared_ptr<GDALSubsetGroupSharedResources> &poShared)
{
    std::string osRet(osParentContext);
    if (!osRet.empty())
        osRet += ". ";
    osRet += "Selection ";
    osRet += poShared->m_osSelection;
    return osRet;
}

/************************************************************************/
/*                           GDALSubsetGroup                            */
/************************************************************************/

class GDALSubsetGroup final : public GDALGroup
{
    std::shared_ptr<GDALGroup> m_poParent{};
    std::shared_ptr<GDALSubsetGroupSharedResources> m_poShared{};

    GDALSubsetGroup(
        const std::shared_ptr<GDALGroup> &poParent,
        const std::shared_ptr<GDALSubsetGroupSharedResources> &poShared)
        : GDALGroup(GetParentName(poParent->GetFullName()), poParent->GetName(),
                    CreateContext(poParent->GetContext(), poShared)),
          m_poParent(std::move(poParent)), m_poShared(std::move(poShared))
    {
    }

  public:
    static std::shared_ptr<GDALGroup>
    Create(const std::shared_ptr<GDALGroup> &poParent,
           const std::shared_ptr<GDALSubsetGroupSharedResources> &poShared)
    {
        auto poGroup = std::shared_ptr<GDALSubsetGroup>(
            new GDALSubsetGroup(poParent, poShared));
        poGroup->SetSelf(poGroup);
        return poGroup;
    }

    std::vector<std::string>
    GetMDArrayNames(CSLConstList papszOptions = nullptr) const override
    {
        return m_poParent->GetMDArrayNames(papszOptions);
    }

    std::shared_ptr<GDALMDArray>
    OpenMDArray(const std::string &osName,
                CSLConstList papszOptions = nullptr) const override;

    std::vector<std::string>
    GetGroupNames(CSLConstList papszOptions = nullptr) const override
    {
        return m_poParent->GetGroupNames(papszOptions);
    }

    std::shared_ptr<GDALGroup>
    OpenGroup(const std::string &osName,
              CSLConstList papszOptions = nullptr) const override;

    std::vector<std::shared_ptr<GDALDimension>>
    GetDimensions(CSLConstList papszOptions = nullptr) const override;

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
/*                           GDALSubsetArray                            */
/************************************************************************/

class GDALSubsetArray final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    std::shared_ptr<GDALSubsetGroupSharedResources> m_poShared{};
    std::vector<std::shared_ptr<GDALDimension>> m_apoDims{};
    std::vector<bool> m_abPatchedDim{};
    bool m_bPatchedDimIsFirst = false;

  protected:
    GDALSubsetArray(
        const std::shared_ptr<GDALMDArray> &poParent,
        const std::shared_ptr<GDALSubsetGroupSharedResources> &poShared,
        const std::string &osContext)
        : GDALAbstractMDArray(GetParentName(poParent->GetFullName()),
                              poParent->GetName()),
          GDALPamMDArray(GetParentName(poParent->GetFullName()),
                         poParent->GetName(), GDALPamMultiDim::GetPAM(poParent),
                         osContext),
          m_poParent(std::move(poParent)), m_poShared(std::move(poShared))
    {
        m_apoDims = m_poParent->GetDimensions();
        for (size_t i = 0; i < m_apoDims.size(); ++i)
        {
            auto &poDim = m_apoDims[i];
            if (poDim->GetFullName() == m_poShared->m_osDimFullName)
            {
                m_bPatchedDimIsFirst = (i == 0);
                poDim = m_poShared->m_poNewDim;
                m_abPatchedDim.push_back(true);
            }
            else
            {
                m_abPatchedDim.push_back(false);
            }
        }
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    static std::shared_ptr<GDALSubsetArray>
    Create(const std::shared_ptr<GDALMDArray> &poParent,
           const std::shared_ptr<GDALSubsetGroupSharedResources> &poShared,
           const std::string &osContext)
    {
        auto newAr(std::shared_ptr<GDALSubsetArray>(
            new GDALSubsetArray(poParent, poShared, osContext)));
        newAr->SetSelf(newAr);
        return newAr;
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
        return m_poParent->GetDataType();
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
        return m_poParent->GetRawNoDataValue();
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        std::vector<GUInt64> ret(m_poParent->GetBlockSize());
        for (size_t i = 0; i < m_apoDims.size(); ++i)
        {
            if (m_abPatchedDim[i])
                ret[1] = 1;
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

    std::shared_ptr<GDALGroup> GetRootGroup() const override
    {
        if (m_poShared->m_poRootGroup)
        {
            return GDALSubsetGroup::Create(m_poShared->m_poRootGroup,
                                           m_poShared);
        }
        return nullptr;
    }
};

/************************************************************************/
/*                            OpenMDArray()                             */
/************************************************************************/

std::shared_ptr<GDALMDArray>
GDALSubsetGroup::OpenMDArray(const std::string &osName,
                             CSLConstList papszOptions) const
{
    auto poArray = m_poParent->OpenMDArray(osName, papszOptions);
    if (poArray)
    {
        for (const auto &poDim : poArray->GetDimensions())
        {
            if (poDim->GetFullName() == m_poShared->m_osDimFullName)
            {
                return GDALSubsetArray::Create(poArray, m_poShared,
                                               GetContext());
            }
        }
    }
    return poArray;
}

/************************************************************************/
/*                             OpenGroup()                              */
/************************************************************************/

std::shared_ptr<GDALGroup>
GDALSubsetGroup::OpenGroup(const std::string &osName,
                           CSLConstList papszOptions) const
{
    auto poSubGroup = m_poParent->OpenGroup(osName, papszOptions);
    if (poSubGroup)
    {
        poSubGroup = GDALSubsetGroup::Create(poSubGroup, m_poShared);
    }
    return poSubGroup;
}

/************************************************************************/
/*                             GetDimensions()                          */
/************************************************************************/

std::vector<std::shared_ptr<GDALDimension>>
GDALSubsetGroup::GetDimensions(CSLConstList papszOptions) const
{
    auto apoDims = m_poParent->GetDimensions(papszOptions);
    for (auto &poDim : apoDims)
    {
        if (poDim->GetFullName() == m_poShared->m_osDimFullName)
        {
            poDim = m_poShared->m_poNewDim;
        }
    }
    return apoDims;
}

/************************************************************************/
/*                             IRead()                                  */
/************************************************************************/

bool GDALSubsetArray::IRead(const GUInt64 *arrayStartIdx, const size_t *count,
                            const GInt64 *arrayStep,
                            const GPtrDiff_t *bufferStride,
                            const GDALExtendedDataType &bufferDataType,
                            void *pDstBuffer) const
{
    const auto nDims = m_apoDims.size();
    std::vector<GUInt64> newArrayStartIdx(nDims);
    // the +1 in nDims + 1 is to make happy -Werror=null-dereference when
    // doing newCount[0] = 1 and newArrayStep[0] = 1
    std::vector<size_t> newCount(nDims + 1, 1);
    std::vector<GInt64> newArrayStep(nDims + 1, 1);
    const size_t nBufferDTSize = bufferDataType.GetSize();

    if (m_bPatchedDimIsFirst)
    {
        // Optimized case when the only patched dimension is the first one.
        std::copy_n(arrayStartIdx, nDims, newArrayStartIdx.data());
        std::copy_n(count, nDims, newCount.data());
        std::copy_n(arrayStep, nDims, newArrayStep.data());
        GUInt64 arrayIdx = arrayStartIdx[0];
        GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        newCount[0] = 1;
        newArrayStep[0] = 1;
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
        for (size_t i = 0; i < count[0]; ++i)
        {
            if (i > 0)
            {
                if (arrayStep[0] > 0)
                    arrayIdx += arrayStep[0];
                else
                    arrayIdx -= static_cast<GUInt64>(-arrayStep[0]);
                pabyDstBuffer += bufferStride[0] * nBufferDTSize;
            }
            newArrayStartIdx[0] =
                m_poShared->m_anMapNewDimToOldDim[static_cast<int>(arrayIdx)];
            if (!m_poParent->Read(newArrayStartIdx.data(), newCount.data(),
                                  newArrayStep.data(), bufferStride,
                                  bufferDataType, pabyDstBuffer))
            {
                return false;
            }
        }
        return true;
    }

    // Slow/unoptimized case
    std::vector<size_t> anStackIter(nDims);
    std::vector<GUInt64> anStackArrayIdx(nDims);
    std::vector<GByte *> pabyDstBufferStack(nDims + 1);
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
    pabyDstBufferStack[0] = static_cast<GByte *>(pDstBuffer);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    size_t iDim = 0;
lbl_next_depth:
    if (iDim == nDims)
    {
        if (!m_poParent->Read(newArrayStartIdx.data(), newCount.data(),
                              newArrayStep.data(), bufferStride, bufferDataType,
                              pabyDstBufferStack[iDim]))
        {
            return false;
        }
    }
    else
    {
        anStackIter[iDim] = 0;
        anStackArrayIdx[iDim] = arrayStartIdx[iDim];
        while (true)
        {
            if (m_abPatchedDim[iDim])
            {
                newArrayStartIdx[iDim] =
                    m_poShared->m_anMapNewDimToOldDim[static_cast<int>(
                        anStackArrayIdx[iDim])];
            }
            else
            {
                newArrayStartIdx[iDim] = anStackArrayIdx[iDim];
            }
            ++iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim - 1];
            goto lbl_next_depth;
        lbl_return_to_caller_in_loop:
            --iDim;
            ++anStackIter[iDim];
            if (anStackIter[iDim] == count[iDim])
                break;
            if (arrayStep[iDim] > 0)
                anStackArrayIdx[iDim] += arrayStep[iDim];
            else
                anStackArrayIdx[iDim] -= -arrayStep[iDim];
            pabyDstBufferStack[iDim] += bufferStride[iDim] * nBufferDTSize;
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller_in_loop;

    return true;
}

/************************************************************************/
/*                   SubsetDimensionFromSelection()                     */
/************************************************************************/

/** Return a virtual group whose one dimension has been subset according to a
 * selection.
 *
 * The selection criterion is currently restricted to the form
 * "/path/to/array=numeric_value" (no spaces around equal)
 *
 * This is similar to XArray indexing by name and label on a XArray Dataset
 * using the sel() method.
 * Cf https://docs.xarray.dev/en/latest/user-guide/indexing.html#quick-overview
 *
 * For example on a EMIT L2A product
 * (https://github.com/nasa/EMIT-Data-Resources/blob/main/python/tutorials/Exploring_EMIT_L2A_Reflectance.ipynb),
 * this can be used to keep only valid bands with
 * SubsetDimensionFromSelection("/sensor_band_parameters/good_wavelengths=1")
 *
 * This is the same as the C function GDALGroupSubsetDimensionFromSelection().
 *
 * @param osSelection Selection criterion.
 * @return a virtual group, or nullptr in case of error
 * @since 3.8
 */
std::shared_ptr<GDALGroup>
GDALGroup::SubsetDimensionFromSelection(const std::string &osSelection) const
{
    auto self = std::dynamic_pointer_cast<GDALGroup>(m_pSelf.lock());
    if (!self)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }

    const auto nEqualPos = osSelection.find('=');
    if (nEqualPos == std::string::npos)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for selection");
        return nullptr;
    }
    const auto osArrayName = osSelection.substr(0, nEqualPos);
    const auto osValue = osSelection.substr(nEqualPos + 1);
    if (CPLGetValueType(osValue.c_str()) != CPL_VALUE_INTEGER &&
        CPLGetValueType(osValue.c_str()) != CPL_VALUE_REAL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Non-numeric value in selection criterion");
        return nullptr;
    }
    auto poArray = OpenMDArrayFromFullname(osArrayName);
    if (!poArray)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find array %s",
                 osArrayName.c_str());
        return nullptr;
    }
    if (poArray->GetDimensionCount() != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array %s is not single dimensional", osArrayName.c_str());
        return nullptr;
    }
    if (poArray->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Array %s is not of numeric type",
                 osArrayName.c_str());
        return nullptr;
    }

    const auto nElts = poArray->GetTotalElementsCount();
    if (nElts > 10 * 1024 * 1024)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too many values in %s",
                 osArrayName.c_str());
        return nullptr;
    }
    std::vector<double> values;
    try
    {
        values.resize(static_cast<size_t>(nElts));
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Out of memory: %s", e.what());
        return nullptr;
    }
    const GUInt64 startIdx[1] = {0};
    const size_t count[1] = {values.size()};
    if (!poArray->Read(startIdx, count, nullptr, nullptr,
                       GDALExtendedDataType::Create(GDT_Float64), &values[0],
                       values.data(), values.size() * sizeof(values[0])))
    {
        return nullptr;
    }
    const double dfSelectionValue = CPLAtof(osValue.c_str());
    std::vector<int> anMapNewDimToOldDim;
    for (int i = 0; i < static_cast<int>(nElts); ++i)
    {
        if (values[i] == dfSelectionValue)
            anMapNewDimToOldDim.push_back(i);
    }
    if (anMapNewDimToOldDim.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No value in %s matching %f",
                 osArrayName.c_str(), dfSelectionValue);
        return nullptr;
    }
    if (anMapNewDimToOldDim.size() == nElts)
    {
        return self;
    }

    auto poDim = poArray->GetDimensions()[0];
    auto poShared = std::make_shared<GDALSubsetGroupSharedResources>();
    if (GetFullName() == "/")
        poShared->m_poRootGroup = self;
    poShared->m_osSelection = osSelection;
    poShared->m_osDimFullName = poArray->GetDimensions()[0]->GetFullName();
    poShared->m_anMapNewDimToOldDim = std::move(anMapNewDimToOldDim);

    // Create a modified dimension of reduced size
    auto poNewDim = std::make_shared<GDALDimensionWeakIndexingVar>(
        GetParentName(poDim->GetFullName()), poDim->GetName(), poDim->GetType(),
        poDim->GetDirection(), poShared->m_anMapNewDimToOldDim.size());
    poShared->m_poNewDim = poNewDim;

    auto poIndexingVar = poDim->GetIndexingVariable();
    if (poIndexingVar)
    {
        // poNewIndexingVar must be created with a different GDALSubsetGroupSharedResources
        // instance than poShared, to avoid cross reference, that would result in
        // objects not being freed !
        auto poSpecificShared =
            std::make_shared<GDALSubsetGroupSharedResources>();
        poSpecificShared->m_poRootGroup = poShared->m_poRootGroup;
        poSpecificShared->m_osSelection = osSelection;
        poSpecificShared->m_osDimFullName =
            poArray->GetDimensions()[0]->GetFullName();
        poSpecificShared->m_anMapNewDimToOldDim =
            poShared->m_anMapNewDimToOldDim;
        poSpecificShared->m_poNewDim = poNewDim;
        auto poNewIndexingVar =
            GDALSubsetArray::Create(poIndexingVar, poSpecificShared,
                                    CreateContext(GetContext(), poShared));
        poNewDim->SetIndexingVariable(poNewIndexingVar);
        poShared->m_poNewIndexingVar = poNewIndexingVar;
    }

    return GDALSubsetGroup::Create(self, poShared);
}
