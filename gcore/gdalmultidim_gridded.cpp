/******************************************************************************
 * $Id$
 *
 * Name:     gdalmultidim_gridded.cpp
 * Project:  GDAL Core
 * Purpose:  GDALMDArray::GetGridded() implementation
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

#include "gdal_alg.h"
#include "gdalgrid.h"
#include "gdal_priv.h"
#include "gdal_pam.h"
#include "ogrsf_frmts.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <new>

/************************************************************************/
/*                         GDALMDArrayGridded                           */
/************************************************************************/

class GDALMDArrayGridded final : public GDALPamMDArray
{
  private:
    std::shared_ptr<GDALMDArray> m_poParent{};
    std::vector<std::shared_ptr<GDALDimension>> m_apoDims{};
    std::shared_ptr<GDALMDArray> m_poVarX{};
    std::shared_ptr<GDALMDArray> m_poVarY{};
    std::unique_ptr<GDALDataset> m_poVectorDS{};
    GDALGridAlgorithm m_eAlg;
    std::unique_ptr<void, VSIFreeReleaser> m_poGridOptions;
    const GDALExtendedDataType m_dt;
    std::vector<GUInt64> m_anBlockSize{};
    const double m_dfNoDataValue;
    const double m_dfMinX;
    const double m_dfResX;
    const double m_dfMinY;
    const double m_dfResY;
    const double m_dfRadius;
    mutable std::vector<GUInt64> m_anLastStartIdx{};
    mutable std::vector<double> m_adfZ{};

  protected:
    explicit GDALMDArrayGridded(
        const std::shared_ptr<GDALMDArray> &poParent,
        const std::vector<std::shared_ptr<GDALDimension>> &apoDims,
        const std::shared_ptr<GDALMDArray> &poVarX,
        const std::shared_ptr<GDALMDArray> &poVarY,
        std::unique_ptr<GDALDataset> &&poVectorDS, GDALGridAlgorithm eAlg,
        std::unique_ptr<void, VSIFreeReleaser> &&poGridOptions,
        double dfNoDataValue, double dfMinX, double dfResX, double dfMinY,
        double dfResY, double dfRadius)
        : GDALAbstractMDArray(std::string(),
                              "Gridded view of " + poParent->GetFullName()),
          GDALPamMDArray(
              std::string(), "Gridded view of " + poParent->GetFullName(),
              GDALPamMultiDim::GetPAM(poParent), poParent->GetContext()),
          m_poParent(std::move(poParent)), m_apoDims(apoDims), m_poVarX(poVarX),
          m_poVarY(poVarY), m_poVectorDS(std::move(poVectorDS)), m_eAlg(eAlg),
          m_poGridOptions(std::move(poGridOptions)),
          m_dt(GDALExtendedDataType::Create(GDT_Float64)),
          m_dfNoDataValue(dfNoDataValue), m_dfMinX(dfMinX), m_dfResX(dfResX),
          m_dfMinY(dfMinY), m_dfResY(dfResY), m_dfRadius(dfRadius)
    {
        const auto anParentBlockSize = m_poParent->GetBlockSize();
        m_anBlockSize.resize(m_apoDims.size());
        for (size_t i = 0; i + 1 < m_apoDims.size(); ++i)
            m_anBlockSize[i] = anParentBlockSize[i];
        m_anBlockSize[m_apoDims.size() - 2] = 256;
        m_anBlockSize[m_apoDims.size() - 1] = 256;
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  public:
    static std::shared_ptr<GDALMDArrayGridded>
    Create(const std::shared_ptr<GDALMDArray> &poParent,
           const std::vector<std::shared_ptr<GDALDimension>> &apoDims,
           const std::shared_ptr<GDALMDArray> &poVarX,
           const std::shared_ptr<GDALMDArray> &poVarY,
           std::unique_ptr<GDALDataset> &&poVectorDS, GDALGridAlgorithm eAlg,
           std::unique_ptr<void, VSIFreeReleaser> &&poGridOptions,
           double dfNoDataValue, double dfMinX, double dfResX, double dfMinY,
           double dfResY, double dfRadius)
    {
        auto newAr(std::shared_ptr<GDALMDArrayGridded>(new GDALMDArrayGridded(
            poParent, apoDims, poVarX, poVarY, std::move(poVectorDS), eAlg,
            std::move(poGridOptions), dfNoDataValue, dfMinX, dfResX, dfMinY,
            dfResY, dfRadius)));
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

    const void *GetRawNoDataValue() const override
    {
        return &m_dfNoDataValue;
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        return m_anBlockSize;
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
/*                             IRead()                                  */
/************************************************************************/

bool GDALMDArrayGridded::IRead(const GUInt64 *arrayStartIdx,
                               const size_t *count, const GInt64 *arrayStep,
                               const GPtrDiff_t *bufferStride,
                               const GDALExtendedDataType &bufferDataType,
                               void *pDstBuffer) const
{
    if (bufferDataType.GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALMDArrayGridded::IRead() only support numeric "
                 "bufferDataType");
        return false;
    }
    const auto nDims = GetDimensionCount();

    std::vector<GUInt64> anStartIdx;
    for (size_t i = 0; i + 2 < nDims; ++i)
    {
        anStartIdx.push_back(arrayStartIdx[i]);
        if (count[i] != 1)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "GDALMDArrayGridded::IRead() only support count = 1 in "
                     "the first dimensions, except the last 2 Y,X ones");
            return false;
        }
    }

    const auto iDimX = nDims - 1;
    const auto iDimY = nDims - 2;

    if (arrayStep[iDimX] < 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALMDArrayGridded::IRead(): "
                 "arrayStep[iDimX] < 0 not supported");
        return false;
    }

    if (arrayStep[iDimY] < 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GDALMDArrayGridded::IRead(): "
                 "arrayStep[iDimY] < 0 not supported");
        return false;
    }

    // Load the values taken by the variable at the considered slice
    // (if not already done)
    if (m_adfZ.empty() || m_anLastStartIdx != anStartIdx)
    {
        std::vector<GUInt64> anTempStartIdx(anStartIdx);
        anTempStartIdx.push_back(0);
        const std::vector<GInt64> anTempArrayStep(
            m_poParent->GetDimensionCount(), 1);
        std::vector<GPtrDiff_t> anTempBufferStride(
            m_poParent->GetDimensionCount() - 1, 0);
        anTempBufferStride.push_back(1);
        std::vector<size_t> anTempCount(m_poParent->GetDimensionCount() - 1, 1);
        anTempCount.push_back(
            static_cast<size_t>(m_poParent->GetDimensions().back()->GetSize()));
        CPLAssert(anTempStartIdx.size() == m_poParent->GetDimensionCount());
        CPLAssert(anTempCount.size() == m_poParent->GetDimensionCount());
        CPLAssert(anTempArrayStep.size() == m_poParent->GetDimensionCount());
        CPLAssert(anTempBufferStride.size() == m_poParent->GetDimensionCount());

        try
        {
            m_adfZ.resize(anTempCount.back());
        }
        catch (const std::bad_alloc &e)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
            return false;
        }

        if (!m_poParent->Read(anTempStartIdx.data(), anTempCount.data(),
                              anTempArrayStep.data(), anTempBufferStride.data(),
                              m_dt, m_adfZ.data()))
        {
            return false;
        }
        // GCC 13.1 warns here. Definitely a false positive.
#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#endif
        m_anLastStartIdx = std::move(anStartIdx);
#if defined(__GNUC__) && __GNUC__ >= 13
#pragma GCC diagnostic pop
#endif
    }

    // Determine the X,Y spatial extent of the request
    const double dfX1 = m_dfMinX + arrayStartIdx[iDimX] * m_dfResX;
    const double dfX2 = m_dfMinX + (arrayStartIdx[iDimX] +
                                    (count[iDimX] - 1) * arrayStep[iDimX]) *
                                       m_dfResX;
    const double dfMinX = std::min(dfX1, dfX2) - m_dfResX / 2;
    const double dfMaxX = std::max(dfX1, dfX2) + m_dfResX / 2;

    const double dfY1 = m_dfMinY + arrayStartIdx[iDimY] * m_dfResY;
    const double dfY2 = m_dfMinY + (arrayStartIdx[iDimY] +
                                    (count[iDimY] - 1) * arrayStep[iDimY]) *
                                       m_dfResY;
    const double dfMinY = std::min(dfY1, dfY2) - m_dfResY / 2;
    const double dfMaxY = std::max(dfY1, dfY2) + m_dfResY / 2;

    // Extract relevant variable values
    auto poLyr = m_poVectorDS->GetLayer(0);
    if (!(arrayStartIdx[iDimX] == 0 &&
          arrayStartIdx[iDimX] + (count[iDimX] - 1) * arrayStep[iDimX] ==
              m_apoDims[iDimX]->GetSize() - 1 &&
          arrayStartIdx[iDimY] == 0 &&
          arrayStartIdx[iDimY] + (count[iDimY] - 1) * arrayStep[iDimY] ==
              m_apoDims[iDimY]->GetSize() - 1))
    {
        poLyr->SetSpatialFilterRect(dfMinX - m_dfRadius, dfMinY - m_dfRadius,
                                    dfMaxX + m_dfRadius, dfMaxY + m_dfRadius);
    }
    else
    {
        poLyr->SetSpatialFilter(nullptr);
    }
    std::vector<double> adfX;
    std::vector<double> adfY;
    std::vector<double> adfZ;
    try
    {
        for (auto &&poFeat : poLyr)
        {
            const auto poGeom = poFeat->GetGeometryRef();
            CPLAssert(poGeom);
            CPLAssert(poGeom->getGeometryType() == wkbPoint);
            adfX.push_back(poGeom->toPoint()->getX());
            adfY.push_back(poGeom->toPoint()->getY());
            const auto nIdxInDataset = poFeat->GetFieldAsInteger64(0);
            assert(nIdxInDataset >= 0 &&
                   static_cast<size_t>(nIdxInDataset) < m_adfZ.size());
            adfZ.push_back(m_adfZ[static_cast<size_t>(nIdxInDataset)]);
        }
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }

    const size_t nXSize =
        static_cast<size_t>((count[iDimX] - 1) * arrayStep[iDimX] + 1);
    const size_t nYSize =
        static_cast<size_t>((count[iDimY] - 1) * arrayStep[iDimY] + 1);
    if (nXSize > std::numeric_limits<GUInt32>::max() / nYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too many points queried at once");
        return false;
    }
    std::vector<double> adfRes;
    try
    {
        adfRes.resize(nXSize * nYSize, m_dfNoDataValue);
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return false;
    }
#if 0
    CPLDebug("GDAL",
             "dfMinX=%f, dfMaxX=%f, dfMinY=%f, dfMaxY=%f",
             dfMinX, dfMaxX, dfMinY, dfMaxY);
#endif

    // Finally do the gridded interpolation
    if (!adfX.empty() &&
        GDALGridCreate(
            m_eAlg, m_poGridOptions.get(), static_cast<GUInt32>(adfX.size()),
            adfX.data(), adfY.data(), adfZ.data(), dfMinX, dfMaxX, dfMinY,
            dfMaxY, static_cast<GUInt32>(nXSize), static_cast<GUInt32>(nYSize),
            GDT_Float64, adfRes.data(), nullptr, nullptr) != CE_None)
    {
        return false;
    }

    // Copy interpolated data into destination buffer
    GByte *pabyDestBuffer = static_cast<GByte *>(pDstBuffer);
    const auto eBufferDT = bufferDataType.GetNumericDataType();
    const auto nBufferDTSize = GDALGetDataTypeSizeBytes(eBufferDT);
    for (size_t iY = 0; iY < count[iDimY]; ++iY)
    {
        GDALCopyWords64(
            adfRes.data() + iY * arrayStep[iDimY] * nXSize, GDT_Float64,
            static_cast<int>(sizeof(double) * arrayStep[iDimX]),
            pabyDestBuffer + iY * bufferStride[iDimY] * nBufferDTSize,
            eBufferDT, static_cast<int>(bufferStride[iDimX] * nBufferDTSize),
            static_cast<GPtrDiff_t>(count[iDimX]));
    }

    return true;
}

/************************************************************************/
/*                            GetGridded()                              */
/************************************************************************/

/** Return a gridded array from scattered point data, that is from an array
 * whose last dimension is the indexing variable of X and Y arrays.
 *
 * The gridding is done in 2D, using GDALGridCreate(), on-the-fly at Read()
 * time, taking into account the spatial extent of the request to limit the
 * gridding. The results got on the whole extent or a subset of it might not be
 * strictly identical depending on the gridding algorithm and its radius.
 * Setting a radius in osGridOptions is recommended to improve performance.
 * For arrays which have more dimensions than the dimension of the indexing
 * variable of the X and Y arrays, Read() must be called on slices of the extra
 * dimensions (ie count[i] must be set to 1, except for the X and Y dimensions
 * of the array returned by this method).
 *
 * This is the same as the C function GDALMDArrayGetGridded().
 *
 * @param osGridOptions Gridding algorithm and options.
 * e.g. "invdist:nodata=nan:radius1=1:radius2=1:max_points=5".
 * See documentation of the <a href="/programs/gdal_grid.html">gdal_grid</a>
 * utility for all options.
 * @param poXArrayIn Single-dimension array containing X values, and whose
 * dimension is the last one of this array. If set to nullptr, the "coordinates"
 * attribute must exist on this array, and the X variable will be the (N-1)th one
 * mentioned in it, unless there is a "x" or "lon" variable in "coordinates".
 * @param poYArrayIn Single-dimension array containing Y values, and whose
 * dimension is the last one of this array. If set to nullptr, the "coordinates"
 * attribute must exist on this array, and the Y variable will be the (N-2)th one
 * mentioned in it,  unless there is a "y" or "lat" variable in "coordinates".
 * @param papszOptions NULL terminated list of options, or nullptr. Supported
 * options are:
 * <ul>
 * <li>RESOLUTION=val: Spatial resolution of the returned array. If not set,
 * will be guessed from the typical spacing of (X,Y) points.</li>
 * </ul>
 *
 * @return gridded array, or nullptr in case of error.
 *
 * @since GDAL 3.7
 */
std::shared_ptr<GDALMDArray>
GDALMDArray::GetGridded(const std::string &osGridOptions,
                        const std::shared_ptr<GDALMDArray> &poXArrayIn,
                        const std::shared_ptr<GDALMDArray> &poYArrayIn,
                        CSLConstList papszOptions) const
{
    auto self = std::dynamic_pointer_cast<GDALMDArray>(m_pSelf.lock());
    if (!self)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Driver implementation issue: m_pSelf not set !");
        return nullptr;
    }

    GDALGridAlgorithm eAlg;
    void *pOptions = nullptr;
    if (GDALGridParseAlgorithmAndOptions(osGridOptions.c_str(), &eAlg,
                                         &pOptions) != CE_None)
    {
        return nullptr;
    }

    std::unique_ptr<void, VSIFreeReleaser> poGridOptions(pOptions);

    if (GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetDataType().GetClass() != GEDTC_NUMERIC");
        return nullptr;
    }

    if (GetDimensionCount() == 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "GetDimensionCount() == 0");
        return nullptr;
    }

    if (poXArrayIn && !poYArrayIn)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "As poXArrayIn is specified, poYArrayIn must also be specified");
        return nullptr;
    }
    else if (!poXArrayIn && poYArrayIn)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "As poYArrayIn is specified, poXArrayIn must also be specified");
        return nullptr;
    }
    std::shared_ptr<GDALMDArray> poXArray = poXArrayIn;
    std::shared_ptr<GDALMDArray> poYArray = poYArrayIn;

    if (!poXArray)
    {
        const auto aoCoordVariables = GetCoordinateVariables();
        if (aoCoordVariables.size() < 2)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "aoCoordVariables.size() < 2");
            return nullptr;
        }

        if (aoCoordVariables.size() != GetDimensionCount() + 1)
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "aoCoordVariables.size() != GetDimensionCount() + 1");
            return nullptr;
        }

        // Default choice for X and Y arrays
        poYArray = aoCoordVariables[aoCoordVariables.size() - 2];
        poXArray = aoCoordVariables[aoCoordVariables.size() - 1];

        // Detect X and Y array from coordinate variables
        for (const auto &poVar : aoCoordVariables)
        {
            const auto &osVarName = poVar->GetName();
#ifdef disabled
            if (poVar->GetDimensionCount() != 1)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Coordinate variable %s is not 1-dimensional",
                         osVarName.c_str());
                return nullptr;
            }
            const auto &osVarDimName = poVar->GetDimensions()[0]->GetFullName();
            bool bFound = false;
            for (const auto &poDim : GetDimensions())
            {
                if (osVarDimName == poDim->GetFullName())
                {
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Dimension %s of coordinate variable %s is not a "
                         "dimension of this array",
                         osVarDimName.c_str(), osVarName.c_str());
                return nullptr;
            }
#endif
            if (osVarName == "x" || osVarName == "lon")
            {
                poXArray = poVar;
            }
            else if (osVarName == "y" || osVarName == "lat")
            {
                poYArray = poVar;
            }
        }
    }

    if (poYArray->GetDimensionCount() != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "aoCoordVariables[aoCoordVariables.size() - "
                 "2]->GetDimensionCount() != 1");
        return nullptr;
    }
    if (poYArray->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "poYArray->GetDataType().GetClass() != GEDTC_NUMERIC");
        return nullptr;
    }

    if (poXArray->GetDimensionCount() != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "aoCoordVariables[aoCoordVariables.size() - "
                 "1]->GetDimensionCount() != 1");
        return nullptr;
    }
    if (poXArray->GetDataType().GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "poXArray->GetDataType().GetClass() != GEDTC_NUMERIC");
        return nullptr;
    }

    if (poYArray->GetDimensions()[0]->GetFullName() !=
        poXArray->GetDimensions()[0]->GetFullName())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "poYArray->GetDimensions()[0]->GetFullName() != "
                 "poXArray->GetDimensions()[0]->GetFullName()");
        return nullptr;
    }

    if (poXArray->GetDimensions()[0]->GetFullName() !=
        GetDimensions().back()->GetFullName())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "poYArray->GetDimensions()[0]->GetFullName() != "
                 "GetDimensions().back()->GetFullName()");
        return nullptr;
    }

    if (poXArray->GetTotalElementsCount() <= 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "poXArray->GetTotalElementsCount() <= 2");
        return nullptr;
    }

    if (poXArray->GetTotalElementsCount() >
        std::numeric_limits<size_t>::max() / 2)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "poXArray->GetTotalElementsCount() > "
                 "std::numeric_limits<size_t>::max() / 2");
        return nullptr;
    }

    if (poXArray->GetTotalElementsCount() > 10 * 1024 * 1024 &&
        !CPLTestBool(CSLFetchNameValueDef(
            papszOptions, "ACCEPT_BIG_SPATIAL_INDEXING_VARIABLE", "NO")))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The spatial indexing variable has " CPL_FRMT_GIB " elements. "
                 "Set the ACCEPT_BIG_SPATIAL_INDEXING_VARIABLE=YES option of "
                 "GetGridded() to mean you want to continue and are aware of "
                 "big RAM and CPU time requirements",
                 static_cast<GIntBig>(poXArray->GetTotalElementsCount()));
        return nullptr;
    }

    std::vector<double> adfXVals;
    std::vector<double> adfYVals;
    try
    {
        adfXVals.resize(static_cast<size_t>(poXArray->GetTotalElementsCount()));
        adfYVals.resize(static_cast<size_t>(poXArray->GetTotalElementsCount()));
    }
    catch (const std::bad_alloc &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return nullptr;
    }

    // Ingest X and Y arrays
    const GUInt64 arrayStartIdx[] = {0};
    const size_t count[] = {adfXVals.size()};
    const GInt64 arrayStep[] = {1};
    const GPtrDiff_t bufferStride[] = {1};
    if (!poXArray->Read(arrayStartIdx, count, arrayStep, bufferStride,
                        GDALExtendedDataType::Create(GDT_Float64),
                        adfXVals.data()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "poXArray->Read() failed");
        return nullptr;
    }
    if (!poYArray->Read(arrayStartIdx, count, arrayStep, bufferStride,
                        GDALExtendedDataType::Create(GDT_Float64),
                        adfYVals.data()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "poYArray->Read() failed");
        return nullptr;
    }

    const char *pszExt = "fgb";
    GDALDriver *poDrv = GetGDALDriverManager()->GetDriverByName("FlatGeoBuf");
    if (!poDrv)
    {
        pszExt = "gpkg";
        poDrv = GetGDALDriverManager()->GetDriverByName("GPKG");
        if (!poDrv)
        {
            pszExt = "mem";
            poDrv = GetGDALDriverManager()->GetDriverByName("Memory");
            if (!poDrv)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot get driver FlatGeoBuf, GPKG or Memory");
                return nullptr;
            }
        }
    }

    // Create a in-memory vector layer with (X,Y) points
    CPLString osTmpFilename;
    osTmpFilename.Printf("/vsimem/GDALMDArray::GetGridded_%p_%p.%s", this,
                         pOptions, pszExt);
    auto poDS = std::unique_ptr<GDALDataset>(
        poDrv->Create(osTmpFilename.c_str(), 0, 0, 0, GDT_Unknown, nullptr));
    if (!poDS)
        return nullptr;
    auto poLyr = poDS->CreateLayer("layer", nullptr, wkbPoint);
    if (!poLyr)
        return nullptr;
    OGRFieldDefn oFieldDefn("IDX", OFTInteger64);
    poLyr->CreateField(&oFieldDefn);
    if (poLyr->StartTransaction() != OGRERR_NONE)
        return nullptr;
    OGRFeature oFeat(poLyr->GetLayerDefn());
    for (size_t i = 0; i < adfXVals.size(); ++i)
    {
        auto poPoint = new OGRPoint(adfXVals[i], adfYVals[i]);
        oFeat.SetFID(OGRNullFID);
        oFeat.SetGeometryDirectly(poPoint);
        oFeat.SetField(0, static_cast<GIntBig>(i));
        if (poLyr->CreateFeature(&oFeat) != OGRERR_NONE)
            return nullptr;
    }
    if (poLyr->CommitTransaction() != OGRERR_NONE)
        return nullptr;
    OGREnvelope sEnvelope;
    CPL_IGNORE_RET_VAL(poLyr->GetExtent(&sEnvelope));
    if (!EQUAL(pszExt, "mem"))
    {
        if (poDS->Close() != OGRERR_NONE)
            return nullptr;
        poDS.reset(GDALDataset::Open(osTmpFilename.c_str(), GDAL_OF_VECTOR));
        if (!poDS)
            return nullptr;
        poDS->MarkSuppressOnClose();
    }

    /* Set of constraints:
    nX * nY = nCount
    nX * res = MaxX - MinX
    nY * res = MaxY - MinY
    */

    double dfRes;
    const char *pszRes = CSLFetchNameValue(papszOptions, "RESOLUTION");
    if (pszRes)
    {
        dfRes = CPLAtofM(pszRes);
    }
    else
    {
        const double dfTotalArea = (sEnvelope.MaxY - sEnvelope.MinY) *
                                   (sEnvelope.MaxX - sEnvelope.MinX);
        dfRes = sqrt(dfTotalArea / static_cast<double>(adfXVals.size()));
        // CPLDebug("GDAL", "dfRes = %f", dfRes);

        // Take 10 "random" points in the set, and find the minimum distance from
        // each to the closest one. And take the geometric average of those minimum
        // distances as the resolution.
        const size_t nNumSamplePoints = std::min<size_t>(10, adfXVals.size());

        poLyr = poDS->GetLayer(0);
        double dfSumDist2Min = 0;
        int nCountDistMin = 0;
        for (size_t i = 0; i < nNumSamplePoints; ++i)
        {
            const auto nIdx = i * adfXVals.size() / nNumSamplePoints;
            poLyr->SetSpatialFilterRect(
                adfXVals[nIdx] - 2 * dfRes, adfYVals[nIdx] - 2 * dfRes,
                adfXVals[nIdx] + 2 * dfRes, adfYVals[nIdx] + 2 * dfRes);
            double dfDist2Min = std::numeric_limits<double>::max();
            for (auto &&poFeat : poLyr)
            {
                const auto poGeom = poFeat->GetGeometryRef();
                CPLAssert(poGeom);
                CPLAssert(poGeom->getGeometryType() == wkbPoint);
                double dfDX = poGeom->toPoint()->getX() - adfXVals[nIdx];
                double dfDY = poGeom->toPoint()->getY() - adfYVals[nIdx];
                double dfDist2 = dfDX * dfDX + dfDY * dfDY;
                if (dfDist2 > 0 && dfDist2 < dfDist2Min)
                    dfDist2Min = dfDist2;
            }
            if (dfDist2Min < std::numeric_limits<double>::max())
            {
                dfSumDist2Min += dfDist2Min;
                nCountDistMin++;
            }
        }
        poLyr->SetSpatialFilter(nullptr);
        if (nCountDistMin > 0)
        {
            const double dfNewRes = sqrt(dfSumDist2Min / nCountDistMin);
            // CPLDebug("GDAL", "dfRes = %f, dfNewRes = %f", dfRes, dfNewRes);
            dfRes = dfNewRes;
        }
    }

    if (!(dfRes > 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid RESOLUTION");
        return nullptr;
    }

    constexpr double EPS = 1e-8;
    const double dfXSize =
        1 + std::floor((sEnvelope.MaxX - sEnvelope.MinX) / dfRes + EPS);
    if (dfXSize > std::numeric_limits<int>::max())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow with dfXSize");
        return nullptr;
    }
    const int nXSize = std::max(2, static_cast<int>(dfXSize));

    const double dfYSize =
        1 + std::floor((sEnvelope.MaxY - sEnvelope.MinY) / dfRes + EPS);
    if (dfYSize > std::numeric_limits<int>::max())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Integer overflow with dfYSize");
        return nullptr;
    }
    const int nYSize = std::max(2, static_cast<int>(dfYSize));

    const double dfResX = (sEnvelope.MaxX - sEnvelope.MinX) / (nXSize - 1);
    const double dfResY = (sEnvelope.MaxY - sEnvelope.MinY) / (nYSize - 1);
    // CPLDebug("GDAL", "nXSize = %d, nYSize = %d", nXSize, nYSize);

    std::vector<std::shared_ptr<GDALDimension>> apoNewDims;
    const auto &apoSelfDims = GetDimensions();
    for (size_t i = 0; i < GetDimensionCount() - 1; ++i)
        apoNewDims.emplace_back(apoSelfDims[i]);

    auto poDimY = std::make_shared<GDALDimensionWeakIndexingVar>(
        std::string(), "dimY", GDAL_DIM_TYPE_HORIZONTAL_Y, "NORTH", nYSize);
    auto varY = GDALMDArrayRegularlySpaced::Create(
        std::string(), poDimY->GetName(), poDimY, sEnvelope.MinY, dfResY, 0);
    poDimY->SetIndexingVariable(varY);

    auto poDimX = std::make_shared<GDALDimensionWeakIndexingVar>(
        std::string(), "dimX", GDAL_DIM_TYPE_HORIZONTAL_X, "EAST", nXSize);
    auto varX = GDALMDArrayRegularlySpaced::Create(
        std::string(), poDimX->GetName(), poDimX, sEnvelope.MinX, dfResX, 0);
    poDimX->SetIndexingVariable(varX);

    apoNewDims.emplace_back(poDimY);
    apoNewDims.emplace_back(poDimX);

    const CPLStringList aosTokens(
        CSLTokenizeString2(osGridOptions.c_str(), ":", FALSE));

    // Extract nodata value from gridding options
    const char *pszNoDataValue = aosTokens.FetchNameValue("nodata");
    double dfNoDataValue = 0;
    if (pszNoDataValue != nullptr)
    {
        dfNoDataValue = CPLAtofM(pszNoDataValue);
    }

    // Extract radius from gridding options
    double dfRadius = 5 * std::max(dfResX, dfResY);
    const char *pszRadius = aosTokens.FetchNameValue("radius");
    if (pszRadius)
    {
        dfRadius = CPLAtofM(pszRadius);
    }
    else
    {
        const char *pszRadius1 = aosTokens.FetchNameValue("radius1");
        if (pszRadius1)
        {
            dfRadius = CPLAtofM(pszRadius1);
            const char *pszRadius2 = aosTokens.FetchNameValue("radius2");
            if (pszRadius2)
            {
                dfRadius = std::max(dfRadius, CPLAtofM(pszRadius2));
            }
        }
    }

    return GDALMDArrayGridded::Create(
        self, apoNewDims, varX, varY, std::move(poDS), eAlg,
        std::move(poGridOptions), dfNoDataValue, sEnvelope.MinX, dfResX,
        sEnvelope.MinY, dfResY, dfRadius);
}
