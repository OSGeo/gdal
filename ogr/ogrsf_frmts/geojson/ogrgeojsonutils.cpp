/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of private utilities used within OGR GeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrgeojsonutils.h"
#include <assert.h>
#include "cpl_port.h"
#include "cpl_conv.h"
#include "cpl_json_streaming_parser.h"
#include "ogr_geometry.h"
#include <json.h>  // JSON-C

#include <algorithm>
#include <memory>

constexpr const char szESRIJSonFeaturesGeometryRings[] =
    "\"features\":[{\"geometry\":{\"rings\":[";

// Cf https://github.com/OSGeo/gdal/issues/9996#issuecomment-2129845692
constexpr const char szESRIJSonFeaturesAttributes[] =
    "\"features\":[{\"attributes\":{";

/************************************************************************/
/*                           SkipUTF8BOM()                              */
/************************************************************************/

static void SkipUTF8BOM(const char *&pszText)
{
    /* Skip UTF-8 BOM (#5630) */
    const GByte *pabyData = reinterpret_cast<const GByte *>(pszText);
    if (pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF)
        pszText += 3;
}

/************************************************************************/
/*                           IsJSONObject()                             */
/************************************************************************/

static bool IsJSONObject(const char *pszText)
{
    if (nullptr == pszText)
        return false;

    SkipUTF8BOM(pszText);

    /* -------------------------------------------------------------------- */
    /*      This is a primitive test, but we need to perform it fast.       */
    /* -------------------------------------------------------------------- */
    while (*pszText != '\0' && isspace(static_cast<unsigned char>(*pszText)))
        pszText++;

    const char *const apszPrefix[] = {"loadGeoJSON(", "jsonp("};
    for (size_t iP = 0; iP < sizeof(apszPrefix) / sizeof(apszPrefix[0]); iP++)
    {
        if (strncmp(pszText, apszPrefix[iP], strlen(apszPrefix[iP])) == 0)
        {
            pszText += strlen(apszPrefix[iP]);
            break;
        }
    }

    if (*pszText != '{')
        return false;

    return true;
}

/************************************************************************/
/*                           GetTopLevelType()                          */
/************************************************************************/

static std::string GetTopLevelType(const char *pszText)
{
    if (!strstr(pszText, "\"type\""))
        return std::string();

    SkipUTF8BOM(pszText);

    struct MyParser : public CPLJSonStreamingParser
    {
        std::string m_osLevel{};
        bool m_bInTopLevelType = false;
        std::string m_osTopLevelTypeValue{};

        void StartObjectMember(const char *pszKey, size_t nLength) override
        {
            m_bInTopLevelType = false;
            if (nLength == strlen("type") && strcmp(pszKey, "type") == 0 &&
                m_osLevel == "{")
            {
                m_bInTopLevelType = true;
            }
        }

        void String(const char *pszValue, size_t nLength) override
        {
            if (m_bInTopLevelType)
            {
                m_osTopLevelTypeValue.assign(pszValue, nLength);
                StopParsing();
            }
        }

        void StartObject() override
        {
            m_osLevel += '{';
            m_bInTopLevelType = false;
        }

        void EndObject() override
        {
            if (!m_osLevel.empty())
                m_osLevel.pop_back();
            m_bInTopLevelType = false;
        }

        void StartArray() override
        {
            m_osLevel += '[';
            m_bInTopLevelType = false;
        }

        void EndArray() override
        {
            if (!m_osLevel.empty())
                m_osLevel.pop_back();
            m_bInTopLevelType = false;
        }
    };

    MyParser oParser;
    oParser.Parse(pszText, strlen(pszText), true);
    return oParser.m_osTopLevelTypeValue;
}

/************************************************************************/
/*                           GetCompactJSon()                           */
/************************************************************************/

static CPLString GetCompactJSon(const char *pszText, size_t nMaxSize)
{
    /* Skip UTF-8 BOM (#5630) */
    const GByte *pabyData = reinterpret_cast<const GByte *>(pszText);
    if (pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF)
        pszText += 3;

    CPLString osWithoutSpace;
    bool bInString = false;
    for (int i = 0; pszText[i] != '\0' && osWithoutSpace.size() < nMaxSize; i++)
    {
        if (bInString)
        {
            if (pszText[i] == '\\')
            {
                osWithoutSpace += pszText[i];
                if (pszText[i + 1] == '\0')
                    break;
                osWithoutSpace += pszText[i + 1];
                i++;
            }
            else if (pszText[i] == '"')
            {
                bInString = false;
                osWithoutSpace += '"';
            }
            else
            {
                osWithoutSpace += pszText[i];
            }
        }
        else if (pszText[i] == '"')
        {
            bInString = true;
            osWithoutSpace += '"';
        }
        else if (!isspace(static_cast<unsigned char>(pszText[i])))
        {
            osWithoutSpace += pszText[i];
        }
    }
    return osWithoutSpace;
}

/************************************************************************/
/*                          IsGeoJSONLikeObject()                       */
/************************************************************************/

static bool IsGeoJSONLikeObject(const char *pszText, bool &bMightBeSequence,
                                bool &bReadMoreBytes, GDALOpenInfo *poOpenInfo,
                                const char *pszExpectedDriverName)
{
    bMightBeSequence = false;
    bReadMoreBytes = false;

    if (!IsJSONObject(pszText))
        return false;

    const std::string osTopLevelType = GetTopLevelType(pszText);
    if (osTopLevelType == "Topology")
        return false;

    if (poOpenInfo->IsSingleAllowedDriver(pszExpectedDriverName) &&
        GDALGetDriverByName(pszExpectedDriverName))
    {
        return true;
    }

    if ((!poOpenInfo->papszAllowedDrivers ||
         CSLFindString(poOpenInfo->papszAllowedDrivers, "JSONFG") >= 0) &&
        GDALGetDriverByName("JSONFG") && JSONFGIsObject(pszText, poOpenInfo))
    {
        return false;
    }

    if (osTopLevelType == "FeatureCollection")
    {
        return true;
    }

    const std::string osWithoutSpace = GetCompactJSon(pszText, strlen(pszText));
    if (osWithoutSpace.find("{\"features\":[") == 0 &&
        osWithoutSpace.find(szESRIJSonFeaturesGeometryRings) ==
            std::string::npos &&
        osWithoutSpace.find(szESRIJSonFeaturesAttributes) == std::string::npos)
    {
        return true;
    }

    // See
    // https://raw.githubusercontent.com/icepack/icepack-data/master/meshes/larsen/larsen_inflow.geojson
    // "{"crs":...,"features":[..."
    // or
    // https://gist.githubusercontent.com/NiklasDallmann/27e339dd78d1942d524fbcd179f9fdcf/raw/527a8319d75a9e29446a32a19e4c902213b0d668/42XR9nLAh8Poh9Xmniqh3Bs9iisNm74mYMC56v3Wfyo=_isochrones_fails.geojson
    // "{"bbox":...,"features":[..."
    if (osWithoutSpace.find(",\"features\":[") != std::string::npos)
    {
        return !ESRIJSONIsObject(pszText, poOpenInfo);
    }

    // See https://github.com/OSGeo/gdal/issues/2720
    if (osWithoutSpace.find("{\"coordinates\":[") == 0 ||
        // and https://github.com/OSGeo/gdal/issues/2787
        osWithoutSpace.find("{\"geometry\":{\"coordinates\":[") == 0 ||
        // and https://github.com/qgis/QGIS/issues/61266
        osWithoutSpace.find(
            "{\"geometry\":{\"type\":\"Point\",\"coordinates\":[") == 0 ||
        osWithoutSpace.find(
            "{\"geometry\":{\"type\":\"LineString\",\"coordinates\":[") == 0 ||
        osWithoutSpace.find(
            "{\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[") == 0 ||
        osWithoutSpace.find(
            "{\"geometry\":{\"type\":\"MultiPoint\",\"coordinates\":[") == 0 ||
        osWithoutSpace.find(
            "{\"geometry\":{\"type\":\"MultiLineString\",\"coordinates\":[") ==
            0 ||
        osWithoutSpace.find(
            "{\"geometry\":{\"type\":\"MultiPolygon\",\"coordinates\":[") ==
            0 ||
        osWithoutSpace.find("{\"geometry\":{\"type\":\"GeometryCollection\","
                            "\"geometries\":[") == 0)
    {
        return true;
    }

    if (osTopLevelType == "Feature" || osTopLevelType == "Point" ||
        osTopLevelType == "LineString" || osTopLevelType == "Polygon" ||
        osTopLevelType == "MultiPoint" || osTopLevelType == "MultiLineString" ||
        osTopLevelType == "MultiPolygon" ||
        osTopLevelType == "GeometryCollection")
    {
        bMightBeSequence = true;
        return true;
    }

    // See https://github.com/OSGeo/gdal/issues/3280
    if (osWithoutSpace.find("{\"properties\":{") == 0)
    {
        bMightBeSequence = true;
        bReadMoreBytes = true;
        return false;
    }

    return false;
}

static bool IsGeoJSONLikeObject(const char *pszText, GDALOpenInfo *poOpenInfo,
                                const char *pszExpectedDriverName)
{
    bool bMightBeSequence;
    bool bReadMoreBytes;
    return IsGeoJSONLikeObject(pszText, bMightBeSequence, bReadMoreBytes,
                               poOpenInfo, pszExpectedDriverName);
}

/************************************************************************/
/*                       ESRIJSONIsObject()                             */
/************************************************************************/

bool ESRIJSONIsObject(const char *pszText, GDALOpenInfo *poOpenInfo)
{
    if (!IsJSONObject(pszText))
        return false;

    if (poOpenInfo->IsSingleAllowedDriver("ESRIJSON") &&
        GDALGetDriverByName("ESRIJSON"))
    {
        return true;
    }

    if (  // ESRI Json geometry
        (strstr(pszText, "\"geometryType\"") != nullptr &&
         strstr(pszText, "\"esriGeometry") != nullptr)

        // ESRI Json "FeatureCollection"
        || strstr(pszText, "\"fieldAliases\"") != nullptr

        // ESRI Json "FeatureCollection"
        || (strstr(pszText, "\"fields\"") != nullptr &&
            strstr(pszText, "\"esriFieldType") != nullptr))
    {
        return true;
    }

    const std::string osWithoutSpace = GetCompactJSon(pszText, strlen(pszText));
    if (osWithoutSpace.find(szESRIJSonFeaturesGeometryRings) !=
            std::string::npos ||
        osWithoutSpace.find(szESRIJSonFeaturesAttributes) !=
            std::string::npos ||
        osWithoutSpace.find("\"spatialReference\":{\"wkid\":") !=
            std::string::npos)
    {
        return true;
    }

    return false;
}

/************************************************************************/
/*                       TopoJSONIsObject()                             */
/************************************************************************/

bool TopoJSONIsObject(const char *pszText, GDALOpenInfo *poOpenInfo)
{
    if (!IsJSONObject(pszText))
        return false;

    if (poOpenInfo->IsSingleAllowedDriver("TopoJSON") &&
        GDALGetDriverByName("TopoJSON"))
    {
        return true;
    }

    return GetTopLevelType(pszText) == "Topology";
}

/************************************************************************/
/*                      IsLikelyNewlineSequenceGeoJSON()                */
/************************************************************************/

static GDALIdentifyEnum
IsLikelyNewlineSequenceGeoJSON(VSILFILE *fpL, const GByte *pabyHeader,
                               const char *pszFileContent)
{
    const size_t nBufferSize = 4096 * 10;
    std::vector<GByte> abyBuffer;
    abyBuffer.resize(nBufferSize + 1);

    int nCurlLevel = 0;
    bool bInString = false;
    bool bLastIsEscape = false;
    bool bFirstIter = true;
    bool bEOLFound = false;
    int nCountObject = 0;
    while (true)
    {
        size_t nRead;
        bool bEnd = false;
        if (bFirstIter)
        {
            const char *pszText =
                pszFileContent ? pszFileContent
                               : reinterpret_cast<const char *>(pabyHeader);
            assert(pszText);
            nRead = std::min(strlen(pszText), nBufferSize);
            memcpy(abyBuffer.data(), pszText, nRead);
            bFirstIter = false;
            if (fpL)
            {
                VSIFSeekL(fpL, nRead, SEEK_SET);
            }
        }
        else
        {
            nRead = VSIFReadL(abyBuffer.data(), 1, nBufferSize, fpL);
            bEnd = nRead < nBufferSize;
        }
        for (size_t i = 0; i < nRead; i++)
        {
            if (nCurlLevel == 0)
            {
                if (abyBuffer[i] == '{')
                {
                    nCountObject++;
                    if (nCountObject == 2)
                    {
                        break;
                    }
                    nCurlLevel++;
                }
                else if (nCountObject == 1 && abyBuffer[i] == '\n')
                {
                    bEOLFound = true;
                }
                else if (!isspace(static_cast<unsigned char>(abyBuffer[i])))
                {
                    return GDAL_IDENTIFY_FALSE;
                }
            }
            else if (bInString)
            {
                if (bLastIsEscape)
                {
                    bLastIsEscape = false;
                }
                else if (abyBuffer[i] == '\\')
                {
                    bLastIsEscape = true;
                }
                else if (abyBuffer[i] == '"')
                {
                    bInString = false;
                }
            }
            else if (abyBuffer[i] == '"')
            {
                bInString = true;
            }
            else if (abyBuffer[i] == '{')
            {
                nCurlLevel++;
            }
            else if (abyBuffer[i] == '}')
            {
                nCurlLevel--;
            }
        }
        if (!fpL || bEnd || nCountObject == 2)
            break;
    }
    if (bEOLFound && nCountObject == 2)
        return GDAL_IDENTIFY_TRUE;
    return GDAL_IDENTIFY_UNKNOWN;
}

/************************************************************************/
/*                           GeoJSONFileIsObject()                      */
/************************************************************************/

static bool GeoJSONFileIsObject(GDALOpenInfo *poOpenInfo)
{
    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.

    if (poOpenInfo->fpL == nullptr || !poOpenInfo->TryToIngest(6000))
    {
        return false;
    }

    bool bMightBeSequence = false;
    bool bReadMoreBytes = false;
    if (!IsGeoJSONLikeObject(
            reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
            bMightBeSequence, bReadMoreBytes, poOpenInfo, "GeoJSON"))
    {
        if (!(bReadMoreBytes && poOpenInfo->nHeaderBytes >= 6000 &&
              poOpenInfo->TryToIngest(1000 * 1000) &&
              !IsGeoJSONLikeObject(
                  reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                  bMightBeSequence, bReadMoreBytes, poOpenInfo, "GeoJSON")))
        {
            return false;
        }
    }

    return !(bMightBeSequence && IsLikelyNewlineSequenceGeoJSON(
                                     poOpenInfo->fpL, poOpenInfo->pabyHeader,
                                     nullptr) == GDAL_IDENTIFY_TRUE);
}

/************************************************************************/
/*                           GeoJSONIsObject()                          */
/************************************************************************/

bool GeoJSONIsObject(const char *pszText, GDALOpenInfo *poOpenInfo)
{
    bool bMightBeSequence = false;
    bool bReadMoreBytes = false;
    if (!IsGeoJSONLikeObject(pszText, bMightBeSequence, bReadMoreBytes,
                             poOpenInfo, "GeoJSON"))
    {
        return false;
    }

    return !(bMightBeSequence &&
             IsLikelyNewlineSequenceGeoJSON(nullptr, nullptr, pszText) ==
                 GDAL_IDENTIFY_TRUE);
}

/************************************************************************/
/*                        GeoJSONSeqFileIsObject()                      */
/************************************************************************/

static bool GeoJSONSeqFileIsObject(GDALOpenInfo *poOpenInfo)
{
    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.

    if (poOpenInfo->fpL == nullptr || !poOpenInfo->TryToIngest(6000))
    {
        return false;
    }

    const char *pszText =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    if (pszText[0] == '\x1e')
        return IsGeoJSONLikeObject(pszText + 1, poOpenInfo, "GeoJSONSeq");

    bool bMightBeSequence = false;
    bool bReadMoreBytes = false;
    if (!IsGeoJSONLikeObject(pszText, bMightBeSequence, bReadMoreBytes,
                             poOpenInfo, "GeoJSONSeq"))
    {
        if (!(bReadMoreBytes && poOpenInfo->nHeaderBytes >= 6000 &&
              poOpenInfo->TryToIngest(1000 * 1000) &&
              IsGeoJSONLikeObject(
                  reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                  bMightBeSequence, bReadMoreBytes, poOpenInfo, "GeoJSONSeq")))
        {
            return false;
        }
    }

    if (poOpenInfo->IsSingleAllowedDriver("GeoJSONSeq") &&
        IsLikelyNewlineSequenceGeoJSON(poOpenInfo->fpL, poOpenInfo->pabyHeader,
                                       nullptr) != GDAL_IDENTIFY_FALSE &&
        GDALGetDriverByName("GeoJSONSeq"))
    {
        return true;
    }

    return bMightBeSequence && IsLikelyNewlineSequenceGeoJSON(
                                   poOpenInfo->fpL, poOpenInfo->pabyHeader,
                                   nullptr) == GDAL_IDENTIFY_TRUE;
}

bool GeoJSONSeqIsObject(const char *pszText, GDALOpenInfo *poOpenInfo)
{
    if (pszText[0] == '\x1e')
        return IsGeoJSONLikeObject(pszText + 1, poOpenInfo, "GeoJSONSeq");

    bool bMightBeSequence = false;
    bool bReadMoreBytes = false;
    if (!IsGeoJSONLikeObject(pszText, bMightBeSequence, bReadMoreBytes,
                             poOpenInfo, "GeoJSONSeq"))
    {
        return false;
    }

    if (poOpenInfo->IsSingleAllowedDriver("GeoJSONSeq") &&
        IsLikelyNewlineSequenceGeoJSON(nullptr, nullptr, pszText) !=
            GDAL_IDENTIFY_FALSE &&
        GDALGetDriverByName("GeoJSONSeq"))
    {
        return true;
    }

    return bMightBeSequence &&
           IsLikelyNewlineSequenceGeoJSON(nullptr, nullptr, pszText) ==
               GDAL_IDENTIFY_TRUE;
}

/************************************************************************/
/*                        JSONFGFileIsObject()                          */
/************************************************************************/

static bool JSONFGFileIsObject(GDALOpenInfo *poOpenInfo)
{
    // 6000 somewhat arbitrary. Based on other JSON-like drivers
    if (poOpenInfo->fpL == nullptr || !poOpenInfo->TryToIngest(6000))
    {
        return false;
    }

    const char *pszText =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);
    return JSONFGIsObject(pszText, poOpenInfo);
}

bool JSONFGIsObject(const char *pszText, GDALOpenInfo *poOpenInfo)
{
    if (!IsJSONObject(pszText))
        return false;

    if (poOpenInfo->IsSingleAllowedDriver("JSONFG") &&
        GDALGetDriverByName("JSONFG"))
    {
        return true;
    }

    const std::string osWithoutSpace = GetCompactJSon(pszText, strlen(pszText));

    // In theory, conformsTo should be required, but let be lax...
    {
        const auto nPos = osWithoutSpace.find("\"conformsTo\":[");
        if (nPos != std::string::npos)
        {
            for (const char *pszVersion : {"0.1", "0.2", "0.3"})
            {
                if (osWithoutSpace.find(
                        CPLSPrintf("\"[ogc-json-fg-1-%s:core]\"", pszVersion),
                        nPos) != std::string::npos ||
                    osWithoutSpace.find(
                        CPLSPrintf(
                            "\"http://www.opengis.net/spec/json-fg-1/%s\"",
                            pszVersion),
                        nPos) != std::string::npos)
                {
                    return true;
                }
            }
        }
    }

    if (osWithoutSpace.find("\"place\":{\"type\":") != std::string::npos ||
        osWithoutSpace.find("\"place\":{\"coordinates\":") !=
            std::string::npos ||
        osWithoutSpace.find("\"time\":{\"date\":") != std::string::npos ||
        osWithoutSpace.find("\"time\":{\"timestamp\":") != std::string::npos ||
        osWithoutSpace.find("\"time\":{\"interval\":") != std::string::npos)
    {
        return true;
    }

    if (osWithoutSpace.find("\"coordRefSys\":") != std::string::npos ||
        osWithoutSpace.find("\"featureType\":") != std::string::npos)
    {
        // Check that coordRefSys and/or featureType are either at the
        // FeatureCollection or Feature level
        struct MyParser : public CPLJSonStreamingParser
        {
            bool m_bFoundJSONFGFeatureType = false;
            bool m_bFoundJSONFGCoordrefSys = false;
            std::string m_osLevel{};

            void StartObjectMember(const char *pszKey, size_t nLength) override
            {
                if (nLength == strlen("featureType") &&
                    strcmp(pszKey, "featureType") == 0)
                {
                    m_bFoundJSONFGFeatureType =
                        (m_osLevel == "{" ||   // At FeatureCollection level
                         m_osLevel == "{[{");  // At Feature level
                    if (m_bFoundJSONFGFeatureType)
                        StopParsing();
                }
                else if (nLength == strlen("coordRefSys") &&
                         strcmp(pszKey, "coordRefSys") == 0)
                {
                    m_bFoundJSONFGCoordrefSys =
                        (m_osLevel == "{" ||   // At FeatureCollection level
                         m_osLevel == "{[{");  // At Feature level
                    if (m_bFoundJSONFGCoordrefSys)
                        StopParsing();
                }
            }

            void StartObject() override
            {
                m_osLevel += '{';
            }

            void EndObject() override
            {
                if (!m_osLevel.empty())
                    m_osLevel.pop_back();
            }

            void StartArray() override
            {
                m_osLevel += '[';
            }

            void EndArray() override
            {
                if (!m_osLevel.empty())
                    m_osLevel.pop_back();
            }
        };

        MyParser oParser;
        oParser.Parse(pszText, strlen(pszText), true);
        if (oParser.m_bFoundJSONFGFeatureType ||
            oParser.m_bFoundJSONFGCoordrefSys)
        {
            return true;
        }
    }

    return false;
}

/************************************************************************/
/*                           IsLikelyESRIJSONURL()                      */
/************************************************************************/

static bool IsLikelyESRIJSONURL(const char *pszURL)
{
    // URLs with f=json are strong candidates for ESRI JSON services
    // except if they have "/items?", in which case they are likely OAPIF
    return (strstr(pszURL, "f=json") != nullptr ||
            strstr(pszURL, "f=pjson") != nullptr ||
            strstr(pszURL, "resultRecordCount=") != nullptr) &&
           strstr(pszURL, "/items?") == nullptr;
}

/************************************************************************/
/*                           GeoJSONGetSourceType()                     */
/************************************************************************/

GeoJSONSourceType GeoJSONGetSourceType(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType srcType = eGeoJSONSourceUnknown;

    // NOTE: Sometimes URL ends with .geojson token, for example
    //       http://example/path/2232.geojson
    //       It's important to test beginning of source first.
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSON:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSON:https://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSON:ftp://"))
    {
        srcType = eGeoJSONSourceService;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "ftp://"))
    {
        if (poOpenInfo->IsSingleAllowedDriver("GeoJSON"))
        {
            return eGeoJSONSourceService;
        }
        if ((strstr(poOpenInfo->pszFilename, "SERVICE=WFS") ||
             strstr(poOpenInfo->pszFilename, "service=WFS") ||
             strstr(poOpenInfo->pszFilename, "service=wfs")) &&
            !strstr(poOpenInfo->pszFilename, "json"))
        {
            return eGeoJSONSourceUnknown;
        }
        if (IsLikelyESRIJSONURL(poOpenInfo->pszFilename))
        {
            return eGeoJSONSourceUnknown;
        }
        srcType = eGeoJSONSourceService;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSON:"))
    {
        VSIStatBufL sStat;
        if (VSIStatL(poOpenInfo->pszFilename + strlen("GeoJSON:"), &sStat) == 0)
        {
            return eGeoJSONSourceFile;
        }
        const char *pszText = poOpenInfo->pszFilename + strlen("GeoJSON:");
        if (GeoJSONIsObject(pszText, poOpenInfo))
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }
    else if (GeoJSONIsObject(poOpenInfo->pszFilename, poOpenInfo))
    {
        srcType = eGeoJSONSourceText;
    }
    else if (GeoJSONFileIsObject(poOpenInfo))
    {
        srcType = eGeoJSONSourceFile;
    }

    return srcType;
}

/************************************************************************/
/*                     ESRIJSONDriverGetSourceType()                    */
/************************************************************************/

GeoJSONSourceType ESRIJSONDriverGetSourceType(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:https://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:ftp://"))
    {
        return eGeoJSONSourceService;
    }
    else if (STARTS_WITH(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH(poOpenInfo->pszFilename, "ftp://"))
    {
        if (poOpenInfo->IsSingleAllowedDriver("ESRIJSON"))
        {
            return eGeoJSONSourceService;
        }
        if (IsLikelyESRIJSONURL(poOpenInfo->pszFilename))
        {
            return eGeoJSONSourceService;
        }
        return eGeoJSONSourceUnknown;
    }

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:"))
    {
        VSIStatBufL sStat;
        if (VSIStatL(poOpenInfo->pszFilename + strlen("ESRIJSON:"), &sStat) ==
            0)
        {
            return eGeoJSONSourceFile;
        }
        const char *pszText = poOpenInfo->pszFilename + strlen("ESRIJSON:");
        if (ESRIJSONIsObject(pszText, poOpenInfo))
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    if (poOpenInfo->fpL == nullptr)
    {
        const char *pszText = poOpenInfo->pszFilename;
        if (ESRIJSONIsObject(pszText, poOpenInfo))
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.
    if (!poOpenInfo->TryToIngest(6000))
    {
        return eGeoJSONSourceUnknown;
    }

    if (poOpenInfo->pabyHeader != nullptr &&
        ESRIJSONIsObject(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                         poOpenInfo))
    {
        return eGeoJSONSourceFile;
    }
    return eGeoJSONSourceUnknown;
}

/************************************************************************/
/*                     TopoJSONDriverGetSourceType()                    */
/************************************************************************/

GeoJSONSourceType TopoJSONDriverGetSourceType(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:https://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:ftp://"))
    {
        return eGeoJSONSourceService;
    }
    else if (STARTS_WITH(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH(poOpenInfo->pszFilename, "ftp://"))
    {
        if (poOpenInfo->IsSingleAllowedDriver("TOPOJSON"))
        {
            return eGeoJSONSourceService;
        }
        if (IsLikelyESRIJSONURL(poOpenInfo->pszFilename))
        {
            return eGeoJSONSourceUnknown;
        }
        return eGeoJSONSourceService;
    }

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:"))
    {
        VSIStatBufL sStat;
        if (VSIStatL(poOpenInfo->pszFilename + strlen("TopoJSON:"), &sStat) ==
            0)
        {
            return eGeoJSONSourceFile;
        }
        const char *pszText = poOpenInfo->pszFilename + strlen("TopoJSON:");
        if (TopoJSONIsObject(pszText, poOpenInfo))
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    if (poOpenInfo->fpL == nullptr)
    {
        const char *pszText = poOpenInfo->pszFilename;
        if (TopoJSONIsObject(pszText, poOpenInfo))
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.
    if (!poOpenInfo->TryToIngest(6000))
    {
        return eGeoJSONSourceUnknown;
    }

    if (poOpenInfo->pabyHeader != nullptr &&
        TopoJSONIsObject(reinterpret_cast<const char *>(poOpenInfo->pabyHeader),
                         poOpenInfo))
    {
        return eGeoJSONSourceFile;
    }
    return eGeoJSONSourceUnknown;
}

/************************************************************************/
/*                          GeoJSONSeqGetSourceType()                   */
/************************************************************************/

GeoJSONSourceType GeoJSONSeqGetSourceType(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType srcType = eGeoJSONSourceUnknown;

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSONSeq:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSONSeq:https://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSONSeq:ftp://"))
    {
        srcType = eGeoJSONSourceService;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "ftp://"))
    {
        if (poOpenInfo->IsSingleAllowedDriver("GeoJSONSeq"))
        {
            return eGeoJSONSourceService;
        }
        if (IsLikelyESRIJSONURL(poOpenInfo->pszFilename))
        {
            return eGeoJSONSourceUnknown;
        }
        srcType = eGeoJSONSourceService;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSONSeq:"))
    {
        VSIStatBufL sStat;
        if (VSIStatL(poOpenInfo->pszFilename + strlen("GEOJSONSeq:"), &sStat) ==
            0)
        {
            return eGeoJSONSourceFile;
        }
        const char *pszText = poOpenInfo->pszFilename + strlen("GEOJSONSeq:");
        if (GeoJSONSeqIsObject(pszText, poOpenInfo))
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }
    else if (GeoJSONSeqIsObject(poOpenInfo->pszFilename, poOpenInfo))
    {
        srcType = eGeoJSONSourceText;
    }
    else if (GeoJSONSeqFileIsObject(poOpenInfo))
    {
        srcType = eGeoJSONSourceFile;
    }

    return srcType;
}

/************************************************************************/
/*                      JSONFGDriverGetSourceType()                     */
/************************************************************************/

GeoJSONSourceType JSONFGDriverGetSourceType(GDALOpenInfo *poOpenInfo)
{
    GeoJSONSourceType srcType = eGeoJSONSourceUnknown;

    if (STARTS_WITH_CI(poOpenInfo->pszFilename, "JSONFG:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "JSONFG:https://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "JSONFG:ftp://"))
    {
        srcType = eGeoJSONSourceService;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "ftp://"))
    {
        if (poOpenInfo->IsSingleAllowedDriver("JSONFG"))
        {
            return eGeoJSONSourceService;
        }
        if (IsLikelyESRIJSONURL(poOpenInfo->pszFilename))
        {
            return eGeoJSONSourceUnknown;
        }
        srcType = eGeoJSONSourceService;
    }
    else if (STARTS_WITH_CI(poOpenInfo->pszFilename, "JSONFG:"))
    {
        VSIStatBufL sStat;
        const size_t nJSONFGPrefixLen = strlen("JSONFG:");
        if (VSIStatL(poOpenInfo->pszFilename + nJSONFGPrefixLen, &sStat) == 0)
        {
            return eGeoJSONSourceFile;
        }
        const char *pszText = poOpenInfo->pszFilename + nJSONFGPrefixLen;
        if (JSONFGIsObject(pszText, poOpenInfo))
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }
    else if (JSONFGIsObject(poOpenInfo->pszFilename, poOpenInfo))
    {
        srcType = eGeoJSONSourceText;
    }
    else if (JSONFGFileIsObject(poOpenInfo))
    {
        srcType = eGeoJSONSourceFile;
    }

    return srcType;
}

/************************************************************************/
/*                        GeoJSONStringPropertyToFieldType()            */
/************************************************************************/

OGRFieldType GeoJSONStringPropertyToFieldType(json_object *poObject,
                                              int &nTZFlag)
{
    if (poObject == nullptr)
    {
        return OFTString;
    }
    const char *pszStr = json_object_get_string(poObject);

    nTZFlag = 0;
    OGRField sWrkField;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    const bool bSuccess = CPL_TO_BOOL(OGRParseDate(pszStr, &sWrkField, 0));
    CPLPopErrorHandler();
    CPLErrorReset();
    if (bSuccess)
    {
        const bool bHasDate =
            strchr(pszStr, '/') != nullptr || strchr(pszStr, '-') != nullptr;
        const bool bHasTime = strchr(pszStr, ':') != nullptr;
        nTZFlag = sWrkField.Date.TZFlag;
        if (bHasDate && bHasTime)
            return OFTDateTime;
        else if (bHasDate)
            return OFTDate;
        else
            return OFTTime;
        // TODO: What if both are false?
    }
    return OFTString;
}

/************************************************************************/
/*                   GeoJSONHTTPFetchWithContentTypeHeader()            */
/************************************************************************/

CPLHTTPResult *GeoJSONHTTPFetchWithContentTypeHeader(const char *pszURL)
{
    std::string osHeaders;
    const char *pszGDAL_HTTP_HEADERS =
        CPLGetConfigOption("GDAL_HTTP_HEADERS", nullptr);
    bool bFoundAcceptHeader = false;
    if (pszGDAL_HTTP_HEADERS)
    {
        bool bHeadersDone = false;
        // Compatibility hack for "HEADERS=Accept: text/plain, application/json"
        if (strstr(pszGDAL_HTTP_HEADERS, "\r\n") == nullptr)
        {
            const char *pszComma = strchr(pszGDAL_HTTP_HEADERS, ',');
            if (pszComma != nullptr && strchr(pszComma, ':') == nullptr)
            {
                osHeaders = pszGDAL_HTTP_HEADERS;
                bFoundAcceptHeader =
                    STARTS_WITH_CI(pszGDAL_HTTP_HEADERS, "Accept:");
                bHeadersDone = true;
            }
        }
        if (!bHeadersDone)
        {
            // We accept both raw headers with \r\n as a separator, or as
            // a comma separated list of foo: bar values.
            const CPLStringList aosTokens(
                strstr(pszGDAL_HTTP_HEADERS, "\r\n")
                    ? CSLTokenizeString2(pszGDAL_HTTP_HEADERS, "\r\n", 0)
                    : CSLTokenizeString2(pszGDAL_HTTP_HEADERS, ",",
                                         CSLT_HONOURSTRINGS));
            for (int i = 0; i < aosTokens.size(); ++i)
            {
                if (!osHeaders.empty())
                    osHeaders += "\r\n";
                if (!bFoundAcceptHeader)
                    bFoundAcceptHeader =
                        STARTS_WITH_CI(aosTokens[i], "Accept:");
                osHeaders += aosTokens[i];
            }
        }
    }
    if (!bFoundAcceptHeader)
    {
        if (!osHeaders.empty())
            osHeaders += "\r\n";
        osHeaders += "Accept: text/plain, application/json";
    }

    CPLStringList aosOptions;
    aosOptions.SetNameValue("HEADERS", osHeaders.c_str());
    CPLHTTPResult *pResult = CPLHTTPFetch(pszURL, aosOptions.List());

    if (nullptr == pResult || 0 == pResult->nDataLen ||
        0 != CPLGetLastErrorNo())
    {
        CPLHTTPDestroyResult(pResult);
        return nullptr;
    }

    if (0 != pResult->nStatus)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Curl reports error: %d: %s",
                 pResult->nStatus, pResult->pszErrBuf);
        CPLHTTPDestroyResult(pResult);
        return nullptr;
    }

    return pResult;
}
