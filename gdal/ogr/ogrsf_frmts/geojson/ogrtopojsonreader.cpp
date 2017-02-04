/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRTopoJSONReader class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
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

#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
#include "ogr_geojson.h"
#include <json.h>  // JSON-C
#include <ogr_api.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRTopoJSONReader()                         */
/************************************************************************/

OGRTopoJSONReader::OGRTopoJSONReader() : poGJObject_( NULL ) {}

/************************************************************************/
/*                         ~OGRTopoJSONReader()                         */
/************************************************************************/

OGRTopoJSONReader::~OGRTopoJSONReader()
{
    if( NULL != poGJObject_ )
    {
        json_object_put(poGJObject_);
    }

    poGJObject_ = NULL;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

OGRErr OGRTopoJSONReader::Parse( const char* pszText )
{
    if( NULL != pszText )
    {
        json_tokener *jstok = json_tokener_new();
        json_object *jsobj = json_tokener_parse_ex(jstok, pszText, -1);
        if( jstok->err != json_tokener_success)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "TopoJSON parsing error: %s (at offset %d)",
                      json_tokener_error_desc(jstok->err), jstok->char_offset );

            json_tokener_free(jstok);
            return OGRERR_CORRUPT_DATA;
        }
        json_tokener_free(jstok);

        // JSON tree is shared for while lifetime of the reader object
        // and will be released in the destructor.
        poGJObject_ = jsobj;
    }

    return OGRERR_NONE;
}

typedef struct
{
    double dfScale0;
    double dfScale1;
    double dfTranslate0;
    double dfTranslate1;
    bool bElementExists;
} ScalingParams;

/************************************************************************/
/*                            ParsePoint()                              */
/************************************************************************/

static bool ParsePoint( json_object* poPoint, double* pdfX, double* pdfY )
{
    if( poPoint != NULL && json_type_array == json_object_get_type(poPoint) &&
        json_object_array_length(poPoint) == 2 )
    {
        json_object* poX = json_object_array_get_idx(poPoint, 0);
        json_object* poY = json_object_array_get_idx(poPoint, 1);
        if( poX != NULL &&
            (json_type_int == json_object_get_type(poX) ||
                json_type_double == json_object_get_type(poX)) &&
            poY != NULL &&
            (json_type_int == json_object_get_type(poY) ||
                json_type_double == json_object_get_type(poY)) )
        {
            *pdfX = json_object_get_double(poX);
            *pdfY = json_object_get_double(poY);
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                             ParseArc()                               */
/************************************************************************/

static void ParseArc( OGRLineString* poLS, json_object* poArcsDB, int nArcID,
                      bool bReverse, ScalingParams* psParams )
{
    json_object* poArcDB = json_object_array_get_idx(poArcsDB, nArcID);
    if( poArcDB == NULL || json_type_array != json_object_get_type(poArcDB) )
        return;
    int nPoints = json_object_array_length(poArcDB);
    double dfAccX = 0.0;
    double dfAccY = 0.0;
    int nBaseIndice = poLS->getNumPoints();
    for( int i = 0; i < nPoints; i++ )
    {
        json_object* poPoint = json_object_array_get_idx(poArcDB, i);
        double dfX = 0.0;
        double dfY = 0.0;
        if( ParsePoint( poPoint, &dfX, &dfY ) )
        {
            if( psParams->bElementExists )
            {
                dfAccX += dfX;
                dfAccY += dfY;
                dfX = dfAccX * psParams->dfScale0 + psParams->dfTranslate0;
                dfY = dfAccY * psParams->dfScale1 + psParams->dfTranslate1;
            }
            else
            {
                dfX = dfX * psParams->dfScale0 + psParams->dfTranslate0;
                dfY = dfY * psParams->dfScale1 + psParams->dfTranslate1;
            }
            if( i == 0 )
            {
                if( !bReverse && poLS->getNumPoints() > 0 )
                {
                    poLS->setNumPoints( nBaseIndice + nPoints - 1 );
                    nBaseIndice --;
                    continue;
                }
                else if( bReverse && poLS->getNumPoints() > 0 )
                {
                    poLS->setNumPoints( nBaseIndice + nPoints - 1 );
                    nPoints --;
                    if( nPoints == 0 )
                        break;
                }
                else
                    poLS->setNumPoints( nBaseIndice + nPoints );
            }

            if( !bReverse )
                poLS->setPoint(nBaseIndice + i, dfX, dfY);
            else
                poLS->setPoint(nBaseIndice + nPoints - 1 - i, dfX, dfY);
        }
    }
}

/************************************************************************/
/*                        ParseLineString()                             */
/************************************************************************/

static void ParseLineString( OGRLineString* poLS, json_object* poRing,
                             json_object* poArcsDB, ScalingParams* psParams )
{
    const int nArcsDB = json_object_array_length(poArcsDB);

    const int nArcsRing = json_object_array_length(poRing);
    for( int j = 0; j < nArcsRing; j++ )
    {
        json_object* poArcId = json_object_array_get_idx(poRing, j);
        if( poArcId != NULL && json_type_int == json_object_get_type(poArcId) )
        {
            int nArcId = json_object_get_int(poArcId);
            bool bReverse = false;
            if( nArcId < 0 )
            {
                nArcId = - nArcId - 1;
                bReverse = true;
            }
            if( nArcId < nArcsDB )
            {
                ParseArc(poLS, poArcsDB, nArcId, bReverse, psParams);
            }
        }
    }
}

/************************************************************************/
/*                          ParsePolygon()                              */
/************************************************************************/

static void ParsePolygon( OGRPolygon* poPoly, json_object* poArcsObj,
                          json_object* poArcsDB, ScalingParams* psParams )
{
    const int nRings = json_object_array_length(poArcsObj);
    for( int i = 0; i < nRings; i++ )
    {
        OGRLinearRing* poLR = new OGRLinearRing();

        json_object* poRing = json_object_array_get_idx(poArcsObj, i);
        if( poRing != NULL && json_type_array == json_object_get_type(poRing) )
        {
            ParseLineString(poLR, poRing, poArcsDB, psParams);
        }
        poLR->closeRings();
        if( poLR->getNumPoints() < 4 )
        {
            CPLDebug("TopoJSON", "Discarding polygon ring made of %d points",
                     poLR->getNumPoints());
            delete poLR;
        }
        else
        {
            poPoly->addRingDirectly(poLR);
        }
    }
}

/************************************************************************/
/*                       ParseMultiLineString()                         */
/************************************************************************/

static void ParseMultiLineString( OGRMultiLineString* poMLS,
                                  json_object* poArcsObj,
                                  json_object* poArcsDB,
                                  ScalingParams* psParams )
{
    const int nRings = json_object_array_length(poArcsObj);
    for( int i = 0; i < nRings; i++ )
    {
        OGRLineString* poLS = new OGRLineString();
        poMLS->addGeometryDirectly(poLS);

        json_object* poRing = json_object_array_get_idx(poArcsObj, i);
        if( poRing != NULL && json_type_array == json_object_get_type(poRing) )
        {
            ParseLineString(poLS, poRing, poArcsDB, psParams);
        }
    }
}

/************************************************************************/
/*                       ParseMultiPolygon()                            */
/************************************************************************/

static void ParseMultiPolygon( OGRMultiPolygon* poMultiPoly,
                               json_object* poArcsObj,
                               json_object* poArcsDB, ScalingParams* psParams )
{
    const int nPolys = json_object_array_length(poArcsObj);
    for( int i = 0; i < nPolys; i++ )
    {
        OGRPolygon* poPoly = new OGRPolygon();

        json_object* poPolyArcs = json_object_array_get_idx(poArcsObj, i);
        if( poPolyArcs != NULL &&
            json_type_array == json_object_get_type(poPolyArcs) )
        {
            ParsePolygon(poPoly, poPolyArcs, poArcsDB, psParams);
        }

        if( poPoly->IsEmpty() )
        {
            delete poPoly;
        }
        else
        {
            poMultiPoly->addGeometryDirectly(poPoly);
        }
    }
}

/************************************************************************/
/*                          ParseObject()                               */
/************************************************************************/

static void ParseObject( const char* pszId,
                         json_object* poObj, OGRGeoJSONLayer* poLayer,
                         json_object* poArcsDB, ScalingParams* psParams )
{
    json_object* poType = OGRGeoJSONFindMemberByName(poObj, "type");
    if( poType == NULL || json_object_get_type(poType) != json_type_string )
        return;
    const char* pszType = json_object_get_string(poType);

    json_object* poArcsObj = OGRGeoJSONFindMemberByName(poObj, "arcs");
    json_object* poCoordinatesObj =
        OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if( strcmp(pszType, "Point") == 0 || strcmp(pszType, "MultiPoint") == 0 )
    {
        if( poCoordinatesObj == NULL ||
            json_type_array != json_object_get_type(poCoordinatesObj) )
            return;
    }
    else
    {
        if( poArcsObj == NULL ||
            json_type_array != json_object_get_type(poArcsObj) )
            return;
    }

    if( pszId == NULL )
    {
        json_object* poId = OGRGeoJSONFindMemberByName(poObj, "id");
        if( poId != NULL &&
            (json_type_string == json_object_get_type(poId) ||
             json_type_int == json_object_get_type(poId)) )
        {
            pszId = json_object_get_string(poId);
        }
    }

    OGRFeature* poFeature = new OGRFeature(poLayer->GetLayerDefn());
    if( pszId != NULL )
        poFeature->SetField("id", pszId);

    json_object* poProperties = OGRGeoJSONFindMemberByName(poObj, "properties");
    if( poProperties != NULL &&
        json_type_object == json_object_get_type(poProperties) )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poProperties, it )
        {
            const int nField = poFeature->GetFieldIndex(it.key);
            OGRGeoJSONReaderSetField(poLayer, poFeature, nField,
                                     it.key, it.val, false, 0);
        }
    }

    OGRGeometry* poGeom = NULL;
    if( strcmp(pszType, "Point") == 0 )
    {
        double dfX = 0.0;
        double dfY = 0.0;
        if( ParsePoint( poCoordinatesObj, &dfX, &dfY ) )
        {
            dfX = dfX * psParams->dfScale0 + psParams->dfTranslate0;
            dfY = dfY * psParams->dfScale1 + psParams->dfTranslate1;
            poGeom = new OGRPoint(dfX, dfY);
        }
        else
        {
            poGeom = new OGRPoint();
        }
    }
    else if( strcmp(pszType, "MultiPoint") == 0 )
    {
        OGRMultiPoint* poMP = new OGRMultiPoint();
        poGeom = poMP;
        int nTuples = json_object_array_length(poCoordinatesObj);
        for( int i = 0; i < nTuples; i++ )
        {
            json_object* poPair =
                json_object_array_get_idx(poCoordinatesObj, i);
            double dfX = 0.0;
            double dfY = 0.0;
            if( ParsePoint( poPair, &dfX, &dfY ) )
            {
                dfX = dfX * psParams->dfScale0 + psParams->dfTranslate0;
                dfY = dfY * psParams->dfScale1 + psParams->dfTranslate1;
                poMP->addGeometryDirectly(new OGRPoint(dfX, dfY));
            }
        }
    }
    else if( strcmp(pszType, "LineString") == 0 )
    {
        OGRLineString* poLS = new OGRLineString();
        poGeom = poLS;
        ParseLineString(poLS, poArcsObj, poArcsDB, psParams);
    }
    else if( strcmp(pszType, "MultiLineString") == 0 )
    {
        OGRMultiLineString* poMLS = new OGRMultiLineString();
        poGeom = poMLS;
        ParseMultiLineString(poMLS, poArcsObj, poArcsDB, psParams);
    }
    else if( strcmp(pszType, "Polygon") == 0 )
    {
        OGRPolygon* poPoly = new OGRPolygon();
        poGeom = poPoly;
        ParsePolygon(poPoly, poArcsObj, poArcsDB, psParams);
    }
    else if( strcmp(pszType, "MultiPolygon") == 0 )
    {
        OGRMultiPolygon* poMultiPoly = new OGRMultiPolygon();
        poGeom = poMultiPoly;
        ParseMultiPolygon(poMultiPoly, poArcsObj, poArcsDB, psParams);
    }

    if( poGeom != NULL )
        poFeature->SetGeometryDirectly(poGeom);
    poLayer->AddFeature(poFeature);
    delete poFeature;
}

/************************************************************************/
/*                        EstablishLayerDefn()                          */
/************************************************************************/

static void EstablishLayerDefn( OGRFeatureDefn* poDefn,
                                json_object* poObj,
                                std::set<int>& aoSetUndeterminedTypeFields )
{
    json_object* poObjProps = OGRGeoJSONFindMemberByName( poObj, "properties" );
    if( NULL != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;

        json_object_object_foreachC( poObjProps, it )
        {
            OGRGeoJSONReaderAddOrUpdateField(poDefn, it.key, it.val,
                                             false, 0, false,
                                             aoSetUndeterminedTypeFields);
        }
    }
}

/************************************************************************/
/*                        ParseObjectMain()                             */
/************************************************************************/

static bool ParseObjectMain( const char* pszId, json_object* poObj,
                             OGRGeoJSONDataSource* poDS,
                             OGRGeoJSONLayer **ppoMainLayer,
                             json_object* poArcs,
                             ScalingParams* psParams,
                             int nPassNumber,
                             std::set<int>& aoSetUndeterminedTypeFields)
{
    bool bNeedSecondPass = false;

    if( poObj != NULL && json_type_object == json_object_get_type( poObj ) )
    {
        json_object* poType = OGRGeoJSONFindMemberByName(poObj, "type");
        if( poType != NULL &&
            json_type_string == json_object_get_type( poType ) )
        {
            const char* pszType = json_object_get_string(poType);
            if( nPassNumber == 1 && strcmp(pszType, "GeometryCollection") == 0 )
            {
                json_object* poGeometries =
                    OGRGeoJSONFindMemberByName(poObj, "geometries");
                if( poGeometries != NULL &&
                    json_type_array == json_object_get_type( poGeometries ) )
                {
                    if( pszId == NULL )
                    {
                        json_object* poId =
                            OGRGeoJSONFindMemberByName(poObj, "id");
                        if( poId != NULL &&
                            (json_type_string == json_object_get_type(poId) ||
                            json_type_int == json_object_get_type(poId)) )
                        {
                            pszId = json_object_get_string(poId);
                        }
                    }

                    OGRGeoJSONLayer* poLayer = new OGRGeoJSONLayer(
                            pszId ? pszId : "TopoJSON", NULL,
                            wkbUnknown, poDS );
                    OGRFeatureDefn* poDefn = poLayer->GetLayerDefn();
                    {
                        OGRFieldDefn fldDefn( "id", OFTString );
                        poDefn->AddFieldDefn( &fldDefn );
                    }

                    const int nGeometries =
                        json_object_array_length(poGeometries);
                    // First pass to establish schema.
                    for( int i = 0; i < nGeometries; i++ )
                    {
                        json_object* poGeom =
                            json_object_array_get_idx(poGeometries, i);
                        if( poGeom != NULL &&
                            json_type_object == json_object_get_type( poGeom ) )
                        {
                            EstablishLayerDefn(poDefn, poGeom,
                                               aoSetUndeterminedTypeFields);
                        }
                    }

                    // Second pass to build objects.
                    for( int i = 0; i < nGeometries; i++ )
                    {
                        json_object* poGeom =
                            json_object_array_get_idx(poGeometries, i);
                        if( poGeom != NULL &&
                            json_type_object == json_object_get_type( poGeom ) )
                        {
                            ParseObject(NULL, poGeom, poLayer,
                                        poArcs, psParams);
                        }
                    }

                    poDS->AddLayer(poLayer);
                }
            }
            else if( strcmp(pszType, "Point") == 0 ||
                     strcmp(pszType, "MultiPoint") == 0 ||
                     strcmp(pszType, "LineString") == 0 ||
                     strcmp(pszType, "MultiLineString") == 0 ||
                     strcmp(pszType, "Polygon") == 0 ||
                     strcmp(pszType, "MultiPolygon") == 0 )
            {
                if( nPassNumber == 1 )
                {
                    if( *ppoMainLayer == NULL )
                    {
                        *ppoMainLayer = new OGRGeoJSONLayer(
                            "TopoJSON", NULL, wkbUnknown, poDS );
                        {
                            OGRFieldDefn fldDefn( "id", OFTString );
                            (*ppoMainLayer)->
                                GetLayerDefn()->AddFieldDefn( &fldDefn );
                        }
                    }
                    OGRFeatureDefn* poDefn = (*ppoMainLayer)->GetLayerDefn();
                    EstablishLayerDefn(poDefn, poObj,
                                       aoSetUndeterminedTypeFields);
                    bNeedSecondPass = true;
                }
                else
                    ParseObject(pszId, poObj, *ppoMainLayer, poArcs, psParams);
            }
        }
    }
    return bNeedSecondPass;
}

/************************************************************************/
/*                           ReadLayers()                               */
/************************************************************************/

void OGRTopoJSONReader::ReadLayers( OGRGeoJSONDataSource* poDS )
{
    if( NULL == poGJObject_ )
    {
        CPLDebug( "TopoJSON",
                  "Missing parsed TopoJSON data. Forgot to call Parse()?" );
        return;
    }

    ScalingParams sParams;
    sParams.dfScale0 = 1.0;
    sParams.dfScale1 = 1.0;
    sParams.dfTranslate0 = 0.0;
    sParams.dfTranslate1 = 0.0;
    sParams.bElementExists = false;
    json_object* poObjTransform =
        OGRGeoJSONFindMemberByName( poGJObject_, "transform" );
    if( NULL != poObjTransform &&
        json_type_object == json_object_get_type( poObjTransform ) )
    {
        json_object* poObjScale =
            OGRGeoJSONFindMemberByName( poObjTransform, "scale" );
        if( NULL != poObjScale &&
            json_type_array == json_object_get_type( poObjScale ) &&
            json_object_array_length( poObjScale ) == 2 )
        {
            json_object* poScale0 = json_object_array_get_idx(poObjScale, 0);
            json_object* poScale1 = json_object_array_get_idx(poObjScale, 1);
            if( poScale0 != NULL &&
                (json_object_get_type(poScale0) == json_type_double ||
                 json_object_get_type(poScale0) == json_type_int) &&
                poScale1 != NULL &&
                (json_object_get_type(poScale1) == json_type_double ||
                 json_object_get_type(poScale1) == json_type_int) )
            {
                sParams.dfScale0 = json_object_get_double(poScale0);
                sParams.dfScale1 = json_object_get_double(poScale1);
                sParams.bElementExists = true;
            }
        }

        json_object* poObjTranslate =
            OGRGeoJSONFindMemberByName( poObjTransform, "translate" );
        if( NULL != poObjTranslate &&
            json_type_array == json_object_get_type( poObjTranslate ) &&
            json_object_array_length( poObjTranslate ) == 2 )
        {
            json_object* poTranslate0 =
                json_object_array_get_idx(poObjTranslate, 0);
            json_object* poTranslate1 =
                json_object_array_get_idx(poObjTranslate, 1);
            if( poTranslate0 != NULL &&
                (json_object_get_type(poTranslate0) == json_type_double ||
                 json_object_get_type(poTranslate0) == json_type_int) &&
                poTranslate1 != NULL &&
                (json_object_get_type(poTranslate1) == json_type_double ||
                 json_object_get_type(poTranslate1) == json_type_int) )
            {
                sParams.dfTranslate0 = json_object_get_double(poTranslate0);
                sParams.dfTranslate1 = json_object_get_double(poTranslate1);
                sParams.bElementExists = true;
            }
        }
    }

    json_object* poArcs = OGRGeoJSONFindMemberByName( poGJObject_, "arcs" );
    if( poArcs == NULL || json_type_array != json_object_get_type( poArcs ) )
        return;

    OGRGeoJSONLayer* poMainLayer = NULL;

    json_object* poObjects =
        OGRGeoJSONFindMemberByName( poGJObject_, "objects" );
    if( poObjects == NULL )
        return;

    std::set<int> aoSetUndeterminedTypeFields;
    if( json_type_object == json_object_get_type( poObjects ) )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        bool bNeedSecondPass = false;
        json_object_object_foreachC( poObjects, it )
        {
            json_object* poObj = it.val;
            bNeedSecondPass |= ParseObjectMain( it.key, poObj, poDS,
                                                &poMainLayer, poArcs, &sParams,
                                                1, aoSetUndeterminedTypeFields);
        }
        if( bNeedSecondPass )
        {
            it.key = NULL;
            it.val = NULL;
            it.entry = NULL;
            json_object_object_foreachC( poObjects, it )
            {
                json_object* poObj = it.val;
                ParseObjectMain(it.key, poObj, poDS, &poMainLayer, poArcs,
                                &sParams, 2, aoSetUndeterminedTypeFields);
            }
        }
    }
    else if( json_type_array == json_object_get_type( poObjects ) )
    {
        const int nObjects = json_object_array_length(poObjects);
        bool bNeedSecondPass = false;
        for( int i = 0; i < nObjects; i++ )
        {
            json_object* poObj = json_object_array_get_idx(poObjects, i);
            bNeedSecondPass |= ParseObjectMain(NULL, poObj, poDS, &poMainLayer,
                                               poArcs, &sParams, 1,
                                               aoSetUndeterminedTypeFields);
        }
        if( bNeedSecondPass )
        {
            for( int i = 0; i < nObjects; i++ )
            {
                json_object* poObj = json_object_array_get_idx(poObjects, i);
                ParseObjectMain(NULL, poObj, poDS, &poMainLayer, poArcs,
                                &sParams, 2, aoSetUndeterminedTypeFields);
            }
        }
    }

    if( poMainLayer != NULL )
        poDS->AddLayer(poMainLayer);
}
