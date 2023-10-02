/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Parts of OGRLayer dealing with Arrow C interface
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2022-2023, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogrsf_frmts.h"
#include "ogr_api.h"
#include "ogr_recordbatch.h"
#include "ograrrowarrayhelper.h"
#include "ogrlayerarrow.h"
#include "ogr_p.h"
#include "ogr_swq.h"
#include "ogr_wkb.h"

#include "cpl_float.h"
#include "cpl_time.h"
#include <cassert>
#include <limits>
#include <set>

/************************************************************************/
/*                            TestBit()                                 */
/************************************************************************/

inline bool TestBit(const uint8_t *pabyData, size_t nIdx)
{
    return (pabyData[nIdx / 8] & (1 << (nIdx % 8))) != 0;
}

/************************************************************************/
/*                            SetBit()                                  */
/************************************************************************/

inline void SetBit(uint8_t *pabyData, size_t nIdx)
{
    pabyData[nIdx / 8] |= (1 << (nIdx % 8));
}

/************************************************************************/
/*                           UnsetBit()                                 */
/************************************************************************/

inline void UnsetBit(uint8_t *pabyData, size_t nIdx)
{
    pabyData[nIdx / 8] &= uint8_t(~(1 << (nIdx % 8)));
}

/************************************************************************/
/*                          DefaultReleaseSchema()                      */
/************************************************************************/

static void OGRLayerDefaultReleaseSchema(struct ArrowSchema *schema)
{
    CPLAssert(schema->release != nullptr);
    if (STARTS_WITH(schema->format, "w:"))
        CPLFree(const_cast<char *>(schema->format));
    CPLFree(const_cast<char *>(schema->name));
    CPLFree(const_cast<char *>(schema->metadata));
    for (int i = 0; i < static_cast<int>(schema->n_children); ++i)
    {
        if (schema->children[i]->release)
        {
            schema->children[i]->release(schema->children[i]);
            CPLFree(schema->children[i]);
        }
    }
    CPLFree(schema->children);
    if (schema->dictionary)
    {
        if (schema->dictionary->release)
        {
            schema->dictionary->release(schema->dictionary);
            CPLFree(schema->dictionary);
        }
    }
    schema->release = nullptr;
}

/** Release a ArrowSchema.
 *
 * To be used by driver implementations that have a custom GetArrowStream()
 * implementation.
 *
 * @param schema Schema to release.
 * @since GDAL 3.6
 */

void OGRLayer::ReleaseSchema(struct ArrowSchema *schema)
{
    OGRLayerDefaultReleaseSchema(schema);
}

/************************************************************************/
/*                        AddDictToSchema()                             */
/************************************************************************/

static void AddDictToSchema(struct ArrowSchema *psChild,
                            const OGRCodedFieldDomain *poCodedDomain)
{
    const OGRCodedValue *psIter = poCodedDomain->GetEnumeration();
    int nLastCode = -1;
    int nCountNull = 0;
    uint32_t nCountChars = 0;
    for (; psIter->pszCode; ++psIter)
    {
        if (CPLGetValueType(psIter->pszCode) != CPL_VALUE_INTEGER)
        {
            return;
        }
        int nCode = atoi(psIter->pszCode);
        if (nCode <= nLastCode || nCode - nLastCode > 100)
        {
            return;
        }
        for (int i = nLastCode + 1; i < nCode; ++i)
        {
            nCountNull++;
        }
        if (psIter->pszValue != nullptr)
        {
            const size_t nLen = strlen(psIter->pszValue);
            if (nLen > std::numeric_limits<uint32_t>::max() - nCountChars)
                return;
            nCountChars += static_cast<uint32_t>(nLen);
        }
        else
            nCountNull++;
        nLastCode = nCode;
    }

    auto psChildDict = static_cast<struct ArrowSchema *>(
        CPLCalloc(1, sizeof(struct ArrowSchema)));
    psChild->dictionary = psChildDict;
    psChildDict->release = OGRLayerDefaultReleaseSchema;
    psChildDict->name = CPLStrdup(poCodedDomain->GetName().c_str());
    psChildDict->format = "u";
    if (nCountNull)
        psChildDict->flags = ARROW_FLAG_NULLABLE;
}

/************************************************************************/
/*                     DefaultGetArrowSchema()                          */
/************************************************************************/

/** Default implementation of the ArrowArrayStream::get_schema() callback.
 *
 * To be used by driver implementations that have a custom GetArrowStream()
 * implementation.
 *
 * @since GDAL 3.6
 */
int OGRLayer::GetArrowSchema(struct ArrowArrayStream *,
                             struct ArrowSchema *out_schema)
{
    const bool bIncludeFID = CPLTestBool(
        m_aosArrowArrayStreamOptions.FetchNameValueDef("INCLUDE_FID", "YES"));
    memset(out_schema, 0, sizeof(*out_schema));
    out_schema->format = "+s";
    out_schema->name = CPLStrdup("");
    out_schema->metadata = nullptr;
    auto poLayerDefn = GetLayerDefn();
    const int nFieldCount = poLayerDefn->GetFieldCount();
    const int nGeomFieldCount = poLayerDefn->GetGeomFieldCount();
    const int nChildren = 1 + nFieldCount + nGeomFieldCount;

    out_schema->children = static_cast<struct ArrowSchema **>(
        CPLCalloc(nChildren, sizeof(struct ArrowSchema *)));
    int iSchemaChild = 0;
    if (bIncludeFID)
    {
        out_schema->children[iSchemaChild] = static_cast<struct ArrowSchema *>(
            CPLCalloc(1, sizeof(struct ArrowSchema)));
        auto psChild = out_schema->children[iSchemaChild];
        ++iSchemaChild;
        psChild->release = OGRLayer::ReleaseSchema;
        const char *pszFIDName = GetFIDColumn();
        psChild->name =
            CPLStrdup((pszFIDName && pszFIDName[0]) ? pszFIDName
                                                    : DEFAULT_ARROW_FID_NAME);
        psChild->format = "l";
    }
    for (int i = 0; i < nFieldCount; ++i)
    {
        const auto poFieldDefn = poLayerDefn->GetFieldDefn(i);
        if (poFieldDefn->IsIgnored())
        {
            continue;
        }

        out_schema->children[iSchemaChild] = static_cast<struct ArrowSchema *>(
            CPLCalloc(1, sizeof(struct ArrowSchema)));
        auto psChild = out_schema->children[iSchemaChild];
        ++iSchemaChild;
        psChild->release = OGRLayer::ReleaseSchema;
        psChild->name = CPLStrdup(poFieldDefn->GetNameRef());
        if (poFieldDefn->IsNullable())
            psChild->flags = ARROW_FLAG_NULLABLE;
        const auto eSubType = poFieldDefn->GetSubType();
        const char *item_format = nullptr;
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            {
                if (eSubType == OFSTBoolean)
                    psChild->format = "b";
                else if (eSubType == OFSTInt16)
                    psChild->format = "s";
                else
                    psChild->format = "i";

                const auto &osDomainName = poFieldDefn->GetDomainName();
                if (!osDomainName.empty())
                {
                    auto poDS = GetDataset();
                    if (poDS)
                    {
                        const auto poFieldDomain =
                            poDS->GetFieldDomain(osDomainName);
                        if (poFieldDomain &&
                            poFieldDomain->GetDomainType() == OFDT_CODED)
                        {
                            const OGRCodedFieldDomain *poCodedDomain =
                                static_cast<const OGRCodedFieldDomain *>(
                                    poFieldDomain);
                            AddDictToSchema(psChild, poCodedDomain);
                        }
                    }
                }

                break;
            }

            case OFTInteger64:
                psChild->format = "l";
                break;

            case OFTReal:
            {
                if (eSubType == OFSTFloat32)
                    psChild->format = "f";
                else
                    psChild->format = "g";
                break;
            }

            case OFTString:
            case OFTWideString:
                psChild->format = "u";
                break;

            case OFTBinary:
            {
                if (poFieldDefn->GetWidth() > 0)
                    psChild->format =
                        CPLStrdup(CPLSPrintf("w:%d", poFieldDefn->GetWidth()));
                else
                    psChild->format = "z";
                break;
            }

            case OFTIntegerList:
            {
                if (eSubType == OFSTBoolean)
                    item_format = "b";
                else if (eSubType == OFSTInt16)
                    item_format = "s";
                else
                    item_format = "i";
                break;
            }

            case OFTInteger64List:
                item_format = "l";
                break;

            case OFTRealList:
            {
                if (eSubType == OFSTFloat32)
                    item_format = "f";
                else
                    item_format = "g";
                break;
            }

            case OFTStringList:
            case OFTWideStringList:
                item_format = "u";
                break;

            case OFTDate:
                psChild->format = "tdD";
                break;

            case OFTTime:
                psChild->format = "ttm";
                break;

            case OFTDateTime:
                psChild->format = "tsm:";
                break;
        }

        if (item_format)
        {
            psChild->format = "+l";
            psChild->n_children = 1;
            psChild->children = static_cast<struct ArrowSchema **>(
                CPLCalloc(1, sizeof(struct ArrowSchema *)));
            psChild->children[0] = static_cast<struct ArrowSchema *>(
                CPLCalloc(1, sizeof(struct ArrowSchema)));
            psChild->children[0]->release = OGRLayer::ReleaseSchema;
            psChild->children[0]->name = CPLStrdup("item");
            psChild->children[0]->format = item_format;
        }
    }
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        const auto poFieldDefn = poLayerDefn->GetGeomFieldDefn(i);
        if (poFieldDefn->IsIgnored())
        {
            continue;
        }

        out_schema->children[iSchemaChild] =
            CreateSchemaForWKBGeometryColumn(poFieldDefn);
        ++iSchemaChild;
    }

    out_schema->n_children = iSchemaChild;
    out_schema->release = OGRLayer::ReleaseSchema;
    return 0;
}

/************************************************************************/
/*                  CreateSchemaForWKBGeometryColumn()                  */
/************************************************************************/

/** Return a ArrowSchema* corresponding to the WKB encoding of a geometry
 * column.
 */

/* static */
struct ArrowSchema *
OGRLayer::CreateSchemaForWKBGeometryColumn(const OGRGeomFieldDefn *poFieldDefn,
                                           const char *pszArrowFormat)
{
    CPLAssert(strcmp(pszArrowFormat, "z") == 0 ||
              strcmp(pszArrowFormat, "Z") == 0);
    auto psSchema = static_cast<struct ArrowSchema *>(
        CPLCalloc(1, sizeof(struct ArrowSchema)));
    psSchema->release = OGRLayer::ReleaseSchema;
    const char *pszGeomFieldName = poFieldDefn->GetNameRef();
    if (pszGeomFieldName[0] == '\0')
        pszGeomFieldName = DEFAULT_ARROW_GEOMETRY_NAME;
    psSchema->name = CPLStrdup(pszGeomFieldName);
    if (poFieldDefn->IsNullable())
        psSchema->flags = ARROW_FLAG_NULLABLE;
    psSchema->format = strcmp(pszArrowFormat, "z") == 0 ? "z" : "Z";
    char *pszMetadata = static_cast<char *>(CPLMalloc(
        sizeof(int32_t) + sizeof(int32_t) + strlen(ARROW_EXTENSION_NAME_KEY) +
        sizeof(int32_t) + strlen(EXTENSION_NAME_WKB)));
    psSchema->metadata = pszMetadata;
    int offsetMD = 0;
    *reinterpret_cast<int32_t *>(pszMetadata + offsetMD) = 1;
    offsetMD += sizeof(int32_t);
    *reinterpret_cast<int32_t *>(pszMetadata + offsetMD) =
        static_cast<int32_t>(strlen(ARROW_EXTENSION_NAME_KEY));
    offsetMD += sizeof(int32_t);
    memcpy(pszMetadata + offsetMD, ARROW_EXTENSION_NAME_KEY,
           strlen(ARROW_EXTENSION_NAME_KEY));
    offsetMD += static_cast<int>(strlen(ARROW_EXTENSION_NAME_KEY));
    *reinterpret_cast<int32_t *>(pszMetadata + offsetMD) =
        static_cast<int32_t>(strlen(EXTENSION_NAME_WKB));
    offsetMD += sizeof(int32_t);
    memcpy(pszMetadata + offsetMD, EXTENSION_NAME_WKB,
           strlen(EXTENSION_NAME_WKB));
    return psSchema;
}

/************************************************************************/
/*                         StaticGetArrowSchema()                       */
/************************************************************************/

/** Default implementation of the ArrowArrayStream::get_schema() callback.
 *
 * To be used by driver implementations that have a custom GetArrowStream()
 * implementation.
 *
 * @since GDAL 3.6
 */
int OGRLayer::StaticGetArrowSchema(struct ArrowArrayStream *stream,
                                   struct ArrowSchema *out_schema)
{
    auto poLayer = static_cast<ArrowArrayStreamPrivateDataSharedDataWrapper *>(
                       stream->private_data)
                       ->poShared->m_poLayer;
    if (poLayer == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Calling get_schema() on a freed OGRLayer is not supported");
        return EINVAL;
    }
    return poLayer->GetArrowSchema(stream, out_schema);
}

/************************************************************************/
/*                         DefaultReleaseArray()                        */
/************************************************************************/

static void OGRLayerDefaultReleaseArray(struct ArrowArray *array)
{
    for (int i = 0; i < static_cast<int>(array->n_buffers); ++i)
        VSIFreeAligned(const_cast<void *>(array->buffers[i]));
    CPLFree(array->buffers);
    for (int i = 0; i < static_cast<int>(array->n_children); ++i)
    {
        if (array->children[i] && array->children[i]->release)
        {
            array->children[i]->release(array->children[i]);
            CPLFree(array->children[i]);
        }
    }
    CPLFree(array->children);
    if (array->dictionary)
    {
        if (array->dictionary->release)
        {
            array->dictionary->release(array->dictionary);
            CPLFree(array->dictionary);
        }
    }
    array->release = nullptr;
}

/** Release a ArrowArray.
 *
 * To be used by driver implementations that have a custom GetArrowStream()
 * implementation.
 *
 * @param array Arrow array to release.
 * @since GDAL 3.6
 */
void OGRLayer::ReleaseArray(struct ArrowArray *array)
{
    OGRLayerDefaultReleaseArray(array);
}

/************************************************************************/
/*                          IsValidField()                              */
/************************************************************************/

static inline bool IsValidField(const OGRField *psRawField)
{
    return (!(psRawField->Set.nMarker1 == OGRUnsetMarker &&
              psRawField->Set.nMarker2 == OGRUnsetMarker &&
              psRawField->Set.nMarker3 == OGRUnsetMarker) &&
            !(psRawField->Set.nMarker1 == OGRNullMarker &&
              psRawField->Set.nMarker2 == OGRNullMarker &&
              psRawField->Set.nMarker3 == OGRNullMarker));
}

/************************************************************************/
/*                    AllocValidityBitmap()                             */
/************************************************************************/

static uint8_t *AllocValidityBitmap(size_t nSize)
{
    auto pabyValidity = static_cast<uint8_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE((nSize + 7) / 8));
    if (pabyValidity)
    {
        // All valid initially
        memset(pabyValidity, 0xFF, (nSize + 7) / 8);
    }
    return pabyValidity;
}

/************************************************************************/
/*                           FillArray()                                */
/************************************************************************/

template <class T, typename TMember>
static bool FillArray(struct ArrowArray *psChild,
                      std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                      const bool bIsNullable, TMember member, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    T *panValues = static_cast<T *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * apoFeatures.size()));
    if (panValues == nullptr)
        return false;
    psChild->buffers[1] = panValues;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            panValues[iFeat] = static_cast<T>((*psRawField).*member);
        }
        else if (bIsNullable)
        {
            panValues[iFeat] = 0;
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
        else
        {
            panValues[iFeat] = 0;
        }
    }
    return true;
}

/************************************************************************/
/*                         FillBoolArray()                              */
/************************************************************************/

template <typename TMember>
static bool FillBoolArray(struct ArrowArray *psChild,
                          std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const bool bIsNullable, TMember member, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    uint8_t *panValues = static_cast<uint8_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE((apoFeatures.size() + 7) / 8));
    if (panValues == nullptr)
        return false;
    memset(panValues, 0, (apoFeatures.size() + 7) / 8);
    psChild->buffers[1] = panValues;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            if ((*psRawField).*member)
                SetBit(panValues, iFeat);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    return true;
}

/************************************************************************/
/*                        FillListArray()                               */
/************************************************************************/

struct GetFromIntegerList
{
    static inline int getCount(const OGRField *psRawField)
    {
        return psRawField->IntegerList.nCount;
    }

    static inline const int *getValues(const OGRField *psRawField)
    {
        return psRawField->IntegerList.paList;
    }
};

struct GetFromInteger64List
{
    static inline int getCount(const OGRField *psRawField)
    {
        return psRawField->Integer64List.nCount;
    }

    static inline const GIntBig *getValues(const OGRField *psRawField)
    {
        return psRawField->Integer64List.paList;
    }
};

struct GetFromRealList
{
    static inline int getCount(const OGRField *psRawField)
    {
        return psRawField->RealList.nCount;
    }

    static inline const double *getValues(const OGRField *psRawField)
    {
        return psRawField->RealList.paList;
    }
};

template <class OffsetType, class T, class GetFromList>
static bool FillListArray(struct ArrowArray *psChild,
                          std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const bool bIsNullable, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    OffsetType *panOffsets =
        static_cast<OffsetType *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            sizeof(OffsetType) * (1 + apoFeatures.size())));
    if (panOffsets == nullptr)
        return false;
    psChild->buffers[1] = panOffsets;

    OffsetType nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        panOffsets[iFeat] = nOffset;
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const unsigned nCount = GetFromList::getCount(psRawField);
            if (nCount > static_cast<size_t>(
                             std::numeric_limits<OffsetType>::max() - nOffset))
                return false;
            nOffset += static_cast<OffsetType>(nCount);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[apoFeatures.size()] = nOffset;

    psChild->n_children = 1;
    psChild->children = static_cast<struct ArrowArray **>(
        CPLCalloc(1, sizeof(struct ArrowArray *)));
    psChild->children[0] = static_cast<struct ArrowArray *>(
        CPLCalloc(1, sizeof(struct ArrowArray)));
    auto psValueChild = psChild->children[0];

    psValueChild->release = OGRLayerDefaultReleaseArray;
    psValueChild->n_buffers = 2;
    psValueChild->buffers =
        static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    psValueChild->length = nOffset;
    T *panValues =
        static_cast<T *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * nOffset));
    if (panValues == nullptr)
        return false;
    psValueChild->buffers[1] = panValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const int nCount = GetFromList::getCount(psRawField);
            const auto paList = GetFromList::getValues(psRawField);
            if (sizeof(*paList) == sizeof(T))
                memcpy(panValues + nOffset, paList, nCount * sizeof(T));
            else
            {
                for (int j = 0; j < nCount; ++j)
                {
                    panValues[nOffset + j] = static_cast<T>(paList[j]);
                }
            }
            nOffset += static_cast<OffsetType>(nCount);
        }
    }

    return true;
}

template <class OffsetType, class GetFromList>
static bool
FillListArrayBool(struct ArrowArray *psChild,
                  std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                  const bool bIsNullable, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    OffsetType *panOffsets =
        static_cast<OffsetType *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            sizeof(OffsetType) * (1 + apoFeatures.size())));
    if (panOffsets == nullptr)
        return false;
    psChild->buffers[1] = panOffsets;

    OffsetType nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        panOffsets[iFeat] = nOffset;
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const unsigned nCount = GetFromList::getCount(psRawField);
            if (nCount > static_cast<size_t>(
                             std::numeric_limits<OffsetType>::max() - nOffset))
                return false;
            nOffset += static_cast<OffsetType>(nCount);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[apoFeatures.size()] = nOffset;

    psChild->n_children = 1;
    psChild->children = static_cast<struct ArrowArray **>(
        CPLCalloc(1, sizeof(struct ArrowArray *)));
    psChild->children[0] = static_cast<struct ArrowArray *>(
        CPLCalloc(1, sizeof(struct ArrowArray)));
    auto psValueChild = psChild->children[0];

    psValueChild->release = OGRLayerDefaultReleaseArray;
    psValueChild->n_buffers = 2;
    psValueChild->buffers =
        static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    psValueChild->length = nOffset;
    uint8_t *panValues = static_cast<uint8_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE((nOffset + 7) / 8));
    if (panValues == nullptr)
        return false;
    memset(panValues, 0, (nOffset + 7) / 8);
    psValueChild->buffers[1] = panValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const int nCount = GetFromList::getCount(psRawField);
            const auto paList = GetFromList::getValues(psRawField);

            for (int j = 0; j < nCount; ++j)
            {
                if (paList[j])
                    SetBit(panValues, nOffset + j);
            }
            nOffset += static_cast<OffsetType>(nCount);
        }
    }

    return true;
}

/************************************************************************/
/*                        FillStringArray()                             */
/************************************************************************/

template <class T>
static bool
FillStringArray(struct ArrowArray *psChild,
                std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                const bool bIsNullable, const int i)
{
    psChild->n_buffers = 3;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    T *panOffsets = static_cast<T *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * (1 + apoFeatures.size())));
    if (panOffsets == nullptr)
        return false;
    psChild->buffers[1] = panOffsets;

    size_t nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        panOffsets[iFeat] = static_cast<T>(nOffset);
        const auto psRawField = apoFeatures[iFeat]->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const size_t nLen = strlen(psRawField->String);
            if (nLen >
                static_cast<size_t>(std::numeric_limits<T>::max()) - nOffset)
                return false;
            nOffset += static_cast<T>(nLen);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[apoFeatures.size()] = static_cast<T>(nOffset);

    char *pachValues =
        static_cast<char *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nOffset));
    if (pachValues == nullptr)
        return false;
    psChild->buffers[2] = pachValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        const size_t nLen =
            static_cast<size_t>(panOffsets[iFeat + 1] - panOffsets[iFeat]);
        if (nLen)
        {
            const auto psRawField = apoFeatures[iFeat]->GetRawFieldRef(i);
            memcpy(pachValues + nOffset, psRawField->String, nLen);
            nOffset += nLen;
        }
    }

    return true;
}

/************************************************************************/
/*                        FillStringListArray()                         */
/************************************************************************/

template <class OffsetType>
static bool
FillStringListArray(struct ArrowArray *psChild,
                    std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                    const bool bIsNullable, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    OffsetType *panOffsets =
        static_cast<OffsetType *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            sizeof(OffsetType) * (1 + apoFeatures.size())));
    if (panOffsets == nullptr)
        return false;
    psChild->buffers[1] = panOffsets;

    OffsetType nStrings = 0;
    OffsetType nCountChars = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        panOffsets[iFeat] = nStrings;
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const int nCount = psRawField->StringList.nCount;
            if (static_cast<size_t>(nCount) >
                static_cast<size_t>(std::numeric_limits<OffsetType>::max() -
                                    nStrings))
                return false;
            for (int j = 0; j < nCount; ++j)
            {
                const size_t nLen = strlen(psRawField->StringList.paList[j]);
                if (nLen >
                    static_cast<size_t>(std::numeric_limits<OffsetType>::max() -
                                        nCountChars))
                    return false;
                nCountChars += static_cast<OffsetType>(nLen);
            }
            nStrings += static_cast<OffsetType>(nCount);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[apoFeatures.size()] = nStrings;

    psChild->n_children = 1;
    psChild->children = static_cast<struct ArrowArray **>(
        CPLCalloc(1, sizeof(struct ArrowArray *)));
    psChild->children[0] = static_cast<struct ArrowArray *>(
        CPLCalloc(1, sizeof(struct ArrowArray)));
    auto psValueChild = psChild->children[0];

    psValueChild->release = OGRLayerDefaultReleaseArray;
    psValueChild->length = nStrings;
    psValueChild->n_buffers = 3;
    psValueChild->buffers =
        static_cast<const void **>(CPLCalloc(3, sizeof(void *)));

    OffsetType *panChildOffsets = static_cast<OffsetType *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(OffsetType) * (1 + nStrings)));
    if (panChildOffsets == nullptr)
        return false;
    psValueChild->buffers[1] = panChildOffsets;

    char *pachValues =
        static_cast<char *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nCountChars));
    if (pachValues == nullptr)
        return false;
    psValueChild->buffers[2] = pachValues;

    nStrings = 0;
    nCountChars = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const int nCount = psRawField->StringList.nCount;
            for (int j = 0; j < nCount; ++j)
            {
                panChildOffsets[nStrings] = nCountChars;
                ++nStrings;
                const size_t nLen = strlen(psRawField->StringList.paList[j]);
                memcpy(pachValues + nCountChars,
                       psRawField->StringList.paList[j], nLen);
                nCountChars += static_cast<OffsetType>(nLen);
            }
        }
    }
    panChildOffsets[nStrings] = nCountChars;

    return true;
}

/************************************************************************/
/*                        FillBinaryArray()                             */
/************************************************************************/

template <class T>
static bool
FillBinaryArray(struct ArrowArray *psChild,
                std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                const bool bIsNullable, const int i)
{
    psChild->n_buffers = 3;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    T *panOffsets = static_cast<T *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * (1 + apoFeatures.size())));
    if (panOffsets == nullptr)
        return false;
    psChild->buffers[1] = panOffsets;

    T nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        panOffsets[iFeat] = nOffset;
        const auto psRawField = apoFeatures[iFeat]->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const size_t nLen = psRawField->Binary.nCount;
            if (nLen >
                static_cast<size_t>(std::numeric_limits<T>::max() - nOffset))
                return false;
            nOffset += static_cast<T>(nLen);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[apoFeatures.size()] = nOffset;

    GByte *pabyValues =
        static_cast<GByte *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nOffset));
    if (pabyValues == nullptr)
        return false;
    psChild->buffers[2] = pabyValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        const size_t nLen =
            static_cast<size_t>(panOffsets[iFeat + 1] - panOffsets[iFeat]);
        if (nLen)
        {
            const auto psRawField = apoFeatures[iFeat]->GetRawFieldRef(i);
            memcpy(pabyValues + nOffset, psRawField->Binary.paData, nLen);
            nOffset += static_cast<T>(nLen);
        }
    }

    return true;
}

/************************************************************************/
/*                     FillFixedWidthBinaryArray()                      */
/************************************************************************/

static bool
FillFixedWidthBinaryArray(struct ArrowArray *psChild,
                          std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const bool bIsNullable, const int nWidth, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;

    if (apoFeatures.size() > std::numeric_limits<size_t>::max() / nWidth)
        return false;
    GByte *pabyValues = static_cast<GByte *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(apoFeatures.size() * nWidth));
    if (pabyValues == nullptr)
        return false;
    psChild->buffers[1] = pabyValues;

    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        const auto psRawField = apoFeatures[iFeat]->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const auto nLen = psRawField->Binary.nCount;
            if (nLen < nWidth)
            {
                memcpy(pabyValues + iFeat * nWidth, psRawField->Binary.paData,
                       nLen);
                memset(pabyValues + iFeat * nWidth + nLen, 0, nWidth - nLen);
            }
            else
            {
                memcpy(pabyValues + iFeat * nWidth, psRawField->Binary.paData,
                       nWidth);
            }
        }
        else
        {
            memset(pabyValues + iFeat * nWidth, 0, nWidth);
            if (bIsNullable)
            {
                ++psChild->null_count;
                if (pabyValidity == nullptr)
                {
                    pabyValidity = AllocValidityBitmap(apoFeatures.size());
                    psChild->buffers[0] = pabyValidity;
                    if (pabyValidity == nullptr)
                        return false;
                }
                UnsetBit(pabyValidity, iFeat);
            }
        }
    }

    return true;
}

/************************************************************************/
/*                      FillWKBGeometryArray()                          */
/************************************************************************/

template <class T>
static bool
FillWKBGeometryArray(struct ArrowArray *psChild,
                     std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                     const OGRGeomFieldDefn *poFieldDefn, const int i)
{
    const bool bIsNullable = CPL_TO_BOOL(poFieldDefn->IsNullable());
    psChild->n_buffers = 3;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    T *panOffsets = static_cast<T *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * (1 + apoFeatures.size())));
    if (panOffsets == nullptr)
        return false;
    psChild->buffers[1] = panOffsets;
    const auto eGeomType = poFieldDefn->GetType();
    auto poEmptyGeom =
        std::unique_ptr<OGRGeometry>(OGRGeometryFactory::createGeometry(
            (eGeomType == wkbNone || wkbFlatten(eGeomType) == wkbUnknown)
                ? wkbGeometryCollection
                : eGeomType));

    size_t nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        panOffsets[iFeat] = static_cast<T>(nOffset);
        const auto poGeom = apoFeatures[iFeat]->GetGeomFieldRef(i);
        if (poGeom != nullptr)
        {
            const size_t nLen = poGeom->WkbSize();
            if (nLen >
                static_cast<size_t>(std::numeric_limits<T>::max()) - nOffset)
                return false;
            nOffset += static_cast<T>(nLen);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
        else if (poEmptyGeom)
        {
            const size_t nLen = poEmptyGeom->WkbSize();
            if (nLen >
                static_cast<size_t>(std::numeric_limits<T>::max()) - nOffset)
                return false;
            nOffset += static_cast<T>(nLen);
        }
    }
    panOffsets[apoFeatures.size()] = static_cast<T>(nOffset);

    GByte *pabyValues =
        static_cast<GByte *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nOffset));
    if (pabyValues == nullptr)
        return false;
    psChild->buffers[2] = pabyValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        const size_t nLen =
            static_cast<size_t>(panOffsets[iFeat + 1] - panOffsets[iFeat]);
        if (nLen)
        {
            const auto poGeom = apoFeatures[iFeat]->GetGeomFieldRef(i);
            poGeom->exportToWkb(wkbNDR, pabyValues + nOffset, wkbVariantIso);
            nOffset += nLen;
        }
        else if (!bIsNullable && poEmptyGeom)
        {
            poEmptyGeom->exportToWkb(wkbNDR, pabyValues + nOffset,
                                     wkbVariantIso);
            nOffset += nLen;
        }
    }

    return true;
}

/************************************************************************/
/*                        FillDateArray()                               */
/************************************************************************/

static bool FillDateArray(struct ArrowArray *psChild,
                          std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const bool bIsNullable, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    int32_t *panValues = static_cast<int32_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(int32_t) * apoFeatures.size()));
    if (panValues == nullptr)
        return false;
    psChild->buffers[1] = panValues;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            struct tm brokenDown;
            memset(&brokenDown, 0, sizeof(brokenDown));
            brokenDown.tm_year = psRawField->Date.Year - 1900;
            brokenDown.tm_mon = psRawField->Date.Month - 1;
            brokenDown.tm_mday = psRawField->Date.Day;
            panValues[iFeat] = static_cast<int>(
                (CPLYMDHMSToUnixTime(&brokenDown) + 36200) / 86400);
        }
        else if (bIsNullable)
        {
            panValues[iFeat] = 0;
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
        else
        {
            panValues[iFeat] = 0;
        }
    }
    return true;
}

/************************************************************************/
/*                        FillDateArray()                               */
/************************************************************************/

static bool FillTimeArray(struct ArrowArray *psChild,
                          std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const bool bIsNullable, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    int32_t *panValues = static_cast<int32_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(int32_t) * apoFeatures.size()));
    if (panValues == nullptr)
        return false;
    psChild->buffers[1] = panValues;
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            panValues[iFeat] =
                psRawField->Date.Hour * 3600000 +
                psRawField->Date.Minute * 60000 +
                static_cast<int>(psRawField->Date.Second * 1000 + 0.5);
        }
        else if (bIsNullable)
        {
            panValues[iFeat] = 0;
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
        else
        {
            panValues[iFeat] = 0;
        }
    }
    return true;
}

/************************************************************************/
/*                       FillDateTimeArray()                            */
/************************************************************************/

static bool
FillDateTimeArray(struct ArrowArray *psChild,
                  std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                  const bool bIsNullable, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    int64_t *panValues = static_cast<int64_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(int64_t) * apoFeatures.size()));
    if (panValues == nullptr)
        return false;
    psChild->buffers[1] = panValues;
    struct tm brokenDown;
    memset(&brokenDown, 0, sizeof(brokenDown));
    for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
    {
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            brokenDown.tm_year = psRawField->Date.Year - 1900;
            brokenDown.tm_mon = psRawField->Date.Month - 1;
            brokenDown.tm_mday = psRawField->Date.Day;
            brokenDown.tm_hour = psRawField->Date.Hour;
            brokenDown.tm_min = psRawField->Date.Minute;
            brokenDown.tm_sec = static_cast<int>(psRawField->Date.Second);
            panValues[iFeat] =
                CPLYMDHMSToUnixTime(&brokenDown) * 1000 +
                (static_cast<int>(psRawField->Date.Second * 1000 + 0.5) % 1000);
        }
        else if (bIsNullable)
        {
            panValues[iFeat] = 0;
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(apoFeatures.size());
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return false;
            }
            UnsetBit(pabyValidity, iFeat);
        }
        else
        {
            panValues[iFeat] = 0;
        }
    }
    return true;
}

/************************************************************************/
/*                          GetNextArrowArray()                         */
/************************************************************************/

/** Default implementation of the ArrowArrayStream::get_next() callback.
 *
 * To be used by driver implementations that have a custom GetArrowStream()
 * implementation.
 *
 * @since GDAL 3.6
 */
int OGRLayer::GetNextArrowArray(struct ArrowArrayStream *,
                                struct ArrowArray *out_array)
{
    const bool bIncludeFID = CPLTestBool(
        m_aosArrowArrayStreamOptions.FetchNameValueDef("INCLUDE_FID", "YES"));
    int nMaxBatchSize = atoi(m_aosArrowArrayStreamOptions.FetchNameValueDef(
        "MAX_FEATURES_IN_BATCH", "65536"));
    if (nMaxBatchSize <= 0)
        nMaxBatchSize = 1;
    if (nMaxBatchSize > INT_MAX - 1)
        nMaxBatchSize = INT_MAX - 1;

    std::vector<std::unique_ptr<OGRFeature>> apoFeatures;
    try
    {
        apoFeatures.reserve(nMaxBatchSize);
    }
    catch (const std::exception &e)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "%s", e.what());
        return ENOMEM;
    }

    memset(out_array, 0, sizeof(*out_array));
    auto poLayerDefn = GetLayerDefn();
    const int nFieldCount = poLayerDefn->GetFieldCount();
    const int nGeomFieldCount = poLayerDefn->GetGeomFieldCount();
    const int nMaxChildren =
        (bIncludeFID ? 1 : 0) + nFieldCount + nGeomFieldCount;
    int iSchemaChild = 0;

    out_array->release = OGRLayerDefaultReleaseArray;

    for (int i = 0; i < nMaxBatchSize; i++)
    {
        auto poFeature = std::unique_ptr<OGRFeature>(GetNextFeature());
        if (!poFeature)
            break;
        apoFeatures.emplace_back(std::move(poFeature));
    }
    if (apoFeatures.empty())
    {
        out_array->release(out_array);
        memset(out_array, 0, sizeof(*out_array));
        return 0;
    }

    out_array->length = apoFeatures.size();
    out_array->null_count = 0;

    out_array->n_children = nMaxChildren;
    out_array->children = static_cast<struct ArrowArray **>(
        CPLCalloc(nMaxChildren, sizeof(struct ArrowArray *)));
    out_array->release = OGRLayerDefaultReleaseArray;
    out_array->n_buffers = 1;
    out_array->buffers =
        static_cast<const void **>(CPLCalloc(1, sizeof(void *)));

    if (bIncludeFID)
    {
        out_array->children[iSchemaChild] = static_cast<struct ArrowArray *>(
            CPLCalloc(1, sizeof(struct ArrowArray)));
        auto psChild = out_array->children[iSchemaChild];
        ++iSchemaChild;
        psChild->release = OGRLayerDefaultReleaseArray;
        psChild->length = apoFeatures.size();
        psChild->n_buffers = 2;
        psChild->buffers =
            static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
        int64_t *panValues =
            static_cast<int64_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                sizeof(int64_t) * apoFeatures.size()));
        if (panValues == nullptr)
            goto error;
        psChild->buffers[1] = panValues;
        for (size_t iFeat = 0; iFeat < apoFeatures.size(); ++iFeat)
        {
            panValues[iFeat] = apoFeatures[iFeat]->GetFID();
        }
    }

    for (int i = 0; i < nFieldCount; ++i)
    {
        const auto poFieldDefn = poLayerDefn->GetFieldDefn(i);
        if (poFieldDefn->IsIgnored())
        {
            continue;
        }

        out_array->children[iSchemaChild] = static_cast<struct ArrowArray *>(
            CPLCalloc(1, sizeof(struct ArrowArray)));
        auto psChild = out_array->children[iSchemaChild];
        ++iSchemaChild;
        psChild->release = OGRLayerDefaultReleaseArray;
        psChild->length = apoFeatures.size();
        const bool bIsNullable = CPL_TO_BOOL(poFieldDefn->IsNullable());
        const auto eSubType = poFieldDefn->GetSubType();
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            {
                if (eSubType == OFSTBoolean)
                {
                    if (!FillBoolArray(psChild, apoFeatures, bIsNullable,
                                       &OGRField::Integer, i))
                        goto error;
                }
                else if (eSubType == OFSTInt16)
                {
                    if (!FillArray<int16_t>(psChild, apoFeatures, bIsNullable,
                                            &OGRField::Integer, i))
                        goto error;
                }
                else
                {
                    if (!FillArray<int32_t>(psChild, apoFeatures, bIsNullable,
                                            &OGRField::Integer, i))
                        goto error;
                }

                const auto &osDomainName = poFieldDefn->GetDomainName();
                if (!osDomainName.empty())
                {
                    auto poDS = GetDataset();
                    if (poDS)
                    {
                        const auto poFieldDomain =
                            poDS->GetFieldDomain(osDomainName);
                        if (poFieldDomain &&
                            poFieldDomain->GetDomainType() == OFDT_CODED)
                        {
                            const OGRCodedFieldDomain *poCodedDomain =
                                static_cast<const OGRCodedFieldDomain *>(
                                    poFieldDomain);
                            OGRArrowArrayHelper::FillDict(psChild,
                                                          poCodedDomain);
                        }
                    }
                }

                break;
            }

            case OFTInteger64:
            {
                if (!FillArray<int64_t>(psChild, apoFeatures, bIsNullable,
                                        &OGRField::Integer64, i))
                    goto error;
                break;
            }

            case OFTReal:
            {
                if (eSubType == OFSTFloat32)
                {
                    if (!FillArray<float>(psChild, apoFeatures, bIsNullable,
                                          &OGRField::Real, i))
                        goto error;
                }
                else
                {
                    if (!FillArray<double>(psChild, apoFeatures, bIsNullable,
                                           &OGRField::Real, i))
                        goto error;
                }
                break;
            }

            case OFTString:
            case OFTWideString:
            {
                if (!FillStringArray<int32_t>(psChild, apoFeatures, bIsNullable,
                                              i))
                    goto error;
                break;
            }

            case OFTBinary:
            {
                const int nWidth = poFieldDefn->GetWidth();
                if (nWidth > 0)
                {
                    if (!FillFixedWidthBinaryArray(psChild, apoFeatures,
                                                   bIsNullable, nWidth, i))
                        goto error;
                }
                else if (!FillBinaryArray<int32_t>(psChild, apoFeatures,
                                                   bIsNullable, i))
                    goto error;
                break;
            }

            case OFTIntegerList:
            {
                if (eSubType == OFSTBoolean)
                {
                    if (!FillListArrayBool<int32_t, GetFromIntegerList>(
                            psChild, apoFeatures, bIsNullable, i))
                        goto error;
                }
                else if (eSubType == OFSTInt16)
                {
                    if (!FillListArray<int32_t, int16_t, GetFromIntegerList>(
                            psChild, apoFeatures, bIsNullable, i))
                        goto error;
                }
                else
                {
                    if (!FillListArray<int32_t, int32_t, GetFromIntegerList>(
                            psChild, apoFeatures, bIsNullable, i))
                        goto error;
                }
                break;
            }

            case OFTInteger64List:
            {
                if (!FillListArray<int32_t, int64_t, GetFromInteger64List>(
                        psChild, apoFeatures, bIsNullable, i))
                    goto error;
                break;
            }

            case OFTRealList:
            {
                if (eSubType == OFSTFloat32)
                {
                    if (!FillListArray<int32_t, float, GetFromRealList>(
                            psChild, apoFeatures, bIsNullable, i))
                        goto error;
                }
                else
                {
                    if (!FillListArray<int32_t, double, GetFromRealList>(
                            psChild, apoFeatures, bIsNullable, i))
                        goto error;
                }
                break;
            }

            case OFTStringList:
            case OFTWideStringList:
            {
                if (!FillStringListArray<int32_t>(psChild, apoFeatures,
                                                  bIsNullable, i))
                    goto error;
                break;
            }

            case OFTDate:
            {
                if (!FillDateArray(psChild, apoFeatures, bIsNullable, i))
                    goto error;
                break;
            }

            case OFTTime:
            {
                if (!FillTimeArray(psChild, apoFeatures, bIsNullable, i))
                    goto error;
                break;
            }

            case OFTDateTime:
            {
                if (!FillDateTimeArray(psChild, apoFeatures, bIsNullable, i))
                    goto error;
                break;
            }
        }
    }
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        const auto poFieldDefn = poLayerDefn->GetGeomFieldDefn(i);
        if (poFieldDefn->IsIgnored())
        {
            continue;
        }

        out_array->children[iSchemaChild] = static_cast<struct ArrowArray *>(
            CPLCalloc(1, sizeof(struct ArrowArray)));
        auto psChild = out_array->children[iSchemaChild];
        ++iSchemaChild;
        psChild->release = OGRLayerDefaultReleaseArray;
        psChild->length = apoFeatures.size();
        if (!FillWKBGeometryArray<int32_t>(psChild, apoFeatures, poFieldDefn,
                                           i))
            goto error;
    }

    out_array->n_children = iSchemaChild;

    return 0;

error:
    out_array->release(out_array);
    memset(out_array, 0, sizeof(*out_array));
    return ENOMEM;
}

/************************************************************************/
/*                       StaticGetNextArrowArray()                      */
/************************************************************************/

/** Default implementation of the ArrowArrayStream::get_next() callback.
 *
 * To be used by driver implementations that have a custom GetArrowStream()
 * implementation.
 *
 * @since GDAL 3.6
 */
int OGRLayer::StaticGetNextArrowArray(struct ArrowArrayStream *stream,
                                      struct ArrowArray *out_array)
{
    auto poLayer = static_cast<ArrowArrayStreamPrivateDataSharedDataWrapper *>(
                       stream->private_data)
                       ->poShared->m_poLayer;
    if (poLayer == nullptr)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Calling get_next() on a freed OGRLayer is not supported");
        return EINVAL;
    }
    return poLayer->GetNextArrowArray(stream, out_array);
}

/************************************************************************/
/*                          GetDataset()                                */
/************************************************************************/

/** Return the dataset associated with this layer.
 *
 * NOTE: that method is implemented in very few drivers, and cannot generally
 * be relied on. It is currently only used by the GetRecordBatchSchema()
 * method to retrieve the field domain associated with a field, to fill the
 * dictionary field of a struct ArrowSchema.
 *
 * @return dataset, or nullptr when unknown.
 * @since GDAL 3.6
 */
GDALDataset *OGRLayer::GetDataset()
{
    return nullptr;
}

/************************************************************************/
/*                            ReleaseStream()                           */
/************************************************************************/

/** Release a ArrowArrayStream.
 *
 * To be used by driver implementations that have a custom GetArrowStream()
 * implementation.
 *
 * @param stream Arrow array stream to release.
 * @since GDAL 3.6
 */
void OGRLayer::ReleaseStream(struct ArrowArrayStream *stream)
{
    assert(stream->release == OGRLayer::ReleaseStream);
    ArrowArrayStreamPrivateDataSharedDataWrapper *poPrivate =
        static_cast<ArrowArrayStreamPrivateDataSharedDataWrapper *>(
            stream->private_data);
    poPrivate->poShared->m_bArrowArrayStreamInProgress = false;
    if (poPrivate->poShared->m_poLayer)
        poPrivate->poShared->m_poLayer->ResetReading();
    delete poPrivate;
    stream->private_data = nullptr;
    stream->release = nullptr;
}

/************************************************************************/
/*                     GetLastErrorArrowArrayStream()                   */
/************************************************************************/

/** Default implementation of the ArrowArrayStream::get_last_error() callback.
 *
 * To be used by driver implementations that have a custom GetArrowStream()
 * implementation.
 *
 * @since GDAL 3.6
 */
const char *OGRLayer::GetLastErrorArrowArrayStream(struct ArrowArrayStream *)
{
    const char *pszLastErrorMsg = CPLGetLastErrorMsg();
    return pszLastErrorMsg[0] != '\0' ? pszLastErrorMsg : nullptr;
}

/************************************************************************/
/*                          GetArrowStream()                            */
/************************************************************************/

/** Get a Arrow C stream.
 *
 * On successful return, and when the stream interfaces is no longer needed, it
must must
 * be freed with out_stream->release(out_stream). Please carefully read
 * https://arrow.apache.org/docs/format/CStreamInterface.html for more details
 * on using Arrow C stream.
 *
 * The method may take into account ignored fields set with SetIgnoredFields()
 * (the default implementation does), and should take into account filters set
with
 * SetSpatialFilter() and SetAttributeFilter(). Note however that specialized
 * implementations may fallback to the default (slower) implementation when
 * filters are set. Drivers that have a specialized implementation should
 * advertise the OLCFastGetArrowStream capability.
 *
 * There are extra precautions to take into account in a OGR context. Unless
 * otherwise specified by a particular driver implementation, the get_schema(),
 * get_next() and get_last_error() function pointers of the ArrowArrayStream
 * structure should no longer be used after the OGRLayer, from which the
 * ArrowArrayStream structure was initialized, has been destroyed (typically at
 * dataset closing). The reason is that those function pointers will typically
 * point to methods of the OGRLayer instance.
 * However, the ArrowSchema and ArrowArray structures filled from those
callbacks
 * can be used and must be released independently from the
 * ArrowArrayStream or the layer.
 *
 * Furthermore, unless otherwise specified by a particular driver
 * implementation, only one ArrowArrayStream can be active at a time on
 * a given layer (that is the last active one must be explicitly released before
 * a next one is asked). Changing filter state, ignored columns, modifying the
schema
 * or using ResetReading()/GetNextFeature() while using a ArrowArrayStream is
 * strongly discouraged and may lead to unexpected results. As a rule of thumb,
 * no OGRLayer methods that affect the state of a layer should be called on a
 * layer, while an ArrowArrayStream on it is active.
 *
 * A potential usage can be:
\code{.cpp}
    struct ArrowArrayStream stream;
    if( !poLayer->GetArrowStream(&stream, nullptr))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "GetArrowStream() failed\n");
        exit(1);
    }
    struct ArrowSchema schema;
    if( stream.get_schema(&stream, &schema) == 0 )
    {
        // Do something useful
        schema.release(schema);
    }
    while( true )
    {
        struct ArrowArray array;
        // Look for an error (get_next() returning a non-zero code), or
        // end of iteration (array.release == nullptr)
        if( stream.get_next(&stream, &array) != 0 ||
            array.release == nullptr )
        {
            break;
        }
        // Do something useful
        array.release(&array);
    }
    stream.release(&stream);
\endcode
 *
 * A full example is available in the
 * <a
href="https://gdal.org/tutorials/vector_api_tut.html#reading-from-ogr-using-the-arrow-c-stream-data-interface">Reading
From OGR using the Arrow C Stream data interface</a> tutorial.
 *
 * Options may be driver specific. The default implementation recognizes the
 * following options:
 * <ul>
 * <li>INCLUDE_FID=YES/NO. Whether to include the FID column. Defaults to
YES.</li>
 * <li>MAX_FEATURES_IN_BATCH=integer. Maximum number of features to retrieve in
 *     a ArrowArray batch. Defaults to 65 536.</li>
 * </ul>
 *
 * The Arrow/Parquet drivers recognize the following option:
 * <ul>
 * <li>GEOMETRY_ENCODING=WKB. To force a fallback to the generic implementation
 *     when the native geometry encoding is not WKB. Otherwise the geometry
 *     will be returned with its native Arrow encoding
 *     (possibly using GeoArrow encoding).</li>
 * </ul>
 *
 * @param out_stream Output stream. Must *not* be NULL. The content of the
 *                  structure does not need to be initialized.
 * @param papszOptions NULL terminated list of key=value options.
 * @return true in case of success.
 * @since GDAL 3.6
 */
bool OGRLayer::GetArrowStream(struct ArrowArrayStream *out_stream,
                              CSLConstList papszOptions)
{
    memset(out_stream, 0, sizeof(*out_stream));
    if (m_poSharedArrowArrayStreamPrivateData &&
        m_poSharedArrowArrayStreamPrivateData->m_bArrowArrayStreamInProgress)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "An arrow Arrow Stream is in progress on that layer. Only "
                 "one at a time is allowed in this implementation.");
        return false;
    }
    m_aosArrowArrayStreamOptions.Assign(CSLDuplicate(papszOptions), true);

    out_stream->get_schema = OGRLayer::StaticGetArrowSchema;
    out_stream->get_next = OGRLayer::StaticGetNextArrowArray;
    out_stream->get_last_error = OGRLayer::GetLastErrorArrowArrayStream;
    out_stream->release = OGRLayer::ReleaseStream;

    if (m_poSharedArrowArrayStreamPrivateData == nullptr)
    {
        m_poSharedArrowArrayStreamPrivateData =
            std::make_shared<ArrowArrayStreamPrivateData>();
        m_poSharedArrowArrayStreamPrivateData->m_poLayer = this;
    }
    m_poSharedArrowArrayStreamPrivateData->m_bArrowArrayStreamInProgress = true;
    auto poPrivateData = new ArrowArrayStreamPrivateDataSharedDataWrapper();
    poPrivateData->poShared = m_poSharedArrowArrayStreamPrivateData;
    out_stream->private_data = poPrivateData;
    return true;
}

/************************************************************************/
/*                       OGR_L_GetArrowStream()                         */
/************************************************************************/

/** Get a Arrow C stream.
 *
 * On successful return, and when the stream interfaces is no longer needed, it
 * must be freed with out_stream->release(out_stream). Please carefully read
 * https://arrow.apache.org/docs/format/CStreamInterface.html for more details
 * on using Arrow C stream.
 *
 * The method may take into account ignored fields set with SetIgnoredFields()
 * (the default implementation does), and should take into account filters set
 * with SetSpatialFilter() and SetAttributeFilter(). Note however that
 * specialized implementations may fallback to the default (slower)
 * implementation when filters are set.
 * Drivers that have a specialized implementation should
 * advertise the OLCFastGetArrowStream capability.
 *
 * There are extra precautions to take into account in a OGR context. Unless
 * otherwise specified by a particular driver implementation, the get_schema(),
 * get_next() and get_last_error() function pointers of the ArrowArrayStream
 * structure should no longer be used after the OGRLayer, from which the
 * ArrowArrayStream structure was initialized, has been destroyed (typically at
 * dataset closing). The reason is that those function pointers will typically
 * point to methods of the OGRLayer instance.
 * However, the ArrowSchema and ArrowArray structures filled from those
 * callbacks can be used and must be released independently from the
 * ArrowArrayStream or the layer.
 *
 * Furthermore, unless otherwise specified by a particular driver
 * implementation, only one ArrowArrayStream can be active at a time on
 * a given layer (that is the last active one must be explicitly released before
 * a next one is asked). Changing filter state, ignored columns, modifying the
 * schema or using ResetReading()/GetNextFeature() while using a
 * ArrowArrayStream is strongly discouraged and may lead to unexpected results.
 * As a rule of thumb, no OGRLayer methods that affect the state of a layer
 * should be called on a layer, while an ArrowArrayStream on it is active.
 *
 * A potential usage can be:
\code{.cpp}
    struct ArrowArrayStream stream;
    if( !OGR_L_GetArrowStream(hLayer, &stream, nullptr))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "OGR_L_GetArrowStream() failed\n");
        exit(1);
    }
    struct ArrowSchema schema;
    if( stream.get_schema(&stream, &schema) == 0 )
    {
        // Do something useful
        schema.release(schema);
    }
    while( true )
    {
        struct ArrowArray array;
        // Look for an error (get_next() returning a non-zero code), or
        // end of iteration (array.release == nullptr)
        if( stream.get_next(&stream, &array) != 0 ||
            array.release == nullptr )
        {
            break;
        }
        // Do something useful
        array.release(&array);
    }
    stream.release(&stream);
\endcode
 *
 * A full example is available in the
 * <a
href="https://gdal.org/tutorials/vector_api_tut.html#reading-from-ogr-using-the-arrow-c-stream-data-interface">Reading
From OGR using the Arrow C Stream data interface</a> tutorial.
 *
 * Options may be driver specific. The default implementation recognizes the
 * following options:
 * <ul>
 * <li>INCLUDE_FID=YES/NO. Whether to include the FID column. Defaults to
YES.</li>
 * <li>MAX_FEATURES_IN_BATCH=integer. Maximum number of features to retrieve in
 *     a ArrowArray batch. Defaults to 65 536.</li>
 * </ul>
 *
 * The Arrow/Parquet drivers recognize the following option:
 * <ul>
 * <li>GEOMETRY_ENCODING=WKB. To force a fallback to the generic implementation
 *     when the native geometry encoding is not WKB. Otherwise the geometry
 *     will be returned with its native Arrow encoding
 *     (possibly using GeoArrow encoding).</li>
 * </ul>
 *
 * @param hLayer Layer
 * @param out_stream Output stream. Must *not* be NULL. The content of the
 *                  structure does not need to be initialized.
 * @param papszOptions NULL terminated list of key=value options.
 * @return true in case of success.
 * @since GDAL 3.6
 */
bool OGR_L_GetArrowStream(OGRLayerH hLayer, struct ArrowArrayStream *out_stream,
                          char **papszOptions)
{
    VALIDATE_POINTER1(hLayer, "OGR_L_GetArrowStream", false);
    VALIDATE_POINTER1(out_stream, "OGR_L_GetArrowStream", false);

    return OGRLayer::FromHandle(hLayer)->GetArrowStream(out_stream,
                                                        papszOptions);
}

/************************************************************************/
/*                     OGRParseArrowMetadata()                          */
/************************************************************************/

std::map<std::string, std::string>
OGRParseArrowMetadata(const char *pabyMetadata)
{
    std::map<std::string, std::string> oMetadata;
    int32_t nKVP;
    memcpy(&nKVP, pabyMetadata, sizeof(int32_t));
    pabyMetadata += sizeof(int32_t);
    for (int i = 0; i < nKVP; ++i)
    {
        int32_t nSizeKey;
        memcpy(&nSizeKey, pabyMetadata, sizeof(int32_t));
        pabyMetadata += sizeof(int32_t);
        std::string osKey;
        osKey.assign(pabyMetadata, nSizeKey);
        pabyMetadata += nSizeKey;

        int32_t nSizeValue;
        memcpy(&nSizeValue, pabyMetadata, sizeof(int32_t));
        pabyMetadata += sizeof(int32_t);
        std::string osValue;
        osValue.assign(pabyMetadata, nSizeValue);
        pabyMetadata += nSizeValue;

        oMetadata[osKey] = osValue;
    }

    return oMetadata;
}

/************************************************************************/
/*                            IsHandledSchema()                         */
/************************************************************************/

static bool IsHandledSchema(bool bTopLevel, const struct ArrowSchema *schema,
                            const std::string &osPrefix, bool bHasAttrQuery,
                            const CPLStringList &aosUsedFields)
{
    if (strcmp(schema->format, "+s") == 0)
    {
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!IsHandledSchema(/* bTopLevel = */ false,
                                 schema->children[static_cast<size_t>(i)],
                                 bTopLevel ? std::string()
                                           : osPrefix + schema->name + ".",
                                 bHasAttrQuery, aosUsedFields))
            {
                return false;
            }
        }
        return true;
    }

    // Lists or maps
    if (strcmp(schema->format, "+l") == 0 ||
        strcmp(schema->format, "+L") == 0 ||
        strncmp(schema->format, "+w:", strlen("+w:")) == 0 ||
        strcmp(schema->format, "+m") == 0)
    {
        if (!IsHandledSchema(/* bTopLevel = */ false, schema->children[0],
                             osPrefix, bHasAttrQuery, aosUsedFields))
        {
            return false;
        }
        // For now, we can't filter on lists or maps
        if (aosUsedFields.FindString((osPrefix + schema->name).c_str()) >= 0)
        {
            CPLDebug("OGR",
                     "Field %s has unhandled format '%s' for an "
                     "attribute to filter on",
                     (osPrefix + schema->name).c_str(), schema->format);
            return false;
        }
        return true;
    }

    const char *const apszHandledFormats[] = {
        "b",    // boolean
        "c",    // int8
        "C",    // uint8
        "s",    // int16
        "S",    // uint16
        "i",    // int32
        "I",    // uint32
        "l",    // int64
        "L",    // uint64
        "e",    // float16
        "f",    // float32
        "g",    // float64,
        "z",    // binary
        "Z",    // large binary
        "u",    // UTF-8 string
        "U",    // large UTF-8 string
        "tdD",  // date32[days]
        "tdm",  // date64[milliseconds]
        "tts",  //time32 [seconds]
        "ttm",  //time32 [milliseconds]
        "ttu",  //time64 [microseconds]
        "ttn",  //time64 [nanoseconds]
    };

    for (const char *pszHandledFormat : apszHandledFormats)
    {
        if (strcmp(schema->format, pszHandledFormat) == 0)
        {
            return true;
        }
    }

    // For now, we can't filter on decimal128/decimal256 fields
    if (bHasAttrQuery && strncmp(schema->format, "d:", 2) == 0)
    {
        if (aosUsedFields.FindString((osPrefix + schema->name).c_str()) >= 0)
        {
            CPLDebug("OGR",
                     "Field %s has unhandled format '%s' for an "
                     "attribute to filter on",
                     (osPrefix + schema->name).c_str(), schema->format);
            return false;
        }
    }

    const char *const apszHandledFormatsPrefix[] = {
        "d:",    // decimal128, decimal256
        "w:",    // fixed width binary
        "tss:",  // timestamp [seconds] with timezone
        "tsm:",  // timestamp [milliseconds] with timezone
        "tsu:",  // timestamp [microseconds] with timezone
        "tsn:",  // timestamp [nanoseconds] with timezone
    };

    for (const char *pszHandledFormat : apszHandledFormatsPrefix)
    {
        if (strncmp(schema->format, pszHandledFormat,
                    strlen(pszHandledFormat)) == 0)
        {
            return true;
        }
    }

    CPLDebug("OGR", "Field %s has unhandled format '%s'",
             (osPrefix + schema->name).c_str(), schema->format);
    return false;
}

/************************************************************************/
/*                  OGRLayer::CanPostFilterArrowArray()                 */
/************************************************************************/

/** Whether the PostFilterArrowArray() can work on the schema to remove
 * rows that aren't selected by the spatial or attribute filter.
 */
bool OGRLayer::CanPostFilterArrowArray(const struct ArrowSchema *schema) const
{
    if (!IsHandledSchema(
            /* bTopLevel=*/true, schema, std::string(),
            m_poAttrQuery != nullptr,
            m_poAttrQuery ? CPLStringList(m_poAttrQuery->GetUsedFields())
                          : CPLStringList()))
    {
        return false;
    }

    if (m_poFilterGeom)
    {
        bool bFound = false;
        const char *pszGeomFieldName =
            const_cast<OGRLayer *>(this)
                ->GetLayerDefn()
                ->GetGeomFieldDefn(m_iGeomFieldFilter)
                ->GetNameRef();
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            const auto fieldSchema = schema->children[i];
            if (strcmp(fieldSchema->name, pszGeomFieldName) == 0)
            {
                if (strcmp(fieldSchema->format, "z") != 0 &&
                    strcmp(fieldSchema->format, "Z") != 0)
                {
                    CPLDebug("OGR", "Geometry field %s has handled format '%s'",
                             fieldSchema->name, fieldSchema->format);
                    return false;
                }

                // Check if ARROW:extension:name = ogc.wkb
                const char *pabyMetadata = fieldSchema->metadata;
                if (!pabyMetadata)
                {
                    CPLDebug(
                        "OGR",
                        "Geometry field %s lacks metadata in its schema field",
                        fieldSchema->name);
                    return false;
                }

                const auto oMetadata = OGRParseArrowMetadata(pabyMetadata);
                auto oIter = oMetadata.find(ARROW_EXTENSION_NAME_KEY);
                if (oIter == oMetadata.end())
                {
                    CPLDebug("OGR",
                             "Geometry field %s lacks "
                             "%s metadata "
                             "in its schema field",
                             fieldSchema->name, ARROW_EXTENSION_NAME_KEY);
                    return false;
                }
                if (oIter->second != EXTENSION_NAME_WKB)
                {
                    CPLDebug("OGR",
                             "Geometry field %s has unexpected "
                             "%s = '%s' metadata "
                             "in its schema field",
                             fieldSchema->name, ARROW_EXTENSION_NAME_KEY,
                             oIter->second.c_str());
                    return false;
                }

                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            CPLDebug("OGR", "Cannot find geometry field %s in schema",
                     pszGeomFieldName);
            return false;
        }
    }

    return true;
}

/************************************************************************/
/*                    CompactValidityBuffer()                           */
/************************************************************************/

static void
CompactValidityBuffer(struct ArrowArray *array, size_t iStart,
                      const std::vector<bool> &abyValidityFromFilters)
{
    if (array->null_count == 0)
        return;
    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());
    uint8_t *pabyValidity =
        static_cast<uint8_t *>(const_cast<void *>(array->buffers[0]));
    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    for (size_t i = 0, j = iStart + nOffset; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            if (TestBit(pabyValidity, i + iStart + nOffset))
                SetBit(pabyValidity, j);
            else
                UnsetBit(pabyValidity, j);

            ++j;
        }
    }
}

/************************************************************************/
/*                       CompactBoolArray()                             */
/************************************************************************/

static void CompactBoolArray(struct ArrowArray *array, size_t iStart,
                             const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(array->n_children == 0);
    CPLAssert(array->n_buffers == 2);
    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());

    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    uint8_t *pabyData =
        static_cast<uint8_t *>(const_cast<void *>(array->buffers[1]));
    size_t j = iStart + nOffset;
    for (size_t i = 0; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            if (TestBit(pabyData, i + iStart + nOffset))
                SetBit(pabyData, j);
            else
                UnsetBit(pabyData, j);

            ++j;
        }
    }

    CompactValidityBuffer(array, iStart, abyValidityFromFilters);
}

/************************************************************************/
/*                       CompactPrimitiveArray()                        */
/************************************************************************/

template <class T>
static void
CompactPrimitiveArray(struct ArrowArray *array, size_t iStart,
                      const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(array->n_children == 0);
    CPLAssert(array->n_buffers == 2);
    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());

    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    T *paData =
        static_cast<T *>(const_cast<void *>(array->buffers[1])) + nOffset;
    size_t j = iStart;
    for (size_t i = 0; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            paData[j] = paData[i + iStart];
            ++j;
        }
    }

    CompactValidityBuffer(array, iStart, abyValidityFromFilters);
}

/************************************************************************/
/*                    CompactStringOrBinaryArray()                      */
/************************************************************************/

template <class OffsetType>
static void
CompactStringOrBinaryArray(struct ArrowArray *array, size_t iStart,
                           const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(array->n_children == 0);
    CPLAssert(array->n_buffers == 3);
    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());

    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    OffsetType *panOffsets =
        static_cast<OffsetType *>(const_cast<void *>(array->buffers[1])) +
        nOffset;
    GByte *pabyData =
        static_cast<GByte *>(const_cast<void *>(array->buffers[2]));
    size_t j = iStart;
    OffsetType nCurOffset = panOffsets[0];
    for (size_t i = 0; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            const auto nStartOffset = panOffsets[i + iStart];
            const auto nEndOffset = panOffsets[i + iStart + 1];
            panOffsets[j] = nCurOffset;
            const auto nSize = static_cast<size_t>(nEndOffset - nStartOffset);
            if (nSize)
            {
                if (nCurOffset < nStartOffset)
                {
                    memmove(pabyData + nCurOffset, pabyData + nStartOffset,
                            nSize);
                }
                nCurOffset += static_cast<OffsetType>(nSize);
            }
            ++j;
        }
    }
    panOffsets[j] = nCurOffset;

    CompactValidityBuffer(array, iStart, abyValidityFromFilters);
}

/************************************************************************/
/*                    CompactFixedWidthArray()                          */
/************************************************************************/

static void
CompactFixedWidthArray(struct ArrowArray *array, int nWidth, size_t iStart,
                       const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(array->n_children == 0);
    CPLAssert(array->n_buffers == 2);
    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());

    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    GByte *pabyData =
        static_cast<GByte *>(const_cast<void *>(array->buffers[1]));
    size_t nStartOffset = (iStart + nOffset) * nWidth;
    size_t nCurOffset = nStartOffset;
    for (size_t i = 0; i < nLength; ++i, nStartOffset += nWidth)
    {
        if (abyValidityFromFilters[i])
        {
            if (nCurOffset < nStartOffset)
            {
                memcpy(pabyData + nCurOffset, pabyData + nStartOffset, nWidth);
            }
            nCurOffset += nWidth;
        }
    }

    CompactValidityBuffer(array, iStart, abyValidityFromFilters);
}

/************************************************************************/
/*                       CompactStructArray()                           */
/************************************************************************/

static bool CompactArray(const struct ArrowSchema *schema,
                         struct ArrowArray *array, size_t iStart,
                         const std::vector<bool> &abyValidityFromFilters);

static bool CompactStructArray(const struct ArrowSchema *schema,
                               struct ArrowArray *array, size_t iStart,
                               const std::vector<bool> &abyValidityFromFilters)
{
    for (int64_t iField = 0; iField < array->n_children; ++iField)
    {
        const auto psChildSchema = schema->children[iField];
        const auto psChildArray = array->children[iField];
        if (!CompactArray(psChildSchema, psChildArray, iStart,
                          abyValidityFromFilters))
            return false;
    }

    CompactValidityBuffer(array, iStart, abyValidityFromFilters);

    return true;
}

/************************************************************************/
/*                       CompactListArray()                             */
/************************************************************************/

template <class OffsetType>
static bool CompactListArray(const struct ArrowSchema *schema,
                             struct ArrowArray *array, size_t iStart,
                             const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());
    CPLAssert(array->n_children == 1);
    CPLAssert(array->n_buffers == 2);

    const auto psChildSchema = schema->children[0];
    const auto psChildArray = array->children[0];

    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    OffsetType *panOffsets =
        static_cast<OffsetType *>(const_cast<void *>(array->buffers[1])) +
        nOffset;

    if (panOffsets[iStart + nLength] > panOffsets[iStart])
    {
        std::vector<bool> abyChildValidity(
            static_cast<size_t>(panOffsets[iStart + nLength] -
                                panOffsets[iStart]),
            true);
        size_t j = iStart;
        OffsetType nCurOffset = panOffsets[iStart];
        for (size_t i = 0; i < nLength; ++i)
        {
            if (abyValidityFromFilters[i])
            {
                const auto nSize = panOffsets[i + 1] - panOffsets[i];
                panOffsets[j] = nCurOffset;
                nCurOffset += nSize;
                ++j;
            }
            else
            {
                const auto nStartOffset = panOffsets[i];
                const auto nEndOffset = panOffsets[i + 1];
                if (nStartOffset != nEndOffset)
                {
                    if (nStartOffset >=
                        panOffsets[iStart] + abyChildValidity.size())
                    {
                        // shouldn't happen in sane arrays...
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "nStartOffset >= panOffsets[iStart] + "
                                 "abyChildValidity.size()");
                        return false;
                    }
                    // nEndOffset might be equal to abyChildValidity.size()
                    if (nEndOffset >
                        panOffsets[iStart] + abyChildValidity.size())
                    {
                        // shouldn't happen in sane arrays...
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "nEndOffset > panOffsets[iStart] + "
                                 "abyChildValidity.size()");
                        return false;
                    }
                    for (auto k = nStartOffset - panOffsets[iStart];
                         k < nEndOffset - panOffsets[iStart]; ++k)
                        abyChildValidity[static_cast<size_t>(k)] = false;
                }
            }
        }
        panOffsets[j] = nCurOffset;

        if (!CompactArray(psChildSchema, psChildArray,
                          static_cast<size_t>(panOffsets[iStart]),
                          abyChildValidity))
            return false;
    }

    CompactValidityBuffer(array, iStart, abyValidityFromFilters);

    return true;
}

/************************************************************************/
/*                     CompactFixedSizeListArray()                      */
/************************************************************************/

static bool
CompactFixedSizeListArray(const struct ArrowSchema *schema,
                          struct ArrowArray *array, size_t N, size_t iStart,
                          const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());
    CPLAssert(array->n_children == 1);

    const auto psChildSchema = schema->children[0];
    const auto psChildArray = array->children[0];

    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    std::vector<bool> abyChildValidity(N * nLength, true);
    for (size_t i = 0; i < nLength; ++i)
    {
        if (!abyValidityFromFilters[i])
        {
            const size_t nStartOffset = i * N;
            const size_t nEndOffset = (i + 1) * N;
            for (size_t k = nStartOffset; k < nEndOffset; ++k)
                abyChildValidity[k] = false;
        }
    }

    if (!CompactArray(psChildSchema, psChildArray, (iStart + nOffset) * N,
                      abyChildValidity))
        return false;

    CompactValidityBuffer(array, iStart, abyValidityFromFilters);

    return true;
}

/************************************************************************/
/*                       CompactListArray()                             */
/************************************************************************/

static bool CompactMapArray(const struct ArrowSchema *schema,
                            struct ArrowArray *array, size_t iStart,
                            const std::vector<bool> &abyValidityFromFilters)
{
    return CompactListArray<uint32_t>(schema, array, iStart,
                                      abyValidityFromFilters);
}

/************************************************************************/
/*                           CompactArray()                             */
/************************************************************************/

static bool CompactArray(const struct ArrowSchema *schema,
                         struct ArrowArray *array, size_t iStart,
                         const std::vector<bool> &abyValidityFromFilters)
{
    const char *format = schema->format;
    if (strcmp(format, "+s") == 0)
    {
        if (!CompactStructArray(schema, array, iStart, abyValidityFromFilters))
            return false;
    }
    else if (strcmp(format, "+l") == 0)
    {
        if (!CompactListArray<uint32_t>(schema, array, iStart,
                                        abyValidityFromFilters))
            return false;
    }
    else if (strcmp(format, "+L") == 0)
    {
        if (!CompactListArray<uint64_t>(schema, array, iStart,
                                        abyValidityFromFilters))
            return false;
    }
    else if (strcmp(format, "+m") == 0)
    {
        // Map
        if (!CompactMapArray(schema, array, iStart, abyValidityFromFilters))
            return false;
    }
    else if (strncmp(format, "+w:", strlen("+w:")) == 0)
    {
        const int N = atoi(format + strlen("+w:"));
        if (N <= 0)
            return false;
        if (!CompactFixedSizeListArray(schema, array, static_cast<size_t>(N),
                                       iStart, abyValidityFromFilters))
            return false;
    }
    else if (strcmp(format, "b") == 0)
    {
        CompactBoolArray(array, iStart, abyValidityFromFilters);
    }
    else if (strcmp(format, "c") == 0 || strcmp(format, "C") == 0)
    {
        CompactPrimitiveArray<uint8_t>(array, iStart, abyValidityFromFilters);
    }
    else if (strcmp(format, "s") == 0 || strcmp(format, "S") == 0 ||
             strcmp(format, "e") == 0)
    {
        CompactPrimitiveArray<uint16_t>(array, iStart, abyValidityFromFilters);
    }
    else if (strcmp(format, "i") == 0 || strcmp(format, "I") == 0 ||
             strcmp(format, "f") == 0 || strcmp(format, "tdD") == 0 ||
             strcmp(format, "tts") == 0 || strcmp(format, "ttm") == 0)
    {
        CompactPrimitiveArray<uint32_t>(array, iStart, abyValidityFromFilters);
    }
    else if (strcmp(format, "l") == 0 || strcmp(format, "L") == 0 ||
             strcmp(format, "g") == 0 || strcmp(format, "tdm") == 0 ||
             strcmp(format, "ttu") == 0 || strcmp(format, "ttn") == 0 ||
             strncmp(format, "ts", 2) == 0)
    {
        CompactPrimitiveArray<uint64_t>(array, iStart, abyValidityFromFilters);
    }
    else if (strcmp(format, "z") == 0 || strcmp(format, "u") == 0)
    {
        CompactStringOrBinaryArray<uint32_t>(array, iStart,
                                             abyValidityFromFilters);
    }
    else if (strcmp(format, "Z") == 0 || strcmp(format, "U") == 0)
    {
        CompactStringOrBinaryArray<uint64_t>(array, iStart,
                                             abyValidityFromFilters);
    }
    else if (strncmp(format, "w:", 2) == 0)
    {
        const int nWidth = atoi(format + 2);
        CompactFixedWidthArray(array, nWidth, iStart, abyValidityFromFilters);
    }
    else if (strncmp(format, "d:", 2) == 0)
    {
        // d:19,10     ==> decimal128 [precision 19, scale 10]
        // d:19,10,NNN ==> decimal bitwidth = NNN [precision 19, scale 10]
        int nWidth = 128 / 8;  // 128 bit
        const char *pszComma = strchr(format + 2, ',');
        if (pszComma)
        {
            pszComma = strchr(pszComma + 1, ',');
            if (pszComma)
            {
                nWidth = atoi(pszComma + 1);
                if ((nWidth % 8) != 0)
                {
                    // shouldn't happen for well-format schemas
                    nWidth = 0;
                }
                else
                    nWidth /= 8;
            }
        }
        else
        {
            // shouldn't happen for well-format schemas
            nWidth = 0;
        }
        if (nWidth == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected error in PostFilterArrowArray(): unhandled "
                     "field format: %s",
                     format);

            return false;
        }
        CompactFixedWidthArray(array, nWidth, iStart, abyValidityFromFilters);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unexpected error in CompactArray(): unhandled "
                 "field format: %s",
                 format);
        return false;
    }

    return true;
}

/************************************************************************/
/*                  FillValidityArrayFromWKBArray()                     */
/************************************************************************/

template <class OffsetType>
static size_t
FillValidityArrayFromWKBArray(struct ArrowArray *array, const OGRLayer *poLayer,
                              std::vector<bool> &abyValidityFromFilters)
{
    const size_t nLength = static_cast<size_t>(array->length);
    const uint8_t *pabyValidity =
        array->null_count == 0
            ? nullptr
            : static_cast<const uint8_t *>(array->buffers[0]);
    const size_t nOffset = static_cast<size_t>(array->offset);
    const OffsetType *panOffsets =
        static_cast<const OffsetType *>(array->buffers[1]) + nOffset;
    const GByte *pabyData = static_cast<const GByte *>(array->buffers[2]);
    OGREnvelope sEnvelope;
    abyValidityFromFilters.resize(nLength);
    size_t nCountIntersecting = 0;
    for (size_t i = 0; i < nLength; ++i)
    {
        if (!pabyValidity || TestBit(pabyValidity, i + nOffset))
        {
            const GByte *pabyWKB = pabyData + panOffsets[i];
            const size_t nWKBSize =
                static_cast<size_t>(panOffsets[i + 1] - panOffsets[i]);
            if (poLayer->FilterWKBGeometry(pabyWKB, nWKBSize,
                                           /* bEnvelopeAlreadySet=*/false,
                                           sEnvelope))
            {
                abyValidityFromFilters[i] = true;
                nCountIntersecting++;
            }
        }
    }
    return nCountIntersecting;
}

/************************************************************************/
/*               ArrowTimestampToOGRDateTime()                          */
/************************************************************************/

static void ArrowTimestampToOGRDateTime(int64_t nTimestamp,
                                        int nInvFactorToSecond,
                                        const char *pszTZ, OGRFeature &oFeature,
                                        int iField)
{
    double floatingPart = 0;
    if (nInvFactorToSecond)
    {
        floatingPart =
            (nTimestamp % nInvFactorToSecond) / double(nInvFactorToSecond);
        nTimestamp /= nInvFactorToSecond;
    }
    int nTZFlag = 0;
    const size_t nTZLen = strlen(pszTZ);
    if ((nTZLen == 3 && strcmp(pszTZ, "UTC") == 0) ||
        (nTZLen == 7 && strcmp(pszTZ, "Etc/UTC") == 7))
    {
        nTZFlag = 100;
    }
    else if (nTZLen == 6 && (pszTZ[0] == '+' || pszTZ[0] == '-') &&
             pszTZ[3] == ':')
    {
        int nTZHour = atoi(pszTZ + 1);
        int nTZMin = atoi(pszTZ + 4);
        if (nTZHour >= 0 && nTZHour <= 14 && nTZMin >= 0 && nTZMin < 60 &&
            (nTZMin % 15) == 0)
        {
            nTZFlag = (nTZHour * 4) + (nTZMin / 15);
            if (pszTZ[0] == '+')
            {
                nTZFlag = 100 + nTZFlag;
                nTimestamp += nTZHour * 3600 + nTZMin * 60;
            }
            else
            {
                nTZFlag = 100 - nTZFlag;
                nTimestamp -= nTZHour * 3600 + nTZMin * 60;
            }
        }
    }
    struct tm dt;
    CPLUnixTimeToYMDHMS(nTimestamp, &dt);
    oFeature.SetField(iField, dt.tm_year + 1900, dt.tm_mon + 1, dt.tm_mday,
                      dt.tm_hour, dt.tm_min,
                      static_cast<float>(dt.tm_sec + floatingPart), nTZFlag);
}

/************************************************************************/
/*                 BuildMapFieldNameToArrowPath()                       */
/************************************************************************/

static void
BuildMapFieldNameToArrowPath(const struct ArrowSchema *schema,
                             std::map<std::string, std::vector<int>> &oMap,
                             const std::string &osPrefix,
                             std::vector<int> &anArrowPath)
{
    for (int64_t i = 0; i < schema->n_children; ++i)
    {
        auto psChild = schema->children[i];
        anArrowPath.push_back(static_cast<int>(i));
        if (strcmp(psChild->format, "s+") == 0)
        {
            std::string osNewPrefix(osPrefix);
            osNewPrefix += psChild->name;
            osNewPrefix += ".";
            BuildMapFieldNameToArrowPath(psChild, oMap, osNewPrefix,
                                         anArrowPath);
        }
        else
        {
            oMap[osPrefix + psChild->name] = anArrowPath;
        }
        anArrowPath.pop_back();
    }
}

/************************************************************************/
/*                      FillFieldList()                                 */
/************************************************************************/

template <typename ListOffsetType, typename ArrowType,
          typename OGRType = ArrowType>
inline static void FillFieldList(const struct ArrowArray *array,
                                 int iOGRFieldIdx, size_t nOffsettedIndex,
                                 const struct ArrowArray *childArray,
                                 OGRFeature &oFeature)
{
    const auto panOffsets =
        static_cast<const ListOffsetType *>(array->buffers[1]) +
        nOffsettedIndex;
    std::vector<OGRType> aValues;
    const auto *paValues =
        static_cast<const ArrowType *>(childArray->buffers[1]);
    for (size_t i = static_cast<size_t>(panOffsets[0]);
         i < static_cast<size_t>(panOffsets[1]); ++i)
    {
        aValues.push_back(static_cast<OGRType>(paValues[i]));
    }
    oFeature.SetField(iOGRFieldIdx, static_cast<int>(aValues.size()),
                      aValues.data());
}

/************************************************************************/
/*                      FillFieldListFromBool()                         */
/************************************************************************/

template <typename ListOffsetType>
inline static void
FillFieldListFromBool(const struct ArrowArray *array, int iOGRFieldIdx,
                      size_t nOffsettedIndex,
                      const struct ArrowArray *childArray, OGRFeature &oFeature)
{
    const auto panOffsets =
        static_cast<const ListOffsetType *>(array->buffers[1]) +
        nOffsettedIndex;
    std::vector<int> aValues;
    const auto *paValues = static_cast<const uint8_t *>(childArray->buffers[1]);
    for (size_t i = static_cast<size_t>(panOffsets[0]);
         i < static_cast<size_t>(panOffsets[1]); ++i)
    {
        aValues.push_back(TestBit(paValues, i) ? 1 : 0);
    }
    oFeature.SetField(iOGRFieldIdx, static_cast<int>(aValues.size()),
                      aValues.data());
}

/************************************************************************/
/*                    FillFieldListFromHalfFloat()                      */
/************************************************************************/

template <typename ListOffsetType>
inline static void FillFieldListFromHalfFloat(
    const struct ArrowArray *array, int iOGRFieldIdx, size_t nOffsettedIndex,
    const struct ArrowArray *childArray, OGRFeature &oFeature)
{
    const auto panOffsets =
        static_cast<const ListOffsetType *>(array->buffers[1]) +
        nOffsettedIndex;
    std::vector<double> aValues;
    const auto *paValues =
        static_cast<const uint16_t *>(childArray->buffers[1]);
    for (size_t i = static_cast<size_t>(panOffsets[0]);
         i < static_cast<size_t>(panOffsets[1]); ++i)
    {
        const auto nFloat16AsUInt32 = CPLHalfToFloat(paValues[i]);
        float f;
        memcpy(&f, &nFloat16AsUInt32, sizeof(f));
        aValues.push_back(f);
    }
    oFeature.SetField(iOGRFieldIdx, static_cast<int>(aValues.size()),
                      aValues.data());
}

/************************************************************************/
/*                    FillFieldListFromString()                         */
/************************************************************************/

template <typename ListOffsetType, typename StringOffsetType>
inline static void FillFieldListFromString(const struct ArrowArray *array,
                                           int iOGRFieldIdx,
                                           size_t nOffsettedIndex,
                                           const struct ArrowArray *childArray,
                                           OGRFeature &oFeature)
{
    const auto panOffsets =
        static_cast<const ListOffsetType *>(array->buffers[1]) +
        nOffsettedIndex;
    CPLStringList aosVals;
    const auto panSubOffsets =
        static_cast<const StringOffsetType *>(childArray->buffers[1]);
    const char *pszValues = static_cast<const char *>(childArray->buffers[2]);
    std::string osTmp;
    for (size_t i = static_cast<size_t>(panOffsets[0]);
         i < static_cast<size_t>(panOffsets[1]); ++i)
    {
        osTmp.assign(
            pszValues + panSubOffsets[i],
            static_cast<size_t>(panSubOffsets[i + 1] - panSubOffsets[i]));
        aosVals.AddString(osTmp.c_str());
    }
    oFeature.SetField(iOGRFieldIdx, aosVals.List());
}

/************************************************************************/
/*                         FillFieldFixedSizeList()                     */
/************************************************************************/

template <typename ArrowType, typename OGRType = ArrowType>
inline static void FillFieldFixedSizeList(
    const struct ArrowArray *, int iOGRFieldIdx, size_t nOffsettedIndex,
    const int nItems, const struct ArrowArray *childArray, OGRFeature &oFeature)
{
    std::vector<OGRType> aValues;
    const auto *paValues =
        static_cast<const ArrowType *>(childArray->buffers[1]) +
        childArray->offset + nOffsettedIndex * nItems;
    for (int i = 0; i < nItems; ++i)
    {
        aValues.push_back(static_cast<OGRType>(paValues[i]));
    }
    oFeature.SetField(iOGRFieldIdx, static_cast<int>(aValues.size()),
                      aValues.data());
}

/************************************************************************/
/*                    FillFieldFixedSizeListString()                    */
/************************************************************************/

template <typename StringOffsetType>
inline static void FillFieldFixedSizeListString(
    const struct ArrowArray *, int iOGRFieldIdx, size_t nOffsettedIndex,
    const int nItems, const struct ArrowArray *childArray, OGRFeature &oFeature)
{
    CPLStringList aosVals;
    const auto panSubOffsets =
        static_cast<const StringOffsetType *>(childArray->buffers[1]) +
        childArray->offset + nOffsettedIndex * nItems;
    const char *pszValues = static_cast<const char *>(childArray->buffers[2]);
    std::string osTmp;
    for (int i = 0; i < nItems; ++i)
    {
        osTmp.assign(
            pszValues + panSubOffsets[i],
            static_cast<size_t>(panSubOffsets[i + 1] - panSubOffsets[i]));
        aosVals.AddString(osTmp.c_str());
    }
    oFeature.SetField(iOGRFieldIdx, aosVals.List());
}

/************************************************************************/
/*                      SetFieldForOtherFormats()                       */
/************************************************************************/

static bool SetFieldForOtherFormats(OGRFeature &oFeature,
                                    const int iOGRFieldIndex,
                                    const size_t nOffsettedIndex,
                                    const struct ArrowSchema *schema,
                                    const struct ArrowArray *array)
{
    const char *format = schema->format;
    if (format[0] == 'e')
    {
        // half-float
        const auto nFloat16AsUInt16 =
            static_cast<const uint16_t *>(array->buffers[1])[nOffsettedIndex];
        const auto nFloat16AsUInt32 = CPLHalfToFloat(nFloat16AsUInt16);
        float f;
        memcpy(&f, &nFloat16AsUInt32, sizeof(f));
        oFeature.SetField(iOGRFieldIndex, f);
    }

    else if (format[0] == 'w' && format[1] == ':')
    {
        // Fixed with binary
        const int nWidth = atoi(format + 2);
        oFeature.SetField(iOGRFieldIndex, nWidth,
                          static_cast<const GByte *>(array->buffers[1]) +
                              nOffsettedIndex * nWidth);
    }
    else if (format[0] == 't' && format[1] == 'd' &&
             format[2] == 'D')  // strcmp(format, "tdD") == 0
    {
        // date32[days]
        // number of days since Epoch
        int64_t timestamp = static_cast<int64_t>(static_cast<const uint32_t *>(
                                array->buffers[1])[nOffsettedIndex]) *
                            3600 * 24;
        struct tm dt;
        CPLUnixTimeToYMDHMS(timestamp, &dt);
        oFeature.SetField(iOGRFieldIndex, dt.tm_year + 1900, dt.tm_mon + 1,
                          dt.tm_mday, 0, 0, 0);
        return true;
    }
    else if (format[0] == 't' && format[1] == 'd' &&
             format[2] == 'm')  // strcmp(format, "tdm") == 0
    {
        // date64[milliseconds]
        // number of milliseconds since Epoch
        int64_t timestamp =
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex] /
            1000;
        struct tm dt;
        CPLUnixTimeToYMDHMS(timestamp, &dt);
        oFeature.SetField(iOGRFieldIndex, dt.tm_year + 1900, dt.tm_mon + 1,
                          dt.tm_mday, 0, 0, 0);
    }
    else if (format[0] == 't' && format[1] == 't' &&
             format[2] == 's')  // strcmp(format, "tts") == 0
    {
        // time32 [seconds]
        int32_t value =
            static_cast<const int32_t *>(array->buffers[1])[nOffsettedIndex];
        const int nHour = value / 3600;
        const int nMinute = (value / 60) % 60;
        const int nSecond = value % 60;
        oFeature.SetField(iOGRFieldIndex, 0, 0, 0, nHour, nMinute,
                          static_cast<float>(nSecond));
    }
    else if (format[0] == 't' && format[1] == 't' &&
             format[2] == 'm')  // strcmp(format, "ttm") == 0
    {
        // time32 [milliseconds]
        int32_t value =
            static_cast<const int32_t *>(array->buffers[1])[nOffsettedIndex];
        double floatingPart = (value % 1000) / 1e3;
        value /= 1000;
        const int nHour = value / 3600;
        const int nMinute = (value / 60) % 60;
        const int nSecond = value % 60;
        oFeature.SetField(iOGRFieldIndex, 0, 0, 0, nHour, nMinute,
                          static_cast<float>(nSecond + floatingPart));
    }
    else if (format[0] == 't' && format[1] == 't' &&
             (format[2] == 'u' ||  // time64 [microseconds]
              format[2] == 'n'))   // time64 [nanoseconds]
    {
        oFeature.SetField(iOGRFieldIndex,
                          static_cast<GIntBig>(static_cast<const int64_t *>(
                              array->buffers[1])[nOffsettedIndex]));
    }
    else if (format[0] == 't' && format[1] == 's' && format[2] == 's' &&
             format[3] == ':')  // STARTS_WITH(format, "tss:")
    {
        // timestamp [seconds] with timezone
        ArrowTimestampToOGRDateTime(
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex], 1,
            format + strlen("tss:"), oFeature, iOGRFieldIndex);
    }
    else if (format[0] == 't' && format[1] == 's' && format[2] == 'm' &&
             format[3] == ':')  // STARTS_WITH(format, "tsm:"))
    {
        //  timestamp [milliseconds] with timezone
        ArrowTimestampToOGRDateTime(
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex],
            1000, format + strlen("tsm:"), oFeature, iOGRFieldIndex);
    }
    else if (format[0] == 't' && format[1] == 's' && format[2] == 'u' &&
             format[3] == ':')  // STARTS_WITH(format, "tsu:"))
    {
        //  timestamp [microseconds] with timezone
        ArrowTimestampToOGRDateTime(
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex],
            1000 * 1000, format + strlen("tsu:"), oFeature, iOGRFieldIndex);
    }
    else if (format[0] == 't' && format[1] == 's' && format[2] == 'n' &&
             format[3] == ':')  // STARTS_WITH(format, "tsn:"))
    {
        //  timestamp [nanoseconds] with timezone
        ArrowTimestampToOGRDateTime(
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex],
            1000 * 1000 * 1000, format + strlen("tsn:"), oFeature,
            iOGRFieldIndex);
    }
    else if (format[0] == '+' && format[1] == 'w' &&
             format[2] ==
                 ':')  // STARTS_WITH(format, "+w:")  // Fixed-size list
    {
        const int nItems = atoi(format + strlen("+w:"));
        const auto childArray = array->children[0];
        const char *childFormat = schema->children[0]->format;
        if (childFormat[0] == 'b')  // Boolean
        {
            std::vector<int> aValues;
            const auto *paValues =
                static_cast<const uint8_t *>(childArray->buffers[1]);
            for (int i = 0; i < nItems; ++i)
            {
                aValues.push_back(
                    TestBit(paValues,
                            static_cast<size_t>(childArray->offset +
                                                nOffsettedIndex * nItems + i))
                        ? 1
                        : 0);
            }
            oFeature.SetField(iOGRFieldIndex, static_cast<int>(aValues.size()),
                              aValues.data());
        }
        else if (childFormat[0] == 'c')  // Int8
        {
            FillFieldFixedSizeList<int8_t, int>(array, iOGRFieldIndex,
                                                nOffsettedIndex, nItems,
                                                childArray, oFeature);
        }
        else if (childFormat[0] == 'C')  // UInt8
        {
            FillFieldFixedSizeList<uint8_t, int>(array, iOGRFieldIndex,
                                                 nOffsettedIndex, nItems,
                                                 childArray, oFeature);
        }
        else if (childFormat[0] == 's')  // Int16
        {
            FillFieldFixedSizeList<int16_t, int>(array, iOGRFieldIndex,
                                                 nOffsettedIndex, nItems,
                                                 childArray, oFeature);
        }
        else if (childFormat[0] == 'S')  // UInt16
        {
            FillFieldFixedSizeList<uint16_t, int>(array, iOGRFieldIndex,
                                                  nOffsettedIndex, nItems,
                                                  childArray, oFeature);
        }
        else if (childFormat[0] == 'i')  // Int32
        {
            FillFieldFixedSizeList<int32_t, int>(array, iOGRFieldIndex,
                                                 nOffsettedIndex, nItems,
                                                 childArray, oFeature);
        }
        else if (childFormat[0] == 'I')  // UInt32
        {
            FillFieldFixedSizeList<uint32_t, GIntBig>(array, iOGRFieldIndex,
                                                      nOffsettedIndex, nItems,
                                                      childArray, oFeature);
        }
        else if (childFormat[0] == 'l')  // Int64
        {
            FillFieldFixedSizeList<int64_t, GIntBig>(array, iOGRFieldIndex,
                                                     nOffsettedIndex, nItems,
                                                     childArray, oFeature);
        }
        else if (childFormat[0] == 'L')  // UInt64 (lossy conversion)
        {
            FillFieldFixedSizeList<uint64_t, double>(array, iOGRFieldIndex,
                                                     nOffsettedIndex, nItems,
                                                     childArray, oFeature);
        }
        else if (childFormat[0] == 'e')  // float16
        {
            std::vector<double> aValues;
            const auto *paValues =
                static_cast<const uint16_t *>(childArray->buffers[1]) +
                childArray->offset + nOffsettedIndex * nItems;
            for (int i = 0; i < nItems; ++i)
            {
                const auto nFloat16AsUInt16 = paValues[i];
                const auto nFloat16AsUInt32 = CPLHalfToFloat(nFloat16AsUInt16);
                float f;
                memcpy(&f, &nFloat16AsUInt32, sizeof(f));
                aValues.push_back(f);
            }
            oFeature.SetField(iOGRFieldIndex, static_cast<int>(aValues.size()),
                              aValues.data());
        }
        else if (childFormat[0] == 'f')  // float32
        {
            FillFieldFixedSizeList<float, double>(array, iOGRFieldIndex,
                                                  nOffsettedIndex, nItems,
                                                  childArray, oFeature);
        }
        else if (childFormat[0] == 'g')  // float64
        {
            FillFieldFixedSizeList<double, double>(array, iOGRFieldIndex,
                                                   nOffsettedIndex, nItems,
                                                   childArray, oFeature);
        }
        else if (childFormat[0] == 'u')  // string
        {
            FillFieldFixedSizeListString<uint32_t>(array, iOGRFieldIndex,
                                                   nOffsettedIndex, nItems,
                                                   childArray, oFeature);
        }
        else if (childFormat[0] == 'U')  // large string
        {
            FillFieldFixedSizeListString<uint64_t>(array, iOGRFieldIndex,
                                                   nOffsettedIndex, nItems,
                                                   childArray, oFeature);
        }
    }
    else if (format[0] == '+' &&
             (format[1] == 'l' || format[1] == 'L'))  // List
    {
        const auto childArray = array->children[0];
        const char *childFormat = schema->children[0]->format;
        if (childFormat[0] == 'b')  // Boolean
        {
            if (format[1] == 'l')
                FillFieldListFromBool<uint32_t>(array, iOGRFieldIndex,
                                                nOffsettedIndex, childArray,
                                                oFeature);
            else
                FillFieldListFromBool<uint64_t>(array, iOGRFieldIndex,
                                                nOffsettedIndex, childArray,
                                                oFeature);
        }
        else if (childFormat[0] == 'c')  // Int8
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, int8_t, int>(array, iOGRFieldIndex,
                                                     nOffsettedIndex,
                                                     childArray, oFeature);
            else
                FillFieldList<uint64_t, int8_t, int>(array, iOGRFieldIndex,
                                                     nOffsettedIndex,
                                                     childArray, oFeature);
        }
        else if (childFormat[0] == 'C')  // UInt8
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, uint8_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
            else
                FillFieldList<uint64_t, uint8_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
        }
        else if (childFormat[0] == 's')  // Int16
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, int16_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
            else
                FillFieldList<uint64_t, int16_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
        }
        else if (childFormat[0] == 'S')  // UInt16
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, uint16_t, int>(array, iOGRFieldIndex,
                                                       nOffsettedIndex,
                                                       childArray, oFeature);
            else
                FillFieldList<uint64_t, uint16_t, int>(array, iOGRFieldIndex,
                                                       nOffsettedIndex,
                                                       childArray, oFeature);
        }
        else if (childFormat[0] == 'i')  // Int32
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, int32_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
            else
                FillFieldList<uint64_t, int32_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
        }
        else if (childFormat[0] == 'I')  // UInt32
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, uint32_t, GIntBig>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
            else
                FillFieldList<uint64_t, uint32_t, GIntBig>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
        }
        else if (childFormat[0] == 'l')  // Int64
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, int64_t, GIntBig>(array, iOGRFieldIndex,
                                                          nOffsettedIndex,
                                                          childArray, oFeature);
            else
                FillFieldList<uint64_t, int64_t, GIntBig>(array, iOGRFieldIndex,
                                                          nOffsettedIndex,
                                                          childArray, oFeature);
        }
        else if (childFormat[0] == 'L')  // UInt64 (lossy conversion)
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, uint64_t, double>(array, iOGRFieldIndex,
                                                          nOffsettedIndex,
                                                          childArray, oFeature);
            else
                FillFieldList<uint64_t, uint64_t, double>(array, iOGRFieldIndex,
                                                          nOffsettedIndex,
                                                          childArray, oFeature);
        }
        else if (childFormat[0] == 'e')  // float16
        {
            if (format[1] == 'l')
                FillFieldListFromHalfFloat<uint32_t>(array, iOGRFieldIndex,
                                                     nOffsettedIndex,
                                                     childArray, oFeature);
            else
                FillFieldListFromHalfFloat<uint64_t>(array, iOGRFieldIndex,
                                                     nOffsettedIndex,
                                                     childArray, oFeature);
        }
        else if (childFormat[0] == 'f')  // float32
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, float, double>(array, iOGRFieldIndex,
                                                       nOffsettedIndex,
                                                       childArray, oFeature);
            else
                FillFieldList<uint64_t, float, double>(array, iOGRFieldIndex,
                                                       nOffsettedIndex,
                                                       childArray, oFeature);
        }
        else if (childFormat[0] == 'g')  // float64
        {
            if (format[1] == 'l')
                FillFieldList<uint32_t, double, double>(array, iOGRFieldIndex,
                                                        nOffsettedIndex,
                                                        childArray, oFeature);
            else
                FillFieldList<uint64_t, double, double>(array, iOGRFieldIndex,
                                                        nOffsettedIndex,
                                                        childArray, oFeature);
        }
        else if (childFormat[0] == 'u')  // string
        {
            if (format[1] == 'l')
                FillFieldListFromString<uint32_t, uint32_t>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
            else
                FillFieldListFromString<uint64_t, uint32_t>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
        }
        else if (childFormat[0] == 'U')  // large string
        {
            if (format[1] == 'l')
                FillFieldListFromString<uint32_t, uint64_t>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
            else
                FillFieldListFromString<uint64_t, uint64_t>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
        }
    }
    else
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                 FillValidityArrayFromAttrQuery()                     */
/************************************************************************/

static size_t FillValidityArrayFromAttrQuery(
    const OGRLayer *poLayer, OGRFeatureQuery *poAttrQuery,
    const struct ArrowSchema *schema, struct ArrowArray *array,
    std::vector<bool> &abyValidityFromFilters, CSLConstList papszOptions)
{
    size_t nCountIntersecting = 0;
    auto poFeatureDefn = const_cast<OGRLayer *>(poLayer)->GetLayerDefn();
    OGRFeature oFeature(poFeatureDefn);

    std::map<std::string, std::vector<int>> oMapFieldNameToArrowPath;
    std::vector<int> anArrowPathTmp;
    BuildMapFieldNameToArrowPath(schema, oMapFieldNameToArrowPath,
                                 std::string(), anArrowPathTmp);

    struct UsedFieldsInfo
    {
        int iOGRFieldIndex{};
        std::vector<int> anArrowPath{};
    };
    std::vector<UsedFieldsInfo> aoUsedFieldsInfo;

    bool bNeedsFID = false;
    const CPLStringList aosUsedFields(poAttrQuery->GetUsedFields());
    for (int i = 0; i < aosUsedFields.size(); ++i)
    {
        int iOGRFieldIndex = poFeatureDefn->GetFieldIndex(aosUsedFields[i]);
        if (iOGRFieldIndex >= 0)
        {
            const auto oIter = oMapFieldNameToArrowPath.find(aosUsedFields[i]);
            if (oIter != oMapFieldNameToArrowPath.end())
            {
                UsedFieldsInfo info;
                info.iOGRFieldIndex = iOGRFieldIndex;
                info.anArrowPath = oIter->second;
                aoUsedFieldsInfo.push_back(info);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find %s in oMapFieldNameToArrowPath",
                         aosUsedFields[i]);
            }
        }
        else if (EQUAL(aosUsedFields[i], "FID"))
        {
            bNeedsFID = true;
        }
        else
        {
            CPLDebug("OGR", "Cannot find used field %s", aosUsedFields[i]);
        }
    }

    const size_t nLength = abyValidityFromFilters.size();

    GIntBig nBaseSeqFID = -1;
    std::vector<int> anArrowPathToFIDColumn;
    if (bNeedsFID)
    {
        // BASE_SEQUENTIAL_FID is set when there is no Arrow column for the FID
        // and we assume sequential FID numbering
        const char *pszBaseSeqFID =
            CSLFetchNameValue(papszOptions, "BASE_SEQUENTIAL_FID");
        if (pszBaseSeqFID)
        {
            nBaseSeqFID = CPLAtoGIntBig(pszBaseSeqFID);

            // Optimizimation for "FID = constant"
            swq_expr_node *poNode =
                static_cast<swq_expr_node *>(poAttrQuery->GetSWQExpr());
            if (poNode->eNodeType == SNT_OPERATION &&
                poNode->nOperation == SWQ_EQ && poNode->nSubExprCount == 2 &&
                poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
                poNode->papoSubExpr[1]->eNodeType == SNT_CONSTANT &&
                poNode->papoSubExpr[0]->field_index ==
                    poFeatureDefn->GetFieldCount() + SPF_FID &&
                poNode->papoSubExpr[1]->field_type == SWQ_INTEGER64)
            {
                if (nBaseSeqFID + static_cast<int64_t>(nLength) <
                        poNode->papoSubExpr[1]->int_value ||
                    nBaseSeqFID > poNode->papoSubExpr[1]->int_value)
                {
                    return 0;
                }
            }
        }
        else
        {
            const char *pszFIDColumn =
                const_cast<OGRLayer *>(poLayer)->GetFIDColumn();
            if (pszFIDColumn && pszFIDColumn[0])
            {
                const auto oIter = oMapFieldNameToArrowPath.find(pszFIDColumn);
                if (oIter != oMapFieldNameToArrowPath.end())
                {
                    anArrowPathToFIDColumn = oIter->second;
                }
            }
            if (anArrowPathToFIDColumn.empty())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Filtering on FID requested but cannot associate a "
                         "FID with Arrow records");
            }
        }
    }

    for (size_t iRow = 0; iRow < nLength; ++iRow)
    {
        if (!abyValidityFromFilters[iRow])
            continue;

        if (bNeedsFID)
        {
            if (nBaseSeqFID >= 0)
            {
                oFeature.SetFID(nBaseSeqFID + iRow);
            }
            else if (!anArrowPathToFIDColumn.empty())
            {
                oFeature.SetFID(OGRNullFID);

                const struct ArrowSchema *psSchemaField = schema;
                const struct ArrowArray *psArray = array;
                bool bSkip = false;
                for (size_t i = 0; i < anArrowPathToFIDColumn.size(); ++i)
                {
                    const int iChild = anArrowPathToFIDColumn[i];
                    if (i > 0)
                    {
                        const uint8_t *pabyValidity =
                            psArray->null_count == 0
                                ? nullptr
                                : static_cast<uint8_t *>(
                                      const_cast<void *>(psArray->buffers[0]));
                        const size_t nOffsettedIndex =
                            static_cast<size_t>(iRow + psArray->offset);
                        if (pabyValidity &&
                            !TestBit(pabyValidity, nOffsettedIndex))
                        {
                            bSkip = true;
                            break;
                        }
                    }

                    psSchemaField = psSchemaField->children[iChild];
                    psArray = psArray->children[iChild];
                }
                if (bSkip)
                    continue;

                const char *format = psSchemaField->format;
                const uint8_t *pabyValidity =
                    psArray->null_count == 0
                        ? nullptr
                        : static_cast<uint8_t *>(
                              const_cast<void *>(psArray->buffers[0]));
                const size_t nOffsettedIndex =
                    static_cast<size_t>(iRow + psArray->offset);
                if (pabyValidity && !TestBit(pabyValidity, nOffsettedIndex))
                {
                    // do nothing
                }
                else if (format[0] == 'i')
                {
                    oFeature.SetFID(static_cast<const int32_t *>(
                        psArray->buffers[1])[nOffsettedIndex]);
                }
                else if (format[0] == 'l')
                {
                    oFeature.SetFID(static_cast<const int64_t *>(
                        psArray->buffers[1])[nOffsettedIndex]);
                }
            }
        }

        for (const auto &sInfo : aoUsedFieldsInfo)
        {
            const int iOGRFieldIndex = sInfo.iOGRFieldIndex;
            const struct ArrowSchema *psSchemaField = schema;
            const struct ArrowArray *psArray = array;
            bool bSkip = false;
            for (size_t i = 0; i < sInfo.anArrowPath.size(); ++i)
            {
                const int iChild = sInfo.anArrowPath[i];
                if (i > 0)
                {
                    const uint8_t *pabyValidity =
                        psArray->null_count == 0
                            ? nullptr
                            : static_cast<uint8_t *>(
                                  const_cast<void *>(psArray->buffers[0]));
                    const size_t nOffsettedIndex =
                        static_cast<size_t>(iRow + psArray->offset);
                    if (pabyValidity && !TestBit(pabyValidity, nOffsettedIndex))
                    {
                        bSkip = true;
                        oFeature.SetFieldNull(iOGRFieldIndex);
                        break;
                    }
                }

                psSchemaField = psSchemaField->children[iChild];
                psArray = psArray->children[iChild];
            }
            if (bSkip)
                continue;

            const char *format = psSchemaField->format;
            const uint8_t *pabyValidity =
                psArray->null_count == 0
                    ? nullptr
                    : static_cast<uint8_t *>(
                          const_cast<void *>(psArray->buffers[0]));
            const size_t nOffsettedIndex =
                static_cast<size_t>(iRow + psArray->offset);
            if (pabyValidity && !TestBit(pabyValidity, nOffsettedIndex))
            {
                oFeature.SetFieldNull(iOGRFieldIndex);
            }
            else if (format[0] == 'b' && format[1] == '\0')
            {
                // Boolean
                oFeature.SetField(
                    iOGRFieldIndex,
                    TestBit(static_cast<const uint8_t *>(psArray->buffers[1]),
                            nOffsettedIndex));
            }
            else if (format[0] == 'c' && format[1] == '\0')
            {
                // signed int8
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const int8_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (format[0] == 'C' && format[1] == '\0')
            {
                // unsigned int8
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const uint8_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (format[0] == 's' && format[1] == '\0')
            {
                // signed int16
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const int16_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (format[0] == 'S' && format[1] == '\0')
            {
                // unsigned int16
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const uint16_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (format[0] == 'i' && format[1] == '\0')
            {
                // signed int32
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const int32_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (format[0] == 'I' && format[1] == '\0')
            {
                // unsigned int32
                oFeature.SetField(
                    iOGRFieldIndex,
                    static_cast<GIntBig>(static_cast<const uint32_t *>(
                        psArray->buffers[1])[nOffsettedIndex]));
            }
            else if (format[0] == 'l' && format[1] == '\0')
            {
                // signed int64
                oFeature.SetField(
                    iOGRFieldIndex,
                    static_cast<GIntBig>(static_cast<const int64_t *>(
                        psArray->buffers[1])[nOffsettedIndex]));
            }
            else if (format[0] == 'L' && format[1] == '\0')
            {
                // unsigned int64
                oFeature.SetField(
                    iOGRFieldIndex,
                    static_cast<double>(static_cast<const uint64_t *>(
                        psArray->buffers[1])[nOffsettedIndex]));
            }
            else if (format[0] == 'f' && format[1] == '\0')
            {
                // float32
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const float *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (format[0] == 'g' && format[1] == '\0')
            {
                // float64
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const double *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (format[0] == 'u' && format[1] == '\0')
            {
                // UTF-8 string
                const auto nOffset = static_cast<const uint32_t *>(
                    psArray->buffers[1])[nOffsettedIndex];
                const auto nNextOffset = static_cast<const uint32_t *>(
                    psArray->buffers[1])[nOffsettedIndex + 1];
                const GByte *pabyData =
                    static_cast<const GByte *>(psArray->buffers[2]);
                const uint32_t nSize = nNextOffset - nOffset;
                char *pszStr = static_cast<char *>(CPLMalloc(nSize + 1));
                memcpy(pszStr, pabyData + nOffset, nSize);
                pszStr[nSize] = 0;
                oFeature.SetFieldSameTypeUnsafe(iOGRFieldIndex, pszStr);
            }
            else if (format[0] == 'U' && format[1] == '\0')
            {
                // Large UTF-8 string
                const auto nOffset = static_cast<const uint64_t *>(
                    psArray->buffers[1])[nOffsettedIndex];
                const auto nNextOffset = static_cast<const uint64_t *>(
                    psArray->buffers[1])[nOffsettedIndex + 1];
                const GByte *pabyData =
                    static_cast<const GByte *>(psArray->buffers[2]);
                const uint64_t nSize64 = nNextOffset - nOffset;
                if (nSize64 >
                    static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
                {
                    abyValidityFromFilters.clear();
                    abyValidityFromFilters.resize(nLength);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unexpected error in PostFilterArrowArray(): too "
                             "large string");
                    return 0;
                }
                const size_t nSize = static_cast<size_t>(nSize64);
                char *pszStr = static_cast<char *>(CPLMalloc(nSize + 1));
                memcpy(pszStr, pabyData + static_cast<size_t>(nOffset), nSize);
                pszStr[nSize] = 0;
                oFeature.SetFieldSameTypeUnsafe(iOGRFieldIndex, pszStr);
            }
            else if (format[0] == 'z' && format[1] == '\0')
            {
                // Binary
                const auto nOffset = static_cast<const uint32_t *>(
                    psArray->buffers[1])[nOffsettedIndex];
                const auto nNextOffset = static_cast<const uint32_t *>(
                    psArray->buffers[1])[nOffsettedIndex + 1];
                const GByte *pabyData =
                    static_cast<const GByte *>(psArray->buffers[2]);
                const uint32_t nSize = nNextOffset - nOffset;
                if (nSize >
                    static_cast<size_t>(std::numeric_limits<int32_t>::max()))
                {
                    abyValidityFromFilters.clear();
                    abyValidityFromFilters.resize(nLength);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unexpected error in PostFilterArrowArray(): too "
                             "large binary");
                    return 0;
                }
                oFeature.SetField(iOGRFieldIndex, static_cast<int>(nSize),
                                  pabyData + nOffset);
            }
            else if (format[0] == 'Z' && format[1] == '\0')
            {
                // Large binary
                const auto nOffset = static_cast<const uint64_t *>(
                    psArray->buffers[1])[nOffsettedIndex];
                const auto nNextOffset = static_cast<const uint64_t *>(
                    psArray->buffers[1])[nOffsettedIndex + 1];
                const GByte *pabyData =
                    static_cast<const GByte *>(psArray->buffers[2]);
                const uint64_t nSize = nNextOffset - nOffset;
                if (nSize >
                    static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
                {
                    abyValidityFromFilters.clear();
                    abyValidityFromFilters.resize(nLength);
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Unexpected error in PostFilterArrowArray(): too "
                             "large binary");
                    return 0;
                }
                oFeature.SetField(iOGRFieldIndex, static_cast<int>(nSize),
                                  pabyData + nOffset);
            }
            else if (!SetFieldForOtherFormats(oFeature, iOGRFieldIndex,
                                              nOffsettedIndex, psSchemaField,
                                              psArray))
            {
                abyValidityFromFilters.clear();
                abyValidityFromFilters.resize(nLength);
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Unexpected error in PostFilterArrowArray(): unhandled "
                    "field format: %s",
                    format);
                return 0;
            }
        }
        if (poAttrQuery->Evaluate(&oFeature))
        {
            nCountIntersecting++;
        }
        else
        {
            abyValidityFromFilters[iRow] = false;
        }
    }
    return nCountIntersecting;
}

/************************************************************************/
/*                  OGRLayer::PostFilterArrowArray()                    */
/************************************************************************/

/** Remove rows that aren't selected by the spatial or attribute filter.
 *
 * Assumes that CanPostFilterArrowArray() has been called and returned true.
 */
void OGRLayer::PostFilterArrowArray(const struct ArrowSchema *schema,
                                    struct ArrowArray *array,
                                    CSLConstList papszOptions) const
{
    if (!m_poFilterGeom && !m_poAttrQuery)
        return;

    CPLAssert(schema->n_children == array->n_children);

    int64_t iGeomField = -1;
    if (m_poFilterGeom)
    {
        const char *pszGeomFieldName =
            const_cast<OGRLayer *>(this)
                ->GetLayerDefn()
                ->GetGeomFieldDefn(m_iGeomFieldFilter)
                ->GetNameRef();
        for (int64_t iField = 0; iField < schema->n_children; ++iField)
        {
            const auto fieldSchema = schema->children[iField];
            if (strcmp(fieldSchema->name, pszGeomFieldName) == 0)
            {
                iGeomField = iField;
                break;
            }
            CPLAssert(array->children[iField]->length ==
                      array->children[0]->length);
        }
        // Guaranteed if CanPostFilterArrowArray() returned true
        CPLAssert(iGeomField >= 0);
        CPLAssert(strcmp(schema->children[iGeomField]->format, "z") == 0 ||
                  strcmp(schema->children[iGeomField]->format, "Z") == 0);
        CPLAssert(array->children[iGeomField]->n_buffers == 3);
    }

    std::vector<bool> abyValidityFromFilters;
    const size_t nLength = static_cast<size_t>(array->length);
    const size_t nCountIntersectingGeom =
        m_poFilterGeom ? (strcmp(schema->children[iGeomField]->format, "z") == 0
                              ? FillValidityArrayFromWKBArray<uint32_t>(
                                    array->children[iGeomField], this,
                                    abyValidityFromFilters)
                              : FillValidityArrayFromWKBArray<uint64_t>(
                                    array->children[iGeomField], this,
                                    abyValidityFromFilters))
                       : nLength;
    if (!m_poFilterGeom)
        abyValidityFromFilters.resize(nLength, true);
    const size_t nCountIntersecting =
        m_poAttrQuery && nCountIntersectingGeom > 0
            ? FillValidityArrayFromAttrQuery(this, m_poAttrQuery, schema, array,
                                             abyValidityFromFilters,
                                             papszOptions)
        : m_poFilterGeom ? nCountIntersectingGeom
                         : nLength;
    // Nothing to do ?
    if (nCountIntersecting == nLength)
    {
        // CPLDebug("OGR", "All rows match filter");
        return;
    }

    if (nCountIntersecting > 0 &&
        !CompactStructArray(schema, array, 0, abyValidityFromFilters))
    {
        array->release(array);
        memset(array, 0, sizeof(*array));
    }

    for (int64_t iField = 0; iField < array->n_children; ++iField)
    {
        const auto psChildArray = array->children[iField];
        psChildArray->length = nCountIntersecting;
    }
    array->length = nCountIntersecting;
}

/************************************************************************/
/*                  OGRLayer::IsArrowSchemaSupported()                  */
/************************************************************************/

const struct
{
    const char *arrowType;
    OGRFieldType eType;
    OGRFieldSubType eSubType;
} gasArrowTypesToOGR[] = {
    {"b", OFTInteger, OFSTBoolean}, {"c", OFTInteger, OFSTInt16},  // Int8
    {"C", OFTInteger, OFSTInt16},                                  // UInt8
    {"s", OFTInteger, OFSTInt16},                                  // Int16
    {"S", OFTInteger, OFSTNone},                                   // UInt16
    {"i", OFTInteger, OFSTNone},                                   // Int32
    {"I", OFTInteger64, OFSTNone},                                 // UInt32
    {"l", OFTInteger64, OFSTNone},                                 // Int64
    {"L", OFTReal, OFSTNone},  // UInt64 (potentially lossy conversion if going through OGRFeature)
    {"e", OFTReal, OFSTFloat32},  // float16
    {"f", OFTReal, OFSTFloat32},  // float32
    {"g", OFTReal, OFSTNone},     // float64
    {"z", OFTBinary, OFSTNone},   // binary
    {"Z", OFTBinary, OFSTNone},  // large binary (will be limited to 32 bit length though if going through OGRFeature!)
    {"u", OFTString, OFSTNone},  // string
    {"U", OFTString, OFSTNone},  // large string
    {"tdD", OFTDate, OFSTNone},  // date32[days]
    {"tdm", OFTDate, OFSTNone},  // date64[milliseconds]
    {"tts", OFTTime, OFSTNone},  // time32 [seconds]
    {"ttm", OFTTime, OFSTNone},  // time32 [milliseconds]
    {"ttu", OFTTime, OFSTNone},  // time64 [microseconds]
    {"ttn", OFTTime, OFSTNone},  // time64 [nanoseconds]
};

const struct
{
    const char *arrowType;
    OGRFieldType eType;
    OGRFieldSubType eSubType;
} gasListTypes[] = {
    {"b", OFTIntegerList, OFSTBoolean},
    {"c", OFTIntegerList, OFSTInt16},   // Int8
    {"C", OFTIntegerList, OFSTInt16},   // UInt8
    {"s", OFTIntegerList, OFSTInt16},   // Int16
    {"S", OFTIntegerList, OFSTNone},    // UInt16
    {"i", OFTIntegerList, OFSTNone},    // Int32
    {"I", OFTInteger64List, OFSTNone},  // UInt32
    {"l", OFTInteger64List, OFSTNone},  // Int64
    {"L", OFTRealList, OFSTNone},  // UInt64 (potentially lossy conversion if going through OGRFeature)
    {"e", OFTRealList, OFSTFloat32},  // float16
    {"f", OFTRealList, OFSTFloat32},  // float32
    {"g", OFTRealList, OFSTNone},     // float64
    {"u", OFTStringList, OFSTNone},   // string
    {"U", OFTStringList, OFSTNone},   // large string
};

/** Returns whether the provided ArrowSchema is supported for writing.
 *
 * This method exists since not all drivers may support all Arrow data types.
 *
 * The ArrowSchema must be of type struct (format=+s)
 *
 * It is recommended to call this method before calling WriteArrowBatch().
 *
 * This is the same as the C function OGR_L_IsArrowSchemaSupported().
 *
 * @param schema Schema of type struct (format = '+s')
 * @param[out] osErrorMsg Reason of the failure, when this method returns false.
 * @return true if the ArrowSchema is supported for writing.
 * @since 3.8
 */
bool OGRLayer::IsArrowSchemaSupported(const struct ArrowSchema *schema,
                                      std::string &osErrorMsg) const
{
    if (strcmp(schema->format, "+s") != 0)
    {
        osErrorMsg =
            "IsArrowSchemaSupported() should be called on a schema that is a "
            "struct of fields";
        return false;
    }
    return IsArrowSchemaSupportedInternal(schema, std::string(), osErrorMsg);
}

//! @cond Doxygen_Suppress
bool OGRLayer::IsArrowSchemaSupportedInternal(const struct ArrowSchema *schema,
                                              const std::string &osFieldPrefix,
                                              std::string &osErrorMsg) const
{
    const auto AppendError = [&osErrorMsg](const std::string &osMsg)
    {
        if (!osErrorMsg.empty())
            osErrorMsg += " ";
        osErrorMsg += osMsg;
    };

    const char *format = schema->format;
    if (strcmp(format, "+s") == 0)
    {
        bool bRet = true;
        const std::string osNewPrefix(osFieldPrefix + schema->name + ".");
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!IsArrowSchemaSupportedInternal(schema->children[i],
                                                osNewPrefix, osErrorMsg))
                bRet = false;
        }
        return bRet;
    }

    else if (strcmp(format, "+l") == 0 ||  // list
             strcmp(format, "+L") == 0 ||  // large list
             STARTS_WITH(format, "+w:"))   // fixed-size list
    {
        // Only some subtypes supported
        const char *childFormat = schema->children[0]->format;
        for (const auto &sType : gasListTypes)
        {
            if (strcmp(childFormat, sType.arrowType) == 0)
            {
                return true;
            }
        }
        AppendError("Type list of '" + std::string(childFormat) +
                    "' for field " + osFieldPrefix + schema->name +
                    " is not supported.");
        return false;
    }

    else if (strcmp(format, "+m") == 0)
    {
        AppendError("Type map for field " + osFieldPrefix + schema->name +
                    " is not supported.");
        return false;
    }
    else
    {
        for (const auto &sType : gasArrowTypesToOGR)
        {
            if (strcmp(format, sType.arrowType) == 0)
            {
                return true;
            }
        }

        const char *const apszSupportedPrefix[] = {
            "w:",    // fixed-size binary
            "+w:",   // fixed-size list
            "tss:",  // timestamp[s]
            "tsm:",  // timestamp[ms]
            "tsu:",  // timestamp[us]
            "tsn:"   // timestamp[ns]
        };
        for (const char *pszSupported : apszSupportedPrefix)
        {
            if (STARTS_WITH(format, pszSupported))
                return true;
        }
        // TODO: "d:"
        AppendError("Type '" + std::string(format) + "' for field " +
                    osFieldPrefix + schema->name + " is not supported.");
        return false;
    }
}
//! @endcond

/************************************************************************/
/*                  OGR_L_IsArrowSchemaSupported()                      */
/************************************************************************/

/** Returns whether the provided ArrowSchema is supported for writing.
 *
 * This function exists since not all drivers may support all Arrow data types.
 *
 * The ArrowSchema must be of type struct (format=+s)
 *
 * It is recommended to call this function before calling OGR_L_WriteArrowBatch().
 *
 * This is the same as the C++ method OGRLayer::IsArrowSchemaSupported().
 *
 * @param hLayer Layer.
 * @param schema Schema of type struct (format = '+s')
 * @param[out] ppszErrorMsg nullptr, or pointer to a string that will contain
 * the reason of the failure, when this function returns false.
 * @return true if the ArrowSchema is supported for writing.
 * @since 3.8
 */
bool OGR_L_IsArrowSchemaSupported(OGRLayerH hLayer,
                                  const struct ArrowSchema *schema,
                                  char **ppszErrorMsg)
{
    VALIDATE_POINTER1(hLayer, __func__, false);
    VALIDATE_POINTER1(schema, __func__, false);

    std::string osErrorMsg;
    if (!OGRLayer::FromHandle(hLayer)->IsArrowSchemaSupported(schema,
                                                              osErrorMsg))
    {
        if (ppszErrorMsg)
            *ppszErrorMsg = VSIStrdup(osErrorMsg.c_str());
        return false;
    }
    else
    {
        if (ppszErrorMsg)
            *ppszErrorMsg = nullptr;
        return true;
    }
}

/************************************************************************/
/*                OGRLayer::CreateFieldFromArrowSchema()                */
/************************************************************************/

//! @cond Doxygen_Suppress
bool OGRLayer::CreateFieldFromArrowSchemaInternal(
    const struct ArrowSchema *schema, const std::string &osFieldPrefix,
    CSLConstList papszOptions)
{
    const char *format = schema->format;
    if (strcmp(format, "+s") == 0)
    {
        const std::string osNewPrefix(osFieldPrefix + schema->name + ".");
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!CreateFieldFromArrowSchemaInternal(schema->children[i],
                                                    osNewPrefix, papszOptions))
                return false;
        }
        return true;
    }

    for (const auto &sType : gasArrowTypesToOGR)
    {
        if (strcmp(format, sType.arrowType) == 0)
        {
            OGRFieldDefn oFieldDefn((osFieldPrefix + schema->name).c_str(),
                                    sType.eType);
            oFieldDefn.SetSubType(sType.eSubType);
            oFieldDefn.SetNullable((schema->flags & ARROW_FLAG_NULLABLE) != 0);
            return CreateField(&oFieldDefn) == OGRERR_NONE;
        }
    }

    if (STARTS_WITH(format, "tss:") ||  // timestamp[s]
        STARTS_WITH(format, "tsm:") ||  // timestamp[ms]
        STARTS_WITH(format, "tsu:") ||  // timestamp[us]
        STARTS_WITH(format, "tsn:"))    // timestamp[ns]
    {
        OGRFieldDefn oFieldDefn((osFieldPrefix + schema->name).c_str(),
                                OFTDateTime);
        oFieldDefn.SetNullable((schema->flags & ARROW_FLAG_NULLABLE) != 0);
        return CreateField(&oFieldDefn) == OGRERR_NONE;
    }

    if (STARTS_WITH(format, "w:"))  // fixed-width binary
    {
        OGRFieldDefn oFieldDefn((osFieldPrefix + schema->name).c_str(),
                                OFTBinary);
        oFieldDefn.SetWidth(atoi(format + strlen("w:")));
        oFieldDefn.SetNullable((schema->flags & ARROW_FLAG_NULLABLE) != 0);
        return CreateField(&oFieldDefn) == OGRERR_NONE;
    }

    if (strcmp(format, "+l") == 0 ||  // list
        strcmp(format, "+L") == 0 ||  // large list
        STARTS_WITH(format, "+w:"))   // fixed-size list
    {
        const char *childFormat = schema->children[0]->format;
        for (const auto &sType : gasListTypes)
        {
            if (strcmp(childFormat, sType.arrowType) == 0)
            {
                OGRFieldDefn oFieldDefn((osFieldPrefix + schema->name).c_str(),
                                        sType.eType);
                oFieldDefn.SetSubType(sType.eSubType);
                oFieldDefn.SetNullable((schema->flags & ARROW_FLAG_NULLABLE) !=
                                       0);
                return CreateField(&oFieldDefn) == OGRERR_NONE;
            }
        }
        CPLError(CE_Failure, CPLE_NotSupported, "%s",
                 ("List of type '" + std::string(childFormat) + "' for field " +
                  osFieldPrefix + schema->name + " is not supported.")
                     .c_str());
        return false;
    }

    CPLError(CE_Failure, CPLE_NotSupported, "%s",
             ("Type '" + std::string(format) + "' for field " + osFieldPrefix +
              schema->name + " is not supported.")
                 .c_str());
    return false;
}
//! @endcond

/** Creates a field from an ArrowSchema.
 *
 * This should only be used for attribute fields. Geometry fields should
 * be created with CreateGeomField(). The FID field should also not be
 * passed with this method.
 *
 * Contrary to the IsArrowSchemaSupported() and WriteArrowBatch() methods, the
 * passed schema must be for an individual field, and thus, is *not* of type
 * struct (format=+s) (unless writing a set of fields grouped together in the
 * same structure).
 *
 * This method and CreateField() are mutually exclusive in the same session.
 *
 * This method is the same as the C function OGR_L_CreateFieldFromArrowSchema().
 *
 * @param schema Schema of the field to create.
 * @param papszOptions Options. Pass nullptr currently.
 * @return true in case of success
 * @since 3.8
 */
bool OGRLayer::CreateFieldFromArrowSchema(const struct ArrowSchema *schema,
                                          CSLConstList papszOptions)
{
    return CreateFieldFromArrowSchemaInternal(schema, std::string(),
                                              papszOptions);
}

/************************************************************************/
/*                  OGR_L_CreateFieldFromArrowSchema()                  */
/************************************************************************/

/** Creates a field from an ArrowSchema.
 *
 * This should only be used for attribute fields. Geometry fields should
 * be created with CreateGeomField(). The FID field should also not be
 * passed with this method.
 *
 * Contrary to the IsArrowSchemaSupported() and WriteArrowBatch() methods, the
 * passed schema must be for an individual field, and thus, is *not* of type
 * struct (format=+s) (unless writing a set of fields grouped together in the
 * same structure).
 *
 * This method and CreateField() are mutually exclusive in the same session.
 *
 * This method is the same as the C++ method OGRLayer::CreateFieldFromArrowSchema().
 *
 * @param hLayer Layer.
 * @param schema Schema of the field to create.
 * @param papszOptions Options. Pass nullptr currently.
 * @return true in case of success
 * @since 3.8
 */
bool OGR_L_CreateFieldFromArrowSchema(OGRLayerH hLayer,
                                      const struct ArrowSchema *schema,
                                      char **papszOptions)
{
    VALIDATE_POINTER1(hLayer, __func__, false);
    VALIDATE_POINTER1(schema, __func__, false);

    return OGRLayer::FromHandle(hLayer)->CreateFieldFromArrowSchema(
        schema, papszOptions);
}

/************************************************************************/
/*                           BuildOGRFieldInfo()                        */
/************************************************************************/

constexpr int FID_COLUMN_SPECIAL_OGR_FIELD_IDX = -2;

struct FieldInfo
{
    std::string osName{};
    int iOGRFieldIdx = -1;
    OGRFieldType eFieldType = OFTMaxType;
    bool bIsGeomCol = false;
};

static bool BuildOGRFieldInfo(
    const struct ArrowSchema *schema, struct ArrowArray *array,
    const OGRFeatureDefn *poFeatureDefn, const std::string &osFieldPrefix,
    std::vector<FieldInfo> &asFieldInfo, const char *pszFIDName,
    const char *pszGeomFieldName, const struct ArrowSchema *&schemaFIDColumn,
    struct ArrowArray *&arrayFIDColumn)
{
    const char *format = schema->format;
    if (strcmp(format, "+s") == 0)
    {
        const std::string osNewPrefix(osFieldPrefix + schema->name + ".");
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!BuildOGRFieldInfo(schema->children[i], array->children[i],
                                   poFeatureDefn, osNewPrefix, asFieldInfo,
                                   pszFIDName, pszGeomFieldName,
                                   schemaFIDColumn, arrayFIDColumn))
            {
                return false;
            }
        }
    }
    else
    {
        FieldInfo sInfo;
        sInfo.osName = osFieldPrefix + schema->name;
        if (pszFIDName && sInfo.osName == pszFIDName)
        {
            if (format[0] == 'i' || format[0] == 'l')
            {
                sInfo.iOGRFieldIdx = FID_COLUMN_SPECIAL_OGR_FIELD_IDX;
                schemaFIDColumn = schema;
                arrayFIDColumn = array;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "FID column '%s' should be of Arrow format 'i' "
                         "(int32) or 'l' (int64)",
                         sInfo.osName.c_str());
                return false;
            }
        }
        else
        {
            sInfo.iOGRFieldIdx =
                poFeatureDefn->GetFieldIndex(sInfo.osName.c_str());
            if (sInfo.iOGRFieldIdx >= 0)
            {
                bool bTypeOK = false;
                const auto eOGRType =
                    poFeatureDefn->GetFieldDefn(sInfo.iOGRFieldIdx)->GetType();
                sInfo.eFieldType = eOGRType;
                for (const auto &sType : gasArrowTypesToOGR)
                {
                    if (strcmp(format, sType.arrowType) == 0)
                    {
                        if (eOGRType == sType.eType)
                        {
                            bTypeOK = true;
                            break;
                        }
                        else
                        {
                            CPLError(
                                CE_Failure, CPLE_AppDefined,
                                "For field %s, OGR field type is %s whereas "
                                "Arrow type implies %s",
                                sInfo.osName.c_str(),
                                OGR_GetFieldTypeName(eOGRType),
                                OGR_GetFieldTypeName(sType.eType));
                            return false;
                        }
                    }
                }

                if (!bTypeOK && (STARTS_WITH(format, "tss:") ||
                                 STARTS_WITH(format, "tsm:") ||
                                 STARTS_WITH(format, "tsu:") ||
                                 STARTS_WITH(format, "tsn:")))
                {
                    if (eOGRType == OFTDateTime)
                    {
                        bTypeOK = true;
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "For field %s, OGR field type is %s whereas "
                                 "Arrow type implies %s",
                                 sInfo.osName.c_str(),
                                 OGR_GetFieldTypeName(eOGRType),
                                 OGR_GetFieldTypeName(OFTDateTime));
                        return false;
                    }
                }

                if (!bTypeOK && STARTS_WITH(format, "w:"))
                {
                    if (eOGRType == OFTBinary)
                    {
                        bTypeOK = true;
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "For field %s, OGR field type is %s whereas "
                                 "Arrow type implies %s",
                                 sInfo.osName.c_str(),
                                 OGR_GetFieldTypeName(eOGRType),
                                 OGR_GetFieldTypeName(OFTBinary));
                        return false;
                    }
                }

                if (!bTypeOK &&
                    (strcmp(format, "+l") == 0 || strcmp(format, "+L") == 0 ||
                     STARTS_WITH(format, "+w:")))
                {
                    const char *childFormat = schema->children[0]->format;
                    for (const auto &sType : gasListTypes)
                    {
                        if (strcmp(childFormat, sType.arrowType) == 0)
                        {
                            if (eOGRType == sType.eType)
                            {
                                bTypeOK = true;
                                break;
                            }
                            else
                            {
                                CPLError(CE_Failure, CPLE_AppDefined,
                                         "For field %s, OGR field type is %s "
                                         "whereas "
                                         "Arrow type implies %s",
                                         sInfo.osName.c_str(),
                                         OGR_GetFieldTypeName(eOGRType),
                                         OGR_GetFieldTypeName(sType.eType));
                                return false;
                            }
                        }
                    }

                    if (!bTypeOK)
                    {
                        CPLError(CE_Failure, CPLE_NotSupported, "%s",
                                 ("List of type '" + std::string(childFormat) +
                                  "' for field " + osFieldPrefix +
                                  schema->name + " is not supported.")
                                     .c_str());
                        return false;
                    }
                }

                if (!bTypeOK)
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "%s",
                             ("Type '" + std::string(format) + "' for field " +
                              osFieldPrefix + schema->name +
                              " is not supported.")
                                 .c_str());
                    return false;
                }
            }
            else
            {
                sInfo.iOGRFieldIdx =
                    poFeatureDefn->GetGeomFieldIndex(sInfo.osName.c_str());
                if (sInfo.iOGRFieldIdx < 0)
                {
                    if (pszGeomFieldName && pszGeomFieldName == sInfo.osName)
                    {
                        if (poFeatureDefn->GetGeomFieldCount() == 0)
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Cannot find OGR geometry field for Arrow "
                                     "array %s",
                                     sInfo.osName.c_str());
                            return false;
                        }
                        sInfo.iOGRFieldIdx = 0;
                    }
                    else
                    {
                        // Check if ARROW:extension:name = ogc.wkb
                        const char *pabyMetadata = schema->metadata;
                        if (pabyMetadata)
                        {
                            const auto oMetadata =
                                OGRParseArrowMetadata(pabyMetadata);
                            auto oIter =
                                oMetadata.find(ARROW_EXTENSION_NAME_KEY);
                            if (oIter != oMetadata.end() &&
                                oIter->second == EXTENSION_NAME_WKB)
                            {
                                if (poFeatureDefn->GetGeomFieldCount() == 0)
                                {
                                    CPLError(CE_Failure, CPLE_AppDefined,
                                             "Cannot find OGR geometry field "
                                             "for Arrow array %s",
                                             sInfo.osName.c_str());
                                    return false;
                                }
                                sInfo.iOGRFieldIdx = 0;
                            }
                        }
                    }
                    if (sInfo.iOGRFieldIdx < 0)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Cannot find OGR field for Arrow array %s",
                                 sInfo.osName.c_str());
                        return false;
                    }
                }

                if (strcmp(schema->format, "z") != 0 &&
                    strcmp(schema->format, "Z") != 0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Geometry column '%s' should be of Arrow format "
                             "'z' (binary) or 'Z' (large binary)",
                             sInfo.osName.c_str());
                    return false;
                }
                sInfo.bIsGeomCol = true;
            }
        }

        asFieldInfo.emplace_back(std::move(sInfo));
    }
    return true;
}

/************************************************************************/
/*                         GetWorkingBufferSize()                       */
/************************************************************************/

static size_t GetWorkingBufferSize(const struct ArrowSchema *schema,
                                   const struct ArrowArray *array,
                                   size_t iFeature)
{
    const char *format = schema->format;
    if (format[0] == '+' && format[1] == 's')
    {
        size_t nRet = 0;
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            nRet += GetWorkingBufferSize(schema->children[i],
                                         array->children[i], iFeature);
        }
        return nRet;
    }
    else if (format[0] == 'u')  // UTF-8
    {
        const uint8_t *pabyValidity =
            static_cast<const uint8_t *>(array->buffers[0]);
        if (array->null_count != 0 && pabyValidity &&
            !TestBit(pabyValidity,
                     static_cast<size_t>(iFeature + array->offset)))
        {
            // empty string
        }
        else
        {
            const auto *panOffsets =
                static_cast<const uint32_t *>(array->buffers[1]) +
                array->offset;
            return 1 + (panOffsets[iFeature + 1] - panOffsets[iFeature]);
        }
    }
    else if (format[0] == 'U')  // large UTF-8
    {
        const uint8_t *pabyValidity =
            static_cast<const uint8_t *>(array->buffers[0]);
        if (array->null_count != 0 && pabyValidity &&
            !TestBit(pabyValidity,
                     static_cast<size_t>(iFeature + array->offset)))
        {
            // empty string
        }
        else
        {
            const auto *panOffsets =
                static_cast<const uint64_t *>(array->buffers[1]) +
                array->offset;
            return 1 + static_cast<size_t>(panOffsets[iFeature + 1] -
                                           panOffsets[iFeature]);
        }
    }
    return 0;
}

/************************************************************************/
/*                              FillField()                             */
/************************************************************************/

template <typename ArrowType, typename OGRType = ArrowType>
inline static void FillField(const struct ArrowArray *array, int iOGRFieldIdx,
                             size_t iFeature, OGRFeature &oFeature)
{
    const auto *panValues = static_cast<const ArrowType *>(array->buffers[1]);
    oFeature.SetFieldSameTypeUnsafe(
        iOGRFieldIdx,
        static_cast<OGRType>(panValues[iFeature + array->offset]));
}
/************************************************************************/
/*                          FillFieldString()                           */
/************************************************************************/

template <typename OffsetType>
inline static void FillFieldString(const struct ArrowArray *array,
                                   int iOGRFieldIdx, size_t iFeature,
                                   std::string &osWorkingBuffer,
                                   OGRFeature &oFeature)
{
    const auto *panOffsets =
        static_cast<const OffsetType *>(array->buffers[1]) + array->offset;
    const char *pszStr = static_cast<const char *>(array->buffers[2]);
    oFeature.SetFieldSameTypeUnsafe(iOGRFieldIdx, &osWorkingBuffer[0] +
                                                      osWorkingBuffer.size());
    const size_t nLen =
        static_cast<size_t>(panOffsets[iFeature + 1] - panOffsets[iFeature]);
    osWorkingBuffer.append(pszStr + panOffsets[iFeature], nLen);
    osWorkingBuffer.push_back(0);  // append null character
}

/************************************************************************/
/*                          FillFieldBinary()                           */
/************************************************************************/

template <typename OffsetType>
inline static bool
FillFieldBinary(const struct ArrowArray *array, int iOGRFieldIdx,
                size_t iFeature, int iArrowIdx,
                const std::vector<FieldInfo> &asFieldInfo,
                const std::string &osFieldPrefix, const char *pszFieldName,
                OGRFeature &oFeature)
{
    const auto *panOffsets =
        static_cast<const OffsetType *>(array->buffers[1]) + array->offset;
    const GByte *pabyData = static_cast<const GByte *>(array->buffers[2]) +
                            static_cast<size_t>(panOffsets[iFeature]);
    const size_t nLen =
        static_cast<size_t>(panOffsets[iFeature + 1] - panOffsets[iFeature]);
    if (asFieldInfo[iArrowIdx].bIsGeomCol)
    {
        size_t nBytesConsumedOut = 0;
        OGRGeometry *poGeometry = nullptr;
        OGRGeometryFactory::createFromWkb(pabyData, nullptr, &poGeometry, nLen,
                                          wkbVariantIso, nBytesConsumedOut);
        oFeature.SetGeomFieldDirectly(iOGRFieldIdx, poGeometry);
    }
    else
    {
        if (nLen > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Content for field %s%s is too large",
                     osFieldPrefix.c_str(), pszFieldName);
            return false;
        }
        oFeature.SetField(iOGRFieldIdx, static_cast<int>(nLen), pabyData);
    }
    return true;
}

/************************************************************************/
/*                             FillFeature()                            */
/************************************************************************/

static bool FillFeature(OGRLayer *poLayer, const struct ArrowSchema *schema,
                        const struct ArrowArray *array,
                        const std::string &osFieldPrefix, size_t iFeature,
                        int &iArrowIdxInOut,
                        const std::vector<FieldInfo> &asFieldInfo,
                        OGRFeature &oFeature, std::string &osWorkingBuffer)

{
    const char *format = schema->format;
    if (format[0] == '+' && format[1] == 's')
    {
        const std::string osNewPrefix(osFieldPrefix + schema->name + ".");
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!FillFeature(poLayer, schema->children[i], array->children[i],
                             osNewPrefix, iFeature, iArrowIdxInOut, asFieldInfo,
                             oFeature, osWorkingBuffer))
                return false;
        }
        return true;
    }
    const int iArrowIdx = iArrowIdxInOut;
    ++iArrowIdxInOut;
    const int iOGRFieldIdx = asFieldInfo[iArrowIdx].iOGRFieldIdx;
    if (array->null_count != 0)
    {
        const uint8_t *pabyValidity =
            static_cast<const uint8_t *>(array->buffers[0]);
        if (pabyValidity &&
            !TestBit(pabyValidity,
                     static_cast<size_t>(iFeature + array->offset)))
        {
            if (iOGRFieldIdx == FID_COLUMN_SPECIAL_OGR_FIELD_IDX)
                oFeature.SetFID(OGRNullFID);
            else if (asFieldInfo[iArrowIdx].bIsGeomCol)
                oFeature.SetGeomFieldDirectly(iOGRFieldIdx, nullptr);
            else
            {
                OGRField *psField = oFeature.GetRawFieldRef(iOGRFieldIdx);
                switch (asFieldInfo[iArrowIdx].eFieldType)
                {
                    case OFTRealList:
                    case OFTIntegerList:
                    case OFTInteger64List:
                        if (IsValidField(psField))
                            CPLFree(psField->IntegerList.paList);
                        break;

                    case OFTStringList:
                        if (IsValidField(psField))
                            CSLDestroy(psField->StringList.paList);
                        break;

                    case OFTBinary:
                        if (IsValidField(psField))
                            CPLFree(psField->Binary.paData);
                        break;

                    default:
                        break;
                }
                OGR_RawField_SetNull(psField);
            }
            return true;
        }
    }

    if (format[0] == 'b')  // Boolean
    {
        const uint8_t *pabyValues =
            static_cast<const uint8_t *>(array->buffers[1]);
        oFeature.SetFieldSameTypeUnsafe(
            iOGRFieldIdx,
            TestBit(pabyValues, static_cast<size_t>(iFeature + array->offset))
                ? 1
                : 0);
        return true;
    }
    else if (format[0] == 'c')  // Int8
    {
        FillField<int8_t>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (format[0] == 'C')  // UInt8
    {
        FillField<uint8_t>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (format[0] == 's')  // Int16
    {
        FillField<int16_t>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (format[0] == 'S')  // UInt16
    {
        FillField<uint16_t>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (format[0] == 'i')  // Int32
    {
        if (iOGRFieldIdx == FID_COLUMN_SPECIAL_OGR_FIELD_IDX)
        {
            const auto *panValues =
                static_cast<const int32_t *>(array->buffers[1]);
            oFeature.SetFID(panValues[iFeature + array->offset]);
        }
        else
        {
            FillField<int32_t>(array, iOGRFieldIdx, iFeature, oFeature);
        }
        return true;
    }
    else if (format[0] == 'I')  // UInt32
    {
        FillField<uint32_t, GIntBig>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (format[0] == 'l')  // Int64
    {
        if (iOGRFieldIdx == FID_COLUMN_SPECIAL_OGR_FIELD_IDX)
        {
            const auto *panValues =
                static_cast<const int64_t *>(array->buffers[1]);
            oFeature.SetFID(panValues[iFeature + array->offset]);
        }
        else
        {
            FillField<int64_t, GIntBig>(array, iOGRFieldIdx, iFeature,
                                        oFeature);
        }
        return true;
    }
    else if (format[0] == 'L')  // UInt64
    {
        FillField<uint64_t, double>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (format[0] == 'f')  // float32
    {
        FillField<float>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (format[0] == 'g')  // float64
    {
        FillField<double>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (format[0] == 'u')  // UTF-8
    {
        FillFieldString<uint32_t>(array, iOGRFieldIdx, iFeature,
                                  osWorkingBuffer, oFeature);
        return true;
    }
    else if (format[0] == 'U')  // large UTF-8
    {
        FillFieldString<uint64_t>(array, iOGRFieldIdx, iFeature,
                                  osWorkingBuffer, oFeature);
        return true;
    }
    else if (format[0] == 'z')  // Binary
    {
        return FillFieldBinary<uint32_t>(array, iOGRFieldIdx, iFeature,
                                         iArrowIdx, asFieldInfo, osFieldPrefix,
                                         schema->name, oFeature);
    }
    else if (format[0] == 'Z')  // large binary
    {
        return FillFieldBinary<uint64_t>(array, iOGRFieldIdx, iFeature,
                                         iArrowIdx, asFieldInfo, osFieldPrefix,
                                         schema->name, oFeature);
    }
    else if (SetFieldForOtherFormats(
                 oFeature, iOGRFieldIdx,
                 static_cast<size_t>(iFeature + array->offset), schema, array))
    {
        return true;
    }

    CPLError(CE_Failure, CPLE_NotSupported, "%s",
             ("Type '" + std::string(format) + "' for field " + osFieldPrefix +
              schema->name + " is not supported.")
                 .c_str());
    return false;
}

/************************************************************************/
/*                    OGRLayer::WriteArrowBatch()                       */
/************************************************************************/

// clang-format off
/** Writes a batch of rows from an ArrowArray.
 *
 * This is semantically close to calling CreateFeature() with multiple features
 * at once.
 *
 * The ArrowArray must be of type struct (format=+s), and its children generally
 * map to a OGR attribute or geometry field (unless they are struct themselves).
 *
 * Method IsArrowSchemaSupported() can be called to determine if the schema
 * will be supported by WriteArrowBatch().
 *
 * OGR fields for the corresponding children arrays must exist and be of a
 * compatible type. For attribute fields, they should be created with
 * CreateFieldFromArrowSchema().
 *
 * Arrays for geometry columns should be of binary or large binary type and
 * contain WKB geometry.
 *
 * Note that the passed array may be set to a released state
 * (array->release==NULL) after this call (not by the base implementation,
 * but in specialized ones such as Parquet or Arrow for example)
 *
 * Supported options of the base implementation are:
 * <ul>
 * <li>FID=name. Name of the FID column in the array. If not provided,
 *     GetFIDColumn() is used to determine it. The special name
 *     OGRLayer::DEFAULT_ARROW_FID_NAME is also recognized if neither FID nor
 *     GetFIDColumn() are set.
 *     The corresponding ArrowArray must be of type int32 (i) or int64 (l).
 *     On input, values of the FID column are used to create the feature.
 *     On output, the values of the FID column may be set with the FID of the
 *     created feature (if the array is not released).
 * </li>
 * <li>GEOMETRY_NAME=name. Name of the geometry column. If not provided,
 *     GetGeometryColumn() is used. The special name
 *     OGRLayer::DEFAULT_ARROW_GEOMETRY_NAME is also recognized if neither
 *     GEOMETRY_NAME nor GetGeometryColumn() are set.
 *     Geometry columns are also identified if they have
 *     ARROW:extension:name=ogc.wkb as a field metadata.
 *     The corresponding ArrowArray must be of type binary (w) or large
 *     binary (W).
 * </li>
 * </ul>
 *
 * The following example demonstrates how to copy a layer from one format to
 * another one (assuming it has at most a single geometry column):
\code{.py}
    def copy_layer(src_lyr, out_filename, out_format, lcos = {}):
        stream = src_lyr.GetArrowStream()
        schema = stream.GetSchema()

        # If the source layer has a FID column and the output driver supports
        # a FID layer creation option, set it to the source FID column name.
        if src_lyr.GetFIDColumn():
            creationOptions = gdal.GetDriverByName(out_format).GetMetadataItem(
                "DS_LAYER_CREATIONOPTIONLIST"
            )
            if creationOptions and '"FID"' in creationOptions:
                lcos["FID"] = src_lyr.GetFIDColumn()

        with ogr.GetDriverByName(out_format).CreateDataSource(out_filename) as out_ds:
            if src_lyr.GetLayerDefn().GetGeomFieldCount() > 1:
                out_lyr = out_ds.CreateLayer(
                    src_lyr.GetName(), geom_type=ogr.wkbNone, options=lcos
                )
                for i in range(src_lyr.GetLayerDefn().GetGeomFieldCount()):
                    out_lyr.CreateGeomField(src_lyr.GetLayerDefn().GetGeomFieldDefn(i))
            else:
                out_lyr = out_ds.CreateLayer(
                    src_lyr.GetName(),
                    geom_type=src_lyr.GetGeomType(),
                    srs=src_lyr.GetSpatialRef(),
                    options=lcos,
                )

            success, error_msg = out_lyr.IsArrowSchemaSupported(schema)
            assert success, error_msg

            src_geom_field_names = [
                src_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName()
                for i in range(src_lyr.GetLayerDefn().GetGeomFieldCount())
            ]
            for i in range(schema.GetChildrenCount()):
                # GetArrowStream() may return "OGC_FID" for a unnamed source FID
                # column and "wkb_geometry" for a unnamed source geometry column.
                # Also test GetFIDColumn() and src_geom_field_names if they are
                # named.
                if (
                    schema.GetChild(i).GetName()
                    not in ("OGC_FID", "wkb_geometry", src_lyr.GetFIDColumn())
                    and schema.GetChild(i).GetName() not in src_geom_field_names
                ):
                    out_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

            write_options = []
            if src_lyr.GetFIDColumn():
                write_options.append("FID=" + src_lyr.GetFIDColumn())
            if (
                src_lyr.GetLayerDefn().GetGeomFieldCount() == 1
                and src_lyr.GetGeometryColumn()
            ):
                write_options.append("GEOMETRY_NAME=" + src_lyr.GetGeometryColumn())

            while True:
                array = stream.GetNextRecordBatch()
                if array is None:
                    break
                out_lyr.WriteArrowBatch(schema, array, write_options)
\endcode
 *
 * This method and CreateFeature() are mutually exclusive in the same session.
 *
 * This method is the same as the C function OGR_L_WriteArrowBatch().
 *
 * @param schema Schema of array
 * @param array Array of type struct. It may be released (array->release==NULL)
 *              after calling this method.
 * @param papszOptions Options. Null terminated list, or nullptr.
 * @return true in case of success
 * @since 3.8
 */
// clang-format on

bool OGRLayer::WriteArrowBatch(const struct ArrowSchema *schema,
                               struct ArrowArray *array,
                               CSLConstList papszOptions)
{
    const char *format = schema->format;
    if (strcmp(format, "+s") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteArrowBatch() should be called on a schema that is a "
                 "struct of fields");
        return false;
    }

    if (schema->n_children != array->n_children)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteArrowBatch(): schema->n_children (%d) != "
                 "array->n_children (%d)",
                 int(schema->n_children), int(array->n_children));
        return false;
    }

    std::vector<FieldInfo> asFieldInfo;
    auto poLayerDefn = GetLayerDefn();
    const char *pszFIDName =
        CSLFetchNameValueDef(papszOptions, "FID", GetFIDColumn());
    if (!pszFIDName || pszFIDName[0] == 0)
        pszFIDName = DEFAULT_ARROW_FID_NAME;
    const char *pszGeomFieldName = CSLFetchNameValueDef(
        papszOptions, "GEOMETRY_NAME", GetGeometryColumn());
    if (!pszGeomFieldName || pszGeomFieldName[0] == 0)
        pszGeomFieldName = DEFAULT_ARROW_GEOMETRY_NAME;
    const struct ArrowSchema *schemaFIDColumn = nullptr;
    struct ArrowArray *arrayFIDColumn = nullptr;
    for (int64_t i = 0; i < schema->n_children; ++i)
    {
        if (!BuildOGRFieldInfo(schema->children[i], array->children[i],
                               poLayerDefn, std::string(), asFieldInfo,
                               pszFIDName, pszGeomFieldName, schemaFIDColumn,
                               arrayFIDColumn))
        {
            return false;
        }
    }

    struct FeatureCleaner
    {
        OGRFeature &m_oFeature;

        explicit FeatureCleaner(OGRFeature &oFeature) : m_oFeature(oFeature)
        {
        }

        // As we set a value that can't be CPLFree()'d in the .String member
        // of string fields, we must take care of manually unsetting it before
        // the destructor of OGRFeature gets called.
        ~FeatureCleaner()
        {
            const auto poLayerDefn = m_oFeature.GetDefnRef();
            const int nFieldCount = poLayerDefn->GetFieldCount();
            for (int i = 0; i < nFieldCount; ++i)
            {
                if (poLayerDefn->GetFieldDefnUnsafe(i)->GetType() == OFTString)
                {
                    if (m_oFeature.IsFieldSetAndNotNullUnsafe(i))
                        m_oFeature.SetFieldSameTypeUnsafe(
                            i, static_cast<char *>(nullptr));
                }
            }
        }
    };

    OGRFeature oFeature(poLayerDefn);
    FeatureCleaner oCleaner(oFeature);

    // We accumulate the content of all strings in osWorkingBuffer to avoid
    // a few dynamic memory allocations
    std::string osWorkingBuffer;

    bool bTransactionOK;
    {
        CPLErrorHandlerPusher oHandler(CPLQuietErrorHandler);
        CPLErrorStateBackuper oBackuper;
        bTransactionOK = StartTransaction() == OGRERR_NONE;
    }

    const std::string emptyString;
    int64_t fidNullCount = 0;
    for (size_t iFeature = 0; iFeature < static_cast<size_t>(array->length);
         ++iFeature)
    {
        oFeature.SetFID(OGRNullFID);

        const size_t nWorkingBufferSize =
            GetWorkingBufferSize(schema, array, iFeature);
        osWorkingBuffer.clear();
        osWorkingBuffer.reserve(nWorkingBufferSize);
#ifdef DEBUG
        const char *pszWorkingBuffer = osWorkingBuffer.c_str();
#endif
        int iArrowIdx = 0;
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!FillFeature(this, schema->children[i], array->children[i],
                             emptyString, iFeature, iArrowIdx, asFieldInfo,
                             oFeature, osWorkingBuffer))
            {
                if (bTransactionOK)
                    RollbackTransaction();
                return false;
            }
        }
#ifdef DEBUG
        // Check that the buffer didn't get reallocated
        CPLAssert(pszWorkingBuffer == osWorkingBuffer.c_str());
        CPLAssert(osWorkingBuffer.size() == nWorkingBufferSize);
#endif

        if (CreateFeature(&oFeature) != OGRERR_NONE)
        {
            if (bTransactionOK)
                RollbackTransaction();
            return false;
        }
        if (arrayFIDColumn)
        {
            uint8_t *pabyValidity = static_cast<uint8_t *>(
                const_cast<void *>(arrayFIDColumn->buffers[0]));
            if (schemaFIDColumn->format[0] == 'i')  // Int32
            {
                auto *panValues = static_cast<int32_t *>(
                    const_cast<void *>(arrayFIDColumn->buffers[1]));
                if (oFeature.GetFID() > std::numeric_limits<int32_t>::max())
                {
                    if (pabyValidity)
                    {
                        ++fidNullCount;
                        UnsetBit(pabyValidity,
                                 static_cast<size_t>(iFeature +
                                                     arrayFIDColumn->offset));
                    }
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "FID " CPL_FRMT_GIB
                             " cannot be stored in FID array of type int32",
                             oFeature.GetFID());
                }
                else
                {
                    if (pabyValidity)
                    {
                        SetBit(pabyValidity,
                               static_cast<size_t>(iFeature +
                                                   arrayFIDColumn->offset));
                    }
                    panValues[iFeature + arrayFIDColumn->offset] =
                        static_cast<int32_t>(oFeature.GetFID());
                }
            }
            else if (schemaFIDColumn->format[0] == 'l')  // Int64
            {
                if (pabyValidity)
                {
                    SetBit(
                        pabyValidity,
                        static_cast<size_t>(iFeature + arrayFIDColumn->offset));
                }
                auto *panValues = static_cast<int64_t *>(
                    const_cast<void *>(arrayFIDColumn->buffers[1]));
                panValues[iFeature + arrayFIDColumn->offset] =
                    oFeature.GetFID();
            }
            else
            {
                CPLAssert(false);
            }
        }
    }
    if (arrayFIDColumn && arrayFIDColumn->buffers[0])
    {
        arrayFIDColumn->null_count = fidNullCount;
    }

    bool bRet = true;
    if (bTransactionOK)
        bRet = CommitTransaction() == OGRERR_NONE;

    return bRet;
}

/************************************************************************/
/*                      OGR_L_WriteArrowBatch()                         */
/************************************************************************/

// clang-format off
/** Writes a batch of rows from an ArrowArray.
 *
 * This is semantically close to calling CreateFeature() with multiple features
 * at once.
 *
 * The ArrowArray must be of type struct (format=+s), and its children generally
 * map to a OGR attribute or geometry field (unless they are struct themselves).
 *
 * Method IsArrowSchemaSupported() can be called to determine if the schema
 * will be supported by WriteArrowBatch().
 *
 * OGR fields for the corresponding children arrays must exist and be of a
 * compatible type. For attribute fields, they should be created with
 * CreateFieldFromArrowSchema(). For geometry fields, they should be created
 * either implicitly at CreateLayer() type (if geom_type != wkbNone), or
 * explicitly with CreateGeomField().
 *
 * Arrays for geometry columns should be of binary or large binary type and
 * contain WKB geometry.
 *
 * Note that the passed array may be set to a released state
 * (array->release==NULL) after this call (not by the base implementation,
 * but in specialized ones such as Parquet or Arrow for example)
 *
 * Supported options of the base implementation are:
 * <ul>
 * <li>FID=name. Name of the FID column in the array. If not provided,
 *     GetFIDColumn() is used to determine it. The special name
 *     OGRLayer::DEFAULT_ARROW_FID_NAME is also recognized if neither FID nor
 *     GetFIDColumn() are set.
 *     The corresponding ArrowArray must be of type int32 (i) or int64 (l).
 *     On input, values of the FID column are used to create the feature.
 *     On output, the values of the FID column may be set with the FID of the
 *     created feature (if the array is not released).
 * </li>
 * <li>GEOMETRY_NAME=name. Name of the geometry column. If not provided,
 *     GetGeometryColumn() is used. The special name
 *     OGRLayer::DEFAULT_ARROW_GEOMETRY_NAME is also recognized if neither
 *     GEOMETRY_NAME nor GetGeometryColumn() are set.
 *     Geometry columns are also identified if they have
 *     ARROW:extension:name=ogc.wkb as a field metadata.
 *     The corresponding ArrowArray must be of type binary (w) or large
 *     binary (W).
 * </li>
 * </ul>
 *
 * The following example demonstrates how to copy a layer from one format to
 * another one (assuming it has at most a single geometry column):
\code{.py}
    def copy_layer(src_lyr, out_filename, out_format, lcos = {}):
        stream = src_lyr.GetArrowStream()
        schema = stream.GetSchema()

        # If the source layer has a FID column and the output driver supports
        # a FID layer creation option, set it to the source FID column name.
        if src_lyr.GetFIDColumn():
            creationOptions = gdal.GetDriverByName(out_format).GetMetadataItem(
                "DS_LAYER_CREATIONOPTIONLIST"
            )
            if creationOptions and '"FID"' in creationOptions:
                lcos["FID"] = src_lyr.GetFIDColumn()

        with ogr.GetDriverByName(out_format).CreateDataSource(out_filename) as out_ds:
            if src_lyr.GetLayerDefn().GetGeomFieldCount() > 1:
                out_lyr = out_ds.CreateLayer(
                    src_lyr.GetName(), geom_type=ogr.wkbNone, options=lcos
                )
                for i in range(src_lyr.GetLayerDefn().GetGeomFieldCount()):
                    out_lyr.CreateGeomField(src_lyr.GetLayerDefn().GetGeomFieldDefn(i))
            else:
                out_lyr = out_ds.CreateLayer(
                    src_lyr.GetName(),
                    geom_type=src_lyr.GetGeomType(),
                    srs=src_lyr.GetSpatialRef(),
                    options=lcos,
                )

            success, error_msg = out_lyr.IsArrowSchemaSupported(schema)
            assert success, error_msg

            src_geom_field_names = [
                src_lyr.GetLayerDefn().GetGeomFieldDefn(i).GetName()
                for i in range(src_lyr.GetLayerDefn().GetGeomFieldCount())
            ]
            for i in range(schema.GetChildrenCount()):
                # GetArrowStream() may return "OGC_FID" for a unnamed source FID
                # column and "wkb_geometry" for a unnamed source geometry column.
                # Also test GetFIDColumn() and src_geom_field_names if they are
                # named.
                if (
                    schema.GetChild(i).GetName()
                    not in ("OGC_FID", "wkb_geometry", src_lyr.GetFIDColumn())
                    and schema.GetChild(i).GetName() not in src_geom_field_names
                ):
                    out_lyr.CreateFieldFromArrowSchema(schema.GetChild(i))

            write_options = []
            if src_lyr.GetFIDColumn():
                write_options.append("FID=" + src_lyr.GetFIDColumn())
            if (
                src_lyr.GetLayerDefn().GetGeomFieldCount() == 1
                and src_lyr.GetGeometryColumn()
            ):
                write_options.append("GEOMETRY_NAME=" + src_lyr.GetGeometryColumn())

            while True:
                array = stream.GetNextRecordBatch()
                if array is None:
                    break
                out_lyr.WriteArrowBatch(schema, array, write_options)
\endcode
 *
 * This method and CreateFeature() are mutually exclusive in the same session.
 *
 * This method is the same as the C++ method OGRLayer::WriteArrowBatch().
 *
 * @param hLayer Layer.
 * @param schema Schema of array.
 * @param array Array of type struct. It may be released (array->release==NULL)
 *              after calling this method.
 * @param papszOptions Options. Null terminated list, or nullptr.
 * @return true in case of success
 * @since 3.8
 */
// clang-format on

bool OGR_L_WriteArrowBatch(OGRLayerH hLayer, const struct ArrowSchema *schema,
                           struct ArrowArray *array, char **papszOptions)
{
    VALIDATE_POINTER1(hLayer, __func__, false);
    VALIDATE_POINTER1(schema, __func__, false);
    VALIDATE_POINTER1(array, __func__, false);

    return OGRLayer::FromHandle(hLayer)->WriteArrowBatch(schema, array,
                                                         papszOptions);
}
