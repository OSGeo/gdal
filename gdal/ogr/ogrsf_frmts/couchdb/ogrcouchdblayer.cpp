/******************************************************************************
 *
 * Project:  CouchDB Translator
 * Purpose:  Implements OGRCouchDBLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogr_couchdb.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRCouchDBLayer()                             */
/************************************************************************/

OGRCouchDBLayer::OGRCouchDBLayer(OGRCouchDBDataSource* poDSIn) :
    poDS(poDSIn),
    poFeatureDefn(nullptr),
    poSRS(nullptr),
    nNextInSeq(0),
    nOffset(0),
    bEOF(false),
    poFeatures(nullptr),
    bGeoJSONDocument(true)
{}

/************************************************************************/
/*                            ~OGRCouchDBLayer()                            */
/************************************************************************/

OGRCouchDBLayer::~OGRCouchDBLayer()

{
    if( poSRS != nullptr )
        poSRS->Release();

    if( poFeatureDefn != nullptr )
        poFeatureDefn->Release();

    json_object_put(poFeatures);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRCouchDBLayer::ResetReading()

{
    nNextInSeq = 0;
    nOffset = 0;
    bEOF = false;
}

/************************************************************************/
/*                           GetLayerDefn()                             */
/************************************************************************/

OGRFeatureDefn * OGRCouchDBLayer::GetLayerDefn()
{
    CPLAssert(poFeatureDefn);
    return poFeatureDefn;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRCouchDBLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    GetLayerDefn();

    while( true )
    {
        if (nNextInSeq < nOffset ||
            nNextInSeq >= nOffset + static_cast<int>(aoFeatures.size()))
        {
            if( bEOF )
                return nullptr;

            nOffset += static_cast<int>(aoFeatures.size());
            if( !FetchNextRows() )
                return nullptr;
        }

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
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRCouchDBLayer::GetNextRawFeature()
{
    if (nNextInSeq < nOffset ||
        nNextInSeq - nOffset >= (int)aoFeatures.size())
        return nullptr;

    OGRFeature* poFeature = TranslateFeature(aoFeatures[nNextInSeq - nOffset]);
    if (poFeature != nullptr && poFeature->GetFID() == OGRNullFID)
        poFeature->SetFID(nNextInSeq);

    nNextInSeq ++;

    return poFeature;
}

/************************************************************************/
/*                          SetNextByIndex()                            */
/************************************************************************/

OGRErr OGRCouchDBLayer::SetNextByIndex( GIntBig nIndex )
{
    if (nIndex < 0 || nIndex >= INT_MAX )
        return OGRERR_FAILURE;
    bEOF = false;
    nNextInSeq = (int)nIndex;
    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCouchDBLayer::TestCapability( const char * pszCap )

{
    if ( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;
    else if ( EQUAL(pszCap, OLCFastSetNextByIndex) )
        return TRUE;
    return FALSE;
}

/************************************************************************/
/*                         TranslateFeature()                            */
/************************************************************************/

OGRFeature* OGRCouchDBLayer::TranslateFeature( json_object* poObj )
{
    OGRFeature* poFeature = new OGRFeature( GetLayerDefn() );

    json_object* poId = CPL_json_object_object_get(poObj, "_id");
    const char* pszId = json_object_get_string(poId);
    if (pszId)
    {
        poFeature->SetField(COUCHDB_ID_FIELD, pszId);

        int nFID = atoi(pszId);
        const char* pszFID = CPLSPrintf("%09d", nFID);
        if (strcmp(pszId, pszFID) == 0)
            poFeature->SetFID(nFID);
    }

    json_object* poRev = CPL_json_object_object_get(poObj, "_rev");
    const char* pszRev = json_object_get_string(poRev);
    if (pszRev)
        poFeature->SetField(COUCHDB_REV_FIELD, pszRev);

/* -------------------------------------------------------------------- */
/*      Translate GeoJSON "properties" object to feature attributes.    */
/* -------------------------------------------------------------------- */

    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    if( bGeoJSONDocument )
    {
        json_object* poObjProps = CPL_json_object_object_get( poObj, "properties" );
        if ( nullptr != poObjProps &&
             json_object_get_type(poObjProps ) == json_type_object )
        {
            json_object_object_foreachC( poObjProps, it )
            {
                ParseFieldValue(poFeature, it.key, it.val);
            }
        }
    }
    else
    {
        json_object_object_foreachC( poObj, it )
        {
            if( strcmp(it.key, "_id") != 0 &&
                strcmp(it.key, "_rev") != 0 &&
                strcmp(it.key, "geometry") != 0 )
            {
                ParseFieldValue(poFeature, it.key, it.val);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate geometry sub-object of GeoJSON Feature.               */
/* -------------------------------------------------------------------- */

    json_object* poObjGeom = CPL_json_object_object_get( poObj, "geometry" );
    if (poObjGeom != nullptr)
    {
        OGRGeometry* poGeometry = OGRGeoJSONReadGeometry( poObjGeom );
        if( nullptr != poGeometry )
        {
            if (poSRS)
                poGeometry->assignSpatialReference(poSRS);
            poFeature->SetGeometryDirectly( poGeometry );
        }
    }

    return poFeature;
}

/************************************************************************/
/*                         ParseFieldValue()                            */
/************************************************************************/

void OGRCouchDBLayer::ParseFieldValue(OGRFeature* poFeature,
                                      const char* pszKey,
                                      json_object* poValue)
{
    int nField = poFeature->GetFieldIndex(pszKey);
    if (nField < 0)
    {
        CPLDebug("CouchDB",
                    "Found field '%s' which is not in the layer definition. "
                    "Ignoring its value",
                    pszKey);
    }
    else if (poValue == nullptr)
    {
        poFeature->SetFieldNull( nField );
    }
    else
    {
        OGRFieldDefn* poFieldDefn = poFeature->GetFieldDefnRef(nField);
        CPLAssert(poFieldDefn != nullptr);
        OGRFieldType eType = poFieldDefn->GetType();

        if( OFTInteger == eType )
        {
            poFeature->SetField( nField, json_object_get_int(poValue) );
        }
        else if( OFTReal == eType )
        {
            poFeature->SetField( nField, json_object_get_double(poValue) );
        }
        else if( OFTIntegerList == eType )
        {
            if ( json_object_get_type(poValue) == json_type_array )
            {
                const auto nLength = json_object_array_length(poValue);
                int* panVal = static_cast<int *>(
                    CPLMalloc(sizeof(int) * nLength));
                for( auto i = decltype(nLength){0}; i < nLength; i++ )
                {
                    json_object* poRow = json_object_array_get_idx(poValue, i);
                    panVal[i] = json_object_get_int(poRow);
                }
                poFeature->SetField( nField, static_cast<int>(nLength), panVal );
                CPLFree(panVal);
            }
        }
        else if( OFTRealList == eType )
        {
            if ( json_object_get_type(poValue) == json_type_array )
            {
                const auto nLength = json_object_array_length(poValue);
                double* padfVal = static_cast<double *>(
                    CPLMalloc(sizeof(double) * nLength));
                for( auto i = decltype(nLength){0}; i < nLength; i++ )
                {
                    json_object* poRow = json_object_array_get_idx(poValue, i);
                    padfVal[i] = json_object_get_double(poRow);
                }
                poFeature->SetField( nField, static_cast<int>(nLength), padfVal );
                CPLFree(padfVal);
            }
        }
        else if( OFTStringList == eType )
        {
            if ( json_object_get_type(poValue) == json_type_array )
            {
                auto nLength = json_object_array_length(poValue);
                char** papszVal = static_cast<char **>(
                    CPLMalloc(sizeof(char*) * (nLength+1)));
                decltype(nLength) i = 0; // Used after for.
                for( ; i < nLength; i++ )
                {
                    json_object* poRow = json_object_array_get_idx(poValue, i);
                    const char* pszVal = json_object_get_string(poRow);
                    if (pszVal == nullptr)
                        break;
                    papszVal[i] = CPLStrdup(pszVal);
                }
                papszVal[i] = nullptr;
                poFeature->SetField( nField, papszVal );
                CSLDestroy(papszVal);
            }
        }
        else
        {
            poFeature->SetField( nField, json_object_get_string(poValue) );
        }
    }
}

/************************************************************************/
/*                      BuildFeatureDefnFromDoc()                       */
/************************************************************************/

void OGRCouchDBLayer::BuildFeatureDefnFromDoc(json_object* poDoc)
{
/* -------------------------------------------------------------------- */
/*      Read collection of properties.                                  */
/* -------------------------------------------------------------------- */
    json_object* poObjProps = CPL_json_object_object_get( poDoc,
                                                        "properties" );
    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    if( nullptr != poObjProps && json_object_get_type(poObjProps) == json_type_object )
    {
        json_object_object_foreachC( poObjProps, it )
        {
            if( -1 == poFeatureDefn->GetFieldIndex( it.key ) )
            {
                OGRFieldSubType eSubType;
                OGRFieldDefn fldDefn( it.key,
                    GeoJSONPropertyToFieldType( it.val, eSubType ) );
                poFeatureDefn->AddFieldDefn( &fldDefn );
            }
        }
    }
    else
    {
        bGeoJSONDocument = false;

        json_object_object_foreachC( poDoc, it )
        {
            if( strcmp(it.key, "_id") != 0 &&
                strcmp(it.key, "_rev") != 0 &&
                strcmp(it.key, "geometry") != 0 &&
                -1 == poFeatureDefn->GetFieldIndex( it.key ) )
            {
                OGRFieldSubType eSubType;
                OGRFieldDefn fldDefn( it.key,
                    GeoJSONPropertyToFieldType( it.val, eSubType ) );
                poFeatureDefn->AddFieldDefn( &fldDefn );
            }
        }
    }

    if( CPL_json_object_object_get( poDoc, "geometry" ) == nullptr )
    {
        poFeatureDefn->SetGeomType(wkbNone);
    }
}

/************************************************************************/
/*                      BuildFeatureDefnFromRows()                      */
/************************************************************************/

bool OGRCouchDBLayer::BuildFeatureDefnFromRows( json_object* poAnswerObj )
{
    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Layer definition creation failed");
        return false;
    }

    if (poDS->IsError(poAnswerObj, "Layer definition creation failed"))
    {
        return false;
    }

    json_object* poRows = CPL_json_object_object_get(poAnswerObj, "rows");
    if (poRows == nullptr ||
        !json_object_is_type(poRows, json_type_array))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Layer definition creation failed");
        return false;
    }

    const auto nRows = json_object_array_length(poRows);

    json_object* poRow = nullptr;
    for(auto i=decltype(nRows){0};i<nRows;i++)
    {
        json_object* poTmpRow = json_object_array_get_idx(poRows, i);
        if (poTmpRow != nullptr &&
            json_object_is_type(poTmpRow, json_type_object))
        {
            json_object* poId = CPL_json_object_object_get(poTmpRow, "id");
            const char* pszId = json_object_get_string(poId);
            if (pszId != nullptr && pszId[0] != '_')
            {
                poRow = poTmpRow;
                break;
            }
        }
    }

    if ( poRow == nullptr )
    {
        return false;
    }

    json_object* poDoc = CPL_json_object_object_get(poRow, "doc");
    if ( poDoc == nullptr )
        poDoc = CPL_json_object_object_get(poRow, "value");
    if ( poDoc == nullptr ||
            !json_object_is_type(poDoc, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Layer definition creation failed");
        return false;
    }

    BuildFeatureDefnFromDoc(poDoc);

    return true;
}

/************************************************************************/
/*                   FetchNextRowsAnalyseDocs()                         */
/************************************************************************/

bool OGRCouchDBLayer::FetchNextRowsAnalyseDocs( json_object* poAnswerObj )
{
    if (poAnswerObj == nullptr)
        return false;

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FetchNextRowsAnalyseDocs() failed");
        json_object_put(poAnswerObj);
        return false;
    }

    if (poDS->IsError(poAnswerObj, "FetchNextRowsAnalyseDocs() failed"))
    {
        json_object_put(poAnswerObj);
        return false;
    }

    json_object* poRows = CPL_json_object_object_get(poAnswerObj, "rows");
    if (poRows == nullptr ||
        !json_object_is_type(poRows, json_type_array))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FetchNextRowsAnalyseDocs() failed");
        json_object_put(poAnswerObj);
        return false;
    }

    const auto nRows = json_object_array_length(poRows);
    for(auto i=decltype(nRows){0};i<nRows;i++)
    {
        json_object* poRow = json_object_array_get_idx(poRows, i);
        if ( poRow == nullptr ||
             !json_object_is_type(poRow, json_type_object) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "FetchNextRowsAnalyseDocs() failed");
            json_object_put(poAnswerObj);
            return false;
        }

        json_object* poDoc = CPL_json_object_object_get(poRow, "doc");
        if ( poDoc == nullptr )
            poDoc = CPL_json_object_object_get(poRow, "value");
        if ( poDoc == nullptr ||
             !json_object_is_type(poDoc, json_type_object) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "FetchNextRowsAnalyseDocs() failed");
            json_object_put(poAnswerObj);
            return false;
        }

        json_object* poId = CPL_json_object_object_get(poDoc, "_id");
        const char* pszId = json_object_get_string(poId);
        if (pszId != nullptr && !STARTS_WITH(pszId, "_design/"))
        {
            aoFeatures.push_back(poDoc);
        }
    }

    bEOF = static_cast<int>(nRows) < GetFeaturesToFetch();

    poFeatures = poAnswerObj;

    return true;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference* OGRCouchDBLayer::GetSpatialRef()
{
    GetLayerDefn();
    return poSRS;
}
