// SPDX-License-Identifier: MIT
// Copyright 2007, Mateusz Loskot
// Copyright 2008-2024, Even Rouault <even.rouault at spatialys.com>

/*! @cond Doxygen_Suppress */

#include "ogrlibjsonutils.h"

#include "cpl_string.h"
#include "ogr_geometry.h"
#include "ogr_p.h"

#include <cmath>

/************************************************************************/
/*                             OGRJSonParse()                           */
/************************************************************************/

bool OGRJSonParse(const char *pszText, json_object **ppoObj, bool bVerboseError)
{
    if (ppoObj == nullptr)
        return false;
    json_tokener *jstok = json_tokener_new();
    const int nLen = pszText == nullptr ? 0 : static_cast<int>(strlen(pszText));
    *ppoObj = json_tokener_parse_ex(jstok, pszText, nLen);
    if (jstok->err != json_tokener_success)
    {
        if (bVerboseError)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JSON parsing error: %s (at offset %d)",
                     json_tokener_error_desc(jstok->err), jstok->char_offset);
        }

        json_tokener_free(jstok);
        *ppoObj = nullptr;
        return false;
    }
    json_tokener_free(jstok);
    return true;
}

/************************************************************************/
/*                    CPL_json_object_object_get()                      */
/************************************************************************/

// This is the same as json_object_object_get() except it will not raise
// deprecation warning.

json_object *CPL_json_object_object_get(struct json_object *obj,
                                        const char *key)
{
    json_object *poRet = nullptr;
    json_object_object_get_ex(obj, key, &poRet);
    return poRet;
}

/************************************************************************/
/*                       json_ex_get_object_by_path()                   */
/************************************************************************/

json_object *json_ex_get_object_by_path(json_object *poObj, const char *pszPath)
{
    if (poObj == nullptr || json_object_get_type(poObj) != json_type_object ||
        pszPath == nullptr || *pszPath == '\0')
    {
        return nullptr;
    }
    char **papszTokens = CSLTokenizeString2(pszPath, ".", 0);
    for (int i = 0; papszTokens[i] != nullptr; i++)
    {
        poObj = CPL_json_object_object_get(poObj, papszTokens[i]);
        if (poObj == nullptr)
            break;
        if (papszTokens[i + 1] != nullptr)
        {
            if (json_object_get_type(poObj) != json_type_object)
            {
                poObj = nullptr;
                break;
            }
        }
    }
    CSLDestroy(papszTokens);
    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONFindMemberByName                 */
/************************************************************************/

lh_entry *OGRGeoJSONFindMemberEntryByName(json_object *poObj,
                                          const char *pszName)
{
    if (nullptr == pszName || nullptr == poObj)
        return nullptr;

    if (nullptr != json_object_get_object(poObj))
    {
        lh_entry *entry = json_object_get_object(poObj)->head;
        while (entry != nullptr)
        {
            if (EQUAL(static_cast<const char *>(entry->k), pszName))
                return entry;
            entry = entry->next;
        }
    }

    return nullptr;
}

json_object *OGRGeoJSONFindMemberByName(json_object *poObj, const char *pszName)
{
    lh_entry *entry = OGRGeoJSONFindMemberEntryByName(poObj, pszName);
    if (nullptr == entry)
        return nullptr;
    return static_cast<json_object *>(const_cast<void *>(entry->v));
}

/************************************************************************/
/*               OGR_json_double_with_precision_to_string()             */
/************************************************************************/

static int OGR_json_double_with_precision_to_string(struct json_object *jso,
                                                    struct printbuf *pb,
                                                    int /* level */,
                                                    int /* flags */)
{
    const void *userData =
#if (!defined(JSON_C_VERSION_NUM)) || (JSON_C_VERSION_NUM < JSON_C_VER_013)
        jso->_userdata;
#else
        json_object_get_userdata(jso);
#endif
    // Precision is stored as a uintptr_t content casted to void*
    const uintptr_t nPrecisionIn = reinterpret_cast<uintptr_t>(userData);
    const double dfVal = json_object_get_double(jso);
    if (fabs(dfVal) > 1e50 && !std::isinf(dfVal))
    {
        char szBuffer[75] = {};
        const size_t nLen =
            CPLsnprintf(szBuffer, sizeof(szBuffer), "%.17g", dfVal);
        return printbuf_memappend(pb, szBuffer, static_cast<int>(nLen));
    }
    else
    {
        const bool bPrecisionIsNegative =
            (nPrecisionIn >> (8 * sizeof(nPrecisionIn) - 1)) != 0;
        const int nPrecision =
            bPrecisionIsNegative ? 15 : static_cast<int>(nPrecisionIn);
        OGRWktOptions opts(nPrecision, /* round = */ true);
        opts.format = OGRWktFormat::F;

        const std::string s = OGRFormatDouble(dfVal, opts, 1);

        return printbuf_memappend(pb, s.data(), static_cast<int>(s.size()));
    }
}

/************************************************************************/
/*                   json_object_new_double_with_precision()            */
/************************************************************************/

json_object *json_object_new_double_with_precision(double dfVal,
                                                   int nCoordPrecision)
{
    json_object *jso = json_object_new_double(dfVal);
    json_object_set_serializer(
        jso, OGR_json_double_with_precision_to_string,
        reinterpret_cast<void *>(static_cast<uintptr_t>(nCoordPrecision)),
        nullptr);
    return jso;
}

/************************************************************************/
/*             OGR_json_double_with_significant_figures_to_string()     */
/************************************************************************/

static int OGR_json_double_with_significant_figures_to_string(
    struct json_object *jso, struct printbuf *pb, int /* level */,
    int /* flags */)
{
    char szBuffer[75] = {};
    int nSize = 0;
    const double dfVal = json_object_get_double(jso);
    if (std::isnan(dfVal))
        nSize = CPLsnprintf(szBuffer, sizeof(szBuffer), "NaN");
    else if (std::isinf(dfVal))
    {
        if (dfVal > 0)
            nSize = CPLsnprintf(szBuffer, sizeof(szBuffer), "Infinity");
        else
            nSize = CPLsnprintf(szBuffer, sizeof(szBuffer), "-Infinity");
    }
    else
    {
        char szFormatting[32] = {};
        const void *userData =
#if (!defined(JSON_C_VERSION_NUM)) || (JSON_C_VERSION_NUM < JSON_C_VER_013)
            jso->_userdata;
#else
            json_object_get_userdata(jso);
#endif
        const uintptr_t nSignificantFigures =
            reinterpret_cast<uintptr_t>(userData);
        const bool bSignificantFiguresIsNegative =
            (nSignificantFigures >> (8 * sizeof(nSignificantFigures) - 1)) != 0;
        const int nInitialSignificantFigures =
            bSignificantFiguresIsNegative
                ? 17
                : static_cast<int>(nSignificantFigures);
        CPLsnprintf(szFormatting, sizeof(szFormatting), "%%.%dg",
                    nInitialSignificantFigures);
        nSize = CPLsnprintf(szBuffer, sizeof(szBuffer), szFormatting, dfVal);
        const char *pszDot = strchr(szBuffer, '.');

        // Try to avoid .xxxx999999y or .xxxx000000y rounding issues by
        // decreasing a bit precision.
        if (nInitialSignificantFigures > 10 && pszDot != nullptr &&
            (strstr(pszDot, "999999") != nullptr ||
             strstr(pszDot, "000000") != nullptr))
        {
            bool bOK = false;
            for (int i = 1; i <= 3; i++)
            {
                CPLsnprintf(szFormatting, sizeof(szFormatting), "%%.%dg",
                            nInitialSignificantFigures - i);
                nSize = CPLsnprintf(szBuffer, sizeof(szBuffer), szFormatting,
                                    dfVal);
                pszDot = strchr(szBuffer, '.');
                if (pszDot != nullptr && strstr(pszDot, "999999") == nullptr &&
                    strstr(pszDot, "000000") == nullptr)
                {
                    bOK = true;
                    break;
                }
            }
            if (!bOK)
            {
                CPLsnprintf(szFormatting, sizeof(szFormatting), "%%.%dg",
                            nInitialSignificantFigures);
                nSize = CPLsnprintf(szBuffer, sizeof(szBuffer), szFormatting,
                                    dfVal);
            }
        }

        if (nSize + 2 < static_cast<int>(sizeof(szBuffer)) &&
            strchr(szBuffer, '.') == nullptr &&
            strchr(szBuffer, 'e') == nullptr)
        {
            nSize +=
                CPLsnprintf(szBuffer + nSize, sizeof(szBuffer) - nSize, ".0");
        }
    }

    return printbuf_memappend(pb, szBuffer, nSize);
}

/************************************************************************/
/*              json_object_new_double_with_significant_figures()       */
/************************************************************************/

json_object *
json_object_new_double_with_significant_figures(double dfVal,
                                                int nSignificantFigures)
{
    json_object *jso = json_object_new_double(dfVal);
    json_object_set_serializer(
        jso, OGR_json_double_with_significant_figures_to_string,
        reinterpret_cast<void *>(static_cast<uintptr_t>(nSignificantFigures)),
        nullptr);
    return jso;
}

/************************************************************************/
/*                           GeoJSONPropertyToFieldType()               */
/************************************************************************/

constexpr GIntBig MY_INT64_MAX =
    (static_cast<GIntBig>(0x7FFFFFFF) << 32) | 0xFFFFFFFF;
constexpr GIntBig MY_INT64_MIN = static_cast<GIntBig>(0x80000000) << 32;

OGRFieldType GeoJSONPropertyToFieldType(json_object *poObject,
                                        OGRFieldSubType &eSubType,
                                        bool bArrayAsString)
{
    eSubType = OFSTNone;

    if (poObject == nullptr)
    {
        return OFTString;
    }

    json_type type = json_object_get_type(poObject);

    if (json_type_boolean == type)
    {
        eSubType = OFSTBoolean;
        return OFTInteger;
    }
    else if (json_type_double == type)
        return OFTReal;
    else if (json_type_int == type)
    {
        GIntBig nVal = json_object_get_int64(poObject);
        if (!CPL_INT64_FITS_ON_INT32(nVal))
        {
            if (nVal == MY_INT64_MIN || nVal == MY_INT64_MAX)
            {
                static bool bWarned = false;
                if (!bWarned)
                {
                    bWarned = true;
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Integer values probably ranging out of 64bit integer "
                        "range have been found. Will be clamped to "
                        "INT64_MIN/INT64_MAX");
                }
            }
            return OFTInteger64;
        }
        else
        {
            return OFTInteger;
        }
    }
    else if (json_type_string == type)
        return OFTString;
    else if (json_type_array == type)
    {
        if (bArrayAsString)
        {
            eSubType = OFSTJSON;
            return OFTString;
        }
        const auto nSize = json_object_array_length(poObject);
        if (nSize == 0)
        {
            eSubType = OFSTJSON;
            return OFTString;
        }
        OGRFieldType eType = OFTIntegerList;
        for (auto i = decltype(nSize){0}; i < nSize; i++)
        {
            json_object *poRow = json_object_array_get_idx(poObject, i);
            if (poRow != nullptr)
            {
                type = json_object_get_type(poRow);
                if (type == json_type_string)
                {
                    if (i == 0 || eType == OFTStringList)
                    {
                        eType = OFTStringList;
                    }
                    else
                    {
                        eSubType = OFSTJSON;
                        return OFTString;
                    }
                }
                else if (type == json_type_double)
                {
                    if (eSubType == OFSTNone &&
                        (i == 0 || eType == OFTRealList ||
                         eType == OFTIntegerList || eType == OFTInteger64List))
                    {
                        eType = OFTRealList;
                    }
                    else
                    {
                        eSubType = OFSTJSON;
                        return OFTString;
                    }
                }
                else if (type == json_type_int)
                {
                    if (eSubType == OFSTNone && eType == OFTIntegerList)
                    {
                        GIntBig nVal = json_object_get_int64(poRow);
                        if (!CPL_INT64_FITS_ON_INT32(nVal))
                            eType = OFTInteger64List;
                    }
                    else if (eSubType == OFSTNone &&
                             (eType == OFTInteger64List ||
                              eType == OFTRealList))
                    {
                        // ok
                    }
                    else
                    {
                        eSubType = OFSTJSON;
                        return OFTString;
                    }
                }
                else if (type == json_type_boolean)
                {
                    if (i == 0 ||
                        (eType == OFTIntegerList && eSubType == OFSTBoolean))
                    {
                        eSubType = OFSTBoolean;
                    }
                    else
                    {
                        eSubType = OFSTJSON;
                        return OFTString;
                    }
                }
                else
                {
                    eSubType = OFSTJSON;
                    return OFTString;
                }
            }
            else
            {
                eSubType = OFSTJSON;
                return OFTString;
            }
        }

        return eType;
    }
    else if (json_type_object == type)
    {
        eSubType = OFSTJSON;
        return OFTString;
    }

    return OFTString;  // null
}

/*! @endcond */
