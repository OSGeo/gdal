/******************************************************************************
 * $Id$
 *
 * Project:  Google Maps Engine API Driver
 * Purpose:  OGRGMELayer Implementation.
 * Author:   Wolf Beregnheim <wolf+grass@bergenheim.net>
 *           Frank Warmerdam <warmerdam@pobox.com>
 *           (derived from GFT driver by Even)
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_gme.h"
#include "ogrgmejson.h"

#include "cpl_multiproc.h"

CPL_CVSID("$Id: ogrgmetablelayer.cpp 25475 2013-01-09 09:09:59Z warmerdam $");

/************************************************************************/
/*                            OGRGMELayer()                             */
/************************************************************************/

OGRGMELayer::OGRGMELayer(OGRGMEDataSource* poDS,
                         const char* pszTableId)

{
    CPLDebug("GME", "Opening existing layer %s", pszTableId);
    this->poDS = poDS;
    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);
    poFeatureDefn = NULL;
    current_feature_page = NULL;
    bDirty = false;
    iBatchPatchSize = 50;
    bCreateTablePending = false;
    osTableId = pszTableId;
    bInTransaction = false;
    m_poFilterGeom = NULL;
    iGxIdField = -1;
    SetDescription( pszTableId );
}


OGRGMELayer::OGRGMELayer(OGRGMEDataSource* poDS,
                         const char* pszTableName,
                         char ** papszOptions)

{
    CPLDebug("GME", "Creating new layer %s", pszTableName);
    this->poDS = poDS;
    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);
    poFeatureDefn = NULL;
    current_feature_page = NULL;
    bDirty = false;
    iBatchPatchSize = 50;
    bCreateTablePending = true;
    osTableName = pszTableName;
    osProjectId = CSLFetchNameValue( papszOptions, "projectId" );
    osDraftACL = CSLFetchNameValueDef( papszOptions, "draftAccessList", "Map Editors" );
    osPublishedACL = CSLFetchNameValueDef( papszOptions, "publishedAccessList", "Map Viewers" );
    iGxIdField = -1;
    SetDescription( pszTableName );
    // TODO: support tags and description
}

/************************************************************************/
/*                            ~OGRGMELayer()                            */
/************************************************************************/

OGRGMELayer::~OGRGMELayer()

{
    SyncToDisk();
    ResetReading();
    if( poSRS )
        poSRS->Release();
    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGMELayer::ResetReading()

{
    if (current_feature_page != NULL)
    {
        json_object_put(current_feature_page);
        current_feature_page = NULL;
        m_nFeaturesRead = 0;

        // TODO - clear current page.
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMELayer::TestCapability( const char * pszCap )

{
    if(EQUAL(pszCap,OLCStringsAsUTF8))
        return TRUE;
    else if(EQUAL(pszCap,OLCIgnoreFields))
        return TRUE;
    else if(EQUAL(pszCap,OLCFastSpatialFilter))
        return TRUE;
    else if(EQUAL(pszCap,OLCSequentialWrite))
        return TRUE;
    else if(EQUAL(pszCap,OLCRandomWrite))
        return TRUE;
    else if(EQUAL(pszCap,OLCDeleteFeature))
        return TRUE;
    else if(EQUAL(pszCap,OLCTransactions))
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRGMELayer::SyncToDisk()

{
    CPLDebug("GME", "SyncToDisk()");
    if (bDirty) {
        if (omnpoInsertedFeatures.size() > 0) {
            BatchInsert();
        }
        if (omnpoUpdatedFeatures.size() > 0) {
            BatchPatch();
        }
        if (oListOfDeletedFeatures.size() > 0) {
            BatchDelete();
        }
        bDirty = false;
    }
    return OGRERR_NONE;
}

/************************************************************************/
/*                           FetchDescribe()                            */
/************************************************************************/

int OGRGMELayer::FetchDescribe()
{
    CPLString osRequest = "tables/" + osTableId;

    CPLHTTPResult *psDescribe = poDS->MakeRequest(osRequest);
    if (psDescribe == NULL)
        return FALSE;

    CPLDebug("GME", "table doc = %s\n", psDescribe->pabyData);

    json_object *table_doc =
        OGRGMEParseJSON((const char *) psDescribe->pabyData);

    CPLHTTPDestroyResult(psDescribe);

    osTableName = OGRGMEGetJSONString(table_doc, "name");

    poFeatureDefn = new OGRFeatureDefn(osTableName);
    poFeatureDefn->Reference();

    json_object *schema_doc = json_object_object_get(table_doc, "schema");
    json_object *columns_doc = json_object_object_get(schema_doc, "columns");
    array_list *column_list = json_object_get_array(columns_doc);

    CPLString osLastGeomColumn;

    int field_count = array_list_length(column_list);
    for( int i = 0; i < field_count; i++ )
    {
        OGRwkbGeometryType eFieldGeomType = wkbNone;

        json_object *field_obj = (json_object*)
            array_list_get_idx(column_list, i);

	const char* name = OGRGMEGetJSONString(field_obj, "name");
        OGRFieldDefn oFieldDefn(name, OFTString);
        const char *type = OGRGMEGetJSONString(field_obj, "type");

        if (EQUAL(type, "integer"))
            oFieldDefn.SetType(OFTInteger);
        else if (EQUAL(type, "double"))
            oFieldDefn.SetType(OFTReal);
        else if (EQUAL(type, "boolean"))
            oFieldDefn.SetType(OFTInteger);
        else if (EQUAL(type, "string"))
            oFieldDefn.SetType(OFTString);
        else if (EQUAL(type, "string")) {
            if (EQUAL(name, "gx_id")) {
                iGxIdField = i;
            }
            oFieldDefn.SetType(OFTString);
        }
        else if (EQUAL(type, "points"))
            eFieldGeomType = wkbPoint;
        else if (EQUAL(type, "linestrings"))
            eFieldGeomType = wkbLineString;
        else if (EQUAL(type, "polygons"))
            eFieldGeomType = wkbPolygon;
        else if (EQUAL(type, "mixedGeometry"))
            eFieldGeomType = wkbGeometryCollection;

        if (eFieldGeomType == wkbNone)
        {
            poFeatureDefn->AddFieldDefn(&oFieldDefn);
        }
        else
        {
            CPLAssert(EQUAL(osGeomColumnName,""));
            osGeomColumnName = oFieldDefn.GetNameRef();
            poFeatureDefn->SetGeomType(eFieldGeomType);
            poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        }
    }

    json_object_put(table_doc);
    return TRUE;
}

/************************************************************************/
/*                         GetPageOfFeatures()                          */
/************************************************************************/

void OGRGMELayer::GetPageOfFeatures()
{
    CPLString osNextPageToken;

    if (current_feature_page != NULL)
    {
        osNextPageToken = OGRGMEGetJSONString(current_feature_page,
                                              "nextPageToken", "");
        json_object_put(current_feature_page);
        current_feature_page = NULL;

        // End of query results?
        if (EQUAL(osNextPageToken,""))
            return;
    }

    index_in_page = 0;
    current_features_array = NULL;

/* -------------------------------------------------------------------- */
/*      Fetch features.                                                 */
/* -------------------------------------------------------------------- */
    CPLString osRequest = "tables/" + osTableId + "/features";
    CPLString osMoreOptions = "&maxResults=1000";

    if (!EQUAL(osNextPageToken,""))
    {
        osMoreOptions += "&pageToken=";
        osMoreOptions += osNextPageToken;
    }
    if (!osSelect.empty()) {
        CPLDebug( "GME", "found select=%s", osSelect.c_str());
        osMoreOptions += "&select=";
        osMoreOptions += osSelect;
    }
    if (!osWhere.empty()) {
        CPLDebug( "GME Layer", "found where=%s", osWhere.c_str());
        osMoreOptions += "&where=";
        osMoreOptions += osWhere;
    }

    if (!osIntersects.empty()) {
        CPLDebug( "GME Layer", "found intersects=%s", osIntersects.c_str());
        osMoreOptions += "&intersects=";
        osMoreOptions += osIntersects;
    }

    CPLHTTPResult *psFeaturesResult =
        poDS->MakeRequest(osRequest, osMoreOptions);

    if (psFeaturesResult == NULL) {
        CPLDebug("GME", "Got NULL from MakeRequest. Something went wrong. You figure it out!");
        current_feature_page = NULL;
        return;
    }
    CPLDebug("GME",
             "features doc = %s...",
             psFeaturesResult->pabyData);

/* -------------------------------------------------------------------- */
/*      Parse result.                                                   */
/* -------------------------------------------------------------------- */

    current_feature_page =
        OGRGMEParseJSON((const char *) psFeaturesResult->pabyData);
    CPLHTTPDestroyResult(psFeaturesResult);

    current_features_array =
        json_object_object_get(current_feature_page, "features");
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRGMELayer::GetNextRawFeature()

{
/* -------------------------------------------------------------------- */
/*      Fetch a new page of features if needed.                         */
/* -------------------------------------------------------------------- */
    if (current_feature_page == NULL
        || index_in_page >= json_object_array_length(current_features_array))
    {
        GetPageOfFeatures();
    }

    if (current_feature_page == NULL)
    {
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Identify our json feature.                                      */
/* -------------------------------------------------------------------- */
    json_object *feature_obj =
        json_object_array_get_idx(current_features_array, index_in_page++);
    if (feature_obj == NULL) 
        return NULL;

    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);

/* -------------------------------------------------------------------- */
/*      Handle properties.                                              */
/* -------------------------------------------------------------------- */
    json_object *properties_obj =
        json_object_object_get(feature_obj, "properties");
    for (int iOGRField = 0;
         iOGRField < poFeatureDefn->GetFieldCount(); 
         iOGRField++ ) 
    {
        const char *pszValue = 
            OGRGMEGetJSONString(
                properties_obj, 
                poFeatureDefn->GetFieldDefn(iOGRField)->GetNameRef(),
                NULL);
        if (pszValue != NULL) {
            poFeature->SetField(iOGRField, pszValue);
	}
    }
/* -------------------------------------------------------------------- */
/*      Handle gx_id.                                                   */
/* -------------------------------------------------------------------- */
    const char *gx_id = OGRGMEGetJSONString(properties_obj, "gx_id");
    if (gx_id) {
        CPLString gmeId(gx_id);
        omnosIdToGMEKey[++m_nFeaturesRead] = gmeId;
        poFeature->SetFID(m_nFeaturesRead);
        CPLDebug("GME", "Mapping ids: \"%s\" to %d", gx_id, (int)m_nFeaturesRead);
    }

/* -------------------------------------------------------------------- */
/*      Handle geometry.                                                */
/* -------------------------------------------------------------------- */
    json_object *geometry_obj =
        json_object_object_get(feature_obj, "geometry");
    OGRGeometry *poGeometry = NULL;

    if (geometry_obj != NULL) 
    {
        poGeometry = OGRGeoJSONReadGeometry(geometry_obj);
    }

    if (poGeometry != NULL) 
    {
        poGeometry->assignSpatialReference(poSRS);
        poFeature->SetGeometryDirectly(poGeometry);
    }

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGMELayer::GetNextFeature()
{
    OGRFeature *poFeature = NULL;
    
    while( TRUE )
    {
        poFeature = GetNextRawFeature();
        if( poFeature == NULL )
            break;

        // incremeted in GetNextRawFeature()
        // m_nFeaturesRead++;

        if( (m_poFilterGeom == NULL
             || poFeature->GetGeometryRef() == NULL 
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
            break;

        delete poFeature;
    }

    return poFeature;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRGMELayer::GetLayerDefn()
{
    if (poFeatureDefn == NULL)
    {
        if (osTableId.size() == 0)
            return NULL;
        if (!FetchDescribe())
	    return NULL;
    }

    return poFeatureDefn;
}


/************************************************************************/
/*                        SetAttributeFilter()                          */
/************************************************************************/

OGRErr OGRGMELayer::SetAttributeFilter( const char *pszWhere )
{
    OGRErr eErr;
    eErr = OGRLayer::SetAttributeFilter(pszWhere);
    if( eErr == OGRERR_NONE ) {
        if ( pszWhere ) {
            char * pszEscaped = CPLEscapeString(pszWhere, -1, CPLES_URL);
            osWhere = CPLString(pszEscaped);
            CPLFree(pszEscaped);
        }
        else {
            osWhere = "";
        }
    }
    return eErr;
}

/************************************************************************/
/*                       SetIgnoredFields()                             */
/************************************************************************/

OGRErr OGRGMELayer::SetIgnoredFields(const char ** papszFields )
{
    osSelect = "geometry";
    OGRErr eErr;
    eErr = OGRLayer::SetIgnoredFields(papszFields);

    if( eErr == OGRERR_NONE ) {
        for ( int iOGRField = 0; iOGRField < poFeatureDefn->GetFieldCount(); iOGRField++ )
        {
            if (!poFeatureDefn->GetFieldDefn(iOGRField)->IsIgnored()) {
                osSelect += ",";
                osSelect += poFeatureDefn->GetFieldDefn(iOGRField)->GetNameRef();
            }
        }
    }
    return eErr;
}

/************************************************************************/
/*                       SetSpatialFilter()                             */
/************************************************************************/

void OGRGMELayer::SetSpatialFilter( OGRGeometry *poGeomIn)
{
    if (poGeomIn == NULL) {
        osIntersects.clear();
        OGRLayer::SetSpatialFilter( poGeomIn );
        return;
    }
    switch( poGeomIn->getGeometryType() )
    {
      case wkbPolygon:
        WindPolygonCCW((OGRPolygon *) poGeomIn);
      case wkbPoint:
      case wkbLineString:
        if( poGeomIn == NULL ) {
          osIntersects = "";
        }
        else {
            char * pszWkt;
            poGeomIn->exportToWkt(&pszWkt);
            char * pszEscaped = CPLEscapeString(pszWkt, -1, CPLES_URL);
            osIntersects = CPLString(pszEscaped);
            CPLFree(pszEscaped);
            CPLFree(pszWkt);
        }
        ResetReading();
        break;
      default:
        m_iGeomFieldFilter = 0;
        if( InstallFilter( poGeomIn ) )
            ResetReading();
        break;
    }
}

/************************************************************************/
/*                          WindPolygonCCW()                            */
/************************************************************************/

OGRPolygon* OGRGMELayer::WindPolygonCCW( OGRPolygon *poPolygon )
{
    CPLAssert( NULL != poPolygon );

    OGRLinearRing* poRing = poPolygon->getExteriorRing();
    if (poRing == NULL) {
        return poPolygon;
    }

    // If the linear ring is CW re-wind it CCW
    if (poRing->isClockwise() ) {
      poRing->reverseWindingOrder();
    }

    /* Interior rings. */
    const int nCount = poPolygon->getNumInteriorRings();
    for( int i = 0; i < nCount; ++i ) {
        poRing = poPolygon->getInteriorRing( i );
        if (poRing == NULL)
            continue;
        // If the linear ring is CW re-wind it CCW

        if (poRing->isClockwise() ) {
            poRing->reverseWindingOrder();
        }
    }

    return poPolygon;
}


/************************************************************************/
/*                            BatchPatch()                              */
/************************************************************************/

OGRErr OGRGMELayer::BatchPatch()
{
    CPLDebug("GME", "BatchPatch() - <%d>", (int)oListOfDeletedFeatures.size() );
    return BatchRequest("batchPatch", omnpoUpdatedFeatures);
}

/************************************************************************/
/*                            BatchInsert()                             */
/************************************************************************/

OGRErr OGRGMELayer::BatchInsert()
{
    CPLDebug("GME", "BatchInsert() - <%d>", (int)oListOfDeletedFeatures.size() );
    return BatchRequest("batchInsert", omnpoInsertedFeatures);
}

/************************************************************************/
/*                            BatchDelete()                             */
/************************************************************************/

OGRErr OGRGMELayer::BatchDelete()
{
    json_object *pjoBatchDelete = json_object_new_object();
    json_object *pjoGxIds = json_object_new_array();
    std::vector<long>::const_iterator fit;
    CPLDebug("GME", "BatchDelete() - <%d>", (int)oListOfDeletedFeatures.size() );
    if (oListOfDeletedFeatures.size() == 0) {
        CPLDebug("GME", "Empty list, not doing BatchDelete");
        return OGRERR_NONE;
    }
    for ( fit = oListOfDeletedFeatures.begin(); fit != oListOfDeletedFeatures.end(); fit++)
    {
        long nFID = *fit;
        if (nFID > 0) {
            CPLString osGxId(omnosIdToGMEKey[nFID]);
            CPLDebug("GME", "Deleting feature %ld -> '%s'", nFID, osGxId.c_str());
            json_object *pjoGxId = json_object_new_string(osGxId.c_str());
            omnosIdToGMEKey.erase(nFID);
            json_object_array_add( pjoGxIds, pjoGxId );
        }
    }
    oListOfDeletedFeatures.clear();
    if (json_object_array_length(pjoGxIds) == 0)
        return OGRERR_FAILURE;
    json_object_object_add( pjoBatchDelete, "gx_ids", pjoGxIds );
    const char *body =
        json_object_to_json_string_ext(pjoBatchDelete,
                                       JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);

/* -------------------------------------------------------------------- */
/*      POST changes                                                    */
/* -------------------------------------------------------------------- */
    CPLString osRequest = "tables/" + osTableId + "/features/batchDelete";
    CPLHTTPResult *poBatchDeleteResult = poDS->PostRequest(osRequest, body);
    if (poBatchDeleteResult) {
        CPLDebug("GME", "batchDelete returned %d", poBatchDeleteResult->nStatus);
        return OGRERR_NONE;
    }
    else {
        CPLDebug("GME", "batchPatch failed, NULL was returned.");
        CPLError(CE_Failure, CPLE_AppDefined, "Server error for batchDelete");
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            BatchRequest()                            */
/************************************************************************/

OGRErr OGRGMELayer::BatchRequest(const char *pszMethod, std::map<int, OGRFeature *> &omnpoFeatures)
{
    json_object *pjoBatchDoc = json_object_new_object();
    json_object *pjoFeatures = json_object_new_array();
    std::map<int, OGRFeature *>::const_iterator fit;
    CPLDebug("GME", "BatchRequest('%s', <%d>)", pszMethod, (int)omnpoFeatures.size() );
    if (omnpoFeatures.size() == 0) {
        CPLDebug("GME", "Empty map, not doing '%s'", pszMethod);
        return OGRERR_NONE;
    }
    for ( fit = omnpoFeatures.begin(); fit != omnpoFeatures.end(); fit++)
    {
        long nFID = fit->first;
        OGRFeature *poFeature = fit->second;
        CPLDebug("GME", "Processing feature: %ld", nFID );
        json_object *pjoFeature = OGRGMEFeatureToGeoJSON(poFeature);

        if (pjoFeature != NULL)
            json_object_array_add( pjoFeatures, pjoFeature );
        delete poFeature;
    }
    omnpoFeatures.clear();
    if (json_object_array_length(pjoFeatures) == 0)
        return OGRERR_FAILURE;
    json_object_object_add( pjoBatchDoc, "features", pjoFeatures );
    const char *body =
        json_object_to_json_string_ext(pjoBatchDoc,
                                       JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);

/* -------------------------------------------------------------------- */
/*      POST changes                                                    */
/* -------------------------------------------------------------------- */
    CPLString osRequest = "tables/" + osTableId + "/features/" + pszMethod;
    CPLHTTPResult *psBatchResult = poDS->PostRequest(osRequest, body);
    if (psBatchResult) {
        CPLDebug("GME", "%s returned %d", pszMethod, psBatchResult->nStatus);
        return OGRERR_NONE;
    }
    else {
        CPLDebug("GME", "%s failed, NULL was returned.", pszMethod );
        CPLError(CE_Failure, CPLE_AppDefined, "Server error for %s", pszMethod);
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                         SetBatchPatchSize()                          */
/************************************************************************/

void OGRGMELayer::SetBatchPatchSize(unsigned int iSize)

{
    iBatchPatchSize = iSize;
}

/************************************************************************/
/*                         GetBatchPatchSize()                          */
/************************************************************************/

unsigned int OGRGMELayer::GetBatchPatchSize()

{
    CPLString osBatchPatchSize;
    osBatchPatchSize = CPLGetConfigOption("GME_BATCH_PATCH_SIZE","0");
    int iSize = atoi( osBatchPatchSize.c_str() );
    if (iSize < 1)
        return iBatchPatchSize;
    else {
        iBatchPatchSize = iSize;
        return (unsigned int) iSize;
    }
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRGMELayer::ICreateFeature( OGRFeature *poFeature )

{
    if (!poFeature)
        return OGRERR_FAILURE;
    if (!CreateTableIfNotCreated()) {
        return OGRERR_FAILURE;
    }

    long nFID = ++m_nFeaturesRead;
    poFeature->SetFID(nFID);

    int nGxId = poFeature->GetFieldIndex("gx_id");
    CPLDebug("GME", "gx_id is field %d", iGxIdField);
    CPLString osGxId;
    CPLDebug("GME", "Inserting feature %ld as %s", poFeature->GetFID(), osGxId.c_str());
    if (nGxId >= 0) {
        iGxIdField = nGxId;
        if(poFeature->IsFieldSet(iGxIdField)) {
          osGxId = poFeature->GetFieldAsString(iGxIdField);
          CPLDebug("GME", "Feature already has %ld gx_id='%s'", poFeature->GetFID(),
                   osGxId.c_str());
        }
        else {
            osGxId = CPLSPrintf("GDAL-%ld", nFID);
            CPLDebug("GME", "Setting field %d as %s", iGxIdField, osGxId.c_str() );
            poFeature->SetField( iGxIdField, osGxId.c_str() );
        }
    }

    if (bInTransaction) {
        unsigned int iBatchSize = GetBatchPatchSize();
        if (omnpoInsertedFeatures.size() >= iBatchSize) {
            CPLDebug("GME", "BatchInsert, reached BatchSize of %d", iBatchSize);
            OGRErr iBatchInsertResult = BatchInsert();
            if (iBatchInsertResult != OGRERR_NONE) {
                return iBatchInsertResult;
            }
        }
        omnosIdToGMEKey[poFeature->GetFID()] = osGxId;
        omnpoInsertedFeatures[nFID] = poFeature->Clone();
        CPLDebug("GME", "In Transaction, added feature to memory only");
        bDirty = true;
        return OGRERR_NONE;
    }
    else {
        CPLDebug("GME", "Not in Transaction, BatchInsert()");
        return BatchInsert();
    }
}

/************************************************************************/
/*                           ISetFeature()                               */
/************************************************************************/

OGRErr OGRGMELayer::ISetFeature( OGRFeature *poFeature )

{
    if (!poFeature)
        return OGRERR_FAILURE;
    long nFID = poFeature->GetFID();
    if(bInTransaction) {
        std::map<int, OGRFeature *>::const_iterator fit;
        fit = omnpoInsertedFeatures.find(nFID);
        if (fit != omnpoInsertedFeatures.end()) {
            omnpoInsertedFeatures[nFID] = poFeature->Clone();
            CPLDebug("GME", "Updated Feature %ld in Transaction", nFID);
        }
        else {
            unsigned int iBatchSize = GetBatchPatchSize();
            if (omnpoUpdatedFeatures.size() >= iBatchSize) {
                CPLDebug("GME", "BatchPatch, reached BatchSize of %d", iBatchSize);
                OGRErr iBatchInsertResult = BatchPatch();
                if (iBatchInsertResult != OGRERR_NONE) {
                    return iBatchInsertResult;
                }
            }
            CPLDebug("GME", "In Transaction, add update to Transaction");
            bDirty = true;
            omnpoUpdatedFeatures[nFID] = poFeature->Clone();
        }
        return OGRERR_NONE;
    }
    else {
        omnpoUpdatedFeatures[nFID] = poFeature->Clone();
        CPLDebug("GME", "Not in Transaction, BatchPatch()");
        return BatchPatch();
    }
}

/************************************************************************/
/*                           DeleteteFeature()                          */
/************************************************************************/

OGRErr OGRGMELayer::DeleteFeature( long nFID )
{
    if(bInTransaction) {
        std::map<int, OGRFeature *>::iterator fit;
        fit = omnpoInsertedFeatures.find(nFID);
        if (fit != omnpoInsertedFeatures.end()) {
            omnpoInsertedFeatures.erase(fit);
            CPLDebug("GME", "Found %ld in omnpoInsertedFeatures", nFID);
        }
        else {
            unsigned int iBatchSize = GetBatchPatchSize();
            if (oListOfDeletedFeatures.size() >= iBatchSize) {
                CPLDebug("GME", "BatchDelete, reached BatchSize of %d", iBatchSize);
                OGRErr iBatchResult = BatchDelete();
                if (iBatchResult != OGRERR_NONE) {
                    return iBatchResult;
                }
            }
            CPLDebug("GME", "In Transaction, adding feature to List");
            bDirty = true;
            oListOfDeletedFeatures.push_back(nFID); 
        }
        return OGRERR_NONE;
    }
    else {
        CPLDebug("GME", "Not in Transaction, BatchDelete()");
        return BatchDelete();
    }
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGMELayer::CreateField( OGRFieldDefn *poField,
                                 CPL_UNUSED int bApproxOK )
{
    CPLDebug("GME", "create field %s of type %s, pending = %d",
             poField->GetNameRef(), OGRFieldDefn::GetFieldTypeName(poField->GetType()),
             bCreateTablePending);
    if (!bCreateTablePending) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot add field to table after schema is defined.");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn == NULL) {
        poFeatureDefn = new OGRFeatureDefn( osTableName );

        poFeatureDefn->Reference();
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
        poFeatureDefn->GetGeomFieldDefn(0)->SetName("geometry");
    }
    poFeatureDefn->AddFieldDefn(poField);
    return OGRERR_NONE;
}

/************************************************************************/
/*                      CreateTableIfNotCreated()                       */
/************************************************************************/

bool OGRGMELayer::CreateTableIfNotCreated()
{
    if (!bCreateTablePending || (osTableId.size() != 0)) {
        CPLDebug("GME", "Not creating table since already created");
        CPLDebug("GME", "bCreateTablePending = %d osTableId ='%s'",
                 bCreateTablePending, osTableId.c_str());
        return true;
    }
    CPLDebug("GME", "Creating table...");

    json_object *pjoCreateDoc = json_object_new_object();
    json_object *pjoProjectId = json_object_new_string( osProjectId.c_str() );
    json_object_object_add( pjoCreateDoc, "projectId", pjoProjectId );
    json_object *pjoName = json_object_new_string( osTableName.c_str() );
    json_object_object_add( pjoCreateDoc, "name",  pjoName );
    json_object *pjoDraftACL = json_object_new_string( osDraftACL.c_str() );
    json_object_object_add( pjoCreateDoc, "draftAccessList", pjoDraftACL );
    json_object *pjoPublishedACL = json_object_new_string( osPublishedACL.c_str() );
    json_object_object_add( pjoCreateDoc, "publishedAccessList", pjoPublishedACL );
    json_object *pjoSchema = json_object_new_object();

    json_object *pjoColumns = json_object_new_array();

    poFeatureDefn->SetGeomType( eGTypeForCreation );

    json_object *pjoGeometryColumn = json_object_new_object();
    json_object *pjoGeometryName = json_object_new_string( "geometry" );
    json_object *pjoGeometryType;
    switch(eGTypeForCreation) {
    case wkbPoint:
    case wkbPoint25D:
    case wkbMultiPoint:
    case wkbMultiPoint25D:
        pjoGeometryType = json_object_new_string( "points" );
        break;
    case wkbLineString:
    case wkbLineString25D:
    case wkbMultiLineString:
    case wkbLinearRing:
    case wkbMultiLineString25D:
        pjoGeometryType = json_object_new_string( "lineStrings" );
        break;
    case wkbPolygon:
    case wkbPolygon25D:
    case wkbMultiPolygon:
    case wkbGeometryCollection:
    case wkbMultiPolygon25D:
        pjoGeometryType = json_object_new_string( "polygons" );
        break;
    case wkbGeometryCollection25D:
        pjoGeometryType = json_object_new_string( "mixedGeometry" );
        break;
    default:
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unsupported Geometry type. Defaulting to Points");
        pjoGeometryType = json_object_new_string( "points" );
        poFeatureDefn->SetGeomType( wkbPoint );
    }
    json_object_object_add( pjoGeometryColumn, "name", pjoGeometryName );
    json_object_object_add( pjoGeometryColumn, "type", pjoGeometryType );
    json_object_array_add( pjoColumns, pjoGeometryColumn );

    for (int iOGRField = 0; iOGRField < poFeatureDefn->GetFieldCount(); iOGRField++ )
    {
        if ((iOGRField == iGxIdField) && (iGxIdField >= 0))
            continue; // don't create the gx_id field.
        const char *pszFieldName = poFeatureDefn->GetFieldDefn(iOGRField)->GetNameRef();
        if (EQUAL(pszFieldName, "gx_id")) {
            iGxIdField = iOGRField;
            continue;
        }
        json_object *pjoColumn = json_object_new_object();
        json_object *pjoFieldName =
            json_object_new_string( pszFieldName );
        json_object *pjoFieldType;

        switch(poFeatureDefn->GetFieldDefn(iOGRField)->GetType()) {
        case OFTInteger:
            pjoFieldType = json_object_new_string( "integer" );
            break;
        case OFTReal:
            pjoFieldType = json_object_new_string( "double" );
            break;
        default:
            pjoFieldType = json_object_new_string( "string" );
        }
        json_object_object_add( pjoColumn, "name", pjoFieldName );
        json_object_object_add( pjoColumn, "type", pjoFieldType );
        json_object_array_add( pjoColumns, pjoColumn );
    }

    json_object_object_add( pjoSchema, "columns", pjoColumns );
    json_object_object_add( pjoCreateDoc, "schema", pjoSchema );
    const char *body =
        json_object_to_json_string_ext(pjoCreateDoc,
                                       JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY);

    CPLDebug("GME", "Create Table Doc:\n%s", body);

/* -------------------------------------------------------------------- */
/*      POST changes                                                    */
/* -------------------------------------------------------------------- */
    CPLString osRequest = "tables";
    CPLHTTPResult *poCreateResult = poDS->PostRequest(osRequest, body);
    if( poCreateResult == NULL || poCreateResult->pabyData == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Table creation failed.");
        if( poCreateResult )
            CPLHTTPDestroyResult(poCreateResult);
        return false;
    }
    CPLDebug("GME", "CreateTable returned %d\n%s", poCreateResult->nStatus,
             poCreateResult->pabyData);

    json_object *pjoResponseDoc = OGRGMEParseJSON((const char *) poCreateResult->pabyData);

    osTableId = OGRGMEGetJSONString(pjoResponseDoc, "id", "");
    CPLHTTPDestroyResult(poCreateResult);
    if (osTableId.size() == 0) {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Table creation failed, or could not find table id.");
        return false;
    }

    /*
    OGRFieldDefn *poGxIdField = new OGRFieldDefn("gx_id", OFTString);

    poFeatureDefn->AddFieldDefn(poGxIdField);
    iGxIdField = poFeatureDefn->GetFieldCount() - 1;
    CPLDebug("GME", "create field %s(%d) of type %s",
             "gx_id", iGxIdField, OGRFieldDefn::GetFieldTypeName(OFTString));
    */
    bCreateTablePending = false;
    CPLDebug("GME", "sleeping 3s to give GME time to create the table...");
    CPLSleep( 3.0 );
    return true;
}

/************************************************************************/
/*                          SetGeometryType()                           */
/************************************************************************/

void OGRGMELayer::SetGeometryType(OGRwkbGeometryType eGType)
{
    eGTypeForCreation = eGType;
}

/************************************************************************/
/*                         StartTransaction()                           */
/************************************************************************/

OGRErr OGRGMELayer::StartTransaction()
{
    if (bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Already in transaction");
        return OGRERR_FAILURE;
    }

    if (!poDS->IsReadWrite())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Operation not available in read-only mode");
        return OGRERR_FAILURE;
    }

    bInTransaction = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CommitTransaction()                          */
/************************************************************************/

OGRErr OGRGMELayer::CommitTransaction()
{
    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot commit, not in transaction");
        return OGRERR_FAILURE;
    }
    bInTransaction = FALSE;
    return SyncToDisk();
}

/************************************************************************/
/*                        RollbackTransaction()                         */
/************************************************************************/

OGRErr OGRGMELayer::RollbackTransaction()
{
    if (!bInTransaction)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot rollback, not in transaction.");
        return OGRERR_FAILURE;
    }
    bInTransaction = FALSE;
    omnpoUpdatedFeatures.clear();
    omnpoInsertedFeatures.clear();
    oListOfDeletedFeatures.clear();
    return OGRERR_NONE;
}
