/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Extension SQL functions
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2012-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_swq.h"

#include <limits>

CPL_CVSID("$Id$")

#undef SQLITE_STATIC
#define SQLITE_STATIC      ((sqlite3_destructor_type)nullptr)


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
    explicit                     OGRSQLiteExtensionData(sqlite3* hDB);
                                ~OGRSQLiteExtensionData();

    OGRCoordinateTransformation* GetTransform(int nSrcSRSId, int nDstSRSId);

    OGRGeocodingSessionH         GetGeocodingSession() { return hGeocodingSession; }
    void                         SetGeocodingSession(OGRGeocodingSessionH hGeocodingSessionIn) { hGeocodingSession = hGeocodingSessionIn; }

    void                         SetRegExpCache(void* hRegExpCacheIn) { hRegExpCache = hRegExpCacheIn; }
};

/************************************************************************/
/*                     OGRSQLiteExtensionData()                         */
/************************************************************************/

OGRSQLiteExtensionData::OGRSQLiteExtensionData(CPL_UNUSED sqlite3* hDB) :
#ifdef DEBUG
    pDummy(CPLMalloc(1)),
#endif
    hRegExpCache(nullptr),
    hGeocodingSession(nullptr)
{}

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
        OGRCoordinateTransformation* poCT = nullptr;
        OGRSpatialReference oSrcSRS, oDstSRS;
        oSrcSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        oDstSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
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
        sqlite3_result_text( pContext, GDALVersionInfo("RELEASE_NAME"), -1,
                             SQLITE_TRANSIENT );
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
    if( poCT == nullptr )
    {
        sqlite3_result_null (pContext);
        return;
    }

    GByte* pabySLBLOB = (GByte *) sqlite3_value_blob (argv[0]);
    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    OGRGeometry* poGeom = nullptr;
    if( OGRSQLiteLayer::ImportSpatiaLiteGeometry(
                    pabySLBLOB, nBLOBLen, &poGeom ) == OGRERR_NONE &&
        poGeom->transform(poCT) == OGRERR_NONE &&
        OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    poGeom, nDstSRSId, wkbNDR, FALSE,
                    FALSE, &pabySLBLOB, &nBLOBLen ) == OGRERR_NONE )
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
    void* pOut = nullptr;
    if( sqlite3_value_type (argv[0]) == SQLITE_TEXT )
    {
        const char* pszVal = (const char*)sqlite3_value_text(argv[0]);
        pOut = CPLZLibDeflate( pszVal, strlen(pszVal) + 1, nLevel, nullptr, 0, &nOutBytes);
    }
    else
    {
        const void* pSrc = sqlite3_value_blob (argv[0]);
        int nLen = sqlite3_value_bytes (argv[0]);
        pOut = CPLZLibDeflate( pSrc, nLen, nLevel, nullptr, 0, &nOutBytes);
    }
    if( pOut != nullptr )
    {
        sqlite3_result_blob (pContext, pOut, static_cast<int>(nOutBytes), VSIFree);
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

    const void* pSrc = sqlite3_value_blob (argv[0]);
    int nLen = sqlite3_value_bytes (argv[0]);
    void* pOut = CPLZLibInflate( pSrc, nLen, nullptr, 0, &nOutBytes);

    if( pOut != nullptr )
    {
        sqlite3_result_blob (pContext, pOut, static_cast<int>(nOutBytes), VSIFree);
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
    if( hLayer == nullptr )
        sqlite3_result_null (pContext);
    else
    {
        OGRLayer* poLayer = (OGRLayer*)hLayer;
        OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
        OGRFeature* poFeature = poLayer->GetNextFeature();
        int nIdx;
        if( poFeature == nullptr )
            sqlite3_result_null (pContext);
        else if( strcmp(pszField, "geometry") == 0 &&
                 poFeature->GetGeometryRef() != nullptr )
        {
            GByte* pabyGeomBLOB = nullptr;
            int nGeomBLOBLen = 0;
            if( OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    poFeature->GetGeometryRef(), 4326, wkbNDR, FALSE, FALSE,
                    &pabyGeomBLOB,
                    &nGeomBLOBLen ) != OGRERR_NONE )
            {
                sqlite3_result_null (pContext);
            }
            else
            {
                sqlite3_result_blob (pContext, pabyGeomBLOB, nGeomBLOBLen, CPLFree);
            }
        }
        else if( (nIdx = poFDefn->GetFieldIndex(pszField)) >= 0 &&
                 poFeature->IsFieldSetAndNotNull(nIdx) )
        {
            OGRFieldType eType = poFDefn->GetFieldDefn(nIdx)->GetType();
            if( eType == OFTInteger )
                sqlite3_result_int(pContext,
                                   poFeature->GetFieldAsInteger(nIdx));
            else if( eType == OFTInteger64 )
                sqlite3_result_int64(pContext,
                                   poFeature->GetFieldAsInteger64(nIdx));
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

    char** papszOptions = nullptr;
    for( int i = 2; i < argc; i++ )
    {
        if( sqlite3_value_type (argv[i]) == SQLITE_TEXT )
        {
            papszOptions = CSLAddString(papszOptions,
                                        (const char*)sqlite3_value_text(argv[i]));
        }
    }

    OGRGeocodingSessionH hSession = poModule->GetGeocodingSession();
    if( hSession == nullptr )
    {
        hSession = OGRGeocodeCreateSession(papszOptions);
        if( hSession == nullptr )
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

    OGRLayerH hLayer = OGRGeocode(hSession, pszQuery, nullptr, papszOptions);

    OGR2SQLITE_ogr_geocode_set_result(pContext, hLayer, osField);

    CSLDestroy(papszOptions);

    return;
}

/************************************************************************/
/*                    OGR2SQLITE_GetValAsDouble()                       */
/************************************************************************/

static double OGR2SQLITE_GetValAsDouble(sqlite3_value* val, int* pbGotVal)
{
    switch(sqlite3_value_type(val))
    {
        case SQLITE_FLOAT:
            if( pbGotVal ) *pbGotVal = TRUE;
            return sqlite3_value_double(val);

        case SQLITE_INTEGER:
            if( pbGotVal ) *pbGotVal = TRUE;
            return (double) sqlite3_value_int64(val);

        default:
            if( pbGotVal ) *pbGotVal = FALSE;
            return 0.0;
    }
}

/************************************************************************/
/*                      OGR2SQLITE_GetGeom()                            */
/************************************************************************/

static OGRGeometry* OGR2SQLITE_GetGeom(CPL_UNUSED sqlite3_context* pContext,
                                       CPL_UNUSED int argc,
                                       sqlite3_value** argv,
                                       int* pnSRSId)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_BLOB )
    {
        return nullptr;
    }

    GByte* pabySLBLOB = (GByte *) sqlite3_value_blob (argv[0]);
    int nBLOBLen = sqlite3_value_bytes (argv[0]);
    OGRGeometry* poGeom = nullptr;
    if( OGRSQLiteLayer::ImportSpatiaLiteGeometry(
                        pabySLBLOB, nBLOBLen, &poGeom, pnSRSId) != OGRERR_NONE )
    {
        if( poGeom != nullptr )
            delete poGeom;
        return nullptr;
    }

    return poGeom;
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

    double dfLon = 0.0;
    double dfLat = 0.0;
    int iAfterGeomIdx = 0;
    int bGotLon = FALSE;
    int bGotLat = FALSE;

    if( argc >= 2 )
    {
        dfLon = OGR2SQLITE_GetValAsDouble(argv[0], &bGotLon);
        dfLat = OGR2SQLITE_GetValAsDouble(argv[1], &bGotLat);
    }

    if( argc >= 3 && bGotLon && bGotLat &&
        sqlite3_value_type (argv[2]) == SQLITE_TEXT )
    {
        iAfterGeomIdx = 2;
    }
    else if( argc >= 2 &&
             sqlite3_value_type (argv[0]) == SQLITE_BLOB &&
             sqlite3_value_type (argv[1]) == SQLITE_TEXT )
    {
        OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, nullptr);
        if( poGeom != nullptr && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
        {
            OGRPoint* poPoint = poGeom->toPoint();
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

    char** papszOptions = nullptr;
    for( int i = iAfterGeomIdx + 1; i < argc; i++ )
    {
        if( sqlite3_value_type (argv[i]) == SQLITE_TEXT )
        {
            papszOptions = CSLAddString(papszOptions,
                                        (const char*)sqlite3_value_text(argv[i]));
        }
    }

    OGRGeocodingSessionH hSession = poModule->GetGeocodingSession();
    if( hSession == nullptr )
    {
        hSession = OGRGeocodeCreateSession(papszOptions);
        if( hSession == nullptr )
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

    const char* pszPrefix = nullptr;
    if( argc >= 3 )
    {
        if( sqlite3_value_type(argv[2]) != SQLITE_TEXT )
        {
            sqlite3_result_int (pContext, 0);
            return;
        }
        pszPrefix = (const char*) sqlite3_value_text(argv[2]);
    }

    OGRDataSource* poDS = (OGRDataSource*)OGROpenShared(pszDataSource, bUpdate, nullptr);
    if( poDS == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszDataSource);
        sqlite3_result_int (pContext, 0);
        return;
    }

    CPLString osEscapedDataSource = SQLEscapeLiteral(pszDataSource);
    for(int i=0;i<poDS->GetLayerCount();i++)
    {
        const char* pszLayerName = poDS->GetLayer(i)->GetName();
        CPLString osEscapedLayerName = SQLEscapeLiteral(pszLayerName);
        CPLString osTableName;
        if( pszPrefix != nullptr )
        {
            osTableName = pszPrefix;
            osTableName += "_";
            osTableName += SQLEscapeName(pszLayerName);
        }
        else
        {
            osTableName = SQLEscapeName(pszLayerName);
        }

        SQLCommand(hDB, CPLSPrintf(
            "CREATE VIRTUAL TABLE \"%s\" USING VirtualOGR('%s', %d, '%s')",
                osTableName.c_str(),
                osEscapedDataSource.c_str(),
                bUpdate,
                osEscapedLayerName.c_str()));
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
    const char* pszVal = CPLGetConfigOption(pszKey, nullptr);
    if( pszVal == nullptr )
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
        (const char*)sqlite3_value_text(argv[1]) : nullptr;
    CPLSetConfigOption(pszKey, pszVal);
    sqlite3_result_null (pContext);
}
#endif // notdef

/************************************************************************/
/*                OGR2SQLITE_SetGeom_AndDestroy()                       */
/************************************************************************/

static void OGR2SQLITE_SetGeom_AndDestroy(sqlite3_context* pContext,
                                          OGRGeometry* poGeom,
                                          int nSRSId)
{
    GByte* pabySLBLOB = nullptr;
    int nBLOBLen = 0;
    if( poGeom != nullptr && OGRSQLiteLayer::ExportSpatiaLiteGeometry(
                    poGeom, nSRSId, wkbNDR,
                    FALSE, FALSE, &pabySLBLOB, &nBLOBLen ) == OGRERR_NONE )
    {
        sqlite3_result_blob(pContext, pabySLBLOB, nBLOBLen, CPLFree);
    }
    else
    {
        sqlite3_result_null(pContext);
    }
    delete poGeom;
}

#ifdef MINIMAL_SPATIAL_FUNCTIONS

/************************************************************************/
/*                     OGR2SQLITE_ST_AsText()                           */
/************************************************************************/

static
void OGR2SQLITE_ST_AsText(sqlite3_context* pContext,
                            int argc, sqlite3_value** argv)
{
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, nullptr);
    if( poGeom != nullptr )
    {
        char* pszWKT = nullptr;
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
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, nullptr);
    if( poGeom != nullptr )
    {
        const size_t nBLOBLen = poGeom->WkbSize();
        if( nBLOBLen > static_cast<size_t>(std::numeric_limits<int>::max()) )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Too large geometry");
            sqlite3_result_null (pContext);
            return;
        }
        GByte* pabyGeomBLOB = (GByte*) VSI_MALLOC_VERBOSE(nBLOBLen);
        if( pabyGeomBLOB != nullptr )
        {
            if( poGeom->exportToWkb(wkbNDR, pabyGeomBLOB) == OGRERR_NONE )
                sqlite3_result_blob( pContext, pabyGeomBLOB,
                                     static_cast<int>(nBLOBLen), CPLFree);
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
    const char* pszWKT =
        reinterpret_cast<const char*>(sqlite3_value_text( argv[0] ));

    int nSRID = -1;
    if( argc == 2 && sqlite3_value_type (argv[1]) == SQLITE_INTEGER )
        nSRID = sqlite3_value_int( argv[1] );

    OGRGeometry* poGeom = nullptr;
    if( OGRGeometryFactory::createFromWkt(pszWKT, nullptr, &poGeom) == OGRERR_NONE )
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
    OGRGeometry* poGeom = nullptr;

    if( OGRGeometryFactory::createFromWkb(pabySLBLOB, nullptr, &poGeom, nBLOBLen)
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
    *ppoGeom1 = nullptr;
    *ppoGeom2 = nullptr;

    if( argc != 2)
    {
        return FALSE;
    }

    *ppoGeom1 = OGR2SQLITE_GetGeom(pContext, argc, argv, pnSRSId);
    if( *ppoGeom1 == nullptr )
        return FALSE;

    *ppoGeom2 = OGR2SQLITE_GetGeom(pContext, argc - 1, argv + 1, nullptr);
    if( *ppoGeom2 == nullptr )
    {
        delete *ppoGeom1;
        *ppoGeom1 = nullptr;
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
    OGRGeometry* poGeom1 = nullptr; \
    OGRGeometry* poGeom2 = nullptr; \
    if( !CheckSTFunctions(pContext, argc, argv, &poGeom1, &poGeom2, nullptr) ) \
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
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, nullptr); \
    if( poGeom != nullptr ) \
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
    OGRGeometry* poGeom1 = nullptr; \
    OGRGeometry* poGeom2 = nullptr; \
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
    if( poGeom != nullptr )
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
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, nullptr);
    if( poGeom != nullptr )
    {
        CPLPushErrorHandler(CPLQuietErrorHandler);
        sqlite3_result_double( pContext, OGR_G_Area((OGRGeometryH)poGeom) );
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
    int bGotVal = FALSE;
    double dfDist = OGR2SQLITE_GetValAsDouble(argv[1], &bGotVal);
    if( poGeom != nullptr && bGotVal )
        OGR2SQLITE_SetGeom_AndDestroy(pContext, poGeom->Buffer(dfDist), nSRSId);
    else
        sqlite3_result_null(pContext);
    delete poGeom;
}

/************************************************************************/
/*                    OGR2SQLITE_ST_MakePoint()                         */
/************************************************************************/

static
void OGR2SQLITE_ST_MakePoint(sqlite3_context* pContext,
                             int argc, sqlite3_value** argv)
{
    double dfY = 0.0;
    int bGotVal = FALSE;
    const double dfX = OGR2SQLITE_GetValAsDouble(argv[0], &bGotVal);
    if( bGotVal )
        dfY = OGR2SQLITE_GetValAsDouble(argv[1], &bGotVal);
    if( !bGotVal )
    {
        sqlite3_result_null(pContext);
        return;
    }

    OGRPoint* poPoint = nullptr;
    if( argc == 3 )
    {
        double dfZ = OGR2SQLITE_GetValAsDouble(argv[2], &bGotVal);
        if( !bGotVal )
        {
            sqlite3_result_null(pContext);
            return;
        }

        poPoint = new OGRPoint(dfX, dfY, dfZ);
    }
    else
    {
        poPoint = new OGRPoint(dfX, dfY);
    }

    OGR2SQLITE_SetGeom_AndDestroy(pContext, poPoint, -1);
}

#endif // #ifdef MINIMAL_SPATIAL_FUNCTIONS

/************************************************************************/
/*                    OGR2SQLITE_ST_MakeValid()                         */
/************************************************************************/

static
void OGR2SQLITE_ST_MakeValid(sqlite3_context* pContext,
                          int argc, sqlite3_value** argv)
{
    int nSRSId = -1;
    OGRGeometry* poGeom = OGR2SQLITE_GetGeom(pContext, argc, argv, &nSRSId);
    if( poGeom != nullptr )
        OGR2SQLITE_SetGeom_AndDestroy(pContext, poGeom->MakeValid(), nSRSId);
    else
        sqlite3_result_null(pContext);
    delete poGeom;
}

/************************************************************************/
/*                     OGRSQLITE_hstore_get_value()                     */
/************************************************************************/

static
void OGRSQLITE_hstore_get_value(sqlite3_context* pContext,
                                CPL_UNUSED int argc,
                                sqlite3_value** argv)
{
    if( sqlite3_value_type (argv[0]) != SQLITE_TEXT ||
        sqlite3_value_type (argv[1]) != SQLITE_TEXT )
    {
        sqlite3_result_null (pContext);
        return;
    }

    const char* pszHStore = (const char*)sqlite3_value_text(argv[0]);
    const char* pszSearchedKey = (const char*)sqlite3_value_text(argv[1]);
    char* pszValue = OGRHStoreGetValue(pszHStore, pszSearchedKey);
    if( pszValue != nullptr )
        sqlite3_result_text( pContext, pszValue, -1, CPLFree );
    else
        sqlite3_result_null( pContext );
}

/************************************************************************/
/*                   OGRSQLiteRegisterSQLFunctions()                    */
/************************************************************************/

#ifndef SQLITE_DETERMINISTIC
#define SQLITE_DETERMINISTIC 0
#endif

static
void* OGRSQLiteRegisterSQLFunctions(sqlite3* hDB)
{
    OGRSQLiteExtensionData* pData = new OGRSQLiteExtensionData(hDB);

    sqlite3_create_function(hDB, "ogr_version", 0,
                            SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                            OGR2SQLITE_ogr_version, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_version", 1,
                            SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                            OGR2SQLITE_ogr_version, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_deflate", 1,
                            SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                            OGR2SQLITE_ogr_deflate, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_deflate", 2,
                            SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                            OGR2SQLITE_ogr_deflate, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_inflate", 1,
                            SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                            OGR2SQLITE_ogr_inflate, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_geocode", -1,
                            SQLITE_UTF8, pData,
                            OGR2SQLITE_ogr_geocode, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_geocode_reverse", -1,
                            SQLITE_UTF8, pData,
                            OGR2SQLITE_ogr_geocode_reverse, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_datasource_load_layers", 1,
                            SQLITE_UTF8, hDB,
                            OGR2SQLITE_ogr_datasource_load_layers, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_datasource_load_layers", 2,
                            SQLITE_UTF8, hDB,
                            OGR2SQLITE_ogr_datasource_load_layers, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_datasource_load_layers", 3,
                            SQLITE_UTF8, hDB,
                            OGR2SQLITE_ogr_datasource_load_layers, nullptr, nullptr);

#if notdef
    sqlite3_create_function(hDB, "ogr_GetConfigOption", 1,
                            SQLITE_UTF8, nullptr,
                            OGR2SQLITE_ogr_GetConfigOption, nullptr, nullptr);

    sqlite3_create_function(hDB, "ogr_SetConfigOption", 2,
                            SQLITE_UTF8, nullptr,
                            OGR2SQLITE_ogr_SetConfigOption, nullptr, nullptr);
#endif

    // Custom and undocumented function, not sure I'll keep it.
    sqlite3_create_function(hDB, "Transform3", 3,
                            SQLITE_UTF8 | SQLITE_DETERMINISTIC, pData,
                            OGR2SQLITE_Transform, nullptr, nullptr);

    // HSTORE functions
    sqlite3_create_function(hDB, "hstore_get_value", 2,
                            SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr,
                            OGRSQLITE_hstore_get_value, nullptr, nullptr);

    /* Check if spatialite is available */
    int rc = sqlite3_exec(hDB, "SELECT spatialite_version()", nullptr, nullptr, nullptr);

    /* Reset error flag */
    sqlite3_exec(hDB, "SELECT 1", nullptr, nullptr, nullptr);

    const bool bSpatialiteAvailable = rc == SQLITE_OK;
    const bool bAllowOGRSQLiteSpatialFunctions =
        CPLTestBool(CPLGetConfigOption("OGR_SQLITE_SPATIAL_FUNCTIONS", "YES"));

#define REGISTER_ST_op(argc, op) \
        sqlite3_create_function(hDB, #op, argc, \
                                SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, \
                                OGR2SQLITE_ST_##op, nullptr, nullptr); \
        sqlite3_create_function(hDB, "ST_" #op, argc, \
                                SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, \
                                OGR2SQLITE_ST_##op, nullptr, nullptr);

#ifdef MINIMAL_SPATIAL_FUNCTIONS
    if( !bSpatialiteAvailable && bAllowOGRSQLiteSpatialFunctions )
    {
        static const auto DebugOnce = []() {
            CPLDebug("SQLITE",
                 "Spatialite not available. Implementing a few functions");
            return true;
        }();
        CPL_IGNORE_RET_VAL(DebugOnce);

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
        sqlite3_create_function(hDB, "ST_Union", 2, SQLITE_ANY, nullptr,
                                OGR2SQLITE_ST_Union, nullptr, nullptr);
        REGISTER_ST_op(2, SymDifference);

        REGISTER_ST_op(1, SRID);
        REGISTER_ST_op(1, Area);
        REGISTER_ST_op(2, Buffer);
        REGISTER_ST_op(2, MakePoint);
        REGISTER_ST_op(3, MakePoint);
    }
#endif // #ifdef MINIMAL_SPATIAL_FUNCTIONS

    if( bAllowOGRSQLiteSpatialFunctions )
    {
        static bool gbRegisterMakeValid = [bSpatialiteAvailable, hDB]() {
            bool bRegisterMakeValid = false;
            if( bSpatialiteAvailable )
            {
                // ST_MakeValid() only available (at time of writing) in
                // Spatialite builds against (GPL) liblwgeom
                // In the future, if they use GEOS 3.8 MakeValid, we could
                // get rid of this.
                int l_rc = sqlite3_exec(hDB,
                    "SELECT ST_MakeValid(ST_GeomFromText('POINT (0 0)'))",
                    nullptr, nullptr, nullptr);

                /* Reset error flag */
                sqlite3_exec(hDB, "SELECT 1", nullptr, nullptr, nullptr);

                bRegisterMakeValid = (l_rc != SQLITE_OK);
            }
            else
            {
                bRegisterMakeValid = true;
            }
            if( bRegisterMakeValid )
            {
                OGRPoint p(0, 0);
                CPLErrorStateBackuper oBackuper;
                CPLErrorHandlerPusher oPusher(CPLQuietErrorHandler);
                auto validGeom = std::unique_ptr<OGRGeometry>(p.MakeValid());
                return validGeom != nullptr;
            }
            return false;
        }();
        if( gbRegisterMakeValid )
        {
            REGISTER_ST_op(1, MakeValid);
        }
    }

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
