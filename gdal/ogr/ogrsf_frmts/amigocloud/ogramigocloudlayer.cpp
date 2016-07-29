/******************************************************************************
 *
 * Project:  AmigoCloud Translator
 * Purpose:  Implements OGRAmigoCloudLayer class.
 * Author:   Victor Chernetsky, <victor at amigocloud dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Victor Chernetsky, <victor at amigocloud dot com>
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

#include "ogr_amigocloud.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRAmigoCloudLayer()                            */
/************************************************************************/

OGRAmigoCloudLayer::OGRAmigoCloudLayer(OGRAmigoCloudDataSource* poDSIn) :
    poDS(poDSIn),
    poFeatureDefn(NULL),
    osFIDColName("amigo_id"),
    poCachedObj(NULL)
{
    ResetReading();
}

/************************************************************************/
/*                         ~OGRAmigoCloudLayer()                           */
/************************************************************************/

OGRAmigoCloudLayer::~OGRAmigoCloudLayer()

{
    if( poCachedObj != NULL )
        json_object_put(poCachedObj);

    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRAmigoCloudLayer::ResetReading()

{
    if( poCachedObj != NULL )
        json_object_put(poCachedObj);
    poCachedObj = NULL;
    bEOF = FALSE;
    nFetchedObjects = -1;
    iNextInFetchedObjects = 0;
    iNext = 0;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRAmigoCloudLayer::GetLayerDefn()
{
    return GetLayerDefnInternal(NULL);
}

/************************************************************************/
/*                            BuildFeature()                            */
/************************************************************************/

OGRFeature *OGRAmigoCloudLayer::BuildFeature(json_object* poRowObj)
{
    OGRFeature* poFeature = NULL;
    if( poRowObj != NULL &&
        json_object_get_type(poRowObj) == json_type_object )
    {
        poFeature = new OGRFeature(poFeatureDefn);

        if( osFIDColName.size() > 0 )
        {
            json_object* poVal = json_object_object_get(poRowObj, osFIDColName);
            if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_string )
            {
                std::string amigo_id = json_object_get_string(poVal);
                OGRAmigoCloudFID aFID(amigo_id, iNext);
                mFIDs[aFID.iFID] = aFID;
                poFeature->SetFID(aFID.iFID);

            }
        }


        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            json_object* poVal = json_object_object_get(poRowObj,
                            poFeatureDefn->GetFieldDefn(i)->GetNameRef());

            if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_string )
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
            else if( poVal != NULL &&
                (json_object_get_type(poVal) == json_type_int ||
                 json_object_get_type(poVal) == json_type_boolean) )
            {
                poFeature->SetField(i, (GIntBig)json_object_get_int64(poVal));
            }
            else if( poVal != NULL &&
                json_object_get_type(poVal) == json_type_double )
            {
                poFeature->SetField(i, json_object_get_double(poVal));
            }
        }

        for(int i=0;i<poFeatureDefn->GetGeomFieldCount();i++)
        {
            OGRGeomFieldDefn* poGeomFldDefn = poFeatureDefn->GetGeomFieldDefn(i);
            json_object* poVal = json_object_object_get(poRowObj,
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

json_object* OGRAmigoCloudLayer::FetchNewFeatures(GIntBig iNextIn)
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
/*                       GetNextRawFeature()                            */
/************************************************************************/

OGRFeature *OGRAmigoCloudLayer::GetNextRawFeature()
{
    if( bEOF )
        return NULL;

    if( iNextInFetchedObjects >= nFetchedObjects )
    {
        if( nFetchedObjects > 0 && nFetchedObjects < GetFeaturesToFetch() )
        {
            bEOF = TRUE;
            return NULL;
        }

        if( poFeatureDefn == NULL && osBaseSQL.size() == 0 )
        {
            GetLayerDefn();
        }

        json_object* poObj = FetchNewFeatures(iNext);
        if( poObj == NULL )
        {
            bEOF = TRUE;
            return NULL;
        }

        if( poFeatureDefn == NULL )
        {
            GetLayerDefnInternal(poObj);
        }

        json_object* poRows = json_object_object_get(poObj, "data");

        if( poRows == NULL ||
            json_object_get_type(poRows) != json_type_array ||
            json_object_array_length(poRows) == 0 )
        {
            json_object_put(poObj);
            bEOF = TRUE;
            return NULL;
        }

        if( poCachedObj != NULL )
            json_object_put(poCachedObj);
        poCachedObj = poObj;

        nFetchedObjects = json_object_array_length(poRows);
        iNextInFetchedObjects = 0;
    }

    json_object* poRows = json_object_object_get(poCachedObj, "data");
    json_object* poRowObj = json_object_array_get_idx(poRows, iNextInFetchedObjects);

    iNextInFetchedObjects ++;

    OGRFeature* poFeature = BuildFeature(poRowObj);

    std::map<GIntBig, OGRAmigoCloudFID>::iterator it = mFIDs.find(poFeature->GetFID());
    if(it!=mFIDs.end())
    {
//        iNext = poFeature->GetFID() + 1;
        iNext = it->second.iIndex + 1;
    }

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRAmigoCloudLayer::GetNextFeature()
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

int OGRAmigoCloudLayer::TestCapability( const char * pszCap )

{
    if ( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                          EstablishLayerDefn()                        */
/************************************************************************/

void OGRAmigoCloudLayer::EstablishLayerDefn(const char* pszLayerName,
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

    json_object* poFields = json_object_object_get(poObj, "columns");
    if( poFields == NULL || json_object_get_type(poFields) != json_type_array)
    {
        if( poObjIn == NULL )
            json_object_put(poObj);
        return;
    }

    int size = json_object_array_length(poFields);

    for(int i=0; i< size; i++)
    {
        json_object *obj = json_object_array_get_idx(poFields, i);

        if(obj != NULL && json_object_get_type(obj) == json_type_object)
        {
            std::string fieldName;
            std::string fieldType;

            json_object_iter it;
            it.key = NULL;
            it.val = NULL;
            it.entry = NULL;
            json_object_object_foreachC(obj, it)
            {
                const char *pszColName = it.key;
                if(it.val != NULL)
                {
                    if(EQUAL(pszColName, "name"))
                    {
                        fieldName = json_object_get_string(it.val);
                    } else if(EQUAL(pszColName, "type"))
                    {
                        fieldType = json_object_get_string(it.val);
                    }
                }
            }
            if(!fieldName.empty() && !fieldType.empty())
            {

                if(EQUAL(fieldType.c_str(), "string") ||
                   EQUAL(fieldType.c_str(), "unknown(19)") /* name */ )
                {
                    OGRFieldDefn oFieldDefn(fieldName.c_str(), OFTString);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
                else if(EQUAL(fieldType.c_str(), "number"))
                {
                    OGRFieldDefn oFieldDefn(fieldName.c_str(), OFTReal);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
                else if(EQUAL(fieldType.c_str(), "date"))
                {
                    if(!EQUAL(fieldName.c_str(), "created_at") &&
                       !EQUAL(fieldName.c_str(), "updated_at"))
                    {
                        OGRFieldDefn oFieldDefn(fieldName.c_str(), OFTDateTime);
                        poFeatureDefn->AddFieldDefn(&oFieldDefn);
                    }
                }
                else if(EQUAL(fieldType.c_str(), "geometry"))
                {
                    OGRAmigoCloudGeomFieldDefn *poFieldDefn =
                            new OGRAmigoCloudGeomFieldDefn(fieldName.c_str(), wkbUnknown);
                    poFeatureDefn->AddGeomFieldDefn(poFieldDefn, FALSE);
                    OGRSpatialReference* poSRS = GetSRS(fieldName.c_str(), &poFieldDefn->nSRID);
                    if( poSRS != NULL )
                    {
                        poFeatureDefn->GetGeomFieldDefn(
                                poFeatureDefn->GetGeomFieldCount() - 1)->SetSpatialRef(poSRS);
                        poSRS->Release();
                    } else {
                        poFeatureDefn->GetGeomFieldDefn(
                                poFeatureDefn->GetGeomFieldCount() - 1)->SetSpatialRef(poSRS);
                    }
                }
                else if(EQUAL(fieldType.c_str(), "boolean"))
                {
                    OGRFieldDefn oFieldDefn(fieldName.c_str(), OFTInteger);
                    oFieldDefn.SetSubType(OFSTBoolean);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
                }
                else
                {
                    CPLDebug("AMIGOCLOUD", "Unhandled type: %s. Defaulting to string", fieldType.c_str());
                    OGRFieldDefn oFieldDefn(fieldName.c_str(), OFTString);
                    poFeatureDefn->AddFieldDefn(&oFieldDefn);
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

OGRSpatialReference* OGRAmigoCloudLayer::GetSRS(const char* pszGeomCol,
                                             int *pnSRID)
{
    json_object* poObj = poDS->RunSQL(GetSRS_SQL(pszGeomCol));
    json_object* poRowObj = OGRAMIGOCLOUDGetSingleRow(poObj);
    if( poRowObj == NULL )
    {
        if( poObj != NULL )
            json_object_put(poObj);
        return NULL;
    }

    json_object* poSRID = json_object_object_get(poRowObj, "srid");
    if( poSRID != NULL && json_object_get_type(poSRID) == json_type_int )
    {
        *pnSRID = json_object_get_int(poSRID);
    }

    json_object* poSRTEXT = json_object_object_get(poRowObj, "srtext");
    OGRSpatialReference* poSRS = NULL;
    if( poSRTEXT != NULL && json_object_get_type(poSRTEXT) == json_type_string )
    {
        const char* pszSRTEXT = json_object_get_string(poSRTEXT);
        poSRS = new OGRSpatialReference();
        char* pszTmp = (char* )pszSRTEXT;
        if( poSRS->importFromWkt(&pszTmp) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }
    json_object_put(poObj);

    return poSRS;
}
