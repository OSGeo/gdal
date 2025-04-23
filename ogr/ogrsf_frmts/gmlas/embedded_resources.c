// SPDX-License-Identifier: MIT
// Copyright 2024, Even Rouault <even.rouault at spatialys.com>

#include "embedded_resources.h"

static const char gmlasconf_xsd[] = {
#embed "data/gmlasconf.xsd"
    , 0};

static const char gmlasconf_xml[] = {
#embed "data/gmlasconf.xml"
    , 0};

const char *GMLASConfXSDGetFileContent(void)
{
    return gmlasconf_xsd;
}

const char *GMLASConfXMLGetFileContent(void)
{
    return gmlasconf_xml;
}
