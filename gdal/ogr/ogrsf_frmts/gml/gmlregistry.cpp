/******************************************************************************
 *
 * Project:  GML registry
 * Purpose:  GML reader
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "gmlregistry.h"

#include <cstring>

#include "cpl_conv.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

bool GMLRegistry::Parse()
{
    if( osRegistryPath.empty() )
    {
        const char *pszFilename = CPLFindFile("gdal", "gml_registry.xml");
        if( pszFilename )
            osRegistryPath = pszFilename;
    }
    if( osRegistryPath.empty() )
        return false;
    CPLXMLNode *psRootNode = CPLParseXMLFile(osRegistryPath);
    if( psRootNode == nullptr )
        return false;
    CPLXMLNode *psRegistryNode = CPLGetXMLNode(psRootNode, "=gml_registry");
    if( psRegistryNode == nullptr )
    {
        CPLDestroyXMLNode(psRootNode);
        return false;
    }
    CPLXMLNode *psIter = psRegistryNode->psChild;
    while( psIter != nullptr )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "namespace") == 0 )
        {
            GMLRegistryNamespace oNameSpace;
            if( oNameSpace.Parse(osRegistryPath, psIter) )
            {
                aoNamespaces.push_back(oNameSpace);
            }
        }
        psIter = psIter->psNext;
    }
    CPLDestroyXMLNode(psRootNode);
    return true;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

bool GMLRegistryNamespace::Parse(const char *pszRegistryFilename,
                                 CPLXMLNode *psNode)
{
    const char *pszPrefix = CPLGetXMLValue(psNode, "prefix", "");
    const char *pszURI = CPLGetXMLValue(psNode, "uri", nullptr);
    if( pszURI == nullptr )
        return false;
    osPrefix = pszPrefix;
    osURI = pszURI;
    const char *pszUseGlobalSRSName =
        CPLGetXMLValue(psNode, "useGlobalSRSName", nullptr);
    if( pszUseGlobalSRSName != nullptr &&
        strcmp(pszUseGlobalSRSName, "true") == 0 )
        bUseGlobalSRSName = true;

    CPLXMLNode *psIter = psNode->psChild;
    while( psIter != nullptr )
    {
        if( psIter->eType == CXT_Element &&
            strcmp(psIter->pszValue, "featureType") == 0 )
        {
            GMLRegistryFeatureType oFeatureType;
            if( oFeatureType.Parse(pszRegistryFilename, psIter) )
            {
                aoFeatureTypes.push_back(oFeatureType);
            }
        }
        psIter = psIter->psNext;
    }
    return true;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

bool GMLRegistryFeatureType::Parse(const char *pszRegistryFilename,
                                   CPLXMLNode *psNode)
{
    const char *pszElementName = CPLGetXMLValue(psNode, "elementName", nullptr);
    const char *pszSchemaLocation =
        CPLGetXMLValue(psNode, "schemaLocation", nullptr);
    const char *pszGFSSchemaLocation =
        CPLGetXMLValue(psNode, "gfsSchemaLocation", nullptr);
    if( pszElementName == nullptr ||
        (pszSchemaLocation == nullptr && pszGFSSchemaLocation == nullptr) )
        return false;

    const char *pszElementValue = CPLGetXMLValue(psNode, "elementValue", nullptr);
    osElementName = pszElementName;

    if( pszSchemaLocation != nullptr )
    {
        if( !STARTS_WITH(pszSchemaLocation, "http://") &&
            !STARTS_WITH(pszSchemaLocation, "https://") &&
            CPLIsFilenameRelative(pszSchemaLocation) )
        {
            pszSchemaLocation = CPLFormFilename(
                CPLGetPath(pszRegistryFilename), pszSchemaLocation, nullptr );
        }
        osSchemaLocation = pszSchemaLocation;
    }
    else if( pszGFSSchemaLocation != nullptr )
    {
        if( !STARTS_WITH(pszGFSSchemaLocation, "http://") &&
            !STARTS_WITH(pszGFSSchemaLocation, "https://") &&
            CPLIsFilenameRelative(pszGFSSchemaLocation) )
        {
            pszGFSSchemaLocation = CPLFormFilename(
                CPLGetPath(pszRegistryFilename), pszGFSSchemaLocation, nullptr);
        }
        osGFSSchemaLocation = pszGFSSchemaLocation;
    }

    if ( pszElementValue != nullptr )
    {
        osElementValue = pszElementValue;
    }

    return true;
}
