/******************************************************************************
 *
 * Project:  Carto Translator
 * Purpose:  Implements OGRCARTOLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_carto.h"
#include "ogr_p.h"
#include "ogrgeojsonreader.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRCARTOLayer()                            */
/************************************************************************/

OGRCARTOLayer::OGRCARTOLayer(OGRCARTODataSource* poDSIn) :
    poDS(poDSIn),
    poFeatureDefn(NULL),
    poCachedObj(NULL)
{
    ResetReading();
}

/************************************************************************/
/*                         ~OGRCARTOLayer()                           */
/************************************************************************/

OGRCARTOLayer::~OGRCARTOLayer()

{
    if( poCachedObj != NULL )
        json_object_put(poCachedObj);

    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCARTOLayer::ResetReading()

{
    if( poCachedObj != NULL )
        json_object_put(poCachedObj);
    poCachedObj = NULL;
    bEOF = false;
    nFetchedObjects = -1;
    iNextInFetchedObjects = 0;
    iNext = 0;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRCARTOLayer::GetLayerDefn()
{
    return GetLayerDefnInternal(NULL);
}

/************************************************************************/
/*                           BuildFeature()                             */
/************************************************************************/

OGRFeature *OGRCARTOLayer::BuildFeature(json_object* poRowObj)
{
    OGRFeature* poFeature = NULL;
    if( poRowObj != NULL &&
        json_object_get_type(poRowObj) == json_type_object )
    {
        //CPLDebug("Carto", "Row: %s", json_object_to_json_string(poRowObj));
        poFeature = new OGRFeature(poFeatureDefn);

        if( !osFIDColName.empty() )
        {
            json_object* poVal = CPL_json_object_object_get(poRowObj, osFIDColName);
            if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_int )
            {
                poFeature->SetFID(json_object_get_int64(poVal));
            }
        }
        else
        {
            poFeature->SetFID(iNext);
        }

        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            json_object* poVal = CPL_json_object_object_get(poRowObj,
                            poFeatureDefn->GetFieldDefn(i)->GetNameRef());
            if( poVal == NULL )
            {
                poFeature->SetFieldNull(i);
            }
            else if( json_object_get_type(poVal) == json_type_string )
            {
                if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTDateTime )
                {
                    OGRField sField;
                    if( OGRParseXMLDateTime( json_object_get_string(poVal),
                                             &sField) )
                    {
                        poFeature->SetField(i, &sField);
                    }
                }
                else
                {
                    poFeature->SetField(i, json_object_get_string(poVal));
                }
            }
            else if( json_object_get_type(poVal) == json_type_int ||
                     json_object_get_type(poVal) == json_type_boolean )
            {
                poFeature->SetField(i, (GIntBig)json_object_get_int64(poVal));
            }
            else if( json_object_get_type(poVal) == json_type_double )
            {
                poFeature->SetField(i, json_object_get_double(poVal));
            }
        }

        for(int i=0;i<poFeatureDefn->GetGeomFieldCount();i++)
        {
            OGRGeomFieldDefn* poGeomFldDefn = poFeatureDefn->GetGeomFieldDefn(i);
            json_object* poVal = CPL_json_object_object_get(poRowObj,
                            poGeomFldDefn->GetNameRef());
            if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_string )
            {
                OGRGeometry* poGeom = OGRGeometryFromHexEWKB(
                    json_object_get_string(poVal), NULL, FALSE);
                if( poGeom != NULL )
                    poGeom->assignSpatialReference(poGeomFldDefn->GetSpatialRef());
                poFeature->SetGeomFieldDirectly(i, poGeom);
            }
        }
    }
    return poFeature;
}

/************************************************************************/
/*                        FetchNewFeatures()                            */
/************************************************************************/

json_object* OGRCARTOLayer::FetchNewFeatures(GIntBig iNextIn)
{
    CPLString osSQL = osBaseSQL;
    if( osSQL.ifind("SELECT") != std::string::npos &&
        osSQL.ifind(" LIMIT ") == std::string::npos )
    {
        osSQL += " LIMIT ";
        osSQL += CPLSPrintf("%d", GetFeaturesToFetch());
        osSQL += " OFFSET ";
        osSQL += CPLSPrintf(CPL_FRMT_GIB, iNextIn);
    }
    return poDS->RunSQL(osSQL);
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature *OGRCARTOLayer::GetNextRawFeature()
{
    if( bEOF )
        return NULL;

    if( iNextInFetchedObjects >= nFetchedObjects )
    {
        if( nFetchedObjects > 0 && nFetchedObjects < GetFeaturesToFetch() )
        {
            bEOF = true;
            return NULL;
        }

        if( poFeatureDefn == NULL && osBaseSQL.empty() )
        {
            GetLayerDefn();
        }

        json_object* poObj = FetchNewFeatures(iNext);
        if( poObj == NULL )
        {
            bEOF = true;
            return NULL;
        }

        if( poFeatureDefn == NULL )
        {
            GetLayerDefnInternal(poObj);
        }

        json_object* poRows = CPL_json_object_object_get(poObj, "rows");
        if( poRows == NULL ||
            json_object_get_type(poRows) != json_type_array ||
            json_object_array_length(poRows) == 0 )
        {
            json_object_put(poObj);
            bEOF = true;
            return NULL;
        }

        if( poCachedObj != NULL )
            json_object_put(poCachedObj);
        poCachedObj = poObj;

        nFetchedObjects = json_object_array_length(poRows);
        iNextInFetchedObjects = 0;
    }

    json_object* poRows = CPL_json_object_object_get(poCachedObj, "rows");
    json_object* poRowObj = json_object_array_get_idx(poRows, iNextInFetchedObjects);

    iNextInFetchedObjects ++;

    OGRFeature* poFeature = BuildFeature(poRowObj);
    iNext = poFeature->GetFID() + 1;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRCARTOLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while( true )
    {
        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
            return NULL;

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCARTOLayer::TestCapability( const char * pszCap )

{
    if ( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                          EstablishLayerDefn()                        */
/************************************************************************/

void OGRCARTOLayer::EstablishLayerDefn(const char* pszLayerName,
                                         json_object* poObjIn)
{
    poFeatureDefn = new OGRFeatureDefn(pszLayerName);
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);

    CPLString osSQL;
    size_t nPos = osBaseSQL.ifind(" LIMIT ");
    if( nPos != std::string::npos )
    {
        osSQL = osBaseSQL;
        size_t nSize = osSQL.size();
        for(size_t i = nPos + strlen(" LIMIT "); i < nSize; i++)
        {
            if( osSQL[i] == ' ' )
                break;
            osSQL[i] = '0';
        }
    }
    else
        osSQL.Printf("%s LIMIT 0", osBaseSQL.c_str());
    json_object* poObj;
    if( poObjIn != NULL )
        poObj = poObjIn;
    else
    {
        poObj = poDS->RunSQL(osSQL);
        if( poObj == NULL )
        {
            return;
        }
    }

    json_object* poFields = CPL_json_object_object_get(poObj, "fields");
    if( poFields == NULL || json_object_get_type(poFields) != json_type_object)
    {
        if( poObjIn == NULL )
            json_object_put(poObj);
        return;
    }

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC( poFields, it )
    {
        const char* pszColName = it.key;
        if( it.val != NULL && json_object_get_type(it.val) == json_type_object)
        {
            json_object* poType = CPL_json_object_object_get(it.val, "type");
            if( poType != NULL && json_object_get_type(poType) == json_type_string )
            {
                const char* pszType = json_object_get_string(poType);
                CPLDebug("CARTO", "%s : %s", pszColName, pszType);
                if( EQUAL(pszType, "string") ||
                    EQUAL(pszType, "unknown(19)") /* name */ )
                {
                    OGRFieldDefn oFieldDefn(pszColName, OFTString);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
                else if( EQUAL(pszType, "number") )
                {
                    if( !EQUAL(pszColName, "cartodb_id") )
                    {
                        OGRFieldDefn oFieldDefn(pszColName, OFTReal);
                        poFeatureDefn->AddFieldDefn(&oFieldDefn);
                    }
                    else
                        osFIDColName = pszColName;
                }
                else if( EQUAL(pszType, "date") )
                {
                    if( !EQUAL(pszColName, "created_at") &&
                        !EQUAL(pszColName, "updated_at") )
                    {
                        OGRFieldDefn oFieldDefn(pszColName, OFTDateTime);
                        poFeatureDefn->AddFieldDefn(&oFieldDefn);
                    }
                }
                else if( EQUAL(pszType, "geometry") )
                {
                    if( !EQUAL(pszColName, "the_geom_webmercator") )
                    {
                        OGRCartoGeomFieldDefn *poFieldDefn =
                            new OGRCartoGeomFieldDefn(pszColName, wkbUnknown);
                        poFeatureDefn->AddGeomFieldDefn(poFieldDefn, FALSE);
                        OGRSpatialReference* l_poSRS = GetSRS(pszColName, &poFieldDefn->nSRID);
                        if( l_poSRS != NULL )
                        {
                            poFeatureDefn->GetGeomFieldDefn(
                                poFeatureDefn->GetGeomFieldCount() - 1)->SetSpatialRef(l_poSRS);
                            l_poSRS->Release();
                        }
                    }
                }
                else if( EQUAL(pszType, "boolean") )
                {
                    OGRFieldDefn oFieldDefn(pszColName, OFTInteger);
                    oFieldDefn.SetSubType(OFSTBoolean);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
                else
                {
                    CPLDebug("CARTO", "Unhandled type: %s. Defaulting to string", pszType);
                    OGRFieldDefn oFieldDefn(pszColName, OFTString);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
            }
            else if( poType != NULL && json_object_get_type(poType) == json_type_int )
            {
                /* FIXME? manual creations of geometry columns return integer types */
                OGRCartoGeomFieldDefn *poFieldDefn =
                    new OGRCartoGeomFieldDefn(pszColName, wkbUnknown);
                poFeatureDefn->AddGeomFieldDefn(poFieldDefn, FALSE);
                OGRSpatialReference* l_poSRS = GetSRS(pszColName, &poFieldDefn->nSRID);
                if( l_poSRS != NULL )
                {
                    poFeatureDefn->GetGeomFieldDefn(
                        poFeatureDefn->GetGeomFieldCount() - 1)->SetSpatialRef(l_poSRS);
                    l_poSRS->Release();
                }
            }
        }
    }
    if( poObjIn == NULL )
        json_object_put(poObj);
}

/************************************************************************/
/*                               GetSRS()                               */
/************************************************************************/

OGRSpatialReference* OGRCARTOLayer::GetSRS(const char* pszGeomCol,
                                             int *pnSRID)
{
    json_object* poObj = poDS->RunSQL(GetSRS_SQL(pszGeomCol));
    json_object* poRowObj = OGRCARTOGetSingleRow(poObj);
    if( poRowObj == NULL )
    {
        if( poObj != NULL )
            json_object_put(poObj);
        return NULL;
    }

    json_object* poSRID = CPL_json_object_object_get(poRowObj, "srid");
    if( poSRID != NULL && json_object_get_type(poSRID) == json_type_int )
    {
        *pnSRID = json_object_get_int(poSRID);
    }

    json_object* poSRTEXT = CPL_json_object_object_get(poRowObj, "srtext");
    OGRSpatialReference* l_poSRS = NULL;
    if( poSRTEXT != NULL && json_object_get_type(poSRTEXT) == json_type_string )
    {
        const char* pszSRTEXT = json_object_get_string(poSRTEXT);
        l_poSRS = new OGRSpatialReference();
        char* pszTmp = (char* )pszSRTEXT;
        if( l_poSRS->importFromWkt(&pszTmp) != OGRERR_NONE )
        {
            delete l_poSRS;
            l_poSRS = NULL;
        }
    }
    json_object_put(poObj);

    return l_poSRS;
}
