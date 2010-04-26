/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
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
 *****************************************************************************/

#include <ogr_geometry.h>

#include <kml/dom.h>

using kmldom::KmlFactory;
using kmldom::CoordinatesPtr;
using kmldom::PointPtr;
using kmldom::LineStringPtr;
using kmldom::LinearRingPtr;
using kmldom::OuterBoundaryIsPtr;
using kmldom::InnerBoundaryIsPtr;
using kmldom::PolygonPtr;
using kmldom::MultiGeometryPtr;
using kmldom::GeometryPtr;
using kmldom::ElementPtr;
using kmldom::GeometryPtr;

using kmlbase::Vec3;

#include "ogrlibkmlgeometry.h"

/******************************************************************************
 funtion to write out a ogr geometry to kml

args:
          poOgrGeom     the ogr geometry
          extra         used in recursion, just pass -1
          wkb25D        used in recursion, just pass 0
          poKmlFactory  pointer to the libkml dom factory

returns:
          ElementPtr to the geometry created

******************************************************************************/

ElementPtr geom2kml (
    OGRGeometry * poOgrGeom,
    int extra,
    int wkb25D,
    KmlFactory * poKmlFactory )
{
    int i;

    if ( !poOgrGeom ) {
        return NULL;
    }

    /***** ogr geom vars *****/

    OGRPoint *poOgrPoint = NULL;
    OGRLineString *poOgrLineString;
    OGRPolygon *poOgrPolygon;
    OGRGeometryCollection *poOgrMultiGeom;

    /***** libkml geom vars *****/

    CoordinatesPtr coordinates;
    PointPtr poKmlPoint;
    LineStringPtr poKmlLineString;
    LinearRingPtr poKmlLinearRing;
    OuterBoundaryIsPtr poKmlOuterRing;
    InnerBoundaryIsPtr poKmlInnerRing;
    PolygonPtr poKmlPolygon;
    MultiGeometryPtr poKmlMultiGeometry;

    ElementPtr poKmlGeometry;
    ElementPtr poKmlTmpGeometry;

    /***** other vars *****/

    double x,
        y,
        z;

    int numpoints = 0;
    int nGeom;
    int type = poOgrGeom->getGeometryType (  );

    wkb25D = type & wkb25DBit;

    switch ( type ) {

    case wkbPoint:

        poOgrPoint = ( OGRPoint * ) poOgrGeom;

        x = poOgrPoint->getX (  );
        y = poOgrPoint->getY (  );

        if ( x > 180 )
            x -= 360;

        coordinates = poKmlFactory->CreateCoordinates (  );
        coordinates->add_latlng ( y, x );
        poKmlGeometry = poKmlPoint = poKmlFactory->CreatePoint (  );
        poKmlPoint->set_coordinates ( coordinates );

        break;

    case wkbPoint25D:
        poOgrPoint = ( OGRPoint * ) poOgrGeom;

        x = poOgrPoint->getX (  );
        y = poOgrPoint->getY (  );
        z = poOgrPoint->getZ (  );

        if ( x > 180 )
            x -= 360;

        coordinates = poKmlFactory->CreateCoordinates (  );
        coordinates->add_latlngalt ( y, x, z );
        poKmlGeometry = poKmlPoint = poKmlFactory->CreatePoint (  );
        poKmlPoint->set_coordinates ( coordinates );

        break;

    case wkbLineString:
        poOgrLineString = ( OGRLineString * ) poOgrGeom;

        coordinates = poKmlFactory->CreateCoordinates (  );

        numpoints = poOgrLineString->getNumPoints (  );
        poOgrPoint = new OGRPoint (  );

        for ( i = 0; i < numpoints; i++ ) {
            poOgrLineString->getPoint ( i, poOgrPoint );

            x = poOgrPoint->getX (  );
            y = poOgrPoint->getY (  );

            coordinates->add_latlng ( y, x );
        }
        delete poOgrPoint;

        /***** check if its a wkbLinearRing *****/

        if ( extra < 0 ) {

            poKmlGeometry = poKmlLineString =
                poKmlFactory->CreateLineString (  );
            poKmlLineString->set_coordinates ( coordinates );

            break;
        }

      /***** fallthough *****/

    case wkbLinearRing:        //this case is for readability only

        poKmlLinearRing = poKmlFactory->CreateLinearRing (  );
        poKmlLinearRing->set_coordinates ( coordinates );

        if ( !extra ) {
            poKmlOuterRing = poKmlFactory->CreateOuterBoundaryIs (  );
            poKmlOuterRing->set_linearring ( poKmlLinearRing );
            poKmlGeometry = poKmlOuterRing;
        }
        else {
            poKmlGeometry = poKmlInnerRing =
                poKmlFactory->CreateInnerBoundaryIs (  );
            poKmlInnerRing->set_linearring ( poKmlLinearRing );
        }

    case wkbLineString25D:

        poOgrLineString = ( OGRLineString * ) poOgrGeom;

        coordinates = poKmlFactory->CreateCoordinates (  );
        poOgrPoint = new OGRPoint (  );
        numpoints = poOgrLineString->getNumPoints (  );
        for ( i = 0; i < numpoints; i++ ) {
            poOgrLineString->getPoint ( i, poOgrPoint );

            x = poOgrPoint->getX (  );
            y = poOgrPoint->getY (  );
            z = poOgrPoint->getZ (  );

            coordinates->add_latlngalt ( y, x, z );
        }
        delete poOgrPoint;

        /***** check if its a wkbLinearRing *****/

        if ( extra < 0 ) {

            poKmlGeometry = poKmlLineString =
                poKmlFactory->CreateLineString (  );
            poKmlLineString->set_coordinates ( coordinates );

            break;
        }
            /***** fallthough *****/

        //case wkbLinearRing25D: // this case is for readability only

        poKmlLinearRing = poKmlFactory->CreateLinearRing (  );
        poKmlLinearRing->set_coordinates ( coordinates );

        if ( !extra ) {
            poKmlGeometry = poKmlOuterRing =
                poKmlFactory->CreateOuterBoundaryIs (  );
            poKmlOuterRing->set_linearring ( poKmlLinearRing );
        }
        else {
            poKmlGeometry = poKmlInnerRing =
                poKmlFactory->CreateInnerBoundaryIs (  );
            poKmlInnerRing->set_linearring ( poKmlLinearRing );
        }

        break;

    case wkbPolygon:

        poOgrPolygon = ( OGRPolygon * ) poOgrGeom;

        poKmlGeometry = poKmlPolygon = poKmlFactory->CreatePolygon (  );

        poKmlTmpGeometry = geom2kml ( poOgrPolygon->getExteriorRing (  ),
                                      0, wkb25D, poKmlFactory );
        poKmlPolygon->
            set_outerboundaryis ( AsOuterBoundaryIs ( poKmlTmpGeometry ) );

        nGeom = poOgrPolygon->getNumInteriorRings (  );
        for ( i = 0; i < nGeom; i++ ) {
            poKmlTmpGeometry = geom2kml ( poOgrPolygon->getInteriorRing ( i ),
                                          i + 1, wkb25D, poKmlFactory );
            poKmlPolygon->
                add_innerboundaryis ( AsInnerBoundaryIs ( poKmlTmpGeometry ) );
        }

        break;

    case wkbPolygon25D:

        poOgrPolygon = ( OGRPolygon * ) poOgrGeom;

        poKmlGeometry = poKmlPolygon = poKmlFactory->CreatePolygon (  );

        poKmlTmpGeometry = geom2kml ( poOgrPolygon->getExteriorRing (  ),
                                      0, wkb25D, poKmlFactory );
        poKmlPolygon->
            set_outerboundaryis ( AsOuterBoundaryIs ( poKmlTmpGeometry ) );

        nGeom = poOgrPolygon->getNumInteriorRings (  );
        for ( i = 0; i < nGeom; i++ ) {
            poKmlTmpGeometry = geom2kml ( poOgrPolygon->getInteriorRing ( i ),
                                          i + 1, wkb25D, poKmlFactory );
            poKmlPolygon->
                add_innerboundaryis ( AsInnerBoundaryIs ( poKmlTmpGeometry ) );
        }

        break;

    case wkbMultiPoint:
    case wkbMultiLineString:
    case wkbMultiPolygon:
    case wkbGeometryCollection:
    case wkbMultiPoint25D:
    case wkbMultiLineString25D:
    case wkbMultiPolygon25D:
    case wkbGeometryCollection25D:

        poOgrMultiGeom = ( OGRGeometryCollection * ) poOgrGeom;

        poKmlGeometry = poKmlMultiGeometry =
            poKmlFactory->CreateMultiGeometry (  );

        nGeom = poOgrMultiGeom->getNumGeometries (  );
        for ( i = 0; i < nGeom; i++ ) {
            poKmlTmpGeometry = geom2kml ( poOgrMultiGeom->getGeometryRef ( i ),
                                          -1, wkb25D, poKmlFactory );
            poKmlMultiGeometry->
                add_geometry ( AsGeometry ( poKmlTmpGeometry ) );
        }

        break;

    case wkbUnknown:
    case wkbNone:
    default:
        break;

    }

    return poKmlGeometry;
}

/******************************************************************************
 function to read a kml geometry and translate to ogr

Args:
            poKmlGeometry   pointer to the kml geometry to translate

Returns:
            pointer to the new ogr geometry object

******************************************************************************/

OGRGeometry *kml2geom (
    GeometryPtr poKmlGeometry )
{

    /***** ogr geom vars *****/

    OGRPoint *poOgrPoint;
    OGRLineString *poOgrLineString;
    OGRLinearRing *poOgrLinearRing;
    OGRPolygon *poOgrPolygon;
    OGRGeometryCollection *poOgrMultiGeometry;
    OGRGeometry *poOgrGeometry = NULL;
    OGRGeometry *poOgrTmpGeometry = NULL;


    /***** libkml geom vars *****/

    CoordinatesPtr poKmlCoordinates;
    PointPtr poKmlPoint;
    LineStringPtr poKmlLineString;
    LinearRingPtr poKmlLinearRing;
    OuterBoundaryIsPtr poKmlOuterRing;
    InnerBoundaryIsPtr poKmlInnerRing;
    PolygonPtr poKmlPolygon;
    MultiGeometryPtr poKmlMultiGeometry;
    GeometryPtr poKmlTmpGeometry;

    Vec3 oKmlVec;

    size_t nRings,
        nCoords,
        nGeom,
        i;

    switch ( poKmlGeometry->Type (  ) ) {
    case kmldom::Type_Point:
        poKmlPoint = AsPoint ( poKmlGeometry );
        if ( poKmlPoint->has_coordinates (  ) ) {
            poKmlCoordinates = poKmlPoint->get_coordinates (  );
            oKmlVec = poKmlCoordinates->get_coordinates_array_at ( 0 );

            if ( oKmlVec.has_altitude (  ) )
                poOgrPoint = new OGRPoint ( oKmlVec.get_longitude (  ),
                                            oKmlVec.get_latitude (  ),
                                            oKmlVec.get_altitude (  ) );
            else
                poOgrPoint = new OGRPoint ( oKmlVec.get_longitude (  ),
                                            oKmlVec.get_latitude (  ) );

            poOgrGeometry = poOgrPoint;
        }

        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );
        if ( poKmlLineString->has_coordinates (  ) ) {
            poKmlCoordinates = poKmlLineString->get_coordinates (  );

            poOgrLineString = new OGRLineString (  );

            nCoords = poKmlCoordinates->get_coordinates_array_size (  );
            for ( i = 0; i < nCoords; i++ ) {
                oKmlVec = poKmlCoordinates->get_coordinates_array_at ( i );
                if ( oKmlVec.has_altitude (  ) )
                    poOgrLineString->
                        addPoint ( oKmlVec.get_longitude (  ),
                                   oKmlVec.get_latitude (  ),
                                   oKmlVec.get_altitude (  ) );
                else
                    poOgrLineString->
                        addPoint ( oKmlVec.get_longitude (  ),
                                   oKmlVec.get_latitude (  ) );

                poOgrGeometry = poOgrLineString;
            }
        }

        break;
    case kmldom::Type_LinearRing:
        poKmlLinearRing = AsLinearRing ( poKmlGeometry );
        if ( poKmlLinearRing->has_coordinates (  ) ) {
            poKmlCoordinates = poKmlLinearRing->get_coordinates (  );

            poOgrLinearRing = new OGRLinearRing (  );

            nCoords = poKmlCoordinates->get_coordinates_array_size (  );
            for ( i = 0; i < nCoords; i++ ) {
                oKmlVec = poKmlCoordinates->get_coordinates_array_at ( i );
                if ( oKmlVec.has_altitude (  ) )
                    poOgrLinearRing->
                        addPoint ( oKmlVec.get_longitude (  ),
                                   oKmlVec.get_latitude (  ),
                                   oKmlVec.get_altitude (  ) );
                else
                    poOgrLinearRing->
                        addPoint ( oKmlVec.get_longitude (  ),
                                   oKmlVec.get_latitude (  ) );

                poOgrGeometry = poOgrLinearRing;
            }
        }

        break;
    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        poOgrPolygon = new OGRPolygon (  );
        if ( poKmlPolygon->has_outerboundaryis (  ) ) {

            poKmlOuterRing = poKmlPolygon->get_outerboundaryis (  );
            poKmlLinearRing = poKmlOuterRing->get_linearring (  );
            poOgrTmpGeometry = kml2geom ( poKmlLinearRing );

            poOgrPolygon->
                addRingDirectly ( ( OGRLinearRing * ) poOgrTmpGeometry );

        }
        nRings = poKmlPolygon->get_innerboundaryis_array_size (  );
        for ( i = 0; i < nRings; i++ ) {
            poKmlInnerRing = poKmlPolygon->get_innerboundaryis_array_at ( i );
            poKmlLinearRing = poKmlInnerRing->get_linearring (  );
            poOgrTmpGeometry = kml2geom ( poKmlLinearRing );

            poOgrPolygon->
                addRingDirectly ( ( OGRLinearRing * ) poOgrTmpGeometry );
        }
        poOgrGeometry = poOgrPolygon;

        break;
    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        poOgrMultiGeometry = new OGRGeometryCollection (  );
        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            poKmlTmpGeometry = poKmlMultiGeometry->get_geometry_array_at ( i );
            poOgrTmpGeometry = kml2geom ( poKmlTmpGeometry );

            poOgrMultiGeometry->addGeometryDirectly ( poOgrTmpGeometry );
        }
        poOgrGeometry = poOgrMultiGeometry;
        break;
    default:
        break;
    }

    return poOgrGeometry;
}
