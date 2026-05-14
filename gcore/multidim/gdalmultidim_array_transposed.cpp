/******************************************************************************
 *
 * Name:     gdalmultidim_array_transposed.cpp
 * Project:  GDAL Core
 * Purpose:  GDALMDArray::Transpose() implementation
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

//! @cond Doxygen_Suppress

/************************************************************************/
/*                   CopyToFinalBufferSameDataType()                    */
/************************************************************************/

template <size_t N>
void CopyToFinalBufferSameDataType(const void *pSrcBuffer, void *pDstBuffer,
                                   size_t nDims, const size_t *count,
                                   const GPtrDiff_t *bufferStride)
{
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte *> pabyDstBufferStack(nDims + 1);
    const GByte *pabySrcBuffer = static_cast<const GByte *>(pSrcBuffer);
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
    if (iDim == nDims - 1)
    {
        size_t n = count[iDim];
        GByte *pabyDstBuffer = pabyDstBufferStack[iDim];
        const auto bufferStrideLastDim = bufferStride[iDim] * N;
        while (n > 0)
        {
            --n;
            memcpy(pabyDstBuffer, pabySrcBuffer, N);
            pabyDstBuffer += bufferStrideLastDim;
            pabySrcBuffer += N;
        }
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while (true)
        {
            ++iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim - 1];
            goto lbl_next_depth;
        lbl_return_to_caller_in_loop:
            --iDim;
            --anStackCount[iDim];
            if (anStackCount[iDim] == 0)
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * N;
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                         CopyToFinalBuffer()                          */
/************************************************************************/

static void CopyToFinalBuffer(const void *pSrcBuffer,
                              const GDALExtendedDataType &eSrcDataType,
                              void *pDstBuffer,
                              const GDALExtendedDataType &eDstDataType,
                              size_t nDims, const size_t *count,
                              const GPtrDiff_t *bufferStride)
{
    const size_t nSrcDataTypeSize(eSrcDataType.GetSize());
    // Use specialized implementation for well-known data types when no
    // type conversion is needed
    if (eSrcDataType == eDstDataType)
    {
        if (nSrcDataTypeSize == 1)
        {
            CopyToFinalBufferSameDataType<1>(pSrcBuffer, pDstBuffer, nDims,
                                             count, bufferStride);
            return;
        }
        else if (nSrcDataTypeSize == 2)
        {
            CopyToFinalBufferSameDataType<2>(pSrcBuffer, pDstBuffer, nDims,
                                             count, bufferStride);
            return;
        }
        else if (nSrcDataTypeSize == 4)
        {
            CopyToFinalBufferSameDataType<4>(pSrcBuffer, pDstBuffer, nDims,
                                             count, bufferStride);
            return;
        }
        else if (nSrcDataTypeSize == 8)
        {
            CopyToFinalBufferSameDataType<8>(pSrcBuffer, pDstBuffer, nDims,
                                             count, bufferStride);
            return;
        }
    }

    const size_t nDstDataTypeSize(eDstDataType.GetSize());
    std::vector<size_t> anStackCount(nDims);
    std::vector<GByte *> pabyDstBufferStack(nDims + 1);
    const GByte *pabySrcBuffer = static_cast<const GByte *>(pSrcBuffer);
    pabyDstBufferStack[0] = static_cast<GByte *>(pDstBuffer);
    size_t iDim = 0;

lbl_next_depth:
    if (iDim == nDims - 1)
    {
        GDALExtendedDataType::CopyValues(pabySrcBuffer, eSrcDataType, 1,
                                         pabyDstBufferStack[iDim], eDstDataType,
                                         bufferStride[iDim], count[iDim]);
        pabySrcBuffer += count[iDim] * nSrcDataTypeSize;
    }
    else
    {
        anStackCount[iDim] = count[iDim];
        while (true)
        {
            ++iDim;
            pabyDstBufferStack[iDim] = pabyDstBufferStack[iDim - 1];
            goto lbl_next_depth;
        lbl_return_to_caller_in_loop:
            --iDim;
            --anStackCount[iDim];
            if (anStackCount[iDim] == 0)
                break;
            pabyDstBufferStack[iDim] += bufferStride[iDim] * nDstDataTypeSize;
        }
    }
    if (iDim > 0)
        goto lbl_return_to_caller_in_loop;
}

/************************************************************************/
/*                        IsTransposedRequest()                         */
/************************************************************************/

bool GDALMDArray::IsTransposedRequest(
    const size_t *count,
    const GPtrDiff_t *bufferStride) const  // stride in elements
{
    /*
    For example:
    count = [2,3,4]
    strides = [12, 4, 1]            (2-1)*12+(3-1)*4+(4-1)*1=23   ==> row major
    stride [12, 1, 3]            (2-1)*12+(3-1)*1+(4-1)*3=23   ==>
    (axis[0],axis[2],axis[1]) transposition [1, 8, 2]             (2-1)*1+
    (3-1)*8+(4-1)*2=23   ==> (axis[2],axis[1],axis[0]) transposition
    */
    const size_t nDims(GetDimensionCount());
    size_t nCurStrideForRowMajorStrides = 1;
    bool bRowMajorStrides = true;
    size_t nElts = 1;
    size_t nLastIdx = 0;
    for (size_t i = nDims; i > 0;)
    {
        --i;
        if (bufferStride[i] < 0)
            return false;
        if (static_cast<size_t>(bufferStride[i]) !=
            nCurStrideForRowMajorStrides)
        {
            bRowMajorStrides = false;
        }
        // Integer overflows have already been checked in CheckReadWriteParams()
        nCurStrideForRowMajorStrides *= count[i];
        nElts *= count[i];
        nLastIdx += static_cast<size_t>(bufferStride[i]) * (count[i] - 1);
    }
    if (bRowMajorStrides)
        return false;
    return nLastIdx == nElts - 1;
}

/************************************************************************/
/*                         TransposeLast2Dims()                         */
/************************************************************************/

static bool TransposeLast2Dims(void *pDstBuffer,
                               const GDALExtendedDataType &eDT,
                               const size_t nDims, const size_t *count,
                               const size_t nEltsNonLast2Dims)
{
    const size_t nEltsLast2Dims = count[nDims - 2] * count[nDims - 1];
    const auto nDTSize = eDT.GetSize();
    void *pTempBufferForLast2DimsTranspose =
        VSI_MALLOC2_VERBOSE(nEltsLast2Dims, nDTSize);
    if (pTempBufferForLast2DimsTranspose == nullptr)
        return false;

    GByte *pabyDstBuffer = static_cast<GByte *>(pDstBuffer);
    for (size_t i = 0; i < nEltsNonLast2Dims; ++i)
    {
        GDALTranspose2D(pabyDstBuffer, eDT.GetNumericDataType(),
                        pTempBufferForLast2DimsTranspose,
                        eDT.GetNumericDataType(), count[nDims - 1],
                        count[nDims - 2]);
        memcpy(pabyDstBuffer, pTempBufferForLast2DimsTranspose,
               nDTSize * nEltsLast2Dims);
        pabyDstBuffer += nDTSize * nEltsLast2Dims;
    }

    VSIFree(pTempBufferForLast2DimsTranspose);

    return true;
}

/************************************************************************/
/*                      ReadForTransposedRequest()                      */
/************************************************************************/

// Using the netCDF/HDF5 APIs to read a slice with strides that express a
// transposed view yield to extremely poor/unusable performance. This fixes
// this by using temporary memory to read in a contiguous buffer in a
// row-major order, and then do the transposition to the final buffer.

bool GDALMDArray::ReadForTransposedRequest(
    const GUInt64 *arrayStartIdx, const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride, const GDALExtendedDataType &bufferDataType,
    void *pDstBuffer) const
{
    const size_t nDims(GetDimensionCount());
    if (nDims == 0)
    {
        CPLAssert(false);
        return false;  // shouldn't happen
    }
    size_t nElts = 1;
    for (size_t i = 0; i < nDims; ++i)
        nElts *= count[i];

    std::vector<GPtrDiff_t> tmpBufferStrides(nDims);
    tmpBufferStrides.back() = 1;
    for (size_t i = nDims - 1; i > 0;)
    {
        --i;
        tmpBufferStrides[i] = tmpBufferStrides[i + 1] * count[i + 1];
    }

    const auto &eDT = GetDataType();
    const auto nDTSize = eDT.GetSize();
    if (bufferDataType == eDT && nDims >= 2 && bufferStride[nDims - 2] == 1 &&
        static_cast<size_t>(bufferStride[nDims - 1]) == count[nDims - 2] &&
        (nDTSize == 1 || nDTSize == 2 || nDTSize == 4 || nDTSize == 8))
    {
        // Optimization of the optimization if only the last 2 dims are
        // transposed that saves on temporary buffer allocation
        const size_t nEltsLast2Dims = count[nDims - 2] * count[nDims - 1];
        size_t nCurStrideForRowMajorStrides = nEltsLast2Dims;
        bool bRowMajorStridesForNonLast2Dims = true;
        size_t nEltsNonLast2Dims = 1;
        for (size_t i = nDims - 2; i > 0;)
        {
            --i;
            if (static_cast<size_t>(bufferStride[i]) !=
                nCurStrideForRowMajorStrides)
            {
                bRowMajorStridesForNonLast2Dims = false;
            }
            // Integer overflows have already been checked in
            // CheckReadWriteParams()
            nCurStrideForRowMajorStrides *= count[i];
            nEltsNonLast2Dims *= count[i];
        }
        if (bRowMajorStridesForNonLast2Dims)
        {
            // We read in the final buffer!
            if (!IRead(arrayStartIdx, count, arrayStep, tmpBufferStrides.data(),
                       eDT, pDstBuffer))
            {
                return false;
            }

            return TransposeLast2Dims(pDstBuffer, eDT, nDims, count,
                                      nEltsNonLast2Dims);
        }
    }

    void *pTempBuffer = VSI_MALLOC2_VERBOSE(nElts, eDT.GetSize());
    if (pTempBuffer == nullptr)
        return false;

    if (!IRead(arrayStartIdx, count, arrayStep, tmpBufferStrides.data(), eDT,
               pTempBuffer))
    {
        VSIFree(pTempBuffer);
        return false;
    }
    CopyToFinalBuffer(pTempBuffer, eDT, pDstBuffer, bufferDataType, nDims,
                      count, bufferStride);

    if (eDT.NeedsFreeDynamicMemory())
    {
        GByte *pabyPtr = static_cast<GByte *>(pTempBuffer);
        for (size_t i = 0; i < nElts; ++i)
        {
            eDT.FreeDynamicMemory(pabyPtr);
            pabyPtr += nDTSize;
        }
    }

    VSIFree(pTempBuffer);
    return true;
}

/************************************************************************/
/*                        GDALMDArrayTransposed                         */
/************************************************************************/

class GDALMDArrayTransposed final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    std::vector<int> m_anMapNewAxisToOldAxis{};
    std::vector<std::shared_ptr<GDALDimension>> m_dims{};

    mutable std::vector<GUInt64> m_parentStart;
    mutable std::vector<size_t> m_parentCount;
    mutable std::vector<GInt64> m_parentStep;
    mutable std::vector<GPtrDiff_t> m_parentStride;

    void PrepareParentArrays(const GUInt64 *arrayStartIdx, const size_t *count,
                             const GInt64 *arrayStep,
                             const GPtrDiff_t *bufferStride) const;

    static std::string
    MappingToStr(const std::vector<int> &anMapNewAxisToOldAxis)
    {
        std::string ret;
        ret += '[';
        for (size_t i = 0; i < anMapNewAxisToOldAxis.size(); ++i)
        {
            if (i > 0)
                ret += ',';
            ret += CPLSPrintf("%d", anMapNewAxisToOldAxis[i]);
        }
        ret += ']';
        return ret;
    }

  protected:
    GDALMDArrayTransposed(const std::shared_ptr<GDALMDArray> &poParent,
                          const std::vector<int> &anMapNewAxisToOldAxis,
                          std::vector<std::shared_ptr<GDALDimension>> &&dims)
        : GDALAbstractMDArray(std::string(),
                              "Transposed view of " + poParent->GetFullName() +
                                  " along " +
                                  MappingToStr(anMapNewAxisToOldAxis)),
          GDALPamMDArray(std::string(),
                         "Transposed view of " + poParent->GetFullName() +
                             " along " + MappingToStr(anMapNewAxisToOldAxis),
                         GDALPamMultiDim::GetPAM(poParent),
                         poParent->GetContext()),
          m_poParent(std::move(poParent)),
          m_anMapNewAxisToOldAxis(anMapNewAxisToOldAxis),
          m_dims(std::move(dims)),
          m_parentStart(m_poParent->GetDimensionCount()),
          m_parentCount(m_poParent->GetDimensionCount()),
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
    static std::shared_ptr<GDALMDArrayTransposed>
    Create(const std::shared_ptr<GDALMDArray> &poParent,
           const std::vector<int> &anMapNewAxisToOldAxis)
    {
        const auto &parentDims(poParent->GetDimensions());
        std::vector<std::shared_ptr<GDALDimension>> dims;
        for (const auto iOldAxis : anMapNewAxisToOldAxis)
        {
            if (iOldAxis < 0)
            {
                dims.push_back(std::make_shared<GDALDimension>(
                    std::string(), "newaxis", std::string(), std::string(), 1));
            }
            else
            {
                dims.emplace_back(parentDims[iOldAxis]);
            }
        }

        auto newAr(
            std::shared_ptr<GDALMDArrayTransposed>(new GDALMDArrayTransposed(
                poParent, anMapNewAxisToOldAxis, std::move(dims))));
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
            for (size_t i = 0; i < m_anMapNewAxisToOldAxis.size(); i++)
            {
                if (m_anMapNewAxisToOldAxis[i] == srcAxis - 1)
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
        for (size_t i = 0; i < m_anMapNewAxisToOldAxis.size(); ++i)
        {
            const auto iOldAxis = m_anMapNewAxisToOldAxis[i];
            if (iOldAxis >= 0)
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

void GDALMDArrayTransposed::PrepareParentArrays(
    const GUInt64 *arrayStartIdx, const size_t *count, const GInt64 *arrayStep,
    const GPtrDiff_t *bufferStride) const
{
    for (size_t i = 0; i < m_anMapNewAxisToOldAxis.size(); ++i)
    {
        const auto iOldAxis = m_anMapNewAxisToOldAxis[i];
        if (iOldAxis >= 0)
        {
            m_parentStart[iOldAxis] = arrayStartIdx[i];
            m_parentCount[iOldAxis] = count[i];
            if (arrayStep)  // only null when called from IAdviseRead()
            {
                m_parentStep[iOldAxis] = arrayStep[i];
            }
            if (bufferStride)  // only null when called from IAdviseRead()
            {
                m_parentStride[iOldAxis] = bufferStride[i];
            }
        }
    }
}

/************************************************************************/
/*                               IRead()                                */
/************************************************************************/

bool GDALMDArrayTransposed::IRead(const GUInt64 *arrayStartIdx,
                                  const size_t *count, const GInt64 *arrayStep,
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

bool GDALMDArrayTransposed::IWrite(const GUInt64 *arrayStartIdx,
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

bool GDALMDArrayTransposed::IAdviseRead(const GUInt64 *arrayStartIdx,
                                        const size_t *count,
                                        CSLConstList papszOptions) const
{
    PrepareParentArrays(arrayStartIdx, count, nullptr, nullptr);
    return m_poParent->AdviseRead(m_parentStart.data(), m_parentCount.data(),
                                  papszOptions);
}

//! @endcond

/************************************************************************/
/*                             Transpose()                              */
/************************************************************************/

/** Return a view of the array whose axis have been reordered.
 *
 * The anMapNewAxisToOldAxis parameter should contain all the values between 0
 * and GetDimensionCount() - 1, and each only once.
 * -1 can be used as a special index value to ask for the insertion of a new
 * axis of size 1.
 * The new array will have anMapNewAxisToOldAxis.size() axis, and if i is the
 * index of one of its dimension, it corresponds to the axis of index
 * anMapNewAxisToOldAxis[i] from the current array.
 *
 * This is similar to the numpy.transpose() method
 *
 * The returned array holds a reference to the original one, and thus is
 * a view of it (not a copy). If the content of the original array changes,
 * the content of the view array too. The view can be written if the underlying
 * array is writable.
 *
 * Note that I/O performance in such a transposed view might be poor.
 *
 * This is the same as the C function GDALMDArrayTranspose().
 *
 * @return a new array, that holds a reference to the original one, and thus is
 * a view of it (not a copy), or nullptr in case of error.
 */
std::shared_ptr<GDALMDArray>
GDALMDArray::Transpose(const std::vector<int> &anMapNewAxisToOldAxis) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if (!self)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }
    const int nDims = static_cast<int>(GetDimensionCount());
    std::vector<bool> alreadyUsedOldAxis(nDims, false);
    int nCountOldAxis = 0;
    for (const auto iOldAxis : anMapNewAxisToOldAxis)
    {
        if (iOldAxis < -1 || iOldAxis >= nDims)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid axis number");
            return nullptr;
        }
        if (iOldAxis >= 0)
        {
            if (alreadyUsedOldAxis[iOldAxis])
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Axis %d is repeated",
                         iOldAxis);
                return nullptr;
            }
            alreadyUsedOldAxis[iOldAxis] = true;
            nCountOldAxis++;
        }
    }
    if (nCountOldAxis != nDims)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "One or several original axis missing");
        return nullptr;
    }
    return GDALMDArrayTransposed::Create(self, anMapNewAxisToOldAxis);
}
