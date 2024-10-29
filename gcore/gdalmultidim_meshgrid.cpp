/******************************************************************************
 * Name:     gdalmultiDim_meshgrid.cpp
 * Project:  GDAL Core
 * Purpose:  Return a vector of coordinate matrices from coordinate vectors.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_priv.h"

#include <algorithm>
#include <limits>

/************************************************************************/
/*                       GetConcatenatedNames()                         */
/************************************************************************/

static std::string
GetConcatenatedNames(const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays)
{
    std::string ret;
    for (const auto &poArray : apoArrays)
    {
        if (!ret.empty())
            ret += ", ";
        ret += poArray->GetFullName();
    }
    return ret;
}

/************************************************************************/
/*                         GDALMDArrayMeshGrid                          */
/************************************************************************/

class GDALMDArrayMeshGrid final : public GDALMDArray
{
    const std::vector<std::shared_ptr<GDALMDArray>> m_apoArrays;
    std::vector<std::shared_ptr<GDALDimension>> m_apoDims{};
    const size_t m_iDim;
    const bool m_bIJIndexing;

  protected:
    explicit GDALMDArrayMeshGrid(
        const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays,
        const std::vector<std::shared_ptr<GDALDimension>> &apoDims, size_t iDim,
        bool bIJIndexing)
        : GDALAbstractMDArray(std::string(),
                              "Mesh grid view of " +
                                  GetConcatenatedNames(apoArrays)),
          GDALMDArray(std::string(),
                      "Mesh grid view of " + GetConcatenatedNames(apoArrays)),
          m_apoArrays(apoArrays), m_apoDims(apoDims), m_iDim(iDim),
          m_bIJIndexing(bIJIndexing)
    {
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    static std::shared_ptr<GDALMDArrayMeshGrid>
    Create(const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays,
           size_t iDim, bool bIJIndexing)
    {
        std::vector<std::shared_ptr<GDALDimension>> apoDims;
        for (size_t i = 0; i < apoArrays.size(); ++i)
        {
            const size_t iTranslatedDim = (!bIJIndexing && i <= 1) ? 1 - i : i;
            apoDims.push_back(apoArrays[iTranslatedDim]->GetDimensions()[0]);
        }
        auto newAr(std::shared_ptr<GDALMDArrayMeshGrid>(
            new GDALMDArrayMeshGrid(apoArrays, apoDims, iDim, bIJIndexing)));
        newAr->SetSelf(newAr);
        return newAr;
    }

    bool IsWritable() const override
    {
        return false;
    }

    const std::string &GetFilename() const override
    {
        return m_apoArrays[m_iDim]->GetFilename();
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_apoDims;
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_apoArrays[m_iDim]->GetDataType();
    }

    std::shared_ptr<GDALAttribute>
    GetAttribute(const std::string &osName) const override
    {
        return m_apoArrays[m_iDim]->GetAttribute(osName);
    }

    std::vector<std::shared_ptr<GDALAttribute>>
    GetAttributes(CSLConstList papszOptions = nullptr) const override
    {
        return m_apoArrays[m_iDim]->GetAttributes(papszOptions);
    }

    const std::string &GetUnit() const override
    {
        return m_apoArrays[m_iDim]->GetUnit();
    }

    const void *GetRawNoDataValue() const override
    {
        return m_apoArrays[m_iDim]->GetRawNoDataValue();
    }

    double GetOffset(bool *pbHasOffset,
                     GDALDataType *peStorageType) const override
    {
        return m_apoArrays[m_iDim]->GetOffset(pbHasOffset, peStorageType);
    }

    double GetScale(bool *pbHasScale,
                    GDALDataType *peStorageType) const override
    {
        return m_apoArrays[m_iDim]->GetScale(pbHasScale, peStorageType);
    }
};

/************************************************************************/
/*                             IRead()                                  */
/************************************************************************/

bool GDALMDArrayMeshGrid::IRead(const GUInt64 *arrayStartIdx,
                                const size_t *count, const GInt64 *arrayStep,
                                const GPtrDiff_t *bufferStride,
                                const GDALExtendedDataType &bufferDataType,
                                void *pDstBuffer) const
{
    const size_t nBufferDTSize = bufferDataType.GetSize();
    const size_t iTranslatedDim =
        (!m_bIJIndexing && m_iDim <= 1) ? 1 - m_iDim : m_iDim;
    std::vector<GByte> abyTmpData(nBufferDTSize * count[iTranslatedDim]);
    const GPtrDiff_t strideOne[] = {1};
    if (!m_apoArrays[m_iDim]->Read(&arrayStartIdx[iTranslatedDim],
                                   &count[iTranslatedDim],
                                   &arrayStep[iTranslatedDim], strideOne,
                                   bufferDataType, abyTmpData.data()))
        return false;

    const auto nDims = GetDimensionCount();

    struct Stack
    {
        size_t nIters = 0;
        GByte *dst_ptr = nullptr;
        GPtrDiff_t dst_inc_offset = 0;
    };

    // +1 to avoid -Werror=null-dereference
    std::vector<Stack> stack(nDims + 1);
    for (size_t i = 0; i < nDims; i++)
    {
        stack[i].dst_inc_offset =
            static_cast<GPtrDiff_t>(bufferStride[i] * nBufferDTSize);
    }
    stack[0].dst_ptr = static_cast<GByte *>(pDstBuffer);
    size_t dimIdx = 0;
    size_t valIdx = 0;

lbl_next_depth:
    if (dimIdx == nDims - 1)
    {
        auto nIters = count[dimIdx];
        GByte *dst_ptr = stack[dimIdx].dst_ptr;
        if (dimIdx == iTranslatedDim)
        {
            valIdx = 0;
            while (true)
            {
                GDALExtendedDataType::CopyValue(
                    &abyTmpData[nBufferDTSize * valIdx], bufferDataType,
                    dst_ptr, bufferDataType);
                if ((--nIters) == 0)
                    break;
                ++valIdx;
                dst_ptr += stack[dimIdx].dst_inc_offset;
            }
        }
        else
        {
            while (true)
            {
                GDALExtendedDataType::CopyValue(
                    &abyTmpData[nBufferDTSize * valIdx], bufferDataType,
                    dst_ptr, bufferDataType);
                if ((--nIters) == 0)
                    break;
                dst_ptr += stack[dimIdx].dst_inc_offset;
            }
        }
    }
    else
    {
        if (dimIdx == iTranslatedDim)
            valIdx = 0;
        stack[dimIdx].nIters = count[dimIdx];
        while (true)
        {
            dimIdx++;
            stack[dimIdx].dst_ptr = stack[dimIdx - 1].dst_ptr;
            goto lbl_next_depth;
        lbl_return_to_caller:
            dimIdx--;
            if ((--stack[dimIdx].nIters) == 0)
                break;
            if (dimIdx == iTranslatedDim)
                ++valIdx;
            stack[dimIdx].dst_ptr += stack[dimIdx].dst_inc_offset;
        }
    }
    if (dimIdx > 0)
    {
        goto lbl_return_to_caller;
    }

    if (bufferDataType.NeedsFreeDynamicMemory())
    {
        for (size_t i = 0; i < count[iTranslatedDim]; ++i)
        {
            bufferDataType.FreeDynamicMemory(&abyTmpData[i * nBufferDTSize]);
        }
    }

    return true;
}

/************************************************************************/
/*                      GDALMDArrayGetMeshGrid()                        */
/************************************************************************/

/** Return a list of multidimensional arrays from a list of one-dimensional
 * arrays.
 *
 * This is typically used to transform one-dimensional longitude, latitude
 * arrays into 2D ones.
 *
 * More formally, for one-dimensional arrays x1, x2,..., xn with lengths
 * Ni=len(xi), returns (N1, N2, ..., Nn) shaped arrays if indexing="ij" or
 * (N2, N1, ..., Nn) shaped arrays if indexing="xy" with the elements of xi
 * repeated to fill the matrix along the first dimension for x1, the second
 * for x2 and so on.
 *
 * For example, if x = [1, 2], and y = [3, 4, 5],
 * GetMeshGrid([x, y], ["INDEXING=xy"]) will return [xm, ym] such that
 * xm=[[1, 2],[1, 2],[1, 2]] and ym=[[3, 3],[4, 4],[5, 5]],
 * or more generally xm[any index][i] = x[i] and ym[i][any index]=y[i]
 *
 * and
 * GetMeshGrid([x, y], ["INDEXING=ij"]) will return [xm, ym] such that
 * xm=[[1, 1, 1],[2, 2, 2]] and ym=[[3, 4, 5],[3, 4, 5]],
 * or more generally xm[i][any index] = x[i] and ym[any index][i]=y[i]
 *
 * The currently supported options are:
 * <ul>
 * <li>INDEXING=xy/ij: Cartesian ("xy", default) or matrix ("ij") indexing of
 * output.
 * </li>
 * </ul>
 *
 * This is the same as
 * <a href="https://numpy.org/doc/stable/reference/generated/numpy.meshgrid.html">numpy.meshgrid()</a>
 * function.
 *
 * This is the same as the C function GDALMDArrayGetMeshGrid()
 *
 * @param apoArrays Input arrays
 * @param papszOptions NULL, or NULL terminated list of options.
 *
 * @return an array of coordinate matrices
 * @since 3.10
 */

/* static */ std::vector<std::shared_ptr<GDALMDArray>> GDALMDArray::GetMeshGrid(
    const std::vector<std::shared_ptr<GDALMDArray>> &apoArrays,
    CSLConstList papszOptions)
{
    std::vector<std::shared_ptr<GDALMDArray>> ret;
    for (const auto &poArray : apoArrays)
    {
        if (poArray->GetDimensionCount() != 1)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Only 1-D input arrays are accepted");
            return ret;
        }
    }

    const char *pszIndexing =
        CSLFetchNameValueDef(papszOptions, "INDEXING", "xy");
    if (!EQUAL(pszIndexing, "xy") && !EQUAL(pszIndexing, "ij"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only INDEXING=xy or ij is accepted");
        return ret;
    }
    const bool bIJIndexing = EQUAL(pszIndexing, "ij");

    for (size_t i = 0; i < apoArrays.size(); ++i)
    {
        ret.push_back(GDALMDArrayMeshGrid::Create(apoArrays, i, bIJIndexing));
    }

    return ret;
}
