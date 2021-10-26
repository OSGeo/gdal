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

#ifndef OGR_LIBKML_STYLE_H
#define OGR_LIBKML_STYLE_H

#include <string>

kmldom::StylePtr addstylestring2kml (
    const char *stylestring,
    kmldom::StylePtr poKmlStyle,
    kmldom::KmlFactory *poKmlFactory,
    kmldom::FeaturePtr poKmlFeature );

/******************************************************************************
 kml2stylemgr
******************************************************************************/

void kml2stylestring(
    kmldom::StylePtr poKmlStyle,
    OGRStyleMgr *poOgrSM );

/******************************************************************************
 Functions to follow the kml stylemap if one exists.
******************************************************************************/

kmldom::StyleSelectorPtr StyleFromStyleSelector(
    const kmldom::StyleSelectorPtr& styleselector,
    OGRStyleTable * poStyleTable );

kmldom::StyleSelectorPtr StyleFromStyleURL(
    const std::string& styleurl,
    OGRStyleTable * poStyleTable );

kmldom::StyleSelectorPtr StyleFromStyleMap(
    const kmldom::StyleMapPtr& stylemap,
    OGRStyleTable * poStyleTable );

/******************************************************************************
 Function to parse a style table out of a document.
******************************************************************************/

void ParseStyles (
    kmldom::DocumentPtr poKmlDocument,
    OGRStyleTable **poStyleTable );

/******************************************************************************
 function to add a style table to a kml container
******************************************************************************/

void styletable2kml (
    OGRStyleTable * poOgrStyleTable,
    kmldom::KmlFactory * poKmlFactory,
    kmldom::ContainerPtr poKmlContainer,
    char** papszOptions = nullptr );

/******************************************************************************
 Function to add a ListStyle and select it to a container.
******************************************************************************/

void createkmlliststyle (
    kmldom::KmlFactory * poKmlFactory,
    const char* pszBaseName,
    kmldom::ContainerPtr poKmlLayerContainer,
    kmldom::DocumentPtr poKmlDocument,
    const CPLString& osListStyleType,
    const CPLString& osListStyleIconHref );

#endif  // OGR_LIBKML_STYLE_H
