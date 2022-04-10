/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of OGRESRIJSONReader class (OGR ESRIJSON Driver)
 *           to read ESRI Feature Service REST data
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogrgeojsonreader.h"

#include <limits.h>
#include <stddef.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "json.h"
// #include "json_object.h"
// #include "json_tokener.h"
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_spatialref.h"
#include "ogr_geojson.h"
#include "ogrgeojsonreader.h"
#include "ogrgeojsonutils.h"
// #include "symbol_renames.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRESRIJSONReader()                         */
/************************************************************************/

OGRESRIJSONReader::OGRESRIJSONReader() :
    poGJObject_(nullptr),
    poLayer_(nullptr)
{}

/************************************************************************/
/*                         ~OGRESRIJSONReader()                         */
/************************************************************************/

OGRESRIJSONReader::~OGRESRIJSONReader()
{
    if( nullptr != poGJObject_ )
    {
        json_object_put(poGJObject_);
    }

    poGJObject_ = nullptr;
    poLayer_ = nullptr;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

OGRErr OGRESRIJSONReader::Parse( const char* pszText )
{
    json_object *jsobj = nullptr;
    if( nullptr != pszText && !OGRJSonParse(pszText, &jsobj, true) )
    {
        return OGRERR_CORRUPT_DATA;
    }

    // JSON tree is shared for while lifetime of the reader object
    // and will be released in the destructor.
    poGJObject_ = jsobj;
    return OGRERR_NONE;
}

/************************************************************************/
/*                           ReadLayers()                               */
/************************************************************************/

void OGRESRIJSONReader::ReadLayers( OGRGeoJSONDataSource* poDS,
                                    GeoJSONSourceType eSourceType )
{
    CPLAssert( nullptr == poLayer_ );

    if( nullptr == poGJObject_ )
    {
        CPLDebug( "ESRIJSON",
                  "Missing parsed ESRIJSON data. Forgot to call Parse()?" );
        return;
    }

    OGRSpatialReference* poSRS = OGRESRIJSONReadSpatialReference( poGJObject_ );

    const char* pszName = "ESRIJSON";
    if( eSourceType == eGeoJSONSourceFile )
    {
        pszName = poDS->GetDescription();
        if( STARTS_WITH_CI(pszName, "ESRIJSON:") )
            pszName += strlen("ESRIJSON:");
        pszName = CPLGetBasename(pszName);
    }

    auto eGeomType = OGRESRIJSONGetGeometryType(poGJObject_);
    if( eGeomType == wkbNone && poSRS != nullptr )
    {
        eGeomType = wkbUnknown;
    }

    poLayer_ = new OGRGeoJSONLayer( pszName, poSRS,
                                    eGeomType,
                                    poDS, nullptr );
    if( poSRS != nullptr )
        poSRS->Release();

    if( !GenerateLayerDefn() )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer schema generation failed." );

        delete poLayer_;
        return;
    }

    OGRGeoJSONLayer *poThisLayer = ReadFeatureCollection( poGJObject_ );
    if( poThisLayer == nullptr )
    {
        delete poLayer_;
        return;
    }

    CPLErrorReset();

    poLayer_->DetectGeometryType();
    poDS->AddLayer(poLayer_);
}

/************************************************************************/
/*                        GenerateFeatureDefn()                         */
/************************************************************************/

bool OGRESRIJSONReader::GenerateLayerDefn()
{
    CPLAssert( nullptr != poGJObject_ );
    CPLAssert( nullptr != poLayer_->GetLayerDefn() );
    CPLAssert( 0 == poLayer_->GetLayerDefn()->GetFieldCount() );

    bool bSuccess = true;

/* -------------------------------------------------------------------- */
/*      Scan all features and generate layer definition.                */
/* -------------------------------------------------------------------- */
    json_object* poFields =
        OGRGeoJSONFindMemberByName( poGJObject_, "fields" );
    if( nullptr != poFields &&
        json_type_array == json_object_get_type( poFields ) )
    {
        const auto nFeatures = json_object_array_length( poFields );
        for( auto i = decltype(nFeatures){0}; i < nFeatures; ++i )
        {
            json_object* poField =
                json_object_array_get_idx( poFields, i );
            if( !ParseField( poField ) )
            {
                CPLDebug( "GeoJSON", "Create feature schema failure." );
                bSuccess = false;
            }
        }
    }
    else
    {
        poFields = OGRGeoJSONFindMemberByName(
            poGJObject_, "fieldAliases" );
        if( nullptr != poFields &&
            json_object_get_type(poFields) == json_type_object )
        {
            OGRFeatureDefn* poDefn = poLayer_->GetLayerDefn();
            json_object_iter it;
            it.key = nullptr;
            it.val = nullptr;
            it.entry = nullptr;
            json_object_object_foreachC( poFields, it )
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
/*                             ParseField()                             */
/************************************************************************/

bool OGRESRIJSONReader::ParseField( json_object* poObj )
{
    OGRFeatureDefn* poDefn = poLayer_->GetLayerDefn();
    CPLAssert( nullptr != poDefn );

    bool bSuccess = false;

/* -------------------------------------------------------------------- */
/*      Read collection of properties.                                  */
/* -------------------------------------------------------------------- */
    json_object* poObjName = OGRGeoJSONFindMemberByName( poObj, "name" );
    json_object* poObjType = OGRGeoJSONFindMemberByName( poObj, "type" );
    if( nullptr != poObjName && nullptr != poObjType )
    {
        OGRFieldType eFieldType = OFTString;
        if( EQUAL(json_object_get_string(poObjType), "esriFieldTypeOID") )
        {
            eFieldType = OFTInteger;
            poLayer_->SetFIDColumn(json_object_get_string(poObjName));
        }
        else if( EQUAL(json_object_get_string(poObjType),
                       "esriFieldTypeDouble") )
        {
            eFieldType = OFTReal;
        }
        else if( EQUAL(json_object_get_string(poObjType),
                       "esriFieldTypeSmallInteger") ||
                 EQUAL(json_object_get_string(poObjType),
                       "esriFieldTypeInteger") )
        {
            eFieldType = OFTInteger;
        }
        OGRFieldDefn fldDefn( json_object_get_string(poObjName),
                              eFieldType );

        json_object * const poObjLength =
            OGRGeoJSONFindMemberByName( poObj, "length" );
        if( poObjLength != nullptr &&
            json_object_get_type(poObjLength) == json_type_int )
        {
            const int nWidth = json_object_get_int(poObjLength);
            // A dummy width of 2147483647 seems to indicate no known field with
            // which in the OGR world is better modelled as 0 field width.
            // (#6529)
            if( nWidth != INT_MAX )
                fldDefn.SetWidth(nWidth);
        }

        poDefn->AddFieldDefn( &fldDefn );

        bSuccess = true;
    }
    return bSuccess;
}

/************************************************************************/
/*                           AddFeature                                 */
/************************************************************************/

bool OGRESRIJSONReader::AddFeature( OGRFeature* poFeature )
{
    if( nullptr == poFeature )
        return false;

    poLayer_->AddFeature( poFeature );
    delete poFeature;

    return true;
}

/************************************************************************/
/*                       OGRESRIJSONReadGeometry()                      */
/************************************************************************/

OGRGeometry* OGRESRIJSONReadGeometry( json_object* poObj )
{
    OGRGeometry* poGeometry = nullptr;

    if( OGRGeoJSONFindMemberByName(poObj, "x") )
        poGeometry = OGRESRIJSONReadPoint( poObj );
    else if( OGRGeoJSONFindMemberByName(poObj, "paths") )
        poGeometry = OGRESRIJSONReadLineString( poObj );
    else if( OGRGeoJSONFindMemberByName(poObj, "rings") )
        poGeometry = OGRESRIJSONReadPolygon( poObj );
    else if( OGRGeoJSONFindMemberByName(poObj, "points") )
        poGeometry = OGRESRIJSONReadMultiPoint( poObj );

    return poGeometry;
}


/************************************************************************/
/*                     OGR_G_CreateGeometryFromEsriJson()               */
/************************************************************************/

/** Create a OGR geometry from a ESRIJson geometry object */
OGRGeometryH OGR_G_CreateGeometryFromEsriJson( const char* pszJson )
{
    if( nullptr == pszJson )
    {
        // Translation failed.
        return nullptr;
    }

    json_object *poObj = nullptr;
    if( !OGRJSonParse(pszJson, &poObj) )
        return nullptr;

    OGRGeometry* poGeometry = OGRESRIJSONReadGeometry( poObj );

    // Release JSON tree.
    json_object_put( poObj );

    return OGRGeometry::ToHandle(poGeometry);
}


/************************************************************************/
/*                           ReadFeature()                              */
/************************************************************************/

OGRFeature* OGRESRIJSONReader::ReadFeature( json_object* poObj )
{
    CPLAssert( nullptr != poObj );
    CPLAssert( nullptr != poLayer_ );

    OGRFeature* poFeature = new OGRFeature( poLayer_->GetLayerDefn() );

/* -------------------------------------------------------------------- */
/*      Translate ESRIJSON "attributes" object to feature attributes.   */
/* -------------------------------------------------------------------- */
    CPLAssert( nullptr != poFeature );

    json_object* poObjProps = OGRGeoJSONFindMemberByName( poObj, "attributes" );
    if( nullptr != poObjProps &&
        json_object_get_type(poObjProps) == json_type_object )
    {
        OGRFieldDefn* poFieldDefn = nullptr;
        json_object_iter it;
        it.key = nullptr;
        it.val = nullptr;
        it.entry = nullptr;
        json_object_object_foreachC( poObjProps, it )
        {
            const int nField
                = poFeature->GetFieldIndex(it.key);
            if( nField >= 0 )
            {
                poFieldDefn = poFeature->GetFieldDefnRef(nField);
                if( poFieldDefn && it.val != nullptr )
                {
                    if( EQUAL( it.key,  poLayer_->GetFIDColumn() ) )
                        poFeature->SetFID( json_object_get_int( it.val ) );
                    if( poLayer_->GetLayerDefn()->
                            GetFieldDefn(nField)->GetType() == OFTReal )
                    {
                        poFeature->SetField(
                            nField, CPLAtofM(json_object_get_string(it.val)) );
                    }
                    else
                    {
                        poFeature->SetField( nField,
                                             json_object_get_string(it.val) );
                    }
                }
            }
        }
    }

    const OGRwkbGeometryType eType = poLayer_->GetGeomType();
    if( eType == wkbNone )
        return poFeature;

/* -------------------------------------------------------------------- */
/*      Translate geometry sub-object of ESRIJSON Feature.               */
/* -------------------------------------------------------------------- */
    json_object* poObjGeom = nullptr;
    json_object* poTmp = poObj;
    json_object_iter it;
    it.key = nullptr;
    it.val = nullptr;
    it.entry = nullptr;
    json_object_object_foreachC(poTmp, it)
    {
        if( EQUAL( it.key, "geometry" ) )
        {
            if( it.val != nullptr )
                poObjGeom = it.val;
            // We're done.  They had 'geometry':null.
            else
                return poFeature;
        }
    }

    if( nullptr != poObjGeom )
    {
        OGRGeometry* poGeometry = OGRESRIJSONReadGeometry( poObjGeom );
        if( nullptr != poGeometry )
        {
            poFeature->SetGeometryDirectly( poGeometry );
        }
    }

    return poFeature;
}

/************************************************************************/
/*                           ReadFeatureCollection()                    */
/************************************************************************/

OGRGeoJSONLayer*
OGRESRIJSONReader::ReadFeatureCollection( json_object* poObj )
{
    CPLAssert( nullptr != poLayer_ );

    json_object* poObjFeatures
        = OGRGeoJSONFindMemberByName( poObj, "features" );
    if( nullptr == poObjFeatures )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid FeatureCollection object. "
                  "Missing \'features\' member." );
        return nullptr;
    }

    if( json_type_array == json_object_get_type( poObjFeatures ) )
    {
        const auto nFeatures = json_object_array_length( poObjFeatures );
        for( auto i = decltype(nFeatures){0}; i < nFeatures; ++i )
        {
            json_object* poObjFeature
                = json_object_array_get_idx( poObjFeatures, i );
            if( poObjFeature != nullptr &&
                json_object_get_type(poObjFeature) == json_type_object )
            {
                OGRFeature* poFeature =
                    OGRESRIJSONReader::ReadFeature( poObjFeature );
                AddFeature( poFeature );
            }
        }
    }

    // We're returning class member to follow the same pattern of
    // Read* functions call convention.
    CPLAssert( nullptr != poLayer_ );
    return poLayer_;
}

/************************************************************************/
/*                        OGRESRIJSONGetType()                          */
/************************************************************************/

OGRwkbGeometryType OGRESRIJSONGetGeometryType( json_object* poObj )
{
    if( nullptr == poObj )
        return wkbUnknown;

    json_object* poObjType =
        OGRGeoJSONFindMemberByName( poObj, "geometryType" );
    if( nullptr == poObjType )
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
/*                     OGRESRIJSONGetCoordinateToDouble()               */
/************************************************************************/

static double OGRESRIJSONGetCoordinateToDouble( json_object* poObjCoord,
                                                const char* pszCoordName,
                                                bool& bValid )
{
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
/*                       OGRESRIJSONGetCoordinate()                     */
/************************************************************************/

static double OGRESRIJSONGetCoordinate( json_object* poObj,
                                        const char* pszCoordName,
                                        bool& bValid )
{
    json_object* poObjCoord = OGRGeoJSONFindMemberByName( poObj, pszCoordName );
    if( nullptr == poObjCoord )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid Point object. "
            "Missing '%s' member.", pszCoordName );
        bValid = false;
        return 0.0;
    }

    return OGRESRIJSONGetCoordinateToDouble( poObjCoord, pszCoordName, bValid );
}

/************************************************************************/
/*                          OGRESRIJSONReadPoint()                      */
/************************************************************************/

OGRPoint* OGRESRIJSONReadPoint( json_object* poObj)
{
    CPLAssert( nullptr != poObj );

    bool bValid = true;
    const double dfX = OGRESRIJSONGetCoordinate(poObj, "x", bValid);
    const double dfY = OGRESRIJSONGetCoordinate(poObj, "y", bValid);
    if( !bValid )
        return nullptr;

    json_object* poObjZ = OGRGeoJSONFindMemberByName( poObj, "z" );
    if( nullptr == poObjZ )
        return new OGRPoint(dfX, dfY);

    const double dfZ = OGRESRIJSONGetCoordinateToDouble(poObjZ, "z", bValid);
    if( !bValid )
        return nullptr;
    return new OGRPoint(dfX, dfY, dfZ);
}

/************************************************************************/
/*                     OGRESRIJSONReaderParseZM()                  */
/************************************************************************/

static bool OGRESRIJSONReaderParseZM( json_object* poObj, bool *bHasZ,
                                      bool *bHasM )
{
    CPLAssert( nullptr != poObj );
    // The ESRI geojson spec states that geometries other than point can
    // have the attributes hasZ and hasM.  A geometry that has a z value
    // implies the 3rd number in the tuple is z.  if hasM is true, but hasZ
    // is not, it is the M value.
    bool bZ = false;
    json_object* poObjHasZ = OGRGeoJSONFindMemberByName( poObj, "hasZ" );
    if( poObjHasZ != nullptr )
    {
        if( json_object_get_type( poObjHasZ ) == json_type_boolean )
        {
            bZ = CPL_TO_BOOL(json_object_get_boolean( poObjHasZ ));
        }
    }

    bool bM = false;
    json_object* poObjHasM = OGRGeoJSONFindMemberByName( poObj, "hasM" );
    if( poObjHasM != nullptr )
    {
        if( json_object_get_type( poObjHasM ) == json_type_boolean )
        {
            bM = CPL_TO_BOOL(json_object_get_boolean( poObjHasM ));
        }
    }
    if( bHasZ != nullptr )
        *bHasZ = bZ;
    if( bHasM != nullptr )
        *bHasM = bM;
    return true;
}

/************************************************************************/
/*                     OGRESRIJSONReaderParseXYZMArray()                  */
/************************************************************************/

static bool OGRESRIJSONReaderParseXYZMArray( json_object* poObjCoords,
                                             bool /*bHasZ*/, bool bHasM,
                                             double* pdfX, double* pdfY,
                                             double* pdfZ, double* pdfM,
                                             int* pnNumCoords )
{
    if( poObjCoords == nullptr )
    {
        CPLDebug( "ESRIJSON",
                  "OGRESRIJSONReaderParseXYZMArray: got null object." );
        return false;
    }

    if( json_type_array != json_object_get_type( poObjCoords ))
    {
        CPLDebug( "ESRIJSON",
                  "OGRESRIJSONReaderParseXYZMArray: got non-array object." );
        return false;
    }

    const auto coordDimension = json_object_array_length( poObjCoords );

    // Allow 4 coordinates if M is present, but it is eventually ignored.
    if( coordDimension < 2 || coordDimension > 4 )
    {
        CPLDebug( "ESRIJSON",
                  "OGRESRIJSONReaderParseXYZMArray: got an unexpected "
                  "array object." );
        return false;
    }

    // Read X coordinate.
    json_object* poObjCoord = json_object_array_get_idx( poObjCoords, 0 );
    if( poObjCoord == nullptr )
    {
        CPLDebug( "ESRIJSON",
                  "OGRESRIJSONReaderParseXYZMArray: got null object." );
        return false;
    }

    bool bValid = true;
    const double dfX = OGRESRIJSONGetCoordinateToDouble(poObjCoord, "x", bValid);

    // Read Y coordinate.
    poObjCoord = json_object_array_get_idx( poObjCoords, 1 );
    if( poObjCoord == nullptr )
    {
        CPLDebug( "ESRIJSON",
                  "OGRESRIJSONReaderParseXYZMArray: got null object." );
        return false;
    }

    const double dfY = OGRESRIJSONGetCoordinateToDouble(poObjCoord, "y", bValid);
    if( !bValid )
        return false;

    // Read Z or M or Z and M coordinates.
    if( coordDimension > 2)
    {
        poObjCoord = json_object_array_get_idx( poObjCoords, 2 );
        if( poObjCoord == nullptr )
        {
            CPLDebug( "ESRIJSON",
                      "OGRESRIJSONReaderParseXYZMArray: got null object." );
            return false;
        }

        const double dfZorM = OGRESRIJSONGetCoordinateToDouble(poObjCoord,
                        (coordDimension > 3 || !bHasM) ? "z": "m", bValid);
        if( !bValid )
            return false;
        if( pdfZ != nullptr )
        {
            if (coordDimension > 3 || !bHasM)
                *pdfZ = dfZorM;
            else
                *pdfZ = 0.0;
        }
        if( pdfM != nullptr && coordDimension == 3 )
        {
            if (bHasM)
                *pdfM = dfZorM;
            else
                *pdfM = 0.0;
        }
        if( coordDimension == 4 )
        {
            poObjCoord = json_object_array_get_idx( poObjCoords, 3 );
            if( poObjCoord == nullptr )
            {
                CPLDebug( "ESRIJSON",
                        "OGRESRIJSONReaderParseXYZMArray: got null object." );
                return false;
            }

            const double dfM = OGRESRIJSONGetCoordinateToDouble(poObjCoord,
                                                                "m", bValid);
            if( !bValid )
                return false;
            if( pdfM != nullptr )
                *pdfM = dfM;
        }
    }
    else
    {
        if( pdfZ != nullptr )
            *pdfZ = 0.0;
        if( pdfM != nullptr )
            *pdfM = 0.0;
    }

    if( pnNumCoords != nullptr )
        *pnNumCoords = static_cast<int>(coordDimension);
    if( pdfX != nullptr )
        *pdfX = dfX;
    if( pdfY != nullptr )
        *pdfY = dfY;

    return true;
}

/************************************************************************/
/*                        OGRESRIJSONReadLineString()                   */
/************************************************************************/

OGRGeometry* OGRESRIJSONReadLineString( json_object* poObj )
{
    CPLAssert( nullptr != poObj );

    bool bHasZ = false;
    bool bHasM = false;

    if( !OGRESRIJSONReaderParseZM( poObj, &bHasZ, &bHasM ) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Failed to parse hasZ and/or hasM from geometry" );
    }

    json_object* poObjPaths = OGRGeoJSONFindMemberByName( poObj, "paths" );
    if( nullptr == poObjPaths )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid LineString object. "
                  "Missing \'paths\' member." );
        return nullptr;
    }

    if( json_type_array != json_object_get_type( poObjPaths ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid LineString object. "
                  "Invalid \'paths\' member." );
        return nullptr;
    }

    OGRMultiLineString* poMLS = nullptr;
    OGRGeometry* poRet = nullptr;
    const auto nPaths = json_object_array_length( poObjPaths );
    for( auto iPath = decltype(nPaths){0}; iPath < nPaths; iPath++ )
    {
        json_object* poObjPath = json_object_array_get_idx( poObjPaths, iPath );
        if( poObjPath == nullptr ||
            json_type_array != json_object_get_type( poObjPath ) )
        {
            delete poRet;
            CPLDebug( "ESRIJSON", "LineString: got non-array object." );
            return nullptr;
        }

        OGRLineString* poLine = new OGRLineString();
        if( nPaths > 1 )
        {
            if( iPath == 0 )
            {
                poMLS = new OGRMultiLineString();
                poRet = poMLS;
            }
            poMLS->addGeometryDirectly(poLine);
        }
        else
        {
            poRet = poLine;
        }
        const auto nPoints = json_object_array_length( poObjPath );
        for( auto i = decltype(nPoints){0}; i < nPoints; i++ )
        {
            int nNumCoords = 2;
            json_object* poObjCoords =
                json_object_array_get_idx( poObjPath, i );
            double dfX = 0.0;
            double dfY = 0.0;
            double dfZ = 0.0;
            double dfM = 0.0;
            if( !OGRESRIJSONReaderParseXYZMArray (
              poObjCoords, bHasZ, bHasM, &dfX, &dfY, &dfZ, &dfM, &nNumCoords) )
            {
                delete poRet;
                return nullptr;
            }

            if( nNumCoords == 3 && !bHasM )
            {
                poLine->addPoint( dfX, dfY, dfZ);
            }
            else if( nNumCoords == 3 )
            {
                poLine->addPointM( dfX, dfY, dfM);
            }
            else if( nNumCoords == 4 )
            {
                poLine->addPoint( dfX, dfY, dfZ, dfM);
            }
            else
            {
                poLine->addPoint( dfX, dfY );
            }
        }
    }

    if( poRet == nullptr )
        poRet = new OGRLineString();

    return poRet;
}

/************************************************************************/
/*                          OGRESRIJSONReadPolygon()                    */
/************************************************************************/

OGRGeometry* OGRESRIJSONReadPolygon( json_object* poObj)
{
    CPLAssert( nullptr != poObj );

    bool bHasZ = false;
    bool bHasM = false;

    if( !OGRESRIJSONReaderParseZM( poObj, &bHasZ, &bHasM ) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Failed to parse hasZ and/or hasM from geometry" );
    }

    json_object* poObjRings = OGRGeoJSONFindMemberByName( poObj, "rings" );
    if( nullptr == poObjRings )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid Polygon object. "
            "Missing \'rings\' member." );
        return nullptr;
    }

    if( json_type_array != json_object_get_type( poObjRings ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Invalid Polygon object. "
            "Invalid \'rings\' member." );
        return nullptr;
    }

    const auto nRings = json_object_array_length( poObjRings );
    OGRGeometry** papoGeoms = new OGRGeometry*[nRings];
    for( auto iRing = decltype(nRings){0}; iRing < nRings; iRing++ )
    {
        json_object* poObjRing = json_object_array_get_idx( poObjRings, iRing );
        if( poObjRing == nullptr ||
            json_type_array != json_object_get_type( poObjRing ) )
        {
            for( auto j = decltype(iRing){0}; j < iRing; j++ )
                delete papoGeoms[j];
            delete[] papoGeoms;
            CPLDebug( "ESRIJSON",
                    "Polygon: got non-array object." );
            return nullptr;
        }

        OGRPolygon* poPoly = new OGRPolygon();
        auto poLine = cpl::make_unique<OGRLinearRing>();
        papoGeoms[iRing] = poPoly;

        const auto nPoints = json_object_array_length( poObjRing );
        for( auto i = decltype(nPoints){0}; i < nPoints; i++ )
        {
            int nNumCoords = 2;
            json_object* poObjCoords =
                json_object_array_get_idx( poObjRing, i );
            double dfX = 0.0;
            double dfY = 0.0;
            double dfZ = 0.0;
            double dfM = 0.0;
            if( !OGRESRIJSONReaderParseXYZMArray (
              poObjCoords, bHasZ, bHasM, &dfX, &dfY, &dfZ, &dfM, &nNumCoords) )
            {
                for( auto j = decltype(iRing){0}; j <= iRing; j++ )
                    delete papoGeoms[j];
                delete[] papoGeoms;
                return nullptr;
            }

            if( nNumCoords == 3 && !bHasM )
            {
                poLine->addPoint( dfX, dfY, dfZ);
            }
            else if( nNumCoords == 3 )
            {
                poLine->addPointM( dfX, dfY, dfM);
            }
            else if( nNumCoords == 4 )
            {
                poLine->addPoint( dfX, dfY, dfZ, dfM);
            }
            else
            {
                poLine->addPoint( dfX, dfY );
            }
        }
        poPoly->addRingDirectly(poLine.release());
    }

    OGRGeometry* poRet = OGRGeometryFactory::organizePolygons( papoGeoms,
                                                               static_cast<int>(nRings),
                                                               nullptr,
                                                               nullptr);
    delete[] papoGeoms;

    return poRet;
}

/************************************************************************/
/*                        OGRESRIJSONReadMultiPoint()                   */
/************************************************************************/

OGRMultiPoint* OGRESRIJSONReadMultiPoint( json_object* poObj)
{
    CPLAssert( nullptr != poObj );

    bool bHasZ = false;
    bool bHasM = false;

    if( !OGRESRIJSONReaderParseZM( poObj, &bHasZ, &bHasM ) )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Failed to parse hasZ and/or hasM from geometry" );
    }

    json_object* poObjPoints = OGRGeoJSONFindMemberByName( poObj, "points" );
    if( nullptr == poObjPoints )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MultiPoint object. "
                  "Missing \'points\' member." );
        return nullptr;
    }

    if( json_type_array != json_object_get_type( poObjPoints ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Invalid MultiPoint object. "
                  "Invalid \'points\' member." );
        return nullptr;
    }

    OGRMultiPoint* poMulti = new OGRMultiPoint();

    const auto nPoints = json_object_array_length( poObjPoints );
    for( auto i = decltype(nPoints){0}; i < nPoints; i++ )
    {
        int nNumCoords = 2;
        json_object* poObjCoords =
            json_object_array_get_idx( poObjPoints, i );
        double dfX = 0.0;
        double dfY = 0.0;
        double dfZ = 0.0;
        double dfM = 0.0;
        if( !OGRESRIJSONReaderParseXYZMArray (
            poObjCoords, bHasZ, bHasM, &dfX, &dfY, &dfZ, &dfM, &nNumCoords) )
        {
            delete poMulti;
            return nullptr;
        }

        if( nNumCoords == 3 && !bHasM )
        {
            poMulti->addGeometryDirectly( new OGRPoint(dfX, dfY, dfZ) );
        }
        else if( nNumCoords == 3 )
        {
            OGRPoint* poPoint = new OGRPoint(dfX, dfY);
            poPoint->setM(dfM);
            poMulti->addGeometryDirectly( poPoint );
        }
        else if( nNumCoords == 4 )
        {
            poMulti->addGeometryDirectly( new OGRPoint(dfX, dfY, dfZ, dfM) );
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
    OGRSpatialReference* poSRS = nullptr;

    json_object* poObjSrs =
        OGRGeoJSONFindMemberByName( poObj, "spatialReference" );
    if( nullptr != poObjSrs )
    {
        json_object* poObjWkid = OGRGeoJSONFindMemberByName( poObjSrs, "latestWkid" );
        if( poObjWkid == nullptr )
            poObjWkid = OGRGeoJSONFindMemberByName( poObjSrs, "wkid" );
        if( poObjWkid == nullptr )
        {
            json_object* poObjWkt =
                OGRGeoJSONFindMemberByName( poObjSrs, "wkt" );
            if( poObjWkt == nullptr )
                return nullptr;

            const char* pszWKT = json_object_get_string( poObjWkt );
            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if( OGRERR_NONE != poSRS->importFromWkt( pszWKT ) )
            {
                delete poSRS;
                poSRS = nullptr;
            }
            else
            {
                int nEntries = 0;
                int* panMatchConfidence = nullptr;
                OGRSpatialReferenceH* pahSRS = poSRS->FindMatches(
                    nullptr, &nEntries, &panMatchConfidence);
                if( nEntries == 1 && panMatchConfidence[0] >= 70 )
                {
                    delete poSRS;
                    poSRS = OGRSpatialReference::FromHandle(pahSRS[0])->Clone();
                    poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
                }
                OSRFreeSRSArray(pahSRS);
                CPLFree(panMatchConfidence);
            }

            return poSRS;
        }

        const int nEPSG = json_object_get_int( poObjWkid );

        poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        if( OGRERR_NONE != poSRS->importFromEPSG( nEPSG ) )
        {
            delete poSRS;
            poSRS = nullptr;
        }
    }

    return poSRS;
}
