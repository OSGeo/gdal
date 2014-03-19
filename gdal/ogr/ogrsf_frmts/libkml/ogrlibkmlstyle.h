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

using kmldom::KmlFactory;
using kmldom::StyleSelectorPtr;
using kmldom::StylePtr;
using kmldom::StyleMapPtr;
using kmldom::DocumentPtr;
using kmldom::ContainerPtr;
using kmldom::FeaturePtr;

StylePtr addstylestring2kml (
    const char *stylestring,
    StylePtr poKmlStyle,
    KmlFactory * poKmlFactory,
    FeaturePtr poKmlFeature );



/******************************************************************************
 kml2stylemgr
******************************************************************************/

void kml2stylestring(
    StylePtr poKmlStyle,
    OGRStyleMgr *poOgrSM);



/******************************************************************************
 functions to follow the kml stylemap if one exists
******************************************************************************/

StyleSelectorPtr StyleFromStyleSelector(
    const StyleSelectorPtr& styleselector,
    OGRStyleTable * poStyleTable);

StyleSelectorPtr StyleFromStyleURL(
    const string styleurl,
    OGRStyleTable * poStyleTable);

StyleSelectorPtr StyleFromStyleMap(
    const StyleMapPtr& stylemap,
    OGRStyleTable * poStyleTable);

/******************************************************************************
 function to parse a style table out of a document
******************************************************************************/

void ParseStyles (
    DocumentPtr poKmlDocument,
    OGRStyleTable **poStyleTable);

/******************************************************************************
 function to add a style table to a kml container
******************************************************************************/

void styletable2kml (
    OGRStyleTable * poOgrStyleTable,
    KmlFactory * poKmlFactory,
    ContainerPtr poKmlContainer,
    char** papszOptions = NULL  );

/******************************************************************************
 function to add a ListStyle and select it to a container
******************************************************************************/

void createkmlliststyle (
    KmlFactory * poKmlFactory,
    const char* pszBaseName,
    DocumentPtr poKmlDocument,
    const CPLString& osListStyleType,
    const CPLString& osListStyleIconHref);
