/******************************************************************************
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault <even.rouault at spatialys.com>
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
 ****************************************************************************/

#include "netcdfdataset.h"

CPL_CVSID("$Id$")

bool netCDFWriterConfiguration::SetNameValue(
    CPLXMLNode *psNode, std::map<CPLString, CPLString> &oMap)
{
    const char *pszName = CPLGetXMLValue(psNode, "name", NULL);
    const char *pszValue = CPLGetXMLValue(psNode, "value", NULL);
    if( pszName != NULL && pszValue != NULL )
    {
        oMap[pszName] = pszValue;
        return true;
    }
    CPLError(CE_Failure, CPLE_IllegalArg, "Missing name/value");
    return false;
}

bool netCDFWriterConfiguration::Parse(const char *pszFilename)
{
    CPLXMLNode *psRoot =
        STARTS_WITH(pszFilename, "<Configuration")
        ? CPLParseXMLString(pszFilename)
        : CPLParseXMLFile(pszFilename);
    if( psRoot == NULL )
        return false;
    CPLXMLTreeCloser oCloser(psRoot);

    for( CPLXMLNode *psIter = psRoot->psChild; psIter != NULL;
         psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element )
            continue;
        if( EQUAL(psIter->pszValue, "DatasetCreationOption") )
        {
            SetNameValue(psIter, m_oDatasetCreationOptions);
        }
        else if( EQUAL(psIter->pszValue, "LayerCreationOption") )
        {
            SetNameValue(psIter, m_oLayerCreationOptions);
        }
        else if( EQUAL(psIter->pszValue, "Attribute") )
        {
            netCDFWriterConfigAttribute oAtt;
            if( oAtt.Parse(psIter) )
                m_aoAttributes.push_back(oAtt);
        }
        else if( EQUAL(psIter->pszValue, "Field") )
        {
            netCDFWriterConfigField oField;
            if( oField.Parse(psIter) )
                m_oFields[!oField.m_osName.empty()
                              ? oField.m_osName
                              : CPLString("__") + oField.m_osNetCDFName] =
                    oField;
        }
        else if( EQUAL(psIter->pszValue, "Layer") )
        {
            netCDFWriterConfigLayer oLayer;
            if( oLayer.Parse(psIter) )
                m_oLayers[oLayer.m_osName] = oLayer;
        }
        else
        {
            CPLDebug("GDAL_netCDF", "Ignoring %s", psIter->pszValue);
        }
    }

    m_bIsValid = true;

    return true;
}

bool netCDFWriterConfigAttribute::Parse(CPLXMLNode *psNode)
{
    const char *pszName = CPLGetXMLValue(psNode, "name", NULL);
    const char *pszValue = CPLGetXMLValue(psNode, "value", NULL);
    const char *pszType = CPLGetXMLValue(psNode, "type", "string");
    if( !EQUAL(pszType, "string") && !EQUAL(pszType, "integer") &&
        !EQUAL(pszType, "double") )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "type='%s' unsupported",
                 pszType);
        return false;
    }
    if( pszName == NULL || pszValue == NULL )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Missing name/value");
        return false;
    }
    m_osName = pszName;
    m_osValue = pszValue;
    m_osType = pszType;
    return true;
}

bool netCDFWriterConfigField::Parse(CPLXMLNode *psNode)
{
    const char *pszName = CPLGetXMLValue(psNode, "name", NULL);
    const char *pszNetCDFName = CPLGetXMLValue(psNode, "netcdf_name", pszName);
    const char *pszMainDim = CPLGetXMLValue(psNode, "main_dim", NULL);
    if( pszName == NULL && pszNetCDFName == NULL )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Bot name and netcdf_name are missing");
        return false;
    }
    if( pszName != NULL )
        m_osName = pszName;
    if( pszNetCDFName != NULL )
        m_osNetCDFName = pszNetCDFName;
    if( pszMainDim != NULL )
        m_osMainDim = pszMainDim;

    for( CPLXMLNode *psIter = psNode->psChild; psIter != NULL;
         psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element )
            continue;
        if( EQUAL(psIter->pszValue, "Attribute") )
        {
            netCDFWriterConfigAttribute oAtt;
            if( oAtt.Parse(psIter) )
                m_aoAttributes.push_back(oAtt);
        }
        else
        {
            CPLDebug("GDAL_netCDF", "Ignoring %s", psIter->pszValue);
        }
    }

    return true;
}

bool netCDFWriterConfigLayer::Parse(CPLXMLNode *psNode)
{
    const char *pszName = CPLGetXMLValue(psNode, "name", NULL);
    const char *pszNetCDFName = CPLGetXMLValue(psNode, "netcdf_name", pszName);
    if( pszName == NULL )
    {
        CPLError(CE_Failure, CPLE_IllegalArg, "Missing name");
        return false;
    }
    m_osName = pszName;
    if( pszNetCDFName != NULL )
        m_osNetCDFName = pszNetCDFName;

    for( CPLXMLNode *psIter = psNode->psChild; psIter != NULL;
         psIter = psIter->psNext )
    {
        if( psIter->eType != CXT_Element )
            continue;
        if( EQUAL(psIter->pszValue, "LayerCreationOption") )
        {
            netCDFWriterConfiguration::SetNameValue(psIter,
                                                    m_oLayerCreationOptions);
        }
        else if( EQUAL(psIter->pszValue, "Attribute") )
        {
            netCDFWriterConfigAttribute oAtt;
            if( oAtt.Parse(psIter) )
                m_aoAttributes.push_back(oAtt);
        }
        else if( EQUAL(psIter->pszValue, "Field") )
        {
            netCDFWriterConfigField oField;
            if( oField.Parse(psIter) )
                m_oFields[!oField.m_osName.empty()
                              ? oField.m_osName
                              : CPLString("__") + oField.m_osNetCDFName] =
                    oField;
        }
        else
        {
            CPLDebug("GDAL_netCDF", "Ignoring %s", psIter->pszValue);
        }
    }

    return true;
}
