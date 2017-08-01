/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRGeoJSONReader class (OGR GeoJSON Driver).
 * Author:   Mateusz Loskot, mateusz@loskot.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Mateusz Loskot
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

CPL_CVSID("$Id$")

static
OGRGeometry* OGRGeoJSONReadGeometry( json_object* poObj,
                                     OGRSpatialReference* poLayerSRS );

/************************************************************************/
/*                           OGRGeoJSONReader                           */
/************************************************************************/

OGRGeoJSONReader::OGRGeoJSONReader() :
    poGJObject_(NULL),
    bGeometryPreserve_(true),
    bAttributesSkip_(false),
    bFlattenNestedAttributes_(false),
    chNestedAttributeSeparator_(0),
    bStoreNativeData_(false),
    bArrayAsString_(false),
    bFlattenGeocouchSpatiallistFormat(-1),
    bFoundId(false),
    bFoundRev(false),
    bFoundTypeFeature(false),
    bIsGeocouchSpatiallistFormat(false),
    bFoundFeatureId(false)
{}

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
}

/************************************************************************/
/*                           Parse                                      */
/************************************************************************/

OGRErr OGRGeoJSONReader::Parse( const char* pszText )
{
    if( NULL != pszText )
    {
        // Skip UTF-8 BOM (#5630).
        const GByte* pabyData = (const GByte*)pszText;
        if( pabyData[0] == 0xEF && pabyData[1] == 0xBB && pabyData[2] == 0xBF )
        {
            CPLDebug("GeoJSON", "Skip UTF-8 BOM");
            pszText += 3;
        }

        // JSON tree is shared for while lifetime of the reader object
        // and will be released in the destructor.
        if( !OGRJSonParse(pszText, &poGJObject_) )
            return OGRERR_CORRUPT_DATA;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReadLayers                                 */
/************************************************************************/

void OGRGeoJSONReader::ReadLayers( OGRGeoJSONDataSource* poDS )
{
    if( NULL == poGJObject_ )
    {
        CPLDebug( "GeoJSON",
                  "Missing parsed GeoJSON data. Forgot to call Parse()?" );
        return;
    }

    ReadLayer(poDS, NULL, poGJObject_);
}

/************************************************************************/
/*                           ReadLayer                                  */
/************************************************************************/

void OGRGeoJSONReader::ReadLayer( OGRGeoJSONDataSource* poDS,
                                  const char* pszName,
                                  json_object* poObj )
{
    GeoJSONObject::Type objType = OGRGeoJSONGetType( poObj );
    if( objType == GeoJSONObject::eUnknown )
    {
        // Check if the object contains key:value pairs where value
        // is a standard GeoJSON object. In which case, use key as the layer
        // name.
        if( json_type_object == json_object_get_type( poObj ) )
        {
            json_object_iter it;
            it.key = NULL;
            it.val = NULL;
            it.entry = NULL;
            json_object_object_foreachC( poObj, it )
            {
                objType = OGRGeoJSONGetType( it.val );
                if( objType != GeoJSONObject::eUnknown )
                    ReadLayer(poDS, it.key, it.val);
            }
        }

        // CPLError(CE_Failure, CPLE_AppDefined,
        //          "Unrecognized GeoJSON structure.");

        return;
    }

    OGRSpatialReference* poSRS = OGRGeoJSONReadSpatialReference( poObj );
    if( poSRS == NULL )
    {
        // If there is none defined, we use 4326.
        poSRS = new OGRSpatialReference();
        poSRS->SetFromUserInput(SRS_WKT_WGS84);
    }

    CPLErrorReset();

    // Figure out layer name
    if( pszName == NULL )
    {
        if( GeoJSONObject::eFeatureCollection == objType )
        {
            json_object* poName = CPL_json_object_object_get(poObj, "name");
            if( poName != NULL &&
                json_object_get_type(poName) == json_type_string )
            {
                pszName = json_object_get_string(poName);
            }
        }
        if( pszName == NULL )
        {
            const char* pszDesc = poDS->GetDescription();
            if( strchr(pszDesc, '?') == NULL &&
                strchr(pszDesc, '{') == NULL )
            {
                pszName = CPLGetBasename(pszDesc);
            }
        }
        if( pszName == NULL )
            pszName = OGRGeoJSONLayer::DefaultName;
    }

    OGRGeoJSONLayer* poLayer =
      new OGRGeoJSONLayer( pszName, poSRS,
                           OGRGeoJSONLayer::DefaultGeometryType,
                           poDS );
    if( poSRS != NULL )
        poSRS->Release();

    if( !GenerateLayerDefn(poLayer, poObj) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer schema generation failed." );

        delete poLayer;
        return;
    }

    if( GeoJSONObject::eFeatureCollection == objType )
    {
        json_object* poDescription =
                        CPL_json_object_object_get(poObj, "description");
        if( poDescription != NULL &&
            json_object_get_type(poDescription) == json_type_string )
        {
            poLayer->SetMetadataItem("DESCRIPTION",
                                     json_object_get_string(poDescription));
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate single geometry-only Feature object.                  */
/* -------------------------------------------------------------------- */

    if( GeoJSONObject::ePoint == objType
        || GeoJSONObject::eMultiPoint == objType
        || GeoJSONObject::eLineString == objType
        || GeoJSONObject::eMultiLineString == objType
        || GeoJSONObject::ePolygon == objType
        || GeoJSONObject::eMultiPolygon == objType
        || GeoJSONObject::eGeometryCollection == objType )
    {
        OGRGeometry* poGeometry = ReadGeometry( poObj, poLayer->GetSpatialRef() );
        if( !AddFeature( poLayer, poGeometry ) )
        {
            CPLDebug( "GeoJSON", "Translation of single geometry failed." );
            delete poLayer;
            return;
        }
    }
/* -------------------------------------------------------------------- */
/*      Translate single but complete Feature object.                   */
/* -------------------------------------------------------------------- */
    else if( GeoJSONObject::eFeature == objType )
    {
        OGRFeature* poFeature = ReadFeature( poLayer, poObj );
        AddFeature( poLayer, poFeature );
    }
/* -------------------------------------------------------------------- */
/*      Translate multi-feature FeatureCollection object.               */
/* -------------------------------------------------------------------- */
    else if( GeoJSONObject::eFeatureCollection == objType )
    {
        ReadFeatureCollection( poLayer, poObj );
    }

    if( CPLGetLastErrorType() != CE_Warning )
        CPLErrorReset();

    poDS->AddLayer(poLayer);
}

/************************************************************************/
/*                    OGRGeoJSONReadSpatialReference                    */
/************************************************************************/

OGRSpatialReference* OGRGeoJSONReadSpatialReference( json_object* poObj )
{

/* -------------------------------------------------------------------- */
/*      Read spatial reference definition.                              */
/* -------------------------------------------------------------------- */
    OGRSpatialReference* poSRS = NULL;

    json_object* poObjSrs = OGRGeoJSONFindMemberByName( poObj, "crs" );
    if( NULL != poObjSrs )
    {
        json_object* poObjSrsType =
            OGRGeoJSONFindMemberByName( poObjSrs, "type" );
        if( poObjSrsType == NULL )
            return NULL;

        const char* pszSrsType = json_object_get_string( poObjSrsType );

        // TODO: Add URL and URN types support.
        if( STARTS_WITH_CI(pszSrsType, "NAME") )
        {
            json_object* poObjSrsProps =
                OGRGeoJSONFindMemberByName( poObjSrs, "properties" );
            if( poObjSrsProps == NULL )
                return NULL;

            json_object* poNameURL =
                OGRGeoJSONFindMemberByName( poObjSrsProps, "name" );
            if( poNameURL == NULL )
                return NULL;

            const char* pszName = json_object_get_string( poNameURL );

            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE != poSRS->SetFromUserInput( pszName ) )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }

        if( STARTS_WITH_CI(pszSrsType, "EPSG") )
        {
            json_object* poObjSrsProps =
                OGRGeoJSONFindMemberByName( poObjSrs, "properties" );
            if( poObjSrsProps == NULL )
                return NULL;

            json_object* poObjCode =
                OGRGeoJSONFindMemberByName( poObjSrsProps, "code" );
            if( poObjCode == NULL )
                return NULL;

            int nEPSG = json_object_get_int( poObjCode );

            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE != poSRS->importFromEPSG( nEPSG ) )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }

        if( STARTS_WITH_CI(pszSrsType, "URL") ||
            STARTS_WITH_CI(pszSrsType, "LINK")  )
        {
            json_object* poObjSrsProps =
                OGRGeoJSONFindMemberByName( poObjSrs, "properties" );
            if( poObjSrsProps == NULL )
                return NULL;

            json_object* poObjURL =
                OGRGeoJSONFindMemberByName( poObjSrsProps, "url" );

            if( NULL == poObjURL )
            {
                poObjURL = OGRGeoJSONFindMemberByName( poObjSrsProps, "href" );
            }
            if( poObjURL == NULL )
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
            json_object* poObjSrsProps =
                OGRGeoJSONFindMemberByName( poObjSrs, "properties" );
            if( poObjSrsProps == NULL )
                return NULL;

            json_object* poObjURN =
                OGRGeoJSONFindMemberByName( poObjSrsProps, "urn" );
            if( poObjURN == NULL )
                return NULL;

            poSRS = new OGRSpatialReference();
            if( OGRERR_NONE !=
                poSRS->importFromURN( json_object_get_string(poObjURN) ) )
            {
                delete poSRS;
                poSRS = NULL;
            }
        }
    }

    // Strip AXIS, since geojson has (easting, northing) / (longitude, latitude)
    // order.  According to http://www.geojson.org/geojson-spec.html#id2 :
    // "Point coordinates are in x, y order (easting, northing for projected
    // coordinates, longitude, latitude for geographic coordinates)".
    if( poSRS != NULL )
    {
        OGR_SRSNode *poGEOGCS = poSRS->GetAttrNode( "GEOGCS" );
        if( poGEOGCS != NULL )
            poGEOGCS->StripNodes( "AXIS" );
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
/*                         SetFlattenNestedAttributes                   */
/************************************************************************/

void OGRGeoJSONReader::SetFlattenNestedAttributes( bool bFlatten,
                                                   char chSeparator )
{
    bFlattenNestedAttributes_ = bFlatten;
    chNestedAttributeSeparator_ = chSeparator;
}

/************************************************************************/
/*                           SetStoreNativeData                         */
/************************************************************************/

void OGRGeoJSONReader::SetStoreNativeData( bool bStoreNativeData )
{
    bStoreNativeData_ = bStoreNativeData;
}

/************************************************************************/
/*                           SetArrayAsString                           */
/************************************************************************/

void OGRGeoJSONReader::SetArrayAsString( bool bArrayAsString )
{
    bArrayAsString_ = bArrayAsString;
}

/************************************************************************/
/*                         GenerateLayerDefn()                          */
/************************************************************************/

bool OGRGeoJSONReader::GenerateLayerDefn( OGRGeoJSONLayer* poLayer,
                                          json_object* poGJObject )
{
    CPLAssert( NULL != poGJObject );
    CPLAssert( NULL != poLayer->GetLayerDefn() );
    CPLAssert( 0 == poLayer->GetLayerDefn()->GetFieldCount() );

    if( bAttributesSkip_ )
        return true;

/* -------------------------------------------------------------------- */
/*      Scan all features and generate layer definition.                */
/* -------------------------------------------------------------------- */
    bool bSuccess = true;

    GeoJSONObject::Type objType = OGRGeoJSONGetType( poGJObject );
    if( GeoJSONObject::eFeature == objType )
    {
        bSuccess = GenerateFeatureDefn( poLayer, poGJObject );
    }
    else if( GeoJSONObject::eFeatureCollection == objType )
    {
        json_object* poObjFeatures
            = OGRGeoJSONFindMemberByName( poGJObject, "features" );
        if( NULL != poObjFeatures
            && json_type_array == json_object_get_type( poObjFeatures ) )
        {
            const int nFeatures = json_object_array_length( poObjFeatures );
            for( int i = 0; i < nFeatures; ++i )
            {
                json_object* poObjFeature =
                    json_object_array_get_idx( poObjFeatures, i );
                if( !GenerateFeatureDefn( poLayer, poObjFeature ) )
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
    OGRFeatureDefn* poLayerDefn = poLayer->GetLayerDefn();
    CPLAssert( NULL != poLayerDefn );

    if( !bFoundFeatureId )
    {
        const int idx = poLayerDefn->GetFieldIndex( "id" );
        if( idx >= 0 )
        {
            OGRFieldDefn* poFDefn = poLayerDefn->GetFieldDefn(idx);
            if( poFDefn->GetType() == OFTInteger ||
                poFDefn->GetType() == OFTInteger64 )
            {
                poLayer->SetFIDColumn(
                    poLayerDefn->GetFieldDefn(idx)->GetNameRef() );
            }
        }
    }

    return bSuccess;
}

/************************************************************************/
/*                     OGRGeoJSONReaderAddOrUpdateField()               */
/************************************************************************/

void OGRGeoJSONReaderAddOrUpdateField(
    OGRFeatureDefn* poDefn,
    const char* pszKey,
    json_object* poVal,
    bool bFlattenNestedAttributes,
    char chNestedAttributeSeparator,
    bool bArrayAsString,
    std::set<int>& aoSetUndeterminedTypeFields )
{
    if( bFlattenNestedAttributes &&
        poVal != NULL && json_object_get_type(poVal) == json_type_object )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poVal, it )
        {
            char szSeparator[2] = { chNestedAttributeSeparator, '\0' };

            CPLString osAttrName(CPLSPrintf("%s%s%s", pszKey, szSeparator,
                                            it.key));
            if( it.val != NULL &&
                json_object_get_type(it.val) == json_type_object )
            {
                OGRGeoJSONReaderAddOrUpdateField(poDefn, osAttrName, it.val,
                                                 true,
                                                 chNestedAttributeSeparator,
                                                 bArrayAsString,
                                                 aoSetUndeterminedTypeFields);
            }
            else
            {
                OGRGeoJSONReaderAddOrUpdateField(poDefn, osAttrName, it.val,
                                                 false, 0,
                                                 bArrayAsString,
                                                 aoSetUndeterminedTypeFields);
            }
        }
        return;
    }

    int nIndex = poDefn->GetFieldIndex(pszKey);
    if( nIndex < 0 )
    {
        OGRFieldSubType eSubType;
        const OGRFieldType eType =
            GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
        OGRFieldDefn fldDefn( pszKey, eType );
        fldDefn.SetSubType(eSubType);
        if( eSubType == OFSTBoolean )
            fldDefn.SetWidth(1);
        if( fldDefn.GetType() == OFTString )
        {
            fldDefn.SetType(GeoJSONStringPropertyToFieldType( poVal ));
        }
        poDefn->AddFieldDefn( &fldDefn );
        if( poVal == NULL )
            aoSetUndeterminedTypeFields.insert( poDefn->GetFieldCount() - 1 );
    }
    else if( poVal )
    {
        // If there is a null value: do not update field definition.
        OGRFieldDefn* poFDefn = poDefn->GetFieldDefn(nIndex);
        const OGRFieldType eType = poFDefn->GetType();
        if( aoSetUndeterminedTypeFields.find(nIndex) !=
            aoSetUndeterminedTypeFields.end() )
        {
            OGRFieldSubType eSubType;
            const OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            poFDefn->SetSubType(OFSTNone);
            poFDefn->SetType(eNewType);
            if( poFDefn->GetType() == OFTString )
            {
                poFDefn->SetType(GeoJSONStringPropertyToFieldType( poVal ));
            }
            poFDefn->SetSubType(eSubType);
            aoSetUndeterminedTypeFields.erase(nIndex);
        }
        else if( eType == OFTInteger )
        {
            OGRFieldSubType eSubType;
            const OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            if( eNewType == OFTInteger &&
                poFDefn->GetSubType() == OFSTBoolean &&
                eSubType != OFSTBoolean )
            {
                poFDefn->SetSubType(OFSTNone);
            }
            else if( eNewType == OFTInteger64 || eNewType == OFTReal ||
                     eNewType == OFTString ||
                     eNewType == OFTInteger64List || eNewType == OFTRealList ||
                     eNewType == OFTStringList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType == OFTIntegerList )
            {
                if( poFDefn->GetSubType() == OFSTBoolean &&
                    eSubType != OFSTBoolean )
                {
                    poFDefn->SetSubType(OFSTNone);
                }
                poFDefn->SetType(eNewType);
            }
            else if( eNewType != OFTInteger )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTInteger64 )
        {
            OGRFieldSubType eSubType;
            const OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            if( eNewType == OFTReal || eNewType == OFTString )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType == OFTIntegerList ||
                     eNewType == OFTInteger64List )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTInteger64List);
            }
            else if( eNewType == OFTRealList || eNewType == OFTStringList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType != OFTInteger && eNewType != OFTInteger64 )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTReal )
        {
            OGRFieldSubType eSubType;
            const OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            if(  eNewType == OFTIntegerList ||
                 eNewType == OFTInteger64List || eNewType == OFTRealList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTRealList);
            }
            else if( eNewType == OFTStringList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTStringList);
            }
            else if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
                     eNewType != OFTReal )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTString )
        {
            OGRFieldSubType eSubType;
            const OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            if( eNewType == OFTStringList )
                poFDefn->SetType(OFTStringList);
        }
        else if( eType == OFTIntegerList )
        {
            OGRFieldSubType eSubType;
            OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            if( eNewType == OFTInteger64List || eNewType == OFTRealList ||
                eNewType == OFTStringList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType == OFTInteger64 )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTInteger64List);
            }
            else if( eNewType == OFTReal )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTRealList);
            }
            else if( eNewType == OFTInteger || eNewType == OFTIntegerList )
            {
                if( poFDefn->GetSubType() == OFSTBoolean &&
                    eSubType != OFSTBoolean )
                {
                    poFDefn->SetSubType(OFSTNone);
                }
            }
            else if( eNewType != OFTInteger )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTInteger64List )
        {
            OGRFieldSubType eSubType;
            OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            if( eNewType == OFTInteger64List || eNewType == OFTRealList ||
                eNewType == OFTStringList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType == OFTReal )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTRealList);
            }
            else if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
                     eNewType != OFTIntegerList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTRealList )
        {
            OGRFieldSubType eSubType;
            const OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            if( eNewType == OFTStringList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(eNewType);
            }
            else if( eNewType != OFTInteger && eNewType != OFTInteger64 &&
                     eNewType != OFTReal && eNewType != OFTIntegerList &&
                     eNewType != OFTInteger64List && eNewType != OFTRealList )
            {
                poFDefn->SetSubType(OFSTNone);
                poFDefn->SetType(OFTString);
            }
        }
        else if( eType == OFTDate || eType == OFTTime || eType == OFTDateTime )
        {
            OGRFieldSubType eSubType;
            OGRFieldType eNewType =
                GeoJSONPropertyToFieldType( poVal, eSubType, bArrayAsString );
            if( eNewType == OFTString )
                eNewType = GeoJSONStringPropertyToFieldType( poVal );
            if( eType != eNewType )
            {
                poFDefn->SetSubType(OFSTNone);
                if( eType == OFTDate && eNewType == OFTDateTime )
                {
                    poFDefn->SetType(OFTDateTime);
                }
                else if( !(eType == OFTDateTime && eNewType == OFTDate) )
                {
                    poFDefn->SetType(OFTString);
                }
            }
        }

        poFDefn->SetWidth( poFDefn->GetSubType() == OFSTBoolean ? 1 : 0 );
    }
}

/************************************************************************/
/*                        GenerateFeatureDefn()                         */
/************************************************************************/
bool OGRGeoJSONReader::GenerateFeatureDefn( OGRGeoJSONLayer* poLayer,
                                            json_object* poObj )
{
    OGRFeatureDefn* poDefn = poLayer->GetLayerDefn();
    CPLAssert( NULL != poDefn );

/* -------------------------------------------------------------------- */
/*      Read collection of properties.                                  */
/* -------------------------------------------------------------------- */
    json_object* poObjProps = OGRGeoJSONFindMemberByName( poObj, "properties" );

    json_object* poObjId = OGRGeoJSONFindMemberByName( poObj, "id" );
    if( poObjId )
    {
        const int nIdx = poDefn->GetFieldIndex( "id" );
        if( nIdx < 0 )
        {
            if( json_object_get_type(poObjId) == json_type_int )
            {
                // If the value is negative, we cannot use it as the FID
                // as OGRMemLayer doesn't support negative FID. And we would
                // have an ambiguity with -1 that can mean OGRNullFID
                // so in that case create a regular attribute and let OGR
                // attribute sequential OGR FIDs.
                if( json_object_get_int64(poObjId) < 0 )
                {
                    bFoundFeatureId = false;
                }
                else
                {
                    bFoundFeatureId = true;
                }
            }
            if( !bFoundFeatureId )
            {
                // If there's a top-level id of type string or negative int,
                // and no properties.id, then declare a id field.
                bool bHasRegularIdProp = false;
                if( NULL != poObjProps &&
                    json_object_get_type(poObjProps) == json_type_object )
                {
                    bHasRegularIdProp =
                        CPL_json_object_object_get(poObjProps, "id") != NULL;
                }
                if( !bHasRegularIdProp )
                {
                    OGRFieldType eType = OFTString;
                    if( json_object_get_type(poObjId) == json_type_int )
                    {
                        if( CPL_INT64_FITS_ON_INT32(
                                json_object_get_int64(poObjId) ) )
                            eType = OFTInteger;
                        else
                            eType = OFTInteger64;
                    }
                    OGRFieldDefn fldDefn( "id", eType );
                    poDefn->AddFieldDefn(&fldDefn);
                }
            }
        }
        else if( json_object_get_type(poObjId) == json_type_int )
        {
            if( poDefn->GetFieldDefn(nIdx)->GetType() == OFTInteger )
            {
                if( !CPL_INT64_FITS_ON_INT32( json_object_get_int64(poObjId) ) )
                    poDefn->GetFieldDefn(nIdx)->SetType(OFTInteger64);
            }
        }
        else
        {
            poDefn->GetFieldDefn(nIdx)->SetType(OFTString);
        }
    }

    bool bSuccess = false;

    if( NULL != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
    {
        if( bIsGeocouchSpatiallistFormat )
        {
            poObjProps = CPL_json_object_object_get(poObjProps, "properties");
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
                // Detect the special kind of GeoJSON output by a spatiallist of
                // GeoCouch such as:
                // http://gd.iriscouch.com/cphosm/_design/geo/_rewrite/data?bbox=12.53%2C55.73%2C12.54%2C55.73
                if( strcmp(it.key, "_id") == 0 )
                {
                    bFoundId = true;
                }
                else if( bFoundId && strcmp(it.key, "_rev") == 0 )
                {
                    bFoundRev = true;
                }
                else if( bFoundRev && strcmp(it.key, "type") == 0 &&
                         it.val != NULL &&
                         json_object_get_type(it.val) == json_type_string &&
                         strcmp(json_object_get_string(it.val),
                                "Feature") == 0 )
                {
                    bFoundTypeFeature = true;
                }
                else if( bFoundTypeFeature &&
                         strcmp(it.key, "properties") == 0 &&
                         it.val != NULL &&
                         json_object_get_type(it.val) == json_type_object )
                {
                    if( bFlattenGeocouchSpatiallistFormat < 0 )
                        bFlattenGeocouchSpatiallistFormat = CPLTestBool(
                            CPLGetConfigOption("GEOJSON_FLATTEN_GEOCOUCH",
                                               "TRUE"));
                    if( bFlattenGeocouchSpatiallistFormat )
                    {
                        poDefn->DeleteFieldDefn(poDefn->GetFieldIndex("type"));
                        bIsGeocouchSpatiallistFormat = true;
                        return GenerateFeatureDefn(poLayer, poObj);
                    }
                }
            }

            OGRGeoJSONReaderAddOrUpdateField(poDefn, it.key, it.val,
                                             bFlattenNestedAttributes_,
                                             chNestedAttributeSeparator_,
                                             bArrayAsString_,
                                             aoSetUndeterminedTypeFields_);
        }

        bSuccess = true;  // SUCCESS
    }
    else if( poObj != NULL && json_object_get_type(poObj) == json_type_object )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poObj, it )
        {
            if( strcmp(it.key, "type") != 0 &&
                strcmp(it.key, "geometry") != 0 &&
                strcmp(it.key, "centroid") != 0 &&
                strcmp(it.key, "bbox") != 0 &&
                strcmp(it.key, "center") != 0 )
            {
                int nFldIndex = poDefn->GetFieldIndex( it.key );
                if( -1 == nFldIndex )
                {
                    OGRGeoJSONReaderAddOrUpdateField(poDefn, it.key, it.val,
                                                    bFlattenNestedAttributes_,
                                                    chNestedAttributeSeparator_,
                                                    bArrayAsString_,
                                                    aoSetUndeterminedTypeFields_);
                }
            }
        }

        bSuccess = true; // SUCCESS
        // CPLError(CE_Failure, CPLE_AppDefined,
        //          "Invalid Feature object. "
        //          "Missing \'properties\' member." );
    }

    return bSuccess;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRGeoJSONReader::AddFeature( OGRGeoJSONLayer* poLayer,
                                   OGRGeometry* poGeometry )
{
    bool bAdded = false;

    // TODO: Should we check if geometry is of type of wkbGeometryCollection?

    if( NULL != poGeometry )
    {
        OGRFeature* poFeature = NULL;
        poFeature = new OGRFeature( poLayer->GetLayerDefn() );
        poFeature->SetGeometryDirectly( poGeometry );

        bAdded = AddFeature( poLayer, poFeature );
    }

    return bAdded;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRGeoJSONReader::AddFeature( OGRGeoJSONLayer* poLayer,
                                   OGRFeature* poFeature )
{
    if( poFeature == NULL )
        return false;

    poLayer->AddFeature( poFeature );
    delete poFeature;

    return true;
}

/************************************************************************/
/*                           ReadGeometry                               */
/************************************************************************/

OGRGeometry* OGRGeoJSONReader::ReadGeometry( json_object* poObj,
                                             OGRSpatialReference* poLayerSRS )
{
    OGRGeometry* poGeometry = OGRGeoJSONReadGeometry( poObj, poLayerSRS );

/* -------------------------------------------------------------------- */
/*      Wrap geometry with GeometryCollection as a common denominator.  */
/*      Sometimes a GeoJSON text may consist of objects of different    */
/*      geometry types. Users may request wrapping all geometries with  */
/*      OGRGeometryCollection type by using option                      */
/*      GEOMETRY_AS_COLLECTION=NO|YES (NO is default).                  */
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
/*                OGRGeoJSONReaderSetFieldNestedAttribute()             */
/************************************************************************/

static void OGRGeoJSONReaderSetFieldNestedAttribute( OGRLayer* poLayer,
                                                     OGRFeature* poFeature,
                                                     const char* pszAttrPrefix,
                                                     char chSeparator,
                                                     json_object* poVal )
{
    json_object_iter it;
    it.key = NULL;
    it.val = NULL;
    it.entry = NULL;
    json_object_object_foreachC( poVal, it )
    {
        const char szSeparator[2] = { chSeparator, '\0' };
        const CPLString osAttrName(
            CPLSPrintf("%s%s%s", pszAttrPrefix, szSeparator, it.key));
        if( it.val != NULL && json_object_get_type(it.val) == json_type_object )
        {
            OGRGeoJSONReaderSetFieldNestedAttribute(poLayer, poFeature,
                                                    osAttrName, chSeparator,
                                                    it.val);
        }
        else
        {
            const int nField = poFeature->GetFieldIndex(osAttrName);
            OGRGeoJSONReaderSetField(poLayer, poFeature, nField,
                                     osAttrName, it.val, false, 0);
        }
    }
}

/************************************************************************/
/*                   OGRGeoJSONReaderSetField()                         */
/************************************************************************/

void OGRGeoJSONReaderSetField( OGRLayer* poLayer,
                               OGRFeature* poFeature,
                               int nField,
                               const char* pszAttrPrefix,
                               json_object* poVal,
                               bool bFlattenNestedAttributes,
                               char chNestedAttributeSeparator )
{
    if( bFlattenNestedAttributes &&
        poVal != NULL && json_object_get_type(poVal) == json_type_object )
    {
        OGRGeoJSONReaderSetFieldNestedAttribute(poLayer,
                                                poFeature,
                                                pszAttrPrefix,
                                                chNestedAttributeSeparator,
                                                poVal);
        return;
    }

    OGRFieldDefn* poFieldDefn = poFeature->GetFieldDefnRef(nField);
    CPLAssert( NULL != poFieldDefn );
    OGRFieldType eType = poFieldDefn->GetType();

    if( poVal == NULL)
    {
        poFeature->SetFieldNull( nField );
    }
    else if( OFTInteger == eType )
    {
        poFeature->SetField( nField, json_object_get_int(poVal) );

        // Check if FID available and set correct value.
        if( EQUAL( poFieldDefn->GetNameRef(), poLayer->GetFIDColumn() ) )
            poFeature->SetFID( json_object_get_int(poVal) );
    }
    else if( OFTInteger64 == eType )
    {
        poFeature->SetField( nField, (GIntBig)json_object_get_int64(poVal) );

        // Check if FID available and set correct value.
        if( EQUAL( poFieldDefn->GetNameRef(), poLayer->GetFIDColumn() ) )
            poFeature->SetFID(
                static_cast<GIntBig>(json_object_get_int64(poVal)));
    }
    else if( OFTReal == eType )
    {
        poFeature->SetField( nField, json_object_get_double(poVal) );
    }
    else if( OFTIntegerList == eType )
    {
        const enum json_type eJSonType(json_object_get_type(poVal));
        if( eJSonType == json_type_array )
        {
            const int nLength = json_object_array_length(poVal);
            int* panVal = static_cast<int *>(CPLMalloc(sizeof(int) * nLength));
            for( int i = 0; i < nLength; i++ )
            {
                json_object* poRow = json_object_array_get_idx(poVal, i);
                panVal[i] = json_object_get_int(poRow);
            }
            poFeature->SetField( nField, nLength, panVal );
            CPLFree(panVal);
        }
        else if ( eJSonType == json_type_boolean ||
                  eJSonType == json_type_int )
        {
            poFeature->SetField( nField, json_object_get_int(poVal) );
        }
    }
    else if( OFTInteger64List == eType )
    {
        const enum json_type eJSonType(json_object_get_type(poVal));
        if( eJSonType == json_type_array )
        {
            const int nLength = json_object_array_length(poVal);
            GIntBig* panVal =
                static_cast<GIntBig *>(CPLMalloc(sizeof(GIntBig) * nLength));
            for( int i = 0; i < nLength; i++ )
            {
                json_object* poRow = json_object_array_get_idx(poVal, i);
                panVal[i] = static_cast<GIntBig>(json_object_get_int64(poRow));
            }
            poFeature->SetField( nField, nLength, panVal );
            CPLFree(panVal);
        }
        else if ( eJSonType == json_type_boolean ||
                  eJSonType == json_type_int )
        {
            poFeature->SetField( nField,
                        static_cast<GIntBig>(json_object_get_int64(poVal)));
        }
    }
    else if( OFTRealList == eType )
    {
        const enum json_type eJSonType(json_object_get_type(poVal));
        if( eJSonType == json_type_array )
        {
            const int nLength = json_object_array_length(poVal);
            double* padfVal =
                static_cast<double *>(CPLMalloc(sizeof(double) * nLength));
            for( int i = 0; i < nLength;i++ )
            {
                json_object* poRow = json_object_array_get_idx(poVal, i);
                padfVal[i] = json_object_get_double(poRow);
            }
            poFeature->SetField( nField, nLength, padfVal );
            CPLFree(padfVal);
        }
        else if ( eJSonType == json_type_boolean ||
                  eJSonType == json_type_int || eJSonType == json_type_double )
        {
            poFeature->SetField( nField, json_object_get_double(poVal) );
        }
    }
    else if( OFTStringList == eType )
    {
        const enum json_type eJSonType(json_object_get_type(poVal));
        if( eJSonType == json_type_array )
        {
            const int nLength = json_object_array_length(poVal);
            char** papszVal = (char**)CPLMalloc(sizeof(char*) * (nLength+1));
            int i = 0;
            for( ; i < nLength; i++ )
            {
                json_object* poRow = json_object_array_get_idx(poVal, i);
                const char* pszVal = json_object_get_string(poRow);
                if( pszVal == NULL )
                    break;
                papszVal[i] = CPLStrdup(pszVal);
            }
            papszVal[i] = NULL;
            poFeature->SetField( nField, papszVal );
            CSLDestroy(papszVal);
        }
        else
        {
            poFeature->SetField( nField, json_object_get_string(poVal) );
        }
    }
    else
    {
        poFeature->SetField( nField, json_object_get_string(poVal) );
    }
}

/************************************************************************/
/*                           ReadFeature()                              */
/************************************************************************/

OGRFeature* OGRGeoJSONReader::ReadFeature( OGRGeoJSONLayer* poLayer,
                                           json_object* poObj )
{
    CPLAssert( NULL != poObj );

    OGRFeature* poFeature = NULL;
    poFeature = new OGRFeature( poLayer->GetLayerDefn() );

    if( bStoreNativeData_ )
    {
        poFeature->SetNativeData( json_object_to_json_string( poObj ) );
        poFeature->SetNativeMediaType( "application/vnd.geo+json" );
    }

/* -------------------------------------------------------------------- */
/*      Translate GeoJSON "properties" object to feature attributes.    */
/* -------------------------------------------------------------------- */
    CPLAssert( NULL != poFeature );

    json_object* poObjProps = OGRGeoJSONFindMemberByName( poObj, "properties" );
    if( !bAttributesSkip_ && NULL != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
    {
        if( bIsGeocouchSpatiallistFormat )
        {
            json_object* poId = CPL_json_object_object_get(poObjProps, "_id");
            if( poId != NULL && json_object_get_type(poId) == json_type_string )
                poFeature->SetField( "_id", json_object_get_string(poId) );

            json_object* poRev = CPL_json_object_object_get(poObjProps, "_rev");
            if( poRev != NULL &&
                json_object_get_type(poRev) == json_type_string )
            {
                poFeature->SetField( "_rev", json_object_get_string(poRev) );
            }

            poObjProps = CPL_json_object_object_get(poObjProps, "properties");
            if( NULL == poObjProps ||
                json_object_get_type(poObjProps) != json_type_object )
            {
                return poFeature;
            }
        }

        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poObjProps, it )
        {
            const int nField = poFeature->GetFieldIndex(it.key);
            OGRGeoJSONReaderSetField(poLayer, poFeature, nField, it.key, it.val,
                                     bFlattenNestedAttributes_,
                                     chNestedAttributeSeparator_);
        }
    }

    if( !bAttributesSkip_ && NULL == poObjProps )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        json_object_object_foreachC( poObj, it )
        {
            const int nFldIndex = poFeature->GetFieldIndex(it.key);
            if( nFldIndex >= 0 )
            {
                if( it.val )
                    poFeature->SetField(nFldIndex, json_object_get_string(it.val) );
                else
                    poFeature->SetFieldNull(nFldIndex );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to use feature-level ID if available                        */
/*      and of integral type. Otherwise, leave unset (-1) then index    */
/*      in features sequence will be used as FID.                       */
/* -------------------------------------------------------------------- */
    json_object* poObjId = OGRGeoJSONFindMemberByName( poObj, "id" );
    if( NULL != poObjId && bFoundFeatureId )
    {
      poFeature->SetFID(
          static_cast<GIntBig>(json_object_get_int64( poObjId )) );
    }

/* -------------------------------------------------------------------- */
/*      Handle the case where the special id is in a regular field.     */
/* -------------------------------------------------------------------- */
    else if( NULL != poObjId )
    {
        const int nIdx = poLayer->GetLayerDefn()->GetFieldIndex( "id" );
        if( nIdx >= 0 && !poFeature->IsFieldSet(nIdx) )
        {
            poFeature->SetField(nIdx, json_object_get_string(poObjId));
        }
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
        if( EQUAL( it.key, "geometry" ) )
        {
            if( it.val != NULL )
                poObjGeom = it.val;
            // Done.  They had 'geometry':null.
            else
                return poFeature;
        }
    }

    if( NULL != poObjGeom )
    {
        // NOTE: If geometry can not be parsed or read correctly
        //       then NULL geometry is assigned to a feature and
        //       geometry type for layer is classified as wkbUnknown.
        OGRGeometry* poGeometry = ReadGeometry( poObjGeom,
                                                poLayer->GetSpatialRef() );
        if( NULL != poGeometry )
        {
            poFeature->SetGeometryDirectly( poGeometry );
        }
    }
    else
    {
        static bool bWarned = false;
        if( !bWarned )
        {
            bWarned = true;
            CPLDebug(
                "GeoJSON",
                "Non conformant Feature object. Missing \'geometry\' member.");
        }
    }

    return poFeature;
}

/************************************************************************/
/*                           ReadFeatureCollection()                    */
/************************************************************************/

void
OGRGeoJSONReader::ReadFeatureCollection( OGRGeoJSONLayer* poLayer,
                                         json_object* poObj )
{
    json_object* poObjFeatures = OGRGeoJSONFindMemberByName(poObj, "features");
    if( NULL == poObjFeatures )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid FeatureCollection object. "
                  "Missing \'features\' member." );
        return;
    }

    if( json_type_array == json_object_get_type( poObjFeatures ) )
    {
        const int nFeatures = json_object_array_length( poObjFeatures );
        for( int i = 0; i < nFeatures; ++i )
        {
            json_object* poObjFeature
                = json_object_array_get_idx( poObjFeatures, i );
            OGRFeature* poFeature = ReadFeature( poLayer, poObjFeature );
            AddFeature( poLayer, poFeature );
        }
    }

    // Collect top objects except 'type' and the 'features' array.
    if( bStoreNativeData_ )
    {
        json_object_iter it;
        it.key = NULL;
        it.val = NULL;
        it.entry = NULL;
        CPLString osNativeData;
        json_object_object_foreachC(poObj, it)
        {
            if( strcmp(it.key, "type") == 0 ||
                strcmp(it.key, "features") == 0 )
            {
                continue;
            }
            if( osNativeData.empty() )
                osNativeData = "{ ";
            else
                osNativeData += ", ";
            json_object* poKey = json_object_new_string(it.key);
            osNativeData += json_object_to_json_string(poKey);
            json_object_put(poKey);
            osNativeData += ": ";
            osNativeData += json_object_to_json_string(it.val);
        }
        if( osNativeData.empty() )
        {
            osNativeData = "{ ";
        }
        osNativeData += " }";

        osNativeData = "NATIVE_DATA=" + osNativeData;

        char *apszMetadata[3] = {
            const_cast<char *>(osNativeData.c_str()),
            const_cast<char *>("NATIVE_MEDIA_TYPE=application/vnd.geo+json"),
            NULL
        };

        poLayer->SetMetadata( apszMetadata, "NATIVE_DATA" );
    }
}

/************************************************************************/
/*                           OGRGeoJSONFindMemberByName                 */
/************************************************************************/

lh_entry* OGRGeoJSONFindMemberEntryByName( json_object* poObj,
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
        it.entry = json_object_get_object(poTmp)->head;
        while( it.entry != NULL )
        {
            it.key = (char*)it.entry->k;
            it.val = (json_object*)it.entry->v;
            if( EQUAL( it.key, pszName ) )
                return it.entry;
            it.entry = it.entry->next;
        }
    }

    return NULL;
}

json_object* OGRGeoJSONFindMemberByName( json_object* poObj,
                                         const char* pszName )
{
    lh_entry* entry = OGRGeoJSONFindMemberEntryByName( poObj, pszName );
    if ( NULL == entry )
        return NULL;
    return (json_object*)entry->v;
}

/************************************************************************/
/*                           OGRGeoJSONGetType                          */
/************************************************************************/

GeoJSONObject::Type OGRGeoJSONGetType( json_object* poObj )
{
    if( NULL == poObj )
        return GeoJSONObject::eUnknown;

    json_object* poObjType = OGRGeoJSONFindMemberByName( poObj, "type" );
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
    return OGRGeoJSONReadGeometry(poObj, NULL);
}

OGRGeometry* OGRGeoJSONReadGeometry( json_object* poObj,
                                     OGRSpatialReference* poLayerSRS )
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
    // If we have a crs object in the current object, let's try and set it too.
    if( poGeometry != NULL )
    {
        lh_entry* entry = OGRGeoJSONFindMemberEntryByName( poObj, "crs" );
        if (entry != NULL ) {
            json_object* poObjSrs = (json_object*)entry->v;
            if( poObjSrs != NULL )
            {
                OGRSpatialReference* poSRS = OGRGeoJSONReadSpatialReference(poObj);
                if( poSRS != NULL )
                {
                    poGeometry->assignSpatialReference(poSRS);
                    poSRS->Release();
                }
            }
        }
        else if( poLayerSRS )
        {
            poGeometry->assignSpatialReference(poLayerSRS);
        }
        else
        {
            // Assign WGS84 if no CRS defined on geometry.
            poGeometry->assignSpatialReference(OGRSpatialReference::GetWGS84SRS());
        }
    }
    return poGeometry;
}

/************************************************************************/
/*                        OGRGeoJSONGetCoordinate()                     */
/************************************************************************/

static double OGRGeoJSONGetCoordinate( json_object* poObj,
                                       const char* pszCoordName,
                                       int nIndex,
                                       bool& bValid )
{
    json_object* poObjCoord = json_object_array_get_idx( poObj, nIndex );
    if( NULL == poObjCoord )
    {
        CPLDebug( "GeoJSON", "Point: got null object for %s.", pszCoordName );
        bValid = false;
        return 0.0;
    }

    const int iType = json_object_get_type(poObjCoord);
    if( json_type_double != iType && json_type_int != iType )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "Invalid '%s' coordinate. "
            "Type is not double or integer for \'%s\'.",
            pszCoordName,
            json_object_to_json_string(poObjCoord) );
        bValid = false;
        return 0.0;
    }

    return json_object_get_double( poObjCoord );
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

        if( nSize < GeoJSONObject::eMinCoordinateDimension )
        {
            CPLDebug( "GeoJSON",
                      "Invalid coord dimension. "
                      "At least 2 dimensions must be present." );
            return false;
        }

        bool bValid = true;
        const double dfX = OGRGeoJSONGetCoordinate(poObj, "x", 0, bValid);
        const double dfY = OGRGeoJSONGetCoordinate(poObj, "y", 1, bValid);
        point.setX(dfX);
        point.setY(dfY);

        // Read Z coordinate.
        if( nSize >= GeoJSONObject::eMaxCoordinateDimension )
        {
            // Don't *expect* mixed-dimension geometries, although the
            // spec doesn't explicitly forbid this.
            const double dfZ = OGRGeoJSONGetCoordinate(poObj, "z", 2, bValid);
            point.setZ(dfZ);
        }
        else
        {
            point.flattenTo2D();
        }
        return bValid;
    }

    return false;
}

/************************************************************************/
/*                           OGRGeoJSONReadPoint                        */
/************************************************************************/

OGRPoint* OGRGeoJSONReadPoint( json_object* poObj )
{
    CPLAssert( NULL != poObj );

    json_object* poObjCoords = OGRGeoJSONFindMemberByName(poObj, "coordinates");
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

    json_object* poObjPoints = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if( NULL == poObjPoints )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MultiPoint object. "
                  "Missing \'coordinates\' member." );
        return NULL;
    }

    OGRMultiPoint* poMultiPoint = NULL;
    if( json_type_array == json_object_get_type( poObjPoints ) )
    {
        const int nPoints = json_object_array_length( poObjPoints );

        poMultiPoint = new OGRMultiPoint();

        for( int i = 0; i < nPoints; ++i)
        {
            json_object* poObjCoords = NULL;
            poObjCoords = json_object_array_get_idx( poObjPoints, i );

            OGRPoint pt;
            if( poObjCoords != NULL &&
                !OGRGeoJSONReadRawPoint( poObjCoords, pt ) )
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

OGRLineString* OGRGeoJSONReadLineString( json_object* poObj , bool bRaw )
{
    CPLAssert( NULL != poObj );

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

    OGRLineString* poLine = NULL;

    if( json_type_array == json_object_get_type( poObjPoints ) )
    {
        const int nPoints = json_object_array_length( poObjPoints );

        poLine = new OGRLineString();
        poLine->setNumPoints( nPoints );

        for( int i = 0; i < nPoints; ++i)
        {
            json_object* poObjCoords =
                json_object_array_get_idx(poObjPoints, i);
            if( poObjCoords == NULL )
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
            if( pt.getCoordinateDimension() == 2 )
            {
                poLine->setPoint( i, pt.getX(), pt.getY());
            }
            else
            {
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

    json_object* poObjLines = OGRGeoJSONFindMemberByName(poObj, "coordinates");
    if( NULL == poObjLines )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MultiLineString object. "
                  "Missing \'coordinates\' member." );
        return NULL;
    }

    OGRMultiLineString* poMultiLine = NULL;

    if( json_type_array == json_object_get_type( poObjLines ) )
    {
        const int nLines = json_object_array_length( poObjLines );

        poMultiLine = new OGRMultiLineString();

        for( int i = 0; i < nLines; ++i)
        {
            json_object* poObjLine = NULL;
            poObjLine = json_object_array_get_idx( poObjLines, i );

            OGRLineString* poLine = NULL;
            if( poObjLine != NULL )
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
            json_object* poObjCoords = json_object_array_get_idx( poObj, i );
            if( poObjCoords == NULL )
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

    OGRPolygon* poPolygon = NULL;

    if( json_type_array == json_object_get_type( poObjRings ) )
    {
        const int nRings = json_object_array_length( poObjRings );
        if( nRings > 0 )
        {
            json_object* poObjPoints = json_object_array_get_idx(poObjRings, 0);
            if( poObjPoints == NULL )
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
                if( poObjPoints == NULL )
                {
                    poPolygon->addRingDirectly( new OGRLinearRing() );
                }
                else
                {
                    OGRLinearRing* poRing =
                        OGRGeoJSONReadLinearRing( poObjPoints );
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

    json_object* poObjPolys = NULL;
    poObjPolys = OGRGeoJSONFindMemberByName( poObj, "coordinates" );
    if( NULL == poObjPolys )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MultiPolygon object. "
                  "Missing \'coordinates\' member." );
        return NULL;
    }

    OGRMultiPolygon* poMultiPoly = NULL;

    if( json_type_array == json_object_get_type( poObjPolys ) )
    {
        const int nPolys = json_object_array_length( poObjPolys );

        poMultiPoly = new OGRMultiPolygon();

        for( int i = 0; i < nPolys; ++i)
        {
            json_object* poObjPoly = json_object_array_get_idx( poObjPolys, i );
            if( poObjPoly == NULL )
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

    json_object* poObjGeoms = OGRGeoJSONFindMemberByName( poObj, "geometries" );
    if( NULL == poObjGeoms )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid GeometryCollection object. "
                  "Missing \'geometries\' member." );
        return NULL;
    }

    OGRGeometryCollection* poCollection = NULL;

    if( json_type_array == json_object_get_type( poObjGeoms ) )
    {
        const int nGeoms = json_object_array_length( poObjGeoms );
        if( nGeoms > 0 )
        {
            poCollection = new OGRGeometryCollection();
        }

        for( int i = 0; i < nGeoms; ++i )
        {
            json_object* poObjGeom = json_object_array_get_idx( poObjGeoms, i );
            if( poObjGeom == NULL )
            {
                CPLDebug( "GeoJSON", "Skipping null sub-geometry");
                continue;
            }

            OGRGeometry* poGeometry = OGRGeoJSONReadGeometry( poObjGeom );
            if( NULL != poGeometry )
            {
                poCollection->addGeometryDirectly( poGeometry );
            }
        }
    }

    return poCollection;
}

/************************************************************************/
/*                       OGR_G_CreateGeometryFromJson                   */
/************************************************************************/

/** Create a OGR geometry from a GeoJSON geometry object */
OGRGeometryH OGR_G_CreateGeometryFromJson( const char* pszJson )
{
    if( NULL == pszJson )
    {
        // Translation failed.
        return NULL;
    }

    json_object *poObj = NULL;
    if( !OGRJSonParse(pszJson, &poObj) )
        return NULL;

    OGRGeometry* poGeometry = OGRGeoJSONReadGeometry( poObj );

    // Release JSON tree.
    json_object_put( poObj );

    return (OGRGeometryH)poGeometry;
}

/************************************************************************/
/*                       json_ex_get_object_by_path()                   */
/************************************************************************/

json_object* json_ex_get_object_by_path( json_object* poObj,
                                         const char* pszPath )
{
    if( poObj == NULL || json_object_get_type(poObj) != json_type_object ||
        pszPath == NULL || *pszPath == '\0' )
    {
        return poObj;
    }
    char** papszTokens = CSLTokenizeString2( pszPath, ".", 0 );
    for( int i = 0; papszTokens[i] != NULL; i++ )
    {
        poObj = CPL_json_object_object_get(poObj, papszTokens[i]);
        if( poObj == NULL )
            break;
        if( papszTokens[i+1] != NULL )
        {
            if( json_object_get_type(poObj) != json_type_object )
            {
                poObj = NULL;
                break;
            }
        }
    }
    CSLDestroy(papszTokens);
    return poObj;
}

/************************************************************************/
/*                             OGRJSonParse()                           */
/************************************************************************/

bool OGRJSonParse( const char* pszText, json_object** ppoObj,
                   bool bVerboseError )
{
    if( ppoObj == NULL )
        return false;
    json_tokener* jstok = json_tokener_new();
    const int nLen = pszText == NULL ? 0 : static_cast<int>(strlen(pszText));
    *ppoObj = json_tokener_parse_ex(jstok, pszText, nLen);
    if( jstok->err != json_tokener_success)
    {
        if( bVerboseError )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "JSON parsing error: %s (at offset %d)",
                     json_tokener_error_desc(jstok->err), jstok->char_offset);
        }

        json_tokener_free(jstok);
        *ppoObj = NULL;
        return false;
    }
    json_tokener_free(jstok);
    return true;
}

/************************************************************************/
/*                    CPL_json_object_object_get()                      */
/************************************************************************/

// This is the same as json_object_object_get() except it will not raise
// deprecation warning.

json_object*  CPL_json_object_object_get(struct json_object* obj,
                                         const char *key)
{
    json_object* poRet = NULL;
    json_object_object_get_ex(obj, key, &poRet);
    return poRet;
}
