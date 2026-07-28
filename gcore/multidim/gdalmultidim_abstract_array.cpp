/******************************************************************************
 *
 * Name:     gdalmultidim_abstract_array.cpp
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALAbstractMDArray class
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_float.h"
#include "cpl_safemaths.hpp"
#include "gdal_multidim.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <map>

/************************************************************************/
/*                        ~GDALAbstractMDArray()                        */
/************************************************************************/

GDALAbstractMDArray::~GDALAbstractMDArray() = default;

/************************************************************************/
/*                        GDALAbstractMDArray()                         */
/************************************************************************/

//! @cond Doxygen_Suppress
GDALAbstractMDArray::GDALAbstractMDArray(const std::string &osParentName,
                                         const std::string &osName)
    : m_osName(osName),
      m_osFullName(
          !osParentName.empty()
              ? ((osParentName == "/" ? "/" : osParentName + "/") + osName)
              : osName)
{
}

//! @endcond

/************************************************************************/
/*                           GetDimensions()                            */
/************************************************************************/

/** \fn GDALAbstractMDArray::GetDimensions() const
 * \brief Return the dimensions of an attribute/array.
 *
 * This is the same as the C functions GDALMDArrayGetDimensions() and
 * similar to GDALAttributeGetDimensionsSize().
 */

/************************************************************************/
/*                            GetDataType()                             */
/************************************************************************/

/** \fn GDALAbstractMDArray::GetDataType() const
 * \brief Return the data type of an attribute/array.
 *
 * This is the same as the C functions GDALMDArrayGetDataType() and
 * GDALAttributeGetDataType()
 */

/************************************************************************/
/*                         GetDimensionCount()                          */
/************************************************************************/

/** Return the number of dimensions.
 *
 * Default implementation is GetDimensions().size(), and may be overridden by
 * drivers if they have a faster / less expensive implementations.
 *
 * This is the same as the C function GDALMDArrayGetDimensionCount() or
 * GDALAttributeGetDimensionCount().
 *
 */
size_t GDALAbstractMDArray::GetDimensionCount() const
{
    return GetDimensions().size();
}

/************************************************************************/
/*                               Rename()                               */
/************************************************************************/

/** Rename the attribute/array.
 *
 * This is not implemented by all drivers.
 *
 * Drivers known to implement it: MEM, netCDF, Zarr.
 *
 * This is the same as the C functions GDALMDArrayRename() or
 * GDALAttributeRename().
 *
 * @param osNewName New name.
 *
 * @return true in case of success
 * @since GDAL 3.8
 */
bool GDALAbstractMDArray::Rename(CPL_UNUSED const std::string &osNewName)
{
    CPLError(CE_Failure, CPLE_NotSupported, "Rename() not implemented");
    return false;
}

/************************************************************************/
/*                             CopyValue()                              */
/************************************************************************/

/** Convert a value from a source type to a destination type.
 *
 * If dstType is GEDTC_STRING, the written value will be a pointer to a char*,
 * that must be freed with CPLFree().
 */
bool GDALExtendedDataType::CopyValue(const void *pSrc,
                                     const GDALExtendedDataType &srcType,
                                     void *pDst,
                                     const GDALExtendedDataType &dstType)
{
    if (srcType.GetClass() == GEDTC_NUMERIC &&
        dstType.GetClass() == GEDTC_NUMERIC)
    {
        GDALCopyWords64(pSrc, srcType.GetNumericDataType(), 0, pDst,
                        dstType.GetNumericDataType(), 0, 1);
        return true;
    }
    if (srcType.GetClass() == GEDTC_STRING &&
        dstType.GetClass() == GEDTC_STRING)
    {
        const char *srcStrPtr;
        memcpy(&srcStrPtr, pSrc, sizeof(const char *));
        char *pszDup = srcStrPtr ? CPLStrdup(srcStrPtr) : nullptr;
        *reinterpret_cast<void **>(pDst) = pszDup;
        return true;
    }
    if (srcType.GetClass() == GEDTC_NUMERIC &&
        dstType.GetClass() == GEDTC_STRING)
    {
        const char *str = nullptr;
        switch (srcType.GetNumericDataType())
        {
            case GDT_Unknown:
                break;
            case GDT_UInt8:
                str = CPLSPrintf("%d", *static_cast<const GByte *>(pSrc));
                break;
            case GDT_Int8:
                str = CPLSPrintf("%d", *static_cast<const GInt8 *>(pSrc));
                break;
            case GDT_UInt16:
                str = CPLSPrintf("%d", *static_cast<const GUInt16 *>(pSrc));
                break;
            case GDT_Int16:
                str = CPLSPrintf("%d", *static_cast<const GInt16 *>(pSrc));
                break;
            case GDT_UInt32:
                str = CPLSPrintf("%u", *static_cast<const GUInt32 *>(pSrc));
                break;
            case GDT_Int32:
                str = CPLSPrintf("%d", *static_cast<const GInt32 *>(pSrc));
                break;
            case GDT_UInt64:
                str =
                    CPLSPrintf(CPL_FRMT_GUIB,
                               static_cast<GUIntBig>(
                                   *static_cast<const std::uint64_t *>(pSrc)));
                break;
            case GDT_Int64:
                str = CPLSPrintf(CPL_FRMT_GIB,
                                 static_cast<GIntBig>(
                                     *static_cast<const std::int64_t *>(pSrc)));
                break;
            case GDT_Float16:
                str = CPLSPrintf("%.5g",
                                 double(*static_cast<const GFloat16 *>(pSrc)));
                break;
            case GDT_Float32:
                str = CPLSPrintf(
                    "%.9g",
                    static_cast<double>(*static_cast<const float *>(pSrc)));
                break;
            case GDT_Float64:
                str = CPLSPrintf("%.17g", *static_cast<const double *>(pSrc));
                break;
            case GDT_CInt16:
            {
                const GInt16 *src = static_cast<const GInt16 *>(pSrc);
                str = CPLSPrintf("%d+%dj", src[0], src[1]);
                break;
            }
            case GDT_CInt32:
            {
                const GInt32 *src = static_cast<const GInt32 *>(pSrc);
                str = CPLSPrintf("%d+%dj", src[0], src[1]);
                break;
            }
            case GDT_CFloat16:
            {
                const GFloat16 *src = static_cast<const GFloat16 *>(pSrc);
                str = CPLSPrintf("%.5g+%.5gj", double(src[0]), double(src[1]));
                break;
            }
            case GDT_CFloat32:
            {
                const float *src = static_cast<const float *>(pSrc);
                str = CPLSPrintf("%.9g+%.9gj", double(src[0]), double(src[1]));
                break;
            }
            case GDT_CFloat64:
            {
                const double *src = static_cast<const double *>(pSrc);
                str = CPLSPrintf("%.17g+%.17gj", src[0], src[1]);
                break;
            }
            case GDT_TypeCount:
                CPLAssert(false);
                break;
        }
        char *pszDup = str ? CPLStrdup(str) : nullptr;
        *reinterpret_cast<void **>(pDst) = pszDup;
        return true;
    }
    if (srcType.GetClass() == GEDTC_STRING &&
        dstType.GetClass() == GEDTC_NUMERIC)
    {
        const char *srcStrPtr;
        memcpy(&srcStrPtr, pSrc, sizeof(const char *));
        if (dstType.GetNumericDataType() == GDT_Int64)
        {
            *(static_cast<int64_t *>(pDst)) =
                srcStrPtr == nullptr ? 0
                                     : static_cast<int64_t>(atoll(srcStrPtr));
        }
        else if (dstType.GetNumericDataType() == GDT_UInt64)
        {
            *(static_cast<uint64_t *>(pDst)) =
                srcStrPtr == nullptr
                    ? 0
                    : static_cast<uint64_t>(strtoull(srcStrPtr, nullptr, 10));
        }
        else
        {
            const double dfVal = srcStrPtr == nullptr ? 0 : CPLAtof(srcStrPtr);
            GDALCopyWords64(&dfVal, GDT_Float64, 0, pDst,
                            dstType.GetNumericDataType(), 0, 1);
        }
        return true;
    }
    if (srcType.GetClass() == GEDTC_COMPOUND &&
        dstType.GetClass() == GEDTC_COMPOUND)
    {
        const auto &srcComponents = srcType.GetComponents();
        const auto &dstComponents = dstType.GetComponents();
        const GByte *pabySrc = static_cast<const GByte *>(pSrc);
        GByte *pabyDst = static_cast<GByte *>(pDst);

        std::map<std::string, const std::unique_ptr<GDALEDTComponent> *>
            srcComponentMap;
        for (const auto &srcComp : srcComponents)
        {
            srcComponentMap[srcComp->GetName()] = &srcComp;
        }
        for (const auto &dstComp : dstComponents)
        {
            auto oIter = srcComponentMap.find(dstComp->GetName());
            if (oIter == srcComponentMap.end())
                return false;
            const auto &srcComp = *(oIter->second);
            if (!GDALExtendedDataType::CopyValue(
                    pabySrc + srcComp->GetOffset(), srcComp->GetType(),
                    pabyDst + dstComp->GetOffset(), dstComp->GetType()))
            {
                return false;
            }
        }
        return true;
    }

    return false;
}

/************************************************************************/
/*                        CheckReadWriteParams()                        */
/************************************************************************/
//! @cond Doxygen_Suppress
bool GDALAbstractMDArray::CheckReadWriteParams(
    const GUInt64 *arrayStartIdx, const size_t *count, const GInt64 *&arrayStep,
    const GPtrDiff_t *&bufferStride, const GDALExtendedDataType &bufferDataType,
    const void *buffer, const void *buffer_alloc_start,
    size_t buffer_alloc_size, std::vector<GInt64> &tmp_arrayStep,
    std::vector<GPtrDiff_t> &tmp_bufferStride) const
{
    const auto lamda_error = []()
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Not all elements pointed by buffer will fit in "
                 "[buffer_alloc_start, "
                 "buffer_alloc_start + buffer_alloc_size]");
    };

    const auto &dims = GetDimensions();
    if (dims.empty())
    {
        if (buffer_alloc_start)
        {
            const size_t elementSize = bufferDataType.GetSize();
            const GByte *paby_buffer = static_cast<const GByte *>(buffer);
            const GByte *paby_buffer_alloc_start =
                static_cast<const GByte *>(buffer_alloc_start);
            const GByte *paby_buffer_alloc_end =
                paby_buffer_alloc_start + buffer_alloc_size;

            if (paby_buffer < paby_buffer_alloc_start ||
                paby_buffer + elementSize > paby_buffer_alloc_end)
            {
                lamda_error();
                return false;
            }
        }
        return true;
    }

    if (arrayStep == nullptr)
    {
        tmp_arrayStep.resize(dims.size(), 1);
        arrayStep = tmp_arrayStep.data();
    }
    for (size_t i = 0; i < dims.size(); i++)
    {
        assert(count);
        if (count[i] == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "count[%u] = 0 is invalid",
                     static_cast<unsigned>(i));
            return false;
        }
    }
    bool bufferStride_all_positive = true;
    if (bufferStride == nullptr)
    {
        GPtrDiff_t stride = 1;
        assert(dims.empty() || count != nullptr);
        // To compute strides we must proceed from the fastest varying dimension
        // (the last one), and then reverse the result
        for (size_t i = dims.size(); i != 0;)
        {
            --i;
            tmp_bufferStride.push_back(stride);
            GUInt64 newStride = 0;
            bool bOK;
            try
            {
                newStride = (CPLSM(static_cast<uint64_t>(stride)) *
                             CPLSM(static_cast<uint64_t>(count[i])))
                                .v();
                bOK = static_cast<size_t>(newStride) == newStride &&
                      newStride < std::numeric_limits<size_t>::max() / 2;
            }
            catch (...)
            {
                bOK = false;
            }
            if (!bOK)
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Too big count values");
                return false;
            }
            stride = static_cast<GPtrDiff_t>(newStride);
        }
        std::reverse(tmp_bufferStride.begin(), tmp_bufferStride.end());
        bufferStride = tmp_bufferStride.data();
    }
    else
    {
        for (size_t i = 0; i < dims.size(); i++)
        {
            if (bufferStride[i] < 0)
            {
                bufferStride_all_positive = false;
                break;
            }
        }
    }
    for (size_t i = 0; i < dims.size(); i++)
    {
        assert(arrayStartIdx);
        assert(count);
        if (arrayStartIdx[i] >= dims[i]->GetSize())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "arrayStartIdx[%u] = " CPL_FRMT_GUIB " >= " CPL_FRMT_GUIB,
                     static_cast<unsigned>(i),
                     static_cast<GUInt64>(arrayStartIdx[i]),
                     static_cast<GUInt64>(dims[i]->GetSize()));
            return false;
        }
        bool bOverflow;
        if (arrayStep[i] >= 0)
        {
            try
            {
                bOverflow = (CPLSM(static_cast<uint64_t>(arrayStartIdx[i])) +
                             CPLSM(static_cast<uint64_t>(count[i] - 1)) *
                                 CPLSM(static_cast<uint64_t>(arrayStep[i])))
                                .v() >= dims[i]->GetSize();
            }
            catch (...)
            {
                bOverflow = true;
            }
            if (bOverflow)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "arrayStartIdx[%u] + (count[%u]-1) * arrayStep[%u] "
                         ">= " CPL_FRMT_GUIB,
                         static_cast<unsigned>(i), static_cast<unsigned>(i),
                         static_cast<unsigned>(i),
                         static_cast<GUInt64>(dims[i]->GetSize()));
                return false;
            }
        }
        else
        {
            try
            {
                bOverflow =
                    arrayStartIdx[i] <
                    (CPLSM(static_cast<uint64_t>(count[i] - 1)) *
                     CPLSM(arrayStep[i] == std::numeric_limits<GInt64>::min()
                               ? (static_cast<uint64_t>(1) << 63)
                               : static_cast<uint64_t>(-arrayStep[i])))
                        .v();
            }
            catch (...)
            {
                bOverflow = true;
            }
            if (bOverflow)
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "arrayStartIdx[%u] + (count[%u]-1) * arrayStep[%u] < 0",
                    static_cast<unsigned>(i), static_cast<unsigned>(i),
                    static_cast<unsigned>(i));
                return false;
            }
        }
    }

    if (buffer_alloc_start)
    {
        const size_t elementSize = bufferDataType.GetSize();
        const GByte *paby_buffer = static_cast<const GByte *>(buffer);
        const GByte *paby_buffer_alloc_start =
            static_cast<const GByte *>(buffer_alloc_start);
        const GByte *paby_buffer_alloc_end =
            paby_buffer_alloc_start + buffer_alloc_size;
        if (bufferStride_all_positive)
        {
            if (paby_buffer < paby_buffer_alloc_start)
            {
                lamda_error();
                return false;
            }
            GUInt64 nOffset = elementSize;
            for (size_t i = 0; i < dims.size(); i++)
            {
                try
                {
                    nOffset = (CPLSM(static_cast<uint64_t>(nOffset)) +
                               CPLSM(static_cast<uint64_t>(bufferStride[i])) *
                                   CPLSM(static_cast<uint64_t>(count[i] - 1)) *
                                   CPLSM(static_cast<uint64_t>(elementSize)))
                                  .v();
                }
                catch (...)
                {
                    lamda_error();
                    return false;
                }
            }
#if SIZEOF_VOIDP == 4
            if (static_cast<size_t>(nOffset) != nOffset)
            {
                lamda_error();
                return false;
            }
#endif
            if (paby_buffer + nOffset > paby_buffer_alloc_end)
            {
                lamda_error();
                return false;
            }
        }
        else if (dims.size() < 31)
        {
            // Check all corners of the hypercube
            const unsigned nLoops = 1U << static_cast<unsigned>(dims.size());
            for (unsigned iCornerCode = 0; iCornerCode < nLoops; iCornerCode++)
            {
                const GByte *paby = paby_buffer;
                for (unsigned i = 0; i < static_cast<unsigned>(dims.size());
                     i++)
                {
                    if (iCornerCode & (1U << i))
                    {
                        // We should check for integer overflows
                        paby += bufferStride[i] * (count[i] - 1) * elementSize;
                    }
                }
                if (paby < paby_buffer_alloc_start ||
                    paby + elementSize > paby_buffer_alloc_end)
                {
                    lamda_error();
                    return false;
                }
            }
        }
    }

    return true;
}

//! @endcond

/************************************************************************/
/*                                Read()                                */
/************************************************************************/

/** Read part or totality of a multidimensional array or attribute.
 *
 * This will extract the content of a hyper-rectangle from the array into
 * a user supplied buffer.
 *
 * If bufferDataType is of type string, the values written in pDstBuffer
 * will be char* pointers and the strings should be freed with CPLFree().
 *
 * This is the same as the C function GDALMDArrayRead().
 *
 * @param arrayStartIdx Values representing the starting index to read
 *                      in each dimension (in [0, aoDims[i].GetSize()-1] range).
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param count         Values representing the number of values to extract in
 *                      each dimension.
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param arrayStep     Spacing between values to extract in each dimension.
 *                      The spacing is in number of array elements, not bytes.
 *                      If provided, must contain GetDimensionCount() values.
 *                      If set to nullptr, [1, 1, ... 1] will be used as a
 * default to indicate consecutive elements.
 *
 * @param bufferStride  Spacing between values to store in pDstBuffer.
 *                      The spacing is in number of array elements, not bytes.
 *                      If provided, must contain GetDimensionCount() values.
 *                      Negative values are possible (for example to reorder
 *                      from bottom-to-top to top-to-bottom).
 *                      If set to nullptr, will be set so that pDstBuffer is
 *                      written in a compact way, with elements of the last /
 *                      fastest varying dimension being consecutive.
 *
 * @param bufferDataType Data type of values in pDstBuffer.
 *
 * @param pDstBuffer    User buffer to store the values read. Should be big
 *                      enough to store the number of values indicated by
 * count[] and with the spacing of bufferStride[].
 *
 * @param pDstBufferAllocStart Optional pointer that can be used to validate the
 *                             validity of pDstBuffer. pDstBufferAllocStart
 * should be the pointer returned by the malloc() or equivalent call used to
 * allocate the buffer. It will generally be equal to pDstBuffer (when
 * bufferStride[] values are all positive), but not necessarily. If specified,
 * nDstBufferAllocSize should be also set to the appropriate value. If no
 * validation is needed, nullptr can be passed.
 *
 * @param nDstBufferAllocSize  Optional buffer size, that can be used to
 * validate the validity of pDstBuffer. This is the size of the buffer starting
 * at pDstBufferAllocStart. If specified, pDstBufferAllocStart should be also
 *                             set to the appropriate value.
 *                             If no validation is needed, 0 can be passed.
 *
 * @return true in case of success.
 */
bool GDALAbstractMDArray::Read(
    const GUInt64 *arrayStartIdx, const size_t *count,
    const GInt64 *arrayStep,         // step in elements
    const GPtrDiff_t *bufferStride,  // stride in elements
    const GDALExtendedDataType &bufferDataType, void *pDstBuffer,
    const void *pDstBufferAllocStart, size_t nDstBufferAllocSize) const
{
    if (!GetDataType().CanConvertTo(bufferDataType))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Array data type is not convertible to buffer data type");
        return false;
    }

    std::vector<GInt64> tmp_arrayStep;
    std::vector<GPtrDiff_t> tmp_bufferStride;
    if (!CheckReadWriteParams(arrayStartIdx, count, arrayStep, bufferStride,
                              bufferDataType, pDstBuffer, pDstBufferAllocStart,
                              nDstBufferAllocSize, tmp_arrayStep,
                              tmp_bufferStride))
    {
        return false;
    }

    return IRead(arrayStartIdx, count, arrayStep, bufferStride, bufferDataType,
                 pDstBuffer);
}

/************************************************************************/
/*                               IWrite()                               */
/************************************************************************/

//! @cond Doxygen_Suppress
bool GDALAbstractMDArray::IWrite(const GUInt64 *, const size_t *,
                                 const GInt64 *, const GPtrDiff_t *,
                                 const GDALExtendedDataType &, const void *)
{
    CPLError(CE_Failure, CPLE_AppDefined, "IWrite() not implemented");
    return false;
}

//! @endcond

/************************************************************************/
/*                               Write()                                */
/************************************************************************/

/** Write part or totality of a multidimensional array or attribute.
 *
 * This will set the content of a hyper-rectangle into the array from
 * a user supplied buffer.
 *
 * If bufferDataType is of type string, the values read from pSrcBuffer
 * will be char* pointers.
 *
 * This is the same as the C function GDALMDArrayWrite().
 *
 * @param arrayStartIdx Values representing the starting index to write
 *                      in each dimension (in [0, aoDims[i].GetSize()-1] range).
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param count         Values representing the number of values to write in
 *                      each dimension.
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param arrayStep     Spacing between values to write in each dimension.
 *                      The spacing is in number of array elements, not bytes.
 *                      If provided, must contain GetDimensionCount() values.
 *                      If set to nullptr, [1, 1, ... 1] will be used as a
 * default to indicate consecutive elements.
 *
 * @param bufferStride  Spacing between values to read from pSrcBuffer.
 *                      The spacing is in number of array elements, not bytes.
 *                      If provided, must contain GetDimensionCount() values.
 *                      Negative values are possible (for example to reorder
 *                      from bottom-to-top to top-to-bottom).
 *                      If set to nullptr, will be set so that pSrcBuffer is
 *                      written in a compact way, with elements of the last /
 *                      fastest varying dimension being consecutive.
 *
 * @param bufferDataType Data type of values in pSrcBuffer.
 *
 * @param pSrcBuffer    User buffer to read the values from. Should be big
 *                      enough to store the number of values indicated by
 * count[] and with the spacing of bufferStride[].
 *
 * @param pSrcBufferAllocStart Optional pointer that can be used to validate the
 *                             validity of pSrcBuffer. pSrcBufferAllocStart
 * should be the pointer returned by the malloc() or equivalent call used to
 * allocate the buffer. It will generally be equal to pSrcBuffer (when
 * bufferStride[] values are all positive), but not necessarily. If specified,
 * nSrcBufferAllocSize should be also set to the appropriate value. If no
 * validation is needed, nullptr can be passed.
 *
 * @param nSrcBufferAllocSize  Optional buffer size, that can be used to
 * validate the validity of pSrcBuffer. This is the size of the buffer starting
 * at pSrcBufferAllocStart. If specified, pDstBufferAllocStart should be also
 *                             set to the appropriate value.
 *                             If no validation is needed, 0 can be passed.
 *
 * @return true in case of success.
 */
bool GDALAbstractMDArray::Write(const GUInt64 *arrayStartIdx,
                                const size_t *count, const GInt64 *arrayStep,
                                const GPtrDiff_t *bufferStride,
                                const GDALExtendedDataType &bufferDataType,
                                const void *pSrcBuffer,
                                const void *pSrcBufferAllocStart,
                                size_t nSrcBufferAllocSize)
{
    if (!bufferDataType.CanConvertTo(GetDataType()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Buffer data type is not convertible to array data type");
        return false;
    }

    std::vector<GInt64> tmp_arrayStep;
    std::vector<GPtrDiff_t> tmp_bufferStride;
    if (!CheckReadWriteParams(arrayStartIdx, count, arrayStep, bufferStride,
                              bufferDataType, pSrcBuffer, pSrcBufferAllocStart,
                              nSrcBufferAllocSize, tmp_arrayStep,
                              tmp_bufferStride))
    {
        return false;
    }

    return IWrite(arrayStartIdx, count, arrayStep, bufferStride, bufferDataType,
                  pSrcBuffer);
}

/************************************************************************/
/*                       GetTotalElementsCount()                        */
/************************************************************************/

/** Return the total number of values in the array.
 *
 * This is the same as the C functions GDALMDArrayGetTotalElementsCount()
 * and GDALAttributeGetTotalElementsCount().
 *
 */
GUInt64 GDALAbstractMDArray::GetTotalElementsCount() const
{
    const auto &dims = GetDimensions();
    if (dims.empty())
        return 1;
    GUInt64 nElts = 1;
    for (const auto &dim : dims)
    {
        try
        {
            nElts = (CPLSM(static_cast<uint64_t>(nElts)) *
                     CPLSM(static_cast<uint64_t>(dim->GetSize())))
                        .v();
        }
        catch (...)
        {
            return 0;
        }
    }
    return nElts;
}

/************************************************************************/
/*                            GetBlockSize()                            */
/************************************************************************/

/** Return the "natural" block size of the array along all dimensions.
 *
 * Some drivers might organize the array in tiles/blocks and reading/writing
 * aligned on those tile/block boundaries will be more efficient.
 *
 * The returned number of elements in the vector is the same as
 * GetDimensionCount(). A value of 0 should be interpreted as no hint regarding
 * the natural block size along the considered dimension.
 * "Flat" arrays will typically return a vector of values set to 0.
 *
 * The default implementation will return a vector of values set to 0.
 *
 * This method is used by GetProcessingChunkSize().
 *
 * Pedantic note: the returned type is GUInt64, so in the highly unlikely
 * theoretical case of a 32-bit platform, this might exceed its size_t
 * allocation capabilities.
 *
 * This is the same as the C function GDALMDArrayGetBlockSize().
 *
 * @return the block size, in number of elements along each dimension.
 */
std::vector<GUInt64> GDALAbstractMDArray::GetBlockSize() const
{
    return std::vector<GUInt64>(GetDimensionCount());
}

/************************************************************************/
/*                       GetProcessingChunkSize()                       */
/************************************************************************/

/** \brief Return an optimal chunk size for read/write operations, given the
 * natural block size and memory constraints specified.
 *
 * This method will use GetBlockSize() to define a chunk whose dimensions are
 * multiple of those returned by GetBlockSize() (unless the block define by
 * GetBlockSize() is larger than nMaxChunkMemory, in which case it will be
 * returned by this method).
 *
 * This is the same as the C function GDALMDArrayGetProcessingChunkSize().
 *
 * @param nMaxChunkMemory Maximum amount of memory, in bytes, to use for the
 * chunk.
 *
 * @return the chunk size, in number of elements along each dimension.
 */
std::vector<size_t>
GDALAbstractMDArray::GetProcessingChunkSize(size_t nMaxChunkMemory) const
{
    const auto &dims = GetDimensions();
    const auto &nDTSize = GetDataType().GetSize();
    std::vector<size_t> anChunkSize;
    auto blockSize = GetBlockSize();
    CPLAssert(blockSize.size() == dims.size());
    size_t nChunkSize = nDTSize;
    bool bOverflow = false;
    constexpr auto kSIZE_T_MAX = std::numeric_limits<size_t>::max();
    // Initialize anChunkSize[i] with blockSize[i] by properly clamping in
    // [1, min(sizet_max, dim_size[i])]
    // Also make sure that the product of all anChunkSize[i]) fits on size_t
    for (size_t i = 0; i < dims.size(); i++)
    {
        const auto sizeDimI =
            std::max(static_cast<size_t>(1),
                     static_cast<size_t>(
                         std::min(static_cast<GUInt64>(kSIZE_T_MAX),
                                  std::min(blockSize[i], dims[i]->GetSize()))));
        anChunkSize.push_back(sizeDimI);
        if (nChunkSize > kSIZE_T_MAX / sizeDimI)
        {
            bOverflow = true;
        }
        else
        {
            nChunkSize *= sizeDimI;
        }
    }
    if (nChunkSize == 0)
        return anChunkSize;

    // If the product of all anChunkSize[i] does not fit on size_t, then
    // set lowest anChunkSize[i] to 1.
    if (bOverflow)
    {
        nChunkSize = nDTSize;
        bOverflow = false;
        for (size_t i = dims.size(); i > 0;)
        {
            --i;
            if (bOverflow || nChunkSize > kSIZE_T_MAX / anChunkSize[i])
            {
                bOverflow = true;
                anChunkSize[i] = 1;
            }
            else
            {
                nChunkSize *= anChunkSize[i];
            }
        }
    }

    nChunkSize = nDTSize;
    std::vector<size_t> anAccBlockSizeFromStart;
    for (size_t i = 0; i < dims.size(); i++)
    {
        nChunkSize *= anChunkSize[i];
        anAccBlockSizeFromStart.push_back(nChunkSize);
    }
    if (nChunkSize <= nMaxChunkMemory / 2)
    {
        size_t nVoxelsFromEnd = 1;
        for (size_t i = dims.size(); i > 0;)
        {
            --i;
            const auto nCurBlockSize =
                anAccBlockSizeFromStart[i] * nVoxelsFromEnd;
            const auto nMul = nMaxChunkMemory / nCurBlockSize;
            if (nMul >= 2)
            {
                const auto nSizeThisDim(dims[i]->GetSize());
                const auto nBlocksThisDim =
                    cpl::div_round_up(nSizeThisDim, anChunkSize[i]);
                anChunkSize[i] = static_cast<size_t>(std::min(
                    anChunkSize[i] *
                        std::min(static_cast<GUInt64>(nMul), nBlocksThisDim),
                    nSizeThisDim));
            }
            nVoxelsFromEnd *= anChunkSize[i];
        }
    }
    return anChunkSize;
}

/************************************************************************/
/*                             BaseRename()                             */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALAbstractMDArray::BaseRename(const std::string &osNewName)
{
    m_osFullName.resize(m_osFullName.size() - m_osName.size());
    m_osFullName += osNewName;
    m_osName = osNewName;

    NotifyChildrenOfRenaming();
}

//! @endcond

//! @cond Doxygen_Suppress
/************************************************************************/
/*                           ParentRenamed()                            */
/************************************************************************/

void GDALAbstractMDArray::ParentRenamed(const std::string &osNewParentFullName)
{
    m_osFullName = osNewParentFullName;
    m_osFullName += "/";
    m_osFullName += m_osName;

    NotifyChildrenOfRenaming();
}

//! @endcond

/************************************************************************/
/*                              Deleted()                               */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALAbstractMDArray::Deleted()
{
    m_bValid = false;

    NotifyChildrenOfDeletion();
}

//! @endcond

/************************************************************************/
/*                           ParentDeleted()                            */
/************************************************************************/

//! @cond Doxygen_Suppress
void GDALAbstractMDArray::ParentDeleted()
{
    Deleted();
}

//! @endcond

/************************************************************************/
/*                     CheckValidAndErrorOutIfNot()                     */
/************************************************************************/

//! @cond Doxygen_Suppress
bool GDALAbstractMDArray::CheckValidAndErrorOutIfNot() const
{
    if (!m_bValid)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This object has been deleted. No action on it is possible");
    }
    return m_bValid;
}

//! @endcond

/************************************************************************/
/*                          ProcessPerChunk()                           */
/************************************************************************/

namespace
{
enum class Caller
{
    CALLER_END_OF_LOOP,
    CALLER_IN_LOOP,
};
}

/** \brief Call a user-provided function to operate on an array chunk by chunk.
 *
 * This method is to be used when doing operations on an array, or a subset of
 * it, in a chunk by chunk way.
 *
 * @param arrayStartIdx Values representing the starting index to use
 *                      in each dimension (in [0, aoDims[i].GetSize()-1] range).
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param count         Values representing the number of values to use in
 *                      each dimension.
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param chunkSize     Values representing the chunk size in each dimension.
 *                      Might typically the output of GetProcessingChunkSize().
 *                      Array of GetDimensionCount() values. Must not be
 *                      nullptr, unless for a zero-dimensional array.
 *
 * @param pfnFunc       User-provided function of type FuncProcessPerChunkType.
 *                      Must NOT be nullptr.
 *
 * @param pUserData     Pointer to pass as the value of the pUserData argument
 * of FuncProcessPerChunkType. Might be nullptr (depends on pfnFunc.
 *
 * @return true in case of success.
 */
bool GDALAbstractMDArray::ProcessPerChunk(const GUInt64 *arrayStartIdx,
                                          const GUInt64 *count,
                                          const size_t *chunkSize,
                                          FuncProcessPerChunkType pfnFunc,
                                          void *pUserData)
{
    const auto &dims = GetDimensions();
    if (dims.empty())
    {
        return pfnFunc(this, nullptr, nullptr, 1, 1, pUserData);
    }

    // Sanity check
    size_t nTotalChunkSize = 1;
    for (size_t i = 0; i < dims.size(); i++)
    {
        const auto nSizeThisDim(dims[i]->GetSize());
        if (count[i] == 0 || count[i] > nSizeThisDim ||
            arrayStartIdx[i] > nSizeThisDim - count[i])
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inconsistent arrayStartIdx[] / count[] values "
                     "regarding array size");
            return false;
        }
        if (chunkSize[i] == 0 || chunkSize[i] > nSizeThisDim ||
            chunkSize[i] > std::numeric_limits<size_t>::max() / nTotalChunkSize)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Inconsistent chunkSize[] values");
            return false;
        }
        nTotalChunkSize *= chunkSize[i];
    }

    size_t dimIdx = 0;
    std::vector<GUInt64> chunkArrayStartIdx(dims.size());
    std::vector<size_t> chunkCount(dims.size());

    struct Stack
    {
        GUInt64 nBlockCounter = 0;
        GUInt64 nBlocksMinusOne = 0;
        size_t first_count = 0;  // only used if nBlocks > 1
        Caller return_point = Caller::CALLER_END_OF_LOOP;
    };

    std::vector<Stack> stack(dims.size());
    GUInt64 iCurChunk = 0;
    GUInt64 nChunkCount = 1;
    for (size_t i = 0; i < dims.size(); i++)
    {
        const auto nStartBlock = arrayStartIdx[i] / chunkSize[i];
        const auto nEndBlock = (arrayStartIdx[i] + count[i] - 1) / chunkSize[i];
        stack[i].nBlocksMinusOne = nEndBlock - nStartBlock;
        nChunkCount *= 1 + stack[i].nBlocksMinusOne;
        if (stack[i].nBlocksMinusOne == 0)
        {
            chunkArrayStartIdx[i] = arrayStartIdx[i];
            chunkCount[i] = static_cast<size_t>(count[i]);
        }
        else
        {
            stack[i].first_count = static_cast<size_t>(
                (nStartBlock + 1) * chunkSize[i] - arrayStartIdx[i]);
        }
    }

lbl_next_depth:
    if (dimIdx == dims.size())
    {
        ++iCurChunk;
        if (!pfnFunc(this, chunkArrayStartIdx.data(), chunkCount.data(),
                     iCurChunk, nChunkCount, pUserData))
        {
            return false;
        }
    }
    else
    {
        if (stack[dimIdx].nBlocksMinusOne != 0)
        {
            stack[dimIdx].nBlockCounter = stack[dimIdx].nBlocksMinusOne;
            chunkArrayStartIdx[dimIdx] = arrayStartIdx[dimIdx];
            chunkCount[dimIdx] = stack[dimIdx].first_count;
            stack[dimIdx].return_point = Caller::CALLER_IN_LOOP;
            while (true)
            {
                dimIdx++;
                goto lbl_next_depth;
            lbl_return_to_caller_in_loop:
                --stack[dimIdx].nBlockCounter;
                if (stack[dimIdx].nBlockCounter == 0)
                    break;
                chunkArrayStartIdx[dimIdx] += chunkCount[dimIdx];
                chunkCount[dimIdx] = chunkSize[dimIdx];
            }

            chunkArrayStartIdx[dimIdx] += chunkCount[dimIdx];
            chunkCount[dimIdx] =
                static_cast<size_t>(arrayStartIdx[dimIdx] + count[dimIdx] -
                                    chunkArrayStartIdx[dimIdx]);
            stack[dimIdx].return_point = Caller::CALLER_END_OF_LOOP;
        }
        dimIdx++;
        goto lbl_next_depth;
    lbl_return_to_caller_end_of_loop:
        if (dimIdx == 0)
            goto end;
    }

    assert(dimIdx > 0);
    dimIdx--;
    // cppcheck-suppress negativeContainerIndex
    switch (stack[dimIdx].return_point)
    {
        case Caller::CALLER_END_OF_LOOP:
            goto lbl_return_to_caller_end_of_loop;
        case Caller::CALLER_IN_LOOP:
            goto lbl_return_to_caller_in_loop;
    }
end:
    return true;
}
