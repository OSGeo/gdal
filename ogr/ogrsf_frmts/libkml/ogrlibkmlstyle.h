/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef OGR_LIBKML_STYLE_H
#define OGR_LIBKML_STYLE_H

#include <string>

kmldom::StylePtr addstylestring2kml(const char *stylestring,
                                    kmldom::StylePtr poKmlStyle,
                                    kmldom::KmlFactory *poKmlFactory,
                                    kmldom::FeaturePtr poKmlFeature);

/******************************************************************************
 kml2stylemgr
******************************************************************************/

void kml2stylestring(kmldom::StylePtr poKmlStyle, OGRStyleMgr *poOgrSM);

/******************************************************************************
 Functions to follow the kml stylemap if one exists.
******************************************************************************/

kmldom::StyleSelectorPtr
StyleFromStyleSelector(const kmldom::StyleSelectorPtr &styleselector,
                       OGRStyleTable *poStyleTable);

kmldom::StyleSelectorPtr StyleFromStyleURL(const std::string &styleurl,
                                           OGRStyleTable *poStyleTable);

kmldom::StyleSelectorPtr StyleFromStyleMap(const kmldom::StyleMapPtr &stylemap,
                                           OGRStyleTable *poStyleTable);

/******************************************************************************
 Function to parse a style table out of a document.
******************************************************************************/

void ParseStyles(kmldom::DocumentPtr poKmlDocument,
                 OGRStyleTable **poStyleTable);

/******************************************************************************
 function to add a style table to a kml container
******************************************************************************/

void styletable2kml(OGRStyleTable *poOgrStyleTable,
                    kmldom::KmlFactory *poKmlFactory,
                    kmldom::ContainerPtr poKmlContainer,
                    char **papszOptions = nullptr);

/******************************************************************************
 Function to add a ListStyle and select it to a container.
******************************************************************************/

void createkmlliststyle(kmldom::KmlFactory *poKmlFactory,
                        const char *pszBaseName,
                        kmldom::ContainerPtr poKmlLayerContainer,
                        kmldom::DocumentPtr poKmlDocument,
                        const CPLString &osListStyleType,
                        const CPLString &osListStyleIconHref);

#endif  // OGR_LIBKML_STYLE_H
