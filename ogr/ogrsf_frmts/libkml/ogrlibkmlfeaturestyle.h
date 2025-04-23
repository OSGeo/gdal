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

#include "ogr_libkml.h"

void featurestyle2kml(OGRLIBKMLDataSource *poOgrDS, OGRLayer *poKOgrLayer,
                      OGRFeature *poOgrFeat, kmldom::KmlFactory *poKmlFactory,
                      kmldom::FeaturePtr poKmlFeature);

/******************************************************************************
 function to read a kml style into ogr's featurestyle
******************************************************************************/

void kml2featurestyle(kmldom::FeaturePtr poKmlFeature,
                      OGRLIBKMLDataSource *poOgrDS, OGRLayer *poOgrLayer,
                      OGRFeature *poOgrFeat);
