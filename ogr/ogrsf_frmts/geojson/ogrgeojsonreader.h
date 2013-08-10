/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines GeoJSON reader within OGR OGRGeoJSON Driver.
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
#ifndef OGR_GEOJSONREADER_H_INCLUDED
#define OGR_GEOJSONREADER_H_INCLUDED

#include <ogr_core.h>
#include <json-c/json.h> // JSON-C

/************************************************************************/
/*                         FORWARD DECLARATIONS                         */
/************************************************************************/

class OGRGeometry;
class OGRRawPoint;
class OGRPoint;
class OGRMultiPoint;
class OGRLineString;
class OGRMultiLineString;
class OGRLinearRing;
class OGRPolygon;
class OGRMultiPolygon;
class OGRGeometryCollection;
class OGRFeature;
class OGRGeoJSONLayer;
class OGRSpatialReference;

/************************************************************************/
/*                           GeoJSONObject                              */
/************************************************************************/

struct GeoJSONObject
{
    enum Type
    {
        eUnknown = wkbUnknown, // non-GeoJSON properties
        ePoint = wkbPoint,
        eLineString = wkbLineString,
        ePolygon = wkbPolygon,
        eMultiPoint = wkbMultiPoint,
        eMultiLineString = wkbMultiLineString,
        eMultiPolygon = wkbMultiPolygon,
        eGeometryCollection = wkbGeometryCollection,
        eFeature,
        eFeatureCollection
    };
    
    enum CoordinateDimension
    {
        eMinCoordinateDimension = 2,
        eMaxCoordinateDimension = 3
    };
};

/************************************************************************/
/*                           OGRGeoJSONReader                           */
/************************************************************************/

class OGRGeoJSONDataSource;

class OGRGeoJSONReader
{
public:

    OGRGeoJSONReader();
    ~OGRGeoJSONReader();

    void SetPreserveGeometryType( bool bPreserve );
    void SetSkipAttributes( bool bSkip );

    OGRErr Parse( const char* pszText );
    void ReadLayers( OGRGeoJSONDataSource* poDS );
    void ReadLayer( OGRGeoJSONDataSource* poDS,
                    const char* pszName,
                    json_object* poObj );

private:

    json_object* poGJObject_;

    bool bGeometryPreserve_;
    bool bAttributesSkip_;

    int bFlattenGeocouchSpatiallistFormat;
    bool bFoundId, bFoundRev, bFoundTypeFeature, bIsGeocouchSpatiallistFormat;

    //
    // Copy operations not supported.
    //
    OGRGeoJSONReader( OGRGeoJSONReader const& );
    OGRGeoJSONReader& operator=( OGRGeoJSONReader const& );

    //
    // Translation utilities.
    //
    bool GenerateLayerDefn( OGRGeoJSONLayer* poLayer, json_object* poGJObject );
    bool GenerateFeatureDefn( OGRGeoJSONLayer* poLayer, json_object* poObj );
    bool AddFeature( OGRGeoJSONLayer* poLayer, OGRGeometry* poGeometry );
    bool AddFeature( OGRGeoJSONLayer* poLayer, OGRFeature* poFeature );

    OGRGeometry* ReadGeometry( json_object* poObj );
    OGRFeature* ReadFeature( OGRGeoJSONLayer* poLayer, json_object* poObj );
    void ReadFeatureCollection( OGRGeoJSONLayer* poLayer, json_object* poObj );
};

/************************************************************************/
/*                 GeoJSON Parsing Utilities                            */
/************************************************************************/

json_object* OGRGeoJSONFindMemberByName(json_object* poObj,  const char* pszName );
GeoJSONObject::Type OGRGeoJSONGetType( json_object* poObj );

/************************************************************************/
/*                 GeoJSON Geometry Translators                         */
/************************************************************************/

bool OGRGeoJSONReadRawPoint( json_object* poObj, OGRPoint& point );
OGRGeometry* OGRGeoJSONReadGeometry( json_object* poObj );
OGRPoint* OGRGeoJSONReadPoint( json_object* poObj );
OGRMultiPoint* OGRGeoJSONReadMultiPoint( json_object* poObj );
OGRLineString* OGRGeoJSONReadLineString( json_object* poObj, bool bRaw=false );
OGRMultiLineString* OGRGeoJSONReadMultiLineString( json_object* poObj );
OGRLinearRing* OGRGeoJSONReadLinearRing( json_object* poObj );
OGRPolygon* OGRGeoJSONReadPolygon( json_object* poObj , bool bRaw=false);
OGRMultiPolygon* OGRGeoJSONReadMultiPolygon( json_object* poObj );
OGRGeometryCollection* OGRGeoJSONReadGeometryCollection( json_object* poObj );
OGRSpatialReference* OGRGeoJSONReadSpatialReference( json_object* poObj );



/************************************************************************/
/*                          OGRESRIJSONReader                           */
/************************************************************************/

class OGRESRIJSONReader
{
public:

    OGRESRIJSONReader();
    ~OGRESRIJSONReader();

    OGRErr Parse( const char* pszText );
    void ReadLayers( OGRGeoJSONDataSource* poDS );

private:

    json_object* poGJObject_;
    OGRGeoJSONLayer* poLayer_;

    //
    // Copy operations not supported.
    //
    OGRESRIJSONReader( OGRESRIJSONReader const& );
    OGRESRIJSONReader& operator=( OGRESRIJSONReader const& );

    //
    // Translation utilities.
    //
    bool GenerateLayerDefn();
    bool GenerateFeatureDefn( json_object* poObj );
    bool AddFeature( OGRFeature* poFeature );

    OGRGeometry* ReadGeometry( json_object* poObj );
    OGRFeature* ReadFeature( json_object* poObj );
    OGRGeoJSONLayer* ReadFeatureCollection( json_object* poObj );
};

OGRSpatialReference* OGRESRIJSONReadSpatialReference( json_object* poObj );
OGRwkbGeometryType OGRESRIJSONGetGeometryType( json_object* poObj );
OGRPoint* OGRESRIJSONReadPoint( json_object* poObj);
OGRLineString* OGRESRIJSONReadLineString( json_object* poObj);
OGRPolygon* OGRESRIJSONReadPolygon( json_object* poObj);
OGRMultiPoint* OGRESRIJSONReadMultiPoint( json_object* poObj);

/************************************************************************/
/*                          OGRTopoJSONReader                           */
/************************************************************************/

class OGRTopoJSONReader
{
public:

    OGRTopoJSONReader();
    ~OGRTopoJSONReader();

    OGRErr Parse( const char* pszText );
    void ReadLayers( OGRGeoJSONDataSource* poDS );

private:

    json_object* poGJObject_;

    //
    // Copy operations not supported.
    //
    OGRTopoJSONReader( OGRTopoJSONReader const& );
    OGRTopoJSONReader& operator=( OGRTopoJSONReader const& );
};

#endif /* OGR_GEOJSONUTILS_H_INCLUDED */
