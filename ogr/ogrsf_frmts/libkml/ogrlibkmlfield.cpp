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

#include  <ogrsf_frmts.h>
#include <ogr_feature.h>
#include "ogr_p.h"

#include <kml/dom.h>
#include <iostream>

using kmldom::ExtendedDataPtr;
using kmldom::SchemaPtr;
using kmldom::SchemaDataPtr;
using kmldom::SimpleDataPtr;

using kmldom::TimeStampPtr;
using kmldom::TimeSpanPtr;
using kmldom::TimePrimitivePtr;

using kmldom::PointPtr;
using kmldom::LineStringPtr;
using kmldom::PolygonPtr;
using kmldom::MultiGeometryPtr;
using kmldom::GeometryPtr;


#include "ogr_libkml.h"

#include "ogrlibkmlfield.h"

void ogr2altitudemode_rec (
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

void ogr2extrude_rec (
    int nExtrude,
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
        poKmlPoint->set_extrude ( nExtrude );
        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );
        poKmlLineString->set_extrude ( nExtrude );
        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );
        poKmlPolygon->set_extrude ( nExtrude );
        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            ogr2extrude_rec ( nExtrude,
                              poKmlMultiGeometry->
                              get_geometry_array_at ( i ) );
        }
        break;

    default:
        break;

    }
}

void ogr2tessellate_rec (
    int nTessellate,
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
        poKmlLineString->set_tessellate ( nTessellate );
        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        poKmlPolygon->set_tessellate ( nTessellate );
        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            ogr2tessellate_rec ( nTessellate,
                                 poKmlMultiGeometry->
                                 get_geometry_array_at ( i ) );
        }

        break;

    default:
        break;

    }
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
    PlacemarkPtr poKmlPlacemark )
{
    int i;

    SchemaDataPtr poKmlSchemaData = poKmlFactory->CreateSchemaData (  );
    SchemaPtr poKmlSchema = poOgrLayer->GetKmlSchema (  );

    /***** set the url to the schema *****/

    if ( poKmlSchema->has_id (  ) ) {
        std::string oKmlSchemaID = poKmlSchema->get_id (  );


        std::string oKmlSchemaURL = "#";
        oKmlSchemaURL.append ( oKmlSchemaID );

        poKmlSchemaData->set_schemaurl ( oKmlSchemaURL );
    }

    const char *namefield = CPLGetConfigOption ( "LIBKML_NAME_FIELD", "Name" );
    const char *descfield =
        CPLGetConfigOption ( "LIBKML_DESCRIPTION_FIELD", "description" );
    const char *tsfield =
        CPLGetConfigOption ( "LIBKML_TIMESTAMP_FIELD", "timestamp" );
    const char *beginfield =
        CPLGetConfigOption ( "LIBKML_BEGIN_FIELD", "begin" );
    const char *endfield = CPLGetConfigOption ( "LIBKML_END_FIELD", "end" );
    const char *altitudeModefield =
        CPLGetConfigOption ( "LIBKML_ALTITUDEMODE_FIELD", "altitudeMode" );
    const char *tessellatefield =
        CPLGetConfigOption ( "LIBKML_TESSELLATE_FIELD", "tessellate" );
    const char *extrudefield =
        CPLGetConfigOption ( "LIBKML_EXTRUDE_FIELD", "extrude" );
    const char *visibilityfield =
        CPLGetConfigOption ( "LIBKML_VISIBILITY_FIELD", "visibility" );

    TimeSpanPtr poKmlTimeSpan = NULL;

    int nFields = poOgrFeat->GetFieldCount (  );
    int iSkip1 = -1;
    int iSkip2 = -1;

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
        int year,
            month,
            day,
            hour,
            min,
            sec,
            tz;

        switch ( type ) {

        case OFTString:        //     String of ASCII chars
            {

                /***** name *****/

                if ( EQUAL ( name, namefield ) ) {
                    poKmlPlacemark->set_name ( poOgrFeat->
                                               GetFieldAsString ( i ) );
                    continue;
                }

                /***** description *****/

                else if ( !strcasecmp ( name, descfield ) ) {
                    poKmlPlacemark->set_description ( poOgrFeat->
                                                      GetFieldAsString ( i ) );
                    continue;
                }

                /***** altitudemode *****/

                else if ( EQUAL ( name, altitudeModefield ) ) {
                    const char *pszAltitudeMode =
                        poOgrFeat->GetFieldAsString ( i );

                    int isGX = FALSE;
                    int iAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;

                    if ( EQUAL ( pszAltitudeMode, "clampToGround" ) )
                        iAltitudeMode = kmldom::ALTITUDEMODE_CLAMPTOGROUND;

                    else if ( EQUAL ( pszAltitudeMode, "relativeToGround" ) )
                        iAltitudeMode = kmldom::ALTITUDEMODE_RELATIVETOGROUND;

                    else if ( EQUAL ( pszAltitudeMode, "absolute" ) )
                        iAltitudeMode = kmldom::ALTITUDEMODE_ABSOLUTE;

                    else if ( EQUAL ( pszAltitudeMode, "relativeToSeaFloor" ) ) {
                        iAltitudeMode =
                            kmldom::GX_ALTITUDEMODE_RELATIVETOSEAFLOOR;
                        isGX = TRUE;
                    }

                    else if ( EQUAL ( pszAltitudeMode, "clampToSeaFloor" ) ) {
                        iAltitudeMode =
                            kmldom::GX_ALTITUDEMODE_CLAMPTOSEAFLOOR;
                        isGX = TRUE;
                    }


                    if ( poKmlPlacemark->has_geometry (  ) ) {
                        GeometryPtr poKmlGeometry =
                            poKmlPlacemark->get_geometry (  );

                        ogr2altitudemode_rec ( poKmlGeometry, iAltitudeMode,
                                               isGX );

                    }

                    continue;
                }
                
                /***** timestamp *****/

                else if ( EQUAL ( name, tsfield ) ) {

                    TimeStampPtr poKmlTimeStamp =
                        poKmlFactory->CreateTimeStamp (  );
                    poKmlTimeStamp->set_when ( poOgrFeat->GetFieldAsString ( i )  );
                    poKmlPlacemark->set_timeprimitive ( poKmlTimeStamp );

                    continue;
                }

                /***** begin *****/

                if ( EQUAL ( name, beginfield ) ) {

                    if ( !poKmlTimeSpan ) {
                        poKmlTimeSpan = poKmlFactory->CreateTimeSpan (  );
                        poKmlPlacemark->set_timeprimitive ( poKmlTimeSpan );
                    }

                    poKmlTimeSpan->set_begin ( poOgrFeat->GetFieldAsString ( i ) );

                    continue;

                }

                /***** end *****/

                else if ( EQUAL ( name, endfield ) ) {

                    if ( !poKmlTimeSpan ) {
                        poKmlTimeSpan = poKmlFactory->CreateTimeSpan (  );
                        poKmlPlacemark->set_timeprimitive ( poKmlTimeSpan );
                    }

                    poKmlTimeSpan->set_end ( poOgrFeat->GetFieldAsString ( i ) );

                    continue;
                }
                
                /***** other *****/

                poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
                poKmlSimpleData->set_name ( name );
                poKmlSimpleData->set_text ( poOgrFeat->
                                            GetFieldAsString ( i ) );

                break;
            }

        case OFTDate:          //   Date
            {
                poOgrFeat->GetFieldAsDateTime ( i, &year, &month, &day,
                                                &hour, &min, &sec, &tz );

                int iTimeField;

                for ( iTimeField = i + 1; iTimeField < nFields; iTimeField++ ) {
                    if ( iTimeField == iSkip1 || iTimeField == iSkip2 )
                        continue;

                    OGRFieldDefn *poOgrFieldDef2 =
                        poOgrFeat->GetFieldDefnRef ( i );
                    OGRFieldType type2 = poOgrFieldDef2->GetType (  );
                    const char *name2 = poOgrFieldDef2->GetNameRef (  );

                    if ( EQUAL ( name2, name ) && type2 == OFTTime &&
                         ( EQUAL ( name, tsfield ) ||
                           EQUAL ( name, beginfield ) ||
                           EQUAL ( name, endfield ) ) ) {

                        int year2,
                            month2,
                            day2,
                            hour2,
                            min2,
                            sec2,
                            tz2;

                        poOgrFeat->GetFieldAsDateTime ( iTimeField, &year2,
                                                        &month2, &day2, &hour2,
                                                        &min2, &sec2, &tz2 );

                        hour = hour2;
                        min = min2;
                        sec = sec2;
                        tz = tz2;

                        if ( 0 > iSkip1 )
                            iSkip1 = iTimeField;
                        else
                            iSkip2 = iTimeField;
                    }
                }

                goto Do_DateTime;

            }


        case OFTTime:          //   Time
            {
                poOgrFeat->GetFieldAsDateTime ( i, &year, &month, &day,
                                                &hour, &min, &sec, &tz );

                int iTimeField;

                for ( iTimeField = i + 1; iTimeField < nFields; iTimeField++ ) {
                    if ( iTimeField == iSkip1 || iTimeField == iSkip2 )
                        continue;

                    OGRFieldDefn *poOgrFieldDef2 =
                        poOgrFeat->GetFieldDefnRef ( i );
                    OGRFieldType type2 = poOgrFieldDef2->GetType (  );
                    const char *name2 = poOgrFieldDef2->GetNameRef (  );

                    if ( EQUAL ( name2, name ) && type2 == OFTTime &&
                         ( EQUAL ( name, tsfield ) ||
                           EQUAL ( name, beginfield ) ||
                           EQUAL ( name, endfield ) ) ) {

                        int year2,
                            month2,
                            day2,
                            hour2,
                            min2,
                            sec2,
                            tz2;

                        poOgrFeat->GetFieldAsDateTime ( iTimeField, &year2,
                                                        &month2, &day2, &hour2,
                                                        &min2, &sec2, &tz2 );

                        year = year2;
                        month = month2;
                        day = day2;

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
                poOgrFeat->GetFieldAsDateTime ( i, &year, &month, &day,
                                                &hour, &min, &sec, &tz );

              Do_DateTime:
                /***** timestamp *****/

                if ( EQUAL ( name, tsfield ) ) {

                    char *timebuf = OGRGetXMLDateTime ( year, month, day, hour,
                                                        min, sec, tz );

                    TimeStampPtr poKmlTimeStamp =
                        poKmlFactory->CreateTimeStamp (  );
                    poKmlTimeStamp->set_when ( timebuf );
                    poKmlPlacemark->set_timeprimitive ( poKmlTimeStamp );

                    continue;
                }

                /***** begin *****/

                if ( EQUAL ( name, beginfield ) ) {

                    char *timebuf = OGRGetXMLDateTime ( year, month, day, hour,
                                                        min, sec, tz );

                    if ( !poKmlTimeSpan ) {
                        poKmlTimeSpan = poKmlFactory->CreateTimeSpan (  );
                        poKmlPlacemark->set_timeprimitive ( poKmlTimeSpan );
                    }

                    poKmlTimeSpan->set_begin ( timebuf );

                    continue;

                }

                /***** end *****/

                else if ( EQUAL ( name, endfield ) ) {

                    char *timebuf = OGRGetXMLDateTime ( year, month, day, hour,
                                                        min, sec, tz );


                    if ( !poKmlTimeSpan ) {
                        poKmlTimeSpan = poKmlFactory->CreateTimeSpan (  );
                        poKmlPlacemark->set_timeprimitive ( poKmlTimeSpan );
                    }

                    poKmlTimeSpan->set_end ( timebuf );

                    continue;
                }

                /***** other *****/

                poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
                poKmlSimpleData->set_name ( name );
                poKmlSimpleData->set_text ( poOgrFeat->
                                            GetFieldAsString ( i ) );

                break;
            }

        case OFTStringList:    //     Array of strings
        case OFTBinary:        //     Raw Binary data

            /***** other *****/

            poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
            poKmlSimpleData->set_name ( name );
            poKmlSimpleData->set_text ( poOgrFeat->GetFieldAsString ( i ) );

            break;
        case OFTInteger:       //    Simple 32bit integer

            /***** extrude *****/

            if ( EQUAL ( name, extrudefield ) ) {

                if ( poKmlPlacemark->has_geometry (  )
                     && -1 < poOgrFeat->GetFieldAsInteger ( i ) ) {
                    GeometryPtr poKmlGeometry =
                        poKmlPlacemark->get_geometry (  );
                    ogr2extrude_rec ( poOgrFeat->GetFieldAsInteger ( i ),
                                      poKmlGeometry );
                }
                continue;
            }

            /***** tessellate *****/


            if ( EQUAL ( name, tessellatefield ) ) {

                if ( poKmlPlacemark->has_geometry (  )
                     && -1 < poOgrFeat->GetFieldAsInteger ( i ) ) {
                    GeometryPtr poKmlGeometry =
                        poKmlPlacemark->get_geometry (  );
                    ogr2tessellate_rec ( poOgrFeat->GetFieldAsInteger ( i ),
                                         poKmlGeometry );
                }

                continue;
            }


            /***** visibility *****/

            if ( EQUAL ( name, visibilityfield ) ) {
                if ( -1 < poOgrFeat->GetFieldAsInteger ( i ) )
                    poKmlPlacemark->set_visibility ( poOgrFeat->
                                                     GetFieldAsInteger ( i ) );

                continue;
            }

            /***** other *****/

            poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
            poKmlSimpleData->set_name ( name );
            poKmlSimpleData->set_text ( poOgrFeat->GetFieldAsString ( i ) );

            break;
        case OFTIntegerList:   //    List of 32bit integers
        case OFTReal:          //   Double Precision floating point

            /***** other *****/

            poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
            poKmlSimpleData->set_name ( name );
            poKmlSimpleData->set_text ( poOgrFeat->GetFieldAsString ( i ) );

            break;
        case OFTRealList:      //   List of doubles

        case OFTWideStringList:    //     deprecated
        default:

        /***** other *****/

            poKmlSimpleData = poKmlFactory->CreateSimpleData (  );
            poKmlSimpleData->set_name ( name );
            poKmlSimpleData->set_text ( poOgrFeat->GetFieldAsString ( i ) );

            break;
        }
        poKmlSchemaData->add_simpledata ( poKmlSimpleData );
    }

    /***** dont add it to the placemark unless there is data *****/

    if ( poKmlSchemaData->get_simpledata_array_size (  ) > 0 ) {
        ExtendedDataPtr poKmlExtendedData =
            poKmlFactory->CreateExtendedData (  );
        poKmlExtendedData->add_schemadata ( poKmlSchemaData );
        poKmlPlacemark->set_extendeddata ( poKmlExtendedData );
    }

    return;
}

/******************************************************************************
 recursive function to read altitude mode from the geometry
******************************************************************************/

int kml2altitudemode_rec (
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

int kml2extrude_rec (
    GeometryPtr poKmlGeometry,
    int *pnExtrude )
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
            *pnExtrude = poKmlPoint->get_extrude (  );
            return TRUE;
        }

        break;

    case kmldom::Type_LineString:
        poKmlLineString = AsLineString ( poKmlGeometry );

        if ( poKmlLineString->has_extrude (  ) ) {
            *pnExtrude = poKmlLineString->get_extrude (  );
            return TRUE;
        }

        break;

    case kmldom::Type_LinearRing:
        break;

    case kmldom::Type_Polygon:
        poKmlPolygon = AsPolygon ( poKmlGeometry );

        if ( poKmlPolygon->has_extrude (  ) ) {
            *pnExtrude = poKmlPolygon->get_extrude (  );
            return TRUE;
        }

        break;

    case kmldom::Type_MultiGeometry:
        poKmlMultiGeometry = AsMultiGeometry ( poKmlGeometry );

        nGeom = poKmlMultiGeometry->get_geometry_array_size (  );
        for ( i = 0; i < nGeom; i++ ) {
            if ( kml2extrude_rec ( poKmlMultiGeometry->
                                   get_geometry_array_at ( i ), pnExtrude ) )
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

int kml2tessellate_rec (
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

/******************************************************************************
 function to read kml into ogr fields
******************************************************************************/

void kml2field (
    OGRFeature * poOgrFeat,
    PlacemarkPtr poKmlPlacemark )
{

    const char *namefield = CPLGetConfigOption ( "LIBKML_NAME_FIELD", "Name" );
    const char *descfield =
        CPLGetConfigOption ( "LIBKML_DESCRIPTION_FIELD", "description" );
    const char *tsfield =
        CPLGetConfigOption ( "LIBKML_TIMESTAMP_FIELD", "timestamp" );
    const char *beginfield =
        CPLGetConfigOption ( "LIBKML_BEGIN_FIELD", "begin" );
    const char *endfield = CPLGetConfigOption ( "LIBKML_END_FIELD", "end" );
    const char *altitudeModefield =
        CPLGetConfigOption ( "LIBKML_ALTITUDEMODE_FIELD", "altitudeMode" );
    const char *tessellatefield =
        CPLGetConfigOption ( "LIBKML_TESSELLATE_FIELD", "tessellate" );
    const char *extrudefield =
        CPLGetConfigOption ( "LIBKML_EXTRUDE_FIELD", "extrude" );
    const char *visibilityfield =
        CPLGetConfigOption ( "LIBKML_VISIBILITY_FIELD", "visibility" );

    /***** name *****/

    if ( poKmlPlacemark->has_name (  ) ) {
        const std::string oKmlName = poKmlPlacemark->get_name (  );
        int iField = poOgrFeat->GetFieldIndex ( namefield );

        if ( iField > -1 )
            poOgrFeat->SetField ( iField, oKmlName.c_str (  ) );
    }

    /***** description *****/

    if ( poKmlPlacemark->has_description (  ) ) {
        const std::string oKmlDesc = poKmlPlacemark->get_description (  );
        int iField = poOgrFeat->GetFieldIndex ( descfield );

        if ( iField > -1 )
            poOgrFeat->SetField ( iField, oKmlDesc.c_str (  ) );
    }

    if ( poKmlPlacemark->has_timeprimitive (  ) ) {
        TimePrimitivePtr poKmlTimePrimitive =
            poKmlPlacemark->get_timeprimitive (  );

        /***** timestamp *****/

        if ( poKmlTimePrimitive->IsA ( kmldom::Type_TimeStamp ) ) {
            TimeStampPtr poKmlTimeStamp = AsTimeStamp ( poKmlTimePrimitive );

            if ( poKmlTimeStamp->has_when (  ) ) {
                const std::string oKmlWhen = poKmlTimeStamp->get_when (  );


                int iField = poOgrFeat->GetFieldIndex ( tsfield );

                if ( iField > -1 ) {
                    int nYear,
                        nMonth,
                        nDay,
                        nHour,
                        nMinute,
                        nTZ;
                    float fSecond;

                    if ( OGRParseXMLDateTime
                         ( oKmlWhen.c_str (  ), &nYear, &nMonth, &nDay, &nHour,
                           &nMinute, &fSecond, &nTZ ) )
                        poOgrFeat->SetField ( iField, nYear, nMonth, nDay,
                                              nHour, nMinute, ( int )fSecond,
                                              nTZ );
                }
            }
        }

        /***** timespan *****/

        if ( poKmlTimePrimitive->IsA ( kmldom::Type_TimeSpan ) ) {
            TimeSpanPtr poKmlTimeSpan = AsTimeSpan ( poKmlTimePrimitive );

            /***** begin *****/

            if ( poKmlTimeSpan->has_begin (  ) ) {
                const std::string oKmlWhen = poKmlTimeSpan->get_begin (  );


                int iField = poOgrFeat->GetFieldIndex ( beginfield );

                if ( iField > -1 ) {
                    int nYear,
                        nMonth,
                        nDay,
                        nHour,
                        nMinute,
                        nTZ;
                    float fSecond;

                    if ( OGRParseXMLDateTime
                         ( oKmlWhen.c_str (  ), &nYear, &nMonth, &nDay, &nHour,
                           &nMinute, &fSecond, &nTZ ) )
                        poOgrFeat->SetField ( iField, nYear, nMonth, nDay,
                                              nHour, nMinute, ( int )fSecond,
                                              nTZ );
                }
            }

            /***** end *****/

            if ( poKmlTimeSpan->has_end (  ) ) {
                const std::string oKmlWhen = poKmlTimeSpan->get_end (  );


                int iField = poOgrFeat->GetFieldIndex ( endfield );

                if ( iField > -1 ) {
                    int nYear,
                        nMonth,
                        nDay,
                        nHour,
                        nMinute,
                        nTZ;
                    float fSecond;

                    if ( OGRParseXMLDateTime
                         ( oKmlWhen.c_str (  ), &nYear, &nMonth, &nDay, &nHour,
                           &nMinute, &fSecond, &nTZ ) )
                        poOgrFeat->SetField ( iField, nYear, nMonth, nDay,
                                              nHour, nMinute, ( int )fSecond,
                                              nTZ );
                }
            }
        }
    }


    if ( poKmlPlacemark->has_geometry (  ) ) {
        GeometryPtr poKmlGeometry = poKmlPlacemark->get_geometry (  );

        /***** altitudeMode *****/


        int bIsGX = FALSE;
        int nAltitudeMode = -1;

        int iField = poOgrFeat->GetFieldIndex ( altitudeModefield );

        if ( iField > -1 ) {

            if ( kml2altitudemode_rec ( poKmlGeometry,
                                        &nAltitudeMode, &bIsGX ) ) {

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

        }

        /***** tessellate *****/

        int nTessellate = -1;

        kml2tessellate_rec ( poKmlGeometry, &nTessellate );

        iField = poOgrFeat->GetFieldIndex ( tessellatefield );
        if ( iField > -1 )
            poOgrFeat->SetField ( iField, nTessellate );

        /***** extrude *****/

        int nExtrude = -1;

        kml2extrude_rec ( poKmlGeometry, &nExtrude );

        iField = poOgrFeat->GetFieldIndex ( extrudefield );
        if ( iField > -1 )
            poOgrFeat->SetField ( iField, nExtrude );

    }

    /***** visibility *****/

    int nVisibility = -1;

    if ( poKmlPlacemark->has_visibility (  ) )
        nVisibility = poKmlPlacemark->get_visibility (  );

    int iField = poOgrFeat->GetFieldIndex ( visibilityfield );

    if ( iField > -1 )
        poOgrFeat->SetField ( iField, nVisibility );

    ExtendedDataPtr poKmlExtendedData = NULL;

    if ( poKmlPlacemark->has_extendeddata (  ) ) {
        poKmlExtendedData = poKmlPlacemark->get_extendeddata (  );

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

                int iField = -1;

                if ( poKmlSimpleData->has_name (  ) ) {
                    const string oName = poKmlSimpleData->get_name (  );
                    const char *pszName = oName.c_str (  );

                    iField = poOgrFeat->GetFieldIndex ( pszName );
                }

                /***** if it has trxt set the field *****/

                if ( iField > -1 && poKmlSimpleData->has_text (  ) ) {
                    const string oText = poKmlSimpleData->get_text (  );

                    poOgrFeat->SetField ( iField, oText.c_str (  ) );
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

    SimpleFieldPtr poKmlSimpleField = poKmlFactory->CreateSimpleField (  );
    const char *pszFieldName = poOgrFieldDef->GetNameRef (  );

    poKmlSimpleField->set_name ( pszFieldName );

    const char *namefield = CPLGetConfigOption ( "LIBKML_NAME_FIELD", "Name" );
    const char *descfield =
        CPLGetConfigOption ( "LIBKML_DESCRIPTION_FIELD", "description" );
    const char *tsfield =
        CPLGetConfigOption ( "LIBKML_TIMESTAMP_FIELD", "timestamp" );
    const char *beginfield =
        CPLGetConfigOption ( "LIBKML_BEGIN_FIELD", "begin" );
    const char *endfield = CPLGetConfigOption ( "LIBKML_END_FIELD", "end" );
    const char *altitudeModefield =
        CPLGetConfigOption ( "LIBKML_ALTITUDEMODE_FIELD", "altitudeMode" );
    const char *tessellatefield =
        CPLGetConfigOption ( "LIBKML_TESSELLATE_FIELD", "tessellate" );
    const char *extrudefield =
        CPLGetConfigOption ( "LIBKML_EXTRUDE_FIELD", "extrude" );
    const char *visibilityfield =
        CPLGetConfigOption ( "LIBKML_VISIBILITY_FIELD", "visibility" );


    SimpleDataPtr poKmlSimpleData = NULL;

    switch ( poOgrFieldDef->GetType (  ) ) {

    case OFTInteger:
    case OFTIntegerList:
        if ( EQUAL ( pszFieldName, tessellatefield ) ||
             EQUAL ( pszFieldName, extrudefield ) ||
             EQUAL ( pszFieldName, visibilityfield ) )
            break;
        poKmlSimpleField->set_type ( "int" );
        return poKmlSimpleField;
    case OFTReal:
    case OFTRealList:
        poKmlSimpleField->set_type ( "float" );
        return poKmlSimpleField;
    case OFTBinary:
        poKmlSimpleField->set_type ( "bool" );
        return poKmlSimpleField;
    case OFTString:
    case OFTStringList:
        if ( EQUAL ( pszFieldName, namefield ) ||
             EQUAL ( pszFieldName, descfield ) ||
             EQUAL ( pszFieldName, altitudeModefield ) )
            break;
        poKmlSimpleField->set_type ( "string" );
        return poKmlSimpleField;

    /***** kml has these types but as timestamp/timespan *****/

    case OFTDate:
    case OFTTime:
    case OFTDateTime:
        if ( EQUAL ( pszFieldName, tsfield )
             || EQUAL ( pszFieldName, beginfield )
             || EQUAL ( pszFieldName, endfield ) )
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
        const char *pszName = "Unknown";

        if ( poKmlSimpleField->has_type (  ) ) {
            const string oType = poKmlSimpleField->get_type (  );

            pszType = oType.c_str (  );
        }

        if ( poKmlSimpleField->has_displayname (  ) ) {
            const string oName = poKmlSimpleField->get_displayname (  );

            pszName = oName.c_str (  );
        }

        else if ( poKmlSimpleField->has_name (  ) ) {
            const string oName = poKmlSimpleField->get_name (  );

            pszName = oName.c_str (  );
        }

        if ( EQUAL ( pszType, "string" ) ) {
            OGRFieldDefn oOgrFieldName (
    pszName,
    OFTString );

            poOgrFeatureDefn->AddFieldDefn ( &oOgrFieldName );
        }
        if ( EQUAL ( pszType, "int" ) ) {
            OGRFieldDefn oOgrFieldName (
    pszName,
    OFTInteger );

            poOgrFeatureDefn->AddFieldDefn ( &oOgrFieldName );
        }
        if ( EQUAL ( pszType, "float" ) ) {
            OGRFieldDefn oOgrFieldName (
    pszName,
    OFTReal );

            poOgrFeatureDefn->AddFieldDefn ( &oOgrFieldName );
        }
        if ( EQUAL ( pszType, "bool" ) ) {
            OGRFieldDefn oOgrFieldName (
    pszName,
    OFTBinary );

            poOgrFeatureDefn->AddFieldDefn ( &oOgrFieldName );
        }


    }


    return;
}
