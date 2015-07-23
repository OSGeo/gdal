/******************************************************************************
 * $Id$
 *
 * Project:  GML registry
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _GMLREGISTRY_H_INCLUDED
#define _GMLREGISTRY_H_INCLUDED

#include "cpl_string.h"
#include "cpl_minixml.h"

#include <vector>

class GMLRegistryFeatureType
{
    public:
        CPLString                           osElementName;
        CPLString                           osElementValue;
        CPLString                           osSchemaLocation;
        CPLString                           osGFSSchemaLocation;

        int Parse(const char* pszRegistryFilename, CPLXMLNode* psNode);
};

class GMLRegistryNamespace
{
    public:
        GMLRegistryNamespace() : bUseGlobalSRSName(FALSE) {}

        CPLString                           osPrefix;
        CPLString                           osURI;
        int                                 bUseGlobalSRSName;
        std::vector<GMLRegistryFeatureType> aoFeatureTypes;

        int Parse(const char* pszRegistryFilename, CPLXMLNode* psNode);
};

class GMLRegistry
{
        CPLString osRegistryPath;

    public:
        std::vector<GMLRegistryNamespace> aoNamespaces;

        GMLRegistry(const CPLString& osRegistryPath) : osRegistryPath(osRegistryPath) {}
        int Parse();
};

#endif /* _GMLREGISTRY_H_INCLUDED */
