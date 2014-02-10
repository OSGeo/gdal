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
#include <json.h> // JSON-C
#include <json_object_private.h>
#include <printbuf.h>
#include <ogr_api.h>
#include <ogr_p.h>

/************************************************************************/
/*                           OGRGeoJSONWriteFeature                     */
/************************************************************************/

json_object* OGRGeoJSONWriteFeature( OGRFeature* poFeature, int bWriteBBOX, int nCoordPrecision )
{
    CPLAssert( NULL != poFeature );

    json_object* poObj = json_object_new_object();
    CPLAssert( NULL != poObj );

    json_object_object_add( poObj, "type",
                            json_object_new_string("Feature") );

/* -------------------------------------------------------------------- */
/*      Write FID if available                                          */
/* -------------------------------------------------------------------- */
    if ( poFeature->GetFID() != OGRNullFID )
    {
        json_object_object_add( poObj, "id",
                                json_object_new_int((int)poFeature->GetFID()) );
    }

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
        poObjGeom = OGRGeoJSONWriteGeometry( poGeometry, nCoordPrecision );

        if ( bWriteBBOX && !poGeometry->IsEmpty() )
        {
            OGREnvelope3D sEnvelope;
            poGeometry->getEnvelope(&sEnvelope);

            json_object* poObjBBOX = json_object_new_array();
            json_object_array_add(poObjBBOX,
                            json_object_new_double_with_precision(sEnvelope.MinX, nCoordPrecision));
            json_object_array_add(poObjBBOX,
                            json_object_new_double_with_precision(sEnvelope.MinY, nCoordPrecision));
            if (poGeometry->getCoordinateDimension() == 3)
                json_object_array_add(poObjBBOX,
                            json_object_new_double_with_precision(sEnvelope.MinZ, nCoordPrecision));
            json_object_array_add(poObjBBOX,
                            json_object_new_double_with_precision(sEnvelope.MaxX, nCoordPrecision));
            json_object_array_add(poObjBBOX,
                            json_object_new_double_with_precision(sEnvelope.MaxY, nCoordPrecision));
            if (poGeometry->getCoordinateDimension() == 3)
                json_object_array_add(poObjBBOX,
                            json_object_new_double_with_precision(sEnvelope.MaxZ, nCoordPrecision));

            json_object_object_add( poObj, "bbox", poObjBBOX );
        }
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
        json_object* poObjProp;
        OGRFieldDefn* poFieldDefn = poDefn->GetFieldDefn( nField );
        CPLAssert( NULL != poFieldDefn );
        OGRFieldType eType = poFieldDefn->GetType();

        if( !poFeature->IsFieldSet(nField) )
        {
            poObjProp = NULL;
        }
        else if( OFTInteger == eType )
        {
            poObjProp = json_object_new_int( 
                poFeature->GetFieldAsInteger( nField ) );
        }
        else if( OFTReal == eType )
        {
            poObjProp = json_object_new_double( 
                poFeature->GetFieldAsDouble(nField) );
        }
        else if( OFTString == eType )
        {
            poObjProp = json_object_new_string( 
                poFeature->GetFieldAsString(nField) );
        }
        else if( OFTIntegerList == eType )
        {
            int nSize = 0;
            const int* panList = poFeature->GetFieldAsIntegerList(nField, &nSize);
            poObjProp = json_object_new_array();
            for(int i=0;i<nSize;i++)
            {
                json_object_array_add(poObjProp,
                            json_object_new_int(panList[i]));
            }
        }
        else if( OFTRealList == eType )
        {
            int nSize = 0;
            const double* padfList = poFeature->GetFieldAsDoubleList(nField, &nSize);
            poObjProp = json_object_new_array();
            for(int i=0;i<nSize;i++)
            {
                json_object_array_add(poObjProp,
                            json_object_new_double(padfList[i]));
            }
        }
        else if( OFTStringList == eType )
        {
            char** papszStringList = poFeature->GetFieldAsStringList(nField);
            poObjProp = json_object_new_array();
            for(int i=0; papszStringList && papszStringList[i]; i++)
            {
                json_object_array_add(poObjProp,
                            json_object_new_string(papszStringList[i]));
            }
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

json_object* OGRGeoJSONWriteGeometry( OGRGeometry* poGeometry, int nCoordPrecision )
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
        poObjGeom = OGRGeoJSONWriteGeometryCollection( static_cast<OGRGeometryCollection*>(poGeometry), nCoordPrecision );
        json_object_object_add( poObj, "geometries", poObjGeom);
    }
    else
    {
        if( wkbPoint == eType || wkbPoint25D == eType )
            poObjGeom = OGRGeoJSONWritePoint( static_cast<OGRPoint*>(poGeometry), nCoordPrecision );
        else if( wkbLineString == eType || wkbLineString25D == eType )
            poObjGeom = OGRGeoJSONWriteLineString( static_cast<OGRLineString*>(poGeometry), nCoordPrecision );
        else if( wkbPolygon == eType || wkbPolygon25D == eType )
            poObjGeom = OGRGeoJSONWritePolygon( static_cast<OGRPolygon*>(poGeometry), nCoordPrecision );
        else if( wkbMultiPoint == eType || wkbMultiPoint25D == eType )
            poObjGeom = OGRGeoJSONWriteMultiPoint( static_cast<OGRMultiPoint*>(poGeometry), nCoordPrecision );
        else if( wkbMultiLineString == eType || wkbMultiLineString25D == eType )
            poObjGeom = OGRGeoJSONWriteMultiLineString( static_cast<OGRMultiLineString*>(poGeometry), nCoordPrecision );
        else if( wkbMultiPolygon == eType || wkbMultiPolygon25D == eType )
            poObjGeom = OGRGeoJSONWriteMultiPolygon( static_cast<OGRMultiPolygon*>(poGeometry), nCoordPrecision );
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

json_object* OGRGeoJSONWritePoint( OGRPoint* poPoint, int nCoordPrecision )
{
    CPLAssert( NULL != poPoint );

    json_object* poObj = NULL;

    /* Generate "coordinates" object for 2D or 3D dimension. */
    if( 3 == poPoint->getCoordinateDimension() )
    {
        poObj = OGRGeoJSONWriteCoords( poPoint->getX(),
                                       poPoint->getY(),
                                       poPoint->getZ(),
                                       nCoordPrecision );
    }
    else if( 2 == poPoint->getCoordinateDimension() )
    {
        poObj = OGRGeoJSONWriteCoords( poPoint->getX(),
                                       poPoint->getY(),
                                       nCoordPrecision );
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

json_object* OGRGeoJSONWriteLineString( OGRLineString* poLine, int nCoordPrecision )
{
    CPLAssert( NULL != poLine );

    /* Generate "coordinates" object for 2D or 3D dimension. */
    json_object* poObj = NULL;
    poObj = OGRGeoJSONWriteLineCoords( poLine, nCoordPrecision );

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWritePolygon                     */
/************************************************************************/

json_object* OGRGeoJSONWritePolygon( OGRPolygon* poPolygon, int nCoordPrecision )
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
    poObjRing = OGRGeoJSONWriteLineCoords( poRing, nCoordPrecision );
    if( poObjRing == NULL )
    {
        json_object_put(poObj);
        return NULL;
    }
    json_object_array_add( poObj, poObjRing );

    /* Interior rings. */
    const int nCount = poPolygon->getNumInteriorRings();
    for( int i = 0; i < nCount; ++i )
    {
        poRing = poPolygon->getInteriorRing( i );
        if (poRing == NULL)
            continue;

        poObjRing = OGRGeoJSONWriteLineCoords( poRing, nCoordPrecision );
        if( poObjRing == NULL )
        {
            json_object_put(poObj);
            return NULL;
        }

        json_object_array_add( poObj, poObjRing );
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteMultiPoint                  */
/************************************************************************/

json_object* OGRGeoJSONWriteMultiPoint( OGRMultiPoint* poGeometry, int nCoordPrecision )
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
        poObjPoint = OGRGeoJSONWritePoint( poPoint, nCoordPrecision );
        if( poObjPoint == NULL )
        {
            json_object_put(poObj);
            return NULL;
        }

        json_object_array_add( poObj, poObjPoint );
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteMultiLineString             */
/************************************************************************/

json_object* OGRGeoJSONWriteMultiLineString( OGRMultiLineString* poGeometry, int nCoordPrecision )
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
        poObjLine = OGRGeoJSONWriteLineString( poLine, nCoordPrecision );
        if( poObjLine == NULL )
        {
            json_object_put(poObj);
            return NULL;
        }
        
        json_object_array_add( poObj, poObjLine );
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteMultiPolygon                */
/************************************************************************/

json_object* OGRGeoJSONWriteMultiPolygon( OGRMultiPolygon* poGeometry, int nCoordPrecision )
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
        poObjPoly = OGRGeoJSONWritePolygon( poPoly, nCoordPrecision );
        if( poObjPoly == NULL )
        {
            json_object_put(poObj);
            return NULL;
        }
        
        json_object_array_add( poObj, poObjPoly );
    }

    return poObj;
}

/************************************************************************/
/*                           OGRGeoJSONWriteGeometryCollection          */
/************************************************************************/

json_object* OGRGeoJSONWriteGeometryCollection( OGRGeometryCollection* poGeometry, int nCoordPrecision )
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
        poObjGeom = OGRGeoJSONWriteGeometry( poGeom, nCoordPrecision );
        if( poGeom == NULL )
        {
            json_object_put(poObj);
            return NULL;
        }
        
        json_object_array_add( poObj, poObjGeom );
    }

    return poObj;
}
/************************************************************************/
/*                           OGRGeoJSONWriteCoords                      */
/************************************************************************/

json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY, int nCoordPrecision )
{
    json_object* poObjCoords = NULL;
    if( CPLIsInf(fX) || CPLIsInf(fY) ||
        CPLIsNan(fX) || CPLIsNan(fY) )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Infinite or NaN coordinate encountered");
        return NULL;
    }
    poObjCoords = json_object_new_array();
    json_object_array_add( poObjCoords, json_object_new_double_with_precision( fX, nCoordPrecision ) );
    json_object_array_add( poObjCoords, json_object_new_double_with_precision( fY, nCoordPrecision ) );

    return poObjCoords;
}

json_object* OGRGeoJSONWriteCoords( double const& fX, double const& fY, double const& fZ, int nCoordPrecision )
{
    json_object* poObjCoords = NULL;
    if( CPLIsInf(fX) || CPLIsInf(fY) || CPLIsInf(fZ) ||
        CPLIsNan(fX) || CPLIsNan(fY) || CPLIsNan(fZ) )
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Infinite or NaN coordinate encountered");
        return NULL;
    }
    poObjCoords = json_object_new_array();
    json_object_array_add( poObjCoords, json_object_new_double_with_precision( fX, nCoordPrecision ) );
    json_object_array_add( poObjCoords, json_object_new_double_with_precision( fY, nCoordPrecision ) );
    json_object_array_add( poObjCoords, json_object_new_double_with_precision( fZ, nCoordPrecision ) );

    return poObjCoords;
}

/************************************************************************/
/*                           OGRGeoJSONWriteLineCoords                  */
/************************************************************************/

json_object* OGRGeoJSONWriteLineCoords( OGRLineString* poLine, int nCoordPrecision )
{
    json_object* poObjPoint = NULL;
    json_object* poObjCoords = json_object_new_array();

    const int nCount = poLine->getNumPoints();
    for( int i = 0; i < nCount; ++i )
    {
        if( poLine->getCoordinateDimension() == 2 )
            poObjPoint = OGRGeoJSONWriteCoords( poLine->getX(i), poLine->getY(i), nCoordPrecision );
        else
            poObjPoint = OGRGeoJSONWriteCoords( poLine->getX(i), poLine->getY(i), poLine->getZ(i), nCoordPrecision );
        if( poObjPoint == NULL )
        {
            json_object_put(poObjCoords);
            return NULL;
        }
        json_object_array_add( poObjCoords, poObjPoint );
    }
    
    return poObjCoords;
}

/************************************************************************/
/*                           OGR_G_ExportToJson                         */
/************************************************************************/

/**
 * \brief Convert a geometry into GeoJSON format.
 *
 * The returned string should be freed with CPLFree() when no longer required.
 *
 * This method is the same as the C++ method OGRGeometry::exportToJson().
 *
 * @param hGeometry handle to the geometry.
 * @return A GeoJSON fragment or NULL in case of error.
 */

char* OGR_G_ExportToJson( OGRGeometryH hGeometry )
{
    return OGR_G_ExportToJsonEx(hGeometry, NULL);
}

/************************************************************************/
/*                           OGR_G_ExportToJsonEx                       */
/************************************************************************/

/**
 * \brief Convert a geometry into GeoJSON format.
 *
 * The returned string should be freed with CPLFree() when no longer required.
 *
 * This method is the same as the C++ method OGRGeometry::exportToJson().
 *
 * @param hGeometry handle to the geometry.
 * @param papszOptions a null terminated list of options. For now, only COORDINATE_PRECISION=int_number
 *                     where int_number is the maximum number of figures after decimal separator to write in coordinates.
 * @return A GeoJSON fragment or NULL in case of error.
 *
 * @since OGR 1.9.0
 */

char* OGR_G_ExportToJsonEx( OGRGeometryH hGeometry, char** papszOptions )
{
    VALIDATE_POINTER1( hGeometry, "OGR_G_ExportToJson", NULL );

    OGRGeometry* poGeometry = (OGRGeometry*) (hGeometry);

    int nCoordPrecision = atoi(CSLFetchNameValueDef(papszOptions, "COORDINATE_PRECISION", "-1"));

    json_object* poObj = NULL;
    poObj = OGRGeoJSONWriteGeometry( poGeometry, nCoordPrecision );

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

/************************************************************************/
/*               OGR_json_double_with_precision_to_string()             */
/************************************************************************/

static int OGR_json_double_with_precision_to_string(struct json_object *jso,
                                                    struct printbuf *pb,
                                                    int level,
                                                    int flags)
{
    char szBuffer[75]; 
    int nPrecision = (int) (size_t) jso->_userdata;
    OGRFormatDouble( szBuffer, sizeof(szBuffer), jso->o.c_double, '.', 
                     (nPrecision < 0) ? 15 : nPrecision ); 
    if( szBuffer[0] == 't' /*oobig */ )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.18g", jso->o.c_double);
    }
    return printbuf_memappend(pb, szBuffer, strlen(szBuffer)); 
}

/************************************************************************/
/*                   json_object_new_double_with_precision()            */
/************************************************************************/

json_object* json_object_new_double_with_precision(double dfVal,
                                                   int nCoordPrecision)
{
    json_object* jso = json_object_new_double(dfVal);
    json_object_set_serializer(jso, OGR_json_double_with_precision_to_string,
                               (void*)(size_t)nCoordPrecision, NULL );
    return jso;
}
