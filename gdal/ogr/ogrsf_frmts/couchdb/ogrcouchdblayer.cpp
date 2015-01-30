/******************************************************************************
 * $Id$
 *
 * Project:  CouchDB Translator
 * Purpose:  Implements OGRCouchDBLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRCouchDBLayer()                             */
/************************************************************************/

OGRCouchDBLayer::OGRCouchDBLayer(OGRCouchDBDataSource* poDS)

{
    this->poDS = poDS;

    nNextInSeq = 0;

    poSRS = NULL;

    poFeatureDefn = NULL;

    nOffset = 0;
    bEOF = FALSE;

    poFeatures = NULL;

    bGeoJSONDocument = TRUE;
}

/************************************************************************/
/*                            ~OGRCouchDBLayer()                            */
/************************************************************************/

OGRCouchDBLayer::~OGRCouchDBLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    if( poFeatureDefn != NULL )
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
    bEOF = FALSE;
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

    while(TRUE)
    {
        if (nNextInSeq < nOffset ||
            nNextInSeq >= nOffset + (int)aoFeatures.size())
        {
            if (bEOF)
                return NULL;

            nOffset += aoFeatures.size();
            if (!FetchNextRows())
                return NULL;
        }

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
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRCouchDBLayer::GetNextRawFeature()
{
    if (nNextInSeq < nOffset ||
        nNextInSeq - nOffset >= (int)aoFeatures.size())
        return NULL;

    OGRFeature* poFeature = TranslateFeature(aoFeatures[nNextInSeq - nOffset]);
    if (poFeature != NULL && poFeature->GetFID() == OGRNullFID)
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
    bEOF = FALSE;
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
    OGRFeature* poFeature = NULL;
    poFeature = new OGRFeature( GetLayerDefn() );

    json_object* poId = json_object_object_get(poObj, "_id");
    const char* pszId = json_object_get_string(poId);
    if (pszId)
    {
        poFeature->SetField(_ID_FIELD, pszId);

        int nFID = atoi(pszId);
        const char* pszFID = CPLSPrintf("%09d", nFID);
        if (strcmp(pszId, pszFID) == 0)
            poFeature->SetFID(nFID);
    }

    json_object* poRev = json_object_object_get(poObj, "_rev");
    const char* pszRev = json_object_get_string(poRev);
    if (pszRev)
        poFeature->SetField(_REV_FIELD, pszRev);

/* -------------------------------------------------------------------- */
/*      Translate GeoJSON "properties" object to feature attributes.    */
/* -------------------------------------------------------------------- */

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    if( bGeoJSONDocument )
    {
        json_object* poObjProps = json_object_object_get( poObj, "properties" );
        if ( NULL != poObjProps &&
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

    json_object* poObjGeom = json_object_object_get( poObj, "geometry" );
    if (poObjGeom != NULL)
    {
        OGRGeometry* poGeometry = OGRGeoJSONReadGeometry( poObjGeom );
        if( NULL != poGeometry )
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
    else if (poValue != NULL)
    {
        OGRFieldDefn* poFieldDefn = poFeature->GetFieldDefnRef(nField);
        CPLAssert(poFieldDefn != NULL);
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
                int nLength = json_object_array_length(poValue);
                int* panVal = (int*)CPLMalloc(sizeof(int) * nLength);
                for(int i=0;i<nLength;i++)
                {
                    json_object* poRow = json_object_array_get_idx(poValue, i);
                    panVal[i] = json_object_get_int(poRow);
                }
                poFeature->SetField( nField, nLength, panVal );
                CPLFree(panVal);
            }
        }
        else if( OFTRealList == eType )
        {
            if ( json_object_get_type(poValue) == json_type_array )
            {
                int nLength = json_object_array_length(poValue);
                double* padfVal = (double*)CPLMalloc(sizeof(double) * nLength);
                for(int i=0;i<nLength;i++)
                {
                    json_object* poRow = json_object_array_get_idx(poValue, i);
                    padfVal[i] = json_object_get_double(poRow);
                }
                poFeature->SetField( nField, nLength, padfVal );
                CPLFree(padfVal);
            }
        }
        else if( OFTStringList == eType )
        {
            if ( json_object_get_type(poValue) == json_type_array )
            {
                int nLength = json_object_array_length(poValue);
                char** papszVal = (char**)CPLMalloc(sizeof(char*) * (nLength+1));
                int i;
                for(i=0;i<nLength;i++)
                {
                    json_object* poRow = json_object_array_get_idx(poValue, i);
                    const char* pszVal = json_object_get_string(poRow);
                    if (pszVal == NULL)
                        break;
                    papszVal[i] = CPLStrdup(pszVal);
                }
                papszVal[i] = NULL;
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
    json_object* poObjProps = json_object_object_get( poDoc,
                                                        "properties" );
    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    if( NULL != poObjProps && json_object_get_type(poObjProps) == json_type_object )
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
        bGeoJSONDocument = FALSE;

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

    if( json_object_object_get( poDoc, "geometry" ) == NULL )
    {
        poFeatureDefn->SetGeomType(wkbNone);
    }
}


/************************************************************************/
/*                      BuildFeatureDefnFromRows()                      */
/************************************************************************/

int OGRCouchDBLayer::BuildFeatureDefnFromRows(json_object* poAnswerObj)
{
    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Layer definition creation failed");
        return FALSE;
    }

    if (poDS->IsError(poAnswerObj, "Layer definition creation failed"))
    {
        return FALSE;
    }

    json_object* poRows = json_object_object_get(poAnswerObj, "rows");
    if (poRows == NULL ||
        !json_object_is_type(poRows, json_type_array))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Layer definition creation failed");
        return FALSE;
    }

    int nRows = json_object_array_length(poRows);

    json_object* poRow = NULL;
    for(int i=0;i<nRows;i++)
    {
        json_object* poTmpRow = json_object_array_get_idx(poRows, i);
        if (poTmpRow != NULL &&
            json_object_is_type(poTmpRow, json_type_object))
        {
            json_object* poId = json_object_object_get(poTmpRow, "id");
            const char* pszId = json_object_get_string(poId);
            if (pszId != NULL && pszId[0] != '_')
            {
                poRow = poTmpRow;
                break;
            }
        }
    }

    if ( poRow == NULL )
    {
        return FALSE;
    }

    json_object* poDoc = json_object_object_get(poRow, "doc");
    if ( poDoc == NULL )
        poDoc = json_object_object_get(poRow, "value");
    if ( poDoc == NULL ||
            !json_object_is_type(poDoc, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Layer definition creation failed");
        return FALSE;
    }

    BuildFeatureDefnFromDoc(poDoc);

    return TRUE;
}

/************************************************************************/
/*                   FetchNextRowsAnalyseDocs()                         */
/************************************************************************/

int OGRCouchDBLayer::FetchNextRowsAnalyseDocs(json_object* poAnswerObj)
{
    if (poAnswerObj == NULL)
        return FALSE;

    if ( !json_object_is_type(poAnswerObj, json_type_object) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FetchNextRowsAnalyseDocs() failed");
        json_object_put(poAnswerObj);
        return FALSE;
    }

    if (poDS->IsError(poAnswerObj, "FetchNextRowsAnalyseDocs() failed"))
    {
        json_object_put(poAnswerObj);
        return FALSE;
    }

    json_object* poRows = json_object_object_get(poAnswerObj, "rows");
    if (poRows == NULL ||
        !json_object_is_type(poRows, json_type_array))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "FetchNextRowsAnalyseDocs() failed");
        json_object_put(poAnswerObj);
        return FALSE;
    }

    int nRows = json_object_array_length(poRows);
    for(int i=0;i<nRows;i++)
    {
        json_object* poRow = json_object_array_get_idx(poRows, i);
        if ( poRow == NULL ||
             !json_object_is_type(poRow, json_type_object) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "FetchNextRowsAnalyseDocs() failed");
            json_object_put(poAnswerObj);
            return FALSE;
        }

        json_object* poDoc = json_object_object_get(poRow, "doc");
        if ( poDoc == NULL )
            poDoc = json_object_object_get(poRow, "value");
        if ( poDoc == NULL ||
             !json_object_is_type(poDoc, json_type_object) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "FetchNextRowsAnalyseDocs() failed");
            json_object_put(poAnswerObj);
            return FALSE;
        }

        json_object* poId = json_object_object_get(poDoc, "_id");
        const char* pszId = json_object_get_string(poId);
        if (pszId != NULL && strncmp(pszId, "_design/", 8) != 0)
        {
            aoFeatures.push_back(poDoc);
        }
    }

    bEOF = nRows < GetFeaturesToFetch();

    poFeatures = poAnswerObj;

    return TRUE;
}

/************************************************************************/
/*              	     GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference* OGRCouchDBLayer::GetSpatialRef()
{
    GetLayerDefn();
    return poSRS;
}
