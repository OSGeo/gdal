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

#include "libkml_headers.h"
#include "ogrlibkmlfeature.h"

#include "gdal.h"
#include "ogr_geometry.h"
#include "ogr_libkml.h"
#include "ogrlibkmlfield.h"
#include "ogrlibkmlfeaturestyle.h"
#include "ogrlibkmlgeometry.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$");

using kmldom::AliasPtr;
using kmldom::CameraPtr;
using kmldom::ElementPtr;
using kmldom::FeaturePtr;
using kmldom::GeometryPtr;
using kmldom::GroundOverlayPtr;
using kmldom::IconPtr;
using kmldom::ImagePyramidPtr;
using kmldom::KmlFactory;
using kmldom::LinkPtr;
using kmldom::LocationPtr;
using kmldom::ModelPtr;
using kmldom::NetworkLinkPtr;
using kmldom::OrientationPtr;
using kmldom::PhotoOverlayPtr;
using kmldom::PlacemarkPtr;
using kmldom::ResourceMapPtr;
using kmldom::ScalePtr;
using kmldom::ViewVolumePtr;

static CameraPtr feat2kmlcamera( const struct fieldconfig& oFC,
                                 int iHeading,
                                 int iTilt,
                                 int iRoll,
                                 OGRFeature * poOgrFeat,
                                 KmlFactory * poKmlFactory )
{
    const int iCameraLongitudeField =
        poOgrFeat->GetFieldIndex(oFC.camera_longitude_field);
    const int iCameraLatitudeField =
        poOgrFeat->GetFieldIndex(oFC.camera_latitude_field);
    const int iCameraAltitudeField =
        poOgrFeat->GetFieldIndex(oFC.camera_altitude_field);
    const int iCameraAltitudeModeField =
        poOgrFeat->GetFieldIndex(oFC.camera_altitudemode_field);

    const bool bNeedCamera =
        iCameraLongitudeField >= 0 &&
        poOgrFeat->IsFieldSet(iCameraLongitudeField) &&
        iCameraLatitudeField >= 0 &&
        poOgrFeat->IsFieldSet(iCameraLatitudeField) &&
        ((iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading)) ||
        (iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt)) ||
        (iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll)));

    if( !bNeedCamera )
        return NULL;

    CameraPtr const camera = poKmlFactory->CreateCamera();
    camera->set_latitude(poOgrFeat->GetFieldAsDouble(iCameraLatitudeField));
    camera->set_longitude(poOgrFeat->GetFieldAsDouble(iCameraLongitudeField));
    int isGX = FALSE;
    int nAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;

    if( iCameraAltitudeModeField >= 0 &&
        poOgrFeat->IsFieldSet(iCameraAltitudeModeField) )
    {
        nAltitudeMode = kmlAltitudeModeFromString(
            poOgrFeat->GetFieldAsString(iCameraAltitudeModeField), isGX);
        camera->set_altitudemode(nAltitudeMode);
    }
    else if( CPLTestBool(
                 CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
    {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Camera should define altitudeMode != 'clampToGround'");
    }

    if( iCameraAltitudeField >= 0 &&
        poOgrFeat->IsFieldSet(iCameraAltitudeField))
    {
        camera->set_altitude(poOgrFeat->GetFieldAsDouble(iCameraAltitudeField));
    }
    else if( CPLTestBool(
                 CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Camera should have an altitude/Z");
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

/************************************************************************/
/*                 OGRLIBKMLReplaceXYLevelInURL()                       */
/************************************************************************/

static CPLString OGRLIBKMLReplaceLevelXYInURL( const char* pszURL,
                                               int level, int x, int y )
{
    CPLString osRet(pszURL);
    size_t nPos = osRet.find("$[level]");
    osRet =
        osRet.substr(0, nPos) + CPLSPrintf("%d", level) +
        osRet.substr(nPos + strlen("$[level]"));

    nPos = osRet.find("$[x]");
    osRet =
        osRet.substr(0, nPos) + CPLSPrintf("%d", x) +
        osRet.substr(nPos + strlen("$[x]"));

    nPos = osRet.find("$[y]");
    osRet =
        osRet.substr(0, nPos) + CPLSPrintf("%d", y) +
        osRet.substr(nPos + strlen("$[y]"));

    return osRet;
}

/************************************************************************/
/*                        IsPowerOf2                                    */
/************************************************************************/

static bool IsPowerOf2( int nVal )
{
    if( nVal < 1 ) return false;

    const unsigned int nTmp = static_cast<unsigned int>(nVal);

    return (nTmp & (nTmp - 1)) == 0;
}

/************************************************************************/
/*                    OGRLIBKMLGetMaxDimensions()                       */
/************************************************************************/

static void OGRLIBKMLGetMaxDimensions( const char* pszURL,
                                       int nTileSize,
                                       int* panMaxWidth,
                                       int* panMaxHeight )
{
    VSIStatBufL sStat;
    int nMaxLevel = 0;
    *panMaxWidth = 0;
    *panMaxHeight = 0;
    while( true )
    {
        CPLString osURL = OGRLIBKMLReplaceLevelXYInURL(pszURL, nMaxLevel, 0, 0);
        if( strstr(osURL, ".kmz/") )
            osURL = "/vsizip/" + osURL;
        if( VSIStatL(osURL, &sStat) == 0 )
            nMaxLevel++;
        else
        {
            if( nMaxLevel == 0 )
                return;
            break;
        }
    }
    nMaxLevel--;

    {
        int i = 0;  // Used after for.
        for( ; ; i++ )
        {
            CPLString osURL =
                OGRLIBKMLReplaceLevelXYInURL(pszURL, nMaxLevel, i + 1, 0);
            if( strstr(osURL, ".kmz/") )
                osURL = "/vsizip/" + osURL;
            if( VSIStatL(osURL, &sStat) != 0 )
                break;
        }
        *panMaxWidth = (i + 1) * nTileSize;
    }

    int i = 0;  // Used after for.
    for( ; ; i++ )
    {
        CPLString osURL =
            OGRLIBKMLReplaceLevelXYInURL(pszURL, nMaxLevel, 0, i + 1);
        if( strstr(osURL, ".kmz/") )
            osURL = "/vsizip/" + osURL;
        if( VSIStatL(osURL, &sStat) != 0 )
            break;
    }
    *panMaxHeight = (i + 1) * nTileSize;
}

/************************************************************************/
/*                           feat2kml()                                 */
/************************************************************************/

FeaturePtr feat2kml(
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeature * poOgrFeat,
    KmlFactory * poKmlFactory,
    int bUseSimpleField )
{
    FeaturePtr poKmlFeature = NULL;

    struct fieldconfig oFC;
    get_fieldconfig( &oFC );

    /***** geometry *****/
    OGRGeometry *poOgrGeom = poOgrFeat->GetGeometryRef();
    const int iHeading = poOgrFeat->GetFieldIndex(oFC.headingfield);
    const int iTilt = poOgrFeat->GetFieldIndex(oFC.tiltfield);
    const int iRoll = poOgrFeat->GetFieldIndex(oFC.rollfield);
    const int iModel = poOgrFeat->GetFieldIndex(oFC.modelfield);
    const int iNetworkLink = poOgrFeat->GetFieldIndex(oFC.networklinkfield);
    const int iPhotoOverlay = poOgrFeat->GetFieldIndex(oFC.photooverlayfield);
    CameraPtr camera = NULL;

    // PhotoOverlay.
    if( iPhotoOverlay >= 0 && poOgrFeat->IsFieldSet(iPhotoOverlay) &&
        poOgrGeom != NULL && !poOgrGeom->IsEmpty() &&
        wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint &&
        (camera = feat2kmlcamera(oFC, iHeading, iTilt, iRoll,
                                 poOgrFeat, poKmlFactory)) != NULL)
    {
        const int iLeftFovField = poOgrFeat->GetFieldIndex(oFC.leftfovfield);
        const int iRightFovField = poOgrFeat->GetFieldIndex(oFC.rightfovfield);
        const int iBottomFovField =
            poOgrFeat->GetFieldIndex(oFC.bottomfovfield);
        const int iTopFovField = poOgrFeat->GetFieldIndex(oFC.topfovfield);
        const int iNearField = poOgrFeat->GetFieldIndex(oFC.nearfield);

        const char* pszURL = poOgrFeat->GetFieldAsString(iPhotoOverlay);
        const int iImagePyramidTileSize =
            poOgrFeat->GetFieldIndex(oFC.imagepyramid_tilesize_field);
        const int iImagePyramidMaxWidth =
            poOgrFeat->GetFieldIndex(oFC.imagepyramid_maxwidth_field);
        const int iImagePyramidMaxHeight =
            poOgrFeat->GetFieldIndex(oFC.imagepyramid_maxheight_field);
        const int iImagePyramidGridOrigin =
            poOgrFeat->GetFieldIndex(oFC.imagepyramid_gridorigin_field);

        int nTileSize = 0;
        int nMaxWidth = 0;
        int nMaxHeight = 0;
        bool bIsTiledPhotoOverlay = false;
        bool bGridOriginIsUpperLeft = true;
        // OGC KML Abstract Test Case (ATC) 52 and 62
        if( strstr(pszURL, "$[x]") &&
            strstr(pszURL, "$[y]") &&
            strstr(pszURL, "$[level]") )
        {
            bIsTiledPhotoOverlay = true;
            bool bErrorEmitted = false;
            if( iImagePyramidTileSize < 0 ||
                !poOgrFeat->IsFieldSet(iImagePyramidTileSize) )
            {
                CPLDebug("LIBKML",
                         "Missing ImagePyramid tileSize. Computing it");
                CPLString osURL = OGRLIBKMLReplaceLevelXYInURL(pszURL, 0, 0, 0);
                if( strstr(osURL, ".kmz/") )
                    osURL = "/vsizip/" + osURL;
                GDALDatasetH hDS = GDALOpen( osURL, GA_ReadOnly );
                if( hDS != NULL )
                {
                    nTileSize = GDALGetRasterXSize(hDS);
                    if( nTileSize != GDALGetRasterYSize(hDS) )
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "Non square tile : %dx%d",
                            GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));
                        nTileSize = 0;
                        bErrorEmitted = true;
                    }
                    GDALClose(hDS);
                }
                else
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Cannot open %s", osURL.c_str());
                    bErrorEmitted = true;
                }
            }
            else
            {
                nTileSize = poOgrFeat->GetFieldAsInteger(iImagePyramidTileSize);
            }
            if( !bErrorEmitted && (nTileSize <= 1 || !IsPowerOf2(nTileSize)) )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Tile size is not a power of two: %d", nTileSize);
                nTileSize = 0;
            }

            if( nTileSize > 0 )
            {
                if( iImagePyramidMaxWidth < 0 ||
                    !poOgrFeat->IsFieldSet(iImagePyramidMaxWidth) ||
                    iImagePyramidMaxHeight < 0 ||
                    !poOgrFeat->IsFieldSet(iImagePyramidMaxHeight) )
                {
                    CPLDebug(
                        "LIBKML",
                        "Missing ImagePyramid maxWidth and/or maxHeight. "
                        "Computing it");
                    OGRLIBKMLGetMaxDimensions(pszURL, nTileSize,
                                              &nMaxWidth, &nMaxHeight);
                }
                else
                {
                    nMaxWidth =
                        poOgrFeat->GetFieldAsInteger(iImagePyramidMaxWidth);
                    nMaxHeight =
                        poOgrFeat->GetFieldAsInteger(iImagePyramidMaxHeight);
                }

                if( nTileSize <= 0 || nMaxWidth <= 0 || nMaxHeight <= 0)
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Cannot generate PhotoOverlay object since there are "
                        "missing information to generate ImagePyramid element");
                }
            }

            if( iImagePyramidGridOrigin >= 0 &&
                poOgrFeat->IsFieldSet(iImagePyramidGridOrigin) )
            {
                const char* pszGridOrigin =
                    poOgrFeat->GetFieldAsString(iImagePyramidGridOrigin);
                if( EQUAL(pszGridOrigin, "UpperLeft") )
                {
                    bGridOriginIsUpperLeft = true;
                }
                else if( EQUAL(pszGridOrigin, "BottomLeft") )
                {
                    bGridOriginIsUpperLeft = false;
                }
                else
                {
                    CPLError(
                        CE_Failure, CPLE_AppDefined,
                        "Unhandled value for imagepyramid_gridorigin : %s. "
                        "Assuming UpperLeft",
                        pszGridOrigin);
                }
            }
        }
        else
        {
            if( (iImagePyramidTileSize >= 0 &&
                 poOgrFeat->IsFieldSet(iImagePyramidTileSize)) ||
                (iImagePyramidMaxWidth >= 0 &&
                 poOgrFeat->IsFieldSet(iImagePyramidMaxWidth)) ||
                (iImagePyramidMaxHeight >= 0 &&
                 poOgrFeat->IsFieldSet(iImagePyramidMaxHeight)) ||
                (iImagePyramidGridOrigin >= 0 &&
                 poOgrFeat->IsFieldSet(iImagePyramidGridOrigin)) )
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Ignoring any ImagePyramid information since the URL does "
                    "not include $[x] and/or $[y] and/or $[level]");
            }
        }

        // OGC KML Abstract Test Case (ATC) 19 & 35.
        double dfNear = 0.0;

        if( (!bIsTiledPhotoOverlay ||
             (nTileSize > 0 && nMaxWidth > 0 && nMaxHeight > 0)) &&
            iLeftFovField >= 0 && poOgrFeat->IsFieldSet(iLeftFovField) &&
            iRightFovField >= 0 && poOgrFeat->IsFieldSet(iRightFovField) &&
            iBottomFovField >= 0 && poOgrFeat->IsFieldSet(iBottomFovField) &&
            iTopFovField >= 0 && poOgrFeat->IsFieldSet(iTopFovField) &&
            iNearField >= 0 &&
            (dfNear = poOgrFeat->GetFieldAsDouble(iNearField)) > 0 )
        {
            const PhotoOverlayPtr poKmlPhotoOverlay =
                poKmlFactory->CreatePhotoOverlay();
            poKmlFeature = poKmlPhotoOverlay;

            const IconPtr poKmlIcon = poKmlFactory->CreateIcon();
            poKmlPhotoOverlay->set_icon(poKmlIcon);
            poKmlIcon->set_href( pszURL );

            const ViewVolumePtr poKmlViewVolume =
                poKmlFactory->CreateViewVolume();
            poKmlPhotoOverlay->set_viewvolume(poKmlViewVolume);

            const double dfLeftFov = poOgrFeat->GetFieldAsDouble(iLeftFovField);
            const double dfRightFov =
                poOgrFeat->GetFieldAsDouble(iRightFovField);
            const double dfBottomFov =
                poOgrFeat->GetFieldAsDouble(iBottomFovField);
            const double dfTopFov = poOgrFeat->GetFieldAsDouble(iTopFovField);

            poKmlViewVolume->set_leftfov(dfLeftFov);
            poKmlViewVolume->set_rightfov(dfRightFov);
            poKmlViewVolume->set_bottomfov(dfBottomFov);
            poKmlViewVolume->set_topfov(dfTopFov);
            poKmlViewVolume->set_near(dfNear);

            if( bIsTiledPhotoOverlay )
            {
                const ImagePyramidPtr poKmlImagePyramid =
                    poKmlFactory->CreateImagePyramid();
                poKmlPhotoOverlay->set_imagepyramid(poKmlImagePyramid);

                poKmlImagePyramid->set_tilesize(nTileSize);
                poKmlImagePyramid->set_maxwidth(nMaxWidth);
                poKmlImagePyramid->set_maxheight(nMaxHeight);
                poKmlImagePyramid->set_gridorigin(
                    bGridOriginIsUpperLeft ?
                    kmldom::GRIDORIGIN_UPPERLEFT :
                    kmldom::GRIDORIGIN_LOWERLEFT );
            }

            const int iPhotoOverlayShapeField =
                poOgrFeat->GetFieldIndex(oFC.photooverlay_shape_field);
            if( iPhotoOverlayShapeField >= 0 &&
                poOgrFeat->IsFieldSet(iPhotoOverlayShapeField) )
            {
                const char* pszShape =
                    poOgrFeat->GetFieldAsString(iPhotoOverlayShapeField);
                if( EQUAL(pszShape, "rectangle") )
                    poKmlPhotoOverlay->set_shape(kmldom::SHAPE_RECTANGLE);
                else if( EQUAL(pszShape, "cylinder") )
                    poKmlPhotoOverlay->set_shape(kmldom::SHAPE_CYLINDER);
                else if( EQUAL(pszShape, "sphere") )
                    poKmlPhotoOverlay->set_shape(kmldom::SHAPE_SPHERE);
            }

            ElementPtr poKmlElement = geom2kml( poOgrGeom, -1, poKmlFactory );

            poKmlPhotoOverlay->set_point( AsPoint( poKmlElement ) );
        }
    }

    // NetworkLink.
    if( poKmlFeature == NULL && iNetworkLink >= 0 &&
        poOgrFeat->IsFieldSet(iNetworkLink) )
    {
        const NetworkLinkPtr poKmlNetworkLink =
            poKmlFactory->CreateNetworkLink();
        poKmlFeature = poKmlNetworkLink;

        const int iRefreshVisibility =
            poOgrFeat->GetFieldIndex(oFC.networklink_refreshvisibility_field);

        if( iRefreshVisibility >= 0 &&
            poOgrFeat->IsFieldSet(iRefreshVisibility) )
        {
            poKmlNetworkLink->set_refreshvisibility(CPL_TO_BOOL(
                poOgrFeat->GetFieldAsInteger(iRefreshVisibility)));
        }

        const int iFlyToView =
            poOgrFeat->GetFieldIndex(oFC.networklink_flytoview_field);

        if( iFlyToView >= 0 && poOgrFeat->IsFieldSet(iFlyToView) )
            poKmlNetworkLink->set_flytoview(CPL_TO_BOOL(
                poOgrFeat->GetFieldAsInteger(iFlyToView)));

        const LinkPtr poKmlLink = poKmlFactory->CreateLink();
        poKmlLink->set_href( poOgrFeat->GetFieldAsString( iNetworkLink ) );
        poKmlNetworkLink->set_link(poKmlLink);

        const int iRefreshMode =
            poOgrFeat->GetFieldIndex(oFC.networklink_refreshMode_field);
        const int iRefreshInterval =
            poOgrFeat->GetFieldIndex(oFC.networklink_refreshInterval_field);
        const int iViewRefreshMode =
            poOgrFeat->GetFieldIndex(oFC.networklink_viewRefreshMode_field);
        const int iViewRefreshTime =
            poOgrFeat->GetFieldIndex(oFC.networklink_viewRefreshTime_field);
        const int iViewBoundScale =
            poOgrFeat->GetFieldIndex(oFC.networklink_viewBoundScale_field);
        const int iViewFormat =
            poOgrFeat->GetFieldIndex(oFC.networklink_viewFormat_field);
        const int iHttpQuery =
            poOgrFeat->GetFieldIndex(oFC.networklink_httpQuery_field);

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

        if( dfRefreshInterval > 0 )  // ATC 51
            poKmlLink->set_refreshmode(kmldom::REFRESHMODE_ONINTERVAL);
        else if( iRefreshMode >= 0 && poOgrFeat->IsFieldSet(iRefreshMode) )
        {
            const char * const pszRefreshMode =
                poOgrFeat->GetFieldAsString(iRefreshMode);
            if( EQUAL(pszRefreshMode, "onChange") )
                poKmlLink->set_refreshmode(kmldom::REFRESHMODE_ONCHANGE);
            else if( EQUAL(pszRefreshMode, "onInterval") )
                poKmlLink->set_refreshmode(kmldom::REFRESHMODE_ONINTERVAL);
            else if( EQUAL(pszRefreshMode, "onExpire") )
                poKmlLink->set_refreshmode(kmldom::REFRESHMODE_ONEXPIRE);
        }

        if( dfRefreshInterval > 0 )  // ATC 9
            poKmlLink->set_refreshinterval(dfRefreshInterval);

        if( dfViewRefreshTime > 0 )  // ATC 51
            poKmlLink->set_viewrefreshmode(kmldom::VIEWREFRESHMODE_ONSTOP);
        else if( iViewRefreshMode >= 0 &&
                 poOgrFeat->IsFieldSet(iViewRefreshMode) )
        {
            const char * const pszViewRefreshMode =
                poOgrFeat->GetFieldAsString(iViewRefreshMode);
            if( EQUAL(pszViewRefreshMode, "never") )
                poKmlLink->set_viewrefreshmode(kmldom::VIEWREFRESHMODE_NEVER);
            else if( EQUAL(pszViewRefreshMode, "onRequest") )
                poKmlLink->set_viewrefreshmode(
                    kmldom::VIEWREFRESHMODE_ONREQUEST);
            else if( EQUAL(pszViewRefreshMode, "onStop") )
                poKmlLink->set_viewrefreshmode(kmldom::VIEWREFRESHMODE_ONSTOP);
            else if( EQUAL(pszViewRefreshMode, "onRegion") )
                poKmlLink->set_viewrefreshmode(
                    kmldom::VIEWREFRESHMODE_ONREGION);
        }

        if( dfViewRefreshTime > 0 ) // ATC 9
            poKmlLink->set_viewrefreshtime(dfViewRefreshTime);

        if( iViewBoundScale >= 0 && poOgrFeat->IsFieldSet(iViewBoundScale) )
        {
            const double dfViewBoundScale =
                poOgrFeat->GetFieldAsDouble(iViewBoundScale);
            if( dfViewBoundScale > 0 ) // ATC 9
                poKmlLink->set_viewboundscale(dfViewBoundScale);
        }

        if( iViewFormat >= 0 && poOgrFeat->IsFieldSet(iViewFormat) )
        {
            const char * const pszViewFormat =
                poOgrFeat->GetFieldAsString(iViewFormat);
            if( pszViewFormat[0] != '\0' ) // ATC 46
                poKmlLink->set_viewformat(pszViewFormat);
        }

        if( iHttpQuery >= 0 && poOgrFeat->IsFieldSet(iHttpQuery) )
        {
            const char* const pszHttpQuery =
                poOgrFeat->GetFieldAsString(iHttpQuery);
            if( strstr(pszHttpQuery, "[clientVersion]") != NULL ||
                strstr(pszHttpQuery, "[kmlVersion]") != NULL ||
                strstr(pszHttpQuery, "[clientName]") != NULL ||
                strstr(pszHttpQuery, "[language]") != NULL )  // ATC 47
            {
                poKmlLink->set_httpquery(pszHttpQuery);
            }
        }
    }

    // Model.
    else if( poKmlFeature == NULL &&
             iModel >= 0 &&
             poOgrFeat->IsFieldSet(iModel) &&
             poOgrGeom != NULL && !poOgrGeom->IsEmpty() &&
             wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint )
    {
        const PlacemarkPtr poKmlPlacemark = poKmlFactory->CreatePlacemark();
        poKmlFeature = poKmlPlacemark;

        const OGRPoint* const poOgrPoint = dynamic_cast<OGRPoint *>(poOgrGeom);
        if( poOgrPoint == NULL )
        {
          CPLError(CE_Failure, CPLE_AppDefined, "dynamic_cast failed.");
          return NULL;
        }
        ModelPtr model = poKmlFactory->CreateModel();

        LocationPtr location = poKmlFactory->CreateLocation();
        model->set_location(location);
        location->set_latitude(poOgrPoint->getY());
        location->set_longitude(poOgrPoint->getX());
        if( poOgrPoint->getCoordinateDimension() == 3 )
            location->set_altitude(poOgrPoint->getZ());

        int isGX = FALSE;
        const int iAltitudeMode =
            poOgrFeat->GetFieldIndex(oFC.altitudeModefield);
        int nAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;
        if( iAltitudeMode >= 0 && poOgrFeat->IsFieldSet(iAltitudeMode) )
        {
            nAltitudeMode = kmlAltitudeModeFromString(
                poOgrFeat->GetFieldAsString(iAltitudeMode), isGX);
            model->set_altitudemode(nAltitudeMode);

            // ATC 55
            if( nAltitudeMode != kmldom::ALTITUDEMODE_CLAMPTOGROUND &&
                poOgrPoint->getCoordinateDimension() != 3 )
            {
                if( CPLTestBool(
                    CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Altitude should be defined");
            }
        }

        if( (iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading)) ||
            (iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt)) ||
            (iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll)) )
        {
            OrientationPtr const orientation =
                poKmlFactory->CreateOrientation();
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
        const int iScaleX = poOgrFeat->GetFieldIndex(oFC.scalexfield);
        const int iScaleY = poOgrFeat->GetFieldIndex(oFC.scaleyfield);
        const int iScaleZ = poOgrFeat->GetFieldIndex(oFC.scalezfield);

        const ScalePtr scale = poKmlFactory->CreateScale();
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

        const LinkPtr link = poKmlFactory->CreateLink();
        model->set_link(link);
        const char* const pszURL = poOgrFeat->GetFieldAsString(oFC.modelfield);
        link->set_href( pszURL );

        // Collada 3D file?
        if( EQUAL(CPLGetExtension(pszURL), "dae") &&
            CPLTestBool(CPLGetConfigOption("LIBKML_ADD_RESOURCE_MAP", "TRUE")) )
        {
            VSILFILE* fp = NULL;
            bool bIsURL = false;
            if( STARTS_WITH_CI(pszURL, "http://") ||
                STARTS_WITH_CI(pszURL, "https://") )
            {
                bIsURL = true;
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
                const char* pszLine = NULL;
                while( (pszLine = CPLReadLineL(fp)) != NULL )
                {
                    const char* pszInitFrom = strstr(pszLine, "<init_from>");
                    if( pszInitFrom )
                    {
                        pszInitFrom += strlen("<init_from>");
                        const char* const pszInitFromEnd =
                            strstr(pszInitFrom, "</init_from>");
                        if( pszInitFromEnd )
                        {
                            CPLString osImage(pszInitFrom);
                            osImage.resize(pszInitFromEnd - pszInitFrom);
                            const char* const pszExtension =
                                CPLGetExtension(osImage);
                            if( EQUAL(pszExtension, "jpg") ||
                                EQUAL(pszExtension, "jpeg") ||
                                EQUAL(pszExtension, "png") ||
                                EQUAL(pszExtension, "gif") )
                            {
                                if( resourceMap == NULL )
                                    resourceMap =
                                        poKmlFactory->CreateResourceMap();
                                const AliasPtr alias =
                                    poKmlFactory->CreateAlias();
                                if( bIsURL && CPLIsFilenameRelative(osImage) )
                                {
                                    if( STARTS_WITH(pszURL, "http") )
                                        alias->set_targethref(
                                            CPLSPrintf(
                                                "%s/%s", CPLGetPath(pszURL),
                                                osImage.c_str()));
                                    else
                                        alias->set_targethref(CPLFormFilename(
                                            CPLGetPath(pszURL), osImage, NULL));
                                }
                                else
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

        poKmlPlacemark->set_geometry( AsGeometry( model ) );
    }

    // Camera.
    else if( poKmlFeature == NULL && poOgrGeom != NULL &&
             !poOgrGeom->IsEmpty() &&
             wkbFlatten(poOgrGeom->getGeometryType()) == wkbPoint &&
             poOgrFeat->GetFieldIndex(oFC.camera_longitude_field) < 0 &&
             ((iHeading >= 0 && poOgrFeat->IsFieldSet(iHeading)) ||
              (iTilt >= 0 && poOgrFeat->IsFieldSet(iTilt)) ||
              (iRoll >= 0 && poOgrFeat->IsFieldSet(iRoll))) )
    {
        const PlacemarkPtr poKmlPlacemark = poKmlFactory->CreatePlacemark();
        poKmlFeature = poKmlPlacemark;

        const OGRPoint* const poOgrPoint = dynamic_cast<OGRPoint *>(poOgrGeom);
              if( poOgrPoint == NULL )
        {
          CPLError(CE_Failure, CPLE_AppDefined, "dynamic_cast failed.");
          return NULL;
        }

        camera = poKmlFactory->CreateCamera();
        camera->set_latitude(poOgrPoint->getY());
        camera->set_longitude(poOgrPoint->getX());
        int isGX = FALSE;
        const int iAltitudeMode =
            poOgrFeat->GetFieldIndex(oFC.altitudeModefield);
        int nAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;
        if( iAltitudeMode >= 0 && poOgrFeat->IsFieldSet(iAltitudeMode) )
        {
            nAltitudeMode = kmlAltitudeModeFromString(
                poOgrFeat->GetFieldAsString(iAltitudeMode), isGX);
            camera->set_altitudemode(nAltitudeMode);
        }
        else if( CPLTestBool(
                     CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Camera should define altitudeMode != 'clampToGround'");
        }

        if( poOgrPoint->getCoordinateDimension() == 3 )
        {
            camera->set_altitude(poOgrPoint->getZ());
        }
        else if( CPLTestBool(
                     CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "Camera should have an altitude/Z");
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
        const PlacemarkPtr poKmlPlacemark = poKmlFactory->CreatePlacemark();
        poKmlFeature = poKmlPlacemark;

        ElementPtr poKmlElement = geom2kml( poOgrGeom, -1, poKmlFactory );

        poKmlPlacemark->set_geometry( AsGeometry( poKmlElement ) );
    }

    if( camera == NULL )
        camera = feat2kmlcamera(oFC, iHeading, iTilt, iRoll,
                                poOgrFeat, poKmlFactory);
    if( camera != NULL )
        poKmlFeature->set_abstractview(camera);

    /***** style *****/
    featurestyle2kml( poOgrDS, poOgrLayer, poOgrFeat, poKmlFactory,
                      poKmlFeature );

    /***** fields *****/
    OGRLIBKMLLayer * const poKmlLayer =
        dynamic_cast<OGRLIBKMLLayer *>(poOgrLayer);
    if( poKmlLayer == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "dynamic_cast failed.");
        return NULL;
    }
    field2kml( poOgrFeat, poKmlLayer, poKmlFactory,
               poKmlFeature, bUseSimpleField );

    return poKmlFeature;
}

OGRFeature *kml2feat(
    PlacemarkPtr poKmlPlacemark,
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeatureDefn * poOgrFeatDefn,
    OGRSpatialReference *poOgrSRS )
{
    OGRFeature *poOgrFeat = new OGRFeature( poOgrFeatDefn );

    /***** style *****/
    kml2featurestyle( poKmlPlacemark, poOgrDS, poOgrLayer, poOgrFeat );

    /***** geometry *****/
    if( poKmlPlacemark->has_geometry() )
    {
        OGRGeometry * const poOgrGeom =
            kml2geom( poKmlPlacemark->get_geometry(), poOgrSRS );
        poOgrFeat->SetGeometryDirectly( poOgrGeom );
    }
    else if( poKmlPlacemark->has_abstractview() &&
             poKmlPlacemark->get_abstractview()->IsA( kmldom::Type_Camera) )
    {
        const CameraPtr& camera = AsCamera(poKmlPlacemark->get_abstractview());
        if( camera->has_longitude() && camera->has_latitude() )
        {
            if( camera->has_altitude() )
                poOgrFeat->SetGeometryDirectly(
                    new OGRPoint( camera->get_longitude(),
                                  camera->get_latitude(),
                                  camera->get_altitude() ) );
            else
                poOgrFeat->SetGeometryDirectly(
                    new OGRPoint( camera->get_longitude(),
                                 camera->get_latitude() ) );
            poOgrFeat->GetGeometryRef()->assignSpatialReference( poOgrSRS );
        }
    }

    /***** fields *****/
    kml2field( poOgrFeat, AsFeature( poKmlPlacemark ) );

    return poOgrFeat;
}

OGRFeature *kmlgroundoverlay2feat(
    GroundOverlayPtr poKmlOverlay,
    OGRLIBKMLDataSource * /* poOgrDS */,
    OGRLayer * /* poOgrLayer */,
    OGRFeatureDefn * poOgrFeatDefn,
    OGRSpatialReference *poOgrSRS)
{
    OGRFeature *poOgrFeat = new OGRFeature( poOgrFeatDefn );

    /***** geometry *****/
    if( poKmlOverlay->has_latlonbox() )
    {
        OGRGeometry * const poOgrGeom =
            kml2geom_latlonbox( poKmlOverlay->get_latlonbox(), poOgrSRS );
        poOgrFeat->SetGeometryDirectly( poOgrGeom );
    }
    else if( poKmlOverlay->has_gx_latlonquad() )
    {
        OGRGeometry * const poOgrGeom =
            kml2geom_latlonquad( poKmlOverlay->get_gx_latlonquad(), poOgrSRS );
        poOgrFeat->SetGeometryDirectly( poOgrGeom );
    }

    /***** fields *****/
    kml2field( poOgrFeat, AsFeature( poKmlOverlay ) );

    return poOgrFeat;
}
