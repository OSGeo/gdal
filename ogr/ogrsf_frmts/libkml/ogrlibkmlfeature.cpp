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

#include <ogrsf_frmts.h>
#include <ogr_geometry.h>

#include <kml/dom.h>

using kmldom::KmlFactory;
using kmldom::PlacemarkPtr;
using kmldom::ElementPtr;
using kmldom::GeometryPtr;
using kmldom::Geometry;
using kmldom::GroundOverlayPtr;
using kmldom::CameraPtr;

#include "ogr_libkml.h"

#include "ogrlibkmlgeometry.h"
#include "ogrlibkmlfield.h"
#include "ogrlibkmlfeaturestyle.h"

PlacemarkPtr feat2kml (
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeature * poOgrFeat,
    KmlFactory * poKmlFactory )
{

    PlacemarkPtr poKmlPlacemark = poKmlFactory->CreatePlacemark (  );

    /***** style *****/

    featurestyle2kml ( poOgrDS, poOgrLayer, poOgrFeat, poKmlFactory,
                       poKmlPlacemark );

    /***** geometry *****/

    OGRGeometry *poOgrGeom = poOgrFeat->GetGeometryRef (  );
    int iHeading = poOgrFeat->GetFieldIndex("heading"),
        iTilt = poOgrFeat->GetFieldIndex("tilt"),
        iRoll = poOgrFeat->GetFieldIndex("roll");

    if( poOgrGeom != NULL && !poOgrGeom->IsEmpty() &&
        wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint &&
        ((iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading)) ||
         (iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt)) ||
         (iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll))) )
    {
        OGRPoint* poOgrPoint = (OGRPoint*) poOgrGeom;
        CameraPtr camera = poKmlFactory->CreateCamera();
        camera->set_latitude(poOgrPoint->getY());
        camera->set_longitude(poOgrPoint->getX());
        if( poOgrPoint->getCoordinateDimension() == 3 )
            camera->set_altitude(poOgrPoint->getZ());
        if( iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading) )
            camera->set_heading(poOgrFeat->GetFieldAsDouble(iHeading));
        if( iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt) )
            camera->set_tilt(poOgrFeat->GetFieldAsDouble(iTilt));
        if( iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll) )
            camera->set_roll(poOgrFeat->GetFieldAsDouble(iRoll));
        poKmlPlacemark->set_abstractview(camera);
    }
    else
    {
        ElementPtr poKmlElement = geom2kml ( poOgrGeom, -1, 0, poKmlFactory );

        poKmlPlacemark->set_geometry ( AsGeometry ( poKmlElement ) );
    }

    /***** fields *****/

    field2kml ( poOgrFeat, ( OGRLIBKMLLayer * ) poOgrLayer, poKmlFactory,
                poKmlPlacemark );



    return poKmlPlacemark;
}

OGRFeature *kml2feat (
    PlacemarkPtr poKmlPlacemark,
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeatureDefn * poOgrFeatDefn,
    OGRSpatialReference *poOgrSRS)
{

    OGRFeature *poOgrFeat = new OGRFeature ( poOgrFeatDefn );

    /***** style *****/

    kml2featurestyle ( poKmlPlacemark, poOgrDS, poOgrLayer, poOgrFeat );

    /***** geometry *****/

    if ( poKmlPlacemark->has_geometry (  ) ) {
        OGRGeometry *poOgrGeom =
            kml2geom ( poKmlPlacemark->get_geometry (  ), poOgrSRS );
        poOgrFeat->SetGeometryDirectly ( poOgrGeom );

    }
    else if ( poKmlPlacemark->has_abstractview (  ) &&
              poKmlPlacemark->get_abstractview()->IsA( kmldom::Type_Camera) ) {
        const CameraPtr& camera = AsCamera(poKmlPlacemark->get_abstractview());
        if( camera->has_longitude() && camera->has_latitude() )
        {
            if( camera->has_altitude() )
                poOgrFeat->SetGeometryDirectly( new OGRPoint( camera->get_longitude(), camera->get_latitude(), camera->get_altitude() ) );
            else
                poOgrFeat->SetGeometryDirectly( new OGRPoint( camera->get_longitude(), camera->get_latitude() ) );
            poOgrFeat->GetGeometryRef()->assignSpatialReference( poOgrSRS );
        }
    }

    /***** fields *****/

    kml2field ( poOgrFeat, AsFeature ( poKmlPlacemark ) );

    return poOgrFeat;
}

OGRFeature *kmlgroundoverlay2feat (
    GroundOverlayPtr poKmlOverlay,
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeatureDefn * poOgrFeatDefn,
    OGRSpatialReference *poOgrSRS)
{

    OGRFeature *poOgrFeat = new OGRFeature ( poOgrFeatDefn );

    /***** style *****/

    //kml2featurestyle ( poKmlPlacemark, poOgrDS, poOgrLayer, poOgrFeat );

    /***** geometry *****/

    if ( poKmlOverlay->has_latlonbox (  ) ) {
        OGRGeometry *poOgrGeom =
            kml2geom_latlonbox ( poKmlOverlay->get_latlonbox (  ), poOgrSRS );
        poOgrFeat->SetGeometryDirectly ( poOgrGeom );

    }
    else if ( poKmlOverlay->has_gx_latlonquad (  ) ) {
        OGRGeometry *poOgrGeom =
            kml2geom_latlonquad ( poKmlOverlay->get_gx_latlonquad (  ), poOgrSRS );
        poOgrFeat->SetGeometryDirectly ( poOgrGeom );

    }

    /***** fields *****/

    kml2field ( poOgrFeat, AsFeature ( poKmlOverlay ) );

    return poOgrFeat;
}
