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

#include "gmlregistry.h"

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

int GMLRegistry::Parse()
{
    if( osRegistryPath.size() == 0 )
    {
        const char* pszFilename = CPLFindFile( "gdal", "gml_registry.xml" );
        if( pszFilename )
            osRegistryPath = pszFilename;
    }
    if( osRegistryPath.size() == 0 )
        return FALSE;
    CPLXMLNode* psRootNode = CPLParseXMLFile(osRegistryPath);
    if( psRootNode == NULL )
        return FALSE;
    CPLXMLNode *psRegistryNode = CPLGetXMLNode( psRootNode, "=gml_registry" );
    if( psRegistryNode == NULL )
    {
        CPLDestroyXMLNode(psRootNode);
        return FALSE;
    }
    CPLXMLNode* psIter = psRegistryNode->psChild;
    while( psIter != NULL )
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
    return TRUE;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

int GMLRegistryNamespace::Parse(const char* pszRegistryFilename, CPLXMLNode* psNode)
{
    const char* pszPrefix = CPLGetXMLValue(psNode, "prefix", NULL);
    const char* pszURI = CPLGetXMLValue(psNode, "uri", NULL);
    if( pszPrefix == NULL || pszURI == NULL )
        return FALSE;
    osPrefix = pszPrefix;
    osURI = pszURI;
    const char* pszUseGlobalSRSName = CPLGetXMLValue(psNode, "useGlobalSRSName", NULL);
    if( pszUseGlobalSRSName != NULL && strcmp(pszUseGlobalSRSName, "true") == 0 )
        bUseGlobalSRSName = TRUE;

    CPLXMLNode* psIter = psNode->psChild;
    while( psIter != NULL )
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
    return TRUE;
}

/************************************************************************/
/*                           Parse()                                    */
/************************************************************************/

int GMLRegistryFeatureType::Parse(const char* pszRegistryFilename, CPLXMLNode* psNode)
{
    const char* pszElementName = CPLGetXMLValue(psNode, "elementName", NULL);
    const char* pszElementValue = CPLGetXMLValue(psNode, "elementValue", NULL);
    const char* pszSchemaLocation = CPLGetXMLValue(psNode, "schemaLocation", NULL);
    const char* pszGFSSchemaLocation = CPLGetXMLValue(psNode, "gfsSchemaLocation", NULL);
    if( pszElementName == NULL || (pszSchemaLocation == NULL && pszGFSSchemaLocation == NULL) )
        return FALSE;
    osElementName = pszElementName;

    if( pszSchemaLocation != NULL )
    {
        if( strncmp(pszSchemaLocation, "http://", 7) != 0 &&
            strncmp(pszSchemaLocation, "https://", 8) != 0 &&
            CPLIsFilenameRelative(pszSchemaLocation ) )
        {
            pszSchemaLocation = CPLFormFilename(
                CPLGetPath(pszRegistryFilename), pszSchemaLocation, NULL );
        }
        osSchemaLocation = pszSchemaLocation;
    }
    else if( pszGFSSchemaLocation != NULL )
    {
        if( strncmp(pszGFSSchemaLocation, "http://", 7) != 0 &&
            strncmp(pszGFSSchemaLocation, "https://", 8) != 0 &&
            CPLIsFilenameRelative(pszGFSSchemaLocation ) )
        {
            pszGFSSchemaLocation = CPLFormFilename(
                CPLGetPath(pszRegistryFilename), pszGFSSchemaLocation, NULL );
        }
        osGFSSchemaLocation = pszGFSSchemaLocation;
    }

    if ( pszElementValue != NULL )
    {
        osElementValue = pszElementValue; 
    }

    return TRUE;
}
