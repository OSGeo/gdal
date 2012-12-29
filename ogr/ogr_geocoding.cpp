/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Client of geocoding service.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault
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

#include "cpl_conv.h"
#include "cpl_http.h"
#include "cpl_multiproc.h"
#include "cpl_minixml.h"

/* Emulation of gettimeofday() for Windows */
#ifdef WIN32

#include <time.h>
#include <windows.h>

struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of DST correction */
};

#define MICROSEC_IN_SEC   1000000

static
int OGR_gettimeofday(struct timeval *tv, struct timezone *tzIgnored)
{
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);

    /* In 100-nanosecond intervals since January 1, 1601 (UTC). */
    GUIntBig nVal = (((GUIntBig)ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    nVal /= 10; /* to microseconds */
    /* There are 11 644 473 600 seconds between 1601 and 1970 */
    nVal -= ((GUIntBig)116444736) * 100 * MICROSEC_IN_SEC;
    tv->tv_sec = (long)(nVal / MICROSEC_IN_SEC);
    tv->tv_usec = (long)(nVal % MICROSEC_IN_SEC);

    return 0;
}

#define gettimeofday OGR_gettimeofday

#else
#include <sys/time.h>
#endif


#include "ogr_geocoding.h"
#include "ogr_mem.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$");

struct _OGRGeocodingSessionHS
{
    char*  pszCacheFilename;
    char*  pszGeocodingService;
    char*  pszEmail;
    char*  pszApplication;
    char*  pszLanguage;
    char*  pszQueryTemplate;
    char*  pszReverseQueryTemplate;
    int    bReadCache;
    int    bWriteCache;
    double dfDelayBetweenQueries;
    OGRDataSource* poDS;
};

static void* hMutex = NULL;
static double dfLastQueryTimeStampOSMNominatim = 0.0;
static double dfLastQueryTimeStampMapQuestNominatim = 0.0;

#define OSM_NOMINATIM_QUERY      "http://nominatim.openstreetmap.org/search?q=%s&format=xml&polygon_text=1"
#define MAPQUEST_NOMINATIM_QUERY "http://open.mapquestapi.com/nominatim/v1/search.php?q=%s&format=xml"
#define YAHOO_QUERY              "http://where.yahooapis.com/geocode?q=%s"

#define OSM_NOMINATIM_REVERSE_QUERY      "http://nominatim.openstreetmap.org/reverse?format=xml&lat={lat}&lon={lon}"
#define MAPQUEST_NOMINATIM_REVERSE_QUERY "http://open.mapquestapi.com/nominatim/v1/reverse.php?format=xml&lat={lat}&lon={lon}"
#define YAHOO_REVERSE_QUERY              "http://where.yahooapis.com/geocode?q={lat},{lon}&gflags=R"

#define CACHE_LAYER_NAME         "ogr_geocode_cache"
#define DEFAULT_CACHE_SQLITE     "ogr_geocode_cache.sqlite"
#define DEFAULT_CACHE_CSV        "ogr_geocode_cache.csv"

#define FIELD_URL                "url"
#define FIELD_BLOB               "blob"

/************************************************************************/
/*                       OGRGeocodeGetParameter()                       */
/************************************************************************/

static
const char* OGRGeocodeGetParameter(char** papszOptions, const char* pszKey,
                                   const char* pszDefaultValue)
{
    const char* pszRet = CSLFetchNameValue(papszOptions, pszKey);
    if( pszRet != NULL )
        return pszRet;

    return CPLGetConfigOption(CPLSPrintf("OGR_GEOCODE_%s", pszKey),
                              pszDefaultValue);
}

/************************************************************************/
/*                      OGRGeocodeHasStringValidFormat()                */
/************************************************************************/

/* Checks that pszQueryTemplate has one and only one occurence of %s in it. */
static
int OGRGeocodeHasStringValidFormat(const char* pszQueryTemplate)
{
    const char* pszIter = pszQueryTemplate;
    int bValidFormat = TRUE;
    int bFoundPctS = FALSE;
    while( *pszIter != '\0' )
    {
        if( *pszIter == '%' )
        {
            if( pszIter[1] == '%' )
            {
                pszIter ++;
            }
            else if( pszIter[1] == 's' )
            {
                if( bFoundPctS )
                {
                    bValidFormat = FALSE;
                    break;
                }
                bFoundPctS = TRUE;
            }
            else
            {
                bValidFormat = FALSE;
                break;
            }
        }
        pszIter ++;
    }
    if( !bFoundPctS )
        bValidFormat = FALSE;
    return bValidFormat;
}

/************************************************************************/
/*                       OGRGeocodeCreateSession()                      */
/************************************************************************/

/**
 * \brief Creates a session handle for geocoding requests.
 *
 * Available papszOptions values:
 * <ul>
 * <li> "CACHE_FILE" : Defaults to "ogr_geocode_cache.sqlite" (or otherwise
 *                    "ogr_geocode_cache.csv" if the SQLite driver isn't
 *                    available). Might be any CSV, SQLite or PostgreSQL
 *                    datasource.
 * <li> "READ_CACHE" : "TRUE" (default) or "FALSE"
 * <li> "WRITE_CACHE" : "TRUE" (default) or "FALSE"
 * <li> "SERVICE": <a href="http://wiki.openstreetmap.org/wiki/Nominatim">"OSM_NOMINATIM"</a>
 *      (default), <a href="http://open.mapquestapi.com/nominatim/">"MAPQUEST_NOMINATIM"</a>,
 *      <a href="http://developer.yahoo.com/geo/placefinder/">"YAHOO"</a> or
 *       other value
 * <li> "EMAIL": used by OSM_NOMINATIM. Optional, but recommanded.
 * <li> "APPLICATION": used to set the User-Agent MIME header. Defaults
 *       to GDAL/OGR version string.
 * <li> "LANGUAGE": used to set the Accept-Language MIME header. Preferred
 *      language order for showing search results.
 * <li> "DELAY": minimum delay, in second, between 2 consecutive queries.
 *       Defaults to 1.0.
 * <li> "QUERY_TEMPLATE": URL template for GET requests. Must contain one
 *       and only one occurence of %%s in it. If not specified, for
 *       SERVICE=OSM_NOMINATIM, MAPQUEST_NOMINATIM or YAHOO, the URL template
 *       is hard-coded.
 * <li> "REVERSE_QUERY_TEMPLATE": URL template for GET requests for reverse
 *       geocoding. Must contain one and only one occurence of {lon} and {lat} in it.
 *       If not specified, for SERVICE=OSM_NOMINATIM, MAPQUEST_NOMINATIM or YAHOO,
 *       the URL template is hard-coded.
 * </ul>
 *
 * All the above options can also be set by defining the configuration option
 * of the same name, prefixed by OGR_GEOCODE_. For example "OGR_GEOCODE_SERVICE"
 * for the "SERVICE" option.
 *
 * @param papszOptions NULL, or a NULL-terminated list of string options.
 *
 * @return an handle that should be freed with OGRGeocodeDestroySession(), or NULL
 *         in case of failure.
 *
 * @since GDAL 1.10
 */

OGRGeocodingSessionH OGRGeocodeCreateSession(char** papszOptions)
{
    OGRGeocodingSessionH hSession =
        (OGRGeocodingSessionH)CPLCalloc(1, sizeof(_OGRGeocodingSessionHS));

    const char* pszCacheFilename = OGRGeocodeGetParameter(papszOptions,
                                                          "CACHE_FILE",
                                                          DEFAULT_CACHE_SQLITE);
    CPLString osExt = CPLGetExtension(pszCacheFilename);
    if( !(EQUALN(pszCacheFilename, "PG:", 3) ||
        EQUAL(osExt, "csv") || EQUAL(osExt, "sqlite")) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Only .csv, .sqlite or PG: datasources are handled for now.");
        OGRGeocodeDestroySession(hSession);
        return NULL;
    }
    hSession->pszCacheFilename = CPLStrdup(pszCacheFilename);

    hSession->bReadCache = CSLTestBoolean(
        OGRGeocodeGetParameter(papszOptions, "READ_CACHE", "TRUE"));
    hSession->bWriteCache = CSLTestBoolean(
        OGRGeocodeGetParameter(papszOptions, "WRITE_CACHE", "TRUE"));

    const char* pszGeocodingService = OGRGeocodeGetParameter(papszOptions,
                                                             "SERVICE",
                                                             "OSM_NOMINATIM");
    hSession->pszGeocodingService = CPLStrdup(pszGeocodingService);

    const char* pszEmail = OGRGeocodeGetParameter(papszOptions, "EMAIL", NULL);
    hSession->pszEmail = pszEmail ? CPLStrdup(pszEmail) : NULL;

    const char* pszApplication = OGRGeocodeGetParameter(papszOptions,
                                                        "APPLICATION",
                                                        GDALVersionInfo(""));
    hSession->pszApplication = CPLStrdup(pszApplication);

    const char* pszLanguage = OGRGeocodeGetParameter(papszOptions,
                                                     "LANGUAGE",
                                                     NULL);
    hSession->pszLanguage = pszLanguage ? CPLStrdup(pszLanguage) : NULL;

    const char* pszDelayBetweenQueries = OGRGeocodeGetParameter(papszOptions,
                                                                "DELAY", "1.0");
    hSession->dfDelayBetweenQueries = CPLAtofM(pszDelayBetweenQueries);

    const char* pszQueryTemplateDefault = NULL;
    if( EQUAL(pszGeocodingService, "OSM_NOMINATIM") )
        pszQueryTemplateDefault = OSM_NOMINATIM_QUERY;
    else if( EQUAL(pszGeocodingService, "MAPQUEST_NOMINATIM") )
        pszQueryTemplateDefault = MAPQUEST_NOMINATIM_QUERY;
    else if( EQUAL(pszGeocodingService, "YAHOO") )
        pszQueryTemplateDefault = YAHOO_QUERY;
    const char* pszQueryTemplate = OGRGeocodeGetParameter(papszOptions,
                                                          "QUERY_TEMPLATE",
                                                          pszQueryTemplateDefault);
    if( pszQueryTemplate == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "QUERY_TEMPLATE parameter not defined");
        OGRGeocodeDestroySession(hSession);
        return NULL;
    }

    if( !OGRGeocodeHasStringValidFormat(pszQueryTemplate) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "QUERY_TEMPLATE value has an invalid format");
        OGRGeocodeDestroySession(hSession);
        return NULL;
    }

    hSession->pszQueryTemplate = CPLStrdup(pszQueryTemplate);

    const char* pszReverseQueryTemplateDefault = NULL;
    if( EQUAL(pszGeocodingService, "OSM_NOMINATIM") )
        pszReverseQueryTemplateDefault = OSM_NOMINATIM_REVERSE_QUERY;
    else if( EQUAL(pszGeocodingService, "MAPQUEST_NOMINATIM") )
        pszReverseQueryTemplateDefault = MAPQUEST_NOMINATIM_REVERSE_QUERY;
    else if( EQUAL(pszGeocodingService, "YAHOO") )
        pszReverseQueryTemplateDefault = YAHOO_REVERSE_QUERY;
    const char* pszReverseQueryTemplate = OGRGeocodeGetParameter(papszOptions,
                                                          "REVERSE_QUERY_TEMPLATE",
                                                          pszReverseQueryTemplateDefault);
    if( pszReverseQueryTemplate == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "REVERSE_QUERY_TEMPLATE parameter not defined");
        OGRGeocodeDestroySession(hSession);
        return NULL;
    }

    hSession->pszReverseQueryTemplate = CPLStrdup(pszReverseQueryTemplate);

    return hSession;
}

/************************************************************************/
/*                       OGRGeocodeDestroySession()                     */
/************************************************************************/

/**
 * \brief Destroys a session handle for geocoding requests.

 * @param hSession the handle to destroy.
 *
 * @since GDAL 1.10
 */
void OGRGeocodeDestroySession(OGRGeocodingSessionH hSession)
{
    if( hSession == NULL )
        return;
    CPLFree(hSession->pszCacheFilename);
    CPLFree(hSession->pszGeocodingService);
    CPLFree(hSession->pszEmail);
    CPLFree(hSession->pszApplication);
    CPLFree(hSession->pszLanguage);
    CPLFree(hSession->pszQueryTemplate);
    CPLFree(hSession->pszReverseQueryTemplate);
    if( hSession->poDS )
        OGRReleaseDataSource((OGRDataSourceH) hSession->poDS);
    CPLFree(hSession);
}

/************************************************************************/
/*                        OGRGeocodeGetCacheLayer()                     */
/************************************************************************/

static OGRLayer* OGRGeocodeGetCacheLayer(OGRGeocodingSessionH hSession,
                                         int bCreateIfNecessary,
                                         int* pnIdxBlob)
{
    OGRDataSource* poDS = hSession->poDS;
    CPLString osExt = CPLGetExtension(hSession->pszCacheFilename);

    if( poDS == NULL )
    {
        if( OGRGetDriverCount() == 0 )
            OGRRegisterAll();

        char* pszOldVal = CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", NULL) ?
            CPLStrdup(CPLGetConfigOption("OGR_SQLITE_SYNCHRONOUS", NULL)) : NULL;
        CPLSetThreadLocalConfigOption("OGR_SQLITE_SYNCHRONOUS", "OFF");

        poDS = (OGRDataSource*) OGROpen(hSession->pszCacheFilename, TRUE, NULL);
        if( poDS == NULL &&
            EQUAL(hSession->pszCacheFilename, DEFAULT_CACHE_SQLITE) )
        {
            poDS = (OGRDataSource*) OGROpen(DEFAULT_CACHE_CSV, TRUE, NULL);
            if( poDS != NULL )
            {
                CPLFree(hSession->pszCacheFilename);
                hSession->pszCacheFilename = CPLStrdup(DEFAULT_CACHE_CSV);
                CPLDebug("OGR", "Switch geocode cache file to %s",
                         hSession->pszCacheFilename);
                osExt = "csv";
            }
        }

        if( bCreateIfNecessary && poDS == NULL &&
            !EQUALN(hSession->pszCacheFilename, "PG:", 3) )
        {
            OGRSFDriverH hDriver = OGRGetDriverByName(osExt);
            if( hDriver == NULL &&
                EQUAL(hSession->pszCacheFilename, DEFAULT_CACHE_SQLITE) )
            {
                CPLFree(hSession->pszCacheFilename);
                hSession->pszCacheFilename = CPLStrdup(DEFAULT_CACHE_CSV);
                CPLDebug("OGR", "Switch geocode cache file to %s",
                         hSession->pszCacheFilename);
                osExt = "csv";
                hDriver = OGRGetDriverByName(osExt);
            }
            if( hDriver != NULL )
            {
                char** papszOptions = NULL;
                if( EQUAL(osExt, "SQLITE") )
                {
                    papszOptions = CSLAddNameValue(papszOptions,
                                                   "METADATA", "FALSE");
                }

                poDS = (OGRDataSource*) OGR_Dr_CreateDataSource(
                            hDriver, hSession->pszCacheFilename, papszOptions);

                if( poDS == NULL &&
                    (EQUAL(osExt, "SQLITE") || EQUAL(osExt, "CSV")))
                {
                    CPLFree(hSession->pszCacheFilename);
                    hSession->pszCacheFilename = CPLStrdup(
                        CPLSPrintf("/vsimem/%s.%s",
                                   CACHE_LAYER_NAME, osExt.c_str()));
                    CPLDebug("OGR", "Switch geocode cache file to %s",
                         hSession->pszCacheFilename);
                    poDS = (OGRDataSource*) OGR_Dr_CreateDataSource(
                            hDriver, hSession->pszCacheFilename, papszOptions);
                }

                CSLDestroy(papszOptions);
            }
        }

        CPLSetThreadLocalConfigOption("OGR_SQLITE_SYNCHRONOUS", pszOldVal);

        if( poDS == NULL )
            return NULL;

        hSession->poDS = poDS;
    }

    CPLPushErrorHandler(CPLQuietErrorHandler);
    OGRLayer* poLayer = poDS->GetLayerByName(CACHE_LAYER_NAME);
    CPLPopErrorHandler();

    if( bCreateIfNecessary && poLayer == NULL )
    {
        poLayer = poDS->CreateLayer(
                        CACHE_LAYER_NAME, NULL, wkbNone, NULL);
        if( poLayer != NULL )
        {
            OGRFieldDefn oFieldDefnURL(FIELD_URL, OFTString);
            poLayer->CreateField(&oFieldDefnURL);
            OGRFieldDefn oFieldDefnBlob(FIELD_BLOB, OFTString);
            poLayer->CreateField(&oFieldDefnBlob);
            if( EQUAL(osExt, "SQLITE") ||
                EQUALN(hSession->pszCacheFilename, "PG:", 3) )
            {
                const char* pszSQL =
                    CPLSPrintf( "CREATE INDEX idx_%s_%s ON %s(%s)",
                                FIELD_URL, poLayer->GetName(),
                                poLayer->GetName(), FIELD_URL );
                poDS->ExecuteSQL(pszSQL, NULL, NULL);
            }
        }
    }

    int nIdxBlob = -1;
    if( poLayer == NULL ||
        poLayer->GetLayerDefn()->GetFieldIndex(FIELD_URL) < 0 ||
        (nIdxBlob = poLayer->GetLayerDefn()->GetFieldIndex(FIELD_BLOB)) < 0 )
    {
        return NULL;
    }

    if( pnIdxBlob )
        *pnIdxBlob = nIdxBlob;

    return poLayer;
}

/************************************************************************/
/*                        OGRGeocodeGetFromCache()                      */
/************************************************************************/

static char* OGRGeocodeGetFromCache(OGRGeocodingSessionH hSession,
                                    const char* pszURL)
{
    CPLMutexHolderD(&hMutex);

    int nIdxBlob = -1;
    OGRLayer* poLayer = OGRGeocodeGetCacheLayer(hSession, FALSE, &nIdxBlob);
    if( poLayer == NULL )
        return NULL;

    char* pszSQLEscapedURL = CPLEscapeString(pszURL, -1, CPLES_SQL);
    poLayer->SetAttributeFilter(CPLSPrintf("%s='%s'", FIELD_URL, pszSQLEscapedURL));
    CPLFree(pszSQLEscapedURL);

    char* pszRet = NULL;
    OGRFeature* poFeature = poLayer->GetNextFeature();
    if( poFeature != NULL )
    {
        if( poFeature->IsFieldSet(nIdxBlob) )
            pszRet = CPLStrdup(poFeature->GetFieldAsString(nIdxBlob));
        OGRFeature::DestroyFeature(poFeature);
    }

    return pszRet;
}

/************************************************************************/
/*                        OGRGeocodePutIntoCache()                      */
/************************************************************************/

static int OGRGeocodePutIntoCache(OGRGeocodingSessionH hSession,
                                  const char* pszURL,
                                  const char* pszContent)
{
    CPLMutexHolderD(&hMutex);

    int nIdxBlob = -1;
    OGRLayer* poLayer = OGRGeocodeGetCacheLayer(hSession, TRUE, &nIdxBlob);
    if( poLayer == NULL )
        return FALSE;

    OGRFeature* poFeature = new OGRFeature(poLayer->GetLayerDefn());
    poFeature->SetField(FIELD_URL, pszURL);
    poFeature->SetField(FIELD_BLOB, pszContent);
    int bRet = poLayer->CreateFeature(poFeature) == OGRERR_NONE;
    delete poFeature;

    return bRet;
}

/************************************************************************/
/*                         OGRGeocodeBuildLayer()                       */
/************************************************************************/

static OGRLayerH OGRGeocodeMakeRawLayer(const char* pszContent)
{
    OGRMemLayer* poLayer = new OGRMemLayer( "result", NULL, wkbNone );
    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
    OGRFieldDefn oFieldDefnRaw("raw", OFTString);
    poLayer->CreateField(&oFieldDefnRaw);
    OGRFeature* poFeature = new OGRFeature(poFDefn);
    poFeature->SetField("raw", pszContent);
    poLayer->CreateFeature(poFeature);
    delete poFeature;
    return (OGRLayerH) poLayer;
}

/************************************************************************/
/*                  OGRGeocodeBuildLayerNominatim()                     */
/************************************************************************/

static OGRLayerH OGRGeocodeBuildLayerNominatim(CPLXMLNode* psSearchResults,
                                               const char* pszContent,
                                               int bAddRawFeature)
{
    OGRMemLayer* poLayer = new OGRMemLayer( "place", NULL, wkbUnknown );
    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();

    CPLXMLNode* psPlace = psSearchResults->psChild;
    while( psPlace != NULL )
    {
        if( psPlace->eType == CXT_Element &&
            strcmp(psPlace->pszValue, "place") == 0 )
        {
            int bFoundLat = FALSE, bFoundLon = FALSE;
            double dfLat = 0.0, dfLon = 0.0;

            /* First iteration to add fields */
            CPLXMLNode* psChild = psPlace->psChild;
            while( psChild != NULL )
            {
                const char* pszName = psChild->pszValue;
                const char* pszVal = CPLGetXMLValue(psChild, NULL, NULL);
                if( (psChild->eType == CXT_Element || psChild->eType == CXT_Attribute) &&
                    poFDefn->GetFieldIndex(pszName) < 0 &&
                    strcmp(pszName, "geotext") != 0 )
                {
                    OGRFieldDefn oFieldDefn(pszName, OFTString);
                    if( strcmp(pszName, "place_rank") == 0 )
                    {
                        oFieldDefn.SetType(OFTInteger);
                    }
                    else if( strcmp(pszName, "lat") == 0 )
                    {
                        if( pszVal != NULL )
                        {
                            bFoundLat = TRUE;
                            dfLat = CPLAtofM(pszVal);
                        }
                        oFieldDefn.SetType(OFTReal);
                    }
                    else if( strcmp(pszName, "lon") == 0 )
                    {
                        if( pszVal != NULL )
                        {
                            bFoundLon = TRUE;
                            dfLon = CPLAtofM(pszVal);
                        }
                        oFieldDefn.SetType(OFTReal);
                    }
                    poLayer->CreateField(&oFieldDefn);
                }
                psChild = psChild->psNext;
            }

            if( bAddRawFeature )
            {
                OGRFieldDefn oFieldDefnRaw("raw", OFTString);
                poLayer->CreateField(&oFieldDefnRaw);
            }

            /* Second iteration to fill the feature */
            OGRFeature* poFeature = new OGRFeature(poFDefn);
            psChild = psPlace->psChild;
            while( psChild != NULL )
            {
                int nIdx;
                const char* pszName = psChild->pszValue;
                const char* pszVal = CPLGetXMLValue(psChild, NULL, NULL);
                if( !(psChild->eType == CXT_Element || psChild->eType == CXT_Attribute) )
                {
                    // do nothing
                }
                else if( (nIdx = poFDefn->GetFieldIndex(pszName)) >= 0 )
                {
                    if( pszVal != NULL )
                        poFeature->SetField(nIdx, pszVal);
                }
                else if( strcmp(pszName, "geotext") == 0 )
                {
                    char* pszWKT = (char*) pszVal;
                    if( pszWKT != NULL )
                    {
                        OGRGeometry* poGeometry = NULL;
                        OGRGeometryFactory::createFromWkt(&pszWKT, NULL,
                                                          &poGeometry);
                        if( poGeometry )
                            poFeature->SetGeometryDirectly(poGeometry);
                    }
                }
                psChild = psChild->psNext;
            }

            if( bAddRawFeature )
            {
                CPLXMLNode* psOldNext = psPlace->psNext;
                psPlace->psNext = NULL;
                char* pszXML = CPLSerializeXMLTree(psPlace);
                psPlace->psNext = psOldNext;

                poFeature->SetField("raw", pszXML);
                CPLFree(pszXML);
            }

            /* If we didn't found an explicit geometry, build it from */
            /* the 'lon' and 'lat' attributes. */
            if( poFeature->GetGeometryRef() == NULL && bFoundLon && bFoundLat )
                poFeature->SetGeometryDirectly(new OGRPoint(dfLon, dfLat));

            poLayer->CreateFeature(poFeature);
            delete poFeature;
        }
        psPlace = psPlace->psNext;
    }
    return (OGRLayerH) poLayer;
}

/************************************************************************/
/*                   OGRGeocodeBuildLayerYahoo()                        */
/************************************************************************/

static OGRLayerH OGRGeocodeBuildLayerYahoo(CPLXMLNode* psResultSet,
                                           const char* pszContent,
                                           int bAddRawFeature)
{
    OGRMemLayer* poLayer = new OGRMemLayer( "place", NULL, wkbPoint );
    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();

    CPLXMLNode* psPlace = psResultSet->psChild;
    while( psPlace != NULL )
    {
        if( psPlace->eType == CXT_Element &&
            strcmp(psPlace->pszValue, "Result") == 0 )
        {
            int bFoundLat = FALSE, bFoundLon = FALSE;
            double dfLat = 0.0, dfLon = 0.0;

            /* First iteration to add fields */
            CPLXMLNode* psChild = psPlace->psChild;
            while( psChild != NULL )
            {
                const char* pszName = psChild->pszValue;
                const char* pszVal = CPLGetXMLValue(psChild, NULL, NULL);
                if( (psChild->eType == CXT_Element || psChild->eType == CXT_Attribute) &&
                    poFDefn->GetFieldIndex(pszName) < 0 )
                {
                    OGRFieldDefn oFieldDefn(pszName, OFTString);
                    if( strcmp(pszName, "latitude") == 0 )
                    {
                        if( pszVal != NULL )
                        {
                            bFoundLat = TRUE;
                            dfLat = CPLAtofM(pszVal);
                        }
                        oFieldDefn.SetType(OFTReal);
                    }
                    else if( strcmp(pszName, "longitude") == 0 )
                    {
                        if( pszVal != NULL )
                        {
                            bFoundLon = TRUE;
                            dfLon = CPLAtofM(pszVal);
                        }
                        oFieldDefn.SetType(OFTReal);
                    }
                    poLayer->CreateField(&oFieldDefn);
                }
                psChild = psChild->psNext;
            }

            OGRFieldDefn oFieldDefnDisplayName("display_name", OFTString);
            poLayer->CreateField(&oFieldDefnDisplayName);

            if( bAddRawFeature )
            {
                OGRFieldDefn oFieldDefnRaw("raw", OFTString);
                poLayer->CreateField(&oFieldDefnRaw);
            }

            /* Second iteration to fill the feature */
            OGRFeature* poFeature = new OGRFeature(poFDefn);
            psChild = psPlace->psChild;
            while( psChild != NULL )
            {
                int nIdx;
                const char* pszName = psChild->pszValue;
                const char* pszVal = CPLGetXMLValue(psChild, NULL, NULL);
                if( !(psChild->eType == CXT_Element || psChild->eType == CXT_Attribute) )
                {
                    // do nothing
                }
                else if( (nIdx = poFDefn->GetFieldIndex(pszName)) >= 0 )
                {
                    if( pszVal != NULL )
                        poFeature->SetField(nIdx, pszVal);
                }
                psChild = psChild->psNext;
            }

            CPLString osDisplayName;
            for(int i=1;;i++)
            {
                int nIdx = poFDefn->GetFieldIndex(CPLSPrintf("line%d", i));
                if( nIdx < 0 )
                    break;
                if( poFeature->IsFieldSet(nIdx) )
                {
                    if( osDisplayName.size() )
                        osDisplayName += ", ";
                    osDisplayName += poFeature->GetFieldAsString(nIdx);
                }
            }
            poFeature->SetField("display_name", osDisplayName.c_str());

            if( bAddRawFeature )
            {
                CPLXMLNode* psOldNext = psPlace->psNext;
                psPlace->psNext = NULL;
                char* pszXML = CPLSerializeXMLTree(psPlace);
                psPlace->psNext = psOldNext;

                poFeature->SetField("raw", pszXML);
                CPLFree(pszXML);
            }

            /* Build geometry from the 'lon' and 'lat' attributes. */
            if( bFoundLon && bFoundLat )
                poFeature->SetGeometryDirectly(new OGRPoint(dfLon, dfLat));

            poLayer->CreateFeature(poFeature);
            delete poFeature;
        }
        psPlace = psPlace->psNext;
    }
    return (OGRLayerH) poLayer;
}

/************************************************************************/
/*                         OGRGeocodeBuildLayer()                       */
/************************************************************************/

static OGRLayerH OGRGeocodeBuildLayer(const char* pszContent,
                                      int bAddRawFeature)
{
    OGRLayerH hLayer = NULL;
    CPLXMLNode* psRoot = CPLParseXMLString( pszContent );
    if( psRoot != NULL )
    {
        CPLXMLNode* psSearchResults;
        CPLXMLNode* psResultSet;
        if( (psSearchResults =
                        CPLSearchXMLNode(psRoot, "=searchresults")) != NULL )
            hLayer = OGRGeocodeBuildLayerNominatim(psSearchResults,
                                                   pszContent,
                                                   bAddRawFeature);
        else if( (psResultSet =
                        CPLSearchXMLNode(psRoot, "=ResultSet")) != NULL )
            hLayer = OGRGeocodeBuildLayerYahoo(psResultSet,
                                               pszContent,
                                               bAddRawFeature);
        CPLDestroyXMLNode( psRoot );
    }
    if( hLayer == NULL && bAddRawFeature )
        hLayer = OGRGeocodeMakeRawLayer(pszContent);
    return hLayer;
}

/************************************************************************/
/*                              OGRGeocode()                            */
/************************************************************************/

/**
 * \brief Runs a geocoding request.
 *
 * If the result is not found in cache, a GET request will be sent to resolve
 * the query.
 *
 * Note: most online services have Term of Uses. You are kindly requested
 * to read and follow them. For the OpenStreetMap Nominatim service, this
 * implementation will make sure that no more than one request is sent by
 * second, but there might be other restrictions that you must follow by other
 * means.
 *
 * In case of success, the return of this function is a OGR layer that contain
 * zero, one or several features matching the query. Note that the geometry of the
 * features is not necessarily a point.  The returned layer must be freed with
 * OGRGeocodeFreeResult().
 *
 * Note: this function is also available as the SQL
 * <a href="ogr_sql_sqlite.html#ogr_sql_sqlite_ogr_geocode_function">ogr_geocode()</a>
 * function of the SQL SQLite dialect.
 *
 * The list of recognized options is :
 * <ul>
 * <li>ADDRESSDETAILS=0 or 1: Include a breakdown of the address into elements
 *     Defaults to 1. (Known to work with OSM and MapQuest Nominatim)
 * <li>COUNTRYCODES=code1,code2,...codeN: Limit search results to a specific
 *     country (or a list of countries). The codes must fellow ISO 3166-1, i.e.
 *     gb for United Kingdom, de for Germany, etc.. (Known to work with OSM and MapQuest Nominatim)
 * <li>LIMIT=number: the number of records to return. Unlimited if not specified.
 *     (Known to work with OSM and MapQuest Nominatim)
 * <li>RAW_FEATURE=YES: to specify that a 'raw' field must be added to the returned
 *     feature with the raw XML content.
 * <li> EXTRA_QUERY_PARAMETERS=params: additionnal parameters for the GET request.
 * </ul>
 *
 * @param hSession the geocoding session handle.
 * @param pszQuery the string to geocode.
 * @param papszStructuredQuery unused for now. Must be NULL.
 * @param papszOptions a list of options.
 *
 * @return a OGR layer with the result(s), or NULL in case of error.
 *         The returned layer must be freed with OGRGeocodeFreeResult().
 *
 * @since GDAL 1.10
 */
OGRLayerH OGRGeocode(OGRGeocodingSessionH hSession,
                     const char* pszQuery,
                     char** papszStructuredQuery,
                     char** papszOptions)
{
    VALIDATE_POINTER1( hSession, "OGRGeocode", NULL );
    if( (pszQuery == NULL && papszStructuredQuery == NULL) ||
        (pszQuery != NULL && papszStructuredQuery != NULL) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Only one of pszQuery or papszStructuredQuery must be set.");
        return NULL;
    }

    if( papszStructuredQuery != NULL )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "papszStructuredQuery not yet supported.");
        return NULL;
    }

    char* pszEscapedQuery = CPLEscapeString(pszQuery, -1, CPLES_URL);
    CPLString osURL = CPLSPrintf(hSession->pszQueryTemplate, pszEscapedQuery);
    CPLFree(pszEscapedQuery);

    if( EQUAL(hSession->pszGeocodingService, "OSM_NOMINATIM") ||
        EQUAL(hSession->pszGeocodingService, "MAPQUEST_NOMINATIM") )
    {
        const char* pszAddressDetails = OGRGeocodeGetParameter(papszOptions, "ADDRESSDETAILS", "1");
        osURL += "&addressdetails=";
        osURL += pszAddressDetails;

        const char* pszCountryCodes = OGRGeocodeGetParameter(papszOptions, "COUNTRYCODES", NULL);
        if( pszCountryCodes != NULL )
        {
            osURL += "&countrycodes=";
            osURL += pszCountryCodes;
        }

        const char* pszLimit = OGRGeocodeGetParameter(papszOptions, "LIMIT", NULL);
        if( pszLimit != NULL && *pszLimit != '\0' )
        {
            osURL += "&limit=";
            osURL += pszLimit;
        }
    }

    /* Only documented to work with OSM Nominatim. */
    if( hSession->pszLanguage != NULL )
    {
        osURL += "&accept-language=";
        osURL += hSession->pszLanguage;
    }

    const char* pszExtraQueryParameters = OGRGeocodeGetParameter(
                                papszOptions, "EXTRA_QUERY_PARAMETERS", NULL);
    if( pszExtraQueryParameters != NULL )
    {
        osURL += "&";
        osURL += pszExtraQueryParameters;
    }

    CPLString osURLWithEmail = osURL;
    if( EQUAL(hSession->pszGeocodingService, "OSM_NOMINATIM") &&
        hSession->pszEmail != NULL )
    {
        char* pszEscapedEmail = CPLEscapeString(hSession->pszEmail,
                                                -1, CPLES_URL);
        osURLWithEmail = osURL + "&email=" + pszEscapedEmail;
        CPLFree(pszEscapedEmail);
    }

    int bAddRawFeature =
        CSLTestBoolean(OGRGeocodeGetParameter(papszOptions, "RAW_FEATURE", "NO"));

    OGRLayerH hLayer = NULL;

    char* pszCachedResult = NULL;
    if( hSession->bReadCache )
        pszCachedResult = OGRGeocodeGetFromCache(hSession, osURL);
    if( pszCachedResult == NULL )
    {
        CPLHTTPResult* psResult;

        double* pdfLastQueryTime = NULL;
        if( EQUAL(hSession->pszGeocodingService, "OSM_NOMINATIM") )
            pdfLastQueryTime = &dfLastQueryTimeStampOSMNominatim;
        else if( EQUAL(hSession->pszGeocodingService, "MAPQUEST_NOMINATIM") )
            pdfLastQueryTime = &dfLastQueryTimeStampMapQuestNominatim;

        char** papszHTTPOptions = NULL;
        CPLString osHeaders;
        osHeaders = "User-Agent: ";
        osHeaders += hSession->pszApplication;
        if( hSession->pszLanguage != NULL )
        {
            osHeaders += "\r\nAccept-Language: ";
            osHeaders += hSession->pszLanguage;
        }
        papszHTTPOptions = CSLAddNameValue(papszHTTPOptions, "HEADERS",
                                           osHeaders.c_str());

        if( pdfLastQueryTime != NULL )
        {
            CPLMutexHolderD(&hMutex);
            struct timeval tv;

            gettimeofday(&tv, NULL);
            double dfCurrentTime = tv.tv_sec + tv.tv_usec / 1e6;
            if( dfCurrentTime < *pdfLastQueryTime +
                                    hSession->dfDelayBetweenQueries )
            {
                CPLSleep(*pdfLastQueryTime + hSession->dfDelayBetweenQueries -
                         dfCurrentTime);
            }

            psResult = CPLHTTPFetch( osURLWithEmail,  papszHTTPOptions );

            gettimeofday(&tv, NULL);
            *pdfLastQueryTime = tv.tv_sec + tv.tv_usec / 1e6;
        }
        else
            psResult = CPLHTTPFetch( osURLWithEmail,  papszHTTPOptions );

        CSLDestroy(papszHTTPOptions);
        papszHTTPOptions = NULL;

        if( psResult == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Query for '%s' failed", pszQuery);
        }
        else
        {
            const char* pszResult = (const char*) psResult->pabyData;
            if( pszResult != NULL )
            {
                if( hSession->bWriteCache )
                    OGRGeocodePutIntoCache(hSession, osURL, pszResult);
                hLayer = OGRGeocodeBuildLayer(pszResult, bAddRawFeature);
            }
            CPLHTTPDestroyResult(psResult);
        }
    }
    else
    {
        hLayer = OGRGeocodeBuildLayer(pszCachedResult, bAddRawFeature);
        CPLFree(pszCachedResult);
    }

    return hLayer;
}

/************************************************************************/
/*               OGRGeocodeReverseBuildLayerNominatim()                 */
/************************************************************************/

static OGRLayerH OGRGeocodeReverseBuildLayerNominatim(CPLXMLNode* psReverseGeocode,
                                                      const char* pszContent,
                                                      int bAddRawFeature)
{
    CPLXMLNode* psResult = CPLGetXMLNode(psReverseGeocode, "result");
    CPLXMLNode* psAddressParts = CPLGetXMLNode(psReverseGeocode, "addressparts");
    if( psResult == NULL || psAddressParts == NULL )
    {
        return NULL;
    }

    OGRMemLayer* poLayer = new OGRMemLayer( "result", NULL, wkbNone );
    OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();

    int bFoundLat = FALSE, bFoundLon = FALSE;
    double dfLat = 0.0, dfLon = 0.0;

    /* First iteration to add fields */
    CPLXMLNode* psChild = psResult->psChild;
    while( psChild != NULL )
    {
        const char* pszName = psChild->pszValue;
        const char* pszVal = CPLGetXMLValue(psChild, NULL, NULL);
        if( (psChild->eType == CXT_Element || psChild->eType == CXT_Attribute) &&
            poFDefn->GetFieldIndex(pszName) < 0 )
        {
            OGRFieldDefn oFieldDefn(pszName, OFTString);
            if( strcmp(pszName, "lat") == 0 )
            {
                if( pszVal != NULL )
                {
                    bFoundLat = TRUE;
                    dfLat = CPLAtofM(pszVal);
                }
                oFieldDefn.SetType(OFTReal);
            }
            else if( strcmp(pszName, "lon") == 0 )
            {
                if( pszVal != NULL )
                {
                    bFoundLon = TRUE;
                    dfLon = CPLAtofM(pszVal);
                }
                oFieldDefn.SetType(OFTReal);
            }
            poLayer->CreateField(&oFieldDefn);
        }
        psChild = psChild->psNext;
    }

    OGRFieldDefn oFieldDefn("display_name", OFTString);
    poLayer->CreateField(&oFieldDefn);

    psChild = psAddressParts->psChild;
    while( psChild != NULL )
    {
        const char* pszName = psChild->pszValue;
        if( (psChild->eType == CXT_Element || psChild->eType == CXT_Attribute) &&
            poFDefn->GetFieldIndex(pszName) < 0 )
        {
            OGRFieldDefn oFieldDefn(pszName, OFTString);
            poLayer->CreateField(&oFieldDefn);
        }
        psChild = psChild->psNext;
    }

    if( bAddRawFeature )
    {
        OGRFieldDefn oFieldDefnRaw("raw", OFTString);
        poLayer->CreateField(&oFieldDefnRaw);
    }

    /* Second iteration to fill the feature */
    OGRFeature* poFeature = new OGRFeature(poFDefn);
    psChild = psResult->psChild;
    while( psChild != NULL )
    {
        int nIdx;
        const char* pszName = psChild->pszValue;
        const char* pszVal = CPLGetXMLValue(psChild, NULL, NULL);
        if( (psChild->eType == CXT_Element || psChild->eType == CXT_Attribute) &&
            (nIdx = poFDefn->GetFieldIndex(pszName)) >= 0 )
        {
            if( pszVal != NULL )
                poFeature->SetField(nIdx, pszVal);
        }
        psChild = psChild->psNext;
    }

    const char* pszVal = CPLGetXMLValue(psResult, NULL, NULL);
    if( pszVal != NULL )
        poFeature->SetField("display_name", pszVal);

    psChild = psAddressParts->psChild;
    while( psChild != NULL )
    {
        int nIdx;
        const char* pszName = psChild->pszValue;
        const char* pszVal = CPLGetXMLValue(psChild, NULL, NULL);
        if( (psChild->eType == CXT_Element || psChild->eType == CXT_Attribute) &&
            (nIdx = poFDefn->GetFieldIndex(pszName)) >= 0 )
        {
            if( pszVal != NULL )
                poFeature->SetField(nIdx, pszVal);
        }
        psChild = psChild->psNext;
    }

    if( bAddRawFeature )
    {
        poFeature->SetField("raw", pszContent);
    }

    /* If we didn't found an explicit geometry, build it from */
    /* the 'lon' and 'lat' attributes. */
    if( poFeature->GetGeometryRef() == NULL && bFoundLon && bFoundLat )
        poFeature->SetGeometryDirectly(new OGRPoint(dfLon, dfLat));

    poLayer->CreateFeature(poFeature);
    delete poFeature;

    return (OGRLayerH) poLayer;
}

/************************************************************************/
/*                     OGRGeocodeReverseBuildLayer()                    */
/************************************************************************/

static OGRLayerH OGRGeocodeReverseBuildLayer(const char* pszContent,
                                             int bAddRawFeature)
{
    OGRLayerH hLayer = NULL;
    CPLXMLNode* psRoot = CPLParseXMLString( pszContent );
    if( psRoot != NULL )
    {
        CPLXMLNode* psReverseGeocode;
        CPLXMLNode* psResultSet;
        if( (psReverseGeocode =
                    CPLSearchXMLNode(psRoot, "=reversegeocode")) != NULL )
            hLayer = OGRGeocodeReverseBuildLayerNominatim(psReverseGeocode,
                                                          pszContent,
                                                          bAddRawFeature);
        else if( (psResultSet =
                    CPLSearchXMLNode(psRoot, "=ResultSet")) != NULL )
            hLayer = OGRGeocodeBuildLayerYahoo(psResultSet,
                                               pszContent,
                                               bAddRawFeature);
        CPLDestroyXMLNode( psRoot );
    }
    if( hLayer == NULL && bAddRawFeature )
        hLayer = OGRGeocodeMakeRawLayer(pszContent);
    return hLayer;
}

/************************************************************************/
/*                      OGRGeocodeReverseSubstitute()                   */
/************************************************************************/

static CPLString OGRGeocodeReverseSubstitute(CPLString osURL,
                                             double dfLon, double dfLat)
{
    size_t iPos = osURL.find("{lon}");
    if( iPos != std::string::npos )
    {
        CPLString osEnd(osURL.substr(iPos + 5));
        osURL = osURL.substr(0,iPos);
        osURL += CPLSPrintf("%.16f", dfLon);
        osURL += osEnd;
    }

    iPos = osURL.find("{lat}");
    if( iPos != std::string::npos )
    {
        CPLString osEnd(osURL.substr(iPos + 5));
        osURL = osURL.substr(0,iPos);
        osURL += CPLSPrintf("%.16f", dfLat);
        osURL += osEnd;
    }

    return osURL;
}

/************************************************************************/
/*                         OGRGeocodeReverse()                          */
/************************************************************************/

/**
 * \brief Runs a reverse geocoding request.
 *
 * If the result is not found in cache, a GET request will be sent to resolve
 * the query.
 *
 * Note: most online services have Term of Uses. You are kindly requested
 * to read and follow them. For the OpenStreetMap Nominatim service, this
 * implementation will make sure that no more than one request is sent by
 * second, but there might be other restrictions that you must follow by other
 * means.
 *
 * In case of success, the return of this function is a OGR layer that contain
 * zero, one or several features matching the query. The returned layer must be freed with
 * OGRGeocodeFreeResult().
 *
 * Note: this function is also available as the SQL
 * <a href="ogr_sql_sqlite.html#ogr_sql_sqlite_ogr_geocode_function">ogr_geocode_reverse()</a>
 * function of the SQL SQLite dialect.
 *
 * The list of recognized options is :
 * <ul>
 * <li>ZOOM=a_level: to query a specific zoom level. Only understood by the OSM Nominatim service.
 * <li>RAW_FEATURE=YES: to specify that a 'raw' field must be added to the returned
 *     feature with the raw XML content.
 * <li>EXTRA_QUERY_PARAMETERS=params: additionnal parameters for the GET request
 *     for reverse geocoding.
 * </ul>
 *
 * @param hSession the geocoding session handle.
 * @param dfLon the longitude.
 * @param dfLat the latitude.
 * @param papszOptions a list of options.
 *
 * @return a OGR layer with the result(s), or NULL in case of error.
 *         The returned layer must be freed with OGRGeocodeFreeResult().
 *
 * @since GDAL 1.10
 */
OGRLayerH OGRGeocodeReverse(OGRGeocodingSessionH hSession,
                            double dfLon, double dfLat,
                            char** papszOptions)
{
    VALIDATE_POINTER1( hSession, "OGRGeocodeReverse", NULL );

    CPLString osURL = hSession->pszReverseQueryTemplate;
    osURL = OGRGeocodeReverseSubstitute(osURL, dfLon, dfLat);

    if( EQUAL(hSession->pszGeocodingService, "OSM_NOMINATIM") )
    {
        const char* pszZoomLevel = OGRGeocodeGetParameter(papszOptions, "ZOOM", NULL);
        if( pszZoomLevel != NULL )
        {
            osURL = osURL + "&zoom=" + pszZoomLevel;
        }
    }

    /* Only documented to work with OSM Nominatim. */
    if( hSession->pszLanguage != NULL )
    {
        osURL += "&accept-language=";
        osURL += hSession->pszLanguage;
    }

    const char* pszExtraQueryParameters = OGRGeocodeGetParameter(
                                papszOptions, "EXTRA_QUERY_PARAMETERS", NULL);
    if( pszExtraQueryParameters != NULL )
    {
        osURL += "&";
        osURL += pszExtraQueryParameters;
    }

    CPLString osURLWithEmail = osURL;
    if( EQUAL(hSession->pszGeocodingService, "OSM_NOMINATIM") &&
        hSession->pszEmail != NULL )
    {
        char* pszEscapedEmail = CPLEscapeString(hSession->pszEmail,
                                                -1, CPLES_URL);
        osURLWithEmail = osURL + "&email=" + pszEscapedEmail;
        CPLFree(pszEscapedEmail);
    }

    int bAddRawFeature =
        CSLTestBoolean(OGRGeocodeGetParameter(papszOptions, "RAW_FEATURE", "NO"));

    OGRLayerH hLayer = NULL;

    char* pszCachedResult = NULL;
    if( hSession->bReadCache )
        pszCachedResult = OGRGeocodeGetFromCache(hSession, osURL);
    if( pszCachedResult == NULL )
    {
        CPLHTTPResult* psResult;

        double* pdfLastQueryTime = NULL;
        if( EQUAL(hSession->pszGeocodingService, "OSM_NOMINATIM") )
            pdfLastQueryTime = &dfLastQueryTimeStampOSMNominatim;
        else if( EQUAL(hSession->pszGeocodingService, "MAPQUEST_NOMINATIM") )
            pdfLastQueryTime = &dfLastQueryTimeStampMapQuestNominatim;

        char** papszHTTPOptions = NULL;
        CPLString osHeaders;
        osHeaders = "User-Agent: ";
        osHeaders += hSession->pszApplication;
        if( hSession->pszLanguage != NULL )
        {
            osHeaders += "\r\nAccept-Language: ";
            osHeaders += hSession->pszLanguage;
        }
        papszHTTPOptions = CSLAddNameValue(papszHTTPOptions, "HEADERS",
                                           osHeaders.c_str());

        if( pdfLastQueryTime != NULL )
        {
            CPLMutexHolderD(&hMutex);
            struct timeval tv;

            gettimeofday(&tv, NULL);
            double dfCurrentTime = tv.tv_sec + tv.tv_usec / 1e6;
            if( dfCurrentTime < *pdfLastQueryTime +
                                    hSession->dfDelayBetweenQueries )
            {
                CPLSleep(*pdfLastQueryTime + hSession->dfDelayBetweenQueries -
                         dfCurrentTime);
            }

            psResult = CPLHTTPFetch( osURLWithEmail,  papszHTTPOptions );

            gettimeofday(&tv, NULL);
            *pdfLastQueryTime = tv.tv_sec + tv.tv_usec / 1e6;
        }
        else
            psResult = CPLHTTPFetch( osURLWithEmail,  papszHTTPOptions );

        CSLDestroy(papszHTTPOptions);
        papszHTTPOptions = NULL;

        if( psResult == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Query failed");
        }
        else
        {
            const char* pszResult = (const char*) psResult->pabyData;
            if( pszResult != NULL )
            {
                if( hSession->bWriteCache )
                    OGRGeocodePutIntoCache(hSession, osURL, pszResult);
                hLayer = OGRGeocodeReverseBuildLayer(pszResult, bAddRawFeature);
            }
            CPLHTTPDestroyResult(psResult);
        }
    }
    else
    {
        hLayer = OGRGeocodeReverseBuildLayer(pszCachedResult, bAddRawFeature);
        CPLFree(pszCachedResult);
    }

    return hLayer;
}

/************************************************************************/
/*                        OGRGeocodeFreeResult()                        */
/************************************************************************/

/**
 * \brief Destroys the result of a geocoding request.
 *
 * @param hLayer the layer returned by OGRGeocode() or OGRGeocodeReverse()
 *               to destroy.
 *
 * @since GDAL 1.10
 */
void OGRGeocodeFreeResult(OGRLayerH hLayer)
{
    delete (OGRLayer*) hLayer;
}
