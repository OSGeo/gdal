/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2014, Even Rouault <even dot rouault at mines-paris dot org>
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
using kmldom::ModelPtr;
using kmldom::LinkPtr;
using kmldom::LocationPtr;
using kmldom::OrientationPtr;
using kmldom::ScalePtr;
using kmldom::ResourceMapPtr;
using kmldom::AliasPtr;

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
    
    struct fieldconfig oFC;
    get_fieldconfig( &oFC );

    /***** style *****/

    featurestyle2kml ( poOgrDS, poOgrLayer, poOgrFeat, poKmlFactory,
                       poKmlPlacemark );

    /***** geometry *****/

    OGRGeometry *poOgrGeom = poOgrFeat->GetGeometryRef (  );
    int iHeading = poOgrFeat->GetFieldIndex(oFC.headingfield),
        iTilt = poOgrFeat->GetFieldIndex(oFC.tiltfield),
        iRoll = poOgrFeat->GetFieldIndex(oFC.rollfield),
        iModel = poOgrFeat->GetFieldIndex(oFC.modelfield);

    /* Model */
    if( iModel>= 0 && poOgrFeat->IsFieldSet(iModel) &&
        poOgrGeom != NULL && !poOgrGeom->IsEmpty() &&
        wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint )
    {
        OGRPoint* poOgrPoint = (OGRPoint*) poOgrGeom;
        ModelPtr model = poKmlFactory->CreateModel();

        LocationPtr location = poKmlFactory->CreateLocation();
        model->set_location(location);
        location->set_latitude(poOgrPoint->getY());
        location->set_longitude(poOgrPoint->getX());
        if( poOgrPoint->getCoordinateDimension() == 3 )
            location->set_altitude(poOgrPoint->getZ());

        int isGX = FALSE;
        int iAltitudeMode = poOgrFeat->GetFieldIndex(oFC.altitudeModefield);
        int nAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;
        if( iAltitudeMode >= 0 && poOgrFeat->IsFieldSet(iAltitudeMode) )
        {
            nAltitudeMode = kmlAltitudeModeFromString(
                poOgrFeat->GetFieldAsString(iAltitudeMode), isGX);
            model->set_altitudemode(nAltitudeMode);

            /* ATC 55 */
            if( nAltitudeMode != kmldom::ALTITUDEMODE_CLAMPTOGROUND &&
                poOgrPoint->getCoordinateDimension() != 3 )
            {
                if( CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
                    CPLError(CE_Warning, CPLE_AppDefined, "Altitude should be defined");
            }
        }

        if( (iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading)) ||
            (iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt)) ||
            (iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll)) )
        {
            OrientationPtr orientation = poKmlFactory->CreateOrientation();
            model->set_orientation(orientation);
            if( iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading) )
                orientation->set_heading(poOgrFeat->GetFieldAsDouble(iHeading));
            else
                orientation->set_heading(0);
            if( iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt) )
                orientation->set_tilt(poOgrFeat->GetFieldAsDouble(iTilt));
            else
                orientation->set_tilt(0);
            if( iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll) )
                orientation->set_roll(poOgrFeat->GetFieldAsDouble(iRoll));
            else
                orientation->set_roll(0);
        }
        int iScaleX = poOgrFeat->GetFieldIndex(oFC.scalexfield);
        int iScaleY = poOgrFeat->GetFieldIndex(oFC.scaleyfield);
        int iScaleZ = poOgrFeat->GetFieldIndex(oFC.scalezfield);
        
        ScalePtr scale = poKmlFactory->CreateScale();
        model->set_scale(scale);
        if( iScaleX >= 0 && poOgrFeat->IsFieldSet(iScaleX) )
            scale->set_x(poOgrFeat->GetFieldAsDouble(iScaleX));
        else
            scale->set_x(1.0);
        if( iScaleY >= 0 && poOgrFeat->IsFieldSet(iScaleY) )
            scale->set_y(poOgrFeat->GetFieldAsDouble(iScaleY));
        else
            scale->set_y(1.0);
        if( iScaleZ >= 0 && poOgrFeat->IsFieldSet(iScaleZ) )
            scale->set_z(poOgrFeat->GetFieldAsDouble(iScaleZ));
        else
            scale->set_z(1.0);

        LinkPtr link = poKmlFactory->CreateLink();
        model->set_link(link);
        const char* pszURL = poOgrFeat->GetFieldAsString(oFC.modelfield);
        link->set_href( pszURL );

        /* Collada 3D file ? */
        if( EQUAL(CPLGetExtension(pszURL), "dae") &&
            CSLTestBoolean(CPLGetConfigOption("LIBKML_ADD_RESOURCE_MAP", "TRUE")) )
        {
            VSILFILE* fp;
            if( EQUALN(pszURL, "http://", strlen("http://")) ||
                EQUALN(pszURL, "https://", strlen("https://")) )
            {
                fp = VSIFOpenL(CPLSPrintf("/vsicurl/%s", pszURL), "rb");
            }
            else if( strstr(pszURL, ".kmz/") != NULL )
            {
                fp = VSIFOpenL(CPLSPrintf("/vsizip/%s", pszURL), "rb");
            }
            else
            {
                fp = VSIFOpenL(pszURL, "rb");
            }
            if( fp != NULL )
            {
                ResourceMapPtr resourceMap = NULL;
                const char* pszLine;
                while( (pszLine = CPLReadLineL(fp)) != NULL )
                {
                    const char* pszInitFrom = strstr(pszLine, "<init_from>");
                    if( pszInitFrom )
                    {
                        pszInitFrom += strlen("<init_from>");
                        const char* pszInitFromEnd = strstr(pszInitFrom, "</init_from>");
                        if( pszInitFromEnd )
                        {
                            CPLString osImage(pszInitFrom);
                            osImage.resize(pszInitFromEnd - pszInitFrom);
                            const char* pszExtension = CPLGetExtension(osImage);
                            if( EQUAL(pszExtension, "jpg") ||
                                EQUAL(pszExtension, "jpeg") ||
                                EQUAL(pszExtension, "png") ||
                                EQUAL(pszExtension, "gif") )
                            {
                                if( resourceMap == NULL )
                                    resourceMap = poKmlFactory->CreateResourceMap();
                                AliasPtr alias = poKmlFactory->CreateAlias();
                                alias->set_targethref(osImage);
                                alias->set_sourcehref(osImage);
                                resourceMap->add_alias(alias);
                            }
                        }
                    }
                }
                if( resourceMap != NULL )
                    model->set_resourcemap(resourceMap);
                VSIFCloseL(fp);
            }
        }

        poKmlPlacemark->set_geometry ( AsGeometry ( model ) );
    }

    /* Camera */
    else if( poOgrGeom != NULL && !poOgrGeom->IsEmpty() &&
        wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint &&
        ((iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading)) ||
         (iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt)) ||
         (iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll))) )
    {
        OGRPoint* poOgrPoint = (OGRPoint*) poOgrGeom;
        CameraPtr camera = poKmlFactory->CreateCamera();
        camera->set_latitude(poOgrPoint->getY());
        camera->set_longitude(poOgrPoint->getX());
        int isGX = FALSE;
        int iAltitudeMode = poOgrFeat->GetFieldIndex(oFC.altitudeModefield);
        int nAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;
        if( iAltitudeMode >= 0 && poOgrFeat->IsFieldSet(iAltitudeMode) )
        {
            nAltitudeMode = kmlAltitudeModeFromString(
                poOgrFeat->GetFieldAsString(iAltitudeMode), isGX);
            camera->set_altitudemode(nAltitudeMode);
        }
        else if( CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
            CPLError(CE_Warning, CPLE_AppDefined, "Camera should define altitudeMode != 'clampToGround'");
        if( poOgrPoint->getCoordinateDimension() == 3 )
            camera->set_altitude(poOgrPoint->getZ());
        else if( CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
            CPLError(CE_Warning, CPLE_AppDefined, "Camera should have an altitude/Z");
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
