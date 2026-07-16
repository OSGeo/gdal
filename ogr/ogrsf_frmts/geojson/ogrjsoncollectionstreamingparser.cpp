/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Streaming parser for GeoJSON-like FeatureCollection
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2017, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrjsoncollectionstreamingparser.h"

#include "cpl_string.h"
#include "ogrlibjsonutils.h"  // CPL_json_object_object_get

#include "ogr_feature.h"

#define JSON_C_VER_013 (13 << 8)

#include <json.h>  // JSON-C

#if (!defined(JSON_C_VERSION_NUM)) || (JSON_C_VERSION_NUM < JSON_C_VER_013)
#include <json_object_private.h>  // just for sizeof(struct json_object)
#endif

#include <charconv>
#include <limits>

#include "include_fast_float.h"

#if (!defined(JSON_C_VERSION_NUM)) || (JSON_C_VERSION_NUM < JSON_C_VER_013)
const size_t ESTIMATE_BASE_OBJECT_SIZE = sizeof(struct json_object);
#elif JSON_C_VERSION_NUM == JSON_C_VER_013  // no way to get the size
#if SIZEOF_VOIDP == 8
const size_t ESTIMATE_BASE_OBJECT_SIZE = 72;
#else
const size_t ESTIMATE_BASE_OBJECT_SIZE = 36;
#endif
#elif JSON_C_VERSION_NUM > JSON_C_VER_013  // we have json_c_object_sizeof()
const size_t ESTIMATE_BASE_OBJECT_SIZE = json_c_object_sizeof();
#endif

const size_t ESTIMATE_ARRAY_SIZE =
    ESTIMATE_BASE_OBJECT_SIZE + sizeof(struct array_list);
const size_t ESTIMATE_ARRAY_ELT_SIZE = sizeof(void *);
const size_t ESTIMATE_OBJECT_ELT_SIZE = sizeof(struct lh_entry);
const size_t ESTIMATE_OBJECT_SIZE =
    ESTIMATE_BASE_OBJECT_SIZE + sizeof(struct lh_table) +
    JSON_OBJECT_DEF_HASH_ENTRIES * ESTIMATE_OBJECT_ELT_SIZE;

/************************************************************************/
/*                  OGRJSONCollectionStreamingParser()                  */
/************************************************************************/

OGRJSONCollectionStreamingParser::OGRJSONCollectionStreamingParser(
    bool bFirstPass, bool bStoreNativeData, size_t nMaxObjectSize)
    : m_bFirstPass(bFirstPass), m_bStoreNativeData(bStoreNativeData),
      m_nMaxObjectSize(nMaxObjectSize)
{
}

/************************************************************************/
/*                 ~OGRJSONCollectionStreamingParser()                  */
/************************************************************************/

OGRJSONCollectionStreamingParser::~OGRJSONCollectionStreamingParser()
{
    if (m_poRootObj)
        json_object_put(m_poRootObj);
    if (m_poCurObj && m_poCurObj != m_poRootObj)
        json_object_put(m_poCurObj);
}

/************************************************************************/
/*                          StealRootObject()                           */
/************************************************************************/

json_object *OGRJSONCollectionStreamingParser::StealRootObject()
{
    json_object *poRet = m_poRootObj;
    if (m_poCurObj == m_poRootObj)
        m_poCurObj = nullptr;
    m_poRootObj = nullptr;
    return poRet;
}

/************************************************************************/
/*                            AppendObject()                            */
/************************************************************************/

void OGRJSONCollectionStreamingParser::AppendObject(json_object *poNewObj)
{
    if (m_bKeySet)
    {
        CPLAssert(json_object_get_type(m_apoCurObj.back()) == json_type_object);
        json_object_object_add(m_apoCurObj.back(), m_osCurKey.c_str(),
                               poNewObj);
        m_osCurKey.clear();
        m_bKeySet = false;
    }
    else
    {
        CPLAssert(json_object_get_type(m_apoCurObj.back()) == json_type_array);
        json_object_array_add(m_apoCurObj.back(), poNewObj);
    }
}

/************************************************************************/
/*                            StartObject()                             */
/************************************************************************/

void OGRJSONCollectionStreamingParser::StartObject()
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    if (m_bInFeaturesArray && m_nDepth == 2)
    {
        m_poCurObj = json_object_new_object();
        m_apoCurObj.push_back(m_poCurObj);
        if (m_bStoreNativeData)
        {
            m_osJson = "{";
            m_abFirstMember.push_back(true);
        }
        m_bStartFeature = true;
    }
    else if (m_poCurObj)
    {
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            m_osJson += "{";
            m_abFirstMember.push_back(true);
        }

        m_nCurObjMemEstimate += ESTIMATE_OBJECT_SIZE;

        json_object *poNewObj = json_object_new_object();
        AppendObject(poNewObj);
        m_apoCurObj.push_back(poNewObj);
    }
    else if (m_bFirstPass && m_nDepth == 0)
    {
        m_poRootObj = json_object_new_object();
        m_apoCurObj.push_back(m_poRootObj);
        m_poCurObj = m_poRootObj;
    }

    m_nDepth++;
}

/************************************************************************/
/*                             EndObject()                              */
/************************************************************************/

void OGRJSONCollectionStreamingParser::EndObject()
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    m_nDepth--;

    if (m_bInFeaturesArray && m_nDepth == 2 && m_poCurObj)
    {
        if (m_bStoreNativeData)
        {
            m_abFirstMember.pop_back();
            m_osJson += "}";
            m_nTotalOGRFeatureMemEstimate +=
                m_osJson.size() + strlen("application/vnd.geo+json");
        }

        json_object *poObjTypeObj =
            CPL_json_object_object_get(m_poCurObj, "type");
        if (poObjTypeObj &&
            json_object_get_type(poObjTypeObj) == json_type_string)
        {
            const char *pszObjType = json_object_get_string(poObjTypeObj);
            if (strcmp(pszObjType, "Feature") == 0)
            {
                GotFeature(m_poCurObj, m_bFirstPass, m_osJson);
            }
        }

        json_object_put(m_poCurObj);
        m_poCurObj = nullptr;
        m_apoCurObj.clear();
        m_nCurObjMemEstimate = 0;
        m_bInCoordinates = false;
        m_nTotalOGRFeatureMemEstimate += sizeof(OGRFeature);
        m_osJson.clear();
        m_abFirstMember.clear();
        m_bEndFeature = true;
    }
    else if (m_poCurObj)
    {
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            m_abFirstMember.pop_back();
            m_osJson += "}";
        }

        m_apoCurObj.pop_back();
    }
    else if (m_nDepth == 1)
    {
        m_bInFeatures = false;
        m_bInMeasures = false;
        m_bInMeasuresEnabled = false;
    }
}

/************************************************************************/
/*                         StartObjectMember()                          */
/************************************************************************/

void OGRJSONCollectionStreamingParser::StartObjectMember(std::string_view sKey)
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    if (m_nDepth == 1)
    {
        m_bInFeatures = sKey == "features";
        m_bInMeasures = sKey == "measures";
        m_bCanEasilyAppend = m_bInFeatures;
        m_bInType = sKey == "type";
        if (m_bInType || m_bInFeatures)
        {
            m_poCurObj = nullptr;
            m_apoCurObj.clear();
            m_nRootObjMemEstimate = m_nCurObjMemEstimate;
        }
        else if (m_poRootObj)
        {
            m_poCurObj = m_poRootObj;
            m_apoCurObj.clear();
            m_apoCurObj.push_back(m_poCurObj);
            m_nCurObjMemEstimate = m_nRootObjMemEstimate;
        }
    }
    else if (m_nDepth == 2 && m_bInMeasures)
    {
        m_bInMeasuresEnabled = sKey == "enabled";
    }
    else if (m_nDepth == 3 && m_bInFeaturesArray)
    {
        m_bInCoordinates = sKey == "coordinates" || sKey == "geometries";
    }

    if (m_poCurObj)
    {
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            if (!m_abFirstMember.back())
                m_osJson += ",";
            m_abFirstMember.back() = false;
            m_osJson += CPLJSonStreamingParser::GetSerializedString(sKey) + ":";
        }

        m_nCurObjMemEstimate += ESTIMATE_OBJECT_ELT_SIZE;
        m_osCurKey = sKey;
        m_bKeySet = true;
    }
}

/************************************************************************/
/*                             StartArray()                             */
/************************************************************************/

void OGRJSONCollectionStreamingParser::StartArray()
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    if (m_nDepth == 1 && m_bInFeatures)
    {
        m_bInFeaturesArray = true;
    }
    else if (m_poCurObj)
    {
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            m_osJson += "[";
            m_abFirstMember.push_back(true);
        }

        m_nCurObjMemEstimate += ESTIMATE_ARRAY_SIZE;

        json_object *poNewObj = json_object_new_array();
        AppendObject(poNewObj);
        m_apoCurObj.push_back(poNewObj);
    }
    m_nDepth++;
}

/************************************************************************/
/*                          StartArrayMember()                          */
/************************************************************************/

void OGRJSONCollectionStreamingParser::StartArrayMember()
{
    if (m_poCurObj)
    {
        m_nCurObjMemEstimate += ESTIMATE_ARRAY_ELT_SIZE;

        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            if (!m_abFirstMember.back())
                m_osJson += ",";
            m_abFirstMember.back() = false;
        }
    }
}

/************************************************************************/
/*                              EndArray()                              */
/************************************************************************/

void OGRJSONCollectionStreamingParser::EndArray()
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    m_nDepth--;
    if (m_nDepth == 1 && m_bInFeaturesArray)
    {
        m_bInFeaturesArray = false;
    }
    else if (m_poCurObj)
    {
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            m_abFirstMember.pop_back();
            m_osJson += "]";
        }

        m_apoCurObj.pop_back();
    }
}

/************************************************************************/
/*                               String()                               */
/************************************************************************/

void OGRJSONCollectionStreamingParser::String(std::string_view sValue)
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    if (m_nDepth == 1 && m_bInType)
    {
        m_bIsTypeKnown = true;
        m_bIsFeatureCollection = sValue == "FeatureCollection";
    }
    else if (m_poCurObj)
    {
        if (m_bFirstPass)
        {
            if (m_bInFeaturesArray)
                m_nTotalOGRFeatureMemEstimate +=
                    sizeof(OGRField) + sValue.size();

            m_nCurObjMemEstimate += ESTIMATE_BASE_OBJECT_SIZE;
            m_nCurObjMemEstimate += sValue.size() + sizeof(void *);
        }
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            m_osJson += CPLJSonStreamingParser::GetSerializedString(sValue);
        }
        if (sValue.size() < static_cast<size_t>(INT_MAX - 1))
            AppendObject(json_object_new_string_len(
                sValue.data(), static_cast<int>(sValue.size())));
        else
            EmitException(
                "OGRJSONCollectionStreamingParser::String(): too large string");
    }
}

/************************************************************************/
/*                               Number()                               */
/************************************************************************/

// recent libc++ std::from_chars() involve unsigned integer overflow
CPL_NOSANITIZE_UNSIGNED_INT_OVERFLOW
void OGRJSONCollectionStreamingParser::Number(std::string_view sValue)
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    if (m_poCurObj)
    {
        if (m_bFirstPass)
        {
            if (m_bInFeaturesArray)
            {
                if (m_bInCoordinates)
                    m_nTotalOGRFeatureMemEstimate += sizeof(double);
                else
                    m_nTotalOGRFeatureMemEstimate += sizeof(OGRField);
            }

            m_nCurObjMemEstimate += ESTIMATE_BASE_OBJECT_SIZE;
        }
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            m_osJson.append(sValue);
        }

        if (sValue.size() == strlen("Infinity") &&
            EQUALN(sValue.data(), "Infinity", strlen("Infinity")))
        {
            AppendObject(json_object_new_double(
                std::numeric_limits<double>::infinity()));
        }
        else if (sValue.size() == strlen("-Infinity") &&
                 EQUALN(sValue.data(), "-Infinity", strlen("-Infinity")))
        {
            AppendObject(json_object_new_double(
                -std::numeric_limits<double>::infinity()));
        }
        else if (sValue.size() == strlen("NaN") &&
                 EQUALN(sValue.data(), "NaN", strlen("NaN")))
        {
            AppendObject(json_object_new_double(
                std::numeric_limits<double>::quiet_NaN()));
        }
        else if (sValue.find_first_of("eE.") != std::string::npos ||
                 sValue.size() >= 20)
        {
            double dfValue = 0;
            const fast_float::parse_options options{
                fast_float::chars_format::general, '.'};
            auto answer = fast_float::from_chars_advanced(
                sValue.data(), sValue.data() + sValue.size(), dfValue, options);
            if (answer.ec == std::errc() &&
                answer.ptr == sValue.data() + sValue.size())
            {
                AppendObject(json_object_new_double(dfValue));
            }
            else
            {
                EmitException(
                    ("Unrecognized number: " + std::string(sValue)).c_str());
            }
        }
        else
        {
            GIntBig nValue = 0;
            auto answer = std::from_chars(
                sValue.data(), sValue.data() + sValue.size(), nValue);
            if (answer.ec == std::errc() &&
                answer.ptr == sValue.data() + sValue.size())
            {
                AppendObject(json_object_new_int64(nValue));
            }
            else
            {
                EmitException(
                    ("Unrecognized number: " + std::string(sValue)).c_str());
            }
        }
    }
}

/************************************************************************/
/*                              Boolean()                               */
/************************************************************************/

void OGRJSONCollectionStreamingParser::Boolean(bool bVal)
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    if (m_bInMeasuresEnabled)
        m_bHasTopLevelMeasures = bVal;

    if (m_poCurObj)
    {
        if (m_bFirstPass)
        {
            if (m_bInFeaturesArray)
                m_nTotalOGRFeatureMemEstimate += sizeof(OGRField);

            m_nCurObjMemEstimate += ESTIMATE_BASE_OBJECT_SIZE;
        }
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            m_osJson += bVal ? "true" : "false";
        }

        AppendObject(json_object_new_boolean(bVal));
    }
}

/************************************************************************/
/*                                Null()                                */
/************************************************************************/

void OGRJSONCollectionStreamingParser::Null()
{
    if (m_nMaxObjectSize > 0 && m_nCurObjMemEstimate > m_nMaxObjectSize)
    {
        TooComplex();
        return;
    }

    if (m_poCurObj)
    {
        if (m_bInFeaturesArray && m_bStoreNativeData && m_nDepth >= 3)
        {
            m_osJson += "null";
        }

        m_nCurObjMemEstimate += ESTIMATE_BASE_OBJECT_SIZE;
        AppendObject(nullptr);
    }
}

/************************************************************************/
/*                             Exception()                              */
/************************************************************************/

void OGRJSONCollectionStreamingParser::Exception(const char *pszMessage)
{
    CPLError(CE_Failure, CPLE_AppDefined, "%s", pszMessage);
}
