/******************************************************************************
 *
 * Project:  GML registry
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GMLREGISTRY_H_INCLUDED
#define GMLREGISTRY_H_INCLUDED

#include "cpl_string.h"
#include "cpl_minixml.h"

#include <vector>

class GMLRegistryFeatureType
{
  public:
    CPLString osElementName{};
    CPLString osElementValue{};
    CPLString osSchemaLocation{};
    CPLString osGFSSchemaLocation{};

    bool Parse(const char *pszRegistryFilename, CPLXMLNode *psNode);
};

class GMLRegistryNamespace
{
  public:
    GMLRegistryNamespace() = default;

    CPLString osPrefix{};
    CPLString osURI{};
    bool bUseGlobalSRSName = false;
    std::vector<GMLRegistryFeatureType> aoFeatureTypes{};

    bool Parse(const char *pszRegistryFilename, CPLXMLNode *psNode);
};

class GMLRegistry
{
    CPLString osRegistryPath{};

  public:
    std::vector<GMLRegistryNamespace> aoNamespaces{};

    explicit GMLRegistry(const CPLString &osRegistryPathIn)
        : osRegistryPath(osRegistryPathIn)
    {
    }

    bool Parse();
};

#endif /* GMLREGISTRY_H_INCLUDED */
