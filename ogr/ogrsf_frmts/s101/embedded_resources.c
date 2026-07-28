// SPDX-License-Identifier: MIT
// Copyright 2026, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char featureCatalogXML[] = {
#embed "data/101_Feature_Catalogue_2.0.0.xml"
    , 0};

const char *S101GetEmbeddedFeatureCatalog()
{
    return featureCatalogXML;
}
