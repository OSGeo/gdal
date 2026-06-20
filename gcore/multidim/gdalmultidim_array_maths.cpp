/******************************************************************************
 *
 * Name:     gdalmultidim_array_maths.cpp
 * Project:  GDAL Core
 * Purpose:  Mathematic operations on GDALMDArray
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

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>

/************************************************************************/
/*                    GDALMDArray::HasSameShapeAs()                     */
/************************************************************************/

/** Returns true if both arrays have the same shape.
 *
 * That is to say the same number of dimensions and the size of corresponding
 * dimensions is the same.
 *
 * @since 3.14
 */
bool GDALMDArray::HasSameShapeAs(const GDALMDArray &other) const
{
    bool bRet = (GetDimensionCount() == other.GetDimensionCount());
    if (bRet)
    {
        const auto &apoThisDims = GetDimensions();
        const auto &apoOtherDims = other.GetDimensions();
        for (size_t i = 0; bRet && i < apoThisDims.size(); ++i)
        {
            bRet = (apoThisDims[i]->GetSize() == apoOtherDims[i]->GetSize());
        }
    }
    return bRet;
}

/************************************************************************/
/*                       GDALMathOperationMDArray                       */
/************************************************************************/

//! @cond Doxygen_Suppress

class GDALMathOperationMDArray final : public GDALPamMDArray
{
  public:
    enum class Operation
    {
        OP_ADD,
        OP_SUBTRACT,
        OP_MULTIPLY,
        OP_DIVIDE,
    };

    static const char *OperationToString(Operation op)
    {
        switch (op)
        {
            case Operation::OP_ADD:
                break;
            case Operation::OP_SUBTRACT:
                return "-";
            case Operation::OP_MULTIPLY:
                return "*";
            case Operation::OP_DIVIDE:
                return "/";
        }
        return "+";
    }

    static std::shared_ptr<GDALMathOperationMDArray>
    Create(const std::shared_ptr<GDALMDArray> &arrayLeft,
           const std::shared_ptr<GDALMDArray> &arrayRight, Operation op);

  protected:
    static std::string GetName(const std::shared_ptr<GDALMDArray> &arrayLeft,
                               const std::shared_ptr<GDALMDArray> &arrayRight,
                               Operation op)
    {
        return std::string(arrayLeft->GetFullName())
            .append(" ")
            .append(OperationToString(op))
            .append(" ")
            .append(arrayRight->GetFullName());
    }

    GDALMathOperationMDArray(const std::shared_ptr<GDALMDArray> &arrayLeft,
                             const std::shared_ptr<GDALMDArray> &arrayRight,
                             Operation op)
        : GDALAbstractMDArray(std::string(),
                              GetName(arrayLeft, arrayRight, op)),
          GDALPamMDArray(std::string(), GetName(arrayLeft, arrayRight, op),
                         // Arbitrary linking to arrayLeft for PAM purposes
                         GDALPamMultiDim::GetPAM(arrayLeft),
                         arrayLeft->GetContext()),
          m_arrayLeft(arrayLeft), m_arrayRight(arrayRight), m_op(op),
          m_dt(GDALExtendedDataType::Create(GDT_Float64)),
          m_dfNoDataLeft(
              m_arrayLeft->GetNoDataValueAsDouble(&m_bHasNoDataLeft)),
          m_dfNoDataRight(
              m_arrayRight->GetNoDataValueAsDouble(&m_bHasNoDataRight))
    {
        if (m_bHasNoDataLeft || m_bHasNoDataRight)
        {
            if (m_bHasNoDataLeft && m_bHasNoDataRight)
            {
                if (m_dfNoDataLeft == m_dfNoDataRight)
                    m_dfNoData = m_dfNoDataLeft;
            }
            else if (m_bHasNoDataLeft)
                m_dfNoData = m_dfNoDataLeft;
            else
                m_dfNoData = m_dfNoDataRight;
            m_abyRawNoDataValue.resize(sizeof(double));
            memcpy(m_abyRawNoDataValue.data(), &m_dfNoData, sizeof(m_dfNoData));
        }

        const auto &leftUnit = m_arrayLeft->GetUnit();
        const auto &rightUnit = m_arrayRight->GetUnit();
        switch (m_op)
        {
            case Operation::OP_ADD:
            case Operation::OP_SUBTRACT:
            {
                if (leftUnit == rightUnit)
                    m_osUnit = leftUnit;
                break;
            }
            case Operation::OP_MULTIPLY:
            case Operation::OP_DIVIDE:
            {
                if (!leftUnit.empty() && !rightUnit.empty())
                {
                    m_osUnit = leftUnit;
                    m_osUnit += ' ';
                    m_osUnit += OperationToString(m_op);
                    m_osUnit += ' ';
                    m_osUnit += rightUnit;
                }
                break;
            }
        }
    }

    bool IsWritable() const override
    {
        return false;
    }

    const std::string &GetFilename() const override
    {
        const auto &filename1 = m_arrayLeft->GetFilename();
        const auto &filename2 = m_arrayRight->GetFilename();
        if (filename1 == filename2)
            return filename1;
        static std::string emptyString;
        return emptyString;
    }

    const std::vector<std::shared_ptr<GDALDimension>> &
    GetDimensions() const override
    {
        return m_arrayLeft->GetDimensions();
    }

    std::vector<std::shared_ptr<GDALMDArray>>
    GetCoordinateVariables() const override
    {
        auto left = m_arrayLeft->GetCoordinateVariables();
        auto right = m_arrayRight->GetCoordinateVariables();
        if (left == right)
            return left;
        return GDALMDArray::GetCoordinateVariables();
    }

    const GDALExtendedDataType &GetDataType() const override
    {
        return m_dt;
    }

    const void *GetRawNoDataValue() const override
    {
        return m_abyRawNoDataValue.empty() ? nullptr
                                           : m_abyRawNoDataValue.data();
    }

    const std::string &GetUnit() const override
    {
        return m_osUnit;
    }

    std::shared_ptr<OGRSpatialReference> GetSpatialRef() const override
    {
        auto leftSRS = m_arrayLeft->GetSpatialRef();
        auto rightSRS = m_arrayRight->GetSpatialRef();
        if (!leftSRS || !rightSRS || !leftSRS->IsSame(rightSRS.get()))
            return nullptr;
        return leftSRS;
    }

    double GetOffset(bool *pbHasOffset,
                     GDALDataType *peStorageType) const override
    {
        bool bHasLeftOffset = false;
        GDALDataType leftStorageType = GDT_Unknown;
        const double dfLeftOffset =
            m_arrayLeft->GetOffset(&bHasLeftOffset, &leftStorageType);

        bool bHasRightOffset = false;
        GDALDataType rightStorageType = GDT_Unknown;
        const double dfRightOffset =
            m_arrayRight->GetOffset(&bHasRightOffset, &rightStorageType);

        bool bHasLeftScale = false;
        const double dfLeftScale = m_arrayLeft->GetScale(&bHasLeftScale);

        bool bHasRightScale = false;
        const double dfRightScale = m_arrayRight->GetScale(&bHasRightScale);

        bool bRet = false;
        double dfRes = 0.0;

        switch (m_op)
        {
            case Operation::OP_ADD:
            {
                bRet = bHasLeftOffset && bHasRightOffset &&
                       (bHasLeftScale == bHasRightScale &&
                        (!bHasLeftScale || dfLeftScale == dfRightScale));
                if (bRet)
                    dfRes = dfLeftOffset + dfRightOffset;
                break;
            }

            case Operation::OP_SUBTRACT:
            {
                bRet = bHasLeftOffset && bHasRightOffset &&
                       (bHasLeftScale == bHasRightScale &&
                        (!bHasLeftScale || dfLeftScale == dfRightScale));
                if (bRet)
                    dfRes = dfLeftOffset - dfRightOffset;
                break;
            }

            case Operation::OP_MULTIPLY:
            case Operation::OP_DIVIDE:
                break;
        }

        if (pbHasOffset)
            *pbHasOffset = bRet;
        if (bRet && peStorageType)
        {
            *peStorageType =
                GDALDataTypeUnion(leftStorageType, rightStorageType);
        }

        return dfRes;
    }

    double GetScale(bool *pbHasScale,
                    GDALDataType *peStorageType) const override
    {
        bool bHasLeftOffset = false;
        const double dfLeftOffset = m_arrayLeft->GetOffset(&bHasLeftOffset);

        bool bHasRightOffset = false;
        const double dfRightOffset = m_arrayRight->GetOffset(&bHasRightOffset);

        bool bHasLeftScale = false;
        GDALDataType leftStorageType = GDT_Unknown;
        const double dfLeftScale =
            m_arrayLeft->GetScale(&bHasLeftScale, &leftStorageType);

        bool bHasRightScale = false;
        GDALDataType rightStorageType = GDT_Unknown;
        const double dfRightScale =
            m_arrayRight->GetScale(&bHasRightScale, &rightStorageType);

        bool bRet = false;
        double dfRes = 1.0;

        const bool bZeroOffset =
            (bHasLeftOffset == bHasRightOffset &&
             (!bHasLeftOffset ||
              (dfLeftOffset == dfRightOffset && dfLeftOffset == 0)));

        switch (m_op)
        {
            case Operation::OP_ADD:
            case Operation::OP_SUBTRACT:
            {
                bRet = bZeroOffset &&
                       (bHasLeftScale == bHasRightScale &&
                        (!bHasLeftScale || dfLeftScale == dfRightScale));
                if (bRet)
                    dfRes = dfLeftScale;
                break;
            }

            case Operation::OP_MULTIPLY:
            {
                bRet = bZeroOffset && bHasLeftScale && bHasRightScale;
                if (bRet)
                    dfRes = dfLeftScale * dfRightScale;
                break;
            }

            case Operation::OP_DIVIDE:
            {
                bRet = bZeroOffset && bHasLeftScale && bHasRightScale;
                if (bRet)
                    dfRes = dfLeftScale / dfRightScale;
                break;
            }
        }

        if (pbHasScale)
            *pbHasScale = bRet;
        if (bRet && peStorageType)
        {
            *peStorageType =
                GDALDataTypeUnion(leftStorageType, rightStorageType);
        }

        return dfRes;
    }

    std::vector<GUInt64> GetBlockSize() const override
    {
        const auto leftBlockSize = m_arrayLeft->GetBlockSize();
        const auto rightBlockSize = m_arrayRight->GetBlockSize();
        if (leftBlockSize != rightBlockSize)
            return GDALMDArray::GetBlockSize();
        return leftBlockSize;
    }

    bool IRead(const GUInt64 *arrayStartIdx, const size_t *count,
               const GInt64 *arrayStep, const GPtrDiff_t *bufferStride,
               const GDALExtendedDataType &bufferDataType,
               void *pDstBuffer) const override;

  private:
    const std::shared_ptr<GDALMDArray> m_arrayLeft;
    const std::shared_ptr<GDALMDArray> m_arrayRight;
    const Operation m_op;
    const GDALExtendedDataType m_dt;
    bool m_bHasNoDataLeft = false;
    bool m_bHasNoDataRight = false;
    const double m_dfNoDataLeft;
    const double m_dfNoDataRight;
    double m_dfNoData = std::numeric_limits<double>::quiet_NaN();
    std::vector<GByte> m_abyRawNoDataValue{};
    std::string m_osUnit{};
    mutable std::vector<double> m_leftValues{};
    mutable std::vector<double> m_rightValues{};

    inline bool IsInvalid(double dfLeft) const
    {
        return std::isnan(dfLeft) ||
               (m_bHasNoDataLeft && m_dfNoDataLeft == dfLeft);
    }

    inline bool IsInvalidTuple(double dfLeft, double dfRight) const
    {
        return std::isnan(dfLeft) ||
               (m_bHasNoDataLeft && m_dfNoDataLeft == dfLeft) ||
               std::isnan(dfRight) ||
               (m_bHasNoDataRight && m_dfNoDataRight == dfRight);
    }

    template <class Op>
    void PerformOperation(double *leftValues, const double *rightValues,
                          size_t nElts, bool bIntegerAndAllValid) const;
};

/************************************************************************/
/*                  GDALMathOperationMDArray::Create()                  */
/************************************************************************/

/* static */ std::shared_ptr<GDALMathOperationMDArray>
GDALMathOperationMDArray::Create(const std::shared_ptr<GDALMDArray> &arrayLeft,
                                 const std::shared_ptr<GDALMDArray> &arrayRight,
                                 Operation op)
{
    if (!arrayLeft)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "arrayLeft is null");
        return nullptr;
    }

    if (!arrayRight)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "arrayRight is null");
        return nullptr;
    }

    if (!arrayLeft->HasSameShapeAs(*arrayRight))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 ("Arrays " + arrayLeft->GetFullName() + " and " +
                  arrayRight->GetFullName() + " do not have the same shape")
                     .c_str());
        return nullptr;
    }

    for (const auto &array : {arrayLeft, arrayRight})
    {
        if (array->GetDataType().GetClass() != GEDTC_NUMERIC)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined, "%s",
                ("Array " + array->GetFullName() + " is not numeric").c_str());
            return nullptr;
        }
    }

    auto newAr(std::shared_ptr<GDALMathOperationMDArray>(
        new GDALMathOperationMDArray(arrayLeft, arrayRight, op)));
    newAr->SetSelf(newAr);
    return newAr;
}

/************************************************************************/
/*             GDALMathOperationMDArray::PerformOperation()             */
/************************************************************************/

template <class Op>
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("tree-vectorize")))
#endif
void GDALMathOperationMDArray::PerformOperation(double *leftValues,
                                                const double *rightValues,
                                                size_t nElts,
                                                bool bIntegerAndAllValid) const
{
    if (bIntegerAndAllValid)
    {
        if (!rightValues)
        {
            for (size_t i = 0; i < nElts; ++i)
            {
                leftValues[i] = Op()(leftValues[i], leftValues[i]);
            }
        }
        else
        {
            for (size_t i = 0; i < nElts; ++i)
            {
                leftValues[i] = Op()(leftValues[i], rightValues[i]);
            }
        }
    }
    else
    {
        if (!rightValues)
        {
            if ((!m_bHasNoDataLeft || std::isnan(m_dfNoDataLeft)) &&
                std::isnan(m_dfNoData))
            {
                for (size_t i = 0; i < nElts; ++i)
                {
                    leftValues[i] = Op()(leftValues[i], leftValues[i]);
                }
            }
            else
            {
                for (size_t i = 0; i < nElts; ++i)
                {
                    if (IsInvalid(leftValues[i]))
                        leftValues[i] = m_dfNoData;
                    else
                        leftValues[i] = Op()(leftValues[i], leftValues[i]);
                }
            }
        }
        else
        {
            if ((!m_bHasNoDataLeft || std::isnan(m_dfNoDataLeft)) &&
                (!m_bHasNoDataRight || std::isnan(m_dfNoDataRight)) &&
                std::isnan(m_dfNoData))
            {
                for (size_t i = 0; i < nElts; ++i)
                {
                    leftValues[i] = Op()(leftValues[i], rightValues[i]);
                }
            }
            else
            {
                for (size_t i = 0; i < nElts; ++i)
                {
                    if (IsInvalidTuple(leftValues[i], rightValues[i]))
                        leftValues[i] = m_dfNoData;
                    else
                        leftValues[i] = Op()(leftValues[i], rightValues[i]);
                }
            }
        }
    }
}

/************************************************************************/
/*                  GDALMathOperationMDArray::IRead()                   */
/************************************************************************/

bool GDALMathOperationMDArray::IRead(const GUInt64 *arrayStartIdx,
                                     const size_t *count,
                                     const GInt64 *arrayStep,
                                     const GPtrDiff_t *bufferStride,
                                     const GDALExtendedDataType &bufferDataType,
                                     void *pDstBuffer) const
{
    if (bufferDataType.GetClass() != GEDTC_NUMERIC)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GDALMathOperationMDArray::IRead(): not supported with "
                 "non-numeric buffer data type");
        return false;
    }

    const auto &apoDims = GetDimensions();
    const size_t nDims = apoDims.size();

    const bool bIntegerAndAllValid =
        m_abyRawNoDataValue.empty() &&
        GDALDataTypeIsInteger(
            m_arrayLeft->GetDataType().GetNumericDataType()) &&
        GDALDataTypeIsInteger(m_arrayRight->GetDataType().GetNumericDataType());

    if (bIntegerAndAllValid && m_op == Operation::OP_SUBTRACT &&
        m_arrayLeft == m_arrayRight)
    {
        CopyContiguousBufferToBuffer(nDims, count, nullptr,
                                     GDALExtendedDataType::Create(GDT_Unknown),
                                     pDstBuffer, bufferDataType, bufferStride);
        return true;
    }

    // Check if the output buffer is in contiguous row major order
    // If this is true and its type is also the type of the array, we can
    // directly read the left array into the output buffer and update it in
    // place.
    bool bRowMajorBufferStride = true;
    GPtrDiff_t nExpectedStrideValue = 1;
    for (size_t i = nDims; i > 0; /* decremented in loop */)
    {
        --i;
        if (bufferStride[i] != nExpectedStrideValue)
        {
            bRowMajorBufferStride = false;
            break;
        }
        nExpectedStrideValue = static_cast<GPtrDiff_t>(nExpectedStrideValue *
                                                       apoDims[i]->GetSize());
    }

    size_t nElts = 1;
    bool bFullArrayRequested = true;
    for (size_t i = 0; i < nDims; ++i)
    {
        nElts *= count[i];
        if (bFullArrayRequested)
            bFullArrayRequested = (count[i] == apoDims[i]->GetSize());
    }

    const bool bLeftValuesAllocNeeded =
        !bRowMajorBufferStride || bufferDataType != m_dt;

    // We can skip both I/O and memory allocations if operating on the same
    // array.
    const bool bRightValuesAllocNeeded = m_arrayLeft != m_arrayRight;

    try
    {
        if (bLeftValuesAllocNeeded)
        {
            if (nElts > m_leftValues.size())
                m_leftValues.resize(nElts);
        }
        if (bRightValuesAllocNeeded)
        {
            if (nElts > m_rightValues.size())
                m_rightValues.resize(nElts);
        }
    }
    catch (const std::exception &)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "GDALMathOperationMDArray::IRead(): out of memory");
        return false;
    }

    double *leftValues = bLeftValuesAllocNeeded
                             ? m_leftValues.data()
                             : static_cast<double *>(pDstBuffer);

    const bool ret = m_arrayLeft->Read(arrayStartIdx, count, arrayStep, nullptr,
                                       m_dt, leftValues) &&
                     (!bRightValuesAllocNeeded ||
                      m_arrayRight->Read(arrayStartIdx, count, arrayStep,
                                         nullptr, m_dt, m_rightValues.data()));

    const double *rightValues =
        bRightValuesAllocNeeded ? m_rightValues.data() : nullptr;

    if (ret)
    {
        switch (m_op)
        {
            case Operation::OP_ADD:
            {
                PerformOperation<std::plus<double>>(leftValues, rightValues,
                                                    nElts, bIntegerAndAllValid);
                break;
            }
            case Operation::OP_SUBTRACT:
            {
                PerformOperation<std::minus<double>>(
                    leftValues, rightValues, nElts, bIntegerAndAllValid);
                break;
            }
            case Operation::OP_MULTIPLY:
            {
                PerformOperation<std::multiplies<double>>(
                    leftValues, rightValues, nElts, bIntegerAndAllValid);
                break;
            }
            case Operation::OP_DIVIDE:
            {
                PerformOperation<std::divides<double>>(
                    leftValues, rightValues, nElts, bIntegerAndAllValid);
                break;
            }
        }

        if (bLeftValuesAllocNeeded)
        {
            CopyContiguousBufferToBuffer(nDims, count, leftValues, m_dt,
                                         pDstBuffer, bufferDataType,
                                         bufferStride);
        }
    }

    if (bFullArrayRequested)
    {
        m_leftValues.clear();
        m_rightValues.clear();
    }

    return ret;
}

//! @endcond

/************************************************************************/
/*                             operator+()                              */
/************************************************************************/

/** Add this array with another one of the same shape.
 *
 * The resulting array is lazy evaluated.
 *
 * The resulting array type is Float64.
 *
 * The operation is nodata-aware.
 *
 * This is the same as C function GDALMDArrayBinaryOperation().
 *
 * @since 3.14
 * @return a new array, or nullptr in case of error
 */
std::shared_ptr<GDALMDArray>
GDALMDArray::operator+(const std::shared_ptr<GDALMDArray> &other) const
{
    return GDALMathOperationMDArray::Create(
        GetSelf(), other, GDALMathOperationMDArray::Operation::OP_ADD);
}

/************************************************************************/
/*                             operator-()                              */
/************************************************************************/

/** Subtract this array with another one of the same shape.
 *
 * The resulting array is lazy evaluated.
 *
 * The resulting array type is Float64.
 *
 * The operation is nodata-aware.
 *
 * This is the same as C function GDALMDArrayBinaryOperation().
 *
 * @since 3.14
 * @return a new array, or nullptr in case of error
 */
std::shared_ptr<GDALMDArray>
GDALMDArray::operator-(const std::shared_ptr<GDALMDArray> &other) const
{
    return GDALMathOperationMDArray::Create(
        GetSelf(), other, GDALMathOperationMDArray::Operation::OP_SUBTRACT);
}

/************************************************************************/
/*                             operator*()                              */
/************************************************************************/

/** Multiply this array with another one of the same shape.
 *
 * The resulting array is lazy evaluated.
 *
 * The resulting array type is Float64.
 *
 * The operation is nodata-aware.
 *
 * This is the same as C function GDALMDArrayBinaryOperation().
 *
 * @since 3.14
 * @return a new array, or nullptr in case of error
 */
std::shared_ptr<GDALMDArray>
GDALMDArray::operator*(const std::shared_ptr<GDALMDArray> &other) const
{
    return GDALMathOperationMDArray::Create(
        GetSelf(), other, GDALMathOperationMDArray::Operation::OP_MULTIPLY);
}

/************************************************************************/
/*                             operator/()                              */
/************************************************************************/

/** Divide this array by another one of the same shape.
 *
 * The resulting array is lazy evaluated.
 *
 * The resulting array type is Float64.
 *
 * The operation is nodata-aware.
 *
 * This is the same as C function GDALMDArrayBinaryOperation().
 *
 * @since 3.14
 * @return a new array, or nullptr in case of error
 */
std::shared_ptr<GDALMDArray>
GDALMDArray::operator/(const std::shared_ptr<GDALMDArray> &other) const
{
    return GDALMathOperationMDArray::Create(
        GetSelf(), other, GDALMathOperationMDArray::Operation::OP_DIVIDE);
}
