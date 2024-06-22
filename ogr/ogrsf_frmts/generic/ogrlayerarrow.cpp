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
#include "ogr_p.h"
#include "ogrlayer_private.h"

#include "cpl_float.h"
#include "cpl_json.h"
#include "cpl_time.h"
#include <cassert>
#include <cinttypes>
#include <limits>
#include <utility>
#include <set>

constexpr const char *MD_GDAL_OGR_ALTERNATIVE_NAME =
    "GDAL:OGR:alternative_name";
constexpr const char *MD_GDAL_OGR_COMMENT = "GDAL:OGR:comment";
constexpr const char *MD_GDAL_OGR_DEFAULT = "GDAL:OGR:default";
constexpr const char *MD_GDAL_OGR_SUBTYPE = "GDAL:OGR:subtype";
constexpr const char *MD_GDAL_OGR_WIDTH = "GDAL:OGR:width";
constexpr const char *MD_GDAL_OGR_UNIQUE = "GDAL:OGR:unique";
constexpr const char *MD_GDAL_OGR_DOMAIN_NAME = "GDAL:OGR:domain_name";

constexpr char ARROW_LETTER_BOOLEAN = 'b';
constexpr char ARROW_LETTER_INT8 = 'c';
constexpr char ARROW_LETTER_UINT8 = 'C';
constexpr char ARROW_LETTER_INT16 = 's';
constexpr char ARROW_LETTER_UINT16 = 'S';
constexpr char ARROW_LETTER_INT32 = 'i';
constexpr char ARROW_LETTER_UINT32 = 'I';
constexpr char ARROW_LETTER_INT64 = 'l';
constexpr char ARROW_LETTER_UINT64 = 'L';
constexpr char ARROW_LETTER_FLOAT16 = 'e';
constexpr char ARROW_LETTER_FLOAT32 = 'f';
constexpr char ARROW_LETTER_FLOAT64 = 'g';
constexpr char ARROW_LETTER_STRING = 'u';
constexpr char ARROW_LETTER_LARGE_STRING = 'U';
constexpr char ARROW_LETTER_BINARY = 'z';
constexpr char ARROW_LETTER_LARGE_BINARY = 'Z';
constexpr char ARROW_LETTER_DECIMAL = 'd';
constexpr char ARROW_2ND_LETTER_LIST = 'l';
constexpr char ARROW_2ND_LETTER_LARGE_LIST = 'L';

static inline bool IsStructure(const char *format)
{
    return format[0] == '+' && format[1] == 's' && format[2] == 0;
}

static inline bool IsMap(const char *format)
{
    return format[0] == '+' && format[1] == 'm' && format[2] == 0;
}

static inline bool IsFixedWidthBinary(const char *format)
{
    return format[0] == 'w' && format[1] == ':';
}

static inline int GetFixedWithBinary(const char *format)
{
    return atoi(format + strlen("w:"));
}

static inline bool IsList(const char *format)
{
    return format[0] == '+' && format[1] == ARROW_2ND_LETTER_LIST &&
           format[2] == 0;
}

static inline bool IsLargeList(const char *format)
{
    return format[0] == '+' && format[1] == ARROW_2ND_LETTER_LARGE_LIST &&
           format[2] == 0;
}

static inline bool IsFixedSizeList(const char *format)
{
    return format[0] == '+' && format[1] == 'w' && format[2] == ':';
}

static inline int GetFixedSizeList(const char *format)
{
    return atoi(format + strlen("+w:"));
}

static inline bool IsDecimal(const char *format)
{
    return format[0] == ARROW_LETTER_DECIMAL && format[1] == ':';
}

static inline bool IsBoolean(const char *format)
{
    return format[0] == ARROW_LETTER_BOOLEAN && format[1] == 0;
}

static inline bool IsInt8(const char *format)
{
    return format[0] == ARROW_LETTER_INT8 && format[1] == 0;
}

static inline bool IsUInt8(const char *format)
{
    return format[0] == ARROW_LETTER_UINT8 && format[1] == 0;
}

static inline bool IsInt16(const char *format)
{
    return format[0] == ARROW_LETTER_INT16 && format[1] == 0;
}

static inline bool IsUInt16(const char *format)
{
    return format[0] == ARROW_LETTER_UINT16 && format[1] == 0;
}

static inline bool IsInt32(const char *format)
{
    return format[0] == ARROW_LETTER_INT32 && format[1] == 0;
}

static inline bool IsUInt32(const char *format)
{
    return format[0] == ARROW_LETTER_UINT32 && format[1] == 0;
}

static inline bool IsInt64(const char *format)
{
    return format[0] == ARROW_LETTER_INT64 && format[1] == 0;
}

static inline bool IsUInt64(const char *format)
{
    return format[0] == ARROW_LETTER_UINT64 && format[1] == 0;
}

static inline bool IsFloat16(const char *format)
{
    return format[0] == ARROW_LETTER_FLOAT16 && format[1] == 0;
}

static inline bool IsFloat32(const char *format)
{
    return format[0] == ARROW_LETTER_FLOAT32 && format[1] == 0;
}

static inline bool IsFloat64(const char *format)
{
    return format[0] == ARROW_LETTER_FLOAT64 && format[1] == 0;
}

static inline bool IsString(const char *format)
{
    return format[0] == ARROW_LETTER_STRING && format[1] == 0;
}

static inline bool IsLargeString(const char *format)
{
    return format[0] == ARROW_LETTER_LARGE_STRING && format[1] == 0;
}

static inline bool IsBinary(const char *format)
{
    return format[0] == ARROW_LETTER_BINARY && format[1] == 0;
}

static inline bool IsLargeBinary(const char *format)
{
    return format[0] == ARROW_LETTER_LARGE_BINARY && format[1] == 0;
}

static inline bool IsTimestampInternal(const char *format, char chType)
{
    return format[0] == 't' && format[1] == 's' && format[2] == chType &&
           format[3] == ':';
}

static inline bool IsTimestampSeconds(const char *format)
{
    return IsTimestampInternal(format, 's');
}

static inline bool IsTimestampMilliseconds(const char *format)
{
    return IsTimestampInternal(format, 'm');
}

static inline bool IsTimestampMicroseconds(const char *format)
{
    return IsTimestampInternal(format, 'u');
}

static inline bool IsTimestampNanoseconds(const char *format)
{
    return IsTimestampInternal(format, 'n');
}

static inline bool IsTimestamp(const char *format)
{
    return IsTimestampSeconds(format) || IsTimestampMilliseconds(format) ||
           IsTimestampMicroseconds(format) || IsTimestampNanoseconds(format);
}

static inline const char *GetTimestampTimezone(const char *format)
{
    return IsTimestamp(format) ? format + strlen("tm?:") : "";
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
/*                          DefaultReleaseSchema()                      */
/************************************************************************/

static void OGRLayerDefaultReleaseSchema(struct ArrowSchema *schema)
{
    CPLAssert(schema->release != nullptr);
    if (STARTS_WITH(schema->format, "w:") ||
        STARTS_WITH(schema->format, "tsm:"))
    {
        CPLFree(const_cast<char *>(schema->format));
    }
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
            {
                const char *pszPrefix = "tsm:";
                const char *pszTZOverride =
                    m_aosArrowArrayStreamOptions.FetchNameValue("TIMEZONE");
                if (pszTZOverride && EQUAL(pszTZOverride, "unknown"))
                {
                    psChild->format = CPLStrdup(pszPrefix);
                }
                else if (pszTZOverride)
                {
                    psChild->format = CPLStrdup(
                        (std::string(pszPrefix) + pszTZOverride).c_str());
                }
                else
                {
                    const int nTZFlag = poFieldDefn->GetTZFlag();
                    if (nTZFlag == OGR_TZFLAG_MIXED_TZ ||
                        nTZFlag == OGR_TZFLAG_UTC)
                    {
                        psChild->format =
                            CPLStrdup(CPLSPrintf("%sUTC", pszPrefix));
                    }
                    else if (nTZFlag == OGR_TZFLAG_UNKNOWN ||
                             nTZFlag == OGR_TZFLAG_LOCALTIME)
                    {
                        psChild->format = CPLStrdup(pszPrefix);
                    }
                    else
                    {
                        psChild->format = CPLStrdup(
                            (pszPrefix + OGRTZFlagToTimezone(nTZFlag, "UTC"))
                                .c_str());
                    }
                }
                break;
            }
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

        std::vector<std::pair<std::string, std::string>> oMetadata;
        const char *pszAlternativeName = poFieldDefn->GetAlternativeNameRef();
        if (pszAlternativeName && pszAlternativeName[0])
            oMetadata.emplace_back(
                std::pair(MD_GDAL_OGR_ALTERNATIVE_NAME, pszAlternativeName));

        const char *pszDefault = poFieldDefn->GetDefault();
        if (pszDefault && pszDefault[0])
            oMetadata.emplace_back(std::pair(MD_GDAL_OGR_DEFAULT, pszDefault));

        const std::string &osComment = poFieldDefn->GetComment();
        if (!osComment.empty())
            oMetadata.emplace_back(std::pair(MD_GDAL_OGR_COMMENT, osComment));

        if (poFieldDefn->GetSubType() != OFSTNone &&
            poFieldDefn->GetSubType() != OFSTBoolean &&
            poFieldDefn->GetSubType() != OFSTFloat32)
        {
            oMetadata.emplace_back(
                std::pair(MD_GDAL_OGR_SUBTYPE,
                          OGR_GetFieldSubTypeName(poFieldDefn->GetSubType())));
        }
        if (poFieldDefn->GetType() == OFTString && poFieldDefn->GetWidth() > 0)
        {
            oMetadata.emplace_back(std::pair(
                MD_GDAL_OGR_WIDTH, CPLSPrintf("%d", poFieldDefn->GetWidth())));
        }
        if (poFieldDefn->IsUnique())
        {
            oMetadata.emplace_back(std::pair(MD_GDAL_OGR_UNIQUE, "true"));
        }
        if (!poFieldDefn->GetDomainName().empty())
        {
            oMetadata.emplace_back(std::pair(MD_GDAL_OGR_DOMAIN_NAME,
                                             poFieldDefn->GetDomainName()));
        }

        if (!oMetadata.empty())
        {
            uint64_t nLen64 = sizeof(int32_t);
            for (const auto &oPair : oMetadata)
            {
                nLen64 += sizeof(int32_t);
                nLen64 += oPair.first.size();
                nLen64 += sizeof(int32_t);
                nLen64 += oPair.second.size();
            }
            if (nLen64 <
                static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
            {
                const size_t nLen = static_cast<size_t>(nLen64);
                char *pszMetadata = static_cast<char *>(CPLMalloc(nLen));
                psChild->metadata = pszMetadata;
                size_t offsetMD = 0;
                int32_t nSize = static_cast<int>(oMetadata.size());
                memcpy(pszMetadata + offsetMD, &nSize, sizeof(nSize));
                offsetMD += sizeof(int32_t);
                for (const auto &oPair : oMetadata)
                {
                    nSize = static_cast<int32_t>(oPair.first.size());
                    memcpy(pszMetadata + offsetMD, &nSize, sizeof(nSize));
                    offsetMD += sizeof(int32_t);
                    memcpy(pszMetadata + offsetMD, oPair.first.data(),
                           oPair.first.size());
                    offsetMD += oPair.first.size();

                    nSize = static_cast<int32_t>(oPair.second.size());
                    memcpy(pszMetadata + offsetMD, &nSize, sizeof(nSize));
                    offsetMD += sizeof(int32_t);
                    memcpy(pszMetadata + offsetMD, oPair.second.data(),
                           oPair.second.size());
                    offsetMD += oPair.second.size();
                }

                CPLAssert(offsetMD == nLen);
                CPL_IGNORE_RET_VAL(offsetMD);
            }
            else
            {
                // Extremely unlikely !
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot write ArrowSchema::metadata due to "
                         "too large content");
            }
        }
    }

    const char *const pszGeometryMetadataEncoding =
        m_aosArrowArrayStreamOptions.FetchNameValue(
            "GEOMETRY_METADATA_ENCODING");
    const char *pszExtensionName = EXTENSION_NAME_OGC_WKB;
    if (pszGeometryMetadataEncoding)
    {
        if (EQUAL(pszGeometryMetadataEncoding, "OGC"))
            pszExtensionName = EXTENSION_NAME_OGC_WKB;
        else if (EQUAL(pszGeometryMetadataEncoding, "GEOARROW"))
            pszExtensionName = EXTENSION_NAME_GEOARROW_WKB;
        else
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Unsupported GEOMETRY_METADATA_ENCODING value: %s",
                     pszGeometryMetadataEncoding);
    }
    for (int i = 0; i < nGeomFieldCount; ++i)
    {
        const auto poFieldDefn = poLayerDefn->GetGeomFieldDefn(i);
        if (poFieldDefn->IsIgnored())
        {
            continue;
        }

        out_schema->children[iSchemaChild] = CreateSchemaForWKBGeometryColumn(
            poFieldDefn, "z", pszExtensionName);

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
                                           const char *pszArrowFormat,
                                           const char *pszExtensionName)
{
    CPLAssert(strcmp(pszArrowFormat, "z") == 0 ||
              strcmp(pszArrowFormat, "Z") == 0);
    if (!EQUAL(pszExtensionName, EXTENSION_NAME_OGC_WKB) &&
        !EQUAL(pszExtensionName, EXTENSION_NAME_GEOARROW_WKB))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Unsupported extension name '%s'. Defaulting to '%s'",
                 pszExtensionName, EXTENSION_NAME_OGC_WKB);
        pszExtensionName = EXTENSION_NAME_OGC_WKB;
    }
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
    std::string osExtensionMetadata;
    if (EQUAL(pszExtensionName, EXTENSION_NAME_GEOARROW_WKB))
    {
        const auto poSRS = poFieldDefn->GetSpatialRef();
        if (poSRS)
        {
            char *pszPROJJSON = nullptr;
            poSRS->exportToPROJJSON(&pszPROJJSON, nullptr);
            if (pszPROJJSON)
            {
                osExtensionMetadata = "{\"crs\":";
                osExtensionMetadata += pszPROJJSON;
                osExtensionMetadata += '}';
                CPLFree(pszPROJJSON);
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Cannot export CRS of geometry field %s to PROJJSON",
                         poFieldDefn->GetNameRef());
            }
        }
    }
    size_t nLen = sizeof(int32_t) + sizeof(int32_t) +
                  strlen(ARROW_EXTENSION_NAME_KEY) + sizeof(int32_t) +
                  strlen(pszExtensionName);
    if (!osExtensionMetadata.empty())
    {
        nLen += sizeof(int32_t) + strlen(ARROW_EXTENSION_METADATA_KEY) +
                sizeof(int32_t) + osExtensionMetadata.size();
    }
    char *pszMetadata = static_cast<char *>(CPLMalloc(nLen));
    psSchema->metadata = pszMetadata;
    size_t offsetMD = 0;
    *reinterpret_cast<int32_t *>(pszMetadata + offsetMD) =
        osExtensionMetadata.empty() ? 1 : 2;
    offsetMD += sizeof(int32_t);
    *reinterpret_cast<int32_t *>(pszMetadata + offsetMD) =
        static_cast<int32_t>(strlen(ARROW_EXTENSION_NAME_KEY));
    offsetMD += sizeof(int32_t);
    memcpy(pszMetadata + offsetMD, ARROW_EXTENSION_NAME_KEY,
           strlen(ARROW_EXTENSION_NAME_KEY));
    offsetMD += static_cast<int>(strlen(ARROW_EXTENSION_NAME_KEY));
    *reinterpret_cast<int32_t *>(pszMetadata + offsetMD) =
        static_cast<int32_t>(strlen(pszExtensionName));
    offsetMD += sizeof(int32_t);
    memcpy(pszMetadata + offsetMD, pszExtensionName, strlen(pszExtensionName));
    offsetMD += strlen(pszExtensionName);
    if (!osExtensionMetadata.empty())
    {
        *reinterpret_cast<int32_t *>(pszMetadata + offsetMD) =
            static_cast<int32_t>(strlen(ARROW_EXTENSION_METADATA_KEY));
        offsetMD += sizeof(int32_t);
        memcpy(pszMetadata + offsetMD, ARROW_EXTENSION_METADATA_KEY,
               strlen(ARROW_EXTENSION_METADATA_KEY));
        offsetMD += static_cast<int>(strlen(ARROW_EXTENSION_METADATA_KEY));
        *reinterpret_cast<int32_t *>(pszMetadata + offsetMD) =
            static_cast<int32_t>(osExtensionMetadata.size());
        offsetMD += sizeof(int32_t);
        memcpy(pszMetadata + offsetMD, osExtensionMetadata.c_str(),
               osExtensionMetadata.size());
        offsetMD += osExtensionMetadata.size();
    }
    CPLAssert(offsetMD == nLen);
    CPL_IGNORE_RET_VAL(offsetMD);
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
    if (array->buffers)
    {
        for (int i = 0; i < static_cast<int>(array->n_buffers); ++i)
            VSIFreeAligned(const_cast<void *>(array->buffers[i]));
        CPLFree(array->buffers);
    }
    if (array->children)
    {
        for (int i = 0; i < static_cast<int>(array->n_children); ++i)
        {
            if (array->children[i] && array->children[i]->release)
            {
                array->children[i]->release(array->children[i]);
                CPLFree(array->children[i]);
            }
        }
        CPLFree(array->children);
    }
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
                      std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                      const size_t nFeatureCountLimit, const bool bIsNullable,
                      TMember member, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    T *panValues = static_cast<T *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * nFeatureCountLimit));
    if (panValues == nullptr)
        return false;
    psChild->buffers[1] = panValues;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat)
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
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
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
                          std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const size_t nFeatureCountLimit,
                          const bool bIsNullable, TMember member, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    uint8_t *panValues = static_cast<uint8_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE((nFeatureCountLimit + 7) / 8));
    if (panValues == nullptr)
        return false;
    memset(panValues, 0, (nFeatureCountLimit + 7) / 8);
    psChild->buffers[1] = panValues;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat)
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
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
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
static size_t
FillListArray(struct ArrowArray *psChild,
              std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
              const size_t nFeatureCountLimit, const bool bIsNullable,
              const int i, const size_t nMemLimit)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    OffsetType *panOffsets =
        static_cast<OffsetType *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            sizeof(OffsetType) * (1 + nFeatureCountLimit)));
    if (panOffsets == nullptr)
        return 0;
    psChild->buffers[1] = panOffsets;

    OffsetType nOffset = 0;
    size_t nFeatCount = 0;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat, ++nFeatCount)
    {
        panOffsets[iFeat] = nOffset;
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const unsigned nCount = GetFromList::getCount(psRawField);
            if (nCount > static_cast<size_t>(nMemLimit - nOffset))
            {
                if (nFeatCount == 0)
                    return 0;
                break;
            }
            nOffset += static_cast<OffsetType>(nCount);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return 0;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[nFeatCount] = nOffset;

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
        return 0;
    psValueChild->buffers[1] = panValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < nFeatCount; ++iFeat)
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

    return nFeatCount;
}

template <class OffsetType, class GetFromList>
static size_t
FillListArrayBool(struct ArrowArray *psChild,
                  std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                  const size_t nFeatureCountLimit, const bool bIsNullable,
                  const int i, const size_t nMemLimit)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    OffsetType *panOffsets =
        static_cast<OffsetType *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            sizeof(OffsetType) * (1 + nFeatureCountLimit)));
    if (panOffsets == nullptr)
        return 0;
    psChild->buffers[1] = panOffsets;

    OffsetType nOffset = 0;
    size_t nFeatCount = 0;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat, ++nFeatCount)
    {
        panOffsets[iFeat] = nOffset;
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const unsigned nCount = GetFromList::getCount(psRawField);
            if (nCount > static_cast<size_t>(nMemLimit - nOffset))
            {
                if (nFeatCount == 0)
                    return 0;
                break;
            }
            nOffset += static_cast<OffsetType>(nCount);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return 0;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[nFeatCount] = nOffset;

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
        return 0;
    memset(panValues, 0, (nOffset + 7) / 8);
    psValueChild->buffers[1] = panValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat)
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

    return nFeatCount;
}

/************************************************************************/
/*                        FillStringArray()                             */
/************************************************************************/

template <class T>
static size_t
FillStringArray(struct ArrowArray *psChild,
                std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                const size_t nFeatureCountLimit, const bool bIsNullable,
                const int i, const size_t nMemLimit)
{
    psChild->n_buffers = 3;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    T *panOffsets = static_cast<T *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * (1 + nFeatureCountLimit)));
    if (panOffsets == nullptr)
        return 0;
    psChild->buffers[1] = panOffsets;

    size_t nOffset = 0;
    size_t nFeatCount = 0;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat, ++nFeatCount)
    {
        panOffsets[iFeat] = static_cast<T>(nOffset);
        const auto psRawField = apoFeatures[iFeat]->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const size_t nLen = strlen(psRawField->String);
            if (nLen > nMemLimit - nOffset)
            {
                if (nFeatCount == 0)
                    return 0;
                break;
            }
            nOffset += static_cast<T>(nLen);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return 0;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[nFeatCount] = static_cast<T>(nOffset);

    char *pachValues =
        static_cast<char *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nOffset));
    if (pachValues == nullptr)
        return 0;
    psChild->buffers[2] = pachValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < nFeatCount; ++iFeat)
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

    return nFeatCount;
}

/************************************************************************/
/*                        FillStringListArray()                         */
/************************************************************************/

template <class OffsetType>
static size_t
FillStringListArray(struct ArrowArray *psChild,
                    std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                    const size_t nFeatureCountLimit, const bool bIsNullable,
                    const int i, const size_t nMemLimit)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    OffsetType *panOffsets =
        static_cast<OffsetType *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
            sizeof(OffsetType) * (1 + nFeatureCountLimit)));
    if (panOffsets == nullptr)
        return false;
    psChild->buffers[1] = panOffsets;

    OffsetType nStrings = 0;
    OffsetType nCountChars = 0;
    size_t nFeatCount = 0;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat, ++nFeatCount)
    {
        panOffsets[iFeat] = nStrings;
        auto &poFeature = apoFeatures[iFeat];
        const auto psRawField = poFeature->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const int nCount = psRawField->StringList.nCount;
            if (static_cast<size_t>(nCount) >
                static_cast<size_t>(nMemLimit - nStrings))
            {
                if (nFeatCount == 0)
                    return 0;
                goto after_loop;
            }
            for (int j = 0; j < nCount; ++j)
            {
                const size_t nLen = strlen(psRawField->StringList.paList[j]);
                if (nLen > static_cast<size_t>(nMemLimit - nCountChars))
                {
                    if (nFeatCount == 0)
                        return 0;
                    goto after_loop;
                }
                nCountChars += static_cast<OffsetType>(nLen);
            }
            nStrings += static_cast<OffsetType>(nCount);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return 0;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
after_loop:
    panOffsets[nFeatCount] = nStrings;

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
        return 0;
    psValueChild->buffers[1] = panChildOffsets;

    char *pachValues =
        static_cast<char *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nCountChars));
    if (pachValues == nullptr)
        return 0;
    psValueChild->buffers[2] = pachValues;

    nStrings = 0;
    nCountChars = 0;
    for (size_t iFeat = 0; iFeat < nFeatCount; ++iFeat)
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

    return nFeatCount;
}

/************************************************************************/
/*                        FillBinaryArray()                             */
/************************************************************************/

template <class T>
static size_t
FillBinaryArray(struct ArrowArray *psChild,
                std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                const size_t nFeatureCountLimit, const bool bIsNullable,
                const int i, const size_t nMemLimit)
{
    psChild->n_buffers = 3;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    T *panOffsets = static_cast<T *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * (1 + nFeatureCountLimit)));
    if (panOffsets == nullptr)
        return 0;
    psChild->buffers[1] = panOffsets;

    T nOffset = 0;
    size_t nFeatCount = 0;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat, ++nFeatCount)
    {
        panOffsets[iFeat] = nOffset;
        const auto psRawField = apoFeatures[iFeat]->GetRawFieldRef(i);
        if (IsValidField(psRawField))
        {
            const size_t nLen = psRawField->Binary.nCount;
            if (nLen > static_cast<size_t>(nMemLimit - nOffset))
            {
                if (iFeat == 0)
                    return 0;
                break;
            }
            nOffset += static_cast<T>(nLen);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return 0;
            }
            UnsetBit(pabyValidity, iFeat);
        }
    }
    panOffsets[nFeatCount] = nOffset;

    GByte *pabyValues =
        static_cast<GByte *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nOffset));
    if (pabyValues == nullptr)
        return 0;
    psChild->buffers[2] = pabyValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < nFeatCount; ++iFeat)
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

    return nFeatCount;
}

/************************************************************************/
/*                     FillFixedWidthBinaryArray()                      */
/************************************************************************/

static bool
FillFixedWidthBinaryArray(struct ArrowArray *psChild,
                          std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const size_t nFeatureCountLimit,
                          const bool bIsNullable, const int nWidth, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;

    assert(nFeatureCountLimit <= std::numeric_limits<size_t>::max() / nWidth);
    GByte *pabyValues = static_cast<GByte *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nFeatureCountLimit * nWidth));
    if (pabyValues == nullptr)
        return false;
    psChild->buffers[1] = pabyValues;

    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat)
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
                    pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
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
static size_t
FillWKBGeometryArray(struct ArrowArray *psChild,
                     std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                     const size_t nFeatureCountLimit,
                     const OGRGeomFieldDefn *poFieldDefn, const int i,
                     const size_t nMemLimit)
{
    const bool bIsNullable = CPL_TO_BOOL(poFieldDefn->IsNullable());
    psChild->n_buffers = 3;
    psChild->buffers = static_cast<const void **>(CPLCalloc(3, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    T *panOffsets = static_cast<T *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(T) * (1 + nFeatureCountLimit)));
    if (panOffsets == nullptr)
        return 0;
    psChild->buffers[1] = panOffsets;
    const auto eGeomType = poFieldDefn->GetType();
    auto poEmptyGeom =
        std::unique_ptr<OGRGeometry>(OGRGeometryFactory::createGeometry(
            (eGeomType == wkbNone || wkbFlatten(eGeomType) == wkbUnknown)
                ? wkbGeometryCollection
                : eGeomType));

    size_t nOffset = 0;
    size_t nFeatCount = 0;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat, ++nFeatCount)
    {
        panOffsets[iFeat] = static_cast<T>(nOffset);
        const auto poGeom = apoFeatures[iFeat]->GetGeomFieldRef(i);
        if (poGeom != nullptr)
        {
            const size_t nLen = poGeom->WkbSize();
            if (nLen > nMemLimit - nOffset)
            {
                if (nFeatCount == 0)
                    return 0;
                break;
            }
            nOffset += static_cast<T>(nLen);
        }
        else if (bIsNullable)
        {
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
                psChild->buffers[0] = pabyValidity;
                if (pabyValidity == nullptr)
                    return 0;
            }
            UnsetBit(pabyValidity, iFeat);
        }
        else if (poEmptyGeom)
        {
            const size_t nLen = poEmptyGeom->WkbSize();
            if (nLen > nMemLimit - nOffset)
            {
                if (nFeatCount == 0)
                    return 0;
                break;
            }
            nOffset += static_cast<T>(nLen);
        }
    }
    panOffsets[nFeatCount] = static_cast<T>(nOffset);

    GByte *pabyValues =
        static_cast<GByte *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nOffset));
    if (pabyValues == nullptr)
        return 0;
    psChild->buffers[2] = pabyValues;

    nOffset = 0;
    for (size_t iFeat = 0; iFeat < nFeatCount; ++iFeat)
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

    return nFeatCount;
}

/************************************************************************/
/*                        FillDateArray()                               */
/************************************************************************/

static bool FillDateArray(struct ArrowArray *psChild,
                          std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const size_t nFeatureCountLimit,
                          const bool bIsNullable, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    int32_t *panValues = static_cast<int32_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(int32_t) * nFeatureCountLimit));
    if (panValues == nullptr)
        return false;
    psChild->buffers[1] = panValues;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat)
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
            panValues[iFeat] =
                static_cast<int>(CPLYMDHMSToUnixTime(&brokenDown) / 86400);
        }
        else if (bIsNullable)
        {
            panValues[iFeat] = 0;
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
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
/*                        FillTimeArray()                               */
/************************************************************************/

static bool FillTimeArray(struct ArrowArray *psChild,
                          std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                          const size_t nFeatureCountLimit,
                          const bool bIsNullable, const int i)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    int32_t *panValues = static_cast<int32_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(int32_t) * nFeatureCountLimit));
    if (panValues == nullptr)
        return false;
    psChild->buffers[1] = panValues;
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat)
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
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
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
                  std::deque<std::unique_ptr<OGRFeature>> &apoFeatures,
                  const size_t nFeatureCountLimit, const bool bIsNullable,
                  const int i, int nFieldTZFlag)
{
    psChild->n_buffers = 2;
    psChild->buffers = static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
    uint8_t *pabyValidity = nullptr;
    int64_t *panValues = static_cast<int64_t *>(
        VSI_MALLOC_ALIGNED_AUTO_VERBOSE(sizeof(int64_t) * nFeatureCountLimit));
    if (panValues == nullptr)
        return false;
    psChild->buffers[1] = panValues;
    struct tm brokenDown;
    memset(&brokenDown, 0, sizeof(brokenDown));
    for (size_t iFeat = 0; iFeat < nFeatureCountLimit; ++iFeat)
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
            auto nVal =
                CPLYMDHMSToUnixTime(&brokenDown) * 1000 +
                (static_cast<int>(psRawField->Date.Second * 1000 + 0.5) % 1000);
            if (nFieldTZFlag > OGR_TZFLAG_MIXED_TZ &&
                psRawField->Date.TZFlag > OGR_TZFLAG_MIXED_TZ)
            {
                // Convert for psRawField->Date.TZFlag to nFieldTZFlag
                const int TZOffset =
                    (psRawField->Date.TZFlag - nFieldTZFlag) * 15;
                const int TZOffsetMS = TZOffset * 60 * 1000;
                nVal -= TZOffsetMS;
            }
            else if (nFieldTZFlag == OGR_TZFLAG_MIXED_TZ &&
                     psRawField->Date.TZFlag > OGR_TZFLAG_MIXED_TZ)
            {
                // Convert for psRawField->Date.TZFlag to UTC
                const int TZOffset =
                    (psRawField->Date.TZFlag - OGR_TZFLAG_UTC) * 15;
                const int TZOffsetMS = TZOffset * 60 * 1000;
                nVal -= TZOffsetMS;
            }
            panValues[iFeat] = nVal;
        }
        else if (bIsNullable)
        {
            panValues[iFeat] = 0;
            ++psChild->null_count;
            if (pabyValidity == nullptr)
            {
                pabyValidity = AllocValidityBitmap(nFeatureCountLimit);
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
int OGRLayer::GetNextArrowArray(struct ArrowArrayStream *stream,
                                struct ArrowArray *out_array)
{
    ArrowArrayStreamPrivateDataSharedDataWrapper *poPrivate =
        static_cast<ArrowArrayStreamPrivateDataSharedDataWrapper *>(
            stream->private_data);

    const bool bIncludeFID = CPLTestBool(
        m_aosArrowArrayStreamOptions.FetchNameValueDef("INCLUDE_FID", "YES"));
    int nMaxBatchSize = atoi(m_aosArrowArrayStreamOptions.FetchNameValueDef(
        "MAX_FEATURES_IN_BATCH", "65536"));
    if (nMaxBatchSize <= 0)
        nMaxBatchSize = 1;
    if (nMaxBatchSize > INT_MAX - 1)
        nMaxBatchSize = INT_MAX - 1;

    auto &oFeatureQueue =
        m_poSharedArrowArrayStreamPrivateData->m_oFeatureQueue;

    memset(out_array, 0, sizeof(*out_array));

    auto poLayerDefn = GetLayerDefn();
    const int nFieldCount = poLayerDefn->GetFieldCount();
    const int nGeomFieldCount = poLayerDefn->GetGeomFieldCount();
    const int nMaxChildren =
        (bIncludeFID ? 1 : 0) + nFieldCount + nGeomFieldCount;
    int iSchemaChild = 0;

    if (!m_poSharedArrowArrayStreamPrivateData->m_anQueriedFIDs.empty())
    {
        if (poPrivate->poShared->m_bEOF)
        {
            return 0;
        }
        if (m_poSharedArrowArrayStreamPrivateData->m_iQueriedFIDS == 0)
        {
            CPLDebug("OGR", "Using fast FID filtering");
        }
        while (
            oFeatureQueue.size() < static_cast<size_t>(nMaxBatchSize) &&
            m_poSharedArrowArrayStreamPrivateData->m_iQueriedFIDS <
                m_poSharedArrowArrayStreamPrivateData->m_anQueriedFIDs.size())
        {
            const auto nFID =
                m_poSharedArrowArrayStreamPrivateData->m_anQueriedFIDs
                    [m_poSharedArrowArrayStreamPrivateData->m_iQueriedFIDS];
            auto poFeature = std::unique_ptr<OGRFeature>(GetFeature(nFID));
            ++m_poSharedArrowArrayStreamPrivateData->m_iQueriedFIDS;
            if (poFeature && (m_poFilterGeom == nullptr ||
                              FilterGeometry(poFeature->GetGeomFieldRef(
                                  m_iGeomFieldFilter))))
            {
                oFeatureQueue.emplace_back(std::move(poFeature));
            }
        }
        if (m_poSharedArrowArrayStreamPrivateData->m_iQueriedFIDS ==
            m_poSharedArrowArrayStreamPrivateData->m_anQueriedFIDs.size())
        {
            poPrivate->poShared->m_bEOF = true;
        }
    }
    else if (!poPrivate->poShared->m_bEOF)
    {
        while (oFeatureQueue.size() < static_cast<size_t>(nMaxBatchSize))
        {
            auto poFeature = std::unique_ptr<OGRFeature>(GetNextFeature());
            if (!poFeature)
            {
                poPrivate->poShared->m_bEOF = true;
                break;
            }
            oFeatureQueue.emplace_back(std::move(poFeature));
        }
    }
    if (oFeatureQueue.empty())
    {
        return 0;
    }

    out_array->release = OGRLayerDefaultReleaseArray;
    out_array->null_count = 0;

    out_array->n_children = nMaxChildren;
    out_array->children = static_cast<struct ArrowArray **>(
        CPLCalloc(nMaxChildren, sizeof(struct ArrowArray *)));
    out_array->release = OGRLayerDefaultReleaseArray;
    out_array->n_buffers = 1;
    out_array->buffers =
        static_cast<const void **>(CPLCalloc(1, sizeof(void *)));

    size_t nFeatureCount = oFeatureQueue.size();
    const uint32_t nMemLimit = OGRArrowArrayHelper::GetMemLimit();
    if (bIncludeFID)
    {
        out_array->children[iSchemaChild] = static_cast<struct ArrowArray *>(
            CPLCalloc(1, sizeof(struct ArrowArray)));
        auto psChild = out_array->children[iSchemaChild];
        ++iSchemaChild;
        psChild->release = OGRLayerDefaultReleaseArray;
        psChild->n_buffers = 2;
        psChild->buffers =
            static_cast<const void **>(CPLCalloc(2, sizeof(void *)));
        int64_t *panValues =
            static_cast<int64_t *>(VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                sizeof(int64_t) * oFeatureQueue.size()));
        if (panValues == nullptr)
            goto error;
        psChild->buffers[1] = panValues;
        for (size_t iFeat = 0; iFeat < oFeatureQueue.size(); ++iFeat)
        {
            panValues[iFeat] = oFeatureQueue[iFeat]->GetFID();
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
        const bool bIsNullable = CPL_TO_BOOL(poFieldDefn->IsNullable());
        const auto eSubType = poFieldDefn->GetSubType();
        switch (poFieldDefn->GetType())
        {
            case OFTInteger:
            {
                if (eSubType == OFSTBoolean)
                {
                    if (!FillBoolArray(psChild, oFeatureQueue, nFeatureCount,
                                       bIsNullable, &OGRField::Integer, i))
                        goto error;
                }
                else if (eSubType == OFSTInt16)
                {
                    if (!FillArray<int16_t>(psChild, oFeatureQueue,
                                            nFeatureCount, bIsNullable,
                                            &OGRField::Integer, i))
                        goto error;
                }
                else
                {
                    if (!FillArray<int32_t>(psChild, oFeatureQueue,
                                            nFeatureCount, bIsNullable,
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
                if (!FillArray<int64_t>(psChild, oFeatureQueue, nFeatureCount,
                                        bIsNullable, &OGRField::Integer64, i))
                    goto error;
                break;
            }

            case OFTReal:
            {
                if (eSubType == OFSTFloat32)
                {
                    if (!FillArray<float>(psChild, oFeatureQueue, nFeatureCount,
                                          bIsNullable, &OGRField::Real, i))
                        goto error;
                }
                else
                {
                    if (!FillArray<double>(psChild, oFeatureQueue,
                                           nFeatureCount, bIsNullable,
                                           &OGRField::Real, i))
                        goto error;
                }
                break;
            }

            case OFTString:
            case OFTWideString:
            {
                const size_t nThisFeatureCount = FillStringArray<int32_t>(
                    psChild, oFeatureQueue, nFeatureCount, bIsNullable, i,
                    nMemLimit);
                if (nThisFeatureCount == 0)
                {
                    goto error_max_mem;
                }
                if (nThisFeatureCount < nFeatureCount)
                    nFeatureCount = nThisFeatureCount;
                break;
            }

            case OFTBinary:
            {
                const int nWidth = poFieldDefn->GetWidth();
                if (nWidth > 0)
                {
                    if (nFeatureCount > nMemLimit / nWidth)
                    {
                        nFeatureCount = nMemLimit / nWidth;
                        if (nFeatureCount == 0)
                            goto error_max_mem;
                    }
                    if (!FillFixedWidthBinaryArray(psChild, oFeatureQueue,
                                                   nFeatureCount, bIsNullable,
                                                   nWidth, i))
                        goto error;
                }
                else
                {
                    const size_t nThisFeatureCount = FillBinaryArray<int32_t>(
                        psChild, oFeatureQueue, nFeatureCount, bIsNullable, i,
                        nMemLimit);
                    if (nThisFeatureCount == 0)
                    {
                        goto error_max_mem;
                    }
                    if (nThisFeatureCount < nFeatureCount)
                        nFeatureCount = nThisFeatureCount;
                }
                break;
            }

            case OFTIntegerList:
            {
                size_t nThisFeatureCount;
                if (eSubType == OFSTBoolean)
                {
                    nThisFeatureCount =
                        FillListArrayBool<int32_t, GetFromIntegerList>(
                            psChild, oFeatureQueue, nFeatureCount, bIsNullable,
                            i, nMemLimit);
                }
                else if (eSubType == OFSTInt16)
                {
                    nThisFeatureCount =
                        FillListArray<int32_t, int16_t, GetFromIntegerList>(
                            psChild, oFeatureQueue, nFeatureCount, bIsNullable,
                            i, nMemLimit);
                }
                else
                {
                    nThisFeatureCount =
                        FillListArray<int32_t, int32_t, GetFromIntegerList>(
                            psChild, oFeatureQueue, nFeatureCount, bIsNullable,
                            i, nMemLimit);
                }
                if (nThisFeatureCount == 0)
                {
                    goto error_max_mem;
                }
                if (nThisFeatureCount < nFeatureCount)
                    nFeatureCount = nThisFeatureCount;
                break;
            }

            case OFTInteger64List:
            {
                const size_t nThisFeatureCount =
                    FillListArray<int32_t, int64_t, GetFromInteger64List>(
                        psChild, oFeatureQueue, nFeatureCount, bIsNullable, i,
                        nMemLimit);
                if (nThisFeatureCount == 0)
                {
                    goto error_max_mem;
                }
                if (nThisFeatureCount < nFeatureCount)
                    nFeatureCount = nThisFeatureCount;
                break;
            }

            case OFTRealList:
            {
                size_t nThisFeatureCount;
                if (eSubType == OFSTFloat32)
                {
                    nThisFeatureCount =
                        FillListArray<int32_t, float, GetFromRealList>(
                            psChild, oFeatureQueue, nFeatureCount, bIsNullable,
                            i, nMemLimit);
                }
                else
                {
                    nThisFeatureCount =
                        FillListArray<int32_t, double, GetFromRealList>(
                            psChild, oFeatureQueue, nFeatureCount, bIsNullable,
                            i, nMemLimit);
                }
                if (nThisFeatureCount == 0)
                {
                    goto error_max_mem;
                }
                if (nThisFeatureCount < nFeatureCount)
                    nFeatureCount = nThisFeatureCount;
                break;
            }

            case OFTStringList:
            case OFTWideStringList:
            {
                const size_t nThisFeatureCount = FillStringListArray<int32_t>(
                    psChild, oFeatureQueue, nFeatureCount, bIsNullable, i,
                    nMemLimit);
                if (nThisFeatureCount == 0)
                {
                    goto error_max_mem;
                }
                if (nThisFeatureCount < nFeatureCount)
                    nFeatureCount = nThisFeatureCount;
                break;
            }

            case OFTDate:
            {
                if (!FillDateArray(psChild, oFeatureQueue, nFeatureCount,
                                   bIsNullable, i))
                    goto error;
                break;
            }

            case OFTTime:
            {
                if (!FillTimeArray(psChild, oFeatureQueue, nFeatureCount,
                                   bIsNullable, i))
                    goto error;
                break;
            }

            case OFTDateTime:
            {
                if (!FillDateTimeArray(psChild, oFeatureQueue, nFeatureCount,
                                       bIsNullable, i,
                                       poFieldDefn->GetTZFlag()))
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
        psChild->length = oFeatureQueue.size();
        const size_t nThisFeatureCount = FillWKBGeometryArray<int32_t>(
            psChild, oFeatureQueue, nFeatureCount, poFieldDefn, i, nMemLimit);
        if (nThisFeatureCount == 0)
        {
            goto error_max_mem;
        }
        if (nThisFeatureCount < nFeatureCount)
            nFeatureCount = nThisFeatureCount;
    }

    // Remove consumed features from the queue
    if (nFeatureCount == oFeatureQueue.size())
        oFeatureQueue.clear();
    else
    {
        for (size_t i = 0; i < nFeatureCount; ++i)
        {
            oFeatureQueue.pop_front();
        }
    }

    out_array->n_children = iSchemaChild;
    out_array->length = nFeatureCount;
    for (int i = 0; i < out_array->n_children; ++i)
    {
        out_array->children[i]->length = nFeatureCount;
    }

    return 0;

error_max_mem:
    CPLError(CE_Failure, CPLE_AppDefined,
             "Too large feature: not even a single feature can be returned");
error:
    oFeatureQueue.clear();
    poPrivate->poShared->m_bEOF = true;
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
    poPrivate->poShared->m_bEOF = false;
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
 * must must be freed with out_stream->release(out_stream). Please carefully
 * read https://arrow.apache.org/docs/format/CStreamInterface.html for more
 * details on using Arrow C stream.
 *
 * The method may take into account ignored fields set with SetIgnoredFields()
 * (the default implementation does), and should take into account filters set
 * with SetSpatialFilter() and SetAttributeFilter(). Note however that
 * specialized implementations may fallback to the default (slower)
 * implementation when filters are set.
 * Drivers that have a specialized implementation should advertise the
 * OLCFastGetArrowStream capability.
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
 * Starting with GDAL 3.8, the ArrowSchema::metadata field filled by the
 * get_schema() callback may be set with the potential following items:
 * <ul>
 * <li>"GDAL:OGR:alternative_name": value of
 *     OGRFieldDefn::GetAlternativeNameRef()</li>
 * <li>"GDAL:OGR:comment": value of OGRFieldDefn::GetComment()</li>
 * <li>"GDAL:OGR:default": value of OGRFieldDefn::GetDefault()</li>
 * <li>"GDAL:OGR:subtype": value of OGRFieldDefn::GetSubType()</li>
 * <li>"GDAL:OGR:width": value of OGRFieldDefn::GetWidth() (serialized as a
 *     string)</li>
 * <li>"GDAL:OGR:unique": value of OGRFieldDefn::IsUnique() (serialized as
 *     "true" or "false")</li>
 * <li>"GDAL:OGR:domain_name": value of OGRFieldDefn::GetDomainName()</li>
 * </ul>
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
 * <li>INCLUDE_FID=YES/NO. Whether to include the FID column. Defaults to YES.
 * </li>
 * <li>MAX_FEATURES_IN_BATCH=integer. Maximum number of features to retrieve in
 *     a ArrowArray batch. Defaults to 65 536.</li>
 * <li>TIMEZONE="unknown", "UTC", "(+|:)HH:MM" or any other value supported by
 *     Arrow. (GDAL >= 3.8)
 *     Override the timezone flag nominally provided by
 *     OGRFieldDefn::GetTZFlag(), and used for the Arrow field timezone
 *     declaration, with a user specified timezone.
 *     Note that datetime values in Arrow arrays are always stored in UTC, and
 *     that the time zone flag used by GDAL to convert to UTC is the one of the
 *     OGRField::Date::TZFlag member at the OGRFeature level. The conversion
 *     to UTC of a OGRField::Date is only done if both the timezone indicated by
 *     OGRField::Date::TZFlag and the one at the OGRFieldDefn level (or set by
 *     this TIMEZONE option) are not unknown.</li>
 * <li>GEOMETRY_METADATA_ENCODING=OGC/GEOARROW (GDAL >= 3.8).
 *     The default is OGC, which will lead to setting
 *     the Arrow geometry column metadata to ARROW:extension:name=ogc.wkb.
 *     If setting to GEOMETRY_METADATA_ENCODING to GEOARROW,
 *     ARROW:extension:name=geoarrow.wkb and
 *     ARROW:extension:metadata={"crs": &lt;projjson CRS representation>&gt; are set.
 * </li>
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

    // Special case for "FID = constant", or "FID IN (constant1, ...., constantN)"
    m_poSharedArrowArrayStreamPrivateData->m_anQueriedFIDs.clear();
    m_poSharedArrowArrayStreamPrivateData->m_iQueriedFIDS = 0;
    if (m_poAttrQuery)
    {
        swq_expr_node *poNode =
            static_cast<swq_expr_node *>(m_poAttrQuery->GetSWQExpr());
        if (poNode->eNodeType == SNT_OPERATION &&
            (poNode->nOperation == SWQ_IN || poNode->nOperation == SWQ_EQ) &&
            poNode->papoSubExpr[0]->eNodeType == SNT_COLUMN &&
            poNode->papoSubExpr[0]->field_index ==
                GetLayerDefn()->GetFieldCount() + SPF_FID &&
            TestCapability(OLCRandomRead))
        {
            std::set<GIntBig> oSetAlreadyListed;
            for (int i = 1; i < poNode->nSubExprCount; ++i)
            {
                if (poNode->papoSubExpr[i]->eNodeType == SNT_CONSTANT &&
                    poNode->papoSubExpr[i]->field_type == SWQ_INTEGER64 &&
                    oSetAlreadyListed.find(poNode->papoSubExpr[i]->int_value) ==
                        oSetAlreadyListed.end())
                {
                    oSetAlreadyListed.insert(poNode->papoSubExpr[i]->int_value);
                    m_poSharedArrowArrayStreamPrivateData->m_anQueriedFIDs
                        .push_back(poNode->papoSubExpr[i]->int_value);
                }
            }
        }
    }

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
 * Starting with GDAL 3.8, the ArrowSchema::metadata field filled by the
 * get_schema() callback may be set with the potential following items:
 * <ul>
 * <li>"GDAL:OGR:alternative_name": value of
 *     OGRFieldDefn::GetAlternativeNameRef()</li>
 * <li>"GDAL:OGR:comment": value of OGRFieldDefn::GetComment()</li>
 * <li>"GDAL:OGR:default": value of OGRFieldDefn::GetDefault()</li>
 * <li>"GDAL:OGR:subtype": value of OGRFieldDefn::GetSubType()</li>
 * <li>"GDAL:OGR:width": value of OGRFieldDefn::GetWidth() (serialized as a
 *     string)</li>
 * <li>"GDAL:OGR:unique": value of OGRFieldDefn::IsUnique() (serialized as
 *     "true" or "false")</li>
 * <li>"GDAL:OGR:domain_name": value of OGRFieldDefn::GetDomainName()</li>
 * </ul>
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
 * <li>TIMEZONE="unknown", "UTC", "(+|:)HH:MM" or any other value supported by
 *     Arrow. (GDAL >= 3.8)
 *     Override the timezone flag nominally provided by
 *     OGRFieldDefn::GetTZFlag(), and used for the Arrow field timezone
 *     declaration, with a user specified timezone.
 *     Note that datetime values in Arrow arrays are always stored in UTC, and
 *     that the time zone flag used by GDAL to convert to UTC is the one of the
 *     OGRField::Date::TZFlag member at the OGRFeature level. The conversion
 *     to UTC of a OGRField::Date is only done if both the timezone indicated by
 *     OGRField::Date::TZFlag and the one at the OGRFieldDefn level (or set by
 *     this TIMEZONE option) are not unknown.</li>
 * <li>GEOMETRY_METADATA_ENCODING=OGC/GEOARROW (GDAL >= 3.8).
 *     The default is OGC, which will lead to setting
 *     the Arrow geometry column metadata to ARROW:extension:name=ogc.wkb.
 *     If setting to GEOMETRY_METADATA_ENCODING to GEOARROW,
 *     ARROW:extension:name=geoarrow.wkb and
 *     ARROW:extension:metadata={"crs": &lt;projjson CRS representation>&gt; are set.
 * </li>
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

        oMetadata[osKey] = std::move(osValue);
    }

    return oMetadata;
}

/************************************************************************/
/*                        ParseDecimalFormat()                          */
/************************************************************************/

static bool ParseDecimalFormat(const char *format, int &nPrecision, int &nScale,
                               int &nWidthInBytes)
{
    // d:19,10     ==> decimal128 [precision 19, scale 10]
    // d:19,10,NNN ==> decimal bitwidth = NNN [precision 19, scale 10]
    nPrecision = 0;
    nScale = 0;
    nWidthInBytes = 128 / 8;  // 128 bit
    const char *pszFirstComma = strchr(format + 2, ',');
    if (pszFirstComma)
    {
        nPrecision = atoi(format + 2);
        nScale = atoi(pszFirstComma + 1);
        const char *pszSecondComma = strchr(pszFirstComma + 1, ',');
        if (pszSecondComma)
        {
            const int nWidthInBits = atoi(pszSecondComma + 1);
            if ((nWidthInBits % 8) != 0)
            {
                // shouldn't happen for well-format schemas
                nWidthInBytes = 0;
                return false;
            }
            else
            {
                nWidthInBytes = nWidthInBits / 8;
            }
        }
    }
    else
    {
        // shouldn't happen for well-format schemas
        nWidthInBytes = 0;
        return false;
    }
    return true;
}

/************************************************************************/
/*                   GetErrorIfUnsupportedDecimal()                     */
/************************************************************************/

static const char *GetErrorIfUnsupportedDecimal(int nWidthInBytes,
                                                int nPrecision)
{

    if (nWidthInBytes != 128 / 8 && nWidthInBytes != 256 / 8)
    {
        return "For decimal field, only width 128 and 256 are supported";
    }

    // precision=19 fits on 64 bits
    if (nPrecision <= 0 || nPrecision > 19)
    {
        return "For decimal field, only precision up to 19 is supported";
    }

    return nullptr;
}

/************************************************************************/
/*                            IsHandledSchema()                         */
/************************************************************************/

static bool IsHandledSchema(bool bTopLevel, const struct ArrowSchema *schema,
                            const std::string &osPrefix, bool bHasAttrQuery,
                            const CPLStringList &aosUsedFields)
{
    const char *format = schema->format;
    if (IsStructure(format))
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
    if (IsList(format) || IsLargeList(format) || IsFixedSizeList(format) ||
        IsMap(format))
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
                     (osPrefix + schema->name).c_str(), format);
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
        if (strcmp(format, pszHandledFormat) == 0)
        {
            return true;
        }
    }

    if (IsDecimal(format))
    {
        if (bHasAttrQuery &&
            aosUsedFields.FindString((osPrefix + schema->name).c_str()) >= 0)
        {
            int nPrecision = 0;
            int nScale = 0;
            int nWidthInBytes = 0;
            if (!ParseDecimalFormat(format, nPrecision, nScale, nWidthInBytes))
            {
                CPLDebug("OGR", "%s",
                         (std::string("Invalid field format ") + format +
                          " for field " + osPrefix + schema->name)
                             .c_str());
                return false;
            }

            const char *pszError =
                GetErrorIfUnsupportedDecimal(nWidthInBytes, nPrecision);
            if (pszError)
            {
                CPLDebug("OGR", "%s", pszError);
                return false;
            }
        }
        return true;
    }

    if (IsFixedWidthBinary(format) || IsTimestamp(format))
    {
        return true;
    }

    CPLDebug("OGR", "Field %s has unhandled format '%s'",
             (osPrefix + schema->name).c_str(), format);
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
                if (!IsBinary(fieldSchema->format) &&
                    !IsLargeBinary(fieldSchema->format))
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
                if (oIter->second != EXTENSION_NAME_OGC_WKB &&
                    oIter->second != EXTENSION_NAME_GEOARROW_WKB)
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

#if 0
/************************************************************************/
/*                      CheckValidityBuffer()                           */
/************************************************************************/

static void CheckValidityBuffer(const struct ArrowArray *array)
{
    if (array->null_count < 0)
        return;
    const uint8_t *pabyValidity =
        static_cast<const uint8_t *>(const_cast<const void *>(array->buffers[0]));
    if( !pabyValidity )
    {
        CPLAssert(array->null_count == 0);
        return;
    }
    size_t null_count = 0;
    const size_t nOffset = static_cast<size_t>(array->offset);
    for(size_t i = 0; i < static_cast<size_t>(array->length); ++i )
    {
        if (!TestBit(pabyValidity, i + nOffset))
            ++ null_count;
    }
    CPLAssert(static_cast<size_t>(array->null_count) == null_count);
}
#endif

/************************************************************************/
/*                    CompactValidityBuffer()                           */
/************************************************************************/

static void CompactValidityBuffer(
    const struct ArrowSchema *, struct ArrowArray *array, size_t iStart,
    const std::vector<bool> &abyValidityFromFilters, size_t nNewLength)
{
    // Invalidate null_count as the same validity buffer may be used when
    // scrolling batches, and this creates confusion if we try to set it
    // to different values among the batches
    if (array->null_count <= 0)
    {
        array->null_count = -1;
        return;
    }
    array->null_count = -1;

    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());
    uint8_t *pabyValidity =
        static_cast<uint8_t *>(const_cast<void *>(array->buffers[0]));
    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    size_t j = iStart + nOffset;
    for (size_t i = 0; i < nLength && j < nNewLength + nOffset; ++i)
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

static void CompactBoolArray(const struct ArrowSchema *schema,
                             struct ArrowArray *array, size_t iStart,
                             const std::vector<bool> &abyValidityFromFilters,
                             size_t nNewLength)
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

    if (schema->flags & ARROW_FLAG_NULLABLE)
        CompactValidityBuffer(schema, array, iStart, abyValidityFromFilters,
                              nNewLength);

    array->length = nNewLength;
}

/************************************************************************/
/*                       CompactPrimitiveArray()                        */
/************************************************************************/

template <class T>
static void CompactPrimitiveArray(
    const struct ArrowSchema *schema, struct ArrowArray *array, size_t iStart,
    const std::vector<bool> &abyValidityFromFilters, size_t nNewLength)
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

    if (schema->flags & ARROW_FLAG_NULLABLE)
        CompactValidityBuffer(schema, array, iStart, abyValidityFromFilters,
                              nNewLength);

    array->length = nNewLength;
}

/************************************************************************/
/*                    CompactStringOrBinaryArray()                      */
/************************************************************************/

template <class OffsetType>
static void CompactStringOrBinaryArray(
    const struct ArrowSchema *schema, struct ArrowArray *array, size_t iStart,
    const std::vector<bool> &abyValidityFromFilters, size_t nNewLength)
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
    OffsetType nCurOffset = panOffsets[iStart];
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

    if (schema->flags & ARROW_FLAG_NULLABLE)
        CompactValidityBuffer(schema, array, iStart, abyValidityFromFilters,
                              nNewLength);

    array->length = nNewLength;
}

/************************************************************************/
/*                    CompactFixedWidthArray()                          */
/************************************************************************/

static void
CompactFixedWidthArray(const struct ArrowSchema *schema,
                       struct ArrowArray *array, int nWidth, size_t iStart,
                       const std::vector<bool> &abyValidityFromFilters,
                       size_t nNewLength)
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

    if (schema->flags & ARROW_FLAG_NULLABLE)
        CompactValidityBuffer(schema, array, iStart, abyValidityFromFilters,
                              nNewLength);

    array->length = nNewLength;
}

/************************************************************************/
/*                       CompactStructArray()                           */
/************************************************************************/

static bool CompactArray(const struct ArrowSchema *schema,
                         struct ArrowArray *array, size_t iStart,
                         const std::vector<bool> &abyValidityFromFilters,
                         size_t nNewLength);

static bool CompactStructArray(const struct ArrowSchema *schema,
                               struct ArrowArray *array, size_t iStart,
                               const std::vector<bool> &abyValidityFromFilters,
                               size_t nNewLength)
{
    // The equality might not be strict in the case of when some sub-arrays
    // are fully void !
    CPLAssert(array->n_children <= schema->n_children);
    for (int64_t iField = 0; iField < array->n_children; ++iField)
    {
        const auto psChildSchema = schema->children[iField];
        const auto psChildArray = array->children[iField];
        // To please Arrow validation...
        const size_t nChildNewLength =
            static_cast<size_t>(array->offset) + nNewLength;
        if (psChildArray->length > array->length)
        {
            std::vector<bool> abyChildValidity(abyValidityFromFilters);
            abyChildValidity.resize(
                abyValidityFromFilters.size() +
                    static_cast<size_t>(psChildArray->length - array->length),
                false);
            if (!CompactArray(psChildSchema, psChildArray, iStart,
                              abyChildValidity, nChildNewLength))
            {
                return false;
            }
        }
        else
        {
            if (!CompactArray(psChildSchema, psChildArray, iStart,
                              abyValidityFromFilters, nChildNewLength))
            {
                return false;
            }
        }
        CPLAssert(psChildArray->length ==
                  static_cast<int64_t>(nChildNewLength));
    }

    if (schema->flags & ARROW_FLAG_NULLABLE)
        CompactValidityBuffer(schema, array, iStart, abyValidityFromFilters,
                              nNewLength);

    array->length = nNewLength;

    return true;
}

/************************************************************************/
/*                     InvalidateNullCountRec()                         */
/************************************************************************/

static void InvalidateNullCountRec(const struct ArrowSchema *schema,
                                   struct ArrowArray *array)
{
    if (schema->flags & ARROW_FLAG_NULLABLE)
        array->null_count = -1;
    for (int i = 0; i < array->n_children; ++i)
        InvalidateNullCountRec(schema->children[i], array->children[i]);
}

/************************************************************************/
/*                       CompactListArray()                             */
/************************************************************************/

template <class OffsetType>
static bool CompactListArray(const struct ArrowSchema *schema,
                             struct ArrowArray *array, size_t iStart,
                             const std::vector<bool> &abyValidityFromFilters,
                             size_t nNewLength)
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
                const auto nSize =
                    panOffsets[i + iStart + 1] - panOffsets[i + iStart];
                panOffsets[j] = nCurOffset;
                nCurOffset += nSize;
                ++j;
            }
            else
            {
                const auto nStartOffset = panOffsets[i + iStart];
                const auto nEndOffset = panOffsets[i + iStart + 1];
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
        const size_t nChildNewLength = static_cast<size_t>(panOffsets[j]);
        // To please Arrow validation
        for (; j < iStart + nLength; ++j)
            panOffsets[j] = nCurOffset;

        if (!CompactArray(psChildSchema, psChildArray,
                          static_cast<size_t>(panOffsets[iStart]),
                          abyChildValidity, nChildNewLength))
            return false;

        CPLAssert(psChildArray->length ==
                  static_cast<int64_t>(nChildNewLength));
    }
    else
    {
        InvalidateNullCountRec(psChildSchema, psChildArray);
    }

    if (schema->flags & ARROW_FLAG_NULLABLE)
        CompactValidityBuffer(schema, array, iStart, abyValidityFromFilters,
                              nNewLength);

    array->length = nNewLength;

    return true;
}

/************************************************************************/
/*                     CompactFixedSizeListArray()                      */
/************************************************************************/

static bool
CompactFixedSizeListArray(const struct ArrowSchema *schema,
                          struct ArrowArray *array, size_t N, size_t iStart,
                          const std::vector<bool> &abyValidityFromFilters,
                          size_t nNewLength)
{
    CPLAssert(static_cast<size_t>(array->length) >=
              iStart + abyValidityFromFilters.size());
    CPLAssert(array->n_children == 1);

    const auto psChildSchema = schema->children[0];
    const auto psChildArray = array->children[0];

    const size_t nLength = abyValidityFromFilters.size();
    const size_t nOffset = static_cast<size_t>(array->offset);
    std::vector<bool> abyChildValidity(N * nLength, true);
    size_t nChildNewLength = (iStart + nOffset) * N;
    size_t nSrcLength = 0;
    for (size_t i = 0; i < nLength; ++i)
    {
        if (abyValidityFromFilters[i])
        {
            nChildNewLength += N;
            nSrcLength++;
        }
        else
        {
            const size_t nStartOffset = i * N;
            const size_t nEndOffset = (i + 1) * N;
            for (size_t k = nStartOffset; k < nEndOffset; ++k)
                abyChildValidity[k] = false;
        }
    }
    CPL_IGNORE_RET_VAL(nSrcLength);
    CPLAssert(iStart + nSrcLength == nNewLength);

    if (!CompactArray(psChildSchema, psChildArray, (iStart + nOffset) * N,
                      abyChildValidity, nChildNewLength))
        return false;

    if (schema->flags & ARROW_FLAG_NULLABLE)
        CompactValidityBuffer(schema, array, iStart, abyValidityFromFilters,
                              nNewLength);

    array->length = nNewLength;

    CPLAssert(psChildArray->length >=
              static_cast<int64_t>(N) * (array->length + array->offset));

    return true;
}

/************************************************************************/
/*                       CompactMapArray()                              */
/************************************************************************/

static bool CompactMapArray(const struct ArrowSchema *schema,
                            struct ArrowArray *array, size_t iStart,
                            const std::vector<bool> &abyValidityFromFilters,
                            size_t nNewLength)
{
    return CompactListArray<uint32_t>(schema, array, iStart,
                                      abyValidityFromFilters, nNewLength);
}

/************************************************************************/
/*                           CompactArray()                             */
/************************************************************************/

static bool CompactArray(const struct ArrowSchema *schema,
                         struct ArrowArray *array, size_t iStart,
                         const std::vector<bool> &abyValidityFromFilters,
                         size_t nNewLength)
{
    const char *format = schema->format;

    if (IsStructure(format))
    {
        if (!CompactStructArray(schema, array, iStart, abyValidityFromFilters,
                                nNewLength))
            return false;
    }
    else if (IsList(format))
    {
        if (!CompactListArray<uint32_t>(schema, array, iStart,
                                        abyValidityFromFilters, nNewLength))
            return false;
    }
    else if (IsLargeList(format))
    {
        if (!CompactListArray<uint64_t>(schema, array, iStart,
                                        abyValidityFromFilters, nNewLength))
            return false;
    }
    else if (IsMap(format))
    {
        if (!CompactMapArray(schema, array, iStart, abyValidityFromFilters,
                             nNewLength))
            return false;
    }
    else if (IsFixedSizeList(format))
    {
        const int N = GetFixedSizeList(format);
        if (N <= 0)
            return false;
        if (!CompactFixedSizeListArray(schema, array, static_cast<size_t>(N),
                                       iStart, abyValidityFromFilters,
                                       nNewLength))
            return false;
    }
    else if (IsBoolean(format))
    {
        CompactBoolArray(schema, array, iStart, abyValidityFromFilters,
                         nNewLength);
    }
    else if (IsInt8(format) || IsUInt8(format))
    {
        CompactPrimitiveArray<uint8_t>(schema, array, iStart,
                                       abyValidityFromFilters, nNewLength);
    }
    else if (IsInt16(format) || IsUInt16(format) || IsFloat16(format))
    {
        CompactPrimitiveArray<uint16_t>(schema, array, iStart,
                                        abyValidityFromFilters, nNewLength);
    }
    else if (IsInt32(format) || IsUInt32(format) || IsFloat32(format) ||
             strcmp(format, "tdD") == 0 || strcmp(format, "tts") == 0 ||
             strcmp(format, "ttm") == 0)
    {
        CompactPrimitiveArray<uint32_t>(schema, array, iStart,
                                        abyValidityFromFilters, nNewLength);
    }
    else if (IsInt64(format) || IsUInt64(format) || IsFloat64(format) ||
             strcmp(format, "tdm") == 0 || strcmp(format, "ttu") == 0 ||
             strcmp(format, "ttn") == 0 || strncmp(format, "ts", 2) == 0)
    {
        CompactPrimitiveArray<uint64_t>(schema, array, iStart,
                                        abyValidityFromFilters, nNewLength);
    }
    else if (IsString(format) || IsBinary(format))
    {
        CompactStringOrBinaryArray<uint32_t>(
            schema, array, iStart, abyValidityFromFilters, nNewLength);
    }
    else if (IsLargeString(format) || IsLargeBinary(format))
    {
        CompactStringOrBinaryArray<uint64_t>(
            schema, array, iStart, abyValidityFromFilters, nNewLength);
    }
    else if (IsFixedWidthBinary(format))
    {
        const int nWidth = GetFixedWithBinary(format);
        CompactFixedWidthArray(schema, array, nWidth, iStart,
                               abyValidityFromFilters, nNewLength);
    }
    else if (IsDecimal(format))
    {
        int nPrecision = 0;
        int nScale = 0;
        int nWidthInBytes = 0;
        if (!ParseDecimalFormat(format, nPrecision, nScale, nWidthInBytes))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unexpected error in PostFilterArrowArray(): unhandled "
                     "field format: %s",
                     format);

            return false;
        }
        CompactFixedWidthArray(schema, array, nWidthInBytes, iStart,
                               abyValidityFromFilters, nNewLength);
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
        (nTZLen == 7 && strcmp(pszTZ, "Etc/UTC") == 0))
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
        if (IsStructure(psChild->format))
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
/*                              GetValue()                              */
/************************************************************************/

template <typename ArrowType>
inline static ArrowType GetValue(const struct ArrowArray *array,
                                 size_t iFeature)
{
    const auto *panValues = static_cast<const ArrowType *>(array->buffers[1]);
    return panValues[iFeature + array->offset];
}

template <> bool GetValue<bool>(const struct ArrowArray *array, size_t iFeature)
{
    const auto *pabyValues = static_cast<const uint8_t *>(array->buffers[1]);
    return TestBit(pabyValues, iFeature + static_cast<size_t>(array->offset));
}

/************************************************************************/
/*                          GetValueFloat16()                           */
/************************************************************************/

static float GetValueFloat16(const struct ArrowArray *array, const size_t nIdx)
{
    const auto *panValues = static_cast<const uint16_t *>(array->buffers[1]);
    const auto nFloat16AsUInt32 =
        CPLHalfToFloat(panValues[nIdx + array->offset]);
    float f;
    memcpy(&f, &nFloat16AsUInt32, sizeof(f));
    return f;
}

/************************************************************************/
/*                          GetValueDecimal()                           */
/************************************************************************/

static double GetValueDecimal(const struct ArrowArray *array,
                              const int nWidthIn64BitWord, const int nScale,
                              const size_t nIdx)
{
#ifdef CPL_LSB
    const auto nIdxIn64BitWord = nIdx * nWidthIn64BitWord;
#else
    const auto nIdxIn64BitWord =
        nIdx * nWidthIn64BitWord + nWidthIn64BitWord - 1;
#endif
    const auto *panValues = static_cast<const int64_t *>(array->buffers[1]);
    const auto nVal =
        panValues[nIdxIn64BitWord + array->offset * nWidthIn64BitWord];
    return static_cast<double>(nVal) * std::pow(10.0, -nScale);
}

/************************************************************************/
/*                             GetString()                              */
/************************************************************************/

template <class OffsetType>
static std::string GetString(const struct ArrowArray *array, const size_t nIdx)
{
    const OffsetType *panOffsets =
        static_cast<const OffsetType *>(array->buffers[1]) +
        static_cast<size_t>(array->offset) + nIdx;
    const char *pabyStr = static_cast<const char *>(array->buffers[2]);
    std::string osStr;
    osStr.assign(pabyStr + static_cast<size_t>(panOffsets[0]),
                 static_cast<size_t>(panOffsets[1] - panOffsets[0]));
    return osStr;
}

/************************************************************************/
/*                       GetBinaryAsBase64()                            */
/************************************************************************/

template <class OffsetType>
static std::string GetBinaryAsBase64(const struct ArrowArray *array,
                                     const size_t nIdx)
{
    const OffsetType *panOffsets =
        static_cast<const OffsetType *>(array->buffers[1]) +
        static_cast<size_t>(array->offset) + nIdx;
    const GByte *pabyData = static_cast<const GByte *>(array->buffers[2]);
    const size_t nLen = static_cast<size_t>(panOffsets[1] - panOffsets[0]);
    if (nLen > static_cast<size_t>(std::numeric_limits<int>::max()))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too large binary");
        return std::string();
    }
    char *pszVal = CPLBase64Encode(
        static_cast<int>(nLen), pabyData + static_cast<size_t>(panOffsets[0]));
    std::string osStr(pszVal);
    CPLFree(pszVal);
    return osStr;
}

/************************************************************************/
/*                   GetValueFixedWithBinaryAsBase64()                  */
/************************************************************************/

static std::string
GetValueFixedWithBinaryAsBase64(const struct ArrowArray *array,
                                const int nWidth, const size_t nIdx)
{
    const GByte *pabyData = static_cast<const GByte *>(array->buffers[1]);
    char *pszVal = CPLBase64Encode(
        nWidth,
        pabyData + (static_cast<size_t>(array->offset) + nIdx) * nWidth);
    std::string osStr(pszVal);
    CPLFree(pszVal);
    return osStr;
}

static CPLJSONObject GetObjectAsJSON(const struct ArrowSchema *schema,
                                     const struct ArrowArray *array,
                                     const size_t nIdx);

/************************************************************************/
/*                               AddToArray()                           */
/************************************************************************/

static void AddToArray(CPLJSONArray &oArray, const struct ArrowSchema *schema,
                       const struct ArrowArray *array, const size_t nIdx)
{
    if (IsBoolean(schema->format))
        oArray.Add(GetValue<bool>(array, nIdx));
    else if (IsUInt8(schema->format))
        oArray.Add(GetValue<uint8_t>(array, nIdx));
    else if (IsInt8(schema->format))
        oArray.Add(GetValue<int8_t>(array, nIdx));
    else if (IsUInt16(schema->format))
        oArray.Add(GetValue<uint16_t>(array, nIdx));
    else if (IsInt16(schema->format))
        oArray.Add(GetValue<int16_t>(array, nIdx));
    else if (IsUInt32(schema->format))
        oArray.Add(static_cast<GIntBig>(GetValue<uint32_t>(array, nIdx)));
    else if (IsInt32(schema->format))
        oArray.Add(GetValue<int32_t>(array, nIdx));
    else if (IsUInt64(schema->format))
        oArray.Add(GetValue<uint64_t>(array, nIdx));
    else if (IsInt64(schema->format))
        oArray.Add(static_cast<GIntBig>(GetValue<int64_t>(array, nIdx)));
    else if (IsFloat16(schema->format))
        oArray.Add(GetValueFloat16(array, nIdx));
    else if (IsFloat32(schema->format))
        oArray.Add(GetValue<float>(array, nIdx));
    else if (IsFloat64(schema->format))
        oArray.Add(GetValue<double>(array, nIdx));
    else if (IsString(schema->format))
        oArray.Add(GetString<uint32_t>(array, nIdx));
    else if (IsLargeString(schema->format))
        oArray.Add(GetString<uint64_t>(array, nIdx));
    else if (IsBinary(schema->format))
        oArray.Add(GetBinaryAsBase64<uint32_t>(array, nIdx));
    else if (IsLargeBinary(schema->format))
        oArray.Add(GetBinaryAsBase64<uint64_t>(array, nIdx));
    else if (IsFixedWidthBinary(schema->format))
        oArray.Add(GetValueFixedWithBinaryAsBase64(
            array, GetFixedWithBinary(schema->format), nIdx));
    else if (IsDecimal(schema->format))
    {
        int nPrecision = 0;
        int nScale = 0;
        int nWidthInBytes = 0;
        const bool bOK = ParseDecimalFormat(schema->format, nPrecision, nScale,
                                            nWidthInBytes);
        // Already validated
        CPLAssert(bOK);
        CPL_IGNORE_RET_VAL(bOK);
        oArray.Add(GetValueDecimal(array, nWidthInBytes / 8, nScale, nIdx));
    }
    else
        oArray.Add(GetObjectAsJSON(schema, array, nIdx));
}

/************************************************************************/
/*                         GetListAsJSON()                              */
/************************************************************************/

template <class OffsetType>
static CPLJSONArray GetListAsJSON(const struct ArrowSchema *schema,
                                  const struct ArrowArray *array,
                                  const size_t nIdx)
{
    CPLJSONArray oArray;
    const auto panOffsets = static_cast<const OffsetType *>(array->buffers[1]) +
                            array->offset + nIdx;
    const auto childSchema = schema->children[0];
    const auto childArray = array->children[0];
    const uint8_t *pabyValidity =
        childArray->null_count == 0
            ? nullptr
            : static_cast<const uint8_t *>(childArray->buffers[0]);
    for (size_t k = static_cast<size_t>(panOffsets[0]);
         k < static_cast<size_t>(panOffsets[1]); k++)
    {
        if (!pabyValidity ||
            TestBit(pabyValidity, k + static_cast<size_t>(childArray->offset)))
        {
            AddToArray(oArray, childSchema, childArray, k);
        }
        else
        {
            oArray.AddNull();
        }
    }
    return oArray;
}

/************************************************************************/
/*                     GetFixedSizeListAsJSON()                         */
/************************************************************************/

static CPLJSONArray GetFixedSizeListAsJSON(const struct ArrowSchema *schema,
                                           const struct ArrowArray *array,
                                           const size_t nIdx)
{
    CPLJSONArray oArray;
    const int nVals = GetFixedSizeList(schema->format);
    const auto childSchema = schema->children[0];
    const auto childArray = array->children[0];
    const uint8_t *pabyValidity =
        childArray->null_count == 0
            ? nullptr
            : static_cast<const uint8_t *>(childArray->buffers[0]);
    for (size_t k = nIdx * nVals; k < (nIdx + 1) * nVals; k++)
    {
        if (!pabyValidity ||
            TestBit(pabyValidity, k + static_cast<size_t>(childArray->offset)))
        {
            AddToArray(oArray, childSchema, childArray, k);
        }
        else
        {
            oArray.AddNull();
        }
    }
    return oArray;
}

/************************************************************************/
/*                              AddToDict()                             */
/************************************************************************/

static void AddToDict(CPLJSONObject &oDict, const std::string &osKey,
                      const struct ArrowSchema *schema,
                      const struct ArrowArray *array, const size_t nIdx)
{
    if (IsBoolean(schema->format))
        oDict.Add(osKey, GetValue<bool>(array, nIdx));
    else if (IsUInt8(schema->format))
        oDict.Add(osKey, GetValue<uint8_t>(array, nIdx));
    else if (IsInt8(schema->format))
        oDict.Add(osKey, GetValue<int8_t>(array, nIdx));
    else if (IsUInt16(schema->format))
        oDict.Add(osKey, GetValue<uint16_t>(array, nIdx));
    else if (IsInt16(schema->format))
        oDict.Add(osKey, GetValue<int16_t>(array, nIdx));
    else if (IsUInt32(schema->format))
        oDict.Add(osKey, static_cast<GIntBig>(GetValue<uint32_t>(array, nIdx)));
    else if (IsInt32(schema->format))
        oDict.Add(osKey, GetValue<int32_t>(array, nIdx));
    else if (IsUInt64(schema->format))
        oDict.Add(osKey, GetValue<uint64_t>(array, nIdx));
    else if (IsInt64(schema->format))
        oDict.Add(osKey, static_cast<GIntBig>(GetValue<int64_t>(array, nIdx)));
    else if (IsFloat16(schema->format))
        oDict.Add(osKey, GetValueFloat16(array, nIdx));
    else if (IsFloat32(schema->format))
        oDict.Add(osKey, GetValue<float>(array, nIdx));
    else if (IsFloat64(schema->format))
        oDict.Add(osKey, GetValue<double>(array, nIdx));
    else if (IsString(schema->format))
        oDict.Add(osKey, GetString<uint32_t>(array, nIdx));
    else if (IsLargeString(schema->format))
        oDict.Add(osKey, GetString<uint64_t>(array, nIdx));
    else if (IsBinary(schema->format))
        oDict.Add(osKey, GetBinaryAsBase64<uint32_t>(array, nIdx));
    else if (IsLargeBinary(schema->format))
        oDict.Add(osKey, GetBinaryAsBase64<uint64_t>(array, nIdx));
    else if (IsFixedWidthBinary(schema->format))
        oDict.Add(osKey, GetValueFixedWithBinaryAsBase64(
                             array, GetFixedWithBinary(schema->format), nIdx));
    else if (IsDecimal(schema->format))
    {
        int nPrecision = 0;
        int nScale = 0;
        int nWidthInBytes = 0;
        const bool bOK = ParseDecimalFormat(schema->format, nPrecision, nScale,
                                            nWidthInBytes);
        // Already validated
        CPLAssert(bOK);
        CPL_IGNORE_RET_VAL(bOK);
        oDict.Add(osKey,
                  GetValueDecimal(array, nWidthInBytes / 8, nScale, nIdx));
    }
    else
        oDict.Add(osKey, GetObjectAsJSON(schema, array, nIdx));
}

/************************************************************************/
/*                         GetMapAsJSON()                               */
/************************************************************************/

static CPLJSONObject GetMapAsJSON(const struct ArrowSchema *schema,
                                  const struct ArrowArray *array,
                                  const size_t nIdx)
{
    const auto schemaStruct = schema->children[0];
    if (!IsStructure(schemaStruct->format))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetMapAsJSON(): !IsStructure(schemaStruct->format))");
        return CPLJSONObject();
    }
    const auto schemaKey = schemaStruct->children[0];
    const auto schemaValues = schemaStruct->children[1];
    if (!IsString(schemaKey->format))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetMapAsJSON(): !IsString(schemaKey->format))");
        return CPLJSONObject();
    }
    const auto arrayKeys = array->children[0]->children[0];
    const auto arrayValues = array->children[0]->children[1];

    CPLJSONObject oDict;
    const auto panOffsets =
        static_cast<const uint32_t *>(array->buffers[1]) + array->offset + nIdx;
    const uint8_t *pabyValidityKeys =
        arrayKeys->null_count == 0
            ? nullptr
            : static_cast<const uint8_t *>(arrayKeys->buffers[0]);
    const uint32_t *panOffsetsKeys =
        static_cast<const uint32_t *>(arrayKeys->buffers[1]) +
        arrayKeys->offset;
    const char *pabyKeys = static_cast<const char *>(arrayKeys->buffers[2]);
    const uint8_t *pabyValidityValues =
        arrayValues->null_count == 0
            ? nullptr
            : static_cast<const uint8_t *>(arrayValues->buffers[0]);
    for (uint32_t k = panOffsets[0]; k < panOffsets[1]; k++)
    {
        if (!pabyValidityKeys ||
            TestBit(pabyValidityKeys,
                    k + static_cast<size_t>(arrayKeys->offset)))
        {
            std::string osKey;
            osKey.assign(pabyKeys + panOffsetsKeys[k],
                         panOffsetsKeys[k + 1] - panOffsetsKeys[k]);

            if (!pabyValidityValues ||
                TestBit(pabyValidityValues,
                        k + static_cast<size_t>(arrayValues->offset)))
            {
                AddToDict(oDict, osKey, schemaValues, arrayValues, k);
            }
            else
            {
                oDict.AddNull(osKey);
            }
        }
    }
    return oDict;
}

/************************************************************************/
/*                        GetStructureAsJSON()                          */
/************************************************************************/

static CPLJSONObject GetStructureAsJSON(const struct ArrowSchema *schema,
                                        const struct ArrowArray *array,
                                        const size_t nIdx)
{
    CPLJSONObject oDict;
    for (int64_t k = 0; k < array->n_children; k++)
    {
        const uint8_t *pabyValidityValues =
            array->children[k]->null_count == 0
                ? nullptr
                : static_cast<const uint8_t *>(array->children[k]->buffers[0]);
        if (!pabyValidityValues ||
            TestBit(pabyValidityValues,
                    nIdx + static_cast<size_t>(array->children[k]->offset)))
        {
            AddToDict(oDict, schema->children[k]->name, schema->children[k],
                      array->children[k], nIdx);
        }
        else
        {
            oDict.AddNull(schema->children[k]->name);
        }
    }
    return oDict;
}

/************************************************************************/
/*                        GetObjectAsJSON()                             */
/************************************************************************/

static CPLJSONObject GetObjectAsJSON(const struct ArrowSchema *schema,
                                     const struct ArrowArray *array,
                                     const size_t nIdx)
{
    if (IsMap(schema->format))
        return GetMapAsJSON(schema, array, nIdx);
    else if (IsList(schema->format))
        return GetListAsJSON<uint32_t>(schema, array, nIdx);
    else if (IsLargeList(schema->format))
        return GetListAsJSON<uint64_t>(schema, array, nIdx);
    else if (IsFixedSizeList(schema->format))
        return GetFixedSizeListAsJSON(schema, array, nIdx);
    else if (IsStructure(schema->format))
        return GetStructureAsJSON(schema, array, nIdx);
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetObjectAsJSON(): unhandled value format: %s",
                 schema->format);
        return CPLJSONObject();
    }
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
    if (IsFloat16(format))
    {
        oFeature.SetField(
            iOGRFieldIndex,
            GetValueFloat16(array, nOffsettedIndex -
                                       static_cast<size_t>(array->offset)));
    }

    else if (IsFixedWidthBinary(format))
    {
        // Fixed width binary
        const int nWidth = GetFixedWithBinary(format);
        oFeature.SetField(iOGRFieldIndex, nWidth,
                          static_cast<const GByte *>(array->buffers[1]) +
                              nOffsettedIndex * nWidth);
    }
    else if (format[0] == 't' && format[1] == 'd' &&
             format[2] == 'D')  // strcmp(format, "tdD") == 0
    {
        // date32[days]
        // number of days since Epoch
        int64_t timestamp = static_cast<int64_t>(static_cast<const int32_t *>(
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
    else if (IsTimestampSeconds(format))
    {
        ArrowTimestampToOGRDateTime(
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex], 1,
            GetTimestampTimezone(format), oFeature, iOGRFieldIndex);
    }
    else if (IsTimestampMilliseconds(format))
    {
        ArrowTimestampToOGRDateTime(
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex],
            1000, GetTimestampTimezone(format), oFeature, iOGRFieldIndex);
    }
    else if (IsTimestampMicroseconds(format))
    {
        ArrowTimestampToOGRDateTime(
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex],
            1000 * 1000, GetTimestampTimezone(format), oFeature,
            iOGRFieldIndex);
    }
    else if (IsTimestampNanoseconds(format))
    {
        ArrowTimestampToOGRDateTime(
            static_cast<const int64_t *>(array->buffers[1])[nOffsettedIndex],
            1000 * 1000 * 1000, GetTimestampTimezone(format), oFeature,
            iOGRFieldIndex);
    }
    else if (IsFixedSizeList(format))
    {
        const int nItems = GetFixedSizeList(format);
        const auto childArray = array->children[0];
        const char *childFormat = schema->children[0]->format;
        if (IsBoolean(childFormat))
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
        else if (IsInt8(childFormat))
        {
            FillFieldFixedSizeList<int8_t, int>(array, iOGRFieldIndex,
                                                nOffsettedIndex, nItems,
                                                childArray, oFeature);
        }
        else if (IsUInt8(childFormat))
        {
            FillFieldFixedSizeList<uint8_t, int>(array, iOGRFieldIndex,
                                                 nOffsettedIndex, nItems,
                                                 childArray, oFeature);
        }
        else if (IsInt16(childFormat))
        {
            FillFieldFixedSizeList<int16_t, int>(array, iOGRFieldIndex,
                                                 nOffsettedIndex, nItems,
                                                 childArray, oFeature);
        }
        else if (IsUInt16(childFormat))
        {
            FillFieldFixedSizeList<uint16_t, int>(array, iOGRFieldIndex,
                                                  nOffsettedIndex, nItems,
                                                  childArray, oFeature);
        }
        else if (IsInt32(childFormat))
        {
            FillFieldFixedSizeList<int32_t, int>(array, iOGRFieldIndex,
                                                 nOffsettedIndex, nItems,
                                                 childArray, oFeature);
        }
        else if (IsUInt32(childFormat))
        {
            FillFieldFixedSizeList<uint32_t, GIntBig>(array, iOGRFieldIndex,
                                                      nOffsettedIndex, nItems,
                                                      childArray, oFeature);
        }
        else if (IsInt64(childFormat))
        {
            FillFieldFixedSizeList<int64_t, GIntBig>(array, iOGRFieldIndex,
                                                     nOffsettedIndex, nItems,
                                                     childArray, oFeature);
        }
        else if (IsUInt64(childFormat))
        {
            FillFieldFixedSizeList<uint64_t, double>(array, iOGRFieldIndex,
                                                     nOffsettedIndex, nItems,
                                                     childArray, oFeature);
        }
        else if (IsFloat16(childFormat))
        {
            std::vector<double> aValues;
            for (int i = 0; i < nItems; ++i)
            {
                aValues.push_back(
                    GetValueFloat16(childArray, nOffsettedIndex * nItems + i));
            }
            oFeature.SetField(iOGRFieldIndex, static_cast<int>(aValues.size()),
                              aValues.data());
        }
        else if (IsFloat32(childFormat))
        {
            FillFieldFixedSizeList<float, double>(array, iOGRFieldIndex,
                                                  nOffsettedIndex, nItems,
                                                  childArray, oFeature);
        }
        else if (IsFloat64(childFormat))
        {
            FillFieldFixedSizeList<double, double>(array, iOGRFieldIndex,
                                                   nOffsettedIndex, nItems,
                                                   childArray, oFeature);
        }
        else if (IsString(childFormat))
        {
            FillFieldFixedSizeListString<uint32_t>(array, iOGRFieldIndex,
                                                   nOffsettedIndex, nItems,
                                                   childArray, oFeature);
        }
        else if (IsLargeString(childFormat))
        {
            FillFieldFixedSizeListString<uint64_t>(array, iOGRFieldIndex,
                                                   nOffsettedIndex, nItems,
                                                   childArray, oFeature);
        }
    }
    else if (IsList(format) || IsLargeList(format))
    {
        const auto childArray = array->children[0];
        const char *childFormat = schema->children[0]->format;
        if (IsBoolean(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldListFromBool<uint32_t>(array, iOGRFieldIndex,
                                                nOffsettedIndex, childArray,
                                                oFeature);
            else
                FillFieldListFromBool<uint64_t>(array, iOGRFieldIndex,
                                                nOffsettedIndex, childArray,
                                                oFeature);
        }
        else if (IsInt8(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, int8_t, int>(array, iOGRFieldIndex,
                                                     nOffsettedIndex,
                                                     childArray, oFeature);
            else
                FillFieldList<uint64_t, int8_t, int>(array, iOGRFieldIndex,
                                                     nOffsettedIndex,
                                                     childArray, oFeature);
        }
        else if (IsUInt8(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, uint8_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
            else
                FillFieldList<uint64_t, uint8_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
        }
        else if (IsInt16(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, int16_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
            else
                FillFieldList<uint64_t, int16_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
        }
        else if (IsUInt16(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, uint16_t, int>(array, iOGRFieldIndex,
                                                       nOffsettedIndex,
                                                       childArray, oFeature);
            else
                FillFieldList<uint64_t, uint16_t, int>(array, iOGRFieldIndex,
                                                       nOffsettedIndex,
                                                       childArray, oFeature);
        }
        else if (IsInt32(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, int32_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
            else
                FillFieldList<uint64_t, int32_t, int>(array, iOGRFieldIndex,
                                                      nOffsettedIndex,
                                                      childArray, oFeature);
        }
        else if (IsUInt32(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, uint32_t, GIntBig>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
            else
                FillFieldList<uint64_t, uint32_t, GIntBig>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
        }
        else if (IsInt64(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, int64_t, GIntBig>(array, iOGRFieldIndex,
                                                          nOffsettedIndex,
                                                          childArray, oFeature);
            else
                FillFieldList<uint64_t, int64_t, GIntBig>(array, iOGRFieldIndex,
                                                          nOffsettedIndex,
                                                          childArray, oFeature);
        }
        else if (IsUInt64(childFormat))  // (lossy conversion)
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, uint64_t, double>(array, iOGRFieldIndex,
                                                          nOffsettedIndex,
                                                          childArray, oFeature);
            else
                FillFieldList<uint64_t, uint64_t, double>(array, iOGRFieldIndex,
                                                          nOffsettedIndex,
                                                          childArray, oFeature);
        }
        else if (IsFloat16(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldListFromHalfFloat<uint32_t>(array, iOGRFieldIndex,
                                                     nOffsettedIndex,
                                                     childArray, oFeature);
            else
                FillFieldListFromHalfFloat<uint64_t>(array, iOGRFieldIndex,
                                                     nOffsettedIndex,
                                                     childArray, oFeature);
        }
        else if (IsFloat32(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, float, double>(array, iOGRFieldIndex,
                                                       nOffsettedIndex,
                                                       childArray, oFeature);
            else
                FillFieldList<uint64_t, float, double>(array, iOGRFieldIndex,
                                                       nOffsettedIndex,
                                                       childArray, oFeature);
        }
        else if (IsFloat64(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldList<uint32_t, double, double>(array, iOGRFieldIndex,
                                                        nOffsettedIndex,
                                                        childArray, oFeature);
            else
                FillFieldList<uint64_t, double, double>(array, iOGRFieldIndex,
                                                        nOffsettedIndex,
                                                        childArray, oFeature);
        }
        else if (IsString(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldListFromString<uint32_t, uint32_t>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
            else
                FillFieldListFromString<uint64_t, uint32_t>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
        }
        else if (IsLargeString(childFormat))
        {
            if (format[1] == ARROW_2ND_LETTER_LIST)
                FillFieldListFromString<uint32_t, uint64_t>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
            else
                FillFieldListFromString<uint64_t, uint64_t>(
                    array, iOGRFieldIndex, nOffsettedIndex, childArray,
                    oFeature);
        }
        else if (format[1] == ARROW_2ND_LETTER_LIST)
        {
            const size_t iFeature =
                static_cast<size_t>(nOffsettedIndex - array->offset);
            oFeature.SetField(iOGRFieldIndex,
                              GetListAsJSON<uint32_t>(schema, array, iFeature)
                                  .Format(CPLJSONObject::PrettyFormat::Plain)
                                  .c_str());
        }
        else
        {
            const size_t iFeature =
                static_cast<size_t>(nOffsettedIndex - array->offset);
            oFeature.SetField(iOGRFieldIndex,
                              GetListAsJSON<uint64_t>(schema, array, iFeature)
                                  .Format(CPLJSONObject::PrettyFormat::Plain)
                                  .c_str());
        }
    }
    else if (IsDecimal(format))
    {
        int nPrecision = 0;
        int nScale = 0;
        int nWidthInBytes = 0;
        if (!ParseDecimalFormat(format, nPrecision, nScale, nWidthInBytes))
        {
            CPLAssert(false);
        }

        // fits on a int64
        CPLAssert(nPrecision <= 19);
        // either 128 or 256 bits
        CPLAssert((nWidthInBytes % 8) == 0);
        const int nWidthIn64BitWord = nWidthInBytes / 8;
        const size_t iFeature =
            static_cast<size_t>(nOffsettedIndex - array->offset);
        oFeature.SetField(
            iOGRFieldIndex,
            GetValueDecimal(array, nWidthIn64BitWord, nScale, iFeature));
        return true;
    }
    else if (IsMap(format))
    {
        const size_t iFeature =
            static_cast<size_t>(nOffsettedIndex - array->offset);
        oFeature.SetField(iOGRFieldIndex,
                          GetMapAsJSON(schema, array, iFeature)
                              .Format(CPLJSONObject::PrettyFormat::Plain)
                              .c_str());
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
                else if (IsInt32(format))
                {
                    oFeature.SetFID(static_cast<const int32_t *>(
                        psArray->buffers[1])[nOffsettedIndex]);
                }
                else if (IsInt64(format))
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
            else if (IsBoolean(format))
            {
                oFeature.SetField(
                    iOGRFieldIndex,
                    TestBit(static_cast<const uint8_t *>(psArray->buffers[1]),
                            nOffsettedIndex));
            }
            else if (IsInt8(format))
            {
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const int8_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (IsUInt8(format))
            {
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const uint8_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (IsInt16(format))
            {
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const int16_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (IsUInt16(format))
            {
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const uint16_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (IsInt32(format))
            {
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const int32_t *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (IsUInt32(format))
            {
                oFeature.SetField(
                    iOGRFieldIndex,
                    static_cast<GIntBig>(static_cast<const uint32_t *>(
                        psArray->buffers[1])[nOffsettedIndex]));
            }
            else if (IsInt64(format))
            {
                oFeature.SetField(
                    iOGRFieldIndex,
                    static_cast<GIntBig>(static_cast<const int64_t *>(
                        psArray->buffers[1])[nOffsettedIndex]));
            }
            else if (IsUInt64(format))
            {
                oFeature.SetField(
                    iOGRFieldIndex,
                    static_cast<double>(static_cast<const uint64_t *>(
                        psArray->buffers[1])[nOffsettedIndex]));
            }
            else if (IsFloat32(format))
            {
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const float *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (IsFloat64(format))
            {
                oFeature.SetField(iOGRFieldIndex,
                                  static_cast<const double *>(
                                      psArray->buffers[1])[nOffsettedIndex]);
            }
            else if (IsString(format))
            {
                const auto nOffset = static_cast<const uint32_t *>(
                    psArray->buffers[1])[nOffsettedIndex];
                const auto nNextOffset = static_cast<const uint32_t *>(
                    psArray->buffers[1])[nOffsettedIndex + 1];
                const GByte *pabyData =
                    static_cast<const GByte *>(psArray->buffers[2]);
                const uint32_t nSize = nNextOffset - nOffset;
                CPLAssert(oFeature.GetFieldDefnRef(iOGRFieldIndex)->GetType() ==
                          OFTString);
                char *pszStr = static_cast<char *>(CPLMalloc(nSize + 1));
                memcpy(pszStr, pabyData + nOffset, nSize);
                pszStr[nSize] = 0;
                OGRField *psField = oFeature.GetRawFieldRef(iOGRFieldIndex);
                if (IsValidField(psField))
                    CPLFree(psField->String);
                psField->String = pszStr;
            }
            else if (IsLargeString(format))
            {
                const auto nOffset = static_cast<const uint64_t *>(
                    psArray->buffers[1])[nOffsettedIndex];
                const auto nNextOffset = static_cast<const uint64_t *>(
                    psArray->buffers[1])[nOffsettedIndex + 1];
                const GByte *pabyData =
                    static_cast<const GByte *>(psArray->buffers[2]);
                const size_t nSize = static_cast<size_t>(nNextOffset - nOffset);
                char *pszStr = static_cast<char *>(CPLMalloc(nSize + 1));
                memcpy(pszStr, pabyData + static_cast<size_t>(nOffset), nSize);
                pszStr[nSize] = 0;
                OGRField *psField = oFeature.GetRawFieldRef(iOGRFieldIndex);
                if (IsValidField(psField))
                    CPLFree(psField->String);
                psField->String = pszStr;
            }
            else if (IsBinary(format))
            {
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
            else if (IsLargeBinary(format))
            {
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
        CPLAssert(IsBinary(schema->children[iGeomField]->format) ||
                  IsLargeBinary(schema->children[iGeomField]->format));
        CPLAssert(array->children[iGeomField]->n_buffers == 3);
    }

    std::vector<bool> abyValidityFromFilters;
    const size_t nLength = static_cast<size_t>(array->length);
    const size_t nCountIntersectingGeom =
        m_poFilterGeom ? (IsBinary(schema->children[iGeomField]->format)
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

    if (nCountIntersecting == 0)
    {
        array->length = 0;
    }
    else if (!CompactStructArray(schema, array, 0, abyValidityFromFilters,
                                 nCountIntersecting))
    {
        array->release(array);
        memset(array, 0, sizeof(*array));
    }
}

/************************************************************************/
/*                          OGRCloneArrowArray                          */
/************************************************************************/

static bool OGRCloneArrowArray(const struct ArrowSchema *schema,
                               const struct ArrowArray *src_array,
                               struct ArrowArray *out_array,
                               size_t nParentOffset)
{
    memset(out_array, 0, sizeof(*out_array));
    const size_t nLength =
        static_cast<size_t>(src_array->length) - nParentOffset;
    out_array->length = nLength;
    out_array->null_count = src_array->null_count;
    out_array->release = OGRLayerDefaultReleaseArray;

    bool bRet = true;

    out_array->n_buffers = src_array->n_buffers;
    out_array->buffers = static_cast<const void **>(CPLCalloc(
        static_cast<size_t>(src_array->n_buffers), sizeof(const void *)));
    CPLAssert(static_cast<size_t>(src_array->length) >= nParentOffset);
    const char *format = schema->format;
    const auto nOffset = static_cast<size_t>(src_array->offset) + nParentOffset;
    for (int64_t i = 0; i < src_array->n_buffers; ++i)
    {
        if (i == 0 || IsBoolean(format))
        {
            if (i == 1)
            {
                CPLAssert(src_array->buffers[i]);
            }
            if (src_array->buffers[i])
            {
                const size_t nBytes = nLength ? (nLength + 7) / 8 : 1;
                uint8_t *CPL_RESTRICT p = static_cast<uint8_t *>(
                    VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nBytes));
                if (!p)
                {
                    bRet = false;
                    break;
                }
                const auto *CPL_RESTRICT pSrcArray =
                    static_cast<const uint8_t *>(src_array->buffers[i]);
                if ((nOffset % 8) != 0)
                {
                    // Make sure last byte is fully initialized
                    p[nBytes - 1] = 0;
                    for (size_t iRow = 0; iRow < nLength; ++iRow)
                    {
                        if (TestBit(pSrcArray, nOffset + iRow))
                            SetBit(p, iRow);
                        else
                            UnsetBit(p, iRow);
                    }
                }
                else
                {
                    memcpy(p, pSrcArray + nOffset / 8, nBytes);
                }
                out_array->buffers[i] = p;
            }
        }
        else if (i == 1)
        {
            CPLAssert(src_array->buffers[i]);
            size_t nEltSize = 0;
            size_t nExtraElt = 0;
            if (IsUInt8(format) || IsInt8(format))
                nEltSize = sizeof(uint8_t);
            else if (IsUInt16(format) || IsInt16(format) || IsFloat16(format))
                nEltSize = sizeof(uint16_t);
            else if (IsUInt32(format) || IsInt32(format) || IsFloat32(format) ||
                     strcmp(format, "tdD") == 0 || strcmp(format, "tts") == 0 ||
                     strcmp(format, "ttm") == 0)
            {
                nEltSize = sizeof(uint32_t);
            }
            else if (IsString(format) || IsBinary(format) || IsList(format) ||
                     IsMap(format))
            {
                nEltSize = sizeof(uint32_t);
                nExtraElt = 1;
            }
            else if (IsUInt64(format) || IsInt64(format) || IsFloat64(format) ||
                     strcmp(format, "tdm") == 0 || strcmp(format, "ttu") == 0 ||
                     strcmp(format, "ttn") == 0 || IsTimestamp(format))
            {
                nEltSize = sizeof(uint64_t);
            }
            else if (IsLargeString(format) || IsLargeBinary(format) ||
                     IsLargeList(format))
            {
                nEltSize = sizeof(uint64_t);
                nExtraElt = 1;
            }
            else if (IsFixedWidthBinary(format))
            {
                nEltSize = GetFixedWithBinary(format);
            }
            else if (IsDecimal(format))
            {
                int nPrecision = 0;
                int nScale = 0;
                int nWidthInBytes = 0;
                if (!ParseDecimalFormat(format, nPrecision, nScale,
                                        nWidthInBytes))
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Unexpected error in OGRCloneArrowArray(): unhandled "
                        "field format: %s",
                        format);

                    return false;
                }
                nEltSize = nWidthInBytes;
            }
            if (nEltSize)
            {
                void *p = VSI_MALLOC_ALIGNED_AUTO_VERBOSE(
                    nLength ? nEltSize * (nLength + nExtraElt) : 1);
                if (!p)
                {
                    bRet = false;
                    break;
                }
                if (nLength)
                {
                    if ((IsString(format) || IsBinary(format)) &&
                        static_cast<const uint32_t *>(
                            src_array->buffers[1])[nOffset] != 0)
                    {
                        const auto *CPL_RESTRICT pSrcOffsets =
                            static_cast<const uint32_t *>(
                                src_array->buffers[1]) +
                            nOffset;
                        const auto nShiftOffset = pSrcOffsets[0];
                        auto *CPL_RESTRICT pDstOffsets =
                            static_cast<uint32_t *>(p);
                        for (size_t iRow = 0; iRow <= nLength; ++iRow)
                        {
                            pDstOffsets[iRow] =
                                pSrcOffsets[iRow] - nShiftOffset;
                        }
                    }
                    else if ((IsLargeString(format) || IsLargeBinary(format)) &&
                             static_cast<const uint64_t *>(
                                 src_array->buffers[1])[nOffset] != 0)
                    {
                        const auto *CPL_RESTRICT pSrcOffsets =
                            static_cast<const uint64_t *>(
                                src_array->buffers[1]) +
                            nOffset;
                        const auto nShiftOffset = pSrcOffsets[0];
                        auto *CPL_RESTRICT pDstOffsets =
                            static_cast<uint64_t *>(p);
                        for (size_t iRow = 0; iRow <= nLength; ++iRow)
                        {
                            pDstOffsets[iRow] =
                                pSrcOffsets[iRow] - nShiftOffset;
                        }
                    }
                    else
                    {
                        memcpy(
                            p,
                            static_cast<const GByte *>(src_array->buffers[i]) +
                                nEltSize * nOffset,
                            nEltSize * (nLength + nExtraElt));
                    }
                }
                out_array->buffers[i] = p;
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "OGRCloneArrowArray(): unhandled case, array = %s, "
                         "format = '%s', i = 1",
                         schema->name, format);
                bRet = false;
                break;
            }
        }
        else if (i == 2)
        {
            CPLAssert(src_array->buffers[i]);
            size_t nSrcCharOffset = 0;
            size_t nCharCount = 0;
            if (IsString(format) || IsBinary(format))
            {
                const auto *pSrcOffsets =
                    static_cast<const uint32_t *>(src_array->buffers[1]) +
                    nOffset;
                nSrcCharOffset = pSrcOffsets[0];
                nCharCount = pSrcOffsets[nLength] - pSrcOffsets[0];
            }
            else if (IsLargeString(format) || IsLargeBinary(format))
            {
                const auto *pSrcOffsets =
                    static_cast<const uint64_t *>(src_array->buffers[1]) +
                    nOffset;
                nSrcCharOffset = static_cast<size_t>(pSrcOffsets[0]);
                nCharCount =
                    static_cast<size_t>(pSrcOffsets[nLength] - pSrcOffsets[0]);
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "OGRCloneArrowArray(): unhandled case, array = %s, "
                         "format = '%s', i = 2",
                         schema->name, format);
                bRet = false;
                break;
            }
            void *p =
                VSI_MALLOC_ALIGNED_AUTO_VERBOSE(nCharCount ? nCharCount : 1);
            if (!p)
            {
                bRet = false;
                break;
            }
            if (nCharCount)
            {
                memcpy(p,
                       static_cast<const GByte *>(src_array->buffers[i]) +
                           nSrcCharOffset,
                       nCharCount);
            }
            out_array->buffers[i] = p;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "OGRCloneArrowArray(): unhandled case, array = %s, format "
                     "= '%s', i = 3",
                     schema->name, format);
            bRet = false;
            break;
        }
    }

    if (bRet)
    {
        out_array->n_children = src_array->n_children;
        out_array->children = static_cast<struct ArrowArray **>(
            CPLCalloc(static_cast<size_t>(src_array->n_children),
                      sizeof(struct ArrowArray *)));
        for (int64_t i = 0; i < src_array->n_children; ++i)
        {
            out_array->children[i] = static_cast<struct ArrowArray *>(
                CPLCalloc(1, sizeof(struct ArrowArray)));
            if (!OGRCloneArrowArray(schema->children[i], src_array->children[i],
                                    out_array->children[i],
                                    IsFixedSizeList(format)
                                        ? nOffset * GetFixedSizeList(format)
                                    : IsStructure(format) ? nOffset
                                                          : 0))
            {
                bRet = false;
                break;
            }
        }
    }

    if (bRet && src_array->dictionary)
    {
        out_array->dictionary = static_cast<struct ArrowArray *>(
            CPLCalloc(1, sizeof(struct ArrowArray)));
        bRet = OGRCloneArrowArray(schema->dictionary, src_array->dictionary,
                                  out_array->dictionary, 0);
    }

    if (!bRet)
    {
        out_array->release(out_array);
        memset(out_array, 0, sizeof(*out_array));
    }
    return bRet;
}

/** Full/deep copy of an array.
 *
 * Renormalize the offset of the array (and its children) to 0.
 *
 * In case of failure, out_array will be let in a released state.
 *
 * @param schema Schema of the array. Must *NOT* be NULL.
 * @param src_array Source array. Must *NOT* be NULL.
 * @param out_array Output array.  Must *NOT* be NULL (but its content may be random)
 * @return true if success.
 */
bool OGRCloneArrowArray(const struct ArrowSchema *schema,
                        const struct ArrowArray *src_array,
                        struct ArrowArray *out_array)
{
    return OGRCloneArrowArray(schema, src_array, out_array, 0);
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
    const char arrowLetter;
    OGRFieldType eType;
    OGRFieldSubType eSubType;
} gasListTypes[] = {
    {ARROW_LETTER_BOOLEAN, OFTIntegerList, OFSTBoolean},
    {ARROW_LETTER_INT8, OFTIntegerList, OFSTInt16},
    {ARROW_LETTER_UINT8, OFTIntegerList, OFSTInt16},
    {ARROW_LETTER_INT16, OFTIntegerList, OFSTInt16},
    {ARROW_LETTER_UINT16, OFTIntegerList, OFSTNone},
    {ARROW_LETTER_INT32, OFTIntegerList, OFSTNone},
    {ARROW_LETTER_UINT32, OFTInteger64List, OFSTNone},
    {ARROW_LETTER_INT64, OFTInteger64List, OFSTNone},
    {ARROW_LETTER_UINT64, OFTRealList,
     OFSTNone},  //(potentially lossy conversion if going through OGRFeature)
    {ARROW_LETTER_FLOAT16, OFTRealList, OFSTFloat32},
    {ARROW_LETTER_FLOAT32, OFTRealList, OFSTFloat32},
    {ARROW_LETTER_FLOAT64, OFTRealList, OFSTNone},
    {ARROW_LETTER_STRING, OFTStringList, OFSTNone},
    {ARROW_LETTER_LARGE_STRING, OFTStringList, OFSTNone},
};

static inline bool IsValidDictionaryIndexType(const char *format)
{
    return (format[0] == ARROW_LETTER_INT8 || format[0] == ARROW_LETTER_UINT8 ||
            format[0] == ARROW_LETTER_INT16 ||
            format[0] == ARROW_LETTER_UINT16 ||
            format[0] == ARROW_LETTER_INT32 ||
            format[0] == ARROW_LETTER_UINT32 ||
            format[0] == ARROW_LETTER_INT64 ||
            format[0] == ARROW_LETTER_UINT64) &&
           format[1] == 0;
}

static bool IsSupportForJSONObj(const struct ArrowSchema *schema)
{
    const char *format = schema->format;
    if (IsStructure(format))
    {
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!IsSupportForJSONObj(schema->children[i]))
                return false;
        }
        return true;
    }

    for (const auto &sType : gasListTypes)
    {
        if (format[0] == sType.arrowLetter && format[1] == 0)
        {
            return true;
        }
    }

    if (IsBinary(format) || IsLargeBinary(format) || IsFixedWidthBinary(format))
        return true;

    if (IsDecimal(format))
    {
        int nPrecision = 0;
        int nScale = 0;
        int nWidthInBytes = 0;
        if (!ParseDecimalFormat(format, nPrecision, nScale, nWidthInBytes))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid field format %s",
                     format);
            return false;
        }

        return GetErrorIfUnsupportedDecimal(nWidthInBytes, nPrecision) ==
               nullptr;
    }

    if (IsMap(format))
    {
        return IsStructure(schema->children[0]->format) &&
               schema->children[0]->n_children == 2 &&
               IsString(schema->children[0]->children[0]->format) &&
               IsSupportForJSONObj(schema->children[0]->children[1]);
    }

    if (IsList(format) || IsLargeList(format) || IsFixedSizeList(format))
    {
        return IsSupportForJSONObj(schema->children[0]);
    }

    return false;
}

static bool IsArrowSchemaSupportedInternal(const struct ArrowSchema *schema,
                                           const std::string &osFieldPrefix,
                                           std::string &osErrorMsg)
{
    const auto AppendError = [&osErrorMsg](const std::string &osMsg)
    {
        if (!osErrorMsg.empty())
            osErrorMsg += " ";
        osErrorMsg += osMsg;
    };

    const char *fieldName = schema->name;
    const char *format = schema->format;
    if (IsStructure(format))
    {
        bool bRet = true;
        const std::string osNewPrefix(osFieldPrefix + fieldName + ".");
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!IsArrowSchemaSupportedInternal(schema->children[i],
                                                osNewPrefix, osErrorMsg))
                bRet = false;
        }
        return bRet;
    }

    if (schema->dictionary)
    {
        if (!IsValidDictionaryIndexType(format))
        {
            AppendError("Dictionary only supported if the parent is of "
                        "type [U]Int[8|16|32|64]");
            return false;
        }

        schema = schema->dictionary;
        format = schema->format;
    }

    if (IsList(format) || IsLargeList(format) || IsFixedSizeList(format))
    {
        // Only some subtypes supported
        const char *childFormat = schema->children[0]->format;
        for (const auto &sType : gasListTypes)
        {
            if (childFormat[0] == sType.arrowLetter && childFormat[1] == 0)
            {
                return true;
            }
        }

        if (IsDecimal(childFormat))
        {
            int nPrecision = 0;
            int nScale = 0;
            int nWidthInBytes = 0;
            if (!ParseDecimalFormat(childFormat, nPrecision, nScale,
                                    nWidthInBytes))
            {
                AppendError(std::string("Invalid field format ") + childFormat +
                            " for field " + osFieldPrefix + fieldName);
                return false;
            }

            const char *pszError =
                GetErrorIfUnsupportedDecimal(nWidthInBytes, nPrecision);
            if (pszError)
            {
                AppendError(pszError);
                return false;
            }

            return true;
        }

        if (IsSupportForJSONObj(schema))
        {
            return true;
        }

        AppendError("Type list for field " + osFieldPrefix + fieldName +
                    " is not supported.");
        return false;
    }

    else if (IsMap(format))
    {
        if (IsSupportForJSONObj(schema))
            return true;

        AppendError("Type map for field " + osFieldPrefix + fieldName +
                    " is not supported.");
        return false;
    }
    else if (IsDecimal(format))
    {
        int nPrecision = 0;
        int nScale = 0;
        int nWidthInBytes = 0;
        if (!ParseDecimalFormat(format, nPrecision, nScale, nWidthInBytes))
        {
            AppendError(std::string("Invalid field format ") + format +
                        " for field " + osFieldPrefix + fieldName);
            return false;
        }

        const char *pszError =
            GetErrorIfUnsupportedDecimal(nWidthInBytes, nPrecision);
        if (pszError)
        {
            AppendError(pszError);
            return false;
        }

        return true;
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

        if (IsFixedWidthBinary(format) || IsTimestamp(format))
            return true;

        AppendError("Type '" + std::string(format) + "' for field " +
                    osFieldPrefix + fieldName + " is not supported.");
        return false;
    }
}

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
 * @param papszOptions Options (none currently). Null terminated list, or nullptr.
 * @param[out] osErrorMsg Reason of the failure, when this method returns false.
 * @return true if the ArrowSchema is supported for writing.
 * @since 3.8
 */
bool OGRLayer::IsArrowSchemaSupported(const struct ArrowSchema *schema,
                                      CPL_UNUSED CSLConstList papszOptions,
                                      std::string &osErrorMsg) const
{
    if (!IsStructure(schema->format))
    {
        osErrorMsg =
            "IsArrowSchemaSupported() should be called on a schema that is a "
            "struct of fields";
        return false;
    }

    bool bRet = true;
    for (int64_t i = 0; i < schema->n_children; ++i)
    {
        if (!IsArrowSchemaSupportedInternal(schema->children[i], std::string(),
                                            osErrorMsg))
            bRet = false;
    }
    return bRet;
}

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
 * @param papszOptions Options (none currently). Null terminated list, or nullptr.
 * @param[out] ppszErrorMsg nullptr, or pointer to a string that will contain
 * the reason of the failure, when this function returns false.
 * @return true if the ArrowSchema is supported for writing.
 * @since 3.8
 */
bool OGR_L_IsArrowSchemaSupported(OGRLayerH hLayer,
                                  const struct ArrowSchema *schema,
                                  char **papszOptions, char **ppszErrorMsg)
{
    VALIDATE_POINTER1(hLayer, __func__, false);
    VALIDATE_POINTER1(schema, __func__, false);

    std::string osErrorMsg;
    if (!OGRLayer::FromHandle(hLayer)->IsArrowSchemaSupported(
            schema, papszOptions, osErrorMsg))
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
/*                     IsKnownCodedFieldDomain()                        */
/************************************************************************/

static bool IsKnownCodedFieldDomain(OGRLayer *poLayer,
                                    const char *arrowMetadata)
{
    if (arrowMetadata)
    {
        const auto oMetadata = OGRParseArrowMetadata(arrowMetadata);
        for (const auto &oIter : oMetadata)
        {
            if (oIter.first == MD_GDAL_OGR_DOMAIN_NAME)
            {
                auto poDS = poLayer->GetDataset();
                if (poDS)
                {
                    const auto poFieldDomain =
                        poDS->GetFieldDomain(oIter.second);
                    if (poFieldDomain &&
                        poFieldDomain->GetDomainType() == OFDT_CODED)
                    {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

/************************************************************************/
/*                OGRLayer::CreateFieldFromArrowSchema()                */
/************************************************************************/

//! @cond Doxygen_Suppress
bool OGRLayer::CreateFieldFromArrowSchemaInternal(
    const struct ArrowSchema *schema, const std::string &osFieldPrefix,
    CSLConstList papszOptions)
{
    const char *fieldName = schema->name;
    const char *format = schema->format;
    if (IsStructure(format))
    {
        const std::string osNewPrefix(osFieldPrefix + fieldName + ".");
        for (int64_t i = 0; i < schema->n_children; ++i)
        {
            if (!CreateFieldFromArrowSchemaInternal(schema->children[i],
                                                    osNewPrefix, papszOptions))
                return false;
        }
        return true;
    }

    CPLStringList aosNativeTypes;
    auto poLayer = const_cast<OGRLayer *>(this);
    auto poDS = poLayer->GetDataset();
    if (poDS)
    {
        auto poDriver = poDS->GetDriver();
        if (poDriver)
        {
            const char *pszMetadataItem =
                poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES);
            if (pszMetadataItem)
                aosNativeTypes = CSLTokenizeString2(pszMetadataItem, " ", 0);
        }
    }

    if (schema->dictionary &&
        !IsKnownCodedFieldDomain(poLayer, schema->metadata))
    {
        if (!IsValidDictionaryIndexType(format))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Dictionary only supported if the parent is of "
                     "type [U]Int[8|16|32|64]");
            return false;
        }

        schema = schema->dictionary;
        format = schema->format;
    }

    const auto AddField = [this, schema, fieldName, &aosNativeTypes,
                           &osFieldPrefix, poDS](OGRFieldType eTypeIn,
                                                 OGRFieldSubType eSubTypeIn,
                                                 int nWidth, int nPrecision)
    {
        const char *pszTypeName = OGRFieldDefn::GetFieldTypeName(eTypeIn);
        auto eTypeOut = eTypeIn;
        auto eSubTypeOut = eSubTypeIn;
        if (!aosNativeTypes.empty() &&
            aosNativeTypes.FindString(pszTypeName) < 0)
        {
            eTypeOut = OFTString;
            eSubTypeOut =
                (eTypeIn == OFTIntegerList || eTypeIn == OFTInteger64List ||
                 eTypeIn == OFTRealList || eTypeIn == OFTStringList)
                    ? OFSTJSON
                    : OFSTNone;
        }

        const std::string osWantedOGRFieldName = osFieldPrefix + fieldName;
        OGRFieldDefn oFieldDefn(osWantedOGRFieldName.c_str(), eTypeOut);
        oFieldDefn.SetSubType(eSubTypeOut);
        if (eTypeOut == eTypeIn && eSubTypeOut == eSubTypeIn)
        {
            oFieldDefn.SetWidth(nWidth);
            oFieldDefn.SetPrecision(nPrecision);
        }
        oFieldDefn.SetNullable((schema->flags & ARROW_FLAG_NULLABLE) != 0);

        if (schema->metadata)
        {
            const auto oMetadata = OGRParseArrowMetadata(schema->metadata);
            for (const auto &oIter : oMetadata)
            {
                if (oIter.first == MD_GDAL_OGR_ALTERNATIVE_NAME)
                    oFieldDefn.SetAlternativeName(oIter.second.c_str());
                else if (oIter.first == MD_GDAL_OGR_COMMENT)
                    oFieldDefn.SetComment(oIter.second);
                else if (oIter.first == MD_GDAL_OGR_DEFAULT)
                    oFieldDefn.SetDefault(oIter.second.c_str());
                else if (oIter.first == MD_GDAL_OGR_SUBTYPE)
                {
                    if (eTypeIn == eTypeOut)
                    {
                        const auto &osSubType = oIter.second;
                        for (auto eSubType = OFSTNone;
                             eSubType <= OFSTMaxSubType;)
                        {
                            if (OGRFieldDefn::GetFieldSubTypeName(eSubType) ==
                                osSubType)
                            {
                                oFieldDefn.SetSubType(eSubType);
                                break;
                            }
                            if (eSubType == OFSTMaxSubType)
                                break;
                            else
                                eSubType =
                                    static_cast<OGRFieldSubType>(eSubType + 1);
                        }
                    }
                }
                else if (oIter.first == MD_GDAL_OGR_WIDTH)
                    oFieldDefn.SetWidth(atoi(oIter.second.c_str()));
                else if (oIter.first == MD_GDAL_OGR_UNIQUE)
                    oFieldDefn.SetUnique(oIter.second == "true");
                else if (oIter.first == MD_GDAL_OGR_DOMAIN_NAME)
                {
                    if (poDS && poDS->GetFieldDomain(oIter.second))
                        oFieldDefn.SetDomainName(oIter.second);
                }
                else if (oIter.first == ARROW_EXTENSION_NAME_KEY &&
                         oIter.second == EXTENSION_NAME_ARROW_JSON)
                {
                    oFieldDefn.SetSubType(OFSTJSON);
                }
                else
                {
                    CPLDebug("OGR", "Unknown field metadata: %s",
                             oIter.first.c_str());
                }
            }
        }
        auto poLayerDefn = GetLayerDefn();
        const int nFieldCountBefore = poLayerDefn->GetFieldCount();
        if (CreateField(&oFieldDefn) != OGRERR_NONE ||
            nFieldCountBefore + 1 != poLayerDefn->GetFieldCount())
        {
            return false;
        }
        const char *pszActualFieldName =
            poLayerDefn->GetFieldDefn(nFieldCountBefore)->GetNameRef();
        if (pszActualFieldName != osWantedOGRFieldName)
        {
            m_poPrivate
                ->m_oMapArrowFieldNameToOGRFieldName[osWantedOGRFieldName] =
                pszActualFieldName;
        }
        return true;
    };

    for (const auto &sType : gasArrowTypesToOGR)
    {
        if (strcmp(format, sType.arrowType) == 0)
        {
            return AddField(sType.eType, sType.eSubType, 0, 0);
        }
    }

    if (IsMap(format))
    {
        return AddField(OFTString, OFSTJSON, 0, 0);
    }

    if (IsTimestamp(format))
    {
        return AddField(OFTDateTime, OFSTNone, 0, 0);
    }

    if (IsFixedWidthBinary(format))
    {
        return AddField(OFTBinary, OFSTNone, GetFixedWithBinary(format), 0);
    }

    if (IsList(format) || IsLargeList(format) || IsFixedSizeList(format))
    {
        const char *childFormat = schema->children[0]->format;
        for (const auto &sType : gasListTypes)
        {
            if (childFormat[0] == sType.arrowLetter && childFormat[1] == 0)
            {
                return AddField(sType.eType, sType.eSubType, 0, 0);
            }
        }

        if (IsDecimal(childFormat))
        {
            int nPrecision = 0;
            int nScale = 0;
            int nWidthInBytes = 0;
            if (!ParseDecimalFormat(childFormat, nPrecision, nScale,
                                    nWidthInBytes))
            {
                CPLError(CE_Failure, CPLE_AppDefined, "%s",
                         (std::string("Invalid field format ") + format +
                          " for field " + osFieldPrefix + fieldName)
                             .c_str());
                return false;
            }

            const char *pszError =
                GetErrorIfUnsupportedDecimal(nWidthInBytes, nPrecision);
            if (pszError)
            {
                CPLError(CE_Failure, CPLE_NotSupported, "%s", pszError);
                return false;
            }

            // DBF convention: add space for negative sign and decimal separator
            return AddField(OFTRealList, OFSTNone, nPrecision + 2, nScale);
        }

        if (IsSupportForJSONObj(schema->children[0]))
        {
            return AddField(OFTString, OFSTJSON, 0, 0);
        }

        CPLError(CE_Failure, CPLE_NotSupported, "%s",
                 ("List of type '" + std::string(childFormat) + "' for field " +
                  osFieldPrefix + fieldName + " is not supported.")
                     .c_str());
        return false;
    }

    if (IsDecimal(format))
    {
        int nPrecision = 0;
        int nScale = 0;
        int nWidthInBytes = 0;
        if (!ParseDecimalFormat(format, nPrecision, nScale, nWidthInBytes))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     (std::string("Invalid field format ") + format +
                      " for field " + osFieldPrefix + fieldName)
                         .c_str());
            return false;
        }

        const char *pszError =
            GetErrorIfUnsupportedDecimal(nWidthInBytes, nPrecision);
        if (pszError)
        {
            CPLError(CE_Failure, CPLE_NotSupported, "%s", pszError);
            return false;
        }

        // DBF convention: add space for negative sign and decimal separator
        return AddField(OFTReal, OFSTNone, nPrecision + 2, nScale);
    }

    CPLError(CE_Failure, CPLE_NotSupported, "%s",
             ("Type '" + std::string(format) + "' for field " + osFieldPrefix +
              fieldName + " is not supported.")
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
 * Additional field metadata can be speciffed through the ArrowSchema::metadata
 * field with the potential following items:
 * <ul>
 * <li>"GDAL:OGR:alternative_name": value of
 *     OGRFieldDefn::GetAlternativeNameRef()</li>
 * <li>"GDAL:OGR:comment": value of OGRFieldDefn::GetComment()</li>
 * <li>"GDAL:OGR:default": value of OGRFieldDefn::GetDefault()</li>
 * <li>"GDAL:OGR:subtype": value of OGRFieldDefn::GetSubType()</li>
 * <li>"GDAL:OGR:width": value of OGRFieldDefn::GetWidth() (serialized as a
 *     string)</li>
 * <li>"GDAL:OGR:unique": value of OGRFieldDefn::IsUnique() (serialized as
 *     "true" or "false")</li>
 * <li>"GDAL:OGR:domain_name": value of OGRFieldDefn::GetDomainName()</li>
 * </ul>
 *
 * This method and CreateField() are mutually exclusive in the same session.
 *
 * This method is the same as the C function OGR_L_CreateFieldFromArrowSchema().
 *
 * @param schema Schema of the field to create.
 * @param papszOptions Options (none currently). Null terminated list, or nullptr.
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
 * Additional field metadata can be speciffed through the ArrowSchema::metadata
 * field with the potential following items:
 * <ul>
 * <li>"GDAL:OGR:alternative_name": value of
 *     OGRFieldDefn::GetAlternativeNameRef()</li>
 * <li>"GDAL:OGR:comment": value of OGRFieldDefn::GetComment()</li>
 * <li>"GDAL:OGR:default": value of OGRFieldDefn::GetDefault()</li>
 * <li>"GDAL:OGR:subtype": value of OGRFieldDefn::GetSubType()</li>
 * <li>"GDAL:OGR:width": value of OGRFieldDefn::GetWidth() (serialized as a
 *     string)</li>
 * <li>"GDAL:OGR:unique": value of OGRFieldDefn::IsUnique() (serialized as
 *     "true" or "false")</li>
 * <li>"GDAL:OGR:domain_name": value of OGRFieldDefn::GetDomainName()</li>
 * </ul>
 *
 * This method and CreateField() are mutually exclusive in the same session.
 *
 * This method is the same as the C++ method OGRLayer::CreateFieldFromArrowSchema().
 *
 * @param hLayer Layer.
 * @param schema Schema of the field to create.
 * @param papszOptions Options (none currently). Null terminated list, or nullptr.
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
    const char *format = nullptr;
    OGRFieldType eNominalFieldType =
        OFTMaxType;  // OGR data type that would best match the Arrow type
    OGRFieldType eTargetFieldType =
        OFTMaxType;  // actual OGR data type of the layer field
    // OGR data type of the feature passed to FillFeature()
    OGRFieldType eSetFeatureFieldType = OFTMaxType;
    bool bIsGeomCol = false;
    bool bUseDictionary = false;
    bool bUseStringOptim = false;
    int nWidthInBytes = 0;  // only used for decimal fields
    int nPrecision = 0;     // only used for decimal fields
    int nScale = 0;         // only used for decimal fields
};

static bool BuildOGRFieldInfo(
    const struct ArrowSchema *schema, struct ArrowArray *array,
    const OGRFeatureDefn *poFeatureDefn, const std::string &osFieldPrefix,
    const CPLStringList &aosNativeTypes, bool &bFallbackTypesUsed,
    std::vector<FieldInfo> &asFieldInfo, const char *pszFIDName,
    const char *pszGeomFieldName, OGRLayer *poLayer,
    const std::map<std::string, std::string> &oMapArrowFieldNameToOGRFieldName,
    const struct ArrowSchema *&schemaFIDColumn,
    struct ArrowArray *&arrayFIDColumn)
{
    const char *fieldName = schema->name;
    const char *format = schema->format;
    if (IsStructure(format))
    {
        const std::string osNewPrefix(osFieldPrefix + fieldName + ".");
        for (int64_t i = 0; i < array->n_children; ++i)
        {
            if (!BuildOGRFieldInfo(schema->children[i], array->children[i],
                                   poFeatureDefn, osNewPrefix, aosNativeTypes,
                                   bFallbackTypesUsed, asFieldInfo, pszFIDName,
                                   pszGeomFieldName, poLayer,
                                   oMapArrowFieldNameToOGRFieldName,
                                   schemaFIDColumn, arrayFIDColumn))
            {
                return false;
            }
        }
        return true;
    }

    FieldInfo sInfo;

    if (schema->dictionary &&
        !IsKnownCodedFieldDomain(poLayer, schema->metadata))
    {
        if (!IsValidDictionaryIndexType(format))
        {
            CPLError(CE_Failure, CPLE_NotSupported,
                     "Dictionary only supported if the parent is of "
                     "type [U]Int[8|16|32|64]");
            return false;
        }

        sInfo.bUseDictionary = true;
        schema = schema->dictionary;
        format = schema->format;
        array = array->dictionary;
    }

    sInfo.osName = osFieldPrefix + fieldName;
    sInfo.format = format;
    if (pszFIDName && sInfo.osName == pszFIDName)
    {
        if (IsInt32(format) || IsInt64(format))
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
        const std::string &osExpectedOGRFieldName =
            [&oMapArrowFieldNameToOGRFieldName, &sInfo]() -> const std::string &
        {
            const auto oIter =
                oMapArrowFieldNameToOGRFieldName.find(sInfo.osName);
            if (oIter != oMapArrowFieldNameToOGRFieldName.end())
                return oIter->second;
            return sInfo.osName;
        }();
        sInfo.iOGRFieldIdx =
            poFeatureDefn->GetFieldIndex(osExpectedOGRFieldName.c_str());
        if (sInfo.iOGRFieldIdx >= 0)
        {
            bool bTypeOK = false;
            const auto eOGRType =
                poFeatureDefn->GetFieldDefn(sInfo.iOGRFieldIdx)->GetType();
            sInfo.eTargetFieldType = eOGRType;
            for (const auto &sType : gasArrowTypesToOGR)
            {
                if (strcmp(format, sType.arrowType) == 0)
                {
                    sInfo.bUseStringOptim = sType.eType == OFTString;
                    sInfo.eNominalFieldType = sType.eType;
                    if (eOGRType == sInfo.eNominalFieldType)
                    {
                        bTypeOK = true;
                        break;
                    }
                    else if (eOGRType == OFTString)
                    {
                        bFallbackTypesUsed = true;
                        bTypeOK = true;
                        break;
                    }
                    else if (eOGRType == OFTInteger &&
                             sType.eType == OFTInteger64)
                    {
                        // Potentially lossy.
                        CPLDebug("OGR",
                                 "For field %s, writing from Arrow array of "
                                 "type Int64 into OGR Int32 field. "
                                 "Potentially loss conversion can happen",
                                 sInfo.osName.c_str());
                        bFallbackTypesUsed = true;
                        bTypeOK = true;
                        break;
                    }
                    else if (eOGRType == OFTInteger && sType.eType == OFTReal)
                    {
                        // Potentially lossy.
                        CPLDebug("OGR",
                                 "For field %s, writing from Arrow array of "
                                 "type Real into OGR Int32 field. "
                                 "Potentially loss conversion can happen",
                                 sInfo.osName.c_str());
                        bFallbackTypesUsed = true;
                        bTypeOK = true;
                        break;
                    }
                    else if (eOGRType == OFTInteger64 && sType.eType == OFTReal)
                    {
                        // Potentially lossy.
                        CPLDebug("OGR",
                                 "For field %s, writing from Arrow array of "
                                 "type Real into OGR Int64 field. "
                                 "Potentially loss conversion can happen",
                                 sInfo.osName.c_str());
                        bFallbackTypesUsed = true;
                        bTypeOK = true;
                        break;
                    }
                    else if (eOGRType == OFTReal && sType.eType == OFTInteger64)
                    {
                        // Potentially lossy.
                        CPLDebug("OGR",
                                 "For field %s, writing from Arrow array of "
                                 "type Int64 into OGR Real field. "
                                 "Potentially loss conversion can happen",
                                 sInfo.osName.c_str());
                        bFallbackTypesUsed = true;
                        bTypeOK = true;
                        break;
                    }
                    else if ((eOGRType == OFTInteger64 ||
                              eOGRType == OFTReal) &&
                             sType.eType == OFTInteger)
                    {
                        // Non-lossy
                        bFallbackTypesUsed = true;
                        bTypeOK = true;
                        break;
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "For field %s, OGR field type is %s whereas "
                                 "Arrow type implies %s",
                                 sInfo.osName.c_str(),
                                 OGR_GetFieldTypeName(eOGRType),
                                 OGR_GetFieldTypeName(sType.eType));
                        return false;
                    }
                }
            }

            if (!bTypeOK && IsMap(format))
            {
                sInfo.eNominalFieldType = OFTString;
                if (eOGRType == sInfo.eNominalFieldType)
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
                             OGR_GetFieldTypeName(OFTString));
                    return false;
                }
            }

            if (!bTypeOK && IsTimestamp(format))
            {
                sInfo.eNominalFieldType = OFTDateTime;
                if (eOGRType == sInfo.eNominalFieldType)
                {
                    bTypeOK = true;
                }
                else if (eOGRType == OFTString)
                {
                    bFallbackTypesUsed = true;
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

            if (!bTypeOK && IsFixedWidthBinary(format))
            {
                sInfo.eNominalFieldType = OFTBinary;
                if (eOGRType == sInfo.eNominalFieldType)
                {
                    bTypeOK = true;
                }
                else if (eOGRType == OFTString)
                {
                    bFallbackTypesUsed = true;
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

            if (!bTypeOK && (IsList(format) || IsLargeList(format) ||
                             IsFixedSizeList(format)))
            {
                const char *childFormat = schema->children[0]->format;
                for (const auto &sType : gasListTypes)
                {
                    if (childFormat[0] == sType.arrowLetter &&
                        childFormat[1] == 0)
                    {
                        sInfo.eNominalFieldType = sType.eType;
                        if (eOGRType == sInfo.eNominalFieldType)
                        {
                            bTypeOK = true;
                            break;
                        }
                        else if (eOGRType == OFTString)
                        {
                            bFallbackTypesUsed = true;
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

                if (!bTypeOK && IsDecimal(childFormat))
                {
                    if (!ParseDecimalFormat(childFormat, sInfo.nPrecision,
                                            sInfo.nScale, sInfo.nWidthInBytes))
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                                 (std::string("Invalid field format ") +
                                  childFormat + " for field " + osFieldPrefix +
                                  fieldName)
                                     .c_str());
                        return false;
                    }

                    const char *pszError = GetErrorIfUnsupportedDecimal(
                        sInfo.nWidthInBytes, sInfo.nPrecision);
                    if (pszError)
                    {
                        CPLError(CE_Failure, CPLE_NotSupported, "%s", pszError);
                        return false;
                    }

                    sInfo.eNominalFieldType = OFTRealList;
                    if (eOGRType == sInfo.eNominalFieldType)
                    {
                        bTypeOK = true;
                    }
                    else if (eOGRType == OFTString)
                    {
                        bFallbackTypesUsed = true;
                        bTypeOK = true;
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "For field %s, OGR field type is %s whereas "
                                 "Arrow type implies %s",
                                 sInfo.osName.c_str(),
                                 OGR_GetFieldTypeName(eOGRType),
                                 OGR_GetFieldTypeName(OFTRealList));
                        return false;
                    }
                }

                if (!bTypeOK && IsSupportForJSONObj(schema->children[0]))
                {
                    sInfo.eNominalFieldType = OFTString;
                    if (eOGRType == sInfo.eNominalFieldType)
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
                                 OGR_GetFieldTypeName(OFTString));
                        return false;
                    }
                }

                if (!bTypeOK)
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "%s",
                             ("List of type '" + std::string(childFormat) +
                              "' for field " + osFieldPrefix + fieldName +
                              " is not supported.")
                                 .c_str());
                    return false;
                }
            }

            if (!bTypeOK && IsDecimal(format))
            {
                if (!ParseDecimalFormat(format, sInfo.nPrecision, sInfo.nScale,
                                        sInfo.nWidthInBytes))
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "%s",
                             (std::string("Invalid field format ") + format +
                              " for field " + osFieldPrefix + fieldName)
                                 .c_str());
                    return false;
                }

                const char *pszError = GetErrorIfUnsupportedDecimal(
                    sInfo.nWidthInBytes, sInfo.nPrecision);
                if (pszError)
                {
                    CPLError(CE_Failure, CPLE_NotSupported, "%s", pszError);
                    return false;
                }

                sInfo.eNominalFieldType = OFTReal;
                if (eOGRType == sInfo.eNominalFieldType)
                {
                    bTypeOK = true;
                }
                else if (eOGRType == OFTString)
                {
                    bFallbackTypesUsed = true;
                    bTypeOK = true;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "For field %s, OGR field type is %s whereas "
                             "Arrow type implies %s",
                             sInfo.osName.c_str(),
                             OGR_GetFieldTypeName(eOGRType),
                             OGR_GetFieldTypeName(OFTReal));
                    return false;
                }
            }

            if (!bTypeOK)
            {
                CPLError(CE_Failure, CPLE_NotSupported, "%s",
                         ("Type '" + std::string(format) + "' for field " +
                          osFieldPrefix + fieldName + " is not supported.")
                             .c_str());
                return false;
            }
        }
        else
        {
            sInfo.iOGRFieldIdx = poFeatureDefn->GetGeomFieldIndex(
                osExpectedOGRFieldName.c_str());
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
                    // Check if ARROW:extension:name = ogc.wkb or geoarrow.wkb
                    const char *pabyMetadata = schema->metadata;
                    if (pabyMetadata)
                    {
                        const auto oMetadata =
                            OGRParseArrowMetadata(pabyMetadata);
                        auto oIter = oMetadata.find(ARROW_EXTENSION_NAME_KEY);
                        if (oIter != oMetadata.end() &&
                            (oIter->second == EXTENSION_NAME_OGC_WKB ||
                             oIter->second == EXTENSION_NAME_GEOARROW_WKB))
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

            if (!IsBinary(format) && !IsLargeBinary(format))
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
    return true;
}

/************************************************************************/
/*                           GetUInt64Value()                           */
/************************************************************************/

static inline uint64_t GetUInt64Value(const struct ArrowSchema *schema,
                                      const struct ArrowArray *array,
                                      size_t iFeature)
{
    uint64_t nVal = 0;
    CPLAssert(schema->format[1] == 0);
    switch (schema->format[0])
    {
        case ARROW_LETTER_INT8:
            nVal = GetValue<int8_t>(array, iFeature);
            break;
        case ARROW_LETTER_UINT8:
            nVal = GetValue<uint8_t>(array, iFeature);
            break;
        case ARROW_LETTER_INT16:
            nVal = GetValue<int16_t>(array, iFeature);
            break;
        case ARROW_LETTER_UINT16:
            nVal = GetValue<uint16_t>(array, iFeature);
            break;
        case ARROW_LETTER_INT32:
            nVal = GetValue<int32_t>(array, iFeature);
            break;
        case ARROW_LETTER_UINT32:
            nVal = GetValue<uint32_t>(array, iFeature);
            break;
        case ARROW_LETTER_INT64:
            nVal = GetValue<int64_t>(array, iFeature);
            break;
        case ARROW_LETTER_UINT64:
            nVal = GetValue<uint64_t>(array, iFeature);
            break;
        default:
            // Shouldn't happen given checks in BuildOGRFieldInfo()
            CPLAssert(false);
            break;
    }
    return nVal;
}

/************************************************************************/
/*                         GetWorkingBufferSize()                       */
/************************************************************************/

static size_t GetWorkingBufferSize(const struct ArrowSchema *schema,
                                   const struct ArrowArray *array,
                                   size_t iFeature, int &iArrowIdxInOut,
                                   const std::vector<FieldInfo> &asFieldInfo)
{
    const char *fieldName = schema->name;
    const char *format = schema->format;
    if (IsStructure(format))
    {
        size_t nRet = 0;
        for (int64_t i = 0; i < array->n_children; ++i)
        {
            nRet += GetWorkingBufferSize(
                schema->children[i], array->children[i],
                iFeature + static_cast<size_t>(array->offset), iArrowIdxInOut,
                asFieldInfo);
        }
        return nRet;
    }
    const int iArrowIdx = iArrowIdxInOut;
    ++iArrowIdxInOut;

    if (!asFieldInfo[iArrowIdx].bUseStringOptim)
        return 0;

    const uint8_t *pabyValidity =
        static_cast<const uint8_t *>(array->buffers[0]);
    if (array->null_count != 0 && pabyValidity &&
        !TestBit(pabyValidity, static_cast<size_t>(iFeature + array->offset)))
    {
        // empty string
        return 0;
    }

    if (asFieldInfo[iArrowIdx].bUseDictionary)
    {
        const uint64_t nDictIdx = GetUInt64Value(schema, array, iFeature);
        const auto dictArray = array->dictionary;
        if (nDictIdx >= static_cast<uint64_t>(dictArray->length))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Feature %" PRIu64
                     ", field %s: invalid dictionary index: %" PRIu64,
                     static_cast<uint64_t>(iFeature), fieldName, nDictIdx);
            return 0;
        }

        array = dictArray;
        schema = schema->dictionary;
        format = schema->format;
        iFeature = static_cast<size_t>(nDictIdx);
    }

    if (IsString(format))
    {
        const auto *panOffsets =
            static_cast<const uint32_t *>(array->buffers[1]) + array->offset;
        return 1 + (panOffsets[iFeature + 1] - panOffsets[iFeature]);
    }
    else if (IsLargeString(format))
    {
        const auto *panOffsets =
            static_cast<const uint64_t *>(array->buffers[1]) + array->offset;
        return 1 + static_cast<size_t>(panOffsets[iFeature + 1] -
                                       panOffsets[iFeature]);
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
inline static void
FillFieldString(const struct ArrowArray *array, int iOGRFieldIdx,
                size_t iFeature, int iArrowIdx,
                const std::vector<FieldInfo> &asFieldInfo,
                std::string &osWorkingBuffer, OGRFeature &oFeature)
{
    const auto *panOffsets =
        static_cast<const OffsetType *>(array->buffers[1]) + array->offset;
    const char *pszStr = static_cast<const char *>(array->buffers[2]);
    const size_t nLen =
        static_cast<size_t>(panOffsets[iFeature + 1] - panOffsets[iFeature]);
    if (asFieldInfo[iArrowIdx].bUseStringOptim)
    {
        oFeature.SetFieldSameTypeUnsafe(
            iOGRFieldIdx, &osWorkingBuffer[0] + osWorkingBuffer.size());
        osWorkingBuffer.append(pszStr + panOffsets[iFeature], nLen);
        osWorkingBuffer.push_back(0);  // append null character
    }
    else
    {
        const std::string osTmp(pszStr, nLen);
        oFeature.SetField(iOGRFieldIdx, osTmp.c_str());
    }
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

        // Check if we can reuse the existing geometry, to save dynamic memory
        // allocations.
        if (nLen >= 5 && pabyData[0] == wkbNDR && pabyData[1] <= wkbTriangle &&
            pabyData[2] == 0 && pabyData[3] == 0 && pabyData[4] == 0)
        {
            const auto poExistingGeom = oFeature.GetGeomFieldRef(iOGRFieldIdx);
            if (poExistingGeom &&
                poExistingGeom->getGeometryType() == pabyData[1])
            {
                poExistingGeom->importFromWkb(pabyData, nLen, wkbVariantIso,
                                              nBytesConsumedOut);
                return true;
            }
        }

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
    const char *fieldName = schema->name;
    const char *format = schema->format;
    if (IsStructure(format))
    {
        const std::string osNewPrefix(osFieldPrefix + fieldName + ".");
        for (int64_t i = 0; i < array->n_children; ++i)
        {
            if (!FillFeature(
                    poLayer, schema->children[i], array->children[i],
                    osNewPrefix, iFeature + static_cast<size_t>(array->offset),
                    iArrowIdxInOut, asFieldInfo, oFeature, osWorkingBuffer))
                return false;
        }
        return true;
    }
    const int iArrowIdx = iArrowIdxInOut;
    ++iArrowIdxInOut;
    const int iOGRFieldIdx = asFieldInfo[iArrowIdx].iOGRFieldIdx;

    if (asFieldInfo[iArrowIdx].bUseDictionary)
    {
        format = schema->dictionary->format;
    }

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
            else if (asFieldInfo[iArrowIdx].eSetFeatureFieldType == OFTString)
            {
                OGRField *psField = oFeature.GetRawFieldRef(iOGRFieldIdx);
                if (!asFieldInfo[iArrowIdx].bUseStringOptim)
                {
                    if (IsValidField(psField))
                    {
                        CPLFree(psField->String);
                        OGR_RawField_SetNull(psField);
                    }
                }
                else
                {
                    OGR_RawField_SetNull(psField);
                }
            }
            else
            {
                OGRField *psField = oFeature.GetRawFieldRef(iOGRFieldIdx);
                switch (asFieldInfo[iArrowIdx].eSetFeatureFieldType)
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

    if (asFieldInfo[iArrowIdx].bUseDictionary)
    {
        const uint64_t nDictIdx = GetUInt64Value(schema, array, iFeature);
        auto dictArray = array->dictionary;
        if (nDictIdx >= static_cast<uint64_t>(dictArray->length))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Feature %" PRIu64
                     ", field %s: invalid dictionary index: %" PRIu64,
                     static_cast<uint64_t>(iFeature),
                     (osFieldPrefix + fieldName).c_str(), nDictIdx);
            return false;
        }
        array = dictArray;
        schema = schema->dictionary;
        iFeature = static_cast<size_t>(nDictIdx);
    }

    if (IsBoolean(format))
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
    else if (IsInt8(format))
    {
        FillField<int8_t>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (IsUInt8(format))
    {
        FillField<uint8_t>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (IsInt16(format))
    {
        FillField<int16_t>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (IsUInt16(format))
    {
        FillField<uint16_t>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (IsInt32(format))
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
    else if (IsUInt32(format))
    {
        FillField<uint32_t, GIntBig>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (IsInt64(format))
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
    else if (IsUInt64(format))
    {
        FillField<uint64_t, double>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (IsFloat32(format))
    {
        FillField<float>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (IsFloat64(format))
    {
        FillField<double>(array, iOGRFieldIdx, iFeature, oFeature);
        return true;
    }
    else if (IsString(format))
    {
        FillFieldString<uint32_t>(array, iOGRFieldIdx, iFeature, iArrowIdx,
                                  asFieldInfo, osWorkingBuffer, oFeature);
        return true;
    }
    else if (IsLargeString(format))
    {
        FillFieldString<uint64_t>(array, iOGRFieldIdx, iFeature, iArrowIdx,
                                  asFieldInfo, osWorkingBuffer, oFeature);
        return true;
    }
    else if (IsBinary(format))
    {
        return FillFieldBinary<uint32_t>(array, iOGRFieldIdx, iFeature,
                                         iArrowIdx, asFieldInfo, osFieldPrefix,
                                         fieldName, oFeature);
    }
    else if (IsLargeBinary(format))
    {
        return FillFieldBinary<uint64_t>(array, iOGRFieldIdx, iFeature,
                                         iArrowIdx, asFieldInfo, osFieldPrefix,
                                         fieldName, oFeature);
    }
    else if (asFieldInfo[iArrowIdx].nPrecision > 0)
    {
        // fits on a int64
        CPLAssert(asFieldInfo[iArrowIdx].nPrecision <= 19);
        // either 128 or 256 bits
        CPLAssert((asFieldInfo[iArrowIdx].nWidthInBytes % 8) == 0);
        const int nWidthIn64BitWord = asFieldInfo[iArrowIdx].nWidthInBytes / 8;

        if (IsList(format))
        {
            const auto panOffsets =
                static_cast<const uint32_t *>(array->buffers[1]) +
                array->offset;
            const auto childArray = array->children[0];
            std::vector<double> aValues;
            for (auto i = panOffsets[iFeature]; i < panOffsets[iFeature + 1];
                 ++i)
            {
                aValues.push_back(GetValueDecimal(childArray, nWidthIn64BitWord,
                                                  asFieldInfo[iArrowIdx].nScale,
                                                  i));
            }
            oFeature.SetField(iOGRFieldIdx, static_cast<int>(aValues.size()),
                              aValues.data());
            return true;
        }
        else if (IsLargeList(format))
        {
            const auto panOffsets =
                static_cast<const uint64_t *>(array->buffers[1]) +
                array->offset;
            const auto childArray = array->children[0];
            std::vector<double> aValues;
            for (auto i = static_cast<size_t>(panOffsets[iFeature]);
                 i < static_cast<size_t>(panOffsets[iFeature + 1]); ++i)
            {
                aValues.push_back(GetValueDecimal(childArray, nWidthIn64BitWord,
                                                  asFieldInfo[iArrowIdx].nScale,
                                                  i));
            }
            oFeature.SetField(iOGRFieldIdx, static_cast<int>(aValues.size()),
                              aValues.data());
            return true;
        }
        else if (IsFixedSizeList(format))
        {
            const int nVals = GetFixedSizeList(format);
            const auto childArray = array->children[0];
            std::vector<double> aValues;
            for (int i = 0; i < nVals; ++i)
            {
                aValues.push_back(GetValueDecimal(childArray, nWidthIn64BitWord,
                                                  asFieldInfo[iArrowIdx].nScale,
                                                  iFeature * nVals + i));
            }
            oFeature.SetField(iOGRFieldIdx, nVals, aValues.data());
            return true;
        }

        CPLAssert(format[0] == ARROW_LETTER_DECIMAL);

        oFeature.SetFieldSameTypeUnsafe(
            iOGRFieldIdx,
            GetValueDecimal(array, nWidthIn64BitWord,
                            asFieldInfo[iArrowIdx].nScale, iFeature));
        return true;
    }
    else if (SetFieldForOtherFormats(
                 oFeature, iOGRFieldIdx,
                 static_cast<size_t>(iFeature + array->offset), schema, array))
    {
        return true;
    }

    CPLError(CE_Failure, CPLE_NotSupported, "%s",
             ("Type '" + std::string(format) + "' for field " + osFieldPrefix +
              fieldName + " is not supported.")
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
 * compatible type. For attribute fields, they should generally be created with
 * CreateFieldFromArrowSchema(). This is strictly required for output drivers
 * Arrow or Parquet, and strongly recommended otherwise. For geometry fields,
 * they should be created either implicitly at CreateLayer() type
 * (if geom_type != wkbNone), or explicitly with CreateGeomField().
 *
 * Starting with GDAL 3.9, some tolerance has been introduced in the base
 * implementation of WriteArrowBatch() for scenarios that involve appending to
 * an already existing output layer when the input Arrow field type and the
 * OGR layer field type are 32/64-bi integers or real number, but do not match
 * exactly, which may cause lossy conversions. The IF_FIELD_NOT_PRESERVED option
 * can be used to control the behavior in case of lossy conversion.
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
 * <li>IF_FID_NOT_PRESERVED=NOTHING/ERROR/WARNING. Action to perform when the
 *     input FID is not preserved in the output layer. The default is NOTHING.
 *     Setting it to ERROR will cause the function to error out. Setting it
 *     to WARNING will cause the function to emit a warning but continue its
 *     processing.
 * </li>
 * <li>IF_FIELD_NOT_PRESERVED=ERROR/WARNING. (since GDAL 3.9)
 *     Action to perform when the input field value is not preserved in the
 *     output layer.
 *     The default is WARNING, which will cause the function to emit a warning
 *     but continue its processing.
 *     Setting it to ERROR will cause the function to error out if a lossy
 *     conversion is detected.
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
    if (!IsStructure(format))
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

    CPLStringList aosNativeTypes;
    auto poDS = const_cast<OGRLayer *>(this)->GetDataset();
    if (poDS)
    {
        auto poDriver = poDS->GetDriver();
        if (poDriver)
        {
            const char *pszMetadataItem =
                poDriver->GetMetadataItem(GDAL_DMD_CREATIONFIELDDATATYPES);
            if (pszMetadataItem)
                aosNativeTypes = CSLTokenizeString2(pszMetadataItem, " ", 0);
        }
    }

    std::vector<FieldInfo> asFieldInfo;
    auto poLayerDefn = GetLayerDefn();
    const char *pszFIDName =
        CSLFetchNameValueDef(papszOptions, "FID", GetFIDColumn());
    if (!pszFIDName || pszFIDName[0] == 0)
        pszFIDName = DEFAULT_ARROW_FID_NAME;
    const bool bErrorIfFIDNotPreserved =
        EQUAL(CSLFetchNameValueDef(papszOptions, "IF_FID_NOT_PRESERVED", ""),
              "ERROR");
    const bool bWarningIfFIDNotPreserved =
        EQUAL(CSLFetchNameValueDef(papszOptions, "IF_FID_NOT_PRESERVED", ""),
              "WARNING");
    const bool bErrorIfFieldNotPreserved =
        EQUAL(CSLFetchNameValueDef(papszOptions, "IF_FIELD_NOT_PRESERVED", ""),
              "ERROR");
    const char *pszGeomFieldName = CSLFetchNameValueDef(
        papszOptions, "GEOMETRY_NAME", GetGeometryColumn());
    if (!pszGeomFieldName || pszGeomFieldName[0] == 0)
        pszGeomFieldName = DEFAULT_ARROW_GEOMETRY_NAME;
    const struct ArrowSchema *schemaFIDColumn = nullptr;
    struct ArrowArray *arrayFIDColumn = nullptr;
    bool bFallbackTypesUsed = false;
    for (int64_t i = 0; i < schema->n_children; ++i)
    {
        if (!BuildOGRFieldInfo(schema->children[i], array->children[i],
                               poLayerDefn, std::string(), aosNativeTypes,
                               bFallbackTypesUsed, asFieldInfo, pszFIDName,
                               pszGeomFieldName, this,
                               m_poPrivate->m_oMapArrowFieldNameToOGRFieldName,
                               schemaFIDColumn, arrayFIDColumn))
        {
            return false;
        }
    }

    std::map<int, int> oMapOGRFieldIndexToFieldInfoIndex;
    std::vector<bool> abUseStringOptim(poLayerDefn->GetFieldCount(), false);
    for (int i = 0; i < static_cast<int>(asFieldInfo.size()); ++i)
    {
        if (asFieldInfo[i].iOGRFieldIdx >= 0 && !asFieldInfo[i].bIsGeomCol)
        {
            CPLAssert(oMapOGRFieldIndexToFieldInfoIndex.find(
                          asFieldInfo[i].iOGRFieldIdx) ==
                      oMapOGRFieldIndexToFieldInfoIndex.end());
            oMapOGRFieldIndexToFieldInfoIndex[asFieldInfo[i].iOGRFieldIdx] = i;
            abUseStringOptim[asFieldInfo[i].iOGRFieldIdx] =
                asFieldInfo[i].bUseStringOptim;
        }
    }

    OGRFeatureDefn oLayerDefnTmp(poLayerDefn->GetName());

    struct LayerDefnTmpRefReleaser
    {
        OGRFeatureDefn &m_oDefn;

        explicit LayerDefnTmpRefReleaser(OGRFeatureDefn &oDefn) : m_oDefn(oDefn)
        {
            m_oDefn.Reference();
        }

        ~LayerDefnTmpRefReleaser()
        {
            m_oDefn.Dereference();
        }
    };

    LayerDefnTmpRefReleaser oLayerDefnTmpRefReleaser(oLayerDefnTmp);

    std::vector<int> anIdentityFieldMap;
    if (bFallbackTypesUsed)
    {
        oLayerDefnTmp.SetGeomType(wkbNone);
        for (int i = 0; i < poLayerDefn->GetFieldCount(); ++i)
        {
            anIdentityFieldMap.push_back(i);
            const auto poSrcFieldDefn = poLayerDefn->GetFieldDefn(i);
            const auto oIter = oMapOGRFieldIndexToFieldInfoIndex.find(i);
            OGRFieldDefn oFieldDefn(
                poSrcFieldDefn->GetNameRef(),
                oIter == oMapOGRFieldIndexToFieldInfoIndex.end()
                    ? poSrcFieldDefn->GetType()
                    : asFieldInfo[oIter->second].eNominalFieldType);
            if (oIter != oMapOGRFieldIndexToFieldInfoIndex.end())
                asFieldInfo[oIter->second].eSetFeatureFieldType =
                    asFieldInfo[oIter->second].eNominalFieldType;
            oLayerDefnTmp.AddFieldDefn(&oFieldDefn);
        }
        for (int i = 0; i < poLayerDefn->GetGeomFieldCount(); ++i)
        {
            oLayerDefnTmp.AddGeomFieldDefn(poLayerDefn->GetGeomFieldDefn(i));
        }
    }
    else
    {
        for (auto &sFieldInfo : asFieldInfo)
            sFieldInfo.eSetFeatureFieldType = sFieldInfo.eTargetFieldType;
    }

    struct FeatureCleaner
    {
        OGRFeature &m_oFeature;
        const std::vector<bool> &m_abUseStringOptim;

        explicit FeatureCleaner(OGRFeature &oFeature,
                                const std::vector<bool> &abUseStringOptim)
            : m_oFeature(oFeature), m_abUseStringOptim(abUseStringOptim)
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
                if (m_abUseStringOptim[i])
                {
                    if (m_oFeature.IsFieldSetAndNotNullUnsafe(i))
                        m_oFeature.SetFieldSameTypeUnsafe(
                            i, static_cast<char *>(nullptr));
                }
            }
        }
    };

    OGRFeature oFeature(bFallbackTypesUsed ? &oLayerDefnTmp : poLayerDefn);
    FeatureCleaner oCleaner(oFeature, abUseStringOptim);
    OGRFeature oFeatureTarget(poLayerDefn);
    OGRFeature *const poFeatureTarget =
        bFallbackTypesUsed ? &oFeatureTarget : &oFeature;

    // We accumulate the content of all strings in osWorkingBuffer to avoid
    // a few dynamic memory allocations
    std::string osWorkingBuffer;

    bool bTransactionOK;
    {
        CPLErrorStateBackuper oBackuper(CPLQuietErrorHandler);
        bTransactionOK = StartTransaction() == OGRERR_NONE;
    }

    const std::string emptyString;
    int64_t fidNullCount = 0;
    for (size_t iFeature = 0; iFeature < static_cast<size_t>(array->length);
         ++iFeature)
    {
        oFeature.SetFID(OGRNullFID);

        int iArrowIdx = 0;
        const size_t nWorkingBufferSize = GetWorkingBufferSize(
            schema, array, iFeature, iArrowIdx, asFieldInfo);
        osWorkingBuffer.clear();
        osWorkingBuffer.reserve(nWorkingBufferSize);
#ifdef DEBUG
        const char *pszWorkingBuffer = osWorkingBuffer.c_str();
        CPL_IGNORE_RET_VAL(pszWorkingBuffer);
#endif
        iArrowIdx = 0;
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

        if (bFallbackTypesUsed)
        {
            oFeatureTarget.SetFrom(&oFeature, anIdentityFieldMap.data(),
                                   /*bForgiving=*/true,
                                   /*bUseISO8601ForDateTimeAsString=*/true);
            oFeatureTarget.SetFID(oFeature.GetFID());

            if (bErrorIfFieldNotPreserved)
            {
                for (int i = 0; i < poLayerDefn->GetFieldCount(); ++i)
                {
                    if (!oFeature.IsFieldSetAndNotNullUnsafe(i))
                    {
                        continue;
                    }
                    bool bLossyConversion = false;
                    const auto eSrcType =
                        oLayerDefnTmp.GetFieldDefnUnsafe(i)->GetType();
                    const auto eDstType =
                        poLayerDefn->GetFieldDefnUnsafe(i)->GetType();

                    const auto IsDoubleCastToInt64EqualTInt64 =
                        [](double dfVal, int64_t nOtherVal)
                    {
                        // Values in the range [INT64_MAX - 1023, INT64_MAX - 1]
                        // get converted to a double that once cast to int64_t
                        // is INT64_MAX + 1, hence the strict < comparison
                        return dfVal >=
                                   static_cast<double>(
                                       std::numeric_limits<int64_t>::min()) &&
                               dfVal <
                                   static_cast<double>(
                                       std::numeric_limits<int64_t>::max()) &&
                               static_cast<int64_t>(dfVal) == nOtherVal;
                    };

                    if (eSrcType == OFTInteger64 && eDstType == OFTInteger &&
                        oFeatureTarget.GetFieldAsIntegerUnsafe(i) !=
                            oFeature.GetFieldAsInteger64Unsafe(i))
                    {
                        bLossyConversion = true;
                    }
                    else if (eSrcType == OFTReal && eDstType == OFTInteger &&
                             oFeatureTarget.GetFieldAsIntegerUnsafe(i) !=
                                 oFeature.GetFieldAsDoubleUnsafe(i))
                    {
                        bLossyConversion = true;
                    }
                    else if (eSrcType == OFTReal && eDstType == OFTInteger64 &&
                             static_cast<double>(
                                 oFeatureTarget.GetFieldAsInteger64Unsafe(i)) !=
                                 oFeature.GetFieldAsDoubleUnsafe(i))
                    {
                        bLossyConversion = true;
                    }
                    else if (eSrcType == OFTInteger64 && eDstType == OFTReal &&
                             !IsDoubleCastToInt64EqualTInt64(
                                 oFeatureTarget.GetFieldAsDoubleUnsafe(i),
                                 oFeature.GetFieldAsInteger64Unsafe(i)))
                    {
                        bLossyConversion = true;
                    }
                    if (bLossyConversion)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "For feature " CPL_FRMT_GIB
                                 ", value of field %s cannot not preserved",
                                 oFeatureTarget.GetFID(),
                                 oLayerDefnTmp.GetFieldDefn(i)->GetNameRef());
                        if (bTransactionOK)
                            RollbackTransaction();
                        return false;
                    }
                }
            }
        }

        const auto nInputFID = poFeatureTarget->GetFID();
        if (CreateFeature(poFeatureTarget) != OGRERR_NONE)
        {
            if (bTransactionOK)
                RollbackTransaction();
            return false;
        }
        if (nInputFID != OGRNullFID)
        {
            if (bWarningIfFIDNotPreserved &&
                poFeatureTarget->GetFID() != nInputFID)
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Feature id " CPL_FRMT_GIB " not preserved",
                         nInputFID);
            }
            else if (bErrorIfFIDNotPreserved &&
                     poFeatureTarget->GetFID() != nInputFID)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Feature id " CPL_FRMT_GIB " not preserved",
                         nInputFID);
                if (bTransactionOK)
                    RollbackTransaction();
                return false;
            }
        }

        if (arrayFIDColumn)
        {
            uint8_t *pabyValidity = static_cast<uint8_t *>(
                const_cast<void *>(arrayFIDColumn->buffers[0]));
            if (IsInt32(schemaFIDColumn->format))
            {
                auto *panValues = static_cast<int32_t *>(
                    const_cast<void *>(arrayFIDColumn->buffers[1]));
                if (poFeatureTarget->GetFID() >
                    std::numeric_limits<int32_t>::max())
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
                             poFeatureTarget->GetFID());
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
                        static_cast<int32_t>(poFeatureTarget->GetFID());
                }
            }
            else if (IsInt64(schemaFIDColumn->format))
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
                    poFeatureTarget->GetFID();
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
 * compatible type. For attribute fields, they should generally be created with
 * CreateFieldFromArrowSchema(). This is strictly required for output drivers
 * Arrow or Parquet, and strongly recommended otherwise. For geometry fields,
 * they should be created either implicitly at CreateLayer() type
 * (if geom_type != wkbNone), or explicitly with CreateGeomField().
 *
 * Starting with GDAL 3.9, some tolerance has been introduced in the base
 * implementation of WriteArrowBatch() for scenarios that involve appending to
 * an already existing output layer when the input Arrow field type and the
 * OGR layer field type are 32/64-bi integers or real number, but do not match
 * exactly, which may cause lossy conversions. The IF_FIELD_NOT_PRESERVED option
 * can be used to control the behavior in case of lossy conversion.
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
 * <li>IF_FID_NOT_PRESERVED=NOTHING/ERROR/WARNING. Action to perform when the
 *     input FID is not preserved in the output layer. The default is NOTHING.
 *     Setting it to ERROR will cause the function to error out. Setting it
 *     to WARNING will cause the function to emit a warning but continue its
 *     processing.
 * </li>
 * <li>IF_FIELD_NOT_PRESERVED=ERROR/WARNING. (since GDAL 3.9)
 *     Action to perform when the input field value is not preserved in the
 *     output layer.
 *     The default is WARNING, which will cause the function to emit a warning
 *     but continue its processing.
 *     Setting it to ERROR will cause the function to error out if a lossy
 *     conversion is detected.
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
