/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRESRIJSONReader class (OGR ESRIJSON Driver)
 *           to read ESRI Feature Service REST data
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2013, Kyle Shannon <kyle at pobox dot com>
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
#include <json.h> // JSON-C
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
            	      json_tokener_error_desc(jstok->err), jstok->char_offset);
            
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
/*                           ReadLayers()                               */
/************************************************************************/

void OGRESRIJSONReader::ReadLayers( OGRGeoJSONDataSource* poDS )
{
    CPLAssert( NULL == poLayer_ );

    if( NULL == poGJObject_ )
    {
        CPLDebug( "ESRIJSON",
                  "Missing parset ESRIJSON data. Forgot to call Parse()?" );
        return;
    }

    OGRSpatialReference* poSRS = NULL;
    poSRS = OGRESRIJSONReadSpatialReference( poGJObject_ );

    poLayer_ = new OGRGeoJSONLayer( OGRGeoJSONLayer::DefaultName, poSRS,
                                    OGRESRIJSONGetGeometryType(poGJObject_),
                                    poDS );
    if( poSRS != NULL )
        poSRS->Release();

    if( !GenerateLayerDefn() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Layer schema generation failed." );

        delete poLayer_;
        return;
    }

    OGRGeoJSONLayer* poThisLayer = NULL;
    poThisLayer = ReadFeatureCollection( poGJObject_ );
    if (poThisLayer == NULL)
    {
        delete poLayer_;
        return;
    }

    CPLErrorReset();

    poDS->AddLayer(poLayer_);
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
        if( NULL != poObjFeatures &&
            json_object_get_type(poObjFeatures) == json_type_object )
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
    if( NULL != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
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
                if ( poLayer_->GetLayerDefn()->GetFieldDefn(nField)->GetType() == OFTReal )
                    poFeature->SetField( nField, CPLAtofM(json_object_get_string(it.val)) );
                else
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
        /* bool bAdded = false; */
        OGRFeature* poFeature = NULL;
        json_object* poObjFeature = NULL;

        const int nFeatures = json_object_array_length( poObjFeatures );
        for( int i = 0; i < nFeatures; ++i )
        {
            poObjFeature = json_object_array_get_idx( poObjFeatures, i );
            if (poObjFeature != NULL &&
                json_object_get_type(poObjFeature) == json_type_object)
            {
                poFeature = OGRESRIJSONReader::ReadFeature( poObjFeature );
                /* bAdded = */ AddFeature( poFeature );
            }
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
    dfX = json_object_get_double( poObjX );
    dfY = json_object_get_double( poObjY );

    bool is3d = false;
    double dfZ = 0.0;

    json_object* poObjZ = OGRGeoJSONFindMemberByName( poObj, "z" );
    if( NULL != poObjZ )
    {
        int iTypeZ = json_object_get_type(poObjZ);
        if ( (json_type_double != iTypeZ) && (json_type_int != iTypeZ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Z coordinate. Type is not double or integer for \'%s\'.",
                    json_object_to_json_string(poObjZ) );
            return NULL;
        }
        is3d = true;
        dfZ = json_object_get_double( poObjZ );
    }

    if(is3d)
    {
        return new OGRPoint(dfX, dfY, dfZ);
    }
    else
    {
       return new OGRPoint(dfX, dfY);
    }
}

/************************************************************************/
/*                     OGRESRIJSONReaderParseZM()                  */
/************************************************************************/

static int OGRESRIJSONReaderParseZM( json_object* poObj, int *bHasZ,
                                     int *bHasM )
{
    CPLAssert( NULL != poObj );
    /*
    ** The esri geojson spec states that geometries other than point can
    ** have the attributes hasZ and hasM.  A geometry that has a z value
    ** implies the 3rd number in the tuple is z.  if hasM is true, but hasZ
    ** is not, it is the M value, and is not supported in OGR.
    */
    int bZ, bM;
    json_object* poObjHasZ = OGRGeoJSONFindMemberByName( poObj, "hasZ" );
    if( poObjHasZ == NULL )
    {
        bZ = FALSE;
    }
    else
    {
        if( json_object_get_type( poObjHasZ ) != json_type_boolean )
        {
            bZ = FALSE;
        }
        else
        {
            bZ = json_object_get_boolean( poObjHasZ );
        }
    }

    json_object* poObjHasM = OGRGeoJSONFindMemberByName( poObj, "hasM" );
    if( poObjHasM == NULL )
    {
        bM = FALSE;
    }
    else
    {
        if( json_object_get_type( poObjHasM ) != json_type_boolean )
        {
            bM = FALSE;
        }
        else
        {
            bM = json_object_get_boolean( poObjHasM );
        }
    }
    if( bHasZ != NULL )
        *bHasZ = bZ;
    if( bHasM != NULL )
        *bHasM = bM;
    return TRUE;
}

/************************************************************************/
/*                     OGRESRIJSONReaderParseXYZMArray()                  */
/************************************************************************/

static int OGRESRIJSONReaderParseXYZMArray (json_object* poObjCoords,
                                          double* pdfX, double* pdfY,
                                          double* pdfZ, int* pnNumCoords)
{
    if (poObjCoords == NULL)
    {
        CPLDebug( "ESRIJSON",
                "OGRESRIJSONReaderParseXYZMArray: got null object." );
        return FALSE;
    }

    if( json_type_array != json_object_get_type( poObjCoords ))
    {
        CPLDebug( "ESRIJSON",
                "OGRESRIJSONReaderParseXYZMArray: got non-array object." );
        return FALSE;
    }

    int coordDimension = json_object_array_length( poObjCoords );
    /*
    ** We allow 4 coordinates if M is present, but it is eventually ignored.
    */
    if(coordDimension < 2 || coordDimension > 4)
    {
        CPLDebug( "ESRIJSON",
                "OGRESRIJSONReaderParseXYZMArray: got an unexpected array object." );
        return FALSE;
    }

    json_object* poObjCoord;
    int iType;
    double dfX, dfY, dfZ = 0.0;

    // Read X coordinate
    poObjCoord = json_object_array_get_idx( poObjCoords, 0 );
    if (poObjCoord == NULL)
    {
        CPLDebug( "ESRIJSON", "OGRESRIJSONReaderParseXYZMArray: got null object." );
        return FALSE;
    }

    iType = json_object_get_type(poObjCoord);
    if ( (json_type_double != iType) && (json_type_int != iType) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Invalid X coordinate. Type is not double or integer for \'%s\'.",
                json_object_to_json_string(poObjCoord) );
        return FALSE;
    }

    dfX = json_object_get_double( poObjCoord );

    // Read Y coordinate
    poObjCoord = json_object_array_get_idx( poObjCoords, 1 );
    if (poObjCoord == NULL)
    {
        CPLDebug( "ESRIJSON", "OGRESRIJSONReaderParseXYZMArray: got null object." );
        return FALSE;
    }

    iType = json_object_get_type(poObjCoord);
    if ( (json_type_double != iType) && (json_type_int != iType) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Invalid Y coordinate. Type is not double or integer for \'%s\'.",
                json_object_to_json_string(poObjCoord) );
        return FALSE;
    }

    dfY = json_object_get_double( poObjCoord );

    // Read Z coordinate
    if(coordDimension > 2)
    {
        poObjCoord = json_object_array_get_idx( poObjCoords, 2 );
        if (poObjCoord == NULL)
        {
            CPLDebug( "ESRIJSON", "OGRESRIJSONReaderParseXYZMArray: got null object." );
            return FALSE;
        }

        iType = json_object_get_type(poObjCoord);
        if ( (json_type_double != iType) && (json_type_int != iType) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid Z coordinate. Type is not double or integer for \'%s\'.",
                    json_object_to_json_string(poObjCoord) );
            return FALSE;
        }
        dfZ = json_object_get_double( poObjCoord );
    }

    if( pnNumCoords != NULL )
        *pnNumCoords = coordDimension;
    if( pdfX != NULL )
        *pdfX = dfX;
    if( pdfY != NULL )
        *pdfY = dfY;
    if( pdfZ != NULL )
        *pdfZ = dfZ;

    return TRUE;
}

/************************************************************************/
/*                        OGRESRIJSONReadLineString()                   */
/************************************************************************/

OGRLineString* OGRESRIJSONReadLineString( json_object* poObj)
{
    CPLAssert( NULL != poObj );

    int bHasZ = FALSE;
    int bHasM = FALSE;

    if( !OGRESRIJSONReaderParseZM( poObj, &bHasZ, &bHasM ) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Failed to parse hasZ and/or hasM from geometry" );
    }

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
            double dfX, dfY, dfZ;
            int nNumCoords = 2;
            json_object* poObjCoords = NULL;

            poObjCoords = json_object_array_get_idx( poObjPath, i );
            if( !OGRESRIJSONReaderParseXYZMArray (poObjCoords, &dfX, &dfY, &dfZ, &nNumCoords) )
            {
                delete poLine;
                return NULL;
            }

            if(nNumCoords > 2 && (TRUE == bHasZ || FALSE == bHasM))
            {
                poLine->addPoint( dfX, dfY, dfZ);
            }
            else
            {
                poLine->addPoint( dfX, dfY );
            }
        }
    }

    return poLine;
}

/************************************************************************/
/*                          OGRESRIJSONReadPolygon()                    */
/************************************************************************/

OGRGeometry* OGRESRIJSONReadPolygon( json_object* poObj)
{
    CPLAssert( NULL != poObj );


    int bHasZ = FALSE;
    int bHasM = FALSE;

    if( !OGRESRIJSONReaderParseZM( poObj, &bHasZ, &bHasM ) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Failed to parse hasZ and/or hasM from geometry" );
    }

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

    const int nRings = json_object_array_length( poObjRings );
    OGRGeometry** papoGeoms = new OGRGeometry*[nRings];
    for(int iRing = 0; iRing < nRings; iRing ++)
    {
        json_object* poObjRing = json_object_array_get_idx( poObjRings, iRing );
        if ( poObjRing == NULL ||
                json_type_array != json_object_get_type( poObjRing ) )
        {
            for(int j=0;j<iRing;j++)
                delete papoGeoms[j];
            delete[] papoGeoms;
            CPLDebug( "ESRIJSON",
                    "Polygon: got non-array object." );
            return NULL;
        }

        OGRPolygon* poPoly = new OGRPolygon();
        OGRLinearRing* poLine = new OGRLinearRing();
        poPoly->addRingDirectly(poLine);
        papoGeoms[iRing] = poPoly;

        const int nPoints = json_object_array_length( poObjRing );
        for(int i = 0; i < nPoints; i++)
        {
            int nNumCoords = 2;
            double dfX, dfY, dfZ;
            json_object* poObjCoords = NULL;

            poObjCoords = json_object_array_get_idx( poObjRing, i );
            if( !OGRESRIJSONReaderParseXYZMArray (poObjCoords, &dfX, &dfY, &dfZ, &nNumCoords) )
            {
                for(int j=0;j<=iRing;j++)
                    delete papoGeoms[j];
                delete[] papoGeoms;
                return NULL;
            }

            if(nNumCoords > 2 && (TRUE == bHasZ || FALSE == bHasM))
            {
                poLine->addPoint( dfX, dfY, dfZ);
            }
            else
            {
                poLine->addPoint( dfX, dfY );
            }
        }
    }
    
    OGRGeometry* poRet = OGRGeometryFactory::organizePolygons( papoGeoms,
                                                               nRings,
                                                               NULL,
                                                               NULL);
    delete[] papoGeoms;

    return poRet;
}

/************************************************************************/
/*                        OGRESRIJSONReadMultiPoint()                   */
/************************************************************************/

OGRMultiPoint* OGRESRIJSONReadMultiPoint( json_object* poObj)
{
    CPLAssert( NULL != poObj );

    int bHasZ = FALSE;
    int bHasM = FALSE;

    if( !OGRESRIJSONReaderParseZM( poObj, &bHasZ, &bHasM ) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Failed to parse hasZ and/or hasM from geometry" );
    }

    OGRMultiPoint* poMulti = NULL;

    json_object* poObjPoints = OGRGeoJSONFindMemberByName( poObj, "points" );
    if( NULL == poObjPoints )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid MultiPoint object. "
            "Missing \'points\' member." );
        return NULL;
    }

    if( json_type_array != json_object_get_type( poObjPoints ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid MultiPoint object. "
            "Invalid \'points\' member." );
        return NULL;
    }

    poMulti = new OGRMultiPoint();

    const int nPoints = json_object_array_length( poObjPoints );
    for(int i = 0; i < nPoints; i++)
    {
        double dfX, dfY, dfZ;
        int nNumCoords = 2;
        json_object* poObjCoords = NULL;

        poObjCoords = json_object_array_get_idx( poObjPoints, i );
        if( !OGRESRIJSONReaderParseXYZMArray (poObjCoords, &dfX, &dfY, &dfZ, &nNumCoords) )
        {
            delete poMulti;
            return NULL;
        }

        if(nNumCoords > 2 && (TRUE == bHasZ || FALSE == bHasM))
        {
            poMulti->addGeometryDirectly( new OGRPoint(dfX, dfY, dfZ) );
        }
        else
        {
            poMulti->addGeometryDirectly( new OGRPoint(dfX, dfY) );
        }
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
