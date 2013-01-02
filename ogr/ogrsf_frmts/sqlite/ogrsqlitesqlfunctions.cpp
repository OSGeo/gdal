/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Extension SQL functions
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

/* WARNING: VERY IMPORTANT NOTE: This file MUST not be directly compiled as */
/* a standalone object. It must be included from ogrsqlitevirtualogr.cpp */
#ifndef COMPILATION_ALLOWED
#error See comment in file
#endif

#include "ogrsqlitesqlfunctions.h"
#include "ogr_geocoding.h"

#include "ogrsqliteregexp.cpp" /* yes the .cpp file, to make it work on Windows with load_extension('gdalXX.dll') */

#ifndef HAVE_SPATIALITE
#define MINIMAL_SPATIAL_FUNCTIONS
#endif

class OGRSQLiteExtensionData
{
#ifdef DEBUG
    void* pDummy; /* to track memory leaks */
#endif
    std::map< std::pair<int,int>, OGRCoordinateTransformation*> oCachedTransformsMap;

    void* hRegExpCache;

    OGRGeocodingSessionH hGeocodingSession;

  public:
                                 OGRSQLiteExtensionData(sqlite3* hDB);
                                ~OGRSQLiteExtensionData();

    OGRCoordinateTransformation* GetTransform(int nSrcSRSId, int nDstSRSId);

    OGRGeocodingSessionH         GetGeocodingSession() { return hGeocodingSession; }
    void                         SetGeocodingSession(OGRGeocodingSessionH hGeocodingSessionIn) { hGeocodingSession = hGeocodingSessionIn; }
    
    void                         SetRegExpCache(void* hRegExpCacheIn) { hRegExpCache = hRegExpCacheIn; }
};

/************************************************************************/
/*                     OGRSQLiteExtensionData()                         */
/************************************************************************/

OGRSQLiteExtensionData::OGRSQLiteExtensionData(sqlite3* hDB) :
        hRegExpCache(NULL), hGeocodingSession(NULL)
{
#ifdef DEBUG
    pDummy = CPLMalloc(1);
#endif
}

/************************************************************************/
/*                       ~OGRSQLiteExtensionData()                      */
/************************************************************************/

OGRSQLiteExtensionData::~OGRSQLiteExtensionData()
{
#ifdef DEBUG
    CPLFree(pDummy);
#endif

    std::map< std::pair<int,int>, OGRCoordinateTransformation*>::iterator oIter =
        oCachedTransformsMap.begin();
    for(; oIter != oCachedTransformsMap.end(); ++oIter)
        delete oIter->second;

    OGRSQLiteFreeRegExpCache(hRegExpCache);
    
    OGRGeocodeDestroySession(hGeocodingSession);
}

/************************************************************************/
/*                          GetTransform()                              */
/************************************************************************/

OGRCoordinateTransformation* OGRSQLiteExtensionData::GetTransform(int nSrcSRSId,
                                                                  int nDstSRSId)
{
    std::map< std::pair<int,int>, OGRCoordinateTransformation*>::iterator oIter =
        oCachedTransformsMap.find(std::pair<int,int>(nSrcSRSId, nDstSRSId));
    if( oIter == oCachedTransformsMap.end() )
    {
        OGRCoordinateTransformation* poCT = NULL;
        OGRSpatialReference oSrcSRS, oDstSRS;
        if (oSrcSRS.importFromEPSG(nSrcSRSId) == OGRERR_NONE &&
            oDstSRS.importFromEPSG(nDstSRSId) == OGRERR_NONE )
        {
            poCT = OGRCreateCoordinateTransformation( &oSrcSRS, &oDstSRS );
        }
        oCachedTransformsMap[std::pair<int,int>(nSrcSRSId, nDstSRSId)] = poCT;
        return poCT;
    }
    else
        return oIter->second;
}

/************************************************************************/
/*                        OGR2SQLITE_ogr_version()                     */
/************************************************************************/

static
void OGR2SQLITE_ogr_version(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    if( argc == 0 || sqlite3_value_type (argv[0]) != SQLITE_TEXT )
    {
        sqlite3_result_text( pContext, GDAL_RELEASE_NAME, -1, SQLITE_STATIC );
    }
    else
    {
        sqlite3_result_text( pContext,
                             GDALVersionInfo((const char*)sqlite3_value_text(argv[0])),
                             -1, SQLITE_TRANSIENT );
    }
}

/************************************************************************/
/*                          OGR2SQLITE_Transform()                      */
/************************************************************************/

static
void OGR2SQLITE_Transform(sqlite3_context* pContext,
                          int argc, sqlite3_value** argv)
{
    if( argc != 3 )
    {
        sqlite3_result_null (pContext);
        return;
    }

    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    if( sqlite3_value_type (argv[1]) != SQLITE_INTEGER )
    {
        sqlite3_result_null (pContext);
        return;
    }

    if( sqlite3_value_type (argv[2]) != SQLITE_INTEGER )
    {
        sqlite3_result_null (pContext);
        return;
    }

    int nSrcSRSId = sqlite3_value_int(argv[1]);
    int nDstSRSId = sqlite3_value_int(argv[2]);

    OGRSQLiteExtensionData* poModule =
                    (OGRSQLiteExtensionData*) sqlite3_user_data(pContext);
    OGRCoordinateTransformation* poCT =
                    poModule->GetTransform(nSrcSRSId, nDstSRSId);
    if( poCT == NULL )
    {
        sqlite3_result_null (pContext);
        return;
    }

    GByte* pabySLBLOB = (GByte *) sqlite3_value_blob (argv[0]);
    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    OGRGeometry* poGeom = NULL;
    if( OGRSQLiteLayer::ImportSpatiaLiteGeometry(
                    pabySLBLOB, nBLOBLen, &poGeom ) == CE_None &&
        poGeom->transform(poCT) == OGRERR_NONE &&
        OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    poGeom, nDstSRSId, wkbNDR, FALSE,
                    FALSE, FALSE, &pabySLBLOB, &nBLOBLen ) == CE_None )
    {
        sqlite3_result_blob(pContext, pabySLBLOB, nBLOBLen, CPLFree);
    }
    else
    {
        sqlite3_result_null (pContext);
    }
    delete poGeom;
}

/************************************************************************/
/*                       OGR2SQLITE_ogr_deflate()                       */
/************************************************************************/

static
void OGR2SQLITE_ogr_deflate(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    int nLevel = -1;
    if( !(argc == 1 || argc == 2) ||
        !(sqlite3_value_type (argv[0]) == SQLITE_TEXT ||
          sqlite3_value_type (argv[0]) == SQLITE_BLOB) )
    {
        sqlite3_result_null (pContext);
        return;
    }
    if( argc == 2 )
    {
        if( sqlite3_value_type (argv[1]) != SQLITE_INTEGER )
        {
            sqlite3_result_null (pContext);
            return;
        }
        nLevel = sqlite3_value_int(argv[1]);
    }

    size_t nOutBytes = 0;
    void* pOut;
    if( sqlite3_value_type (argv[0]) == SQLITE_TEXT )
    {
        const char* pszVal = (const char*)sqlite3_value_text(argv[0]);
        pOut = CPLZLibDeflate( pszVal, strlen(pszVal) + 1, nLevel, NULL, 0, &nOutBytes);
    }
    else
    {
        const void* pSrc = sqlite3_value_blob (argv[0]);
        int nLen = sqlite3_value_bytes (argv[0]);
        pOut = CPLZLibDeflate( pSrc, nLen, nLevel, NULL, 0, &nOutBytes);
    }
    if( pOut != NULL )
    {
        sqlite3_result_blob (pContext, pOut, nOutBytes, VSIFree);
    }
    else
    {
        sqlite3_result_null (pContext);
    }
 
    return;
}

/************************************************************************/
/*                       OGR2SQLITE_ogr_inflate()                       */
/************************************************************************/

static
void OGR2SQLITE_ogr_inflate(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    if( argc != 1 || 
        sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    size_t nOutBytes = 0;
    void* pOut;

    const void* pSrc = sqlite3_value_blob (argv[0]);
    int nLen = sqlite3_value_bytes (argv[0]);
    pOut = CPLZLibInflate( pSrc, nLen, NULL, 0, &nOutBytes);

    if( pOut != NULL )
    {
        sqlite3_result_blob (pContext, pOut, nOutBytes, VSIFree);
    }
    else
    {
        sqlite3_result_null (pContext);
    }

    return;
}

/************************************************************************/
/*                     OGR2SQLITE_ogr_geocode_set_result()              */
/************************************************************************/

static
void OGR2SQLITE_ogr_geocode_set_result(sqlite3_context* pContext,
                                       OGRLayerH hLayer,
                                       const char* pszField)
{
    if( hLayer == NULL )
        sqlite3_result_null (pContext);
    else
    {
        OGRLayer* poLayer = (OGRLayer*)hLayer;
        OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
        OGRFeature* poFeature = poLayer->GetNextFeature();
        int nIdx = -1;
        if( poFeature == NULL )
            sqlite3_result_null (pContext);
        else if( strcmp(pszField, "geometry") == 0 &&
                 poFeature->GetGeometryRef() != NULL )
        {
            GByte* pabyGeomBLOB = NULL;
            int nGeomBLOBLen = 0;
            if( OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    poFeature->GetGeometryRef(), 4326, wkbNDR, FALSE, FALSE, FALSE,
                    &pabyGeomBLOB,
                    &nGeomBLOBLen ) != CE_None )
            {
                sqlite3_result_null (pContext);
            }
            else
            {
                sqlite3_result_blob (pContext, pabyGeomBLOB, nGeomBLOBLen, CPLFree);
            }
        }
        else if( (nIdx = poFDefn->GetFieldIndex(pszField)) >= 0 &&
                 poFeature->IsFieldSet(nIdx) )
        {
            OGRFieldType eType = poFDefn->GetFieldDefn(nIdx)->GetType();
            if( eType == OFTInteger )
                sqlite3_result_int(pContext,
                                   poFeature->GetFieldAsInteger(nIdx));
            else if( eType == OFTReal )
                sqlite3_result_double(pContext,
                                      poFeature->GetFieldAsDouble(nIdx));
            else
                sqlite3_result_text(pContext,
                                    poFeature->GetFieldAsString(nIdx),
                                    -1, SQLITE_TRANSIENT);
        }
        else
            sqlite3_result_null (pContext);
        delete poFeature;
        OGRGeocodeFreeResult(hLayer);
    }
}

/************************************************************************/
/*                       OGR2SQLITE_ogr_geocode()                       */
/************************************************************************/

static
void OGR2SQLITE_ogr_geocode(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    OGRSQLiteExtensionData* poModule =
                    (OGRSQLiteExtensionData*) sqlite3_user_data(pContext);

    if( argc < 1 || sqlite3_value_type (argv[0]) != SQLITE_TEXT )
    {
        sqlite3_result_null (pContext);
        return;
    }
    const char* pszQuery = (const char*)sqlite3_value_text(argv[0]);

    CPLString osField = "geometry";
    if( argc >= 2 && sqlite3_value_type (argv[1]) == SQLITE_TEXT )
    {
        osField = (const char*)sqlite3_value_text(argv[1]);
    }

    int i;
    char** papszOptions = NULL;
    for(i = 2; i < argc; i++)
    {
        if( sqlite3_value_type (argv[i]) == SQLITE_TEXT )
        {
            papszOptions = CSLAddString(papszOptions,
                                        (const char*)sqlite3_value_text(argv[i]));
        }
    }

    OGRGeocodingSessionH hSession = poModule->GetGeocodingSession();
    if( hSession == NULL )
    {
        hSession = OGRGeocodeCreateSession(papszOptions);
        if( hSession == NULL )
        {
            sqlite3_result_null (pContext);
            CSLDestroy(papszOptions);
            return;
        }
        poModule->SetGeocodingSession(hSession);
    }

    if( osField == "raw" )
        papszOptions = CSLAddString(papszOptions, "RAW_FEATURE=YES");

    if( CSLFindString(papszOptions, "LIMIT") == -1 )
        papszOptions = CSLAddString(papszOptions, "LIMIT=1");

    OGRLayerH hLayer = OGRGeocode(hSession, pszQuery, NULL, papszOptions);

    OGR2SQLITE_ogr_geocode_set_result(pContext, hLayer, osField);

    CSLDestroy(papszOptions);

    return;
}

/************************************************************************/
/*                   OGR2SQLITE_ogr_geocode_reverse()                   */
/************************************************************************/

static
void OGR2SQLITE_ogr_geocode_reverse(sqlite3_context* pContext,
                                    int argc, sqlite3_value** argv)
{
    OGRSQLiteExtensionData* poModule =
                    (OGRSQLiteExtensionData*) sqlite3_user_data(pContext);

    double dfLon = 0.0, dfLat = 0.0;
    int iAfterGeomIdx = 0;

    if( argc >= 2 &&
        (sqlite3_value_type (argv[0]) == SQLITE_FLOAT ||
         sqlite3_value_type (argv[0]) == SQLITE_INTEGER) &&
        (sqlite3_value_type (argv[1]) == SQLITE_FLOAT ||
         sqlite3_value_type (argv[1]) == SQLITE_INTEGER) )
    {
        if( argc < 3 )
        {
            sqlite3_result_null (pContext);
            return;
        }
        dfLon = (sqlite3_value_type (argv[0]) != SQLITE_FLOAT) ?
            sqlite3_value_double(argv[0]) : sqlite3_value_int(argv[0]);
        dfLat = (sqlite3_value_type (argv[1]) != SQLITE_FLOAT) ?
            sqlite3_value_double(argv[1]) : sqlite3_value_int(argv[1]);
        iAfterGeomIdx = 2;
    }
    else if( argc >= 2 && 
             sqlite3_value_type (argv[0]) == SQLITE_BLOB &&
             sqlite3_value_type (argv[1]) == SQLITE_TEXT )
    {
        GByte* pabyBlob = (GByte *) sqlite3_value_blob (argv[0]);
        int nLen = sqlite3_value_bytes (argv[0]);
        OGRGeometry* poGeom = NULL;
        if( OGRSQLiteLayer::ImportSpatiaLiteGeometry(
                        pabyBlob, nLen, &poGeom ) == CE_None &&
            poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            OGRPoint* poPoint = (OGRPoint*) poGeom;
            dfLon = poPoint->getX();
            dfLat = poPoint->getY();
            delete poGeom;
        }
        else
        {
            delete poGeom;
            sqlite3_result_null (pContext);
            return;
        }
        iAfterGeomIdx = 1;
    }
    else
    {
        sqlite3_result_null (pContext);
        return;
    }

    const char* pszField = (const char*)sqlite3_value_text(argv[iAfterGeomIdx]);

    int i;
    char** papszOptions = NULL;
    for(i = iAfterGeomIdx + 1; i < argc; i++)
    {
        if( sqlite3_value_type (argv[i]) == SQLITE_TEXT )
        {
            papszOptions = CSLAddString(papszOptions,
                                        (const char*)sqlite3_value_text(argv[i]));
        }
    }

    OGRGeocodingSessionH hSession = poModule->GetGeocodingSession();
    if( hSession == NULL )
    {
        hSession = OGRGeocodeCreateSession(papszOptions);
        if( hSession == NULL )
        {
            sqlite3_result_null (pContext);
            CSLDestroy(papszOptions);
            return;
        }
        poModule->SetGeocodingSession(hSession);
    }

    if( strcmp(pszField, "raw") == 0 )
        papszOptions = CSLAddString(papszOptions, "RAW_FEATURE=YES");

    OGRLayerH hLayer = OGRGeocodeReverse(hSession, dfLon, dfLat, papszOptions);

    OGR2SQLITE_ogr_geocode_set_result(pContext, hLayer, pszField);

    CSLDestroy(papszOptions);

    return;
}

/************************************************************************/
/*               OGR2SQLITE_ogr_datasource_load_layers()                */
/************************************************************************/

static
void OGR2SQLITE_ogr_datasource_load_layers(sqlite3_context* pContext,
                                           int argc, sqlite3_value** argv)
{
    sqlite3* hDB = (sqlite3*) sqlite3_user_data(pContext);

    if( (argc < 1 || argc > 3) || sqlite3_value_type (argv[0]) != SQLITE_TEXT )
    {
        sqlite3_result_int (pContext, 0);
        return;
    }
    const char* pszDataSource = (const char*) sqlite3_value_text(argv[0]);

    int bUpdate = FALSE;
    if( argc >= 2 )
    {
        if( sqlite3_value_type(argv[1]) != SQLITE_INTEGER )
        {
            sqlite3_result_int (pContext, 0);
            return;
        }
        bUpdate = sqlite3_value_int(argv[1]);
    }

    const char* pszPrefix = NULL;
    if( argc >= 3 )
    {
        if( sqlite3_value_type(argv[2]) != SQLITE_TEXT )
        {
            sqlite3_result_int (pContext, 0);
            return;
        }
        pszPrefix = (const char*) sqlite3_value_text(argv[2]);
    }

    OGRDataSource* poDS = (OGRDataSource*)OGROpenShared(pszDataSource, bUpdate, NULL);
    if( poDS == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszDataSource);
        sqlite3_result_int (pContext, 0);
        return;
    }

    CPLString osEscapedDataSource = OGRSQLiteEscape(pszDataSource);
    for(int i=0;i<poDS->GetLayerCount();i++)
    {
        const char* pszLayerName = poDS->GetLayer(i)->GetName();
        CPLString osEscapedLayerName = OGRSQLiteEscape(pszLayerName);
        CPLString osTableName;
        if( pszPrefix != NULL )
        {
            osTableName = pszPrefix;
            osTableName += "_";
            osTableName += OGRSQLiteEscapeName(pszLayerName);
        }
        else
        {
            osTableName = OGRSQLiteEscapeName(pszLayerName);
        }

        char* pszErrMsg = NULL;
        if( sqlite3_exec(hDB, CPLSPrintf(
            "CREATE VIRTUAL TABLE \"%s\" USING VirtualOGR('%s', %d, '%s')",
                osTableName.c_str(),
                osEscapedDataSource.c_str(),
                bUpdate,
                osEscapedLayerName.c_str()),
            NULL, NULL, &pszErrMsg) != SQLITE_OK )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot create table \"%s\" : %s",
                     osTableName.c_str(), pszErrMsg);
            sqlite3_free(pszErrMsg);
        }
    }

    poDS->Release();
    sqlite3_result_int (pContext, 1);
}

#ifdef notdef
/************************************************************************/
/*                  OGR2SQLITE_ogr_GetConfigOption()                    */
/************************************************************************/

static
void OGR2SQLITE_ogr_GetConfigOption(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT )
    {
        sqlite3_result_null (pContext);
        return;
    }
    
    const char* pszKey = (const char*)sqlite3_value_text(argv[0]);
    const char* pszVal = CPLGetConfigOption(pszKey, NULL);
    if( pszVal == NULL )
        sqlite3_result_null (pContext);
    else
        sqlite3_result_text( pContext, pszVal, -1, SQLITE_TRANSIENT );
}

/************************************************************************/
/*                  OGR2SQLITE_ogr_SetConfigOption()                    */
/************************************************************************/

static
void OGR2SQLITE_ogr_SetConfigOption(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT )
    {
        sqlite3_result_null (pContext);
        return;
    }
    if( sqlite3_value_type (argv[1]) != SQLITE_TEXT &&
        sqlite3_value_type (argv[1]) != SQLITE_NULL )
    {
        sqlite3_result_null (pContext);
        return;
    }

    const char* pszKey = (const char*)sqlite3_value_text(argv[0]);
    const char* pszVal = (sqlite3_value_type (argv[1]) == SQLITE_TEXT) ?
        (const char*)sqlite3_value_text(argv[1]) : NULL;
    CPLSetConfigOption(pszKey, pszVal);
    sqlite3_result_null (pContext);
}
#endif // notdef

#ifdef MINIMAL_SPATIAL_FUNCTIONS

/************************************************************************/
/*                      OGR2SQLITE_GetGeom()                            */
/************************************************************************/

static OGRGeometry* OGR2SQLITE_GetGeom(sqlite3_context* pContext,
                                       int argc, sqlite3_value** argv,
                                       int* pnSRSId)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        return NULL;
    }

    GByte* pabySLBLOB = (GByte *) sqlite3_value_blob (argv[0]);
    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    OGRGeometry* poGeom = NULL;
    if( OGRSQLiteLayer::ImportSpatiaLiteGeometry(
                        pabySLBLOB, nBLOBLen, &poGeom, pnSRSId) != CE_None )
    {
        return NULL;
    }

    return poGeom;
}

/************************************************************************/
/*                     OGR2SQLITE_ST_AsText()                           */
/************************************************************************/

static
void OGR2SQLITE_ST_AsText(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, NULL);
    if( poGeom != NULL )
    {
        char* pszWKT = NULL;
        if( poGeom->exportToWkt(&pszWKT) == OGRERR_NONE )
            sqlite3_result_text( pContext, pszWKT, -1, CPLFree);
        else
            sqlite3_result_null (pContext);
        delete poGeom;
    }
    else
        sqlite3_result_null (pContext);
}

/************************************************************************/
/*                    OGR2SQLITE_ST_AsBinary()                          */
/************************************************************************/

static
void OGR2SQLITE_ST_AsBinary(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, NULL);
    if( poGeom != NULL )
    {
        int nBLOBLen = poGeom->WkbSize();
        GByte* pabyGeomBLOB = (GByte*) VSIMalloc(nBLOBLen);
        if( pabyGeomBLOB != NULL )
        {
            if( poGeom->exportToWkb(wkbNDR, pabyGeomBLOB) == OGRERR_NONE )
                sqlite3_result_blob( pContext, pabyGeomBLOB, nBLOBLen, CPLFree);
            else
            {
                VSIFree(pabyGeomBLOB);
                sqlite3_result_null (pContext);
            }
        }
        else
            sqlite3_result_null (pContext);
        delete poGeom;
    }
    else
        sqlite3_result_null (pContext);
}

/************************************************************************/
/*                OGR2SQLITE_SetGeom_AndDestroy()                       */
/************************************************************************/

static void OGR2SQLITE_SetGeom_AndDestroy(sqlite3_context* pContext,
                                          OGRGeometry* poGeom,
                                          int nSRSId)
{
    GByte* pabySLBLOB = NULL;
    int nBLOBLen = 0;
    if( poGeom != NULL && OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    poGeom, nSRSId, wkbNDR, FALSE,
                    FALSE, FALSE, &pabySLBLOB, &nBLOBLen ) == CE_None )
    {
        sqlite3_result_blob(pContext, pabySLBLOB, nBLOBLen, CPLFree);
    }
    else
    {
        sqlite3_result_null(pContext);
    }
    delete poGeom;
}

/************************************************************************/
/*                   OGR2SQLITE_ST_GeomFromText()                       */
/************************************************************************/

static
void OGR2SQLITE_ST_GeomFromText(sqlite3_context* pContext,
                                int argc, sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT )
    {
        sqlite3_result_null (pContext);
        return;
    }
    char* pszWKT = (char*) sqlite3_value_text( argv[0] );

    int nSRID = -1;
    if( argc == 2 && sqlite3_value_type (argv[1]) == SQLITE_INTEGER )
        nSRID = sqlite3_value_int( argv[1] );

    OGRGeometry* poGeom = NULL;
    if( OGRGeometryFactory::createFromWkt(&pszWKT, NULL, &poGeom) == OGRERR_NONE )
    {
        OGR2SQLITE_SetGeom_AndDestroy(pContext, poGeom, nSRID);
    }
    else
        sqlite3_result_null (pContext);
}

/************************************************************************/
/*                   OGR2SQLITE_ST_GeomFromWKB()                        */
/************************************************************************/

static
void OGR2SQLITE_ST_GeomFromWKB(sqlite3_context* pContext,
                                int argc, sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        sqlite3_result_null (pContext);
        return;
    }

    int nSRID = -1;
    if( argc == 2 && sqlite3_value_type (argv[1]) == SQLITE_INTEGER )
        nSRID = sqlite3_value_int( argv[1] );

    GByte* pabySLBLOB = (GByte *) sqlite3_value_blob (argv[0]);
    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    OGRGeometry* poGeom = NULL;

    if( OGRGeometryFactory::createFromWkb(pabySLBLOB, NULL, &poGeom, nBLOBLen)
            == OGRERR_NONE )
    {
        OGR2SQLITE_SetGeom_AndDestroy(pContext, poGeom, nSRID);
    }
    else
        sqlite3_result_null (pContext);
}

/************************************************************************/
/*                         CheckSTFunctions()                           */
/************************************************************************/

static int CheckSTFunctions(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv,
                            OGRGeometry** ppoGeom1,
                            OGRGeometry** ppoGeom2,
                            int *pnSRSId )
{
    *ppoGeom1 = NULL;
    *ppoGeom2 = NULL;

    if( argc != 2)
    {
        return FALSE;
    }

    *ppoGeom1 = OGR2SQLITE_GetGeom(pContext, argc, argv, pnSRSId);
    if( *ppoGeom1 == NULL )
        return FALSE;

    *ppoGeom2 = OGR2SQLITE_GetGeom(pContext, argc - 1, argv + 1, NULL);
    if( *ppoGeom2 == NULL )
    {
        delete *ppoGeom1;
        *ppoGeom1 = NULL;
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                 OGR2SQLITE_ST_int_geomgeom_op()                      */
/************************************************************************/

#define OGR2SQLITE_ST_int_geomgeom_op(op) \
static \
void OGR2SQLITE_ST_##op (sqlite3_context* pContext, \
                                int argc, sqlite3_value** argv) \
{ \
    OGRGeometry* poGeom1 = NULL; \
    OGRGeometry* poGeom2 = NULL; \
    if( !CheckSTFunctions(pContext, argc, argv, &poGeom1, &poGeom2, NULL) ) \
    { \
        sqlite3_result_int(pContext, 0); \
        return; \
    } \
 \
    sqlite3_result_int( pContext, poGeom1-> op (poGeom2) ); \
 \
    delete poGeom1; \
    delete poGeom2; \
}

OGR2SQLITE_ST_int_geomgeom_op(Intersects)
OGR2SQLITE_ST_int_geomgeom_op(Equals)
OGR2SQLITE_ST_int_geomgeom_op(Disjoint)
OGR2SQLITE_ST_int_geomgeom_op(Touches)
OGR2SQLITE_ST_int_geomgeom_op(Crosses)
OGR2SQLITE_ST_int_geomgeom_op(Within)
OGR2SQLITE_ST_int_geomgeom_op(Contains)
OGR2SQLITE_ST_int_geomgeom_op(Overlaps)

/************************************************************************/
/*                   OGR2SQLITE_ST_int_geom_op()                        */
/************************************************************************/

#define OGR2SQLITE_ST_int_geom_op(op) \
static \
void OGR2SQLITE_ST_##op (sqlite3_context* pContext, \
                                int argc, sqlite3_value** argv) \
{ \
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, NULL); \
    if( poGeom != NULL ) \
        sqlite3_result_int( pContext, poGeom-> op () ); \
    else \
        sqlite3_result_int( pContext, 0 ); \
 \
    delete poGeom; \
}

OGR2SQLITE_ST_int_geom_op(IsEmpty)
OGR2SQLITE_ST_int_geom_op(IsSimple)
OGR2SQLITE_ST_int_geom_op(IsValid)

/************************************************************************/
/*                  OGR2SQLITE_ST_geom_geomgeom_op()                    */
/************************************************************************/

#define OGR2SQLITE_ST_geom_geomgeom_op(op) \
static \
void OGR2SQLITE_ST_##op (sqlite3_context* pContext, \
                                int argc, sqlite3_value** argv) \
{ \
    OGRGeometry* poGeom1 = NULL; \
    OGRGeometry* poGeom2 = NULL; \
    int nSRSId = -1; \
    if( !CheckSTFunctions(pContext, argc, argv, &poGeom1, &poGeom2, &nSRSId) ) \
    { \
        sqlite3_result_null(pContext); \
        return; \
    } \
 \
    OGR2SQLITE_SetGeom_AndDestroy(pContext, \
                           poGeom1-> op (poGeom2), \
                           nSRSId); \
 \
    delete poGeom1; \
    delete poGeom2; \
}

OGR2SQLITE_ST_geom_geomgeom_op(Intersection)
OGR2SQLITE_ST_geom_geomgeom_op(Difference)
OGR2SQLITE_ST_geom_geomgeom_op(Union)
OGR2SQLITE_ST_geom_geomgeom_op(SymDifference)

/************************************************************************/
/*                      OGR2SQLITE_ST_SRID()                            */
/************************************************************************/

static
void OGR2SQLITE_ST_SRID(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    int nSRSId = -1;
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, &nSRSId);
    if( poGeom != NULL )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        sqlite3_result_int( pContext, nSRSId );
        CPLPopErrorHandler();
    }
    else
        sqlite3_result_null(pContext);
    delete poGeom;
}

/************************************************************************/
/*                      OGR2SQLITE_ST_Area()                            */
/************************************************************************/

static
void OGR2SQLITE_ST_Area(sqlite3_context* pContext,
                        int argc, sqlite3_value** argv)
{
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, NULL);
    if( poGeom != NULL )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        sqlite3_result_double( pContext, OGR_G_GetArea((OGRGeometryH)poGeom) );
        CPLPopErrorHandler();
    }
    else
        sqlite3_result_null(pContext);
    delete poGeom;
}

/************************************************************************/
/*                     OGR2SQLITE_ST_Buffer()                           */
/************************************************************************/

static
void OGR2SQLITE_ST_Buffer(sqlite3_context* pContext,
                          int argc, sqlite3_value** argv)
{
    int nSRSId = -1;
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, &nSRSId);
    if( poGeom != NULL && (
            sqlite3_value_type(argv[1]) == SQLITE_FLOAT ||
            sqlite3_value_type(argv[1]) == SQLITE_INTEGER) )
    {
        double dfDist = (sqlite3_value_type(argv[1]) == SQLITE_FLOAT) ?
            sqlite3_value_double(argv[1]) : sqlite3_value_int(argv[1]);
        OGR2SQLITE_SetGeom_AndDestroy(pContext, poGeom->Buffer(dfDist), nSRSId);
    }
    else
        sqlite3_result_null(pContext);
    delete poGeom;
}

#endif // #ifdef MINIMAL_SPATIAL_FUNCTIONS

/************************************************************************/
/*                   OGRSQLiteRegisterSQLFunctions()                    */
/************************************************************************/

static
void* OGRSQLiteRegisterSQLFunctions(sqlite3* hDB)
{
    OGRSQLiteExtensionData* pData = new OGRSQLiteExtensionData(hDB);

    sqlite3_create_function(hDB, "ogr_version", 0, SQLITE_ANY, NULL,
                           OGR2SQLITE_ogr_version, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_version", 1, SQLITE_ANY, NULL,
                           OGR2SQLITE_ogr_version, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_deflate", 1, SQLITE_ANY, NULL,
                            OGR2SQLITE_ogr_deflate, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_deflate", 2, SQLITE_ANY, NULL,
                            OGR2SQLITE_ogr_deflate, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_inflate", 1, SQLITE_ANY, NULL,
                            OGR2SQLITE_ogr_inflate, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_geocode", -1, SQLITE_ANY, pData,
                            OGR2SQLITE_ogr_geocode, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_geocode_reverse", -1, SQLITE_ANY, pData,
                            OGR2SQLITE_ogr_geocode_reverse, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_datasource_load_layers", 1, SQLITE_ANY, hDB,
                            OGR2SQLITE_ogr_datasource_load_layers, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_datasource_load_layers", 2, SQLITE_ANY, hDB,
                            OGR2SQLITE_ogr_datasource_load_layers, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_datasource_load_layers", 3, SQLITE_ANY, hDB,
                            OGR2SQLITE_ogr_datasource_load_layers, NULL, NULL);

#if notdef
    sqlite3_create_function(hDB, "ogr_GetConfigOption", 1, SQLITE_ANY, NULL,
                            OGR2SQLITE_ogr_GetConfigOption, NULL, NULL);

    sqlite3_create_function(hDB, "ogr_SetConfigOption", 2, SQLITE_ANY, NULL,
                            OGR2SQLITE_ogr_SetConfigOption, NULL, NULL);
#endif

    // Custom and undocumented function, not sure I'll keep it.
    sqlite3_create_function(hDB, "Transform3", 3, SQLITE_ANY, pData,
                            OGR2SQLITE_Transform, NULL, NULL);

#ifdef MINIMAL_SPATIAL_FUNCTIONS
    /* Check if spatialite is available */
    int rc = sqlite3_exec(hDB, "SELECT spatialite_version()", NULL, NULL, NULL);
    if( rc != SQLITE_OK &&
        CSLTestBoolean(CPLGetConfigOption("OGR_SQLITE_SPATIAL_FUNCTIONS", "YES")) )
    {
        CPLDebug("SQLITE",
                 "Spatialite not available. Implementing a few functions");

#define REGISTER_ST_op(argc, op) \
        sqlite3_create_function(hDB, #op, argc, SQLITE_ANY, NULL, \
                                OGR2SQLITE_ST_##op, NULL, NULL); \
        sqlite3_create_function(hDB, "ST_" #op, argc, SQLITE_ANY, NULL, \
                                OGR2SQLITE_ST_##op, NULL, NULL);

        REGISTER_ST_op(1, AsText);
        REGISTER_ST_op(1, AsBinary);
        REGISTER_ST_op(1, GeomFromText);
        REGISTER_ST_op(2, GeomFromText);
        REGISTER_ST_op(1, GeomFromWKB);
        REGISTER_ST_op(2, GeomFromWKB);

        REGISTER_ST_op(1, IsEmpty);
        REGISTER_ST_op(1, IsSimple);
        REGISTER_ST_op(1, IsValid);

        REGISTER_ST_op(2, Intersects);
        REGISTER_ST_op(2, Equals);
        REGISTER_ST_op(2, Disjoint);
        REGISTER_ST_op(2, Touches);
        REGISTER_ST_op(2, Crosses);
        REGISTER_ST_op(2, Within);
        REGISTER_ST_op(2, Contains);
        REGISTER_ST_op(2, Overlaps);

        REGISTER_ST_op(2, Intersection);
        REGISTER_ST_op(2, Difference);
        // Union() is invalid
        sqlite3_create_function(hDB, "ST_Union", 2, SQLITE_ANY, NULL,
                                OGR2SQLITE_ST_Union, NULL, NULL);
        REGISTER_ST_op(2, SymDifference);

        REGISTER_ST_op(1, SRID);
        REGISTER_ST_op(1, Area);
        REGISTER_ST_op(2, Buffer);
    }
#endif // #ifdef MINIMAL_SPATIAL_FUNCTIONS

    pData->SetRegExpCache(OGRSQLiteRegisterRegExpFunction(hDB));

    return pData;
}

/************************************************************************/
/*                   OGRSQLiteUnregisterSQLFunctions()                  */
/************************************************************************/

static
void OGRSQLiteUnregisterSQLFunctions(void* hHandle)
{
    OGRSQLiteExtensionData* pData = (OGRSQLiteExtensionData* )hHandle;
    delete pData;
}
