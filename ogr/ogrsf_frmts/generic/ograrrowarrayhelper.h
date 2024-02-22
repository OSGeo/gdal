/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Helper to fill ArrowArray
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
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

#pragma once

//! @cond Doxygen_Suppress

#include <algorithm>
#include <limits>

#include "cpl_time.h"

#include "ogrsf_frmts.h"
#include "ogr_recordbatch.h"

class CPL_DLL OGRArrowArrayHelper
{
    OGRArrowArrayHelper(const OGRArrowArrayHelper &) = delete;
    OGRArrowArrayHelper &operator=(const OGRArrowArrayHelper &) = delete;

  public:
    bool m_bIncludeFID = false;
    int m_nMaxBatchSize = 0;
    int m_nChildren = 0;
    const int m_nFieldCount = 0;
    const int m_nGeomFieldCount = 0;
    std::vector<int> m_mapOGRFieldToArrowField{};
    std::vector<int> m_mapOGRGeomFieldToArrowField{};
    std::vector<bool> m_abNullableFields{};
    std::vector<uint32_t> m_anArrowFieldMaxAlloc{};
    std::vector<int> m_anTZFlags{};
    int64_t *m_panFIDValues = nullptr;
    struct ArrowArray *m_out_array = nullptr;

    static uint32_t GetMemLimit();

    static int
    GetMaxFeaturesInBatch(const CPLStringList &aosArrowArrayStreamOptions);

    OGRArrowArrayHelper(GDALDataset *poDS, OGRFeatureDefn *poFeatureDefn,
                        const CPLStringList &aosArrowArrayStreamOptions,
                        struct ArrowArray *out_array);

    bool SetNull(int iArrowField, int iFeat)
    {
        auto psArray = m_out_array->children[iArrowField];
        ++psArray->null_count;
        uint8_t *pabyNull =
            static_cast<uint8_t *>(const_cast<void *>(psArray->buffers[0]));
        if (psArray->buffers[0] == nullptr)
        {
            pabyNull = static_cast<uint8_t *>(
                VSI_MALLOC_ALIGNED_AUTO_VERBOSE((m_nMaxBatchSize + 7) / 8));
            if (pabyNull == nullptr)
            {
                return false;
            }
            memset(pabyNull, 0xFF, (m_nMaxBatchSize + 7) / 8);
            psArray->buffers[0] = pabyNull;
        }
        pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));

        if (psArray->n_buffers == 3)
        {
            auto panOffsets =
                static_cast<int32_t *>(const_cast<void *>(psArray->buffers[1]));
            panOffsets[iFeat + 1] = panOffsets[iFeat];
        }
        return true;
    }

    inline static void SetBoolOn(struct ArrowArray *psArray, int iFeat)
    {
        static_cast<uint8_t *>(
            const_cast<void *>(psArray->buffers[1]))[iFeat / 8] |=
            static_cast<uint8_t>(1 << (iFeat % 8));
    }

    inline static void SetInt8(struct ArrowArray *psArray, int iFeat,
                               int8_t nVal)
    {
        static_cast<int8_t *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            nVal;
    }

    inline static void SetUInt8(struct ArrowArray *psArray, int iFeat,
                                uint8_t nVal)
    {
        static_cast<uint8_t *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            nVal;
    }

    inline static void SetInt16(struct ArrowArray *psArray, int iFeat,
                                int16_t nVal)
    {
        static_cast<int16_t *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            nVal;
    }

    inline static void SetUInt16(struct ArrowArray *psArray, int iFeat,
                                 uint16_t nVal)
    {
        static_cast<uint16_t *>(
            const_cast<void *>(psArray->buffers[1]))[iFeat] = nVal;
    }

    inline static void SetInt32(struct ArrowArray *psArray, int iFeat,
                                int32_t nVal)
    {
        static_cast<int32_t *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            nVal;
    }

    inline static void SetUInt32(struct ArrowArray *psArray, int iFeat,
                                 uint32_t nVal)
    {
        static_cast<uint32_t *>(
            const_cast<void *>(psArray->buffers[1]))[iFeat] = nVal;
    }

    inline static void SetInt64(struct ArrowArray *psArray, int iFeat,
                                int64_t nVal)
    {
        static_cast<int64_t *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            nVal;
    }

    inline static void SetUInt64(struct ArrowArray *psArray, int iFeat,
                                 uint64_t nVal)
    {
        static_cast<uint64_t *>(
            const_cast<void *>(psArray->buffers[1]))[iFeat] = nVal;
    }

    inline static void SetFloat(struct ArrowArray *psArray, int iFeat,
                                float fVal)
    {
        static_cast<float *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            fVal;
    }

    inline static void SetDouble(struct ArrowArray *psArray, int iFeat,
                                 double dfVal)
    {
        static_cast<double *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            dfVal;
    }

    static void SetDate(struct ArrowArray *psArray, int iFeat,
                        struct tm &brokenDown, const OGRField &ogrField)
    {
        brokenDown.tm_year = ogrField.Date.Year - 1900;
        brokenDown.tm_mon = ogrField.Date.Month - 1;
        brokenDown.tm_mday = ogrField.Date.Day;
        brokenDown.tm_hour = 0;
        brokenDown.tm_min = 0;
        brokenDown.tm_sec = 0;
        static_cast<int32_t *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            static_cast<int>(CPLYMDHMSToUnixTime(&brokenDown) / 86400);
    }

    static void SetDateTime(struct ArrowArray *psArray, int iFeat,
                            struct tm &brokenDown, int nFieldTZFlag,
                            const OGRField &ogrField)
    {
        brokenDown.tm_year = ogrField.Date.Year - 1900;
        brokenDown.tm_mon = ogrField.Date.Month - 1;
        brokenDown.tm_mday = ogrField.Date.Day;
        brokenDown.tm_hour = ogrField.Date.Hour;
        brokenDown.tm_min = ogrField.Date.Minute;
        brokenDown.tm_sec = static_cast<int>(ogrField.Date.Second);
        auto nVal =
            CPLYMDHMSToUnixTime(&brokenDown) * 1000 +
            (static_cast<int>(ogrField.Date.Second * 1000 + 0.5) % 1000);
        if (nFieldTZFlag >= OGR_TZFLAG_MIXED_TZ &&
            ogrField.Date.TZFlag > OGR_TZFLAG_MIXED_TZ)
        {
            // Convert for ogrField.Date.TZFlag to UTC
            const int TZOffset = (ogrField.Date.TZFlag - OGR_TZFLAG_UTC) * 15;
            const int TZOffsetMS = TZOffset * 60 * 1000;
            nVal -= TZOffsetMS;
        }
        static_cast<int64_t *>(const_cast<void *>(psArray->buffers[1]))[iFeat] =
            nVal;
    }

    GByte *GetPtrForStringOrBinary(int iArrowField, int iFeat, size_t nLen)
    {
        auto psArray = m_out_array->children[iArrowField];
        auto panOffsets =
            static_cast<int32_t *>(const_cast<void *>(psArray->buffers[1]));
        const uint32_t nCurLength = static_cast<uint32_t>(panOffsets[iFeat]);
        if (nLen > m_anArrowFieldMaxAlloc[iArrowField] - nCurLength)
        {
            if (nLen >
                static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) -
                    nCurLength)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too large string or binary content");
                return nullptr;
            }
            uint32_t nNewSize = nCurLength + static_cast<uint32_t>(nLen);
            if ((m_anArrowFieldMaxAlloc[iArrowField] >> 31) == 0)
            {
                const uint32_t nDoubleSize =
                    2U * m_anArrowFieldMaxAlloc[iArrowField];
                if (nNewSize < nDoubleSize)
                    nNewSize = nDoubleSize;
            }
            void *newBuffer = VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nNewSize);
            if (newBuffer == nullptr)
                return nullptr;
            m_anArrowFieldMaxAlloc[iArrowField] = nNewSize;
            memcpy(newBuffer, psArray->buffers[2], nCurLength);
            VSIFreeAligned(const_cast<void *>(psArray->buffers[2]));
            psArray->buffers[2] = newBuffer;
        }
        GByte *paby =
            static_cast<GByte *>(const_cast<void *>(psArray->buffers[2])) +
            nCurLength;
        panOffsets[iFeat + 1] = panOffsets[iFeat] + static_cast<int32_t>(nLen);
        return paby;
    }

    static void SetEmptyStringOrBinary(struct ArrowArray *psArray, int iFeat)
    {
        auto panOffsets =
            static_cast<int32_t *>(const_cast<void *>(psArray->buffers[1]));
        panOffsets[iFeat + 1] = panOffsets[iFeat];
    }

    void Shrink(int nFeatures)
    {
        if (nFeatures < m_nMaxBatchSize)
        {
            m_out_array->length = nFeatures;
            for (int i = 0; i < m_nChildren; i++)
            {
                m_out_array->children[i]->length = nFeatures;
            }
        }
    }

    void ClearArray()
    {
        if (m_out_array->release)
            m_out_array->release(m_out_array);
        memset(m_out_array, 0, sizeof(*m_out_array));
    }

    static bool FillDict(struct ArrowArray *psChild,
                         const OGRCodedFieldDomain *poCodedDomain);
};

//! @endcond
