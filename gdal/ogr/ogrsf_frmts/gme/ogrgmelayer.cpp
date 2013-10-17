/******************************************************************************
 * $Id$
 *
 * Project:  Google Maps Engine API Driver
 * Purpose:  OGRGMELayer Implementation.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
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

CPL_CVSID("$Id: ogrgmetablelayer.cpp 25475 2013-01-09 09:09:59Z warmerdam $");

/************************************************************************/
/*                            OGRGMELayer()                             */
/************************************************************************/

OGRGMELayer::OGRGMELayer(OGRGMEDataSource* poDS,
                         const char* pszTableId)

{
    this->poDS = poDS;
    poSRS = new OGRSpatialReference(SRS_WKT_WGS84);
    poFeatureDefn = NULL;
    osTableId = pszTableId;
    current_feature_page = NULL;
}

/************************************************************************/
/*                            ~OGRGMELayer()                            */
/************************************************************************/

OGRGMELayer::~OGRGMELayer()

{
    ResetReading();
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

        // TODO - clear current page.
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMELayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;
    return OGRGMELayer::TestCapability(pszCap);
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
        poDS->Parse((const char *) psDescribe->pabyData);

    CPLHTTPDestroyResult(psDescribe);

    osTableName = poDS->GetJSONString(table_doc, "name");

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

	const char* name = poDS->GetJSONString(field_obj, "name");
        OGRFieldDefn oFieldDefn(name, OFTString);
        const char *type = poDS->GetJSONString(field_obj, "type");

        if (EQUAL(type, "integer")) 
            oFieldDefn.SetType(OFTInteger);
        else if (EQUAL(type, "double")) 
            oFieldDefn.SetType(OFTReal);
        else if (EQUAL(type, "boolean")) 
            oFieldDefn.SetType(OFTInteger);
        else if (EQUAL(type, "string")) 
            oFieldDefn.SetType(OFTString);
        else if (EQUAL(type, "string")) 
            oFieldDefn.SetType(OFTString);
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
        osNextPageToken = poDS->GetJSONString(current_feature_page, 
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

    CPLHTTPResult *psFeaturesResult = 
        poDS->MakeRequest(osRequest, osMoreOptions);

    if (psFeaturesResult == NULL)
        return;
    
    CPLDebug("GME", 
             "features doc = %s...", 
             psFeaturesResult->pabyData);
    
/* -------------------------------------------------------------------- */
/*      Parse result.                                                   */
/* -------------------------------------------------------------------- */
    
    current_feature_page = 
        poDS->Parse((const char *) psFeaturesResult->pabyData);
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
            poDS->GetJSONString(
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
    const char *gx_id = poDS->GetJSONString(properties_obj, "gx_id");
    int nFID = atoi(gx_id);
    if (EQUAL(CPLSPrintf("%d", nFID), gx_id))
	poFeature->SetFID(nFID);
    else
        poFeature->SetFID(m_nFeaturesRead);

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

        m_nFeaturesRead++;

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
        FetchDescribe();
    }

    return poFeatureDefn;
}

