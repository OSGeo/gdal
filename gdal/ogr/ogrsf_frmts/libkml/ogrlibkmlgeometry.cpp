/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_p.h"
#include "libkml_headers.h"

using kmldom::KmlFactory;
using kmldom::CoordinatesPtr;
using kmldom::PointPtr;
using kmldom::LatLonBoxPtr;
using kmldom::LineStringPtr;
using kmldom::LinearRingPtr;
using kmldom::OuterBoundaryIsPtr;
using kmldom::InnerBoundaryIsPtr;
using kmldom::PolygonPtr;
using kmldom::MultiGeometryPtr;
using kmldom::GeometryPtr;
using kmldom::ElementPtr;
using kmldom::GeometryPtr;
using kmldom::GxLatLonQuadPtr;
using kmldom::GxTrackPtr;
using kmldom::GxMultiTrackPtr;

using kmlbase::Vec3;

#include "ogrlibkmlgeometry.h"

/******************************************************************************
 function to write out a ogr geometry to kml

args:
          poOgrGeom     the ogr geometry
          extra         used in recursion, just pass -1
          poKmlFactory  pointer to the libkml dom factory

returns:
          ElementPtr to the geometry created

******************************************************************************/

ElementPtr geom2kml (
    OGRGeometry * poOgrGeom,
    int extra,
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
    OGRwkbGeometryType type = poOgrGeom->getGeometryType (  );

    switch ( type ) {

    case wkbPoint:

        poOgrPoint = ( OGRPoint * ) poOgrGeom;
        if (poOgrPoint->getCoordinateDimension() == 0)
        {
            poKmlGeometry = poKmlPoint = poKmlFactory->CreatePoint (  );
        }
        else
        {
            x = poOgrPoint->getX (  );
            y = poOgrPoint->getY (  );

            if ( x > 180 )
                x -= 360;

            coordinates = poKmlFactory->CreateCoordinates (  );
            coordinates->add_latlng ( y, x );
            poKmlGeometry = poKmlPoint = poKmlFactory->CreatePoint (  );
            poKmlPoint->set_coordinates ( coordinates );
        }

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

        if( extra >= 0 )
        {
            ((OGRLinearRing*)poOgrGeom)->closeRings();
        }

        numpoints = poOgrLineString->getNumPoints (  );
        if( extra >= 0 )
        {
            if( numpoints < 4 && 
                CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "A linearring should have at least 4 points");
                return NULL;
            }
        }
        else
        {
            if( numpoints < 2 && 
                CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "A linestring should have at least 2 points");
                return NULL;
            }
        }

        coordinates = poKmlFactory->CreateCoordinates (  );

        poOgrPoint = new OGRPoint (  );

        for ( i = 0; i < numpoints; i++ ) {
            poOgrLineString->getPoint ( i, poOgrPoint );

            x = poOgrPoint->getX (  );
            y = poOgrPoint->getY (  );

            if ( x > 180 )
                x -= 360;

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

      /***** fallthrough *****/

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

        break;

    case wkbLineString25D:

        poOgrLineString = ( OGRLineString * ) poOgrGeom;

        if( extra >= 0 )
        {
            ((OGRLinearRing*)poOgrGeom)->closeRings();
        }

        numpoints = poOgrLineString->getNumPoints (  );
        if( extra >= 0 )
        {
            if( numpoints < 4 && 
                CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "A linearring should have at least 4 points");
                return NULL;
            }
        }
        else
        {
            if( numpoints < 2 && 
                CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
            {
                CPLError(CE_Failure, CPLE_NotSupported, "A linestring should have at least 2 points");
                return NULL;
            }
        }

        coordinates = poKmlFactory->CreateCoordinates (  );
        poOgrPoint = new OGRPoint (  );

        for ( i = 0; i < numpoints; i++ ) {
            poOgrLineString->getPoint ( i, poOgrPoint );

            x = poOgrPoint->getX (  );
            y = poOgrPoint->getY (  );
            z = poOgrPoint->getZ (  );

            if ( x > 180 )
                x -= 360;

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
            /***** fallthrough *****/

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

        CPLErrorReset();
        if( CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) &&
            OGRGeometryFactory::haveGEOS() && (!poOgrGeom->IsValid() ||
             CPLGetLastErrorType() != CE_None) )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Invalid polygon");
            return NULL;
        }
        poOgrPolygon = ( OGRPolygon * ) poOgrGeom;

        poKmlGeometry = poKmlPolygon = poKmlFactory->CreatePolygon (  );

        poKmlTmpGeometry = geom2kml ( poOgrPolygon->getExteriorRing (  ),
                                      0, poKmlFactory );
        poKmlPolygon->
            set_outerboundaryis ( AsOuterBoundaryIs ( poKmlTmpGeometry ) );

        nGeom = poOgrPolygon->getNumInteriorRings (  );
        for ( i = 0; i < nGeom; i++ ) {
            poKmlTmpGeometry = geom2kml ( poOgrPolygon->getInteriorRing ( i ),
                                          i + 1, poKmlFactory );
            poKmlPolygon->
                add_innerboundaryis ( AsInnerBoundaryIs ( poKmlTmpGeometry ) );
        }

        break;

    case wkbPolygon25D:

        CPLErrorReset();
        if( CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) &&
            OGRGeometryFactory::haveGEOS() && (!poOgrGeom->IsValid() ||
             CPLGetLastErrorType() != CE_None) )
        {
            CPLError(CE_Failure, CPLE_NotSupported, "Invalid polygon");
            return NULL;
        }
        poOgrPolygon = ( OGRPolygon * ) poOgrGeom;

        poKmlGeometry = poKmlPolygon = poKmlFactory->CreatePolygon (  );

        poKmlTmpGeometry = geom2kml ( poOgrPolygon->getExteriorRing (  ),
                                      0, poKmlFactory );
        poKmlPolygon->
            set_outerboundaryis ( AsOuterBoundaryIs ( poKmlTmpGeometry ) );

        nGeom = poOgrPolygon->getNumInteriorRings (  );
        for ( i = 0; i < nGeom; i++ ) {
            poKmlTmpGeometry = geom2kml ( poOgrPolygon->getInteriorRing ( i ),
                                          i + 1, poKmlFactory );
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

        nGeom = poOgrMultiGeom->getNumGeometries (  );

        if( nGeom == 1 && 
            CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
        {
            CPLDebug("LIBKML", "Turning multiple geometry into single geometry");
            poKmlGeometry = geom2kml( poOgrMultiGeom->getGeometryRef ( 0 ),
                                      -1, poKmlFactory );
        }
        else
        {
            if( nGeom == 0 && 
                CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Empty multi geometry are not recommended");
            }
            poKmlGeometry = poKmlMultiGeometry =
                poKmlFactory->CreateMultiGeometry (  );
            for ( i = 0; i < nGeom; i++ ) {
                poKmlTmpGeometry = geom2kml ( poOgrMultiGeom->getGeometryRef ( i ),
                                            -1, poKmlFactory );
                poKmlMultiGeometry->
                    add_geometry ( AsGeometry ( poKmlTmpGeometry ) );
            }
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
 recursive function to read a kml geometry and translate to ogr

Args:
            poKmlGeometry   pointer to the kml geometry to translate
            poOgrSRS        pointer to the spatial ref to set on the geometry 

Returns:
            pointer to the new ogr geometry object

******************************************************************************/

static OGRGeometry *kml2geom_rec (
    GeometryPtr poKmlGeometry,
    OGRSpatialReference *poOgrSRS)

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
    GxTrackPtr poKmlGxTrack;
    GxMultiTrackPtr poKmlGxMultiTrack;
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
            nCoords = poKmlCoordinates->get_coordinates_array_size (  );
            if (nCoords > 0)
            {
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
            else
            {
                poOgrGeometry = new OGRPoint();
            }
        }
        else
        {
            poOgrGeometry = new OGRPoint();
        }

        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );
        poOgrLineString = new OGRLineString (  );
        if ( poKmlLineString->has_coordinates (  ) ) {
            poKmlCoordinates = poKmlLineString->get_coordinates (  );

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
            }
        }
        poOgrGeometry = poOgrLineString;

        break;
    case kmldom::Type_LinearRing:
        poKmlLinearRing = AsLinearRing ( poKmlGeometry );
        poOgrLinearRing = new OGRLinearRing (  );
        if ( poKmlLinearRing->has_coordinates (  ) ) {
            poKmlCoordinates = poKmlLinearRing->get_coordinates (  );

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
            }
        }
        poOgrGeometry = poOgrLinearRing;

        break;
    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        poOgrPolygon = new OGRPolygon (  );
        if ( poKmlPolygon->has_outerboundaryis (  ) ) {

            poKmlOuterRing = poKmlPolygon->get_outerboundaryis (  );
            poKmlLinearRing = poKmlOuterRing->get_linearring (  );
            if (poKmlLinearRing)
            {
                poOgrTmpGeometry = kml2geom_rec ( poKmlLinearRing, poOgrSRS );

                poOgrPolygon->
                    addRingDirectly ( ( OGRLinearRing * ) poOgrTmpGeometry );
            }

        }
        nRings = poKmlPolygon->get_innerboundaryis_array_size (  );
        for ( i = 0; i < nRings; i++ ) {
            poKmlInnerRing = poKmlPolygon->get_innerboundaryis_array_at ( i );
            poKmlLinearRing = poKmlInnerRing->get_linearring (  );
            if (poKmlLinearRing)
            {
                poOgrTmpGeometry = kml2geom_rec ( poKmlLinearRing, poOgrSRS );

                poOgrPolygon->
                    addRingDirectly ( ( OGRLinearRing * ) poOgrTmpGeometry );
            }
        }
        poOgrGeometry = poOgrPolygon;

        break;
    case kmldom::Type_MultiGeometry:
    {
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );
        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );

        // Detect subgeometry type to instantiate appropriate
        // multi geometry type.
        kmldom::KmlDomType type = kmldom::Type_Unknown;
        for ( i = 0; i < nGeom; i++ ) {
            poKmlTmpGeometry = poKmlMultiGeometry->get_geometry_array_at ( i );
            if (type == kmldom::Type_Unknown)
                type = poKmlTmpGeometry->Type();
            else if (type != poKmlTmpGeometry->Type())
            {
                type = kmldom::Type_Unknown;
                break;
            }
        }

        if (type == kmldom::Type_Point)
            poOgrMultiGeometry = new OGRMultiPoint();
        else if (type == kmldom::Type_LineString)
            poOgrMultiGeometry = new OGRMultiLineString();
        else if (type == kmldom::Type_Polygon)
            poOgrMultiGeometry = new OGRMultiPolygon();
        else
            poOgrMultiGeometry = new OGRGeometryCollection ();

        for ( i = 0; i < nGeom; i++ ) {
            poKmlTmpGeometry = poKmlMultiGeometry->get_geometry_array_at ( i );
            poOgrTmpGeometry = kml2geom_rec ( poKmlTmpGeometry, poOgrSRS );

            poOgrMultiGeometry->addGeometryDirectly ( poOgrTmpGeometry );
        }
        poOgrGeometry = poOgrMultiGeometry;
        break;
    }

    case kmldom::Type_GxTrack:
        poKmlGxTrack = AsGxTrack ( poKmlGeometry );
        nCoords = poKmlGxTrack->get_gx_coord_array_size();
        poOgrLineString = new OGRLineString (  );
        for ( i = 0; i < nCoords; i++ ) {
            oKmlVec = poKmlGxTrack->get_gx_coord_array_at ( i );
            if ( oKmlVec.has_altitude (  ) )
                poOgrLineString->
                    addPoint ( oKmlVec.get_longitude (  ),
                                oKmlVec.get_latitude (  ),
                                oKmlVec.get_altitude (  ) );
            else
                poOgrLineString->
                    addPoint ( oKmlVec.get_longitude (  ),
                                oKmlVec.get_latitude (  ) );
        }
        poOgrGeometry = poOgrLineString;
        break;

    case kmldom::Type_GxMultiTrack:
    {
        poKmlGxMultiTrack = AsGxMultiTrack ( poKmlGeometry );
        nGeom = poKmlGxMultiTrack->get_gx_track_array_size (  );
        poOgrMultiGeometry = new OGRMultiLineString();
        for( size_t j = 0; j < nGeom; j++ )
        {
            poKmlGxTrack = poKmlGxMultiTrack->get_gx_track_array_at ( j );
            nCoords = poKmlGxTrack->get_gx_coord_array_size();
            poOgrLineString = new OGRLineString (  );
            for ( i = 0; i < nCoords; i++ ) {
                oKmlVec = poKmlGxTrack->get_gx_coord_array_at ( i );
                if ( oKmlVec.has_altitude (  ) )
                    poOgrLineString->
                        addPoint ( oKmlVec.get_longitude (  ),
                                    oKmlVec.get_latitude (  ),
                                    oKmlVec.get_altitude (  ) );
                else
                    poOgrLineString->
                        addPoint ( oKmlVec.get_longitude (  ),
                                    oKmlVec.get_latitude (  ) );
            }
            poOgrMultiGeometry->addGeometryDirectly(poOgrLineString);
        }
        poOgrGeometry = poOgrMultiGeometry;
        break;
    }

    default:
        break;
    }

    if (poOgrGeometry)
        poOgrGeometry->assignSpatialReference(poOgrSRS);

    return poOgrGeometry;
}

static
OGRGeometry *kml2geom_latlonbox_int (
    LatLonBoxPtr poKmlLatLonBox,
    OGRSpatialReference *poOgrSRS)

{
    OGRPolygon *poOgrPolygon;
    double north, south, east, west;
    if ( !poKmlLatLonBox->has_north (  ) ||
         !poKmlLatLonBox->has_south (  ) ||
         !poKmlLatLonBox->has_east (  ) ||
         !poKmlLatLonBox->has_west (  ) ) {

        return NULL;
    }
    poOgrPolygon = new OGRPolygon (  );
    north = poKmlLatLonBox->get_north (  );
    south = poKmlLatLonBox->get_south (  );
    east = poKmlLatLonBox->get_east (  );
    west = poKmlLatLonBox->get_west (  );
    OGRLinearRing* poOgrRing = new OGRLinearRing (  );
    poOgrRing->addPoint ( east, north, 0.0 );
    poOgrRing->addPoint ( east, south, 0.0 );
    poOgrRing->addPoint ( west, south, 0.0 );
    poOgrRing->addPoint ( west, north, 0.0 );
    poOgrRing->addPoint ( east, north, 0.0 );
    poOgrPolygon->
        addRingDirectly ( poOgrRing );
    poOgrPolygon->assignSpatialReference(poOgrSRS);

    return poOgrPolygon;
}

static
OGRGeometry *kml2geom_latlonquad_int (
    GxLatLonQuadPtr poKmlLatLonQuad,
    OGRSpatialReference *poOgrSRS)

{
    if( !poKmlLatLonQuad->has_coordinates() )
        return NULL;

    const CoordinatesPtr& poKmlCoordinates =
        poKmlLatLonQuad->get_coordinates();

    OGRLinearRing* poOgrLinearRing = new OGRLinearRing (  );

    size_t nCoords = poKmlCoordinates->get_coordinates_array_size (  );
    for ( size_t i = 0; i < nCoords; i++ ) {
        Vec3 oKmlVec = poKmlCoordinates->get_coordinates_array_at ( i );
        if ( oKmlVec.has_altitude (  ) )
            poOgrLinearRing->
                addPoint ( oKmlVec.get_longitude (  ),
                           oKmlVec.get_latitude (  ),
                           oKmlVec.get_altitude (  ) );
        else
            poOgrLinearRing->
                addPoint ( oKmlVec.get_longitude (  ),
                           oKmlVec.get_latitude (  ) );
    }
    poOgrLinearRing->closeRings();

    OGRPolygon *poOgrPolygon = new OGRPolygon();
    poOgrPolygon->
        addRingDirectly ( poOgrLinearRing );
    poOgrPolygon->assignSpatialReference(poOgrSRS);

    return poOgrPolygon;
}

/******************************************************************************
 main function to read a kml geometry and translate to ogr

Args:
            poKmlGeometry   pointer to the kml geometry to translate
            poOgrSRS        pointer to the spatial ref to set on the geometry 

Returns:
            pointer to the new ogr geometry object

******************************************************************************/

OGRGeometry *kml2geom (
    GeometryPtr poKmlGeometry,
    OGRSpatialReference *poOgrSRS)

{

    /***** get the geometry *****/

    OGRGeometry *poOgrGeometry = kml2geom_rec (poKmlGeometry, poOgrSRS);

    /***** split the geometry at the dateline? *****/

    const char *pszWrap = CPLGetConfigOption ( "LIBKML_WRAPDATELINE", "no" );
    if (CSLTestBoolean(pszWrap)) {

        char **papszTransformOptions = NULL;
        papszTransformOptions = CSLAddString( papszTransformOptions,
                                                "WRAPDATELINE=YES");

        /***** transform *****/

        OGRGeometry *poOgrDstGeometry = 
            OGRGeometryFactory::transformWithOptions(poOgrGeometry,
                                                        NULL,
                                                        papszTransformOptions);

        /***** replace the original geom *****/

        if (poOgrDstGeometry) {
            delete poOgrGeometry;
            poOgrGeometry = poOgrDstGeometry;
        }

        CSLDestroy(papszTransformOptions);
    }

    return poOgrGeometry;
}

OGRGeometry *kml2geom_latlonbox (
    LatLonBoxPtr poKmlLatLonBox,
    OGRSpatialReference *poOgrSRS)

{

    /***** get the geometry *****/

    OGRGeometry *poOgrGeometry = kml2geom_latlonbox_int (poKmlLatLonBox, poOgrSRS);

    /***** split the geometry at the dateline? *****/

    const char *pszWrap = CPLGetConfigOption ( "LIBKML_WRAPDATELINE", "no" );
    if (CSLTestBoolean(pszWrap)) {

        char **papszTransformOptions = NULL;
        papszTransformOptions = CSLAddString( papszTransformOptions,
                                                "WRAPDATELINE=YES");

        /***** transform *****/

        OGRGeometry *poOgrDstGeometry = 
            OGRGeometryFactory::transformWithOptions(poOgrGeometry,
                                                        NULL,
                                                        papszTransformOptions);

        /***** replace the original geom *****/

        if (poOgrDstGeometry) {
            delete poOgrGeometry;
            poOgrGeometry = poOgrDstGeometry;
        }

        CSLDestroy(papszTransformOptions);
    }

    return poOgrGeometry;
}

OGRGeometry *kml2geom_latlonquad (
    GxLatLonQuadPtr poKmlLatLonQuad,
    OGRSpatialReference *poOgrSRS)

{

    /***** get the geometry *****/

    OGRGeometry *poOgrGeometry = kml2geom_latlonquad_int (poKmlLatLonQuad, poOgrSRS);

    /***** split the geometry at the dateline? *****/

    const char *pszWrap = CPLGetConfigOption ( "LIBKML_WRAPDATELINE", "no" );
    if (CSLTestBoolean(pszWrap)) {

        char **papszTransformOptions = NULL;
        papszTransformOptions = CSLAddString( papszTransformOptions,
                                                "WRAPDATELINE=YES");

        /***** transform *****/

        OGRGeometry *poOgrDstGeometry = 
            OGRGeometryFactory::transformWithOptions(poOgrGeometry,
                                                        NULL,
                                                        papszTransformOptions);

        /***** replace the original geom *****/

        if (poOgrDstGeometry) {
            delete poOgrGeometry;
            poOgrGeometry = poOgrDstGeometry;
        }

        CSLDestroy(papszTransformOptions);
    }

    return poOgrGeometry;
}
