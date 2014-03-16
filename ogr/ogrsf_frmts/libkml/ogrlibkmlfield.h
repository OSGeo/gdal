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

using kmldom::SimpleFieldPtr;
using kmldom::SchemaPtr;
using kmldom::KmlFactory;
using kmldom::FeaturePtr;
using kmldom::PlacemarkPtr;

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
    PlacemarkPtr poKmlPlacemark );

/******************************************************************************
 function to read kml into ogr fields
******************************************************************************/

void kml2field (
    OGRFeature * poOgrFeat,
    FeaturePtr poKmlFeature );

/******************************************************************************
 function create a simplefield from a FieldDefn
******************************************************************************/

SimpleFieldPtr FieldDef2kml (
    OGRFieldDefn *poOgrFieldDef,
    KmlFactory * poKmlFactory );

/******************************************************************************
 function to add the simpleFields in a schema to a featuredefn
******************************************************************************/

void kml2FeatureDef (
    SchemaPtr poKmlSchema,
    OGRFeatureDefn *poOgrFeatureDefn);

/*******************************************************************************
 * function to fetch the field config options
 * 
*******************************************************************************/

struct fieldconfig {
	const char *namefield;
    const char *descfield;
    const char *tsfield;
    const char *beginfield;
    const char *endfield;
    const char *altitudeModefield;
    const char *tessellatefield;
    const char *extrudefield;
    const char *visibilityfield;
    const char *drawOrderfield;
    const char *iconfield;
    const char *headingfield;
    const char *tiltfield;
    const char *rollfield;
    const char *snippetfield;
    const char *modelfield;
    const char *scalexfield;
    const char *scaleyfield;
    const char *scalezfield;
};

void get_fieldconfig( struct fieldconfig *oFC );

int kmlAltitudeModeFromString(const char* pszAltitudeMode,
                              int& isGX);
