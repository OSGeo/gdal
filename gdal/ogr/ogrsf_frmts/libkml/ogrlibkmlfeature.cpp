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
using kmldom::NetworkLinkPtr;
using kmldom::PhotoOverlayPtr;
using kmldom::IconPtr;
using kmldom::ViewVolumePtr;

#include "ogr_libkml.h"

#include "ogrlibkmlgeometry.h"
#include "ogrlibkmlfield.h"
#include "ogrlibkmlfeaturestyle.h"

static CameraPtr feat2kmlcamera( const struct fieldconfig& oFC,
                                 int iHeading,
                                 int iTilt,
                                 int iRoll,
                                 OGRFeature * poOgrFeat,
                                 KmlFactory * poKmlFactory )
{
    int iCameraLongitudeField = poOgrFeat->GetFieldIndex(oFC.camera_longitude_field);
    int iCameraLatitudeField = poOgrFeat->GetFieldIndex(oFC.camera_latitude_field);
    int iCameraAltitudeField = poOgrFeat->GetFieldIndex(oFC.camera_altitude_field);
    int iCameraAltitudeModeField = poOgrFeat->GetFieldIndex(oFC.camera_altitudemode_field);
    if( iCameraLongitudeField >= 0 && poOgrFeat->IsFieldSet(iCameraLongitudeField) &&
        iCameraLatitudeField >= 0  && poOgrFeat->IsFieldSet(iCameraLatitudeField) &&
        ((iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading)) ||
        (iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt)) ||
        (iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll))) )
    {
        CameraPtr camera = poKmlFactory->CreateCamera();
        camera->set_latitude(poOgrFeat->GetFieldAsDouble(iCameraLatitudeField));
        camera->set_longitude(poOgrFeat->GetFieldAsDouble(iCameraLongitudeField));
        int isGX = FALSE;
        int nAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;
        if( iCameraAltitudeModeField >= 0 && poOgrFeat->IsFieldSet(iCameraAltitudeModeField) )
        {
            nAltitudeMode = kmlAltitudeModeFromString(
                poOgrFeat->GetFieldAsString(iCameraAltitudeModeField), isGX);
            camera->set_altitudemode(nAltitudeMode);
        }
        else if( CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
            CPLError(CE_Warning, CPLE_AppDefined, "Camera should define altitudeMode != 'clampToGround'");
        if( iCameraAltitudeField >= 0 && poOgrFeat->IsFieldSet(iCameraAltitudeField))
            camera->set_altitude(poOgrFeat->GetFieldAsDouble(iCameraAltitudeField));
        else if( CSLTestBoolean(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
        {
            CPLError(CE_Warning, CPLE_AppDefined, "Camera should have an altitude/Z");
            camera->set_altitude(0.0);
        }
        if( iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading) )
            camera->set_heading(poOgrFeat->GetFieldAsDouble(iHeading));
        if( iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt) )
            camera->set_tilt(poOgrFeat->GetFieldAsDouble(iTilt));
        if( iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll) )
            camera->set_roll(poOgrFeat->GetFieldAsDouble(iRoll));
        return camera;
    }
    else
        return NULL;
}


FeaturePtr feat2kml (
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeature * poOgrFeat,
    KmlFactory * poKmlFactory )
{
    FeaturePtr poKmlFeature = NULL;

    struct fieldconfig oFC;
    get_fieldconfig( &oFC );

    /***** geometry *****/

    OGRGeometry *poOgrGeom = poOgrFeat->GetGeometryRef (  );
    int iHeading = poOgrFeat->GetFieldIndex(oFC.headingfield),
        iTilt = poOgrFeat->GetFieldIndex(oFC.tiltfield),
        iRoll = poOgrFeat->GetFieldIndex(oFC.rollfield),
        iModel = poOgrFeat->GetFieldIndex(oFC.modelfield);
    int iNetworkLink = poOgrFeat->GetFieldIndex(oFC.networklinkfield);
    int iPhotoOverlay = poOgrFeat->GetFieldIndex(oFC.photooverlayfield);
    CameraPtr camera = NULL;

    /* PhotoOverlay */
    if( iPhotoOverlay >= 0 && poOgrFeat->IsFieldSet(iPhotoOverlay) &&
        poOgrGeom != NULL && !poOgrGeom->IsEmpty() &&
        wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint &&
        (camera = feat2kmlcamera(oFC, iHeading, iTilt, iRoll, poOgrFeat, poKmlFactory)) != NULL)
    {
        int iLeftFovField = poOgrFeat->GetFieldIndex(oFC.leftfovfield);
        int iRightFovField = poOgrFeat->GetFieldIndex(oFC.rightfovfield);
        int iBottomFovField = poOgrFeat->GetFieldIndex(oFC.bottomfovfield);
        int iTopFovField = poOgrFeat->GetFieldIndex(oFC.topfovfield);
        int iNearField = poOgrFeat->GetFieldIndex(oFC.nearfield);
        double dfNear;

        /* ATC 19 & 35 */
        if( iLeftFovField >= 0 && poOgrFeat->IsFieldSet(iLeftFovField) &&
            iRightFovField >= 0 && poOgrFeat->IsFieldSet(iRightFovField) &&
            iBottomFovField >= 0 && poOgrFeat->IsFieldSet(iBottomFovField) &&
            iTopFovField >= 0 && poOgrFeat->IsFieldSet(iTopFovField) &&
            iNearField >= 0 && (dfNear = poOgrFeat->GetFieldAsDouble(iNearField)) > 0 )
        {
            PhotoOverlayPtr poKmlPhotoOverlay = poKmlFactory->CreatePhotoOverlay (  );
            poKmlFeature = poKmlPhotoOverlay;

            IconPtr poKmlIcon = poKmlFactory->CreateIcon (  );
            poKmlPhotoOverlay->set_icon(poKmlIcon);
            poKmlIcon->set_href( poOgrFeat->GetFieldAsString(iPhotoOverlay) );

            ViewVolumePtr poKmlViewVolume = poKmlFactory->CreateViewVolume( );
            poKmlPhotoOverlay->set_viewvolume(poKmlViewVolume);

            double dfLeftFov = poOgrFeat->GetFieldAsDouble(iLeftFovField);
            double dfRightFov = poOgrFeat->GetFieldAsDouble(iRightFovField);
            double dfBottomFov = poOgrFeat->GetFieldAsDouble(iBottomFovField);
            double dfTopFov = poOgrFeat->GetFieldAsDouble(iTopFovField);

            poKmlViewVolume->set_leftfov(dfLeftFov);
            poKmlViewVolume->set_rightfov(dfRightFov);
            poKmlViewVolume->set_bottomfov(dfBottomFov);
            poKmlViewVolume->set_topfov(dfTopFov);
            poKmlViewVolume->set_near(dfNear);
            
            int iPhotoOverlayShapeField = poOgrFeat->GetFieldIndex(oFC.photooverlay_shape_field);
            if( iPhotoOverlayShapeField >= 0 && poOgrFeat->IsFieldSet(iPhotoOverlayShapeField) )
            {
                const char* pszShape = poOgrFeat->GetFieldAsString(iPhotoOverlayShapeField);
                if( EQUAL(pszShape, "rectangle") )
                    poKmlPhotoOverlay->set_shape(kmldom::SHAPE_RECTANGLE);
                else if( EQUAL(pszShape, "cylinder") )
                    poKmlPhotoOverlay->set_shape(kmldom::SHAPE_CYLINDER);
                else if( EQUAL(pszShape, "sphere") )
                    poKmlPhotoOverlay->set_shape(kmldom::SHAPE_SPHERE);
            }

            ElementPtr poKmlElement = geom2kml ( poOgrGeom, -1, 0, poKmlFactory );

            poKmlPhotoOverlay->set_point ( AsPoint ( poKmlElement ) );
        }
    }

    /* NetworkLink */
    if( poKmlFeature == NULL && iNetworkLink >= 0 && poOgrFeat->IsFieldSet(iNetworkLink) )
    {
        NetworkLinkPtr poKmlNetworkLink = poKmlFactory->CreateNetworkLink (  );
        poKmlFeature = poKmlNetworkLink;

        int iRefreshVisibility = poOgrFeat->GetFieldIndex(oFC.networklink_refreshvisibility_field);
        int iFlyToView = poOgrFeat->GetFieldIndex(oFC.networklink_flytoview_field);

        if( iRefreshVisibility >= 0 && poOgrFeat->IsFieldSet(iRefreshVisibility) )
            poKmlNetworkLink->set_refreshvisibility(poOgrFeat->GetFieldAsInteger(iRefreshVisibility));

        if( iFlyToView >= 0 && poOgrFeat->IsFieldSet(iFlyToView) )
            poKmlNetworkLink->set_flytoview(poOgrFeat->GetFieldAsInteger(iFlyToView));

        LinkPtr poKmlLink = poKmlFactory->CreateLink (  );
        poKmlLink->set_href( poOgrFeat->GetFieldAsString( iNetworkLink ) );
        poKmlNetworkLink->set_link(poKmlLink);

        int iRefreshMode = poOgrFeat->GetFieldIndex(oFC.networklink_refreshMode_field);
        int iRefreshInterval = poOgrFeat->GetFieldIndex(oFC.networklink_refreshInterval_field);
        int iViewRefreshMode = poOgrFeat->GetFieldIndex(oFC.networklink_viewRefreshMode_field);
        int iViewRefreshTime = poOgrFeat->GetFieldIndex(oFC.networklink_viewRefreshTime_field);
        int iViewBoundScale = poOgrFeat->GetFieldIndex(oFC.networklink_viewBoundScale_field);
        int iViewFormat = poOgrFeat->GetFieldIndex(oFC.networklink_viewFormat_field);
        int iHttpQuery = poOgrFeat->GetFieldIndex(oFC.networklink_httpQuery_field);

        double dfRefreshInterval = 0.0;
        if( iRefreshInterval >= 0 && poOgrFeat->IsFieldSet(iRefreshInterval) )
        {
            dfRefreshInterval = poOgrFeat->GetFieldAsDouble(iRefreshInterval);
            if( dfRefreshInterval < 0 )
                dfRefreshInterval = 0.0;
        }

        double dfViewRefreshTime = 0.0;
        if( iViewRefreshTime >= 0 && poOgrFeat->IsFieldSet(iViewRefreshTime) )
        {
            dfViewRefreshTime = poOgrFeat->GetFieldAsDouble(iViewRefreshTime);
            if( dfViewRefreshTime < 0 )
                dfViewRefreshTime = 0.0;
        }

        if( dfRefreshInterval > 0 ) /* ATC 51 */
            poKmlLink->set_refreshmode(kmldom::REFRESHMODE_ONINTERVAL);
        else if( iRefreshMode >= 0 && poOgrFeat->IsFieldSet(iRefreshMode) )
        {
            const char* pszRefreshMode = poOgrFeat->GetFieldAsString(iRefreshMode);
            if( EQUAL(pszRefreshMode, "onChange") )
                poKmlLink->set_refreshmode(kmldom::REFRESHMODE_ONCHANGE);
            else if( EQUAL(pszRefreshMode, "onInterval") )
                poKmlLink->set_refreshmode(kmldom::REFRESHMODE_ONINTERVAL);
            else if( EQUAL(pszRefreshMode, "onExpire") )
                poKmlLink->set_refreshmode(kmldom::REFRESHMODE_ONEXPIRE);
        }

        if( dfRefreshInterval > 0 ) /* ATC 9 */
            poKmlLink->set_refreshinterval(dfRefreshInterval);

        if( dfViewRefreshTime > 0 ) /* ATC  51 */
            poKmlLink->set_viewrefreshmode(kmldom::VIEWREFRESHMODE_ONSTOP);
        else if( iViewRefreshMode >= 0 && poOgrFeat->IsFieldSet(iViewRefreshMode) )
        {
            const char* pszViewRefreshMode = poOgrFeat->GetFieldAsString(iViewRefreshMode);
            if( EQUAL(pszViewRefreshMode, "never") )
                poKmlLink->set_viewrefreshmode(kmldom::VIEWREFRESHMODE_NEVER);
            else if( EQUAL(pszViewRefreshMode, "onRequest") )
                poKmlLink->set_viewrefreshmode(kmldom::VIEWREFRESHMODE_ONREQUEST);
            else if( EQUAL(pszViewRefreshMode, "onStop") )
                poKmlLink->set_viewrefreshmode(kmldom::VIEWREFRESHMODE_ONSTOP);
            else if( EQUAL(pszViewRefreshMode, "onRegion") )
                poKmlLink->set_viewrefreshmode(kmldom::VIEWREFRESHMODE_ONREGION);
        }

        if( dfViewRefreshTime > 0 ) /* ATC 9 */
            poKmlLink->set_viewrefreshtime(dfViewRefreshTime);

        if( iViewBoundScale >= 0 && poOgrFeat->IsFieldSet(iViewBoundScale) )
        {
            double dfViewBoundScale = poOgrFeat->GetFieldAsDouble(iViewBoundScale);
            if( dfViewBoundScale > 0 ) /* ATC 9 */
                poKmlLink->set_viewboundscale(dfViewBoundScale);
        }

        if( iViewFormat >= 0 && poOgrFeat->IsFieldSet(iViewFormat) )
        {
            const char* pszViewFormat = poOgrFeat->GetFieldAsString(iViewFormat);
            if( pszViewFormat[0] != '\0' ) /* ATC 46 */
                poKmlLink->set_viewformat(pszViewFormat);
        }

        if( iHttpQuery >= 0 && poOgrFeat->IsFieldSet(iHttpQuery) )
        {
            const char* pszHttpQuery = poOgrFeat->GetFieldAsString(iHttpQuery);
            if( strstr(pszHttpQuery, "[clientVersion]") != NULL ||
                strstr(pszHttpQuery, "[kmlVersion]") != NULL ||
                strstr(pszHttpQuery, "[clientName]") != NULL || 
                strstr(pszHttpQuery, "[language]") != NULL ) /* ATC 47 */
            {
                poKmlLink->set_httpquery(pszHttpQuery);
            }
        }
    }

    /* Model */
    else if( poKmlFeature == NULL && iModel>= 0 && poOgrFeat->IsFieldSet(iModel) &&
        poOgrGeom != NULL && !poOgrGeom->IsEmpty() &&
        wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint )
    {
        PlacemarkPtr poKmlPlacemark = poKmlFactory->CreatePlacemark (  );
        poKmlFeature = poKmlPlacemark;

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
    else if( poKmlFeature == NULL && poOgrGeom != NULL && !poOgrGeom->IsEmpty() &&
        wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint &&
        poOgrFeat->GetFieldIndex(oFC.camera_longitude_field) < 0 &&
        ((iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading)) ||
         (iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt)) ||
         (iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll))) )
    {
        PlacemarkPtr poKmlPlacemark = poKmlFactory->CreatePlacemark (  );
        poKmlFeature = poKmlPlacemark;

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
        {
            CPLError(CE_Warning, CPLE_AppDefined, "Camera should have an altitude/Z");
            camera->set_altitude(0.0);
        }
        if( iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading) )
            camera->set_heading(poOgrFeat->GetFieldAsDouble(iHeading));
        if( iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt) )
            camera->set_tilt(poOgrFeat->GetFieldAsDouble(iTilt));
        if( iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll) )
            camera->set_roll(poOgrFeat->GetFieldAsDouble(iRoll));
        poKmlPlacemark->set_abstractview(camera);
    }

    else if( poKmlFeature == NULL )
    {
        PlacemarkPtr poKmlPlacemark = poKmlFactory->CreatePlacemark (  );
        poKmlFeature = poKmlPlacemark;

        ElementPtr poKmlElement = geom2kml ( poOgrGeom, -1, 0, poKmlFactory );

        poKmlPlacemark->set_geometry ( AsGeometry ( poKmlElement ) );
    }

    if( camera == NULL )
        camera = feat2kmlcamera(oFC, iHeading, iTilt, iRoll, poOgrFeat, poKmlFactory);
    if( camera != NULL )
        poKmlFeature->set_abstractview(camera);

    /***** style *****/

    featurestyle2kml ( poOgrDS, poOgrLayer, poOgrFeat, poKmlFactory,
                       poKmlFeature );

    /***** fields *****/

    field2kml ( poOgrFeat, ( OGRLIBKMLLayer * ) poOgrLayer, poKmlFactory,
                poKmlFeature );

    return poKmlFeature;
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
