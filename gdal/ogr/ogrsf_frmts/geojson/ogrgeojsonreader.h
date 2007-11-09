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
#include <json.h> // JSON-C

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

class OGRGeoJSONReader
{
public:

    OGRGeoJSONReader();
    ~OGRGeoJSONReader();

    void SetPreserveGeometryType( bool bPreserve );
    void SetSkipAttributes( bool bSkip );

    OGRErr Parse( const char* pszText );
    OGRGeoJSONLayer* ReadLayer( const char* pszName );

private:

    json_object* poGJObject_;
    OGRGeoJSONLayer* poLayer_;
    bool bGeometryPreserve_;
    bool bAttributesSkip_;

    //
    // Copy operations not supported.
    //
    OGRGeoJSONReader( OGRGeoJSONReader const& );
    OGRGeoJSONReader& operator=( OGRGeoJSONReader const& );

    //
    // GeoJSON tree parsing utilities.
    //
    json_object* FindMemberByName(json_object* poObj,  const char* pszName );
    GeoJSONObject::Type GetType( json_object* poObj );

    //
    // Translation utilities.
    //
    bool GenerateLayerDefn();
    bool GenerateFeatureDefn( json_object* poObj );
    bool AddFeature( OGRGeometry* poGeometry );
    bool AddFeature( OGRFeature* poFeature );
    bool ReadRawPoint( json_object* poObj, OGRPoint& point );
    OGRRawPoint* ReadRawPoint( json_object* poObj );
    OGRPoint* ReadPoint( json_object* poObj );
    OGRMultiPoint* ReadMultiPoint( json_object* poObj );
    OGRLineString* ReadLineString( json_object* poObj, bool bRaw=false );
    OGRLinearRing* ReadLinearRing( json_object* poObj );
    OGRMultiLineString* ReadMultiLineString( json_object* poObj );
    OGRPolygon* ReadPolygon( json_object* poObj , bool bRaw=false);
    OGRMultiPolygon* ReadMultiPolygon( json_object* poObj );
    OGRGeometry* ReadGeometry( json_object* poObj );
    OGRGeometryCollection* ReadGeometryCollection( json_object* poObj );
    OGRFeature* ReadFeature( json_object* poObj );
    OGRGeoJSONLayer* ReadFeatureCollection( json_object* poObj );

};

#endif /* OGR_GEOJSONUTILS_H_INCLUDED */
