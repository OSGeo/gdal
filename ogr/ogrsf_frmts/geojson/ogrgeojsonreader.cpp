/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONReader class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
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
/*                           OGRGeoJSONReader                           */
/************************************************************************/

OGRGeoJSONReader::OGRGeoJSONReader()
    : poGJObject_( NULL ), poLayer_( NULL ),
        bGeometryPreserve_( true ),
        bAttributesSkip_( false ),
        bFlattenGeocouchSpatiallistFormat (-1), bFoundId (false), bFoundRev(false), bFoundTypeFeature(false), bIsGeocouchSpatiallistFormat(false)
{
    // Take a deep breath and get to work.
}

/************************************************************************/
/*                          ~OGRGeoJSONReader                           */
/************************************************************************/

OGRGeoJSONReader::~OGRGeoJSONReader()
{
    if( NULL != poGJObject_ )
    {
        json_object_put(poGJObject_);
    }

    poGJObject_ = NULL;
    poLayer_ = NULL;
}

/************************************************************************/
/*                           Parse                                      */
/************************************************************************/

OGRErr OGRGeoJSONReader::Parse( const char* pszText )
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
                      "GeoJSON parsing error: %s (at offset %d)",
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
/*                           ReadLayer                                  */
/************************************************************************/

OGRGeoJSONLayer* OGRGeoJSONReader::ReadLayer( const char* pszName,
                                              OGRGeoJSONDataSource* poDS )
{
    CPLAssert( NULL == poLayer_ );

    if( NULL == poGJObject_ )
    {
        CPLDebug( "GeoJSON",
                  "Missing parset GeoJSON data. Forgot to call Parse()?" );
        return NULL;
    }
        
    poLayer_ = new OGRGeoJSONLayer( pszName, NULL,
                                   OGRGeoJSONLayer::DefaultGeometryType,
                                   NULL, poDS );

    if( !GenerateLayerDefn() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Layer schema generation failed." );

        delete poLayer_;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Translate single geometry-only Feature object.                  */
/* -------------------------------------------------------------------- */
    GeoJSONObject::Type objType = OGRGeoJSONGetType( poGJObject_ );

    if( GeoJSONObject::ePoint == objType
        || GeoJSONObject::eMultiPoint == objType
        || GeoJSONObject::eLineString == objType
        || GeoJSONObject::eMultiLineString == objType
        || GeoJSONObject::ePolygon == objType
        || GeoJSONObject::eMultiPolygon == objType
        || GeoJSONObject::eGeometryCollection == objType )
    {
        OGRGeometry* poGeometry = NULL;
        poGeometry = ReadGeometry( poGJObject_ );
        if( !AddFeature( poGeometry ) )
        {
            CPLDebug( "GeoJSON",
                      "Translation of single geometry failed." );
            delete poLayer_;
            return NULL;
        }
    }
/* -------------------------------------------------------------------- */
/*      Translate single but complete Feature object.                   */
/* -------------------------------------------------------------------- */
    else if( GeoJSONObject::eFeature == objType )
    {
        OGRFeature* poFeature = NULL;
        poFeature = ReadFeature( poGJObject_ );
        if( !AddFeature( poFeature ) )
        {
            CPLDebug( "GeoJSON",
                      "Translation of single feature failed." );

            delete poLayer_;
            return NULL;
        }
    }
/* -------------------------------------------------------------------- */
/*      Translate multi-feature FeatureCollection object.               */
/* -------------------------------------------------------------------- */
    else if( GeoJSONObject::eFeatureCollection == objType )
    {
        OGRGeoJSONLayer* poThisLayer = NULL;
        poThisLayer = ReadFeatureCollection( poGJObject_ );
        CPLAssert( poLayer_ == poThisLayer );
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Unrecognized GeoJSON structure." );

        delete poLayer_;
        return NULL;
    }

    OGRSpatialReference* poSRS = NULL;
    poSRS = OGRGeoJSONReadSpatialReference( poGJObject_ );
    if (poSRS == NULL ) {
        // If there is none defined, we use 4326
        poSRS = new OGRSpatialReference();
        if( OGRERR_NONE != poSRS->importFromEPSG( 4326 ) )
        {
            delete poSRS;
            poSRS = NULL;
        }
        poLayer_->SetSpatialRef( poSRS );
        delete poSRS;
    }
    else {
        poLayer_->SetSpatialRef( poSRS );
        delete poSRS;
    }

    // TODO: FeatureCollection

    return poLayer_;
}

OGRSpatialReference* OGRGeoJSONReadSpatialReference( json_object* poObj) {
    
/* -------------------------------------------------------------------- */
/*      Read spatial reference definition.                              */
/* -------------------------------------------------------------------- */
    OGRSpatialReference* poSRS = NULL;

    json_object* poObjSrs = OGRGeoJSONFindMemberByName( poObj, "crs" );
    if( NULL != poObjSrs )
    {
        json_object* poObjSrsType = OGRGeoJSONFindMemberByName( poObjSrs, "type" );
        if (poObjSrsType == NULL)
            return NULL;

        const char* pszSrsType = json_object_get_string( poObjSrsType );

        // TODO: Add URL and URN types support
        if( EQUALN( pszSrsType, "NAME", 4 ) )
        {
            json_object* poObjSrsProps = OGRGeoJSONFindMemberByName( poObjSrs, "properties" );
            if (poObjSrsProps == NULL)
                return NULL;

            json_object* poNameURL = OGRGeoJSONFindMemberByName( poObjSrsProps, "name" );
            if (poNameURL == NULL)
                return NULL;

            const char* pszName = json_object_get_string( poNameURL );

            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE != poSRS->SetFromUserInput( pszName ) )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }

        if( EQUALN( pszSrsType, "EPSG", 4 ) )
        {
            json_object* poObjSrsProps = OGRGeoJSONFindMemberByName( poObjSrs, "properties" );
            if (poObjSrsProps == NULL)
                return NULL;

            json_object* poObjCode = OGRGeoJSONFindMemberByName( poObjSrsProps, "code" );
            if (poObjCode == NULL)
                return NULL;

            int nEPSG = json_object_get_int( poObjCode );

            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE != poSRS->importFromEPSG( nEPSG ) )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }

        if( EQUALN( pszSrsType, "URL", 3 ) || EQUALN( pszSrsType, "LINK", 4 )  )
        {
            json_object* poObjSrsProps = OGRGeoJSONFindMemberByName( poObjSrs, "properties" );
            if (poObjSrsProps == NULL)
                return NULL;

            json_object* poObjURL = OGRGeoJSONFindMemberByName( poObjSrsProps, "url" );
            
            if (NULL == poObjURL) {
                poObjURL = OGRGeoJSONFindMemberByName( poObjSrsProps, "href" );
            }
            if (poObjURL == NULL)
                return NULL;

            const char* pszURL = json_object_get_string( poObjURL );

            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE != poSRS->importFromUrl( pszURL ) )
            {
                delete poSRS;
                poSRS = NULL;

            }
        }


        if( EQUAL( pszSrsType, "OGC" ) )
        {
            json_object* poObjSrsProps = OGRGeoJSONFindMemberByName( poObjSrs, "properties" );
            if (poObjSrsProps == NULL)
                return NULL;

            json_object* poObjURN = OGRGeoJSONFindMemberByName( poObjSrsProps, "urn" );
            if (poObjURN == NULL)
                return NULL;

            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE != poSRS->importFromURN( json_object_get_string(poObjURN) ) )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }
    }
    
    return poSRS;
}
/************************************************************************/
/*                           SetPreserveGeometryType                    */
/************************************************************************/

void OGRGeoJSONReader::SetPreserveGeometryType( bool bPreserve )
{
    bGeometryPreserve_ = bPreserve;
}

/************************************************************************/
/*                           SetSkipAttributes                          */
/************************************************************************/

void OGRGeoJSONReader::SetSkipAttributes( bool bSkip )
{
    bAttributesSkip_ = bSkip;
}

/************************************************************************/
/*                           GenerateFeatureDefn                        */
/************************************************************************/

bool OGRGeoJSONReader::GenerateLayerDefn()
{
    CPLAssert( NULL != poGJObject_ );
    CPLAssert( NULL != poLayer_->GetLayerDefn() );
    CPLAssert( 0 == poLayer_->GetLayerDefn()->GetFieldCount() );

    bool bSuccess = true;

    if( bAttributesSkip_ )
        return true;

/* -------------------------------------------------------------------- */
/*      Scan all features and generate layer definition.				*/
/* -------------------------------------------------------------------- */
    GeoJSONObject::Type objType = OGRGeoJSONGetType( poGJObject_ );
    if( GeoJSONObject::eFeature == objType )
    {
        bSuccess = GenerateFeatureDefn( poGJObject_ );
    }
    else if( GeoJSONObject::eFeatureCollection == objType )
    {
        json_object* poObjFeatures = NULL;
        poObjFeatures = OGRGeoJSONFindMemberByName( poGJObject_, "features" );
        if( NULL != poObjFeatures
            && json_type_array == json_object_get_type( poObjFeatures ) )
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
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid FeatureCollection object. "
                      "Missing \'features\' member." );
            bSuccess = false;
        }
    }

/* -------------------------------------------------------------------- */
/*      Validate and add FID column if necessary.                       */
/* -------------------------------------------------------------------- */
	OGRFeatureDefn* poLayerDefn = poLayer_->GetLayerDefn();
	CPLAssert( NULL != poLayerDefn );

	bool bHasFID = false;

	for( int i = 0; i < poLayerDefn->GetFieldCount(); ++i )
	{
		OGRFieldDefn* poDefn = poLayerDefn->GetFieldDefn(i);
		if( EQUAL( poDefn->GetNameRef(), OGRGeoJSONLayer::DefaultFIDColumn )
			&& OFTInteger == poDefn->GetType() )
		{
			poLayer_->SetFIDColumn( poDefn->GetNameRef() );
            bHasFID = true;
            break;
		}
	}

    // TODO - mloskot: This is wrong! We want to add only FID field if
    // found in source layer (by default name or by FID_PROPERTY= specifier,
    // the latter has to be implemented).
    /*
    if( !bHasFID )
    {
        OGRFieldDefn fldDefn( OGRGeoJSONLayer::DefaultFIDColumn, OFTInteger );
        poLayerDefn->AddFieldDefn( &fldDefn );
        poLayer_->SetFIDColumn( fldDefn.GetNameRef() );
    }
    */

    return bSuccess;
}

bool OGRGeoJSONReader::GenerateFeatureDefn( json_object* poObj )
{
    OGRFeatureDefn* poDefn = poLayer_->GetLayerDefn();
    CPLAssert( NULL != poDefn );

    bool bSuccess = false;

/* -------------------------------------------------------------------- */
/*      Read collection of properties.									*/
/* -------------------------------------------------------------------- */
    json_object* poObjProps = NULL;
    poObjProps = OGRGeoJSONFindMemberByName( poObj, "properties" );
    if( NULL != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
    {
        if (bIsGeocouchSpatiallistFormat)
        {
            poObjProps = json_object_object_get(poObjProps, "properties");
            if( NULL == poObjProps ||
                json_object_get_type(poObjProps) != json_type_object )
            {
                return true;
            }
        }

        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poObjProps, it )
        {
            int nFldIndex = poDefn->GetFieldIndex( it.key );
            if( -1 == nFldIndex )
            {
                /* Detect the special kind of GeoJSON output by a spatiallist of GeoCouch */
                /* such as http://gd.iriscouch.com/cphosm/_design/geo/_rewrite/data?bbox=12.53%2C55.73%2C12.54%2C55.73 */
                if (strcmp(it.key, "_id") == 0)
                    bFoundId = true;
                else if (bFoundId && strcmp(it.key, "_rev") == 0)
                    bFoundRev = true;
                else if (bFoundRev && strcmp(it.key, "type") == 0 &&
                         it.val != NULL && json_object_get_type(it.val) == json_type_string &&
                         strcmp(json_object_get_string(it.val), "Feature") == 0)
                    bFoundTypeFeature = true;
                else if (bFoundTypeFeature && strcmp(it.key, "properties") == 0 &&
                         it.val != NULL && json_object_get_type(it.val) == json_type_object)
                {
                    if (bFlattenGeocouchSpatiallistFormat < 0)
                        bFlattenGeocouchSpatiallistFormat = CSLTestBoolean(
                            CPLGetConfigOption("GEOJSON_FLATTEN_GEOCOUCH", "TRUE"));
                    if (bFlattenGeocouchSpatiallistFormat)
                    {
                        poDefn->DeleteFieldDefn(poDefn->GetFieldIndex("type"));
                        bIsGeocouchSpatiallistFormat = true;
                        return GenerateFeatureDefn(poObj);
                    }
                }

                OGRFieldDefn fldDefn( it.key,
                    GeoJSONPropertyToFieldType( it.val ) );
                poDefn->AddFieldDefn( &fldDefn );
            }
            else
            {
                OGRFieldDefn* poFDefn = poDefn->GetFieldDefn(nFldIndex);
                OGRFieldType eType = poFDefn->GetType();
                if( eType == OFTInteger )
                {
                    OGRFieldType eNewType = GeoJSONPropertyToFieldType( it.val );
                    if( eNewType == OFTReal )
                        poFDefn->SetType(eNewType);
                }
            }
        }

        bSuccess = true; // SUCCESS
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Feature object. "
                  "Missing \'properties\' member." );
    }

    return bSuccess;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRGeoJSONReader::AddFeature( OGRGeometry* poGeometry )
{
    bool bAdded = false;

    // TODO: Should we check if geometry is of type of 
    //       wkbGeometryCollection ?

    if( NULL != poGeometry )
    {
        OGRFeature* poFeature = NULL;
        poFeature = new OGRFeature( poLayer_->GetLayerDefn() );
        poFeature->SetGeometryDirectly( poGeometry );

        bAdded = AddFeature( poFeature );
    }
    
    return bAdded;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRGeoJSONReader::AddFeature( OGRFeature* poFeature )
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
/*                           ReadGeometry                               */
/************************************************************************/

OGRGeometry* OGRGeoJSONReader::ReadGeometry( json_object* poObj )
{
    OGRGeometry* poGeometry = NULL;

    poGeometry = OGRGeoJSONReadGeometry( poObj );

/* -------------------------------------------------------------------- */
/*      Wrap geometry with GeometryCollection as a common denominator.  */
/*      Sometimes a GeoJSON text may consist of objects of different    */
/*      geometry types. Users may request wrapping all geometries with  */
/*      OGRGeometryCollection type by using option                      */
/*      GEOMETRY_AS_COLLECTION=NO|YES (NO is default).                 */
/* -------------------------------------------------------------------- */
    if( NULL != poGeometry )
    {
        if( !bGeometryPreserve_ 
            && wkbGeometryCollection != poGeometry->getGeometryType() )
        {
            OGRGeometryCollection* poMetaGeometry = NULL;
            poMetaGeometry = new OGRGeometryCollection();
            poMetaGeometry->addGeometryDirectly( poGeometry );
            return poMetaGeometry;
        }
    }

    return poGeometry;
}

/************************************************************************/
/*                           ReadFeature()                              */
/************************************************************************/

OGRFeature* OGRGeoJSONReader::ReadFeature( json_object* poObj )
{
    CPLAssert( NULL != poObj );
    CPLAssert( NULL != poLayer_ );

    OGRFeature* poFeature = NULL;
    poFeature = new OGRFeature( poLayer_->GetLayerDefn() );

/* -------------------------------------------------------------------- */
/*      Translate GeoJSON "properties" object to feature attributes.    */
/* -------------------------------------------------------------------- */
    CPLAssert( NULL != poFeature );

    json_object* poObjProps = NULL;
    poObjProps = OGRGeoJSONFindMemberByName( poObj, "properties" );
    if( !bAttributesSkip_ && NULL != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
    {
        if (bIsGeocouchSpatiallistFormat)
        {
            json_object* poId = json_object_object_get(poObjProps, "_id");
            if (poId != NULL && json_object_get_type(poId) == json_type_string)
                poFeature->SetField( "_id", json_object_get_string(poId) );

            json_object* poRev = json_object_object_get(poObjProps, "_rev");
            if (poRev != NULL && json_object_get_type(poRev) == json_type_string)
                poFeature->SetField( "_rev", json_object_get_string(poRev) );

            poObjProps = json_object_object_get(poObjProps, "properties");
            if( NULL == poObjProps ||
                json_object_get_type(poObjProps) != json_type_object )
            {
                return poFeature;
            }
        }

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
            CPLAssert( NULL != poFieldDefn );
            OGRFieldType eType = poFieldDefn->GetType();

            if( it.val == NULL)
            {
                /* nothing to do */
            }
            else if( OFTInteger == eType )
			{
                poFeature->SetField( nField, json_object_get_int(it.val) );
				
				/* Check if FID available and set correct value. */
				if( EQUAL( it.key, poLayer_->GetFIDColumn() ) )
					poFeature->SetFID( json_object_get_int(it.val) );
			}
            else if( OFTReal == eType )
			{
                poFeature->SetField( nField, json_object_get_double(it.val) );
			}
            else if( OFTIntegerList == eType )
            {
                if ( json_object_get_type(it.val) == json_type_array )
                {
                    int nLength = json_object_array_length(it.val);
                    int* panVal = (int*)CPLMalloc(sizeof(int) * nLength);
                    for(int i=0;i<nLength;i++)
                    {
                        json_object* poRow = json_object_array_get_idx(it.val, i);
                        panVal[i] = json_object_get_int(poRow);
                    }
                    poFeature->SetField( nField, nLength, panVal );
                    CPLFree(panVal);
                }
            }
            else if( OFTRealList == eType )
            {
                if ( json_object_get_type(it.val) == json_type_array )
                {
                    int nLength = json_object_array_length(it.val);
                    double* padfVal = (double*)CPLMalloc(sizeof(double) * nLength);
                    for(int i=0;i<nLength;i++)
                    {
                        json_object* poRow = json_object_array_get_idx(it.val, i);
                        padfVal[i] = json_object_get_double(poRow);
                    }
                    poFeature->SetField( nField, nLength, padfVal );
                    CPLFree(padfVal);
                }
            }
            else if( OFTStringList == eType )
            {
                if ( json_object_get_type(it.val) == json_type_array )
                {
                    int nLength = json_object_array_length(it.val);
                    char** papszVal = (char**)CPLMalloc(sizeof(char*) * (nLength+1));
                    int i;
                    for(i=0;i<nLength;i++)
                    {
                        json_object* poRow = json_object_array_get_idx(it.val, i);
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
                poFeature->SetField( nField, json_object_get_string(it.val) );
			}
        }
    }

/* -------------------------------------------------------------------- */
/*      If FID not set, try to use feature-level ID if available        */
/*      and of integral type. Otherwise, leave unset (-1) then index    */
/*      in features sequence will be used as FID.                       */
/* -------------------------------------------------------------------- */
    if( -1 == poFeature->GetFID() )
    {
        json_object* poObjId = NULL;
        poObjId = OGRGeoJSONFindMemberByName( poObj, OGRGeoJSONLayer::DefaultFIDColumn );
        if( NULL != poObjId
            && EQUAL( OGRGeoJSONLayer::DefaultFIDColumn, poLayer_->GetFIDColumn() )
            && OFTInteger == GeoJSONPropertyToFieldType( poObjId ) )
        {
            poFeature->SetFID( json_object_get_int( poObjId ) );
            int nField = poFeature->GetFieldIndex( poLayer_->GetFIDColumn() );
            if( -1 != nField )
                poFeature->SetField( nField, (int) poFeature->GetFID() );
        }
    }

    if( -1 == poFeature->GetFID() )
    {
        json_object* poObjId = OGRGeoJSONFindMemberByName( poObj, "id" );
        if (poObjId != NULL && json_object_get_type(poObjId) == json_type_int)
            poFeature->SetFID( json_object_get_int( poObjId ) );
    }

/* -------------------------------------------------------------------- */
/*      Translate geometry sub-object of GeoJSON Feature.               */
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
        // NOTE: If geometry can not be parsed or read correctly
        //       then NULL geometry is assigned to a feature and
        //       geometry type for layer is classified as wkbUnknown.
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
OGRGeoJSONReader::ReadFeatureCollection( json_object* poObj )
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
            poFeature = OGRGeoJSONReader::ReadFeature( poObjFeature );
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
/*                           OGRGeoJSONFindMemberByName                 */
/************************************************************************/

json_object* OGRGeoJSONFindMemberByName( json_object* poObj,
                                         const char* pszName )
{
    if( NULL == pszName || NULL == poObj)
        return NULL;

    json_object* poTmp = poObj;

    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    if( NULL != json_object_get_object(poTmp) &&
        NULL != json_object_get_object(poTmp)->head )
    {
        for( it.entry = json_object_get_object(poTmp)->head;
             ( it.entry ?
               ( it.key = (char*)it.entry->k,
                 it.val = (json_object*)it.entry->v, it.entry) : 0);
             it.entry = it.entry->next)
        {
            if( EQUAL( it.key, pszName ) )
                return it.val;
        }
    }

    return NULL;
}

/************************************************************************/
/*                           OGRGeoJSONGetType                          */
/************************************************************************/

GeoJSONObject::Type OGRGeoJSONGetType( json_object* poObj )
{
    if( NULL == poObj )
        return GeoJSONObject::eUnknown;

    json_object* poObjType = NULL;
    poObjType = OGRGeoJSONFindMemberByName( poObj, "type" );
    if( NULL == poObjType )
        return GeoJSONObject::eUnknown;

    const char* name = json_object_get_string( poObjType );
    if( EQUAL( name, "Point" ) )
        return GeoJSONObject::ePoint;
    else if( EQUAL( name, "LineString" ) )
        return GeoJSONObject::eLineString;
    else if( EQUAL( name, "Polygon" ) )
        return GeoJSONObject::ePolygon;
    else if( EQUAL( name, "MultiPoint" ) )
        return GeoJSONObject::eMultiPoint;
    else if( EQUAL( name, "MultiLineString" ) )
        return GeoJSONObject::eMultiLineString;
    else if( EQUAL( name, "MultiPolygon" ) )
        return GeoJSONObject::eMultiPolygon;
    else if( EQUAL( name, "GeometryCollection" ) )
        return GeoJSONObject::eGeometryCollection;
    else if( EQUAL( name, "Feature" ) )
        return GeoJSONObject::eFeature;
    else if( EQUAL( name, "FeatureCollection" ) )
        return GeoJSONObject::eFeatureCollection;
    else
        return GeoJSONObject::eUnknown;
}

/************************************************************************/
/*                           OGRGeoJSONReadGeometry                     */
/************************************************************************/

OGRGeometry* OGRGeoJSONReadGeometry( json_object* poObj )
{
    OGRGeometry* poGeometry = NULL;

    GeoJSONObject::Type objType = OGRGeoJSONGetType( poObj );
    if( GeoJSONObject::ePoint == objType )
        poGeometry = OGRGeoJSONReadPoint( poObj );
    else if( GeoJSONObject::eMultiPoint == objType )
        poGeometry = OGRGeoJSONReadMultiPoint( poObj );
    else if( GeoJSONObject::eLineString == objType )
        poGeometry = OGRGeoJSONReadLineString( poObj );
    else if( GeoJSONObject::eMultiLineString == objType )
        poGeometry = OGRGeoJSONReadMultiLineString( poObj );
    else if( GeoJSONObject::ePolygon == objType )
        poGeometry = OGRGeoJSONReadPolygon( poObj );
    else if( GeoJSONObject::eMultiPolygon == objType )
        poGeometry = OGRGeoJSONReadMultiPolygon( poObj );
    else if( GeoJSONObject::eGeometryCollection == objType )
        poGeometry = OGRGeoJSONReadGeometryCollection( poObj );
    else
    {
        CPLDebug( "GeoJSON",
                  "Unsupported geometry type detected. "
                  "Feature gets NULL geometry assigned." );
    }
    // If we have a crs object in the current object, let's try and 
    // set it too.
    
    json_object* poObjSrs = OGRGeoJSONFindMemberByName( poObj, "crs" );
    if (poObjSrs != NULL) {
        OGRSpatialReference* poSRS = OGRGeoJSONReadSpatialReference(poObj);
        if (poSRS != NULL) {
            poGeometry->assignSpatialReference(poSRS);
            poSRS->Release();
        }
    }
    return poGeometry;
}

/************************************************************************/
/*                           OGRGeoJSONReadRawPoint                     */
/************************************************************************/

bool OGRGeoJSONReadRawPoint( json_object* poObj, OGRPoint& point )
{
    CPLAssert( NULL != poObj );

    if( json_type_array == json_object_get_type( poObj ) ) 
    {
        const int nSize = json_object_array_length( poObj );
        int iType = 0;

        if( nSize != GeoJSONObject::eMinCoordinateDimension
            && nSize != GeoJSONObject::eMaxCoordinateDimension )
        {
            CPLDebug( "GeoJSON",
                      "Invalid coord dimension. Only 2D and 3D supported." );
            return false;
        }

        json_object* poObjCoord = NULL;

        // Read X coordinate
        poObjCoord = json_object_array_get_idx( poObj, 0 );
        if (poObjCoord == NULL)
        {
            CPLDebug( "GeoJSON", "Point: got null object." );
            return false;
        }
        
        iType = json_object_get_type(poObjCoord);
        if ( (json_type_double != iType) && (json_type_int != iType) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid X coordinate. Type is not double or integer for \'%s\'.",
                      json_object_to_json_string(poObj) );
            return false;
        }
        
        if (iType == json_type_double)
            point.setX(json_object_get_double( poObjCoord ));
        else
            point.setX(json_object_get_int( poObjCoord ));
        
        // Read Y coordiante
        poObjCoord = json_object_array_get_idx( poObj, 1 );
        if (poObjCoord == NULL)
        {
            CPLDebug( "GeoJSON", "Point: got null object." );
            return false;
        }
        
        iType = json_object_get_type(poObjCoord);
        if ( (json_type_double != iType) && (json_type_int != iType) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid Y coordinate. Type is not double or integer for \'%s\'.",
                      json_object_to_json_string(poObj) );
            return false;
        }

        if (iType == json_type_double)
            point.setY(json_object_get_double( poObjCoord ));
        else
            point.setY(json_object_get_int( poObjCoord ));
        
        // Read Z coordinate
        if( nSize == GeoJSONObject::eMaxCoordinateDimension )
        {
            // Don't *expect* mixed-dimension geometries, although the 
            // spec doesn't explicitly forbid this.
            poObjCoord = json_object_array_get_idx( poObj, 2 );
            if (poObjCoord == NULL)
            {
                CPLDebug( "GeoJSON", "Point: got null object." );
                return false;
            }
            
            iType = json_object_get_type(poObjCoord);
            if ( (json_type_double != iType) && (json_type_int != iType) )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "Invalid Z coordinate. Type is not double or integer for \'%s\'.",
                          json_object_to_json_string(poObj) );
                return false;
            }

            if (iType == json_type_double)
                point.setZ(json_object_get_double( poObjCoord ));
            else
                point.setZ(json_object_get_int( poObjCoord ));
        }
        else
        {
            point.flattenTo2D();
        }
        return true;
    }
    
    return false;
}

/************************************************************************/
/*                           OGRGeoJSONReadPoint                        */
/************************************************************************/

OGRPoint* OGRGeoJSONReadPoint( json_object* poObj )
{
    CPLAssert( NULL != poObj );

    json_object* poObjCoords = NULL;
    poObjCoords = OGRGeoJSONFindMemberByName( poObj, "coordinates" );
    if( NULL == poObjCoords )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid Point object. Missing \'coordinates\' member." );
        return NULL;
    }

    OGRPoint* poPoint = new OGRPoint();
    if( !OGRGeoJSONReadRawPoint( poObjCoords, *poPoint ) )
    {
        CPLDebug( "GeoJSON", "Point: raw point parsing failure." );
        delete poPoint;
        return NULL;
    }

    return poPoint;
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiPoint                   */
/************************************************************************/

OGRMultiPoint* OGRGeoJSONReadMultiPoint( json_object* poObj )
{
    CPLAssert( NULL != poObj );

    OGRMultiPoint* poMultiPoint = NULL;

    json_object* poObjPoints = NULL;
    poObjPoints = OGRGeoJSONFindMemberByName( poObj, "coordinates" );
    if( NULL == poObjPoints )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MultiPoint object. "
                  "Missing \'coordinates\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjPoints ) )
    {
        const int nPoints = json_object_array_length( poObjPoints );

        poMultiPoint = new OGRMultiPoint();

        for( int i = 0; i < nPoints; ++i)
        {
            json_object* poObjCoords = NULL;
            poObjCoords = json_object_array_get_idx( poObjPoints, i );

            OGRPoint pt;
            if( poObjCoords != NULL && !OGRGeoJSONReadRawPoint( poObjCoords, pt ) )
            {
                delete poMultiPoint;
                CPLDebug( "GeoJSON",
                          "LineString: raw point parsing failure." );
                return NULL;
            }
            poMultiPoint->addGeometry( &pt );
        }
    }

    return poMultiPoint;
}

/************************************************************************/
/*                           OGRGeoJSONReadLineString                   */
/************************************************************************/

OGRLineString* OGRGeoJSONReadLineString( json_object* poObj , bool bRaw)
{
    CPLAssert( NULL != poObj );

    OGRLineString* poLine = NULL;
    json_object* poObjPoints = NULL;
    
    if( !bRaw )
    {
        poObjPoints = OGRGeoJSONFindMemberByName( poObj, "coordinates" );
        if( NULL == poObjPoints )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                    "Invalid LineString object. "
                    "Missing \'coordinates\' member." );
                return NULL;
        }
    }
    else
    {
        poObjPoints = poObj;
    }

    if( json_type_array == json_object_get_type( poObjPoints ) )
    {
        const int nPoints = json_object_array_length( poObjPoints );

        poLine = new OGRLineString();
        poLine->setNumPoints( nPoints );

        for( int i = 0; i < nPoints; ++i)
        {
            json_object* poObjCoords = NULL;
            poObjCoords = json_object_array_get_idx( poObjPoints, i );
            if (poObjCoords == NULL)
            {
                delete poLine;
                CPLDebug( "GeoJSON",
                          "LineString: got null object." );
                return NULL;
            }
            
            OGRPoint pt;
            if( !OGRGeoJSONReadRawPoint( poObjCoords, pt ) )
            {
                delete poLine;
                CPLDebug( "GeoJSON",
                          "LineString: raw point parsing failure." );
                return NULL;
            }
            if (pt.getCoordinateDimension() == 2) {
                poLine->setPoint( i, pt.getX(), pt.getY());
            } else {
                poLine->setPoint( i, pt.getX(), pt.getY(), pt.getZ() );
            }
            
        }
    }

    return poLine;
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiLineString              */
/************************************************************************/

OGRMultiLineString* OGRGeoJSONReadMultiLineString( json_object* poObj )
{
    CPLAssert( NULL != poObj );

    OGRMultiLineString* poMultiLine = NULL;

    json_object* poObjLines = NULL;
    poObjLines = OGRGeoJSONFindMemberByName( poObj, "coordinates" );
    if( NULL == poObjLines )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MultiLineString object. "
                  "Missing \'coordinates\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjLines ) )
    {
        const int nLines = json_object_array_length( poObjLines );

        poMultiLine = new OGRMultiLineString();

        for( int i = 0; i < nLines; ++i)
        {
            json_object* poObjLine = NULL;
            poObjLine = json_object_array_get_idx( poObjLines, i );

            OGRLineString* poLine;
            if (poObjLine != NULL)
                poLine = OGRGeoJSONReadLineString( poObjLine , true );
            else
                poLine = new OGRLineString();

            if( NULL != poLine )
            {
                poMultiLine->addGeometryDirectly( poLine );
            }
        }
    }

    return poMultiLine;
}

/************************************************************************/
/*                           OGRGeoJSONReadLinearRing                   */
/************************************************************************/

OGRLinearRing* OGRGeoJSONReadLinearRing( json_object* poObj )
{
    CPLAssert( NULL != poObj );

    OGRLinearRing* poRing = NULL;

    if( json_type_array == json_object_get_type( poObj ) )
    {
        const int nPoints = json_object_array_length( poObj );

        poRing= new OGRLinearRing();
        poRing->setNumPoints( nPoints );

        for( int i = 0; i < nPoints; ++i)
        {
            json_object* poObjCoords = NULL;
            poObjCoords = json_object_array_get_idx( poObj, i );
            if (poObjCoords == NULL)
            {
                delete poRing;
                CPLDebug( "GeoJSON",
                          "LinearRing: got null object." );
                return NULL;
            }

            OGRPoint pt;
            if( !OGRGeoJSONReadRawPoint( poObjCoords, pt ) )
            {
                delete poRing;
                CPLDebug( "GeoJSON",
                          "LinearRing: raw point parsing failure." );
                return NULL;
            }
            
            if( 2 == pt.getCoordinateDimension() )
                poRing->setPoint( i, pt.getX(), pt.getY());
            else
                poRing->setPoint( i, pt.getX(), pt.getY(), pt.getZ() );
        }
    }

    return poRing;
}

/************************************************************************/
/*                           OGRGeoJSONReadPolygon                      */
/************************************************************************/

OGRPolygon* OGRGeoJSONReadPolygon( json_object* poObj , bool bRaw )
{
    CPLAssert( NULL != poObj );

    OGRPolygon* poPolygon = NULL;

    json_object* poObjRings = NULL;
    
    if( !bRaw )
    {
        poObjRings = OGRGeoJSONFindMemberByName( poObj, "coordinates" );
        if( NULL == poObjRings )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Invalid Polygon object. "
                      "Missing \'coordinates\' member." );
            return NULL;
        }
    }
    else
    {
        poObjRings = poObj;
    }
    
    if( json_type_array == json_object_get_type( poObjRings ) )
    {
        const int nRings = json_object_array_length( poObjRings );
        if( nRings > 0 )
        {
            json_object* poObjPoints = NULL;
            poObjPoints = json_object_array_get_idx( poObjRings, 0 );
            if (poObjPoints == NULL)
            {
                poPolygon = new OGRPolygon();
                poPolygon->addRingDirectly( new OGRLinearRing() );
            }
            else
            {
                OGRLinearRing* poRing = OGRGeoJSONReadLinearRing( poObjPoints );
                if( NULL != poRing )
                {
                    poPolygon = new OGRPolygon();
                    poPolygon->addRingDirectly( poRing );
                }
            }

            for( int i = 1; i < nRings && NULL != poPolygon; ++i )
            {
                poObjPoints = json_object_array_get_idx( poObjRings, i );
                if (poObjPoints == NULL)
                {
                    poPolygon->addRingDirectly( new OGRLinearRing() );
                }
                else
                {
                    OGRLinearRing* poRing = OGRGeoJSONReadLinearRing( poObjPoints );
                    if( NULL != poRing )
                    {
                        poPolygon->addRingDirectly( poRing );
                    }
                }
            }
        }
    }

    return poPolygon;
}

/************************************************************************/
/*                           OGRGeoJSONReadMultiPolygon                 */
/************************************************************************/

OGRMultiPolygon* OGRGeoJSONReadMultiPolygon( json_object* poObj )
{
    CPLAssert( NULL != poObj );

    OGRMultiPolygon* poMultiPoly = NULL;

    json_object* poObjPolys = NULL;
    poObjPolys = OGRGeoJSONFindMemberByName( poObj, "coordinates" );
    if( NULL == poObjPolys )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MultiPolygon object. "
                  "Missing \'coordinates\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjPolys ) )
    {
        const int nPolys = json_object_array_length( poObjPolys );

        poMultiPoly = new OGRMultiPolygon();

        for( int i = 0; i < nPolys; ++i)
        {

            json_object* poObjPoly = NULL;
            poObjPoly = json_object_array_get_idx( poObjPolys, i );
            if (poObjPoly == NULL)
            {
                poMultiPoly->addGeometryDirectly( new OGRPolygon() );
            }
            else
            {
                OGRPolygon* poPoly = OGRGeoJSONReadPolygon( poObjPoly , true );
                if( NULL != poPoly )
                {
                    poMultiPoly->addGeometryDirectly( poPoly );
                }
            }
        }
    }

    return poMultiPoly;
}
/************************************************************************/
/*                           OGRGeoJSONReadGeometryCollection           */
/************************************************************************/

OGRGeometryCollection* OGRGeoJSONReadGeometryCollection( json_object* poObj )
{
    CPLAssert( NULL != poObj );

    OGRGeometry* poGeometry = NULL;
    OGRGeometryCollection* poCollection = NULL;

    json_object* poObjGeoms = NULL;
    poObjGeoms = OGRGeoJSONFindMemberByName( poObj, "geometries" );
    if( NULL == poObjGeoms )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid GeometryCollection object. "
                  "Missing \'geometries\' member." );
        return NULL;
    }

    if( json_type_array == json_object_get_type( poObjGeoms ) )
    {
        const int nGeoms = json_object_array_length( poObjGeoms );
        if( nGeoms > 0 )
        {
            poCollection = new OGRGeometryCollection();
        }

        json_object* poObjGeom = NULL;
        for( int i = 0; i < nGeoms; ++i )
        {
            poObjGeom = json_object_array_get_idx( poObjGeoms, i );
            if (poObjGeom == NULL)
            {
                CPLDebug( "GeoJSON", "Skipping null sub-geometry");
                continue;
            }

            poGeometry = OGRGeoJSONReadGeometry( poObjGeom );
            if( NULL != poGeometry )
            {
                poCollection->addGeometryDirectly( poGeometry );
            }
        }
    }

    return poCollection;
}

/************************************************************************/
/*                           OGR_G_ExportToJson                         */
/************************************************************************/

OGRGeometryH OGR_G_CreateGeometryFromJson( const char* pszJson )
{
    VALIDATE_POINTER1( pszJson, "OGR_G_CreateGeometryFromJson", NULL );

    if( NULL != pszJson )
    {
        json_tokener* jstok = NULL;
        json_object* poObj = NULL;

        jstok = json_tokener_new();
        poObj = json_tokener_parse_ex(jstok, pszJson, -1);
        if( jstok->err != json_tokener_success)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "GeoJSON parsing error: %s (at offset %d)",
                      json_tokener_errors[jstok->err], jstok->char_offset);
            json_tokener_free(jstok);
            return NULL;
        }
        json_tokener_free(jstok);

        OGRGeometry* poGeometry = NULL;
        poGeometry = OGRGeoJSONReadGeometry( poObj );
        
        /* Release JSON tree. */
        json_object_put( poObj );

        return (OGRGeometryH)poGeometry;
    }

    /* Translation failed */
    return NULL;
}

