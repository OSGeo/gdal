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

#include "ogrgeojsonutils.h"
#include <assert.h>
#include "cpl_port.h"
#include "cpl_conv.h"
#include "ogr_geometry.h"
#include <json.h> // JSON-C

#include <algorithm>
#include <memory>

CPL_CVSID("$Id$")

const char szESRIJSonPotentialStart1[] =
    "{\"features\":[{\"geometry\":{\"rings\":[";

/************************************************************************/
/*                           IsJSONObject()                             */
/************************************************************************/

static bool IsJSONObject( const char* pszText )
{
    if( nullptr == pszText )
        return false;

    /* Skip UTF-8 BOM (#5630) */
    const GByte* pabyData = reinterpret_cast<const GByte *>(pszText);
    if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
        pszText += 3;

/* -------------------------------------------------------------------- */
/*      This is a primitive test, but we need to perform it fast.       */
/* -------------------------------------------------------------------- */
    while( *pszText != '\0' && isspace( (unsigned char)*pszText ) )
        pszText++;

    const char* const apszPrefix[] = { "loadGeoJSON(", "jsonp(" };
    for( size_t iP = 0; iP < sizeof(apszPrefix) / sizeof(apszPrefix[0]); iP++ )
    {
        if( strncmp(pszText, apszPrefix[iP], strlen(apszPrefix[iP])) == 0 )
        {
            pszText += strlen(apszPrefix[iP]);
            break;
        }
    }

    if( *pszText != '{' )
        return false;

    return true;
}

/************************************************************************/
/*                           IsTypeSomething()                          */
/************************************************************************/

static bool IsTypeSomething( const char* pszText, const char* pszTypeValue )
{
    const char* pszIter = pszText;
    while( true )
    {
        pszIter = strstr(pszIter, "\"type\"");
        if( pszIter == nullptr )
            return false;
        pszIter += strlen("\"type\"");
        while( isspace(*pszIter) )
            pszIter ++;
        if( *pszIter != ':' )
            return false;
        pszIter ++;
        while( isspace(*pszIter) )
            pszIter ++;
        CPLString osValue;
        osValue.Printf("\"%s\"", pszTypeValue);
        if( STARTS_WITH(pszIter, osValue.c_str()) )
            return true;
    }
}

/************************************************************************/
/*                           GetCompactJSon()                           */
/************************************************************************/

static CPLString GetCompactJSon( const char* pszText, size_t nMaxSize )
{
    /* Skip UTF-8 BOM (#5630) */
    const GByte* pabyData = reinterpret_cast<const GByte *>(pszText);
    if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
        pszText += 3;

    CPLString osWithoutSpace;
    bool bInString = false;
    for( int i = 0; pszText[i] != '\0' &&
                    osWithoutSpace.size() < nMaxSize; i++ )
    {
        if( bInString )
        {
            if( pszText[i] == '\\' )
            {
                osWithoutSpace += pszText[i];
                if( pszText[i+1] == '\0' )
                    break;
                osWithoutSpace += pszText[i+1];
                i ++;
            }
            else if( pszText[i] == '"' )
            {
                bInString = false;
                osWithoutSpace += '"';
            }
            else
            {
                osWithoutSpace += pszText[i];
            }
        }
        else if( pszText[i] == '"' )
        {
            bInString = true;
            osWithoutSpace += '"';
        }
        else if( !isspace(static_cast<int>(pszText[i])) )
        {
            osWithoutSpace += pszText[i];
        }
    }
    return osWithoutSpace;
}

/************************************************************************/
/*                          IsGeoJSONLikeObject()                       */
/************************************************************************/

static bool IsGeoJSONLikeObject( const char* pszText, bool& bMightBeSequence,
                                 bool& bReadMoreBytes)
{
    bMightBeSequence = false;
    bReadMoreBytes = false;

    if( !IsJSONObject(pszText) )
        return false;

    if( IsTypeSomething(pszText, "Topology") )
        return false;

    if( IsTypeSomething(pszText, "FeatureCollection") )
    {
        return true;
    }

    CPLString osWithoutSpace = GetCompactJSon(pszText, strlen(pszText));
    if( osWithoutSpace.find("{\"features\":[") == 0 &&
        osWithoutSpace.find(szESRIJSonPotentialStart1) != 0 )
    {
        return true;
    }

    // See https://raw.githubusercontent.com/icepack/icepack-data/master/meshes/larsen/larsen_inflow.geojson
    // "{"crs":...,"features":[..."
    // or https://gist.githubusercontent.com/NiklasDallmann/27e339dd78d1942d524fbcd179f9fdcf/raw/527a8319d75a9e29446a32a19e4c902213b0d668/42XR9nLAh8Poh9Xmniqh3Bs9iisNm74mYMC56v3Wfyo=_isochrones_fails.geojson
    // "{"bbox":...,"features":[..."
    if( osWithoutSpace.find(",\"features\":[") != std::string::npos )
    {
        return !ESRIJSONIsObject(pszText);
    }

    // See https://github.com/OSGeo/gdal/issues/2720
    if( osWithoutSpace.find("{\"coordinates\":[") == 0 ||
        // and https://github.com/OSGeo/gdal/issues/2787
        osWithoutSpace.find("{\"geometry\":{\"coordinates\":[") == 0 )
    {
        return true;
    }

    if( IsTypeSomething(pszText, "Feature") ||
           IsTypeSomething(pszText, "Point") ||
           IsTypeSomething(pszText, "LineString") ||
           IsTypeSomething(pszText, "Polygon") ||
           IsTypeSomething(pszText, "MultiPoint") ||
           IsTypeSomething(pszText, "MultiLineString") ||
           IsTypeSomething(pszText, "MultiPolygon") ||
           IsTypeSomething(pszText, "GeometryCollection") )
    {
        bMightBeSequence = true;
        return true;
    }

    // See https://github.com/OSGeo/gdal/issues/3280
    if( osWithoutSpace.find("{\"properties\":{") == 0 )
    {
        bMightBeSequence = true;
        bReadMoreBytes = true;
        return false;
    }

    return false;
}

static bool IsGeoJSONLikeObject( const char* pszText )
{
    bool bMightBeSequence;
    bool bReadMoreBytes;
    return IsGeoJSONLikeObject(pszText, bMightBeSequence, bReadMoreBytes);
}

/************************************************************************/
/*                       ESRIJSONIsObject()                             */
/************************************************************************/

bool ESRIJSONIsObject(const char *pszText)
{
    if( !IsJSONObject(pszText) )
        return false;

    if(  // ESRI Json geometry
            (strstr(pszText, "\"geometryType\"") != nullptr &&
             strstr(pszText, "\"esriGeometry") != nullptr)

            // ESRI Json "FeatureCollection"
            || strstr(pszText, "\"fieldAliases\"") != nullptr

            // ESRI Json "FeatureCollection"
            || (strstr(pszText, "\"fields\"") != nullptr &&
                strstr(pszText, "\"esriFieldType") != nullptr) )
    {
        return true;
    }

    CPLString osWithoutSpace = GetCompactJSon(pszText,
                                            strlen(szESRIJSonPotentialStart1));
    if( osWithoutSpace.find(szESRIJSonPotentialStart1) == 0 )
    {
        return true;
    }

    return false;
}


/************************************************************************/
/*                       TopoJSONIsObject()                             */
/************************************************************************/

bool TopoJSONIsObject(const char *pszText)
{
    if( !IsJSONObject(pszText) )
        return false;

    return IsTypeSomething(pszText, "Topology");
}

/************************************************************************/
/*                      IsLikelyNewlineSequenceGeoJSON()                */
/************************************************************************/

static bool IsLikelyNewlineSequenceGeoJSON( VSILFILE* fpL,
                                            const GByte* pabyHeader,
                                            const char* pszFileContent )
{
    const size_t nBufferSize = 4096 * 10;
    std::vector<GByte> abyBuffer;
    abyBuffer.resize(nBufferSize+1);

    int nCurlLevel = 0;
    bool bInString = false;
    bool bLastIsEscape = false;
    bool bCompatibleOfSequence = true;
    bool bFirstIter = true;
    bool bEOLFound = false;
    int nCountObject = 0;
    while( true )
    {
        size_t nRead;
        bool bEnd = false;
        if( bFirstIter )
        {
            const char* pszText = pszFileContent ? pszFileContent:
                reinterpret_cast<const char*>(pabyHeader);
            assert(pszText);
            nRead = std::min(strlen(pszText), nBufferSize);
            memcpy(abyBuffer.data(), pszText, nRead);
            bFirstIter = false;
            if( fpL )
            {
                VSIFSeekL(fpL, nRead, SEEK_SET);
            }
        }
        else
        {
            nRead = VSIFReadL(abyBuffer.data() , 1, nBufferSize, fpL);
            bEnd = nRead < nBufferSize;
        }
        for( size_t i = 0; i < nRead; i++ )
        {
            if( nCurlLevel == 0 )
            {
                if( abyBuffer[i] == '{' )
                {
                    nCountObject ++;
                    if( nCountObject == 2 )
                    {
                        break;
                    }
                    nCurlLevel ++;
                }
                else if( nCountObject == 1 && abyBuffer[i] == '\n' )
                {
                    bEOLFound = true;
                }
                else if( !isspace( static_cast<int>(abyBuffer[i]) ) )
                {
                    bCompatibleOfSequence = false;
                    break;
                }
            }
            else if( bInString )
            {
                if( bLastIsEscape )
                {
                    bLastIsEscape = false;
                }
                else if( abyBuffer[i] == '\\' )
                {
                    bLastIsEscape = true;
                }
                else if( abyBuffer[i] == '"' )
                {
                    bInString = false;
                }
            }
            else if( abyBuffer[i] == '"' )
            {
                bInString = true;
            }
            else if( abyBuffer[i] == '{' )
            {
                nCurlLevel ++;
            }
            else if( abyBuffer[i] == '}' )
            {
                nCurlLevel --;
            }
        }
        if( !fpL || bEnd || !bCompatibleOfSequence || nCountObject == 2 )
            break;
    }
    return bCompatibleOfSequence && bEOLFound && nCountObject == 2;
}

/************************************************************************/
/*                           GeoJSONFileIsObject()                      */
/************************************************************************/

static
bool GeoJSONFileIsObject( GDALOpenInfo* poOpenInfo )
{
    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.

    if( poOpenInfo->fpL == nullptr ||
        !poOpenInfo->TryToIngest(6000) )
    {
        return false;
    }

    bool bMightBeSequence = false;
    bool bReadMoreBytes = false;
    if( !IsGeoJSONLikeObject(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                             bMightBeSequence, bReadMoreBytes) )
    {
        if( !(bReadMoreBytes && poOpenInfo->nHeaderBytes >= 6000 &&
              poOpenInfo->TryToIngest(1000 * 1000) &&
             !IsGeoJSONLikeObject(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                                  bMightBeSequence, bReadMoreBytes)) )
        {
            return false;
        }
    }

    return !(bMightBeSequence && IsLikelyNewlineSequenceGeoJSON(
        poOpenInfo->fpL, poOpenInfo->pabyHeader, nullptr));
}

/************************************************************************/
/*                           GeoJSONIsObject()                          */
/************************************************************************/

bool GeoJSONIsObject( const char* pszText )
{
    bool bMightBeSequence = false;
    bool bReadMoreBytes = false;
    if( !IsGeoJSONLikeObject(pszText, bMightBeSequence, bReadMoreBytes) )
    {
        return false;
    }

    return !(bMightBeSequence &&
             IsLikelyNewlineSequenceGeoJSON(nullptr, nullptr, pszText));
}

/************************************************************************/
/*                        GeoJSONSeqFileIsObject()                      */
/************************************************************************/

static
bool GeoJSONSeqFileIsObject( GDALOpenInfo* poOpenInfo )
{
    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.

    if( poOpenInfo->fpL == nullptr ||
        !poOpenInfo->TryToIngest(6000) )
    {
        return false;
    }

    const char* pszText = reinterpret_cast<const char*>(poOpenInfo->pabyHeader);
    if( pszText[0] == '\x1e' )
        return IsGeoJSONLikeObject(pszText+1);

    bool bMightBeSequence = false;
    bool bReadMoreBytes = false;
    if( !IsGeoJSONLikeObject(pszText, bMightBeSequence, bReadMoreBytes) )
    {
        if( !(bReadMoreBytes && poOpenInfo->nHeaderBytes >= 6000 &&
              poOpenInfo->TryToIngest(1000 * 1000) &&
             !IsGeoJSONLikeObject(reinterpret_cast<const char*>(poOpenInfo->pabyHeader),
                                  bMightBeSequence, bReadMoreBytes)) )
        {
            return false;
        }
    }

    return bMightBeSequence && IsLikelyNewlineSequenceGeoJSON(
        poOpenInfo->fpL, poOpenInfo->pabyHeader, nullptr);
}

bool GeoJSONSeqIsObject( const char* pszText )
{
    if( pszText[0] == '\x1e' )
        return IsGeoJSONLikeObject(pszText+1);

    bool bMightBeSequence = false;
    bool bReadMoreBytes = false;
    if( !IsGeoJSONLikeObject(pszText, bMightBeSequence, bReadMoreBytes) )
    {
        return false;
    }

    return bMightBeSequence && IsLikelyNewlineSequenceGeoJSON(
        nullptr, nullptr, pszText);
}

/************************************************************************/
/*                           IsLikelyESRIJSONURL()                      */
/************************************************************************/

static bool IsLikelyESRIJSONURL(const char* pszURL)
{
    // URLs with f=json are strong candidates for ESRI JSON services
    // except if they have "/items?", in which case they are likely OAPIF
    return strstr(pszURL, "f=json") != nullptr &&
          strstr(pszURL, "/items?") == nullptr;
}

/************************************************************************/
/*                           GeoJSONGetSourceType()                     */
/************************************************************************/

GeoJSONSourceType GeoJSONGetSourceType( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType srcType = eGeoJSONSourceUnknown;

    // NOTE: Sometimes URL ends with .geojson token, for example
    //       http://example/path/2232.geojson
    //       It's important to test beginning of source first.
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSON:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSON:https://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSON:ftp://") )
    {
        srcType = eGeoJSONSourceService;
    }
    else if( STARTS_WITH_CI(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "ftp://") )
    {
        if( (strstr(poOpenInfo->pszFilename, "SERVICE=WFS") ||
             strstr(poOpenInfo->pszFilename, "service=WFS") ||
             strstr(poOpenInfo->pszFilename, "service=wfs")) &&
             !strstr(poOpenInfo->pszFilename, "json") )
        {
            return eGeoJSONSourceUnknown;
        }
        if( IsLikelyESRIJSONURL(poOpenInfo->pszFilename) )
        {
            return eGeoJSONSourceUnknown;
        }
        srcType = eGeoJSONSourceService;
    }
    else if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GeoJSON:") )
    {
        VSIStatBufL sStat;
        if( VSIStatL(poOpenInfo->pszFilename + strlen("GeoJSON:"), &sStat) == 0 )
        {
            return eGeoJSONSourceFile;
        }
        const char* pszText = poOpenInfo->pszFilename + strlen("GeoJSON:");
        if( GeoJSONIsObject(pszText) )
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }
    else if( GeoJSONIsObject( poOpenInfo->pszFilename ) )
    {
        srcType = eGeoJSONSourceText;
    }
    else if( GeoJSONFileIsObject( poOpenInfo ) )
    {
        srcType = eGeoJSONSourceFile;
    }

    return srcType;
}

/************************************************************************/
/*                     ESRIJSONDriverGetSourceType()                    */
/************************************************************************/

GeoJSONSourceType ESRIJSONDriverGetSourceType( GDALOpenInfo* poOpenInfo )
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:https://")||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:ftp://") )
    {
        return eGeoJSONSourceService;
    }
    else if( STARTS_WITH(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH(poOpenInfo->pszFilename, "ftp://") )
    {
        if( IsLikelyESRIJSONURL(poOpenInfo->pszFilename) )
        {
            return eGeoJSONSourceService;
        }
        return eGeoJSONSourceUnknown;
    }

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "ESRIJSON:") )
    {
        VSIStatBufL sStat;
        if( VSIStatL(poOpenInfo->pszFilename + strlen("ESRIJSON:"), &sStat) == 0 )
        {
            return eGeoJSONSourceFile;
        }
        const char* pszText = poOpenInfo->pszFilename + strlen("ESRIJSON:");
        if( ESRIJSONIsObject(pszText) )
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    if( poOpenInfo->fpL == nullptr )
    {
        const char* pszText = poOpenInfo->pszFilename;
        if( ESRIJSONIsObject(pszText) )
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.
    if( !poOpenInfo->TryToIngest(6000) )
    {
        return eGeoJSONSourceUnknown;
    }

    if( poOpenInfo->pabyHeader != nullptr &&
        ESRIJSONIsObject(reinterpret_cast<const char*>(poOpenInfo->pabyHeader)) )
    {
        return eGeoJSONSourceFile;
    }
    return eGeoJSONSourceUnknown;
}

/************************************************************************/
/*                     TopoJSONDriverGetSourceType()                    */
/************************************************************************/

GeoJSONSourceType TopoJSONDriverGetSourceType( GDALOpenInfo* poOpenInfo )
{
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:https://")||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:ftp://") )
    {
        return eGeoJSONSourceService;
    }
    else if( STARTS_WITH(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH(poOpenInfo->pszFilename, "ftp://") )
    {
        if( IsLikelyESRIJSONURL(poOpenInfo->pszFilename) )
        {
            return eGeoJSONSourceUnknown;
        }
        return eGeoJSONSourceService;
    }

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "TopoJSON:") )
    {
        VSIStatBufL sStat;
        if( VSIStatL(poOpenInfo->pszFilename + strlen("TopoJSON:"), &sStat) == 0 )
        {
            return eGeoJSONSourceFile;
        }
        const char* pszText = poOpenInfo->pszFilename + strlen("TopoJSON:");
        if( TopoJSONIsObject(pszText) )
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    if( poOpenInfo->fpL == nullptr )
    {
        const char* pszText = poOpenInfo->pszFilename;
        if( TopoJSONIsObject(pszText) )
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.
    if( !poOpenInfo->TryToIngest(6000) )
    {
        return eGeoJSONSourceUnknown;
    }

    if( poOpenInfo->pabyHeader != nullptr &&
        TopoJSONIsObject(reinterpret_cast<const char*>(poOpenInfo->pabyHeader)) )
    {
        return eGeoJSONSourceFile;
    }
    return eGeoJSONSourceUnknown;
}

/************************************************************************/
/*                          GeoJSONSeqGetSourceType()                   */
/************************************************************************/

GeoJSONSourceType GeoJSONSeqGetSourceType( GDALOpenInfo* poOpenInfo )
{
    GeoJSONSourceType srcType = eGeoJSONSourceUnknown;

    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSONSeq:http://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSONSeq:https://") ||
        STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSONSeq:ftp://") )
    {
        srcType = eGeoJSONSourceService;
    }
    else if( STARTS_WITH_CI(poOpenInfo->pszFilename, "http://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "https://") ||
             STARTS_WITH_CI(poOpenInfo->pszFilename, "ftp://") )
    {
        if( IsLikelyESRIJSONURL(poOpenInfo->pszFilename) )
        {
            return eGeoJSONSourceUnknown;
        }
        srcType = eGeoJSONSourceService;
    }
    else if( STARTS_WITH_CI(poOpenInfo->pszFilename, "GEOJSONSeq:") )
    {
        VSIStatBufL sStat;
        if( VSIStatL(poOpenInfo->pszFilename + strlen("GEOJSONSeq:"), &sStat) == 0 )
        {
            return eGeoJSONSourceFile;
        }
        const char* pszText = poOpenInfo->pszFilename + strlen("GEOJSONSeq:");
        if( GeoJSONSeqIsObject(pszText) )
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }
    else if( GeoJSONSeqIsObject( poOpenInfo->pszFilename ) )
    {
        srcType = eGeoJSONSourceText;
    }
    else if( GeoJSONSeqFileIsObject( poOpenInfo ) )
    {
        srcType = eGeoJSONSourceFile;
    }

    return srcType;
}

/************************************************************************/
/*                           GeoJSONPropertyToFieldType()               */
/************************************************************************/

constexpr GIntBig MY_INT64_MAX = (((GIntBig)0x7FFFFFFF) << 32) | 0xFFFFFFFF;
constexpr GIntBig MY_INT64_MIN = ((GIntBig)0x80000000) << 32;

OGRFieldType GeoJSONPropertyToFieldType( json_object* poObject,
                                         OGRFieldSubType& eSubType,
                                         bool bArrayAsString )
{
    eSubType = OFSTNone;

    if( poObject == nullptr ) { return OFTString; }

    json_type type = json_object_get_type( poObject );

    if( json_type_boolean == type )
    {
        eSubType = OFSTBoolean;
        return OFTInteger;
    }
    else if( json_type_double == type )
        return OFTReal;
    else if( json_type_int == type )
    {
        GIntBig nVal = json_object_get_int64(poObject);
        if( !CPL_INT64_FITS_ON_INT32(nVal) )
        {
            if( nVal == MY_INT64_MIN || nVal == MY_INT64_MAX )
            {
                static bool bWarned = false;
                if( !bWarned )
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
    else if( json_type_string == type )
        return OFTString;
    else if( json_type_array == type )
    {
        if( bArrayAsString )
            return OFTString;
        const auto nSize = json_object_array_length(poObject);
        if( nSize == 0 )
            // We don't know, so let's assume it is a string list.
            return OFTStringList;
        OGRFieldType eType = OFTIntegerList;
        bool bOnlyBoolean = true;
        for( auto i = decltype(nSize){0}; i < nSize; i++ )
        {
            json_object* poRow = json_object_array_get_idx(poObject, i);
            if( poRow != nullptr )
            {
                type = json_object_get_type( poRow );
                bOnlyBoolean &= type == json_type_boolean;
                if( type == json_type_string )
                    return OFTStringList;
                else if( type == json_type_double )
                    eType = OFTRealList;
                else if( eType == OFTIntegerList &&
                         type == json_type_int )
                {
                    GIntBig nVal = json_object_get_int64(poRow);
                    if( !CPL_INT64_FITS_ON_INT32(nVal) )
                        eType = OFTInteger64List;
                }
                else if( type != json_type_int &&
                         type != json_type_boolean )
                    return OFTString;
            }
        }
        if( bOnlyBoolean )
            eSubType = OFSTBoolean;

        return eType;
    }

    return OFTString; // null, object
}

/************************************************************************/
/*                        GeoJSONStringPropertyToFieldType()            */
/************************************************************************/

OGRFieldType GeoJSONStringPropertyToFieldType( json_object* poObject )
{
    if( poObject == nullptr ) { return OFTString; }
    const char* pszStr = json_object_get_string( poObject );

    OGRField sWrkField;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    const bool bSuccess = CPL_TO_BOOL(OGRParseDate( pszStr, &sWrkField, 0 ));
    CPLPopErrorHandler();
    CPLErrorReset();
    if( bSuccess )
    {
        const bool bHasDate =
            strchr( pszStr, '/' ) != nullptr ||
            strchr( pszStr, '-' ) != nullptr;
        const bool bHasTime = strchr( pszStr, ':' ) != nullptr;
        if( bHasDate && bHasTime )
            return OFTDateTime;
        else if( bHasDate )
            return OFTDate;
        else
            return OFTTime;
        // TODO: What if both are false?
    }
    return OFTString;
}

/************************************************************************/
/*                           OGRGeoJSONGetGeometryName()                */
/************************************************************************/

const char* OGRGeoJSONGetGeometryName( OGRGeometry const* poGeometry )
{
    CPLAssert( nullptr != poGeometry );

    const OGRwkbGeometryType eType = wkbFlatten(poGeometry->getGeometryType());

    if( wkbPoint == eType )
        return "Point";
    else if( wkbLineString == eType )
        return "LineString";
    else if( wkbPolygon == eType )
        return "Polygon";
    else if( wkbMultiPoint == eType )
        return "MultiPoint";
    else if( wkbMultiLineString == eType )
        return "MultiLineString";
    else if( wkbMultiPolygon == eType )
        return "MultiPolygon";
    else if( wkbGeometryCollection == eType )
        return "GeometryCollection";

    return "Unknown";
}
