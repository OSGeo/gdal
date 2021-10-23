/******************************************************************************
 *
 * Project:  Carto Translator
 * Purpose:  Implements OGRCARTOLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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
    poFeatureDefn(nullptr),
    bEOF(false),
    nFetchedObjects(-1),
    iNextInFetchedObjects(0),
    m_nNextFID(0),
    m_nNextOffset(0),
    poCachedObj(nullptr)
{
}

/************************************************************************/
/*                         ~OGRCARTOLayer()                           */
/************************************************************************/

OGRCARTOLayer::~OGRCARTOLayer()

{
    if( poCachedObj != nullptr )
        json_object_put(poCachedObj);

    if( poFeatureDefn != nullptr )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCARTOLayer::ResetReading()

{
    if( poCachedObj != nullptr )
        json_object_put(poCachedObj);
    poCachedObj = nullptr;
    bEOF = false;
    nFetchedObjects = -1;
    iNextInFetchedObjects = 0;
    m_nNextOffset = 0;
    m_nNextFID = 0;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRCARTOLayer::GetLayerDefn()
{
    return GetLayerDefnInternal(nullptr);
}

/************************************************************************/
/*                           BuildFeature()                             */
/************************************************************************/

OGRFeature *OGRCARTOLayer::BuildFeature(json_object* poRowObj)
{
    OGRFeature* poFeature = nullptr;
    if( poRowObj != nullptr &&
        json_object_get_type(poRowObj) == json_type_object )
    {
        //CPLDebug("Carto", "Row: %s", json_object_to_json_string(poRowObj));
        poFeature = new OGRFeature(poFeatureDefn);

        if( !osFIDColName.empty() )
        {
            json_object* poVal = CPL_json_object_object_get(poRowObj, osFIDColName);
            if( poVal != nullptr &&
                json_object_get_type(poVal) == json_type_int )
            {
                poFeature->SetFID(json_object_get_int64(poVal));
            }
        }
        else
        {
            poFeature->SetFID(m_nNextFID);
        }

        for(int i=0;i<poFeatureDefn->GetFieldCount();i++)
        {
            json_object* poVal = CPL_json_object_object_get(poRowObj,
                            poFeatureDefn->GetFieldDefn(i)->GetNameRef());
            if( poVal == nullptr )
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
            if( poVal != nullptr &&
                json_object_get_type(poVal) == json_type_string )
            {
                OGRGeometry* poGeom = OGRGeometryFromHexEWKB(
                    json_object_get_string(poVal), nullptr, FALSE);
                if( poGeom != nullptr )
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

json_object* OGRCARTOLayer::FetchNewFeatures()
{
    CPLString osSQL = osBaseSQL;
    if( osSQL.ifind("SELECT") != std::string::npos &&
        osSQL.ifind(" LIMIT ") == std::string::npos )
    {
        osSQL += " LIMIT ";
        osSQL += CPLSPrintf("%d", GetFeaturesToFetch());
        osSQL += " OFFSET ";
        osSQL += CPLSPrintf(CPL_FRMT_GIB, m_nNextOffset);
    }
    return poDS->RunSQL(osSQL);
}

/************************************************************************/
/*                        GetNextRawFeature()                           */
/************************************************************************/

OGRFeature *OGRCARTOLayer::GetNextRawFeature()
{
    if( bEOF )
        return nullptr;

    if( iNextInFetchedObjects >= nFetchedObjects )
    {
        if( nFetchedObjects > 0 && nFetchedObjects < GetFeaturesToFetch() )
        {
            bEOF = true;
            return nullptr;
        }

        if( poFeatureDefn == nullptr && osBaseSQL.empty() )
        {
            GetLayerDefn();
        }

        json_object* poObj = FetchNewFeatures();
        if( poObj == nullptr )
        {
            bEOF = true;
            return nullptr;
        }

        if( poFeatureDefn == nullptr )
        {
            GetLayerDefnInternal(poObj);
        }

        json_object* poRows = CPL_json_object_object_get(poObj, "rows");
        if( poRows == nullptr ||
            json_object_get_type(poRows) != json_type_array ||
            json_object_array_length(poRows) == 0 )
        {
            json_object_put(poObj);
            bEOF = true;
            return nullptr;
        }

        if( poCachedObj != nullptr )
            json_object_put(poCachedObj);
        poCachedObj = poObj;

        nFetchedObjects = static_cast<decltype(nFetchedObjects)>(json_object_array_length(poRows));
        iNextInFetchedObjects = 0;
    }

    json_object* poRows = CPL_json_object_object_get(poCachedObj, "rows");
    json_object* poRowObj = json_object_array_get_idx(poRows, iNextInFetchedObjects);

    iNextInFetchedObjects ++;

    OGRFeature* poFeature = BuildFeature(poRowObj);
    m_nNextOffset ++;
    m_nNextFID = poFeature->GetFID() + 1;

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
        if (poFeature == nullptr)
            return nullptr;

        if((m_poFilterGeom == nullptr
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == nullptr
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
    if( poObjIn != nullptr )
        poObj = poObjIn;
    else
    {
        poObj = poDS->RunSQL(osSQL);
        if( poObj == nullptr )
        {
            return;
        }
    }

    json_object* poFields = CPL_json_object_object_get(poObj, "fields");
    if( poFields == nullptr || json_object_get_type(poFields) != json_type_object)
    {
        if( poObjIn == nullptr )
            json_object_put(poObj);
        return;
    }

    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    json_object_object_foreachC( poFields, it )
    {
        const char* pszColName = it.key;
        if( it.val != nullptr && json_object_get_type(it.val) == json_type_object)
        {
            json_object* poType = CPL_json_object_object_get(it.val, "type");
            if( poType != nullptr && json_object_get_type(poType) == json_type_string )
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
                        auto poFieldDefn =
                            cpl::make_unique<OGRCartoGeomFieldDefn>(pszColName, wkbUnknown);
                        OGRSpatialReference* l_poSRS = GetSRS(pszColName, &poFieldDefn->nSRID);
                        if( l_poSRS != nullptr )
                        {
                            poFieldDefn->SetSpatialRef(l_poSRS);
                            l_poSRS->Release();
                        }
                        poFeatureDefn->AddGeomFieldDefn(std::move(poFieldDefn));
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
            else if( poType != nullptr && json_object_get_type(poType) == json_type_int )
            {
                /* FIXME? manual creations of geometry columns return integer types */
                auto poFieldDefn =
                    cpl::make_unique<OGRCartoGeomFieldDefn>(pszColName, wkbUnknown);
                OGRSpatialReference* l_poSRS = GetSRS(pszColName, &poFieldDefn->nSRID);
                if( l_poSRS != nullptr )
                {
                    poFieldDefn->SetSpatialRef(l_poSRS);
                    l_poSRS->Release();
                }
                poFeatureDefn->AddGeomFieldDefn(std::move(poFieldDefn));
            }
        }
    }
    if( poObjIn == nullptr )
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
    if( poRowObj == nullptr )
    {
        if( poObj != nullptr )
            json_object_put(poObj);
        return nullptr;
    }

    json_object* poSRID = CPL_json_object_object_get(poRowObj, "srid");
    if( poSRID != nullptr && json_object_get_type(poSRID) == json_type_int )
    {
        *pnSRID = json_object_get_int(poSRID);
    }

    json_object* poSRTEXT = CPL_json_object_object_get(poRowObj, "srtext");
    OGRSpatialReference* l_poSRS = nullptr;
    if( poSRTEXT != nullptr && json_object_get_type(poSRTEXT) == json_type_string )
    {
        const char* pszSRTEXT = json_object_get_string(poSRTEXT);
        l_poSRS = new OGRSpatialReference();
        l_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( l_poSRS->importFromWkt(pszSRTEXT) != OGRERR_NONE )
        {
            delete l_poSRS;
            l_poSRS = nullptr;
        }
    }
    json_object_put(poObj);

    return l_poSRS;
}
