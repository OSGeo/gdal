/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  Zarr driver, "transpose" codec
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "zarr_v3_codec.h"

// Implements https://zarr-specs.readthedocs.io/en/latest/v3/codecs/transpose/index.html

/************************************************************************/
/*                       ZarrV3CodecTranspose()                         */
/************************************************************************/

ZarrV3CodecTranspose::ZarrV3CodecTranspose() : ZarrV3Codec(NAME)
{
}

/************************************************************************/
/*                             IsNoOp()                                 */
/************************************************************************/

bool ZarrV3CodecTranspose::IsNoOp() const
{
    for (int i = 0; i < static_cast<int>(m_anOrder.size()); ++i)
    {
        if (m_anOrder[i] != i)
            return false;
    }
    return true;
}

/************************************************************************/
/*                           GetConfiguration()                         */
/************************************************************************/

/* static */ CPLJSONObject
ZarrV3CodecTranspose::GetConfiguration(const std::vector<int> &anOrder)
{
    CPLJSONObject oConfig;
    CPLJSONArray oOrder;
    for (const auto nVal : anOrder)
        oOrder.Add(nVal);
    oConfig.Add("order", oOrder);
    return oConfig;
}

/************************************************************************/
/*                ZarrV3CodecTranspose::InitFromConfiguration()         */
/************************************************************************/

bool ZarrV3CodecTranspose::InitFromConfiguration(
    const CPLJSONObject &configuration,
    const ZarrArrayMetadata &oInputArrayMetadata,
    ZarrArrayMetadata &oOutputArrayMetadata, bool /* bEmitWarnings */)
{
    m_oConfiguration = configuration.Clone();
    m_oInputArrayMetadata = oInputArrayMetadata;
    oOutputArrayMetadata = oInputArrayMetadata;

    if (!configuration.IsValid() &&
        configuration.GetType() != CPLJSONObject::Type::Object)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec transpose: configuration missing or not an object");
        return false;
    }

    for (const auto &oChild : configuration.GetChildren())
    {
        if (oChild.GetName() != "order")
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec transpose: configuration contains a unhandled "
                     "member: %s",
                     oChild.GetName().c_str());
            return false;
        }
    }

    const auto oOrder = configuration.GetObj("order");
    const int nDims = static_cast<int>(oInputArrayMetadata.anBlockSizes.size());
    if (oOrder.GetType() == CPLJSONObject::Type::String)
    {
        // Deprecated
        const auto osOrder = oOrder.ToString();
        if (osOrder == "C")
        {
            for (int i = 0; i < nDims; ++i)
            {
                m_anOrder.push_back(i);
            }
        }
        else if (osOrder == "F")
        {
            for (int i = 0; i < nDims; ++i)
            {
                m_anOrder.push_back(nDims - 1 - i);
                oOutputArrayMetadata.anBlockSizes[i] =
                    oInputArrayMetadata.anBlockSizes[nDims - 1 - i];
            }
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec transpose: invalid value for order");
            return false;
        }
    }
    else if (oOrder.GetType() == CPLJSONObject::Type::Array)
    {
        const auto oOrderArray = oOrder.ToArray();
        if (oOrderArray.Size() != nDims)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Codec transpose: order[] does not have the expected "
                     "number of elements");
            return false;
        }
        std::vector<int> oSet(nDims);
        oOutputArrayMetadata.anBlockSizes.clear();
        for (const auto &oVal : oOrderArray)
        {
            const int nVal = oVal.ToInteger();
            if (nVal < 0 || nVal >= nDims || oSet[nVal])
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Codec transpose: order[] does not define a valid "
                         "transposition");
                return false;
            }
            oSet[nVal] = true;
            m_anOrder.push_back(nVal);
            oOutputArrayMetadata.anBlockSizes.push_back(
                oInputArrayMetadata.anBlockSizes[nVal]);
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Codec transpose: invalid value for order");
        return false;
    }

    int i = 0;
    m_anReverseOrder.resize(m_anOrder.size());
    for (const auto nVal : m_anOrder)
    {
        m_anReverseOrder[nVal] = i;
        ++i;
    }

    return true;
}

/************************************************************************/
/*              ZarrV3CodecTranspose::GetInnerMostBlockSize()           */
/************************************************************************/

std::vector<size_t> ZarrV3CodecTranspose::GetInnerMostBlockSize(
    const std::vector<size_t> &anInnerBlockSize) const
{
    std::vector<size_t> ret;
    for (int idx : m_anReverseOrder)
        ret.push_back(anInnerBlockSize[idx]);
    return ret;
}

/************************************************************************/
/*                   ZarrV3CodecTranspose::Clone()                      */
/************************************************************************/

std::unique_ptr<ZarrV3Codec> ZarrV3CodecTranspose::Clone() const
{
    auto psClone = std::make_unique<ZarrV3CodecTranspose>();
    ZarrArrayMetadata oOutputArrayMetadata;
    psClone->InitFromConfiguration(m_oConfiguration, m_oInputArrayMetadata,
                                   oOutputArrayMetadata,
                                   /* bEmitWarnings = */ false);
    return psClone;
}

/************************************************************************/
/*                  ZarrV3CodecTranspose::Transpose()                   */
/************************************************************************/

bool ZarrV3CodecTranspose::Transpose(
    const ZarrByteVectorQuickResize &abySrc, ZarrByteVectorQuickResize &abyDst,
    bool bEncodeDirection, const std::vector<size_t> &anForwardBlockSizes) const
{
    CPLAssert(m_anOrder.size() == anForwardBlockSizes.size());
    CPLAssert(m_anReverseOrder.size() == anForwardBlockSizes.size());
    const size_t nDims = m_anOrder.size();
    const size_t nSourceSize = m_oInputArrayMetadata.oElt.nativeSize;
    CPLAssert(nDims > 0);
    if (abySrc.size() < MultiplyElements(anForwardBlockSizes) * nSourceSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ZarrV3CodecTranspose::Transpose(): input buffer too small");
        return false;
    }
    abyDst.resize(MultiplyElements(anForwardBlockSizes) * nSourceSize);

    struct Stack
    {
        size_t nIters = 0;
        const GByte *src_ptr = nullptr;
        GByte *dst_ptr = nullptr;
        size_t src_inc_offset = 0;
        size_t dst_inc_offset = 0;
    };

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
    std::vector<Stack> stack(nDims);
    stack.emplace_back(
        Stack());  // to make gcc 9.3 -O2 -Wnull-dereference happy
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    if (!bEncodeDirection)
    {
        stack[m_anReverseOrder[nDims - 1]].src_inc_offset = nSourceSize;
        size_t nStride = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            nStride *= anForwardBlockSizes[m_anReverseOrder[i + 1]];
            stack[m_anReverseOrder[i]].src_inc_offset = nStride;
        }

        stack[nDims - 1].dst_inc_offset = nSourceSize;
        nStride = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            nStride *= anForwardBlockSizes[i + 1];
            stack[i].dst_inc_offset = nStride;
        }
    }
    else
    {
        stack[m_anReverseOrder[nDims - 1]].dst_inc_offset = nSourceSize;
        size_t nStride = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            nStride *= anForwardBlockSizes[m_anReverseOrder[i + 1]];
            stack[m_anReverseOrder[i]].dst_inc_offset = nStride;
        }

        stack[nDims - 1].src_inc_offset = nSourceSize;
        nStride = nSourceSize;
        for (size_t i = nDims - 1; i > 0;)
        {
            --i;
            nStride *= anForwardBlockSizes[i + 1];
            stack[i].src_inc_offset = nStride;
        }
    }

    stack[0].src_ptr = abySrc.data();
    stack[0].dst_ptr = &abyDst[0];

    size_t dimIdx = 0;
lbl_next_depth:
    if (dimIdx == nDims)
    {
        void *dst_ptr = stack[nDims].dst_ptr;
        const void *src_ptr = stack[nDims].src_ptr;
        if (nSourceSize == 1)
            *stack[nDims].dst_ptr = *stack[nDims].src_ptr;
        else if (nSourceSize == 2)
            *static_cast<uint16_t *>(dst_ptr) =
                *static_cast<const uint16_t *>(src_ptr);
        else if (nSourceSize == 4)
            *static_cast<uint32_t *>(dst_ptr) =
                *static_cast<const uint32_t *>(src_ptr);
        else if (nSourceSize == 8)
            *static_cast<uint64_t *>(dst_ptr) =
                *static_cast<const uint64_t *>(src_ptr);
        else
            memcpy(dst_ptr, src_ptr, nSourceSize);
    }
    else
    {
        stack[dimIdx].nIters = anForwardBlockSizes[dimIdx];
        while (true)
        {
            dimIdx++;
            stack[dimIdx].src_ptr = stack[dimIdx - 1].src_ptr;
            stack[dimIdx].dst_ptr = stack[dimIdx - 1].dst_ptr;
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if ((--stack[dimIdx].nIters) == 0)
                break;
            stack[dimIdx].src_ptr += stack[dimIdx].src_inc_offset;
            stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    if (dimIdx > 0)
        goto lbl_return_to_caller;

    return true;
}

/************************************************************************/
/*                    ZarrV3CodecTranspose::Encode()                    */
/************************************************************************/

bool ZarrV3CodecTranspose::Encode(const ZarrByteVectorQuickResize &abySrc,
                                  ZarrByteVectorQuickResize &abyDst) const
{
    CPLAssert(!IsNoOp());

    return Transpose(abySrc, abyDst, true, m_oInputArrayMetadata.anBlockSizes);
}

/************************************************************************/
/*                    ZarrV3CodecTranspose::Decode()                    */
/************************************************************************/

bool ZarrV3CodecTranspose::Decode(const ZarrByteVectorQuickResize &abySrc,
                                  ZarrByteVectorQuickResize &abyDst) const
{
    CPLAssert(!IsNoOp());

    return Transpose(abySrc, abyDst, false, m_oInputArrayMetadata.anBlockSizes);
}

/************************************************************************/
/*                   ZarrV3CodecTranspose::DecodePartial()              */
/************************************************************************/

bool ZarrV3CodecTranspose::DecodePartial(
    VSIVirtualHandle * /* poFile */, const ZarrByteVectorQuickResize &abySrc,
    ZarrByteVectorQuickResize &abyDst, std::vector<size_t> &anStartIdx,
    std::vector<size_t> &anCount)
{
    CPLAssert(anStartIdx.size() == m_oInputArrayMetadata.anBlockSizes.size());
    CPLAssert(anStartIdx.size() == anCount.size());

    Reorder1DInverse(anStartIdx);
    Reorder1DInverse(anCount);

    // Note that we don't need to take anStartIdx into account for the
    // transpose operation, as abySrc corresponds to anStartIdx.
    return Transpose(abySrc, abyDst, false, anCount);
}
