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

#ifndef OGR_LIBKML_FEATURE_H
#define OGR_LIBKML_FEATURE_H

#include "ogr_libkml.h"

/******************************************************************************
 Function to output a ogr feature to a kml placemark.
******************************************************************************/

kmldom::FeaturePtr feat2kml(OGRLIBKMLDataSource *poOgrDS,
                            OGRLIBKMLLayer *poKOgrLayer, OGRFeature *poOgrFeat,
                            kmldom::KmlFactory *poKmlFactory,
                            int bUseSimpleField);

/******************************************************************************
 Function to read a kml placemark into a ogr feature.
******************************************************************************/

OGRFeature *kml2feat(kmldom::PlacemarkPtr poKmlPlacemark,
                     OGRLIBKMLDataSource *poOgrDS, OGRLIBKMLLayer *poOgrLayer,
                     OGRFeatureDefn *poOgrFeatDefn,
                     OGRSpatialReference *poOgrSRS);

OGRFeature *kmlgroundoverlay2feat(kmldom::GroundOverlayPtr poKmlOverlay,
                                  OGRLIBKMLDataSource *poOgrDS,
                                  OGRLIBKMLLayer *poOgrLayer,
                                  OGRFeatureDefn *poOgrFeatDefn,
                                  OGRSpatialReference *poOgrSRS);

#endif /*  OGR_LIBKML_FEATURE_H */
