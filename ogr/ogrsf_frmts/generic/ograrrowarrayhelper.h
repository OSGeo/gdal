/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Helper to fill ArrowArray
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
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

    //! Construct an helper from an already initialized array
    OGRArrowArrayHelper(struct ArrowArray *out_array, int nMaxBatchSize);

    static bool SetNull(struct ArrowArray *psArray, int iFeat,
                        int nMaxBatchSize, bool bAlignedMalloc)
    {
        ++psArray->null_count;
        uint8_t *pabyNull =
            static_cast<uint8_t *>(const_cast<void *>(psArray->buffers[0]));
        if (psArray->buffers[0] == nullptr)
        {
            pabyNull = static_cast<uint8_t *>(
                bAlignedMalloc
                    ? VSI_MALLOC_ALIGNED_AUTO_VERBOSE((nMaxBatchSize + 7) / 8)
                    : VSI_MALLOC_VERBOSE((nMaxBatchSize + 7) / 8));
            if (pabyNull == nullptr)
            {
                return false;
            }
            memset(pabyNull, 0xFF, (nMaxBatchSize + 7) / 8);
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

    bool SetNull(int iArrowField, int iFeat)
    {
        return SetNull(m_out_array->children[iArrowField], iFeat,
                       m_nMaxBatchSize, true);
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
            (static_cast<int>(ogrField.Date.Second * 1000 + 0.5f) % 1000);
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

    static GByte *GetPtrForStringOrBinary(struct ArrowArray *psArray, int iFeat,
                                          size_t nLen, uint32_t &nMaxAlloc,
                                          bool bAlignedMalloc)
    {
        auto panOffsets =
            static_cast<int32_t *>(const_cast<void *>(psArray->buffers[1]));
        const uint32_t nCurLength = static_cast<uint32_t>(panOffsets[iFeat]);
        if (nLen > nMaxAlloc - nCurLength)
        {
            constexpr uint32_t INT32_MAX_AS_UINT32 =
                static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
            if (!(nCurLength <= INT32_MAX_AS_UINT32 &&
                  nLen <= INT32_MAX_AS_UINT32 - nCurLength))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too large string or binary content");
                return nullptr;
            }
            uint32_t nNewSize = nCurLength + static_cast<uint32_t>(nLen);
            if (nMaxAlloc <= INT32_MAX_AS_UINT32)
            {
                const uint32_t nDoubleSize = 2U * nMaxAlloc;
                if (nNewSize < nDoubleSize)
                    nNewSize = nDoubleSize;
            }
            void *newBuffer;
            if (bAlignedMalloc)
            {
                newBuffer = VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nNewSize);
                if (newBuffer == nullptr)
                    return nullptr;
                nMaxAlloc = nNewSize;
                memcpy(newBuffer, psArray->buffers[2], nCurLength);
                VSIFreeAligned(const_cast<void *>(psArray->buffers[2]));
            }
            else
            {
                // coverity[overflow_sink]
                newBuffer = VSI_REALLOC_VERBOSE(
                    const_cast<void *>(psArray->buffers[2]), nNewSize);
                if (newBuffer == nullptr)
                    return nullptr;
                nMaxAlloc = nNewSize;
            }
            psArray->buffers[2] = newBuffer;
        }
        GByte *paby =
            static_cast<GByte *>(const_cast<void *>(psArray->buffers[2])) +
            nCurLength;
        panOffsets[iFeat + 1] = panOffsets[iFeat] + static_cast<int32_t>(nLen);
        return paby;
    }

    GByte *GetPtrForStringOrBinary(int iArrowField, int iFeat, size_t nLen,
                                   bool bAlignedMalloc = true)
    {
        auto psArray = m_out_array->children[iArrowField];
        return GetPtrForStringOrBinary(psArray, iFeat, nLen,
                                       m_anArrowFieldMaxAlloc[iArrowField],
                                       bAlignedMalloc);
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
