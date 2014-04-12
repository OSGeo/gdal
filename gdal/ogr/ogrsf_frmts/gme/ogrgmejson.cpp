/******************************************************************************
 * $Id$
 *
 * Project:  Google Maps Engine API Driver
 * Purpose:  GME GeoJSON helpper function Implementations.
 * Author:   Wolf Beregnheim <wolf+grass@bergenheim.net>
 *           (derived from Geo JSON driver by Mateusz)
 *
 ******************************************************************************
 * Copyright (c) 2013, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogrgmejson.h"
#include <printbuf.h>

static int json_gme_double_to_string(json_object *jso,
                                     printbuf *pb,
                                     int level,
                                     int flags);

/************************************************************************/
/*                      OGRGMEFeatureToGeoJSON()                        */
/************************************************************************/
json_object* OGRGMEFeatureToGeoJSON(OGRFeature* poFeature)

{
    if( NULL == poFeature )
        return NULL;

    json_object* pjoFeature = json_object_new_object();
    CPLAssert( NULL != pjoFeature );

    json_object_object_add( pjoFeature, "type",
                            json_object_new_string("Feature") );

    /* -------------------------------------------------------------------- */
    /*      Write feature geometry to GeoJSON "geometry" object.            */
    /* -------------------------------------------------------------------- */
    json_object* pjoGeometry = NULL;
    OGRGeometry* poGeometry = poFeature->GetGeometryRef();

    pjoGeometry = OGRGMEGeometryToGeoJSON(poGeometry);
    if ( NULL == pjoGeometry ) {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GME: NULL Geometry detected in feature %ld. Ignoring feature.",
                  poFeature->GetFID() );
        json_object_put( pjoFeature );
        return NULL;
    }
    json_object_object_add( pjoFeature, "geometry", pjoGeometry );

    /* -------------------------------------------------------------------- */
    /*      Write feature attributes to GeoJSON "properties" object.        */
    /* -------------------------------------------------------------------- */
    json_object* pjoProps = NULL;

    pjoProps = OGRGMEAttributesToGeoJSON( poFeature );
    if ( pjoProps )
        json_object_object_add( pjoFeature, "properties", pjoProps );

    return pjoFeature;
}

/************************************************************************/
/*                        OGRGMEGeometryToGeoJSON()                     */
/************************************************************************/
json_object* OGRGMEGeometryToGeoJSON(OGRGeometry* poGeometry)

{
    if ( NULL == poGeometry )
        return NULL;

    json_object* pjoGeometry = json_object_new_object();
    CPLAssert( NULL != pjoGeometry );

    /* -------------------------------------------------------------------- */
    /*      Build "type" member of GeoJSOn "geometry" object                */
    /*      and "coordinates" member of GeoJSOn "geometry" object.          */
    /* -------------------------------------------------------------------- */
    const char* pszType = NULL;
    OGRwkbGeometryType eType = poGeometry->getGeometryType();
    json_object* pjoCoordinates = NULL;
    if( wkbGeometryCollection == eType || wkbGeometryCollection25D == eType )
    {
        pszType = "GeometryCollection";
        json_object *pjoGeometries =
            OGRGMEGeometryCollectionToGeoJSON(static_cast<OGRGeometryCollection*>(poGeometry));
        if ( pjoGeometries ) {
            json_object *pjoType = json_object_new_string(pszType);
            json_object_object_add( pjoGeometry, "type", pjoType );
            json_object_object_add( pjoGeometry, "geometries", pjoGeometries );
        }
        else {
            json_object_put(pjoGeometry);
            pjoGeometry = NULL;
        }            
    }
    else 
    {
        if( wkbPoint == eType || wkbPoint25D == eType ) {
            pszType = "Point";
            pjoCoordinates = OGRGMEPointToGeoJSON( static_cast<OGRPoint*>(poGeometry) );
        }
        if( wkbMultiPoint == eType || wkbMultiPoint25D == eType ) {
            pszType = "MultiPoint";
            pjoCoordinates = OGRGMEMultiPointToGeoJSON( static_cast<OGRMultiPoint*>(poGeometry) );
        }
        else if( wkbLineString == eType || wkbLineString25D == eType ) {
            pszType = "LineString";
            pjoCoordinates = OGRGMELineStringToGeoJSON( static_cast<OGRLineString*>(poGeometry) );
        }
        else if( wkbMultiLineString == eType || wkbMultiLineString25D == eType ) {
            pszType = "MultiLineString";
            pjoCoordinates = OGRGMEMultiLineStringToGeoJSON( static_cast<OGRMultiLineString*>(poGeometry) );
        }
        else if( wkbPolygon == eType || wkbPolygon25D == eType ) {
            pszType = "Polygon";
            pjoCoordinates = OGRGMEPolygonToGeoJSON( static_cast<OGRPolygon*>(poGeometry) );
        }
        else if( wkbMultiPolygon == eType || wkbMultiPolygon25D == eType ) {
            pszType = "MultiPolygon";
            pjoCoordinates = OGRGMEMultiPolygonToGeoJSON( static_cast<OGRMultiPolygon*>(poGeometry) );
        }
        else {
            CPLDebug( "GME", "Unsupported geometry type detected. Geometry is IGNORED." );
        }

        if ( pjoCoordinates && pszType ) {
            json_object *pjoType = json_object_new_string(pszType);
            json_object_object_add( pjoGeometry, "type", pjoType );
            json_object_object_add( pjoGeometry, "coordinates", pjoCoordinates);
        }
        else {
            json_object_put(pjoGeometry);
            pjoGeometry = NULL;
        }
    }
    return pjoGeometry;
}

/************************************************************************/
/*                  OGRGMEGeometryCollectionToGeoJSON()                 */
/************************************************************************/
json_object* OGRGMEGeometryCollectionToGeoJSON(OGRGeometryCollection* poGeometryCollection)

{
    if ( NULL == poGeometryCollection )
        return NULL;

    /* Generate "geometries" object. */
    json_object* pjoGeometries = NULL;
    pjoGeometries = json_object_new_array();

    for( int i = 0; i < poGeometryCollection->getNumGeometries(); ++i ) {
        OGRGeometry* poGeometry = poGeometryCollection->getGeometryRef( i );
        json_object* pjoGeometry = NULL;
        pjoGeometry = OGRGMEGeometryToGeoJSON( poGeometry );
        if ( NULL != poGeometry )
            json_object_array_add( pjoGeometries, pjoGeometry );
        else
            CPLError( CE_Failure, CPLE_AppDefined, "GME: Ignoring NULL geometry" );
    }
    return pjoGeometries;
}

/************************************************************************/
/*                           OGRGMEPointToGeoJSON                       */
/************************************************************************/

json_object* OGRGMEPointToGeoJSON( OGRPoint* poPoint )
{
    if( NULL == poPoint )
        return NULL;

    json_object* pjoCoordinates = NULL;

    /* Generate "coordinates" object for 2D or 3D dimension. */
    if( 3 == poPoint->getCoordinateDimension() ) {
        pjoCoordinates = OGRGMECoordsToGeoJSON( poPoint->getX(),
                                                poPoint->getY(),
                                                poPoint->getZ() );
    }
    else if( 2 == poPoint->getCoordinateDimension() ) {
        pjoCoordinates = OGRGMECoordsToGeoJSON( poPoint->getX(),
                                                poPoint->getY() );
    }
    else {
        CPLError( CE_Failure, CPLE_AppDefined, "GME: Found EMPTY point, ignoring" );
    }

    return pjoCoordinates;
}

/************************************************************************/
/*                           OGRGMEMultiPointToGeoJSON                  */
/************************************************************************/

json_object* OGRGMEMultiPointToGeoJSON( OGRMultiPoint* poGeometry )
{
    if( NULL == poGeometry )
        return NULL;

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* pjoMultiPoint = NULL;
    pjoMultiPoint = json_object_new_array();

    for( int i = 0; i < poGeometry->getNumGeometries(); ++i )
        {
            OGRGeometry* poGeom = poGeometry->getGeometryRef( i );
            CPLAssert( NULL != poGeom );
            OGRPoint* poPoint = static_cast<OGRPoint*>(poGeom);

            json_object* pjoPoint = NULL;
            pjoPoint = OGRGMEPointToGeoJSON( poPoint );
            if ( pjoPoint )
                json_object_array_add( pjoMultiPoint, pjoPoint );
        }

    return pjoMultiPoint;
}

/************************************************************************/
/*                           OGRGMELineStringToGeoJSON                  */
/************************************************************************/

json_object* OGRGMELineStringToGeoJSON( OGRLineString* poLine )
{
    if( NULL == poLine )
        return NULL;

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* pjoLineString = NULL;
    pjoLineString = OGRGMELineCoordsToGeoJSON( poLine );

    return pjoLineString;
}

/************************************************************************/
/*                           OGRGMEMultiLineStringToGeoJSON             */
/************************************************************************/

json_object* OGRGMEMultiLineStringToGeoJSON( OGRMultiLineString* poGeometry )
{
    CPLAssert( NULL != poGeometry );

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* pjoCoordinates = NULL;
    pjoCoordinates = json_object_new_array();

    for( int i = 0; i < poGeometry->getNumGeometries(); ++i )
    {
        OGRGeometry* poGeom = poGeometry->getGeometryRef( i );
        CPLAssert( NULL != poGeom );
        OGRLineString* poLine = static_cast<OGRLineString*>(poGeom);

        json_object* pjoLine = NULL;
        pjoLine = OGRGMELineStringToGeoJSON( poLine );

        json_object_array_add( pjoCoordinates, pjoLine );
    }

    return pjoCoordinates;
}

/************************************************************************/
/*                           OGRGMEPolygonToGeoJSON                     */
/************************************************************************/

json_object* OGRGMEPolygonToGeoJSON( OGRPolygon* poPolygon )
{
    CPLAssert( NULL != poPolygon );

    /* Generate "coordinates" array object. */
    json_object* pjoCoordinates = NULL;
    pjoCoordinates = json_object_new_array();
    
    /* Exterior ring. */
    OGRLinearRing* poRing = poPolygon->getExteriorRing();
    if (poRing == NULL) {
        json_object_put(pjoCoordinates);
        return NULL;
    }
    
    json_object* pjoRing = NULL;
    pjoRing = OGRGMELineCoordsToGeoJSON( poRing );
    json_object_array_add( pjoCoordinates, pjoRing );

    /* Interior rings. */
    const int nCount = poPolygon->getNumInteriorRings();
    for( int i = 0; i < nCount; ++i ) {
        poRing = poPolygon->getInteriorRing( i );
        if (poRing == NULL)
            continue;
        pjoRing = OGRGMELineCoordsToGeoJSON( poRing );
        json_object_array_add( pjoCoordinates, pjoRing );
    }

    return pjoCoordinates;
}

/************************************************************************/
/*                           OGRGMEMultiPolygonToGeoJSON                */
/************************************************************************/

json_object* OGRGMEMultiPolygonToGeoJSON( OGRMultiPolygon* poGeometry )
{
    CPLAssert( NULL != poGeometry );

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* pjoCoordinates = NULL;
    pjoCoordinates = json_object_new_array();

    for( int i = 0; i < poGeometry->getNumGeometries(); ++i )
    {
        OGRGeometry* poGeom = poGeometry->getGeometryRef( i );
        CPLAssert( NULL != poGeom );
        OGRPolygon* poPoly = static_cast<OGRPolygon*>(poGeom);

        json_object* pjoPoly = NULL;
        pjoPoly = OGRGMEPolygonToGeoJSON( poPoly );

        json_object_array_add( pjoCoordinates, pjoPoly );
    }

    return pjoCoordinates;
}

/************************************************************************/
/*                           OGRGMECoordsToGeoJSON                      */
/************************************************************************/

json_object* OGRGMECoordsToGeoJSON( double const& fX, double const& fY )
{
    json_object* pjoCoords = NULL;
    pjoCoords = json_object_new_array();
    json_object_array_add( pjoCoords, json_object_new_gme_double( fX ) );
    json_object_array_add( pjoCoords, json_object_new_gme_double( fY ) );

    return pjoCoords;
}

json_object* OGRGMECoordsToGeoJSON( double const& fX, double const& fY, double const& fZ )
{
    json_object* pjoCoords = NULL;
    pjoCoords = json_object_new_array();
    json_object_array_add( pjoCoords, json_object_new_gme_double( fX ) );
    json_object_array_add( pjoCoords, json_object_new_gme_double( fY ) );
    json_object_array_add( pjoCoords, json_object_new_gme_double( fZ ) );

    return pjoCoords;
}

/************************************************************************/
/*                           OGRGMELineCoordsToGeoJSON                  */
/************************************************************************/

json_object* OGRGMELineCoordsToGeoJSON( OGRLineString* poLine )
{
    json_object* pjoCoords = json_object_new_array();

    const int nCount = poLine->getNumPoints();
    for( int i = 0; i < nCount; ++i )
    {
        json_object* pjoPoint = NULL;
        if( poLine->getCoordinateDimension() == 2 )
            pjoPoint = OGRGMECoordsToGeoJSON( poLine->getX(i), poLine->getY(i) );
        else
            pjoPoint = OGRGMECoordsToGeoJSON( poLine->getX(i), poLine->getY(i), poLine->getZ(i) );
        json_object_array_add( pjoCoords, pjoPoint );
    }

    return pjoCoords;
}

/************************************************************************/
/*                          OGRGMEAttributesToGeoJSON                   */
/************************************************************************/

json_object* OGRGMEAttributesToGeoJSON( OGRFeature* poFeature )
{
    if ( NULL == poFeature )
        return NULL;

    json_object* pjoProperties = json_object_new_object();
    CPLAssert( NULL != pjoProperties );

    OGRFeatureDefn* poDefn = poFeature->GetDefnRef();
    for( int nField = 0; nField < poDefn->GetFieldCount(); ++nField ) {
        json_object* pjoProperty = NULL;
        OGRFieldDefn* poFieldDefn = poDefn->GetFieldDefn( nField );
        if ( NULL == poFieldDefn )
            continue;
        OGRFieldType eType = poFieldDefn->GetType();

        if( !poFeature->IsFieldSet(nField) )
            pjoProperty = NULL;

        // In GME integers are encoded as strings.
        else if( OFTInteger == eType )
            pjoProperty = json_object_new_string( poFeature->GetFieldAsString( nField ) ); 

        else if( OFTReal == eType )
            pjoProperty = json_object_new_gme_double( poFeature->GetFieldAsDouble(nField) );

        // Supported types are integer, double and string. So treating everything else as strings
        else
            pjoProperty = json_object_new_string( poFeature->GetFieldAsString(nField) ); 

        json_object_object_add( pjoProperties, poFieldDefn->GetNameRef(), pjoProperty );
    }
    return pjoProperties;
}

/************************************************************************/
/*                        json_object_new_gme_double()                  */
/************************************************************************/

json_object* json_object_new_gme_double(double dfVal)

{
    json_object* pjoD = json_object_new_double(dfVal);
    json_object_set_serializer(pjoD, json_gme_double_to_string, NULL, NULL );

    return pjoD;
}

/************************************************************************/
/*                        json_gme_double_to_string()                   */
/************************************************************************/

static int json_gme_double_to_string(json_object *pjo,
                                     printbuf *pb,
                                     int level,
                                     int flags)
{
  char buf[128], *p, *q;
  int size;

  size = snprintf(buf, 128, "%.8f", json_object_get_double(pjo));
  p = strchr(buf, ',');
  if (p) {
    *p = '.';
  } else {
    p = strchr(buf, '.');
  }
  if (p) {
    /* last useful digit, always keep 1 zero */
    p++;
    for (q=p ; *q ; q++) {
      if (*q!='0') p=q;
    }
    /* drop trailing zeroes */
    *(++p) = 0;
    size = p-buf;
  }
  printbuf_memappend(pb, buf, size);
  return size;
}
