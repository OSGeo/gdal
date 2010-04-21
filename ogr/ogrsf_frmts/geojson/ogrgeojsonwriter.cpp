/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of GeoJSON writer utilities (OGR GeoJSON Driver).
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
#include "ogrgeojsonwriter.h"
#include "ogrgeojsonutils.h"
#include "ogr_geojson.h"
#include <jsonc/json.h> // JSON-C
#include <ogr_api.h>

/************************************************************************/
/*                           OGRGeoJSONWriteFeature                     */
/************************************************************************/

json_object* OGRGeoJSONWriteFeature( OGRFeature* poFeature )
{
    CPLAssert( NULL != poFeature );

    json_object* poObj = json_object_new_object();
    CPLAssert( NULL != poObj );

    json_object_object_add( poObj, "type",
                            json_object_new_string("Feature") );

/* -------------------------------------------------------------------- */
/*      Write feature attributes to GeoJSON "properties" object.        */
/* -------------------------------------------------------------------- */
    json_object* poObjProps = NULL;

    poObjProps = OGRGeoJSONWriteAttributes( poFeature );
    json_object_object_add( poObj, "properties", poObjProps );

/* -------------------------------------------------------------------- */
/*      Write feature geometry to GeoJSON "geometry" object.            */
/*      Null geometries are allowed, according to the GeoJSON Spec.     */
/* -------------------------------------------------------------------- */
    json_object* poObjGeom = NULL;

    OGRGeometry* poGeometry = poFeature->GetGeometryRef();
    if ( NULL != poGeometry )
    {
        poObjGeom = OGRGeoJSONWriteGeometry( poGeometry );
        CPLAssert( NULL != poObjGeom );
    }
    
    json_object_object_add( poObj, "geometry", poObjGeom );

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteGeometry                    */
/************************************************************************/

json_object* OGRGeoJSONWriteAttributes( OGRFeature* poFeature )
{
    CPLAssert( NULL != poFeature );

    json_object* poObjProps = json_object_new_object();
    CPLAssert( NULL != poObjProps );

    OGRFeatureDefn* poDefn = poFeature->GetDefnRef();
    for( int nField = 0; nField < poDefn->GetFieldCount(); ++nField )
    {
        json_object* poObjProp = NULL;
        OGRFieldDefn* poFieldDefn = poDefn->GetFieldDefn( nField );
        CPLAssert( NULL != poFieldDefn );

        if( OFTInteger == poFieldDefn->GetType() )
        {
            poObjProp = json_object_new_int( 
                poFeature->GetFieldAsInteger( nField ) );
        }
        else if( OFTReal == poFieldDefn->GetType() )
        {
            poObjProp = json_object_new_double( 
                poFeature->GetFieldAsDouble(nField) );
        }
        else if( OFTString == poFieldDefn->GetType() )
        {
            poObjProp = json_object_new_string( 
                poFeature->GetFieldAsString(nField) );
        }
        else
        {
            poObjProp = json_object_new_string( 
                 poFeature->GetFieldAsString(nField) );
        }

        json_object_object_add( poObjProps,
                                poFieldDefn->GetNameRef(),
                                poObjProp );
    }

    return poObjProps;
}

/************************************************************************/
/*                           OGRGeoJSONWriteGeometry                    */
/************************************************************************/

json_object* OGRGeoJSONWriteGeometry( OGRGeometry* poGeometry )
{
    CPLAssert( NULL != poGeometry );

    json_object* poObj = json_object_new_object();
    CPLAssert( NULL != poObj );

/* -------------------------------------------------------------------- */
/*      Build "type" member of GeoJSOn "geometry" object.               */
/* -------------------------------------------------------------------- */
    
    // XXX - mloskot: workaround hack for pure JSON-C API design.
    char* pszName = const_cast<char*>(OGRGeoJSONGetGeometryName( poGeometry ));
    json_object_object_add( poObj, "type", json_object_new_string(pszName) );

/* -------------------------------------------------------------------- */
/*      Build "coordinates" member of GeoJSOn "geometry" object.        */
/* -------------------------------------------------------------------- */
    json_object* poObjGeom = NULL;

    OGRwkbGeometryType eType = poGeometry->getGeometryType();
    if( wkbGeometryCollection == eType || wkbGeometryCollection25D == eType )
    {
        poObjGeom = OGRGeoJSONWriteGeometryCollection( static_cast<OGRGeometryCollection*>(poGeometry) );
        json_object_object_add( poObj, "geometries", poObjGeom);
    }
    else
    {
        if( wkbPoint == eType || wkbPoint25D == eType )
            poObjGeom = OGRGeoJSONWritePoint( static_cast<OGRPoint*>(poGeometry) );
        else if( wkbLineString == eType || wkbLineString25D == eType )
            poObjGeom = OGRGeoJSONWriteLineString( static_cast<OGRLineString*>(poGeometry) );
        else if( wkbPolygon == eType || wkbPolygon25D == eType )
            poObjGeom = OGRGeoJSONWritePolygon( static_cast<OGRPolygon*>(poGeometry) );
        else if( wkbMultiPoint == eType || wkbMultiPoint25D == eType )
            poObjGeom = OGRGeoJSONWriteMultiPoint( static_cast<OGRMultiPoint*>(poGeometry) );
        else if( wkbMultiLineString == eType || wkbMultiLineString25D == eType )
            poObjGeom = OGRGeoJSONWriteMultiLineString( static_cast<OGRMultiLineString*>(poGeometry) );
        else if( wkbMultiPolygon == eType || wkbMultiPolygon25D == eType )
            poObjGeom = OGRGeoJSONWriteMultiPolygon( static_cast<OGRMultiPolygon*>(poGeometry) );
        else
        {
            CPLDebug( "GeoJSON",
                "Unsupported geometry type detected. "
                "Feature gets NULL geometry assigned." );
        }

        json_object_object_add( poObj, "coordinates", poObjGeom);
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWritePoint                       */
/************************************************************************/

json_object* OGRGeoJSONWritePoint( OGRPoint* poPoint )
{
    CPLAssert( NULL != poPoint );

    json_object* poObj = NULL;

    /* Generate "coordinates" object for 2D or 3D dimension. */
    if( 3 == poPoint->getCoordinateDimension() )
    {
        poObj = OGRGeoJSONWriteCoords( poPoint->getX(),
                                       poPoint->getY(),
                                       poPoint->getZ() );
    }
    else if( 2 == poPoint->getCoordinateDimension() )
    {
        poObj = OGRGeoJSONWriteCoords( poPoint->getX(),
                                       poPoint->getY() );
    }
    else
    {
        /* We can get here with POINT EMPTY geometries */
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteLineString                  */
/************************************************************************/

json_object* OGRGeoJSONWriteLineString( OGRLineString* poLine )
{
    CPLAssert( NULL != poLine );

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* poObj = NULL;
    poObj = OGRGeoJSONWriteLineCoords( poLine );

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWritePolygon                     */
/************************************************************************/

json_object* OGRGeoJSONWritePolygon( OGRPolygon* poPolygon )
{
    CPLAssert( NULL != poPolygon );

    /* Generate "coordinates" array object. */
    json_object* poObj = NULL;
    poObj = json_object_new_array();
    
    /* Exterior ring. */
    OGRLinearRing* poRing = poPolygon->getExteriorRing();
    if (poRing == NULL)
        return poObj;
    
    json_object* poObjRing = NULL;
    poObjRing = OGRGeoJSONWriteLineCoords( poRing );
    json_object_array_add( poObj, poObjRing );

    /* Interior rings. */
    const int nCount = poPolygon->getNumInteriorRings();
    for( int i = 0; i < nCount; ++i )
    {
        poRing = poPolygon->getInteriorRing( i );
        if (poRing == NULL)
            continue;

        poObjRing = OGRGeoJSONWriteLineCoords( poRing );

        json_object_array_add( poObj, poObjRing );
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteMultiPoint                  */
/************************************************************************/

json_object* OGRGeoJSONWriteMultiPoint( OGRMultiPoint* poGeometry )
{
    CPLAssert( NULL != poGeometry );

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* poObj = NULL;
    poObj = json_object_new_array();

    for( int i = 0; i < poGeometry->getNumGeometries(); ++i )
    {
        OGRGeometry* poGeom = poGeometry->getGeometryRef( i );
        CPLAssert( NULL != poGeom );
        OGRPoint* poPoint = static_cast<OGRPoint*>(poGeom);

        json_object* poObjPoint = NULL;
        poObjPoint = OGRGeoJSONWritePoint( poPoint );

        json_object_array_add( poObj, poObjPoint );
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteMultiLineString             */
/************************************************************************/

json_object* OGRGeoJSONWriteMultiLineString( OGRMultiLineString* poGeometry )
{
    CPLAssert( NULL != poGeometry );

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* poObj = NULL;
    poObj = json_object_new_array();

    for( int i = 0; i < poGeometry->getNumGeometries(); ++i )
    {
        OGRGeometry* poGeom = poGeometry->getGeometryRef( i );
        CPLAssert( NULL != poGeom );
        OGRLineString* poLine = static_cast<OGRLineString*>(poGeom);

        json_object* poObjLine = NULL;
        poObjLine = OGRGeoJSONWriteLineString( poLine );
        
        json_object_array_add( poObj, poObjLine );
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteMultiPolygon                */
/************************************************************************/

json_object* OGRGeoJSONWriteMultiPolygon( OGRMultiPolygon* poGeometry )
{
    CPLAssert( NULL != poGeometry );

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* poObj = NULL;
    poObj = json_object_new_array();

    for( int i = 0; i < poGeometry->getNumGeometries(); ++i )
    {
        OGRGeometry* poGeom = poGeometry->getGeometryRef( i );
        CPLAssert( NULL != poGeom );
        OGRPolygon* poPoly = static_cast<OGRPolygon*>(poGeom);

        json_object* poObjPoly = NULL;
        poObjPoly = OGRGeoJSONWritePolygon( poPoly );
        
        json_object_array_add( poObj, poObjPoly );
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteGeometryCollection          */
/************************************************************************/

json_object* OGRGeoJSONWriteGeometryCollection( OGRGeometryCollection* poGeometry )
{
    CPLAssert( NULL != poGeometry );

    /* Generate "geometries" object. */
    json_object* poObj = NULL;
    poObj = json_object_new_array();

    for( int i = 0; i < poGeometry->getNumGeometries(); ++i )
    {
        OGRGeometry* poGeom = poGeometry->getGeometryRef( i );
        CPLAssert( NULL != poGeom );
        
        json_object* poObjGeom = NULL;
        poObjGeom = OGRGeoJSONWriteGeometry( poGeom );
        
        json_object_array_add( poObj, poObjGeom );
    }

    return poObj;
}
/************************************************************************/
/*                           OGRGeoJSONWriteCoords                      */
/************************************************************************/

json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY )
{
    json_object* poObjCoords = NULL;
    poObjCoords = json_object_new_array();
    json_object_array_add( poObjCoords, json_object_new_double( fX ) );
    json_object_array_add( poObjCoords, json_object_new_double( fY ) );

    return poObjCoords;
}

json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY, double const& fZ )
{
    json_object* poObjCoords = NULL;
    poObjCoords = json_object_new_array();
    json_object_array_add( poObjCoords, json_object_new_double( fX ) );
    json_object_array_add( poObjCoords, json_object_new_double( fY ) );
    json_object_array_add( poObjCoords, json_object_new_double( fZ ) );

    return poObjCoords;
}

/************************************************************************/
/*                           OGRGeoJSONWriteLineCoords                  */
/************************************************************************/

json_object* OGRGeoJSONWriteLineCoords( OGRLineString* poLine )
{
    json_object* poObjPoint = NULL;
    json_object* poObjCoords = json_object_new_array();

    const int nCount = poLine->getNumPoints();
    for( int i = 0; i < nCount; ++i )
    {
        poObjPoint = OGRGeoJSONWriteCoords( poLine->getX(i), poLine->getY(i) );
        json_object_array_add( poObjCoords, poObjPoint );
    }
    
    return poObjCoords;
}

/************************************************************************/
/*                           OGR_G_ExportToJson                         */
/************************************************************************/

char* OGR_G_ExportToJson( OGRGeometryH hGeometry )
{
    VALIDATE_POINTER1( hGeometry, "OGR_G_ExportToJson", NULL );

    OGRGeometry* poGeometry = (OGRGeometry*) (hGeometry);

    json_object* poObj = NULL;
    poObj = OGRGeoJSONWriteGeometry( poGeometry );
    
    if( NULL != poObj )
    {
        char* pszJson = CPLStrdup( json_object_to_json_string( poObj ) );
        
        /* Release JSON tree. */
        json_object_put( poObj );

        return pszJson;
    }

    /* Translation failed */
    return NULL;
}

