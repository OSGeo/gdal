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
#include "ogr_wkb.h"

#include "cpl_time.h"
#include <cassert>
#include <limits>
#include <set>

constexpr const char *ARROW_EXTENSION_NAME_KEY = "ARROW:extension:name";
constexpr const char *EXTENSION_NAME = "ogc.wkb";

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
            CPLStrdup((pszFIDName && pszFIDName[0]) ? pszFIDName : "OGC_FID");
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
        pszGeomFieldName = "wkb_geometry";
    psSchema->name = CPLStrdup(pszGeomFieldName);
    if (poFieldDefn->IsNullable())
        psSchema->flags = ARROW_FLAG_NULLABLE;
    psSchema->format = strcmp(pszArrowFormat, "z") == 0 ? "z" : "Z";
    char *pszMetadata = static_cast<char *>(CPLMalloc(
        sizeof(int32_t) + sizeof(int32_t) + strlen(ARROW_EXTENSION_NAME_KEY) +
        sizeof(int32_t) + strlen(EXTENSION_NAME)));
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
        static_cast<int32_t>(strlen(EXTENSION_NAME));
    offsetMD += sizeof(int32_t);
    memcpy(pszMetadata + offsetMD, EXTENSION_NAME, strlen(EXTENSION_NAME));
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
/*                           FillArray()                                */
/************************************************************************/

template <class T, typename TMember>
static bool FillArray(struct ArrowArray *psChild,
                      std::vector<std::unique_ptr<OGRFeature>> &apoFeatures,
                      const bool bIsNullable, TMember member, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
                panValues[iFeat / 8] |= static_cast<uint8_t>(1 << (iFeat % 8));
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
                    panValues[(nOffset + j) / 8] |=
                        static_cast<uint8_t>(1 << ((nOffset + j) % 8));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;

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
                if (pabyNull == nullptr)
                {
                    pabyNull =
                        static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                            (apoFeatures.size() + 7) / 8));
                    if (pabyNull == nullptr)
                        return false;
                    memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                    psChild->buffers[0] = pabyNull;
                }
                pabyNull[iFeat / 8] &=
                    static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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
    uint8_t *pabyNull = nullptr;
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
            if (pabyNull == nullptr)
            {
                pabyNull =
                    static_cast<uint8_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                        (apoFeatures.size() + 7) / 8));
                if (pabyNull == nullptr)
                    return false;
                memset(pabyNull, 0xFF, (apoFeatures.size() + 7) / 8);
                psChild->buffers[0] = pabyNull;
            }
            pabyNull[iFeat / 8] &= static_cast<uint8_t>(~(1 << (iFeat % 8)));
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

    ResetReading();

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
/*                       ParseArrowMetadata()                           */
/************************************************************************/

static std::map<std::string, std::string>
ParseArrowMetadata(const char *pabyMetadata)
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
/*                  OGRLayer::CanPostFilterArrowArray()                 */
/************************************************************************/

/** Whether the PostFilterArrowArray() can work on the schema to remove
 * rows that aren't selected by the spatial or attribute filter.
 *
 * Note: only spatial filter implemented for now.
 */
bool OGRLayer::CanPostFilterArrowArray(const struct ArrowSchema *schema) const
{
    if (m_poAttrQuery)
    {
        CPLDebug("OGR",
                 "Cannot post filter ArrowArray with attribute filter set");
        return false;
    }

    if (strcmp(schema->format, "+s") != 0)
    {
        CPLDebug("OGR", "Unexpected top level schema->format = %s",
                 schema->format);
        return false;
    }

    const char *const apszHandledFormats[] = {
        "b",  // boolean
        "c",  // int8
        "C",  // uint8
        "s",  // int16
        "S",  // uint16
        "i",  // int32
        "I",  // uint32
        "l",  // int64
        "L",  // uint64
        "e",  // float16
        "f",  // float32
        "g",  // float64,
        "z",  // binary
        "Z",  // large binary
        "u",  // UTF-8 string
        "U",  // large UTF-8 string
        // "d:xxxxx"  // decimal128, decimal256
        // "w:xxxxx"  // fixed width binary
        "tdD",  // date32[days]
        "tdm",  // date64[milliseconds]
        "tts",  //time32 [seconds]
        "ttm",  //time32 [milliseconds]
        "ttu",  //time64 [microseconds]
        "ttn",  //time64 [nanoseconds]
    };

    const char *const apszHandledFormatsPrefix[] = {
        "tss:",  // timestamp [seconds] with timezone
        "tsm:",  // timestamp [milliseconds] with timezone
        "tsu:",  // timestamp [microseconds] with timezone
        "tsn:",  // timestamp [nanoseconds] with timezone
    };

    for (int64_t i = 0; i < schema->n_children; ++i)
    {
        const auto fieldSchema = schema->children[i];
        bool bFound = false;
        for (const char *pszHandledFormat : apszHandledFormats)
        {
            if (strcmp(fieldSchema->format, pszHandledFormat) == 0)
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            for (const char *pszHandledFormat : apszHandledFormatsPrefix)
            {
                if (strncmp(fieldSchema->format, pszHandledFormat,
                            strlen(pszHandledFormat)) == 0)
                {
                    bFound = true;
                    break;
                }
            }
        }
        if (!bFound)
        {
            CPLDebug("OGR", "Field %s has unhandled format '%s'",
                     fieldSchema->name, fieldSchema->format);
            return false;
        }
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

                const auto oMetadata = ParseArrowMetadata(pabyMetadata);
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
                if (oIter->second != EXTENSION_NAME)
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
/*                    CompactValidityBuffer()                           */
/************************************************************************/

static void
CompactValidityBuffer(struct ArrowArray *array,
                      const std::vector<bool> &abyValidityFromFilters)
{
    if (array->null_count == 0)
        return;
    uint8_t *pabyValidity =
        static_cast<uint8_t *>(const_cast<void *>(array->buffers[0]));
    const size_t nLength = static_cast<size_t>(array->length);
    const size_t nOffset = static_cast<size_t>(array->offset);
    for (size_t i = 0, j = 0; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            if (TestBit(pabyValidity, i + nOffset))
                SetBit(pabyValidity, j + nOffset);
            else
                UnsetBit(pabyValidity, j + nOffset);

            ++j;
        }
    }
}

/************************************************************************/
/*                       CompactBoolArray()                             */
/************************************************************************/

static void CompactBoolArray(struct ArrowArray *array,
                             const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(array->n_children == 0);
    CPLAssert(array->n_buffers == 2);
    CPLAssert(static_cast<size_t>(array->length) ==
              abyValidityFromFilters.size());

    const size_t nLength = static_cast<size_t>(array->length);
    const size_t nOffset = static_cast<size_t>(array->offset);
    uint8_t *pabyData =
        static_cast<uint8_t *>(const_cast<void *>(array->buffers[1]));
    size_t j = 0;
    for (size_t i = 0; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            if (TestBit(pabyData, i + nOffset))
                SetBit(pabyData, j + nOffset);
            else
                UnsetBit(pabyData, j + nOffset);

            ++j;
        }
    }

    CompactValidityBuffer(array, abyValidityFromFilters);
    array->length = j;
}

/************************************************************************/
/*                       CompactPrimitiveArray()                        */
/************************************************************************/

template <class T>
static void
CompactPrimitiveArray(struct ArrowArray *array,
                      const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(array->n_children == 0);
    CPLAssert(array->n_buffers == 2);
    CPLAssert(static_cast<size_t>(array->length) ==
              abyValidityFromFilters.size());

    const size_t nLength = static_cast<size_t>(array->length);
    const size_t nOffset = static_cast<size_t>(array->offset);
    T *paData =
        static_cast<T *>(const_cast<void *>(array->buffers[1])) + nOffset;
    size_t j = 0;
    for (size_t i = 0; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            paData[j] = paData[i];
            ++j;
        }
    }

    CompactValidityBuffer(array, abyValidityFromFilters);
    array->length = j;
}

/************************************************************************/
/*                    CompactStringOrBinaryArray()                      */
/************************************************************************/

template <class OffsetType>
static void
CompactStringOrBinaryArray(struct ArrowArray *array,
                           const std::vector<bool> &abyValidityFromFilters)
{
    CPLAssert(array->n_children == 0);
    CPLAssert(array->n_buffers == 3);
    CPLAssert(static_cast<size_t>(array->length) ==
              abyValidityFromFilters.size());

    const size_t nLength = static_cast<size_t>(array->length);
    const size_t nOffset = static_cast<size_t>(array->offset);
    OffsetType *panOffsets =
        static_cast<OffsetType *>(const_cast<void *>(array->buffers[1])) +
        nOffset;
    GByte *pabyData =
        static_cast<GByte *>(const_cast<void *>(array->buffers[2]));
    size_t j = 0;
    OffsetType nCurOffset = panOffsets[0];
    for (size_t i = 0; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            const auto nStartOffset = panOffsets[i];
            const auto nEndOffset = panOffsets[i + 1];
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

    CompactValidityBuffer(array, abyValidityFromFilters);
    array->length = j;
}

/************************************************************************/
/*                  FillValidityArrayFromWKBArray()                     */
/************************************************************************/

template <class OffsetType>
static size_t
FillValidityArrayFromWKBArray(struct ArrowArray *array,
                              const OGREnvelope &sFilterEnvelope,
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
            if (OGRWKBGetBoundingBox(
                    pabyData + panOffsets[i],
                    static_cast<size_t>(panOffsets[i + 1] - panOffsets[i]),
                    sEnvelope) &&
                sFilterEnvelope.Intersects(sEnvelope))
            {
                abyValidityFromFilters[i] = true;
                nCountIntersecting++;
            }
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
 *
 * Note: only spatial filter implemented for now.
 */
void OGRLayer::PostFilterArrowArray(const struct ArrowSchema *schema,
                                    struct ArrowArray *array) const
{
    if (!m_poFilterGeom)
        return;

    CPLAssert(schema->n_children == array->n_children);

    const char *pszGeomFieldName = const_cast<OGRLayer *>(this)
                                       ->GetLayerDefn()
                                       ->GetGeomFieldDefn(m_iGeomFieldFilter)
                                       ->GetNameRef();
    int64_t iGeomField = -1;
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

    std::vector<bool> abyValidityFromFilters;
    const size_t nCountIntersecting =
        strcmp(schema->children[iGeomField]->format, "z") == 0
            ? FillValidityArrayFromWKBArray<uint32_t>(
                  array->children[iGeomField], m_sFilterEnvelope,
                  abyValidityFromFilters)
            : FillValidityArrayFromWKBArray<uint64_t>(
                  array->children[iGeomField], m_sFilterEnvelope,
                  abyValidityFromFilters);
    const size_t nLength =
        static_cast<size_t>(array->children[iGeomField]->length);
    // Nothing to do ?
    if (nCountIntersecting == nLength)
    {
        // CPLDebug("OGR", "All rows match filter");
        return;
    }

    array->length = nCountIntersecting;

    for (int64_t iField = 0; iField < array->n_children; ++iField)
    {
        const auto psSchemaField = schema->children[iField];
        const auto psArray = array->children[iField];
        const char *format = psSchemaField->format;

        if (strcmp(format, "b") == 0)
        {
            CompactBoolArray(psArray, abyValidityFromFilters);
        }
        else if (strcmp(format, "c") == 0 || strcmp(format, "C") == 0)
        {
            CompactPrimitiveArray<uint8_t>(psArray, abyValidityFromFilters);
        }
        else if (strcmp(format, "s") == 0 || strcmp(format, "S") == 0 ||
                 strcmp(format, "e") == 0)
        {
            CompactPrimitiveArray<uint16_t>(psArray, abyValidityFromFilters);
        }
        else if (strcmp(format, "i") == 0 || strcmp(format, "I") == 0 ||
                 strcmp(format, "f") == 0 || strcmp(format, "tdD") == 0 ||
                 strcmp(format, "tts") == 0 || strcmp(format, "ttm") == 0)
        {
            CompactPrimitiveArray<uint32_t>(psArray, abyValidityFromFilters);
        }
        else if (strcmp(format, "l") == 0 || strcmp(format, "L") == 0 ||
                 strcmp(format, "g") == 0 || strcmp(format, "tdm") == 0 ||
                 strcmp(format, "ttu") == 0 || strcmp(format, "ttn") == 0 ||
                 strncmp(format, "ts", 2) == 0)
        {
            CompactPrimitiveArray<uint64_t>(psArray, abyValidityFromFilters);
        }
        else if (strcmp(format, "z") == 0 || strcmp(format, "u") == 0)
        {
            CompactStringOrBinaryArray<uint32_t>(psArray,
                                                 abyValidityFromFilters);
        }
        else if (strcmp(format, "Z") == 0 || strcmp(format, "U") == 0)
        {
            CompactStringOrBinaryArray<uint64_t>(psArray,
                                                 abyValidityFromFilters);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected error in PostFilterArrowArray(): unhandled "
                     "field format: %s",
                     format);

            array->release(array);
            memset(array, 0, sizeof(*array));

            break;
        }

        CPLAssert(psArray->length == array->length);
    }
}
