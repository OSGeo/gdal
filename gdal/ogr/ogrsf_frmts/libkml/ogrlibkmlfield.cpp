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

#include "libkml_headers.h"

#include <string>

#include <ogr_feature.h>
#include "ogr_p.h"
#include <ogrsf_frmts.h>

using kmldom::CameraPtr;
using kmldom::DataPtr;
using kmldom::ExtendedDataPtr;
using kmldom::FeaturePtr;
using kmldom::GeometryPtr;
using kmldom::GroundOverlayPtr;
using kmldom::GxMultiTrackPtr;
using kmldom::GxTrackPtr;
using kmldom::IconPtr;
using kmldom::KmlFactory;
using kmldom::LineStringPtr;
using kmldom::MultiGeometryPtr;
using kmldom::PlacemarkPtr;
using kmldom::PointPtr;
using kmldom::PolygonPtr;
using kmldom::SchemaDataPtr;
using kmldom::SchemaPtr;
using kmldom::SimpleDataPtr;
using kmldom::SimpleFieldPtr;
using kmldom::SnippetPtr;
using kmldom::TimePrimitivePtr;
using kmldom::TimeSpanPtr;
using kmldom::TimeStampPtr;

#include "ogr_libkml.h"

#include "ogrlibkmlfield.h"

static void ogr2altitudemode_rec (
    GeometryPtr poKmlGeometry,
    int iAltitudeMode,
    int isGX )
{

    PointPtr poKmlPoint;
    LineStringPtr poKmlLineString;
    PolygonPtr poKmlPolygon;
    MultiGeometryPtr poKmlMultiGeometry;

    size_t nGeom;
    size_t i;

    switch ( poKmlGeometry->Type (  ) ) {

    case kmldom::Type_Point:
        poKmlPoint = AsPoint ( poKmlGeometry );

        if ( !isGX )
            poKmlPoint->set_altitudemode ( iAltitudeMode );
        else
            poKmlPoint->set_gx_altitudemode ( iAltitudeMode );

        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );

        if ( !isGX )
            poKmlLineString->set_altitudemode ( iAltitudeMode );
        else
            poKmlLineString->set_gx_altitudemode ( iAltitudeMode );

        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        if ( !isGX )
            poKmlPolygon->set_altitudemode ( iAltitudeMode );
        else
            poKmlPolygon->set_gx_altitudemode ( iAltitudeMode );

        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            ogr2altitudemode_rec ( poKmlMultiGeometry->
                                   get_geometry_array_at ( i ), iAltitudeMode,
                                   isGX );
        }

        break;

    default:
        break;

    }

}

static void ogr2extrude_rec (
    bool bExtrude,
    GeometryPtr poKmlGeometry )
{

    PointPtr poKmlPoint;
    LineStringPtr poKmlLineString;
    PolygonPtr poKmlPolygon;
    MultiGeometryPtr poKmlMultiGeometry;

    size_t nGeom;
    size_t i;

    switch ( poKmlGeometry->Type (  ) ) {
    case kmldom::Type_Point:
        poKmlPoint = AsPoint ( poKmlGeometry );
        poKmlPoint->set_extrude ( bExtrude );
        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );
        poKmlLineString->set_extrude ( bExtrude );
        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );
        poKmlPolygon->set_extrude ( bExtrude );
        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            ogr2extrude_rec ( bExtrude,
                              poKmlMultiGeometry->
                              get_geometry_array_at ( i ) );
        }
        break;

    default:
        break;

    }
}

static void ogr2tessellate_rec (
    bool bTessellate,
    GeometryPtr poKmlGeometry )
{

    LineStringPtr poKmlLineString;
    PolygonPtr poKmlPolygon;
    MultiGeometryPtr poKmlMultiGeometry;

    size_t nGeom;
    size_t i;

    switch ( poKmlGeometry->Type (  ) ) {

    case kmldom::Type_Point:
        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );
        poKmlLineString->set_tessellate ( bTessellate );
        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        poKmlPolygon->set_tessellate ( bTessellate );
        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            ogr2tessellate_rec ( bTessellate,
                                 poKmlMultiGeometry->
                                 get_geometry_array_at ( i ) );
        }

        break;

    default:
        break;

    }
}


/************************************************************************/
/*                 OGRLIBKMLSanitizeUTF8String()                        */
/************************************************************************/

static char* OGRLIBKMLSanitizeUTF8String(const char* pszString)
{
    if (!CPLIsUTF8(pszString, -1) &&
         CPLTestBool(CPLGetConfigOption("OGR_FORCE_ASCII", "YES")))
    {
        static int bFirstTime = TRUE;
        if (bFirstTime)
        {
            bFirstTime = FALSE;
            CPLError(CE_Warning, CPLE_AppDefined,
                    "%s is not a valid UTF-8 string. Forcing it to ASCII.\n"
                    "If you still want the original string and change the XML file encoding\n"
                    "afterwards, you can define OGR_FORCE_ASCII=NO as configuration option.\n"
                    "This warning won't be issued anymore", pszString);
        }
        else
        {
            CPLDebug("OGR", "%s is not a valid UTF-8 string. Forcing it to ASCII",
                    pszString);
        }
        return CPLForceToASCII(pszString, -1, '?');
    }
    else
        return CPLStrdup(pszString);
}

/******************************************************************************
 function to output ogr fields in kml

 args:
        poOgrFeat       pointer to the feature the field is in
        poOgrLayer      pointer to the layer the feature is in
        poKmlFactory    pointer to the libkml dom factory
        poKmlPlacemark  pointer to the placemark to add to

 returns:
        nothing

 env vars:
  LIBKML_TIMESTAMP_FIELD         default: OFTDate or OFTDateTime named timestamp
  LIBKML_TIMESPAN_BEGIN_FIELD    default: OFTDate or OFTDateTime named begin
  LIBKML_TIMESPAN_END_FIELD      default: OFTDate or OFTDateTime named end
  LIBKML_DESCRIPTION_FIELD       default: none
  LIBKML_NAME_FIELD              default: OFTString field named name


******************************************************************************/

void field2kml (
    OGRFeature * poOgrFeat,
    OGRLIBKMLLayer * poOgrLayer,
    KmlFactory * poKmlFactory,
    FeaturePtr poKmlFeature,
    int bUseSimpleField)
{
    int i;

    ExtendedDataPtr poKmlExtendedData = NULL;
    SchemaDataPtr poKmlSchemaData = NULL;
    if( bUseSimpleField )
    {
        poKmlSchemaData = poKmlFactory->CreateSchemaData (  );
        SchemaPtr poKmlSchema = poOgrLayer->GetKmlSchema (  );

        /***** set the url to the schema *****/

        if ( poKmlSchema && poKmlSchema->has_id (  ) ) {
            std::string oKmlSchemaID = poKmlSchema->get_id (  );


            std::string oKmlSchemaURL = "#";
            oKmlSchemaURL.append ( oKmlSchemaID );

            poKmlSchemaData->set_schemaurl ( oKmlSchemaURL );
        }
    }

    /***** get the field config *****/

    struct fieldconfig oFC;
    get_fieldconfig( &oFC );

    TimeSpanPtr poKmlTimeSpan = NULL;

    int nFields = poOgrFeat->GetFieldCount (  );
    int iSkip1 = -1;
    int iSkip2 = -1;
    int iAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;
    int isGX = FALSE;

    for ( i = 0; i < nFields; i++ ) {

        /***** if the field is set to skip, do so *****/

        if ( i == iSkip1 || i == iSkip2 )
            continue;

        /***** if the field isn't set just bail now *****/

        if ( !poOgrFeat->IsFieldSet ( i ) )
            continue;

        OGRFieldDefn *poOgrFieldDef = poOgrFeat->GetFieldDefnRef ( i );
        OGRFieldType type = poOgrFieldDef->GetType (  );
        const char *name = poOgrFieldDef->GetNameRef (  );

        SimpleDataPtr poKmlSimpleData = NULL;
        DataPtr poKmlData = NULL;
        OGRField sFieldDT;

        switch ( type ) {

        case OFTString:        //     String of ASCII chars
            {
                char* pszUTF8String = OGRLIBKMLSanitizeUTF8String(
                                        poOgrFeat->GetFieldAsString ( i ));
                if( pszUTF8String[0] == '\0' )
                {
                    CPLFree( pszUTF8String );
                    continue;
                }

                /***** name *****/

                if ( EQUAL ( name, oFC.namefield ) ) {
                    poKmlFeature->set_name ( pszUTF8String );
                    CPLFree( pszUTF8String );
                    continue;
                }

                /***** description *****/

                else if ( EQUAL ( name, oFC.descfield ) ) {
                    poKmlFeature->set_description ( pszUTF8String );
                    CPLFree( pszUTF8String );
                    continue;
                }

                /***** altitudemode *****/

                else if ( EQUAL ( name, oFC.altitudeModefield ) ) {
                    const char *pszAltitudeMode = pszUTF8String ;

                    iAltitudeMode = kmlAltitudeModeFromString(pszAltitudeMode, isGX);

                    if ( poKmlFeature->IsA ( kmldom::Type_Placemark ) ) {
                        PlacemarkPtr poKmlPlacemark = AsPlacemark ( poKmlFeature );
                        if ( poKmlPlacemark->has_geometry (  ) ) {
                            GeometryPtr poKmlGeometry =
                                poKmlPlacemark->get_geometry (  );

                            ogr2altitudemode_rec ( poKmlGeometry, iAltitudeMode,
                                                isGX );

                        }
                    }

                    CPLFree( pszUTF8String );

                    continue;
                }

                /***** timestamp *****/

                else if ( EQUAL ( name, oFC.tsfield ) ) {

                    TimeStampPtr poKmlTimeStamp =
                        poKmlFactory->CreateTimeStamp (  );
                    poKmlTimeStamp->set_when ( pszUTF8String  );
                    poKmlFeature->set_timeprimitive ( poKmlTimeStamp );

                    CPLFree( pszUTF8String );

                    continue;
                }

                /***** begin *****/

                if ( EQUAL ( name, oFC.beginfield ) ) {

                    if ( !poKmlTimeSpan ) {
                        poKmlTimeSpan = poKmlFactory->CreateTimeSpan (  );
                        poKmlFeature->set_timeprimitive ( poKmlTimeSpan );
                    }

                    poKmlTimeSpan->set_begin ( pszUTF8String );

                    CPLFree( pszUTF8String );

                    continue;

                }

                /***** end *****/

                else if ( EQUAL ( name, oFC.endfield ) ) {

                    if ( !poKmlTimeSpan ) {
                        poKmlTimeSpan = poKmlFactory->CreateTimeSpan (  );
                        poKmlFeature->set_timeprimitive ( poKmlTimeSpan );
                    }

                    poKmlTimeSpan->set_end ( pszUTF8String );

                    CPLFree( pszUTF8String );

                    continue;
                }

                /***** snippet *****/

                else if  ( EQUAL ( name, oFC.snippetfield ) ) {

                    SnippetPtr snippet = poKmlFactory->CreateSnippet (  );
                    snippet->set_text(pszUTF8String);
                    poKmlFeature->set_snippet ( snippet );

                    CPLFree( pszUTF8String );

                    continue;

                }

                /***** other special fields *****/

                else if (  EQUAL ( name, oFC.iconfield ) ||
                           EQUAL ( name, oFC.modelfield ) ||
                           EQUAL ( name, oFC.networklinkfield ) ||
                           EQUAL ( name, oFC.networklink_refreshMode_field ) ||
                           EQUAL ( name, oFC.networklink_viewRefreshMode_field ) ||
                           EQUAL ( name, oFC.networklink_viewFormat_field ) ||
                           EQUAL ( name, oFC.networklink_httpQuery_field ) ||
                           EQUAL ( name, oFC.camera_altitudemode_field ) ||
                           EQUAL ( name, oFC.photooverlayfield ) ||
                           EQUAL ( name, oFC.photooverlay_shape_field ) ||
                           EQUAL ( name, oFC.imagepyramid_gridorigin_field ) ) {

                    CPLFree( pszUTF8String );

                    continue;
                }

                /***** other *****/

                if( bUseSimpleField )
                {
                    poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
                    poKmlSimpleData->set_name ( name );
                    poKmlSimpleData->set_text ( pszUTF8String );
                }
                else
                {
                    poKmlData = poKmlFactory->CreateData (  );
                    poKmlData->set_name ( name );
                    poKmlData->set_value ( pszUTF8String );
                }

                CPLFree( pszUTF8String );

                break;
            }

        /* This code checks if there's a OFTTime field with the same name */
        /* that could be used to compose a DateTime. Not sure this is really */
        /* supported in OGR data model to have 2 fields with same name... */
        case OFTDate:          //   Date
            {
                memcpy(&sFieldDT, poOgrFeat->GetRawFieldRef(i), sizeof(OGRField));

                int iTimeField;

                for ( iTimeField = i + 1; iTimeField < nFields; iTimeField++ ) {
                    if ( iTimeField == iSkip1 || iTimeField == iSkip2 )
                        continue;

                    OGRFieldDefn *poOgrFieldDef2 =
                        poOgrFeat->GetFieldDefnRef ( i );
                    OGRFieldType type2 = poOgrFieldDef2->GetType (  );
                    const char *name2 = poOgrFieldDef2->GetNameRef (  );

                    if ( EQUAL ( name2, name ) && type2 == OFTTime &&
                         ( EQUAL ( name, oFC.tsfield ) ||
                           EQUAL ( name, oFC.beginfield ) ||
                           EQUAL ( name, oFC.endfield ) ) ) {

                        const OGRField* psField2 = poOgrFeat->GetRawFieldRef(iTimeField);
                        sFieldDT.Date.Hour = psField2->Date.Hour;
                        sFieldDT.Date.Minute = psField2->Date.Minute;
                        sFieldDT.Date.Second = psField2->Date.Second;
                        sFieldDT.Date.TZFlag = psField2->Date.TZFlag;

                        if ( 0 > iSkip1 )
                            iSkip1 = iTimeField;
                        else
                            iSkip2 = iTimeField;
                    }
                }

                goto Do_DateTime;

            }

        /* This code checks if there's a OFTTime field with the same name */
        /* that could be used to compose a DateTime. Not sure this is really */
        /* supported in OGR data model to have 2 fields with same name... */
        case OFTTime:          //   Time
            {
                memcpy(&sFieldDT, poOgrFeat->GetRawFieldRef(i), sizeof(OGRField));

                int iTimeField;

                for ( iTimeField = i + 1; iTimeField < nFields; iTimeField++ ) {
                    if ( iTimeField == iSkip1 || iTimeField == iSkip2 )
                        continue;

                    OGRFieldDefn *poOgrFieldDef2 =
                        poOgrFeat->GetFieldDefnRef ( i );
                    OGRFieldType type2 = poOgrFieldDef2->GetType (  );
                    const char *name2 = poOgrFieldDef2->GetNameRef (  );

                    if ( EQUAL ( name2, name ) && type2 == OFTDate &&
                         ( EQUAL ( name, oFC.tsfield ) ||
                           EQUAL ( name, oFC.beginfield ) ||
                           EQUAL ( name, oFC.endfield ) ) ) {

                        const OGRField* psField2 = poOgrFeat->GetRawFieldRef(iTimeField);
                        sFieldDT.Date.Year = psField2->Date.Year;
                        sFieldDT.Date.Month = psField2->Date.Month;
                        sFieldDT.Date.Day = psField2->Date.Day;

                        if ( 0 > iSkip1 )
                            iSkip1 = iTimeField;
                        else
                            iSkip2 = iTimeField;
                    }
                }

                goto Do_DateTime;

            }

        case OFTDateTime:      //  Date and Time
            {
              memcpy(&sFieldDT, poOgrFeat->GetRawFieldRef(i), sizeof(OGRField));

              Do_DateTime:
                /***** timestamp *****/

                if ( EQUAL ( name, oFC.tsfield ) ) {

                    char *timebuf = OGRGetXMLDateTime ( &sFieldDT );

                    TimeStampPtr poKmlTimeStamp =
                        poKmlFactory->CreateTimeStamp (  );
                    poKmlTimeStamp->set_when ( timebuf );
                    poKmlFeature->set_timeprimitive ( poKmlTimeStamp );
                    CPLFree( timebuf );

                    continue;
                }

                /***** begin *****/

                if ( EQUAL ( name, oFC.beginfield ) ) {

                    char *timebuf = OGRGetXMLDateTime ( &sFieldDT );

                    if ( !poKmlTimeSpan ) {
                        poKmlTimeSpan = poKmlFactory->CreateTimeSpan (  );
                        poKmlFeature->set_timeprimitive ( poKmlTimeSpan );
                    }

                    poKmlTimeSpan->set_begin ( timebuf );
                    CPLFree( timebuf );

                    continue;

                }

                /***** end *****/

                else if ( EQUAL ( name, oFC.endfield ) ) {

                    char *timebuf = OGRGetXMLDateTime ( &sFieldDT );


                    if ( !poKmlTimeSpan ) {
                        poKmlTimeSpan = poKmlFactory->CreateTimeSpan (  );
                        poKmlFeature->set_timeprimitive ( poKmlTimeSpan );
                    }

                    poKmlTimeSpan->set_end ( timebuf );
                    CPLFree( timebuf );

                    continue;
                }

                /***** other *****/

                if( bUseSimpleField )
                {
                    poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
                    poKmlSimpleData->set_name ( name );
                    poKmlSimpleData->set_text ( poOgrFeat->
                                                GetFieldAsString ( i ) );
                }
                else
                {
                    poKmlData = poKmlFactory->CreateData (  );
                    poKmlData->set_name ( name );
                    poKmlData->set_value ( poOgrFeat->
                                                GetFieldAsString ( i ) );
                }

                break;
            }

        case OFTInteger:       //    Simple 32bit integer

            /***** extrude *****/

            if ( EQUAL ( name, oFC.extrudefield ) ) {

                if ( poKmlFeature->IsA ( kmldom::Type_Placemark ) ) {
                    PlacemarkPtr poKmlPlacemark = AsPlacemark ( poKmlFeature );
                    if ( poKmlPlacemark->has_geometry (  )
                        && -1 < poOgrFeat->GetFieldAsInteger ( i ) ) {
                        int iExtrude = poOgrFeat->GetFieldAsInteger ( i );
                        if( iExtrude &&
                            isGX == FALSE && iAltitudeMode == kmldom::ALTITUDEMODE_CLAMPTOGROUND &&
                            CPLTestBool(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
                        {
                            CPLError(CE_Warning, CPLE_NotSupported,
                                "altitudeMode=clampToGround unsupported with extrude=1");
                        }
                        else
                        {
                            GeometryPtr poKmlGeometry =
                                poKmlPlacemark->get_geometry (  );
                            ogr2extrude_rec ( CPL_TO_BOOL(iExtrude),
                                            poKmlGeometry );
                        }
                    }
                }
                continue;
            }

            /***** tessellate *****/


            if ( EQUAL ( name, oFC.tessellatefield ) ) {

                if ( poKmlFeature->IsA ( kmldom::Type_Placemark ) ) {
                    PlacemarkPtr poKmlPlacemark = AsPlacemark ( poKmlFeature );
                    if ( poKmlPlacemark->has_geometry (  )
                        && -1 < poOgrFeat->GetFieldAsInteger ( i ) ) {
                        int iTesselate = poOgrFeat->GetFieldAsInteger ( i );
                        if( iTesselate &&
                            !(isGX == FALSE && static_cast<kmldom::AltitudeModeEnum>(iAltitudeMode) == kmldom::ALTITUDEMODE_CLAMPTOGROUND) &&
                            !(isGX == TRUE && static_cast<kmldom::GxAltitudeModeEnum>(iAltitudeMode) == kmldom::GX_ALTITUDEMODE_CLAMPTOSEAFLOOR) &&
                            CPLTestBool(CPLGetConfigOption("LIBKML_STRICT_COMPLIANCE", "TRUE")) )
                        {
                            CPLError( CE_Warning, CPLE_NotSupported,
                                      "altitudeMode!=clampToGround && "
                                      "altitudeMode!=clampToSeaFloor "
                                      "unsupported with tessellate=1" );
                        }
                        else
                        {
                            GeometryPtr poKmlGeometry =
                                poKmlPlacemark->get_geometry (  );
                            ogr2tessellate_rec ( CPL_TO_BOOL(iTesselate),
                                                poKmlGeometry );
                            if( isGX == FALSE && iAltitudeMode == kmldom::ALTITUDEMODE_CLAMPTOGROUND )
                                ogr2altitudemode_rec ( poKmlGeometry, iAltitudeMode,
                                                    isGX );
                        }
                    }
                }

                continue;
            }


            /***** visibility *****/

            if ( EQUAL ( name, oFC.visibilityfield ) ) {
                if ( -1 < poOgrFeat->GetFieldAsInteger ( i ) )
                    poKmlFeature->set_visibility ( CPL_TO_BOOL(poOgrFeat->
                                                     GetFieldAsInteger ( i )) );

                continue;
            }


            /***** other special fields *****/

            else if (  EQUAL ( name, oFC.drawOrderfield ) ||
                        EQUAL ( name, oFC.networklink_refreshvisibility_field ) ||
                        EQUAL ( name, oFC.networklink_flytoview_field ) ||
                        EQUAL ( name, oFC.networklink_refreshInterval_field ) ||
                        EQUAL ( name, oFC.networklink_viewRefreshMode_field ) ||
                        EQUAL ( name, oFC.networklink_viewRefreshTime_field ) ||
                        EQUAL ( name, oFC.imagepyramid_tilesize_field ) ||
                        EQUAL ( name, oFC.imagepyramid_maxwidth_field ) ||
                        EQUAL ( name, oFC.imagepyramid_maxheight_field ) ) {

                continue;
            }

            /***** other *****/

            if( bUseSimpleField )
            {
                poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
                poKmlSimpleData->set_name ( name );
                poKmlSimpleData->set_text ( poOgrFeat->GetFieldAsString ( i ) );
            }
            else
            {
                poKmlData = poKmlFactory->CreateData (  );
                poKmlData->set_name ( name );
                poKmlData->set_value ( poOgrFeat->GetFieldAsString ( i ) );
            }

            break;

        case OFTReal:          //   Double Precision floating point
        {
            if( EQUAL(name, oFC.headingfield) ||
                EQUAL(name, oFC.tiltfield) ||
                EQUAL(name, oFC.rollfield) ||
                EQUAL(name, oFC.scalexfield) ||
                EQUAL(name, oFC.scaleyfield) ||
                EQUAL(name, oFC.scalezfield) ||
                EQUAL(name, oFC.networklink_refreshInterval_field ) ||
                EQUAL(name, oFC.networklink_viewRefreshMode_field) ||
                EQUAL(name, oFC.networklink_viewRefreshTime_field) ||
                EQUAL(name, oFC.networklink_viewBoundScale_field) ||
                EQUAL(name, oFC.camera_longitude_field) ||
                EQUAL(name, oFC.camera_latitude_field) ||
                EQUAL(name, oFC.camera_altitude_field) ||
                EQUAL(name, oFC.leftfovfield) ||
                EQUAL(name, oFC.rightfovfield) ||
                EQUAL(name, oFC.bottomfovfield) ||
                EQUAL(name, oFC.topfovfield) ||
                EQUAL(name, oFC.nearfield) ||
                EQUAL(name, oFC.camera_altitude_field) )
            {
                continue;
            }

            char* pszStr = CPLStrdup( poOgrFeat->GetFieldAsString ( i ) );

            if( bUseSimpleField )
            {
                poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
                poKmlSimpleData->set_name ( name );
                poKmlSimpleData->set_text ( pszStr );
            }
            else
            {
                poKmlData = poKmlFactory->CreateData (  );
                poKmlData->set_name ( name );
                poKmlData->set_value ( pszStr );
            }

            CPLFree(pszStr);

            break;
        }

        case OFTStringList:    //     Array of strings
        case OFTIntegerList:   //    List of 32bit integers
        case OFTRealList:   //    List of doubles
        case OFTBinary:        //     Raw Binary data
        case OFTWideStringList:    //     deprecated
        default:

        /***** other *****/

            if( bUseSimpleField )
            {
                poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
                poKmlSimpleData->set_name ( name );
                poKmlSimpleData->set_text ( poOgrFeat->GetFieldAsString ( i ) );
            }
            else
            {
                poKmlData = poKmlFactory->CreateData (  );
                poKmlData->set_name ( name );
                poKmlData->set_value ( poOgrFeat->GetFieldAsString ( i ) );
            }

            break;
        }

        if( poKmlSimpleData )
        {
            poKmlSchemaData->add_simpledata ( poKmlSimpleData );
        }
        else if( poKmlData )
        {
            if( poKmlExtendedData == NULL )
                poKmlExtendedData = poKmlFactory->CreateExtendedData (  );
            poKmlExtendedData->add_data ( poKmlData );
        }
    }

    // Do not add it to the placemark unless there is data.

    if ( bUseSimpleField && poKmlSchemaData->get_simpledata_array_size (  ) > 0 ) {
        poKmlExtendedData = poKmlFactory->CreateExtendedData (  );
        poKmlExtendedData->add_schemadata ( poKmlSchemaData );
    }
    if( poKmlExtendedData != NULL )
    {
        poKmlFeature->set_extendeddata ( poKmlExtendedData );
    }

    return;
}

/******************************************************************************
 recursive function to read altitude mode from the geometry
******************************************************************************/

static int kml2altitudemode_rec (
    GeometryPtr poKmlGeometry,
    int *pnAltitudeMode,
    int *pbIsGX )
{

    PointPtr poKmlPoint;
    LineStringPtr poKmlLineString;
    PolygonPtr poKmlPolygon;
    MultiGeometryPtr poKmlMultiGeometry;

    size_t nGeom;
    size_t i;

    switch ( poKmlGeometry->Type (  ) ) {

    case kmldom::Type_Point:
        poKmlPoint = AsPoint ( poKmlGeometry );

        if ( poKmlPoint->has_altitudemode (  ) ) {
            *pnAltitudeMode = poKmlPoint->get_altitudemode (  );
            return TRUE;
        }
        else if ( poKmlPoint->has_gx_altitudemode (  ) ) {
            *pnAltitudeMode = poKmlPoint->get_gx_altitudemode (  );
            *pbIsGX = TRUE;
            return TRUE;
        }

        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );

        if ( poKmlLineString->has_altitudemode (  ) ) {
            *pnAltitudeMode = poKmlLineString->get_altitudemode (  );
            return TRUE;
        }
        else if ( poKmlLineString->has_gx_altitudemode (  ) ) {
            *pnAltitudeMode = poKmlLineString->get_gx_altitudemode (  );
            *pbIsGX = TRUE;
            return TRUE;
        }
        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        if ( poKmlPolygon->has_altitudemode (  ) ) {
            *pnAltitudeMode = poKmlPolygon->get_altitudemode (  );
            return TRUE;
        }
        else if ( poKmlPolygon->has_gx_altitudemode (  ) ) {
            *pnAltitudeMode = poKmlPolygon->get_gx_altitudemode (  );
            *pbIsGX = TRUE;
            return TRUE;
        }

        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            if ( kml2altitudemode_rec ( poKmlMultiGeometry->
                                        get_geometry_array_at ( i ),
                                        pnAltitudeMode, pbIsGX ) )
                return TRUE;
        }

        break;

    default:
        break;

    }

    return FALSE;
}

/******************************************************************************
 recursive function to read extrude from the geometry
******************************************************************************/

static int kml2extrude_rec (
    GeometryPtr poKmlGeometry,
    bool *pbExtrude )
{

    PointPtr poKmlPoint;
    LineStringPtr poKmlLineString;
    PolygonPtr poKmlPolygon;
    MultiGeometryPtr poKmlMultiGeometry;

    size_t nGeom;
    size_t i;

    switch ( poKmlGeometry->Type (  ) ) {

    case kmldom::Type_Point:
        poKmlPoint = AsPoint ( poKmlGeometry );

        if ( poKmlPoint->has_extrude (  ) ) {
            *pbExtrude = poKmlPoint->get_extrude (  );
            return TRUE;
        }

        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );

        if ( poKmlLineString->has_extrude (  ) ) {
            *pbExtrude = poKmlLineString->get_extrude (  );
            return TRUE;
        }

        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        if ( poKmlPolygon->has_extrude (  ) ) {
            *pbExtrude = poKmlPolygon->get_extrude (  );
            return TRUE;
        }

        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            if ( kml2extrude_rec ( poKmlMultiGeometry->
                                   get_geometry_array_at ( i ), pbExtrude ) )
                return TRUE;
        }

        break;

    default:
        break;

    }

    return FALSE;
}

/******************************************************************************
 recursive function to read tessellate from the geometry
******************************************************************************/

static int kml2tessellate_rec (
    GeometryPtr poKmlGeometry,
    int *pnTessellate )
{

    LineStringPtr poKmlLineString;
    PolygonPtr poKmlPolygon;
    MultiGeometryPtr poKmlMultiGeometry;

    size_t nGeom;
    size_t i;

    switch ( poKmlGeometry->Type (  ) ) {

    case kmldom::Type_Point:
        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );

        if ( poKmlLineString->has_tessellate (  ) ) {
            *pnTessellate = poKmlLineString->get_tessellate (  );
            return TRUE;
        }

        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        if ( poKmlPolygon->has_tessellate (  ) ) {
            *pnTessellate = poKmlPolygon->get_tessellate (  );
            return TRUE;
        }

        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            if ( kml2tessellate_rec ( poKmlMultiGeometry->
                                      get_geometry_array_at ( i ),
                                      pnTessellate ) )
                return TRUE;
        }

        break;

    default:
        break;

    }

    return FALSE;
}

/************************************************************************/
/*                     ogrkmlSetAltitudeMode()                          */
/************************************************************************/

static void ogrkmlSetAltitudeMode(OGRFeature* poOgrFeat, int iField,
                                  int nAltitudeMode, int bIsGX)
{
    if ( !bIsGX ) {
        switch ( nAltitudeMode ) {
        case kmldom::ALTITUDEMODE_CLAMPTOGROUND:
            poOgrFeat->SetField ( iField, "clampToGround" );
            break;

        case kmldom::ALTITUDEMODE_RELATIVETOGROUND:
            poOgrFeat->SetField ( iField, "relativeToGround" );
            break;

        case kmldom::ALTITUDEMODE_ABSOLUTE:
            poOgrFeat->SetField ( iField, "absolute" );
            break;

        }
    }

    else {
        switch ( nAltitudeMode ) {
        case kmldom::GX_ALTITUDEMODE_RELATIVETOSEAFLOOR:
            poOgrFeat->SetField ( iField, "relativeToSeaFloor" );
            break;

        case kmldom::GX_ALTITUDEMODE_CLAMPTOSEAFLOOR:
            poOgrFeat->SetField ( iField, "clampToSeaFloor" );
            break;
        }
    }
}

/************************************************************************/
/*                            TrimSpaces()                              */
/************************************************************************/

static const char* TrimSpaces(string& oText)
{

    /* SerializePretty() adds a new line before the data */
    /* ands trailing spaces. I believe this is wrong */
    /* as it breaks round-tripping */

    /* Trim trailing spaces */
    while (oText.size() != 0 && oText[oText.size()-1] == ' ')
        oText.resize(oText.size()-1);

    /* Skip leading newline and spaces */
    const char* pszText = oText.c_str (  );
    if (pszText[0] == '\n')
        pszText ++;
    while (pszText[0] == ' ')
        pszText ++;

    return pszText;
}

/************************************************************************/
/*                            kmldatetime2ogr()                         */
/************************************************************************/

static void kmldatetime2ogr( OGRFeature* poOgrFeat,
                             const char* pszOGRField,
                             const std::string& osKmlDateTime )
{
    int iField = poOgrFeat->GetFieldIndex ( pszOGRField );

    if ( iField > -1 ) {
        OGRField sField;

        if ( OGRParseXMLDateTime( osKmlDateTime.c_str (  ), &sField ) )
            poOgrFeat->SetField ( iField, &sField );
    }
}

/******************************************************************************
 function to read kml into ogr fields
******************************************************************************/

void kml2field (
    OGRFeature * poOgrFeat,
    FeaturePtr poKmlFeature )
{

    /***** get the field config *****/

    struct fieldconfig oFC;
    get_fieldconfig( &oFC );

    /***** name *****/

    if ( poKmlFeature->has_name (  ) ) {
        const std::string oKmlName = poKmlFeature->get_name (  );
        int iField = poOgrFeat->GetFieldIndex ( oFC.namefield );

        if ( iField > -1 )
            poOgrFeat->SetField ( iField, oKmlName.c_str (  ) );
    }

    /***** description *****/

    if ( poKmlFeature->has_description (  ) ) {
        const std::string oKmlDesc = poKmlFeature->get_description (  );
        int iField = poOgrFeat->GetFieldIndex ( oFC.descfield );

        if ( iField > -1 )
            poOgrFeat->SetField ( iField, oKmlDesc.c_str (  ) );
    }

    if ( poKmlFeature->has_timeprimitive (  ) ) {
        TimePrimitivePtr poKmlTimePrimitive =
            poKmlFeature->get_timeprimitive (  );

        /***** timestamp *****/

        if ( poKmlTimePrimitive->IsA ( kmldom::Type_TimeStamp ) ) {
            // probably a libkml bug: AsTimeStamp should really return not NULL on a gx:TimeStamp
            TimeStampPtr poKmlTimeStamp = AsTimeStamp ( poKmlTimePrimitive );
            if( poKmlTimeStamp == NULL )
                poKmlTimeStamp = AsGxTimeStamp ( poKmlTimePrimitive );

            if ( poKmlTimeStamp && poKmlTimeStamp->has_when (  ) ) {
                const std::string oKmlWhen = poKmlTimeStamp->get_when (  );
                kmldatetime2ogr(poOgrFeat, oFC.tsfield, oKmlWhen );
            }
        }

        /***** timespan *****/

        if ( poKmlTimePrimitive->IsA ( kmldom::Type_TimeSpan ) ) {
            // probably a libkml bug: AsTimeSpan should really return not NULL on a gx:TimeSpan
            TimeSpanPtr poKmlTimeSpan = AsTimeSpan ( poKmlTimePrimitive );
            if( poKmlTimeSpan == NULL )
                poKmlTimeSpan = AsGxTimeSpan ( poKmlTimePrimitive );

            /***** begin *****/

            if ( poKmlTimeSpan && poKmlTimeSpan->has_begin (  ) ) {
                const std::string oKmlWhen = poKmlTimeSpan->get_begin (  );
                kmldatetime2ogr(poOgrFeat, oFC.beginfield, oKmlWhen );
            }

            /***** end *****/

            if ( poKmlTimeSpan && poKmlTimeSpan->has_end (  ) ) {
                const std::string oKmlWhen = poKmlTimeSpan->get_end (  );
                kmldatetime2ogr(poOgrFeat, oFC.endfield, oKmlWhen );
            }
        }
    }

    /***** placemark *****/

    PlacemarkPtr poKmlPlacemark = AsPlacemark ( poKmlFeature );
    GroundOverlayPtr poKmlGroundOverlay = AsGroundOverlay ( poKmlFeature );
    if ( poKmlPlacemark && poKmlPlacemark->has_geometry (  ) ) {
        GeometryPtr poKmlGeometry = poKmlPlacemark->get_geometry (  );

        /***** altitudeMode *****/


        int bIsGX = FALSE;
        int nAltitudeMode = -1;

        int iField = poOgrFeat->GetFieldIndex ( oFC.altitudeModefield );

        if ( iField > -1 ) {

            if ( kml2altitudemode_rec ( poKmlGeometry,
                                        &nAltitudeMode, &bIsGX ) ) {
                ogrkmlSetAltitudeMode(poOgrFeat, iField, nAltitudeMode, bIsGX);
            }

        }

        /***** tessellate *****/

        int nTessellate = -1;

        kml2tessellate_rec ( poKmlGeometry, &nTessellate );

        iField = poOgrFeat->GetFieldIndex ( oFC.tessellatefield );
        if ( iField > -1 )
            poOgrFeat->SetField ( iField, nTessellate );

        /***** extrude *****/

        bool bExtrude = false;

        kml2extrude_rec ( poKmlGeometry, &bExtrude );

        iField = poOgrFeat->GetFieldIndex ( oFC.extrudefield );
        if ( iField > -1 )
            poOgrFeat->SetField ( iField, bExtrude ? 1 : 0 );

        /***** special case for gx:Track ******/
        /* we set the first timestamp as begin and the last one as end */
        if ( poKmlGeometry->Type (  )  == kmldom::Type_GxTrack &&
             !poKmlFeature->has_timeprimitive (  ) ) {
            GxTrackPtr poKmlGxTrack = AsGxTrack ( poKmlGeometry );
            size_t nCoords = poKmlGxTrack->get_gx_coord_array_size();
            if( nCoords > 0 )
            {
                kmldatetime2ogr(poOgrFeat, oFC.beginfield,
                            poKmlGxTrack->get_when_array_at ( 0 ).c_str() );
                kmldatetime2ogr(poOgrFeat, oFC.endfield,
                            poKmlGxTrack->get_when_array_at ( nCoords - 1 ).c_str() );
            }
        }

        /***** special case for gx:MultiTrack ******/
        /* we set the first timestamp as begin and the last one as end */
        else if ( poKmlGeometry->Type (  )  == kmldom::Type_GxMultiTrack &&
             !poKmlFeature->has_timeprimitive (  ) ) {
            GxMultiTrackPtr poKmlGxMultiTrack = AsGxMultiTrack ( poKmlGeometry );
            size_t nGeom = poKmlGxMultiTrack->get_gx_track_array_size (  );
            if( nGeom >= 1 )
            {
                GxTrackPtr poKmlGxTrack = poKmlGxMultiTrack->get_gx_track_array_at ( 0 );
                size_t nCoords = poKmlGxTrack->get_gx_coord_array_size();
                if( nCoords > 0 )
                {
                    kmldatetime2ogr(poOgrFeat, oFC.beginfield,
                                poKmlGxTrack->get_when_array_at ( 0 ).c_str() );
                }

                poKmlGxTrack = poKmlGxMultiTrack->get_gx_track_array_at (nGeom -1);
                nCoords = poKmlGxTrack->get_gx_coord_array_size();
                if( nCoords > 0 )
                {
                    kmldatetime2ogr(poOgrFeat, oFC.endfield,
                                poKmlGxTrack->get_when_array_at ( nCoords - 1 ).c_str() );
                }
            }
        }
    }

    /***** camera *****/

    else if ( poKmlPlacemark &&
              poKmlPlacemark->has_abstractview (  ) &&
              poKmlPlacemark->get_abstractview()->IsA( kmldom::Type_Camera) ) {

        const CameraPtr& camera = AsCamera(poKmlPlacemark->get_abstractview());

        if( camera->has_heading() )
        {
            int iField = poOgrFeat->GetFieldIndex ( oFC.headingfield );
            if ( iField > -1 )
                poOgrFeat->SetField ( iField, camera->get_heading() );
        }

        if( camera->has_tilt() )
        {
            int iField = poOgrFeat->GetFieldIndex ( oFC.tiltfield );
            if ( iField > -1 )
                poOgrFeat->SetField ( iField, camera->get_tilt() );
        }

        if( camera->has_roll() )
        {
            int iField = poOgrFeat->GetFieldIndex ( oFC.rollfield );
            if ( iField > -1 )
                poOgrFeat->SetField ( iField, camera->get_roll() );
        }

        int nAltitudeMode = -1;

        int iField = poOgrFeat->GetFieldIndex ( oFC.altitudeModefield );

        if ( iField > -1 ) {

            if ( camera->has_altitudemode (  ) ) {
                nAltitudeMode = camera->get_altitudemode (  );
                ogrkmlSetAltitudeMode(poOgrFeat, iField, nAltitudeMode, FALSE);
            }
            else if ( camera->has_gx_altitudemode (  ) ) {
                nAltitudeMode = camera->get_gx_altitudemode (  );
                ogrkmlSetAltitudeMode(poOgrFeat, iField, nAltitudeMode, TRUE);
            }
        }
    }

    /***** ground overlay *****/

    else if ( poKmlGroundOverlay ) {

        /***** icon *****/

        int iField = poOgrFeat->GetFieldIndex ( oFC.iconfield );
        if ( iField > -1 ) {

            if ( poKmlGroundOverlay->has_icon (  ) ) {
                IconPtr icon = poKmlGroundOverlay->get_icon (  );
                if ( icon->has_href (  ) ) {
                    poOgrFeat->SetField ( iField, icon->get_href (  ).c_str (  ) );
                }
            }
        }

        /***** drawOrder *****/


        iField = poOgrFeat->GetFieldIndex ( oFC.drawOrderfield );
        if ( iField > -1 ) {

            if ( poKmlGroundOverlay->has_draworder (  ) ) {
                poOgrFeat->SetField ( iField, poKmlGroundOverlay->get_draworder (  ) );
            }
        }

        /***** altitudeMode *****/

        iField = poOgrFeat->GetFieldIndex ( oFC.altitudeModefield );

        if ( iField > -1 ) {

            if ( poKmlGroundOverlay->has_altitudemode (  ) ) {
                switch ( poKmlGroundOverlay->get_altitudemode (  ) ) {
                case kmldom::ALTITUDEMODE_CLAMPTOGROUND:
                    poOgrFeat->SetField ( iField, "clampToGround" );
                    break;

                case kmldom::ALTITUDEMODE_RELATIVETOGROUND:
                    poOgrFeat->SetField ( iField, "relativeToGround" );
                    break;

                case kmldom::ALTITUDEMODE_ABSOLUTE:
                    poOgrFeat->SetField ( iField, "absolute" );
                    break;

                }
            } else if ( poKmlGroundOverlay->has_gx_altitudemode (  ) ) {
                switch ( poKmlGroundOverlay->get_gx_altitudemode ( ) ) {
                case kmldom::GX_ALTITUDEMODE_RELATIVETOSEAFLOOR:
                    poOgrFeat->SetField ( iField, "relativeToSeaFloor" );
                    break;

                case kmldom::GX_ALTITUDEMODE_CLAMPTOSEAFLOOR:
                    poOgrFeat->SetField ( iField, "clampToSeaFloor" );
                    break;
                }
            }

        }
    }

    /***** visibility *****/

    int nVisibility = -1;

    if ( poKmlFeature->has_visibility (  ) )
        nVisibility = poKmlFeature->get_visibility (  );

    int iField = poOgrFeat->GetFieldIndex ( oFC.visibilityfield );

    if ( iField > -1 )
        poOgrFeat->SetField ( iField, nVisibility );

    /***** snippet *****/

    if ( poKmlFeature->has_snippet (  ) )
    {
        string oText = poKmlFeature->get_snippet (  )->get_text();

        iField = poOgrFeat->GetFieldIndex ( oFC.snippetfield );

        if ( iField > -1 )
            poOgrFeat->SetField ( iField, TrimSpaces(oText) );
    }

    /***** extended schema *****/
    ExtendedDataPtr poKmlExtendedData = NULL;

    if ( poKmlFeature->has_extendeddata (  ) ) {
        poKmlExtendedData = poKmlFeature->get_extendeddata (  );

        /***** loop over the schemadata_arrays *****/

        size_t nSchemaData = poKmlExtendedData->get_schemadata_array_size (  );

        size_t iSchemaData;

        for ( iSchemaData = 0; iSchemaData < nSchemaData; iSchemaData++ ) {
            SchemaDataPtr poKmlSchemaData =
                poKmlExtendedData->get_schemadata_array_at ( iSchemaData );

            /***** loop over the simpledata array *****/

            size_t nSimpleData =
                poKmlSchemaData->get_simpledata_array_size (  );

            size_t iSimpleData;

            for ( iSimpleData = 0; iSimpleData < nSimpleData; iSimpleData++ ) {
                SimpleDataPtr poKmlSimpleData =
                    poKmlSchemaData->get_simpledata_array_at ( iSimpleData );

                /***** find the field index *****/

                iField = -1;

                if ( poKmlSimpleData->has_name (  ) ) {
                    const string oName = poKmlSimpleData->get_name (  );
                    const char *pszName = oName.c_str (  );

                    iField = poOgrFeat->GetFieldIndex ( pszName );
                }

                /***** if it has trxt set the field *****/

                if ( iField > -1 && poKmlSimpleData->has_text (  ) ) {
                    string oText = poKmlSimpleData->get_text (  );

                    poOgrFeat->SetField ( iField, TrimSpaces(oText) );
                }
            }
        }

        if (nSchemaData == 0 &&  poKmlExtendedData->get_data_array_size() > 0 )
        {
            int bLaunderFieldNames =
                CPLTestBool(CPLGetConfigOption("LIBKML_LAUNDER_FIELD_NAMES", "YES"));
            size_t nDataArraySize = poKmlExtendedData->get_data_array_size();
            for(size_t i=0; i < nDataArraySize; i++)
            {
                const DataPtr& data = poKmlExtendedData->get_data_array_at(i);
                if (data->has_name() && data->has_value())
                {
                    CPLString osName = std::string(data->get_name());
                    if (bLaunderFieldNames)
                        osName = OGRLIBKMLLayer::LaunderFieldNames(osName);
                    iField = poOgrFeat->GetFieldIndex ( osName );
                    if (iField >= 0)
                    {
                        poOgrFeat->SetField ( iField, data->get_value().c_str() );
                    }
                }
            }
        }
    }

}

/******************************************************************************
 function create a simplefield from a FieldDefn
******************************************************************************/

SimpleFieldPtr FieldDef2kml (
    OGRFieldDefn * poOgrFieldDef,
    KmlFactory * poKmlFactory )
{
    /***** get the field config *****/

    struct fieldconfig oFC;
    get_fieldconfig( &oFC );

    const char *pszFieldName = poOgrFieldDef->GetNameRef (  );

    if ( EQUAL ( pszFieldName, oFC.namefield ) ||
         EQUAL ( pszFieldName, oFC.descfield ) ||
         EQUAL ( pszFieldName, oFC.tsfield ) ||
         EQUAL ( pszFieldName, oFC.beginfield ) ||
         EQUAL ( pszFieldName, oFC.endfield ) ||
         EQUAL ( pszFieldName, oFC.altitudeModefield ) ||
         EQUAL ( pszFieldName, oFC.tessellatefield ) ||
         EQUAL ( pszFieldName, oFC.extrudefield ) ||
         EQUAL ( pszFieldName, oFC.visibilityfield ) ||
         EQUAL ( pszFieldName, oFC.drawOrderfield ) ||
         EQUAL ( pszFieldName, oFC.iconfield ) ||
         EQUAL ( pszFieldName, oFC.headingfield ) ||
         EQUAL ( pszFieldName, oFC.tiltfield ) ||
         EQUAL ( pszFieldName, oFC.rollfield ) ||
         EQUAL ( pszFieldName, oFC.snippetfield ) ||
         EQUAL ( pszFieldName, oFC.modelfield ) ||
         EQUAL ( pszFieldName, oFC.scalexfield ) ||
         EQUAL ( pszFieldName, oFC.scaleyfield ) ||
         EQUAL ( pszFieldName, oFC.scalezfield ) ||
         EQUAL ( pszFieldName, oFC.networklinkfield ) ||
         EQUAL ( pszFieldName, oFC.networklink_refreshvisibility_field ) ||
         EQUAL ( pszFieldName, oFC.networklink_flytoview_field ) ||
         EQUAL ( pszFieldName, oFC.networklink_refreshMode_field ) ||
         EQUAL ( pszFieldName, oFC.networklink_refreshInterval_field ) ||
         EQUAL ( pszFieldName, oFC.networklink_viewRefreshMode_field ) ||
         EQUAL ( pszFieldName, oFC.networklink_viewRefreshTime_field ) ||
         EQUAL ( pszFieldName, oFC.networklink_viewBoundScale_field ) ||
         EQUAL ( pszFieldName, oFC.networklink_viewFormat_field ) ||
         EQUAL ( pszFieldName, oFC.networklink_httpQuery_field ) ||
         EQUAL ( pszFieldName, oFC.camera_longitude_field ) ||
         EQUAL ( pszFieldName, oFC.camera_latitude_field ) ||
         EQUAL ( pszFieldName, oFC.camera_altitude_field ) ||
         EQUAL ( pszFieldName, oFC.camera_altitudemode_field ) ||
         EQUAL ( pszFieldName, oFC.photooverlayfield ) ||
         EQUAL ( pszFieldName, oFC.leftfovfield ) ||
         EQUAL ( pszFieldName, oFC.rightfovfield ) ||
         EQUAL ( pszFieldName, oFC.bottomfovfield ) ||
         EQUAL ( pszFieldName, oFC.topfovfield ) ||
         EQUAL ( pszFieldName, oFC.nearfield ) ||
         EQUAL ( pszFieldName, oFC.photooverlay_shape_field ) ||
         EQUAL ( pszFieldName, oFC.imagepyramid_tilesize_field) ||
         EQUAL ( pszFieldName, oFC.imagepyramid_maxwidth_field) ||
         EQUAL ( pszFieldName, oFC.imagepyramid_maxheight_field) ||
         EQUAL ( pszFieldName, oFC.imagepyramid_gridorigin_field) )
    {
        return NULL;
    }

    SimpleFieldPtr poKmlSimpleField = poKmlFactory->CreateSimpleField (  );
    poKmlSimpleField->set_name ( pszFieldName );


    SimpleDataPtr poKmlSimpleData = NULL;

    switch ( poOgrFieldDef->GetType (  ) ) {
    case OFTInteger:
    case OFTIntegerList:
        poKmlSimpleField->set_type ( "int" );
        return poKmlSimpleField;

    case OFTReal:
    case OFTRealList:
        poKmlSimpleField->set_type ( "float" );
        return poKmlSimpleField;

    case OFTString:
    case OFTStringList:
        poKmlSimpleField->set_type ( "string" );
        return poKmlSimpleField;

    /***** kml has these types but as timestamp/timespan *****/

    case OFTDate:
    case OFTTime:
    case OFTDateTime:
        break;

    default:
        poKmlSimpleField->set_type ( "string" );
        return poKmlSimpleField;
    }

    return NULL;
}

/******************************************************************************
 function to add the simpleFields in a schema to a featuredefn
******************************************************************************/

void kml2FeatureDef (
    SchemaPtr poKmlSchema,
    OGRFeatureDefn * poOgrFeatureDefn )
{

    size_t nSimpleFields = poKmlSchema->get_simplefield_array_size (  );
    size_t iSimpleField;

    for ( iSimpleField = 0; iSimpleField < nSimpleFields; iSimpleField++ ) {
        SimpleFieldPtr poKmlSimpleField =
            poKmlSchema->get_simplefield_array_at ( iSimpleField );

        const char *pszType = "string";
        string osName = "Unknown";
        string osType;

        if ( poKmlSimpleField->has_type (  ) ) {
            osType = poKmlSimpleField->get_type (  );

            pszType = osType.c_str (  );
        }

        /* FIXME? We cannot set displayname as the field name because in kml2field() we make the */
        /* lookup on fields based on their name. We would need some map if we really */
        /* want to use displayname, but that might not be a good idea because displayname */
        /* may have HTML formatting, which makes it impractical when converting to other */
        /* drivers or to make requests */
        /* Example: http://www.jasonbirch.com/files/newt_combined.kml */
        /*if ( poKmlSimpleField->has_displayname (  ) ) {
            osName = poKmlSimpleField->get_displayname (  );
        }

        else*/ if ( poKmlSimpleField->has_name (  ) ) {
            osName = poKmlSimpleField->get_name (  );
        }

        if ( EQUAL ( pszType, "bool" ) ||
             EQUAL ( pszType, "boolean" ) ||
             EQUAL ( pszType, "int" ) ||
             EQUAL ( pszType, "short" ) ||
             EQUAL ( pszType, "ushort" ) ) {
            OGRFieldDefn oOgrFieldName ( osName.c_str(), OFTInteger );
            poOgrFeatureDefn->AddFieldDefn ( &oOgrFieldName );
        }
        else if ( EQUAL ( pszType, "uint" ) )  {
            OGRFieldDefn oOgrFieldName ( osName.c_str(), OFTInteger64 );
            poOgrFeatureDefn->AddFieldDefn ( &oOgrFieldName );
        }
        else if ( EQUAL ( pszType, "float" ) ||
                  EQUAL ( pszType, "double" ) ) {
            OGRFieldDefn oOgrFieldName ( osName.c_str(), OFTReal );
            poOgrFeatureDefn->AddFieldDefn ( &oOgrFieldName );
        }
        else /* string, or any other unrecognized type */
        {
            OGRFieldDefn oOgrFieldName ( osName.c_str(), OFTString );
            poOgrFeatureDefn->AddFieldDefn ( &oOgrFieldName );
        }
    }

    return;
}

/*******************************************************************************
 * function to fetch the field config options
 *
*******************************************************************************/

void get_fieldconfig( struct fieldconfig *oFC) {

    oFC->namefield = CPLGetConfigOption ( "LIBKML_NAME_FIELD",
                                                  "Name" );
    oFC->descfield = CPLGetConfigOption ( "LIBKML_DESCRIPTION_FIELD",
                                                  "description" );
    oFC->tsfield = CPLGetConfigOption ( "LIBKML_TIMESTAMP_FIELD",
                                                "timestamp" );
    oFC->beginfield = CPLGetConfigOption ( "LIBKML_BEGIN_FIELD",
                                           "begin" );
    oFC->endfield = CPLGetConfigOption ( "LIBKML_END_FIELD",
                                         "end" );
    oFC->altitudeModefield = CPLGetConfigOption ( "LIBKML_ALTITUDEMODE_FIELD",
                                                          "altitudeMode" );
    oFC->tessellatefield = CPLGetConfigOption ( "LIBKML_TESSELLATE_FIELD",
                                                        "tessellate" );
    oFC->extrudefield = CPLGetConfigOption ( "LIBKML_EXTRUDE_FIELD",
                                                     "extrude" );
    oFC->visibilityfield = CPLGetConfigOption ( "LIBKML_VISIBILITY_FIELD",
                                                        "visibility" );
    oFC->drawOrderfield = CPLGetConfigOption ( "LIBKML_DRAWORDER_FIELD",
                                                       "drawOrder" );
    oFC->iconfield = CPLGetConfigOption ( "LIBKML_ICON_FIELD",
                                                  "icon" );
    oFC->headingfield = CPLGetConfigOption( "LIBKML_HEADING_FIELD", "heading");
    oFC->tiltfield = CPLGetConfigOption( "LIBKML_TILT_FIELD", "tilt");
    oFC->rollfield = CPLGetConfigOption( "LIBKML_ROLL_FIELD", "roll");
    oFC->snippetfield = CPLGetConfigOption( "LIBKML_SNIPPET_FIELD", "snippet");
    oFC->modelfield = CPLGetConfigOption( "LIBKML_MODEL_FIELD", "model");
    oFC->scalexfield = CPLGetConfigOption( "LIBKML_SCALE_X_FIELD", "scale_x");
    oFC->scaleyfield = CPLGetConfigOption( "LIBKML_SCALE_Y_FIELD", "scale_y");
    oFC->scalezfield = CPLGetConfigOption( "LIBKML_SCALE_Z_FIELD", "scale_z");
    oFC->networklinkfield = CPLGetConfigOption( "LIBKML_NETWORKLINK_FIELD", "networklink");
    oFC->networklink_refreshvisibility_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_REFRESHVISIBILITY_FIELD", "networklink_refreshvisibility");
    oFC->networklink_flytoview_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_FLYTOVIEW_FIELD", "networklink_flytoview");
    oFC->networklink_refreshMode_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_REFRESHMODE_FIELD", "networklink_refreshmode");
    oFC->networklink_refreshInterval_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_REFRESHINTERVAL_FIELD", "networklink_refreshinterval");
    oFC->networklink_viewRefreshMode_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_VIEWREFRESHMODE_FIELD", "networklink_viewrefreshmode");
    oFC->networklink_viewRefreshTime_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_VIEWREFRESHTIME_FIELD", "networklink_viewrefreshtime");
    oFC->networklink_viewBoundScale_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_VIEWBOUNDSCALE_FIELD", "networklink_viewboundscale");
    oFC->networklink_viewFormat_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_VIEWFORMAT_FIELD", "networklink_viewformat");
    oFC->networklink_httpQuery_field = CPLGetConfigOption( "LIBKML_NETWORKLINK_HTTPQUERY_FIELD", "networklink_httpquery");
    oFC->camera_longitude_field = CPLGetConfigOption( "LIBKML_CAMERA_LONGITUDE_FIELD", "camera_longitude");
    oFC->camera_latitude_field = CPLGetConfigOption( "LIBKML_CAMERA_LATITUDE_FIELD", "camera_latitude");
    oFC->camera_altitude_field = CPLGetConfigOption( "LIBKML_CAMERA_ALTITUDE_FIELD", "camera_altitude");
    oFC->camera_altitudemode_field = CPLGetConfigOption( "LIBKML_CAMERA_ALTITUDEMODE_FIELD", "camera_altitudemode");
    oFC->photooverlayfield = CPLGetConfigOption( "LIBKML_PHOTOOVERLAY_FIELD", "photooverlay");
    oFC->leftfovfield = CPLGetConfigOption( "LIBKML_LEFTFOV_FIELD", "leftfov");
    oFC->rightfovfield = CPLGetConfigOption( "LIBKML_RIGHTFOV_FIELD", "rightfov");
    oFC->bottomfovfield = CPLGetConfigOption( "LIBKML_BOTTOMFOV_FIELD", "bottomfov");
    oFC->topfovfield = CPLGetConfigOption( "LIBKML_TOPFOV_FIELD", "topfov");
    oFC->nearfield = CPLGetConfigOption( "LIBKML_NEARFOV_FIELD", "near");
    oFC->photooverlay_shape_field = CPLGetConfigOption( "LIBKML_PHOTOOVERLAY_SHAPE_FIELD", "photooverlay_shape");
    oFC->imagepyramid_tilesize_field = CPLGetConfigOption( "LIBKML_IMAGEPYRAMID_TILESIZE", "imagepyramid_tilesize");
    oFC->imagepyramid_maxwidth_field = CPLGetConfigOption( "LIBKML_IMAGEPYRAMID_MAXWIDTH", "imagepyramid_maxwidth");
    oFC->imagepyramid_maxheight_field = CPLGetConfigOption( "LIBKML_IMAGEPYRAMID_MAXHEIGHT", "imagepyramid_maxheight");
    oFC->imagepyramid_gridorigin_field = CPLGetConfigOption( "LIBKML_IMAGEPYRAMID_GRIDORIGIN", "imagepyramid_gridorigin");
}

/************************************************************************/
/*                 kmlAltitudeModeFromString()                          */
/************************************************************************/

int kmlAltitudeModeFromString(const char* pszAltitudeMode,
                              int& isGX)
{
    isGX = FALSE;
    int iAltitudeMode = static_cast<int>(kmldom::ALTITUDEMODE_CLAMPTOGROUND);

    if ( EQUAL ( pszAltitudeMode, "clampToGround" ) )
        iAltitudeMode = static_cast<int>(kmldom::ALTITUDEMODE_CLAMPTOGROUND);

    else if ( EQUAL ( pszAltitudeMode, "relativeToGround" ) )
        iAltitudeMode = static_cast<int>(kmldom::ALTITUDEMODE_RELATIVETOGROUND);

    else if ( EQUAL ( pszAltitudeMode, "absolute" ) )
        iAltitudeMode = static_cast<int>(kmldom::ALTITUDEMODE_ABSOLUTE);

    else if ( EQUAL ( pszAltitudeMode, "relativeToSeaFloor" ) ) {
        iAltitudeMode =
            static_cast<int>(kmldom::GX_ALTITUDEMODE_RELATIVETOSEAFLOOR);
        isGX = TRUE;
    }

    else if ( EQUAL ( pszAltitudeMode, "clampToSeaFloor" ) ) {
        iAltitudeMode =
            static_cast<int>(kmldom::GX_ALTITUDEMODE_CLAMPTOSEAFLOOR);
        isGX = TRUE;
    }
    else
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "Unrecognized value for altitudeMode: %s",
                 pszAltitudeMode);
    }

    return iAltitudeMode;
}
