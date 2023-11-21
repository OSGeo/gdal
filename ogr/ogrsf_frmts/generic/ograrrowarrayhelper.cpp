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

#include "ograrrowarrayhelper.h"
#include "ogr_p.h"

#include <limits>

//! @cond Doxygen_Suppress

/************************************************************************/
/*                           GetMemLimit()                              */
/************************************************************************/

/*static*/ uint32_t OGRArrowArrayHelper::GetMemLimit()
{
    uint32_t nMemLimit =
        static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
    // Just for tests
    const char *pszOGR_ARROW_MEM_LIMIT =
        CPLGetConfigOption("OGR_ARROW_MEM_LIMIT", nullptr);
    if (pszOGR_ARROW_MEM_LIMIT)
        nMemLimit = atoi(pszOGR_ARROW_MEM_LIMIT);
    else
    {
        const uint64_t nUsableRAM = CPLGetUsablePhysicalRAM();
        if (nUsableRAM > 0 && nUsableRAM / 4 < nMemLimit)
            nMemLimit = static_cast<uint32_t>(nUsableRAM / 4);
    }
    return nMemLimit;
}

/************************************************************************/
/*                       GetMaxFeaturesInBatch()                        */
/************************************************************************/

/* static */
int OGRArrowArrayHelper::GetMaxFeaturesInBatch(
    const CPLStringList &aosArrowArrayStreamOptions)
{
    int l_nMaxBatchSize = atoi(aosArrowArrayStreamOptions.FetchNameValueDef(
        "MAX_FEATURES_IN_BATCH", "65536"));
    if (l_nMaxBatchSize <= 0)
        l_nMaxBatchSize = 1;
    if (l_nMaxBatchSize > INT_MAX - 1)
        l_nMaxBatchSize = INT_MAX - 1;

    return l_nMaxBatchSize;
}

/************************************************************************/
/*                       OGRArrowArrayHelper()                          */
/************************************************************************/

OGRArrowArrayHelper::OGRArrowArrayHelper(
    GDALDataset *poDS, OGRFeatureDefn *poFeatureDefn,
    const CPLStringList &aosArrowArrayStreamOptions,
    struct ArrowArray *out_array)
    : m_bIncludeFID(CPLTestBool(
          aosArrowArrayStreamOptions.FetchNameValueDef("INCLUDE_FID", "YES"))),
      m_nMaxBatchSize(GetMaxFeaturesInBatch(aosArrowArrayStreamOptions)),
      m_nFieldCount(poFeatureDefn->GetFieldCount()),
      m_nGeomFieldCount(poFeatureDefn->GetGeomFieldCount()),
      m_out_array(out_array)
{
    memset(out_array, 0, sizeof(*out_array));

    m_mapOGRFieldToArrowField.resize(m_nFieldCount, -1);
    m_mapOGRGeomFieldToArrowField.resize(m_nGeomFieldCount, -1);
    m_abNullableFields.resize(m_nFieldCount);
    m_anTZFlags.resize(m_nFieldCount);
    int nTZFlagOverride = -1;
    const char *pszTZOverride =
        aosArrowArrayStreamOptions.FetchNameValue("TIMEZONE");
    if (pszTZOverride)
    {
        if (EQUAL(pszTZOverride, "unknown") || EQUAL(pszTZOverride, ""))
        {
            nTZFlagOverride = OGR_TZFLAG_UNKNOWN;
        }
        else
        {
            // we don't really care about the actual timezone, since we
            // will convert OGRField::Date to UTC in all cases
            nTZFlagOverride = OGR_TZFLAG_UTC;
        }
    }

    if (m_bIncludeFID)
    {
        m_nChildren++;
    }
    for (int i = 0; i < m_nFieldCount; i++)
    {
        const auto poFieldDefn = poFeatureDefn->GetFieldDefn(i);
        m_abNullableFields[i] = CPL_TO_BOOL(poFieldDefn->IsNullable());
        m_anTZFlags[i] =
            nTZFlagOverride >= 0 ? nTZFlagOverride : poFieldDefn->GetTZFlag();
        if (!poFieldDefn->IsIgnored())
        {
            m_mapOGRFieldToArrowField[i] = m_nChildren;
            m_nChildren++;
        }
    }
    for (int i = 0; i < m_nGeomFieldCount; i++)
    {
        if (!poFeatureDefn->GetGeomFieldDefn(i)->IsIgnored())
        {
            m_mapOGRGeomFieldToArrowField[i] = m_nChildren;
            m_nChildren++;
        }
    }

    m_anArrowFieldMaxAlloc.resize(m_nChildren);

    out_array->release = OGRLayer::ReleaseArray;

    out_array->length = m_nMaxBatchSize;
    out_array->null_count = 0;

    out_array->n_children = m_nChildren;
    out_array->children = static_cast<struct ArrowArray **>(
        CPLCalloc(m_nChildren, sizeof(struct ArrowArray *)));
    out_array->release = OGRLayer::ReleaseArray;
    out_array->n_buffers = 1;
    out_array->buffers =
        static_cast<const void **>(CPLCalloc(1, sizeof(void *)));

    // Allocate buffers

    if (m_bIncludeFID)
    {
        out_array->children[0] = static_cast<struct ArrowArray *>(
            CPLCalloc(1, sizeof(struct ArrowArray)));
        auto psChild = out_array->children[0];
        psChild->release = OGRLayer::ReleaseArray;
        psChild->length = m_nMaxBatchSize;
        psChild->n_buffers = 2;
        psChild->buffers =
            static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
        m_panFIDValues = static_cast<int64_t *>(
            VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(int64_t) * m_nMaxBatchSize));
        if (m_panFIDValues == nullptr)
            goto error;
        psChild->buffers[1] = m_panFIDValues;
    }

    for (int i = 0; i < m_nFieldCount; i++)
    {
        const int iArrowField = m_mapOGRFieldToArrowField[i];
        if (iArrowField >= 0)
        {
            const auto poFieldDefn = poFeatureDefn->GetFieldDefn(i);
            out_array->children[iArrowField] = static_cast<struct ArrowArray *>(
                CPLCalloc(1, sizeof(struct ArrowArray)));
            auto psChild = out_array->children[iArrowField];

            psChild->release = OGRLayer::ReleaseArray;
            psChild->length = m_nMaxBatchSize;
            const auto eSubType = poFieldDefn->GetSubType();
            size_t nEltSize = 0;
            switch (poFieldDefn->GetType())
            {
                case OFTInteger:
                {
                    if (eSubType == OFSTBoolean)
                    {
                        nEltSize = sizeof(uint8_t);
                    }
                    else if (eSubType == OFSTInt16)
                    {
                        nEltSize = sizeof(int16_t);
                    }
                    else
                    {
                        nEltSize = sizeof(int32_t);
                    }

                    const auto &osDomainName = poFieldDefn->GetDomainName();
                    if (!osDomainName.empty() && poDS != nullptr)
                    {
                        const auto poFieldDomain =
                            poDS->GetFieldDomain(osDomainName);
                        if (poFieldDomain &&
                            poFieldDomain->GetDomainType() == OFDT_CODED)
                        {
                            const OGRCodedFieldDomain *poCodedDomain =
                                static_cast<const OGRCodedFieldDomain *>(
                                    poFieldDomain);
                            FillDict(psChild, poCodedDomain);
                        }
                    }

                    break;
                }
                case OFTInteger64:
                {
                    nEltSize = sizeof(int64_t);
                    break;
                }
                case OFTReal:
                {
                    if (eSubType == OFSTFloat32)
                    {
                        nEltSize = sizeof(float);
                    }
                    else
                    {
                        nEltSize = sizeof(double);
                    }
                    break;
                }
                case OFTString:
                case OFTBinary:
                {
                    psChild->n_buffers = 3;
                    psChild->buffers = static_cast<const void **>(
                        CPLCalloc(3, sizeof(void *)));
                    psChild->buffers[1] = VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        sizeof(uint32_t) * (1 + m_nMaxBatchSize));
                    if (psChild->buffers[1] == nullptr)
                        goto error;
                    memset(const_cast<void *>(psChild->buffers[1]), 0,
                           sizeof(uint32_t) * (1 + m_nMaxBatchSize));
                    constexpr size_t DEFAULT_STRING_SIZE = 10;
                    m_anArrowFieldMaxAlloc[iArrowField] =
                        DEFAULT_STRING_SIZE * m_nMaxBatchSize;
                    psChild->buffers[2] = VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        m_anArrowFieldMaxAlloc[iArrowField]);
                    if (psChild->buffers[2] == nullptr)
                        goto error;
                    break;
                }

                case OFTDate:
                {
                    nEltSize = sizeof(int32_t);
                    break;
                }

                case OFTTime:
                {
                    nEltSize = sizeof(int32_t);
                    break;
                }

                case OFTDateTime:
                {
                    nEltSize = sizeof(int64_t);
                    break;
                }

                default:
                    break;
            }

            if (nEltSize != 0)
            {
                psChild->n_buffers = 2;
                psChild->buffers =
                    static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
                psChild->buffers[1] =
                    VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nEltSize * m_nMaxBatchSize);
                if (psChild->buffers[1] == nullptr)
                    goto error;
                memset(const_cast<void *>(psChild->buffers[1]), 0,
                       nEltSize * m_nMaxBatchSize);
            }
        }
    }

    for (int i = 0; i < m_nGeomFieldCount; i++)
    {
        const int iArrowField = m_mapOGRGeomFieldToArrowField[i];
        if (iArrowField >= 0)
        {
            out_array->children[iArrowField] = static_cast<struct ArrowArray *>(
                CPLCalloc(1, sizeof(struct ArrowArray)));
            auto psChild = out_array->children[iArrowField];

            psChild->release = OGRLayer::ReleaseArray;
            psChild->length = m_nMaxBatchSize;

            psChild->n_buffers = 3;
            psChild->buffers =
                static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
            psChild->buffers[1] = VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                sizeof(uint32_t) * (1 + m_nMaxBatchSize));
            if (psChild->buffers[1] == nullptr)
                goto error;
            memset(const_cast<void *>(psChild->buffers[1]), 0,
                   sizeof(uint32_t) * (1 + m_nMaxBatchSize));
            constexpr size_t DEFAULT_WKB_SIZE = 100;
            m_anArrowFieldMaxAlloc[iArrowField] =
                DEFAULT_WKB_SIZE * m_nMaxBatchSize;
            psChild->buffers[2] = VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                m_anArrowFieldMaxAlloc[iArrowField]);
            if (psChild->buffers[2] == nullptr)
                goto error;
        }
    }

    return;

error:
    out_array->release(out_array);
    memset(out_array, 0, sizeof(*out_array));
}

/************************************************************************/
/*                             FillDict()                               */
/************************************************************************/

/* static */
bool OGRArrowArrayHelper::FillDict(struct ArrowArray *psChild,
                                   const OGRCodedFieldDomain *poCodedDomain)
{
    int nLastCode = -1;
    uint32_t nCountChars = 0;
    int nCountNull = 0;
    for (const OGRCodedValue *psIter = poCodedDomain->GetEnumeration();
         psIter->pszCode; ++psIter)
    {
        if (CPLGetValueType(psIter->pszCode) != CPL_VALUE_INTEGER)
        {
            return false;
        }
        int nCode = atoi(psIter->pszCode);
        if (nCode <= nLastCode || nCode - nLastCode > 100)
        {
            return false;
        }
        for (int i = nLastCode + 1; i < nCode; ++i)
        {
            nCountNull++;
        }
        if (psIter->pszValue)
        {
            const size_t nLen = strlen(psIter->pszValue);
            if (nLen > std::numeric_limits<uint32_t>::max() - nCountChars)
                return false;
            nCountChars += static_cast<uint32_t>(nLen);
        }
        else
        {
            nCountNull++;
        }
        nLastCode = nCode;
    }
    const int nLength = 1 + nLastCode;

    auto psDict = static_cast<struct ArrowArray *>(
        CPLCalloc(1, sizeof(struct ArrowArray)));
    psChild->dictionary = psDict;

    psDict->release = OGRLayer::ReleaseArray;
    psDict->length = nLength;
    psDict->n_buffers = 3;
    psDict->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    psDict->null_count = nCountNull;
    uint8_t *pabyNull = nullptr;
    if (nCountNull)
    {
        pabyNull = static_cast<uint8_t *>(
            VSI_MALLOC_ALIGNED_AUTO_VERBOSE((nLength + 7) / 8));
        if (pabyNull == nullptr)
        {
            psDict->release(psDict);
            CPLFree(psDict);
            psChild->dictionary = nullptr;
            return false;
        }
        memset(pabyNull, 0xFF, (nLength + 7) / 8);
        psDict->buffers[0] = pabyNull;
    }

    uint32_t *panOffsets = static_cast<uint32_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(uint32_t) * (1 + nLength)));
    if (panOffsets == nullptr)
    {
        psDict->release(psDict);
        CPLFree(psDict);
        psChild->dictionary = nullptr;
        return false;
    }
    psDict->buffers[1] = panOffsets;

    char *pachValues =
        static_cast<char *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nCountChars));
    if (pachValues == nullptr)
    {
        psDict->release(psDict);
        CPLFree(psDict);
        psChild->dictionary = nullptr;
        return false;
    }
    psDict->buffers[2] = pachValues;

    nLastCode = -1;
    uint32_t nOffset = 0;
    for (const OGRCodedValue *psIter = poCodedDomain->GetEnumeration();
         psIter->pszCode; ++psIter)
    {
        if (CPLGetValueType(psIter->pszCode) != CPL_VALUE_INTEGER)
        {
            psDict->release(psDict);
            CPLFree(psDict);
            psChild->dictionary = nullptr;
            return false;
        }
        int nCode = atoi(psIter->pszCode);
        if (nCode <= nLastCode || nCode - nLastCode > 100)
        {
            psDict->release(psDict);
            CPLFree(psDict);
            psChild->dictionary = nullptr;
            return false;
        }
        for (int i = nLastCode + 1; i < nCode; ++i)
        {
            panOffsets[i] = nOffset;
            if (pabyNull)
                pabyNull[i / 8] &= static_cast<uint8_t>(~(1 << (i % 8)));
        }
        panOffsets[nCode] = nOffset;
        if (psIter->pszValue)
        {
            const size_t nLen = strlen(psIter->pszValue);
            memcpy(pachValues + nOffset, psIter->pszValue, nLen);
            nOffset += static_cast<uint32_t>(nLen);
        }
        else if (pabyNull)
        {
            pabyNull[nCode / 8] &= static_cast<uint8_t>(~(1 << (nCode % 8)));
        }
        nLastCode = nCode;
    }
    panOffsets[nLength] = nOffset;

    return true;
}

//! @endcond
