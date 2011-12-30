/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRESRIJSONReader class (OGR ESRIJSON Driver)
 *           to read ESRI Feature Service REST data
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
 * Copyright (c) 2007, Mateusz Loskot
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
#include <jsonc/json.h> // JSON-C
#include <jsonc/json_object_private.h> // json_object_iter, complete type required
#include <ogr_api.h>

/************************************************************************/
/*                          OGRESRIJSONReader()                         */
/************************************************************************/

OGRESRIJSONReader::OGRESRIJSONReader()
    : poGJObject_( NULL ), poLayer_( NULL )
{
    // Take a deep breath and get to work.
}

/************************************************************************/
/*                         ~OGRESRIJSONReader()                         */
/************************************************************************/

OGRESRIJSONReader::~OGRESRIJSONReader()
{
    if( NULL != poGJObject_ )
    {
        json_object_put(poGJObject_);
    }

    poGJObject_ = NULL;
    poLayer_ = NULL;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

OGRErr OGRESRIJSONReader::Parse( const char* pszText )
{
    if( NULL != pszText )
    {
        json_tokener* jstok = NULL;
        json_object* jsobj = NULL;

        jstok = json_tokener_new();
        jsobj = json_tokener_parse_ex(jstok, pszText, -1);
        if( jstok->err != json_tokener_success)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "ESRIJSON parsing error: %s (at offset %d)",
            	      json_tokener_errors[jstok->err], jstok->char_offset);
            
            json_tokener_free(jstok);
            return OGRERR_CORRUPT_DATA;
        }
        json_tokener_free(jstok);

        /* JSON tree is shared for while lifetime of the reader object
         * and will be released in the destructor.
         */
        poGJObject_ = jsobj;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReadLayer()                                */
/************************************************************************/

OGRGeoJSONLayer* OGRESRIJSONReader::ReadLayer( const char* pszName,
                                                OGRGeoJSONDataSource* poDS )
{
    CPLAssert( NULL == poLayer_ );

    if( NULL == poGJObject_ )
    {
        CPLDebug( "ESRIJSON",
                  "Missing parset ESRIJSON data. Forgot to call Parse()?" );
        return NULL;
    }
        
    poLayer_ = new OGRGeoJSONLayer( pszName, NULL,
                                    OGRESRIJSONGetGeometryType(poGJObject_),
                                    poDS );

    if( !GenerateLayerDefn() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Layer schema generation failed." );

        delete poLayer_;
        return NULL;
    }

    OGRGeoJSONLayer* poThisLayer = NULL;
    poThisLayer = ReadFeatureCollection( poGJObject_ );
    if (poThisLayer == NULL)
    {
        delete poLayer_;
        return NULL;
    }

    OGRSpatialReference* poSRS = NULL;
    poSRS = OGRESRIJSONReadSpatialReference( poGJObject_ );
    if (poSRS != NULL )
    {
        poLayer_->SetSpatialRef( poSRS );
        delete poSRS;
    }

    return poLayer_;
}

/************************************************************************/
/*                        GenerateFeatureDefn()                         */
/************************************************************************/

bool OGRESRIJSONReader::GenerateLayerDefn()
{
    CPLAssert( NULL != poGJObject_ );
    CPLAssert( NULL != poLayer_->GetLayerDefn() );
    CPLAssert( 0 == poLayer_->GetLayerDefn()->GetFieldCount() );

    bool bSuccess = true;

/* -------------------------------------------------------------------- */
/*      Scan all features and generate layer definition.				*/
/* -------------------------------------------------------------------- */
    json_object* poObjFeatures = NULL;

    poObjFeatures = OGRGeoJSONFindMemberByName( poGJObject_, "fields" );
    if( NULL != poObjFeatures && json_type_array == json_object_get_type( poObjFeatures ) )
    {
        json_object* poObjFeature = NULL;
        const int nFeatures = json_object_array_length( poObjFeatures );
        for( int i = 0; i < nFeatures; ++i )
        {
            poObjFeature = json_object_array_get_idx( poObjFeatures, i );
            if( !GenerateFeatureDefn( poObjFeature ) )
            {
                CPLDebug( "GeoJSON", "Create feature schema failure." );
                bSuccess = false;
            }
        }
    }
    else
    {
        poObjFeatures = OGRGeoJSONFindMemberByName( poGJObject_, "fieldAliases" );
        if( NULL != poObjFeatures )
        {
            OGRFeatureDefn* poDefn = poLayer_->GetLayerDefn();
            json_object_iter it;
            it.key = NULL;
            it.val = NULL;
            it.entry = NULL;
            json_object_object_foreachC( poObjFeatures, it )
            {
                OGRFieldDefn fldDefn( it.key, OFTString );
                poDefn->AddFieldDefn( &fldDefn );
            }
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid FeatureCollection object. "
                        "Missing \'fields\' member." );
            bSuccess = false;
        }
    }

    return bSuccess;
}

/************************************************************************/
/*                        GenerateFeatureDefn()                         */
/************************************************************************/

bool OGRESRIJSONReader::GenerateFeatureDefn( json_object* poObj )
{
    OGRFeatureDefn* poDefn = poLayer_->GetLayerDefn();
    CPLAssert( NULL != poDefn );

    bool bSuccess = false;

/* -------------------------------------------------------------------- */
/*      Read collection of properties.									*/
/* -------------------------------------------------------------------- */
    json_object* poObjName = OGRGeoJSONFindMemberByName( poObj, "name" );
    json_object* poObjType = OGRGeoJSONFindMemberByName( poObj, "type" );
    if( NULL != poObjName && NULL != poObjType )
    {
        OGRFieldType eFieldType = OFTString;
        if (EQUAL(json_object_get_string(poObjType), "esriFieldTypeOID"))
        {
            eFieldType = OFTInteger;
            poLayer_->SetFIDColumn(json_object_get_string(poObjName));
        }
        else if (EQUAL(json_object_get_string(poObjType), "esriFieldTypeDouble"))
        {
            eFieldType = OFTReal;
        }
        else if (EQUAL(json_object_get_string(poObjType), "esriFieldTypeSmallInteger") ||
                 EQUAL(json_object_get_string(poObjType), "esriFieldTypeInteger") )
        {
            eFieldType = OFTInteger;
        }
        OGRFieldDefn fldDefn( json_object_get_string(poObjName),
                              eFieldType);

        json_object* poObjLength = OGRGeoJSONFindMemberByName( poObj, "length" );
        if (poObjLength != NULL && json_object_get_type(poObjLength) == json_type_int )
        {
            fldDefn.SetWidth(json_object_get_int(poObjLength));
        }

        poDefn->AddFieldDefn( &fldDefn );

        bSuccess = true; // SUCCESS
    }
    return bSuccess;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRESRIJSONReader::AddFeature( OGRFeature* poFeature )
{
    bool bAdded = false;
  
    if( NULL != poFeature )
    {
        poLayer_->AddFeature( poFeature );
        bAdded = true;
        delete poFeature;
    }

    return bAdded;
}

/************************************************************************/
/*                           ReadGeometry()                             */
/************************************************************************/

OGRGeometry* OGRESRIJSONReader::ReadGeometry( json_object* poObj )
{
    OGRGeometry* poGeometry = NULL;

    OGRwkbGeometryType eType = poLayer_->GetGeomType();
    if (eType == wkbPoint)
        poGeometry = OGRESRIJSONReadPoint( poObj );
    else if (eType == wkbLineString)
        poGeometry = OGRESRIJSONReadLineString( poObj );
    else if (eType == wkbPolygon)
        poGeometry = OGRESRIJSONReadPolygon( poObj );
    else if (eType == wkbMultiPoint)
        poGeometry = OGRESRIJSONReadMultiPoint( poObj );

    return poGeometry;
}

/************************************************************************/
/*                           ReadFeature()                              */
/************************************************************************/

OGRFeature* OGRESRIJSONReader::ReadFeature( json_object* poObj )
{
    CPLAssert( NULL != poObj );
    CPLAssert( NULL != poLayer_ );

    OGRFeature* poFeature = NULL;
    poFeature = new OGRFeature( poLayer_->GetLayerDefn() );

/* -------------------------------------------------------------------- */
/*      Translate ESRIJSON "attributes" object to feature attributes.   */
/* -------------------------------------------------------------------- */
    CPLAssert( NULL != poFeature );

    json_object* poObjProps = NULL;
    poObjProps = OGRGeoJSONFindMemberByName( poObj, "attributes" );
    if( NULL != poObjProps )
    {
        int nField = -1;
        OGRFieldDefn* poFieldDefn = NULL;
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poObjProps, it )
        {
            nField = poFeature->GetFieldIndex(it.key);
            poFieldDefn = poFeature->GetFieldDefnRef(nField);
            if (poFieldDefn && it.val != NULL )
            {
                if ( EQUAL( it.key,  poLayer_->GetFIDColumn() ) )
                    poFeature->SetFID( json_object_get_int( it.val ) );
                poFeature->SetField( nField, json_object_get_string(it.val) );
            }
        }
    }

    OGRwkbGeometryType eType = poLayer_->GetGeomType();
    if (eType == wkbNone)
        return poFeature;

/* -------------------------------------------------------------------- */
/*      Translate geometry sub-object of ESRIJSON Feature.               */
/* -------------------------------------------------------------------- */
    json_object* poObjGeom = NULL;

    json_object* poTmp = poObj;

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC(poTmp, it)
    {
        if( EQUAL( it.key, "geometry" ) ) {
            if (it.val != NULL)
                poObjGeom = it.val;
            // we're done.  They had 'geometry':null
            else
                return poFeature;
        }
    }
    
    if( NULL != poObjGeom )
    {
        OGRGeometry* poGeometry = ReadGeometry( poObjGeom );
        if( NULL != poGeometry )
        {
            poFeature->SetGeometryDirectly( poGeometry );
        }
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Feature object. "
                  "Missing \'geometry\' member." );
        delete poFeature;
        return NULL;
    }

    return poFeature;
}

/************************************************************************/
/*                           ReadFeatureCollection()                    */
/************************************************************************/

OGRGeoJSONLayer*
OGRESRIJSONReader::ReadFeatureCollection( json_object* poObj )
{
    CPLAssert( NULL != poLayer_ );

    json_object* poObjFeatures = NULL;
    poObjFeatures = OGRGeoJSONFindMemberByName( poObj, "features" );
    if( NULL == poObjFeatures )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid FeatureCollection object. "
                  "Missing \'features\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjFeatures ) )
    {
        bool bAdded = false;
        OGRFeature* poFeature = NULL;
        json_object* poObjFeature = NULL;

        const int nFeatures = json_object_array_length( poObjFeatures );
        for( int i = 0; i < nFeatures; ++i )
        {
            poObjFeature = json_object_array_get_idx( poObjFeatures, i );
            poFeature = OGRESRIJSONReader::ReadFeature( poObjFeature );
            bAdded = AddFeature( poFeature );
            //CPLAssert( bAdded );
        }
        //CPLAssert( nFeatures == poLayer_->GetFeatureCount() );
    }

    // We're returning class member to follow the same pattern of
    // Read* functions call convention.
    CPLAssert( NULL != poLayer_ );
    return poLayer_;
}

/************************************************************************/
/*                        OGRESRIJSONGetType()                          */
/************************************************************************/

OGRwkbGeometryType OGRESRIJSONGetGeometryType( json_object* poObj )
{
    if( NULL == poObj )
        return wkbUnknown;

    json_object* poObjType = NULL;
    poObjType = OGRGeoJSONFindMemberByName( poObj, "geometryType" );
    if( NULL == poObjType )
    {
        return wkbNone;
    }

    const char* name = json_object_get_string( poObjType );
    if( EQUAL( name, "esriGeometryPoint" ) )
        return wkbPoint;
    else if( EQUAL( name, "esriGeometryPolyline" ) )
        return wkbLineString;
    else if( EQUAL( name, "esriGeometryPolygon" ) )
        return wkbPolygon;
    else if( EQUAL( name, "esriGeometryMultiPoint" ) )
        return wkbMultiPoint;
    else
        return wkbUnknown;
}

/************************************************************************/
/*                          OGRESRIJSONReadPoint()                      */
/************************************************************************/

OGRPoint* OGRESRIJSONReadPoint( json_object* poObj)
{
    CPLAssert( NULL != poObj );

    json_object* poObjX = OGRGeoJSONFindMemberByName( poObj, "x" );
    if( NULL == poObjX )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid Point object. "
            "Missing \'x\' member." );
        return NULL;
    }

    int iTypeX = json_object_get_type(poObjX);
    if ( (json_type_double != iTypeX) && (json_type_int != iTypeX) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Invalid X coordinate. Type is not double or integer for \'%s\'.",
                json_object_to_json_string(poObjX) );
        return NULL;
    }

    json_object* poObjY = OGRGeoJSONFindMemberByName( poObj, "y" );
    if( NULL == poObjY )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid Point object. "
            "Missing \'y\' member." );
        return NULL;
    }

    int iTypeY = json_object_get_type(poObjY);
    if ( (json_type_double != iTypeY) && (json_type_int != iTypeY) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Invalid Y coordinate. Type is not double or integer for \'%s\'.",
                json_object_to_json_string(poObjY) );
        return NULL;
    }

    double dfX, dfY;
    if (iTypeX == json_type_double)
        dfX = json_object_get_double( poObjX );
    else
        dfX = json_object_get_int( poObjX );
    if (iTypeY == json_type_double)
        dfY = json_object_get_double( poObjY );
    else
        dfY = json_object_get_int( poObjY );

    return new OGRPoint(dfX, dfY);
}

/************************************************************************/
/*                        OGRESRIJSONReadLineString()                   */
/************************************************************************/

OGRLineString* OGRESRIJSONReadLineString( json_object* poObj)
{
    CPLAssert( NULL != poObj );

    OGRLineString* poLine = NULL;
    
    json_object* poObjPaths = OGRGeoJSONFindMemberByName( poObj, "paths" );
    if( NULL == poObjPaths )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid LineString object. "
            "Missing \'paths\' member." );
        return NULL;
    }

    if( json_type_array != json_object_get_type( poObjPaths ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid LineString object. "
            "Invalid \'paths\' member." );
        return NULL;
    }
    
    poLine = new OGRLineString();
    const int nPaths = json_object_array_length( poObjPaths );
    for(int iPath = 0; iPath < nPaths; iPath ++)
    {
        json_object* poObjPath = json_object_array_get_idx( poObjPaths, iPath );
        if ( poObjPath == NULL ||
                json_type_array != json_object_get_type( poObjPath ) )
        {
            delete poLine;
            CPLDebug( "ESRIJSON",
                    "LineString: got non-array object." );
            return NULL;
        }

        const int nPoints = json_object_array_length( poObjPath );
        for(int i = 0; i < nPoints; i++)
        {
            json_object* poObjCoords = NULL;

            poObjCoords = json_object_array_get_idx( poObjPath, i );
            if (poObjCoords == NULL)
            {
                delete poLine;
                CPLDebug( "ESRIJSON",
                        "LineString: got null object." );
                return NULL;
            }
            if( json_type_array != json_object_get_type( poObjCoords ) ||
                json_object_array_length( poObjCoords ) != 2 )
            {
                delete poLine;
                CPLDebug( "ESRIJSON",
                        "LineString: got non-array object." );
                return NULL;
            }

            json_object* poObjCoord;
            int iType;
            double dfX, dfY;

            // Read X coordinate
            poObjCoord = json_object_array_get_idx( poObjCoords, 0 );
            if (poObjCoord == NULL)
            {
                CPLDebug( "ESRIJSON", "LineString: got null object." );
                delete poLine;
                return NULL;
            }

            iType = json_object_get_type(poObjCoord);
            if ( (json_type_double != iType) && (json_type_int != iType) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid X coordinate. Type is not double or integer for \'%s\'.",
                        json_object_to_json_string(poObjCoord) );
                delete poLine;
                return NULL;
            }

            if (iType == json_type_double)
                dfX = json_object_get_double( poObjCoord );
            else
                dfX = json_object_get_int( poObjCoord );

            // Read Y coordinate
            poObjCoord = json_object_array_get_idx( poObjCoords, 1 );
            if (poObjCoord == NULL)
            {
                CPLDebug( "ESRIJSON", "LineString: got null object." );
                delete poLine;
                return NULL;
            }

            iType = json_object_get_type(poObjCoord);
            if ( (json_type_double != iType) && (json_type_int != iType) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid Y coordinate. Type is not double or integer for \'%s\'.",
                        json_object_to_json_string(poObjCoord) );
                delete poLine;
                return NULL;
            }

            if (iType == json_type_double)
                dfY = json_object_get_double( poObjCoord );
            else
                dfY = json_object_get_int( poObjCoord );

            poLine->addPoint( dfX, dfY );
        }
    }

    return poLine;
}

/************************************************************************/
/*                          OGRESRIJSONReadPolygon()                    */
/************************************************************************/

OGRPolygon* OGRESRIJSONReadPolygon( json_object* poObj)
{
    CPLAssert( NULL != poObj );

    OGRPolygon* poPoly = NULL;

    json_object* poObjRings = OGRGeoJSONFindMemberByName( poObj, "rings" );
    if( NULL == poObjRings )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid Polygon object. "
            "Missing \'rings\' member." );
        return NULL;
    }

    if( json_type_array != json_object_get_type( poObjRings ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid Polygon object. "
            "Invalid \'rings\' member." );
        return NULL;
    }

    poPoly = new OGRPolygon();

    const int nRings = json_object_array_length( poObjRings );
    for(int iRing = 0; iRing < nRings; iRing ++)
    {
        json_object* poObjRing = json_object_array_get_idx( poObjRings, iRing );
        if ( poObjRing == NULL ||
                json_type_array != json_object_get_type( poObjRing ) )
        {
            delete poPoly;
            CPLDebug( "ESRIJSON",
                    "Polygon: got non-array object." );
            return NULL;
        }

        OGRLinearRing* poLine = new OGRLinearRing();
        poPoly->addRingDirectly(poLine);

        const int nPoints = json_object_array_length( poObjRing );
        for(int i = 0; i < nPoints; i++)
        {
            json_object* poObjCoords = NULL;

            poObjCoords = json_object_array_get_idx( poObjRing, i );
            if (poObjCoords == NULL)
            {
                delete poPoly;
                CPLDebug( "ESRIJSON",
                        "Polygon: got null object." );
                return NULL;
            }
            if( json_type_array != json_object_get_type( poObjCoords ) ||
                json_object_array_length( poObjCoords ) != 2 )
            {
                delete poPoly;
                CPLDebug( "ESRIJSON",
                        "Polygon: got non-array object." );
                return NULL;
            }

            json_object* poObjCoord;
            int iType;
            double dfX, dfY;

            // Read X coordinate
            poObjCoord = json_object_array_get_idx( poObjCoords, 0 );
            if (poObjCoord == NULL)
            {
                CPLDebug( "ESRIJSON", "Polygon: got null object." );
                delete poPoly;
                return NULL;
            }

            iType = json_object_get_type(poObjCoord);
            if ( (json_type_double != iType) && (json_type_int != iType) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid X coordinate. Type is not double or integer for \'%s\'.",
                        json_object_to_json_string(poObjCoord) );
                delete poPoly;
                return NULL;
            }

            if (iType == json_type_double)
                dfX = json_object_get_double( poObjCoord );
            else
                dfX = json_object_get_int( poObjCoord );

            // Read Y coordinate
            poObjCoord = json_object_array_get_idx( poObjCoords, 1 );
            if (poObjCoord == NULL)
            {
                CPLDebug( "ESRIJSON", "Polygon: got null object." );
                delete poPoly;
                return NULL;
            }

            iType = json_object_get_type(poObjCoord);
            if ( (json_type_double != iType) && (json_type_int != iType) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Invalid Y coordinate. Type is not double or integer for \'%s\'.",
                        json_object_to_json_string(poObjCoord) );
                delete poPoly;
                return NULL;
            }

            if (iType == json_type_double)
                dfY = json_object_get_double( poObjCoord );
            else
                dfY = json_object_get_int( poObjCoord );

            poLine->addPoint( dfX, dfY );
        }
    }

    return poPoly;
}

/************************************************************************/
/*                        OGRESRIJSONReadMultiPoint()                   */
/************************************************************************/

OGRMultiPoint* OGRESRIJSONReadMultiPoint( json_object* poObj)
{
    CPLAssert( NULL != poObj );

    OGRMultiPoint* poMulti = NULL;

    json_object* poObjPoints = OGRGeoJSONFindMemberByName( poObj, "points" );
    if( NULL == poObjPoints )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid LineString object. "
            "Missing \'points\' member." );
        return NULL;
    }

    if( json_type_array != json_object_get_type( poObjPoints ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid LineString object. "
            "Invalid \'points\' member." );
        return NULL;
    }

    poMulti = new OGRMultiPoint();

    const int nPoints = json_object_array_length( poObjPoints );
    for(int i = 0; i < nPoints; i++)
    {
        json_object* poObjCoords = NULL;

        poObjCoords = json_object_array_get_idx( poObjPoints, i );
        if (poObjCoords == NULL)
        {
            delete poMulti;
            CPLDebug( "ESRIJSON",
                    "MultiPoint: got null object." );
            return NULL;
        }
        if( json_type_array != json_object_get_type( poObjCoords ) ||
            json_object_array_length( poObjCoords ) != 2 )
        {
            delete poMulti;
            CPLDebug( "ESRIJSON",
                    "MultiPoint: got non-array object." );
            return NULL;
        }

        json_object* poObjCoord;
        int iType;
        double dfX, dfY;

        // Read X coordinate
        poObjCoord = json_object_array_get_idx( poObjCoords, 0 );
        if (poObjCoord == NULL)
        {
            CPLDebug( "ESRIJSON", "MultiPoint: got null object." );
            delete poMulti;
            return NULL;
        }

        iType = json_object_get_type(poObjCoord);
        if ( (json_type_double != iType) && (json_type_int != iType) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid X coordinate. Type is not double or integer for \'%s\'.",
                    json_object_to_json_string(poObjCoord) );
            delete poMulti;
            return NULL;
        }

        if (iType == json_type_double)
            dfX = json_object_get_double( poObjCoord );
        else
            dfX = json_object_get_int( poObjCoord );

        // Read Y coordinate
        poObjCoord = json_object_array_get_idx( poObjCoords, 1 );
        if (poObjCoord == NULL)
        {
            CPLDebug( "ESRIJSON", "MultiPoint: got null object." );
            delete poMulti;
            return NULL;
        }

        iType = json_object_get_type(poObjCoord);
        if ( (json_type_double != iType) && (json_type_int != iType) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Y coordinate. Type is not double or integer for \'%s\'.",
                    json_object_to_json_string(poObjCoord) );
            delete poMulti;
            return NULL;
        }

        if (iType == json_type_double)
            dfY = json_object_get_double( poObjCoord );
        else
            dfY = json_object_get_int( poObjCoord );

        poMulti->addGeometryDirectly( new OGRPoint(dfX, dfY) );
    }

    return poMulti;
}

/************************************************************************/
/*                    OGRESRIJSONReadSpatialReference()                 */
/************************************************************************/

OGRSpatialReference* OGRESRIJSONReadSpatialReference( json_object* poObj )
{
/* -------------------------------------------------------------------- */
/*      Read spatial reference definition.                              */
/* -------------------------------------------------------------------- */
    OGRSpatialReference* poSRS = NULL;

    json_object* poObjSrs = OGRGeoJSONFindMemberByName( poObj, "spatialReference" );
    if( NULL != poObjSrs )
    {
        json_object* poObjWkid = OGRGeoJSONFindMemberByName( poObjSrs, "wkid" );
        if (poObjWkid == NULL)
        {
            json_object* poObjWkt = OGRGeoJSONFindMemberByName( poObjSrs, "wkt" );
            if (poObjWkt == NULL)
                return NULL;

            char* pszWKT = (char*) json_object_get_string( poObjWkt );
            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE != poSRS->importFromWkt( &pszWKT ) ||
                poSRS->morphFromESRI() != OGRERR_NONE )
            {
                delete poSRS;
                poSRS = NULL;
            }

            return poSRS;
        }

        int nEPSG = json_object_get_int( poObjWkid );

        poSRS = new OGRSpatialReference();
        if( OGRERR_NONE != poSRS->importFromEPSG( nEPSG ) )
        {
            delete poSRS;
            poSRS = NULL;
        }
    }

    return poSRS;
}
