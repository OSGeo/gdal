/******************************************************************************
 * Name:     gdalmultidim_gltorthorectification.cpp
 * Project:  GDAL Core
 * Purpose:  GLT orthorectification implementation
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
#include <limits>

/************************************************************************/
/*                        GLTOrthoRectifiedArray                        */
/************************************************************************/

class GLTOrthoRectifiedArray final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    std::vector<std::shared_ptr<GDALDimension>> m_apoDims;
    std::vector<GUInt64> m_anBlockSize;
    GDALExtendedDataType m_dt;
    std::shared_ptr<OGRSpatialReference> m_poSRS{};
    std::shared_ptr<GDALMDArray> m_poVarX{};
    std::shared_ptr<GDALMDArray> m_poVarY{};
    std::shared_ptr<GDALMDArray> m_poGLTX{};
    std::shared_ptr<GDALMDArray> m_poGLTY{};
    int m_nGLTIndexOffset = 0;

  protected:
    GLTOrthoRectifiedArray(
        const std::shared_ptr<GDALMDArray> &poParent,
        const std::vector<std::shared_ptr<GDALDimension>> &apoDims,
        const std::vector<GUInt64> &anBlockSize)
        : GDALAbstractMDArray(std::string(), "GLTOrthoRectifiedArray view of " +
                                                 poParent->GetFullName()),
          GDALPamMDArray(std::string(),
                         "GLTOrthoRectifiedArray view of " +
                             poParent->GetFullName(),
                         GDALPamMultiDim::GetPAM(poParent)),
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
           const std::shared_ptr<GDALMDArray> &poGLTX,
           const std::shared_ptr<GDALMDArray> &poGLTY, int nGLTIndexOffset,
           const std::vector<double> &adfGeoTransform)
    {
        std::vector<std::shared_ptr<GDALDimension>> apoNewDims;

        auto poDimY = std::make_shared<GDALDimensionWeakIndexingVar>(
            std::string(), "lat", GDAL_DIM_TYPE_HORIZONTAL_Y, "NORTH",
            poGLTX->GetDimensions()[0]->GetSize());
        auto varY = GDALMDArrayRegularlySpaced::Create(
            std::string(), poDimY->GetName(), poDimY,
            adfGeoTransform[3] + adfGeoTransform[5] / 2, adfGeoTransform[5], 0);
        poDimY->SetIndexingVariable(varY);
        apoNewDims.emplace_back(poDimY);

        auto poDimX = std::make_shared<GDALDimensionWeakIndexingVar>(
            std::string(), "lon", GDAL_DIM_TYPE_HORIZONTAL_X, "EAST",
            poGLTX->GetDimensions()[1]->GetSize());
        auto varX = GDALMDArrayRegularlySpaced::Create(
            std::string(), poDimX->GetName(), poDimX,
            adfGeoTransform[0] + adfGeoTransform[1] / 2, adfGeoTransform[1], 0);
        poDimX->SetIndexingVariable(varX);
        apoNewDims.emplace_back(poDimX);

        if (poParent->GetDimensionCount() == 3)
            apoNewDims.emplace_back(poParent->GetDimensions()[2]);

        std::vector<GUInt64> anBlockSize = std::vector<GUInt64>{
            std::min<GUInt64>(apoNewDims[0]->GetSize(), 512),
            std::min<GUInt64>(apoNewDims[1]->GetSize(), 512)};
        if (poParent->GetDimensionCount() == 3)
        {
            anBlockSize.push_back(poParent->GetDimensions()[2]->GetSize());
        }

        auto newAr(std::shared_ptr<GLTOrthoRectifiedArray>(
            new GLTOrthoRectifiedArray(poParent, apoNewDims, anBlockSize)));
        newAr->SetSelf(newAr);
        newAr->m_poVarX = varX;
        newAr->m_poVarY = varY;
        newAr->m_poGLTX = poGLTX;
        newAr->m_poGLTY = poGLTY;
        newAr->m_nGLTIndexOffset = nGLTIndexOffset;
        OGRSpatialReference oSRS;
        oSRS.importFromEPSG(4326);
        newAr->m_poSRS.reset(oSRS.Clone());
        newAr->m_poSRS->SetDataAxisToSRSAxisMapping(
            {/*latIdx = */ 1, /* lonIdx = */ 2});
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
/*                   GDALMDArrayResampled::IRead()                      */
/************************************************************************/

bool GLTOrthoRectifiedArray::IRead(const GUInt64 *arrayStartIdx,
                                   const size_t *count, const GInt64 *arrayStep,
                                   const GPtrDiff_t *bufferStride,
                                   const GDALExtendedDataType &bufferDataType,
                                   void *pDstBuffer) const
{
    if (bufferDataType.GetClass() != GEDTC_NUMERIC)
        return false;

    const size_t nXYValsCount = count[0] * count[1];
    const auto eInt32DT = GDALExtendedDataType::Create(GDT_Int32);
    std::vector<int32_t> anGLTX;
    std::vector<int32_t> anGLTY;
    try
    {
        anGLTX.resize(nXYValsCount);
        anGLTY.resize(nXYValsCount);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "GLTOrthoRectifiedArray::IRead(): %s", e.what());
        return false;
    }
    if (!m_poGLTX->Read(arrayStartIdx, count, arrayStep, nullptr, eInt32DT,
                        anGLTX.data()) ||
        !m_poGLTY->Read(arrayStartIdx, count, arrayStep, nullptr, eInt32DT,
                        anGLTY.data()))
    {
        return false;
    }

    int32_t nMinX = std::numeric_limits<int32_t>::max();
    int32_t nMaxX = std::numeric_limits<int32_t>::min();
    const auto nXSize = m_poParent->GetDimensions()[0]->GetSize();
    for (size_t i = 0; i < nXYValsCount; ++i)
    {
        const int64_t nX64 =
            static_cast<int64_t>(anGLTX[i]) + m_nGLTIndexOffset;
        if (nX64 >= 0 && static_cast<uint64_t>(nX64) < nXSize)
        {
            const int32_t nX = static_cast<int32_t>(nX64);
            if (nX < nMinX)
                nMinX = nX;
            if (nX > nMaxX)
                nMaxX = nX;
        }
    }

    int32_t nMinY = std::numeric_limits<int32_t>::max();
    int32_t nMaxY = std::numeric_limits<int32_t>::min();
    const auto nYSize = m_poParent->GetDimensions()[0]->GetSize();
    for (size_t i = 0; i < nXYValsCount; ++i)
    {
        const int64_t nY64 =
            static_cast<int64_t>(anGLTY[i]) + m_nGLTIndexOffset;
        if (nY64 >= 0 && static_cast<uint64_t>(nY64) < nYSize)
        {
            const int32_t nY = static_cast<int32_t>(nY64);
            if (nY < nMinY)
                nMinY = nY;
            if (nY > nMaxY)
                nMaxY = nY;
        }
    }

    const auto eBufferDT = bufferDataType.GetNumericDataType();
    auto pRawNoDataValue = GetRawNoDataValue();
    std::vector<GByte> abyNoData(16);
    if (pRawNoDataValue)
        GDALCopyWords(pRawNoDataValue, GetDataType().GetNumericDataType(), 0,
                      abyNoData.data(), eBufferDT, 0, 1);

    const auto nBufferDTSize = bufferDataType.GetSize();
    const int nCopyWordsDstStride =
        m_apoDims.size() == 3
            ? static_cast<int>(bufferStride[2] * nBufferDTSize)
            : 0;
    const int nCopyWordsCount =
        m_apoDims.size() == 3 ? static_cast<int>(count[2]) : 1;
    if (nMinX > nMaxX || nMinY > nMaxY)
    {
        for (size_t iY = 0; iY < count[0]; ++iY)
        {
            for (size_t iX = 0; iX < count[1]; ++iX)
            {
                GByte *pabyDstBuffer =
                    static_cast<GByte *>(pDstBuffer) +
                    (iY * bufferStride[0] + iX * bufferStride[1]) *
                        static_cast<int>(nBufferDTSize);
                GDALCopyWords(abyNoData.data(), eBufferDT, 0, pabyDstBuffer,
                              eBufferDT, nCopyWordsDstStride, nCopyWordsCount);
            }
        }
        return true;
    }

    GUInt64 parentArrayIdxStart[3] = {
        static_cast<GUInt64>(nMinY), static_cast<GUInt64>(nMinX),
        m_apoDims.size() == 3 ? arrayStartIdx[2] : 0};
    size_t parentCount[3] = {static_cast<size_t>(nMaxY - nMinY + 1),
                             static_cast<size_t>(nMaxX - nMinX + 1),
                             m_apoDims.size() == 3 ? count[2] : 1};
    GInt64 parentArrayStep[3] = {1, 1,
                                 m_apoDims.size() == 3 ? arrayStep[2] : 0};

    size_t nParentValueSize = nBufferDTSize;
    for (int i = 0; i < 3; ++i)
    {
        if (parentCount[i] >
            std::numeric_limits<size_t>::max() / nParentValueSize)
        {
            CPLError(
                CE_Failure, CPLE_OutOfMemory,
                "GLTOrthoRectifiedArray::IRead(): too big temporary array");
            return false;
        }
        nParentValueSize *= parentCount[i];
    }

    GPtrDiff_t parentStride[3] = {
        static_cast<GPtrDiff_t>(parentCount[1] * parentCount[2]),
        static_cast<GPtrDiff_t>(parentCount[2]), 1};
    std::vector<GByte> parentValues;
    try
    {
        parentValues.resize(nParentValueSize);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "GLTOrthoRectifiedArray::IRead(): %s", e.what());
        return false;
    }
    if (!m_poParent->Read(parentArrayIdxStart, parentCount, parentArrayStep,
                          parentStride, bufferDataType, parentValues.data()))
    {
        return false;
    }

    size_t iGLTIndex = 0;
    const size_t nXCount = parentCount[1];
    const size_t nBandCount = m_apoDims.size() == 3 ? parentCount[2] : 1;
    for (size_t iY = 0; iY < count[0]; ++iY)
    {
        for (size_t iX = 0; iX < count[1]; ++iX, ++iGLTIndex)
        {
            const int64_t nX64 =
                static_cast<int64_t>(anGLTX[iGLTIndex]) + m_nGLTIndexOffset;
            const int64_t nY64 =
                static_cast<int64_t>(anGLTY[iGLTIndex]) + m_nGLTIndexOffset;
            GByte *pabyDstBuffer =
                static_cast<GByte *>(pDstBuffer) +
                (iY * bufferStride[0] + iX * bufferStride[1]) *
                    static_cast<int>(nBufferDTSize);
            if (nX64 >= nMinX && nX64 <= nMaxX && nY64 >= nMinY &&
                nY64 <= nMaxY)
            {
                const int32_t iSrcX = static_cast<int32_t>(nX64) - nMinX;
                const int32_t iSrcY = static_cast<int32_t>(nY64) - nMinY;
                const GByte *pabySrcBuffer =
                    parentValues.data() +
                    (iSrcY * nXCount + iSrcX) * nBandCount * nBufferDTSize;
                GDALCopyWords(pabySrcBuffer, eBufferDT,
                              static_cast<int>(nBufferDTSize), pabyDstBuffer,
                              eBufferDT, nCopyWordsDstStride, nCopyWordsCount);
            }
            else
            {
                GDALCopyWords(abyNoData.data(), eBufferDT, 0, pabyDstBuffer,
                              eBufferDT, nCopyWordsDstStride, nCopyWordsCount);
            }
        }
    }

    return true;
}

/************************************************************************/
/*                     CreateGLTOrthorectified()                        */
/************************************************************************/

//! @cond Doxygen_Suppress

/* static */ std::shared_ptr<GDALMDArray> GDALMDArray::CreateGLTOrthorectified(
    const std::shared_ptr<GDALMDArray> &poParent,
    const std::shared_ptr<GDALMDArray> &poGLTX,
    const std::shared_ptr<GDALMDArray> &poGLTY, int nGLTIndexOffset,
    const std::vector<double> &adfGeoTransform)
{
    return GLTOrthoRectifiedArray::Create(poParent, poGLTX, poGLTY,
                                          nGLTIndexOffset, adfGeoTransform);
}

//! @endcond
