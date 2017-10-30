/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of private utilities used within OGR GeoJSON Driver.
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include <cpl_port.h>
#include <cpl_conv.h>
#include <ogr_geometry.h>
#include <json.h> // JSON-C

CPL_CVSID("$Id$")

/************************************************************************/
/*                           IsJSONObject()                             */
/************************************************************************/

static bool IsJSONObject( const char* pszText )
{
    if( NULL == pszText )
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
    const char* pszIter = strstr(pszText, "\"type\"");
    if( pszIter == NULL )
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
    return STARTS_WITH(pszIter, osValue.c_str());
}

/************************************************************************/
/*                           GeoJSONIsObject()                          */
/************************************************************************/

bool GeoJSONIsObject( const char* pszText )
{
    if( !IsJSONObject(pszText) )
        return false;

    return IsTypeSomething(pszText, "Feature") ||
           IsTypeSomething(pszText, "FeatureCollection") ||
           IsTypeSomething(pszText, "Point") ||
           IsTypeSomething(pszText, "LineString") ||
           IsTypeSomething(pszText, "Polygon") ||
           IsTypeSomething(pszText, "MultiPoint") ||
           IsTypeSomething(pszText, "MultiLineString") ||
           IsTypeSomething(pszText, "MultiPolygon") ||
           IsTypeSomething(pszText, "GeometryCollection");
}


/************************************************************************/
/*                       ESRIJSONIsObject()                             */
/************************************************************************/

bool ESRIJSONIsObject(const char *pszText)
{
    if( !IsJSONObject(pszText) )
        return false;

    return  // ESRI Json geometry
            (strstr(pszText, "\"geometryType\"") != NULL &&
             strstr(pszText, "\"esriGeometry") != NULL)

            // ESRI Json "FeatureCollection"
            || strstr(pszText, "\"fieldAliases\"") != NULL

            // ESRI Json "FeatureCollection"
            || (strstr(pszText, "\"fields\"") != NULL &&
                strstr(pszText, "\"esriFieldType") != NULL);
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
/*                           GeoJSONFileIsObject()                      */
/************************************************************************/

static
bool GeoJSONFileIsObject( GDALOpenInfo* poOpenInfo )
{
    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.

    if( poOpenInfo->fpL == NULL ||
        !poOpenInfo->TryToIngest(6000) )
    {
        return false;
    }

    if( !GeoJSONIsObject((const char*)poOpenInfo->pabyHeader) )
    {
        return false;
    }

    return true;
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
            return eGeoJSONSourceUnknown;
        if( strstr(poOpenInfo->pszFilename, "f=json") )
            return eGeoJSONSourceUnknown;
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
        if( strstr(poOpenInfo->pszFilename, "f=json") != NULL )
            return eGeoJSONSourceService;
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

    if( poOpenInfo->fpL == NULL )
    {
        const char* pszText = poOpenInfo->pszFilename;
        if( ESRIJSONIsObject(pszText) )
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.
    if( poOpenInfo->fpL == NULL ||
        !poOpenInfo->TryToIngest(6000) )
    {
        return eGeoJSONSourceUnknown;
    }

    if( poOpenInfo->pabyHeader != NULL &&
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
        if( strstr(poOpenInfo->pszFilename, "f=json") != NULL )
            return eGeoJSONSourceUnknown;
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

    if( poOpenInfo->fpL == NULL )
    {
        const char* pszText = poOpenInfo->pszFilename;
        if( TopoJSONIsObject(pszText) )
            return eGeoJSONSourceText;
        return eGeoJSONSourceUnknown;
    }

    // By default read first 6000 bytes.
    // 6000 was chosen as enough bytes to
    // enable all current tests to pass.
    if( poOpenInfo->fpL == NULL ||
        !poOpenInfo->TryToIngest(6000) )
    {
        return eGeoJSONSourceUnknown;
    }

    if( poOpenInfo->pabyHeader != NULL &&
        TopoJSONIsObject(reinterpret_cast<const char*>(poOpenInfo->pabyHeader)) )
    {
        return eGeoJSONSourceFile;
    }
    return eGeoJSONSourceUnknown;
}
/************************************************************************/
/*                           GeoJSONPropertyToFieldType()               */
/************************************************************************/

static const GIntBig MY_INT64_MAX = (((GIntBig)0x7FFFFFFF) << 32) | 0xFFFFFFFF;
static const GIntBig MY_INT64_MIN = ((GIntBig)0x80000000) << 32;

OGRFieldType GeoJSONPropertyToFieldType( json_object* poObject,
                                         OGRFieldSubType& eSubType,
                                         bool bArrayAsString )
{
    eSubType = OFSTNone;

    if( poObject == NULL ) { return OFTString; }

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
        const int nSize = json_object_array_length(poObject);
        if( nSize == 0 )
            // We don't know, so let's assume it's a string list.
            return OFTStringList;
        OGRFieldType eType = OFTIntegerList;
        bool bOnlyBoolean = true;
        for( int i = 0; i < nSize; i++ )
        {
            json_object* poRow = json_object_array_get_idx(poObject, i);
            if( poRow != NULL )
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
    if( poObject == NULL ) { return OFTString; }
    const char* pszStr = json_object_get_string( poObject );

    OGRField sWrkField;
    CPLPushErrorHandler(CPLQuietErrorHandler);
    const bool bSuccess = CPL_TO_BOOL(OGRParseDate( pszStr, &sWrkField, 0 ));
    CPLPopErrorHandler();
    CPLErrorReset();
    if( bSuccess )
    {
        const bool bHasDate =
            strchr( pszStr, '/' ) != NULL ||
            strchr( pszStr, '-' ) != NULL;
        const bool bHasTime = strchr( pszStr, ':' ) != NULL;
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
    CPLAssert( NULL != poGeometry );

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
