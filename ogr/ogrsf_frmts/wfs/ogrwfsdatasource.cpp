/******************************************************************************
 * $Id$
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGRWFSDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "ogr_wfs.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "gmlutils.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRWFSDataSource()                          */
/************************************************************************/

OGRWFSDataSource::OGRWFSDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;

    bGetFeatureSupportHits = FALSE;
    bNeedNAMESPACE = FALSE;
}

/************************************************************************/
/*                         ~OGRWFSDataSource()                          */
/************************************************************************/

OGRWFSDataSource::~OGRWFSDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWFSDataSource::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRWFSDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetLayerByName()                            */
/************************************************************************/

OGRLayer* OGRWFSDataSource::GetLayerByName(const char* pszName)
{
    if ( ! pszName )
        return NULL;

    int  i;

    /* first a case sensitive check */
    for( i = 0; i < nLayers; i++ )
    {
        OGRWFSLayer *poLayer = papoLayers[i];

        if( strcmp( pszName, poLayer->GetName() ) == 0 )
            return poLayer;
    }

    /* then case insensitive */
    for( i = 0; i < nLayers; i++ )
    {
        OGRWFSLayer *poLayer = papoLayers[i];

        if( EQUAL( pszName, poLayer->GetName() ) )
            return poLayer;
    }

    return NULL;
}

/************************************************************************/
/*                    FindSubStringInsensitive()                        */
/************************************************************************/

static const char* FindSubStringInsensitive(const char* pszStr,
                                            const char* pszSubStr)
{
    while(*pszStr)
    {
        const char* pszStrIter = pszStr;
        const char* pszSubStrIter = pszSubStr;
        while(*pszSubStrIter)
        {
            if (toupper((int)*pszStrIter) != toupper((int)*pszSubStrIter))
            {
                break;
            }
            pszStrIter ++;
            pszSubStrIter ++;
        }
        if (*pszSubStrIter == 0)
            return pszStrIter - (pszSubStrIter - pszSubStr);
        pszStr ++;
    }
    return NULL;
}

/************************************************************************/
/*                 DetectIfGetFeatureSupportHits()                      */
/************************************************************************/

static int DetectIfGetFeatureSupportHits(CPLXMLNode* psRoot)
{
    CPLXMLNode* psOperationsMetadata =
        CPLGetXMLNode(psRoot, "OperationsMetadata");
    if (!psOperationsMetadata)
    {
        CPLDebug("WFS", "Could not find <OperationsMetadata>");
        return FALSE;
    }

    CPLXMLNode* psChild = psOperationsMetadata->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Operation") == 0 &&
            strcmp(CPLGetXMLValue(psChild, "name", ""), "GetFeature") == 0)
        {
            break;
        }
        psChild = psChild->psNext;
    }
    if (!psChild)
    {
        CPLDebug("WFS", "Could not find <Operation name=\"GetFeature\">");
        return FALSE;
    }

    psChild = psChild->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Parameter") == 0 &&
            strcmp(CPLGetXMLValue(psChild, "name", ""), "resultType") == 0)
        {
            break;
        }
        psChild = psChild->psNext;
    }
   if (!psChild)
    {
        CPLDebug("WFS", "Could not find <Parameter name=\"resultType\">");
        return FALSE;
    }

    psChild = psChild->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Value") == 0)
        {
            CPLXMLNode* psChild2 = psChild->psChild;
            while(psChild2)
            {
                if (psChild2->eType == CXT_Text &&
                    strcmp(psChild2->pszValue, "hits") == 0)
                {
                    CPLDebug("WFS", "GetFeature operation supports hits");
                    return TRUE;
                }
                psChild2 = psChild2->psNext;
            }
        }
        psChild = psChild->psNext;
    }

    return FALSE;
}

/************************************************************************/
/*                         FetchValueFromURL()                          */
/************************************************************************/

CPLString FetchValueFromURL(const char* pszURL, const char* pszKey)
{
    CPLString osKey(pszKey);
    osKey += "=";
    const char* pszExistingKey = FindSubStringInsensitive(pszURL, osKey);
    if (pszExistingKey)
    {
        CPLString osValue(pszExistingKey + strlen(osKey));
        const char* pszValue = osValue.c_str();
        const char* pszSep = strchr(pszValue, '&');
        if (pszSep)
        {
            osValue.resize(pszSep - pszValue);
        }
        return osValue;
    }
    return "";
}

/************************************************************************/
/*                          AddKVToURL()                                */
/************************************************************************/

CPLString AddKVToURL(const char* pszURL, const char* pszKey, const char* pszValue)
{
    CPLString osURL(pszURL);
    if (strchr(osURL, '?') == NULL)
        osURL += "?";
    pszURL = osURL.c_str();

    CPLString osKey(pszKey);
    osKey += "=";
    const char* pszExistingKey = FindSubStringInsensitive(pszURL, osKey);
    if (pszExistingKey)
    {
        CPLString osNewURL(osURL);
        osNewURL.resize(pszExistingKey - pszURL);
        if (pszValue)
        {
            if (osNewURL[osNewURL.size()-1] != '&' && osNewURL[osNewURL.size()-1] != '?')
                osNewURL += '&';
            osNewURL += osKey;
            osNewURL += pszValue;
        }
        const char* pszNext = strchr(pszExistingKey, '&');
        if (pszNext)
        {
            if (osNewURL[osNewURL.size()-1] == '&')
                osNewURL += pszNext + 1;
            else
                osNewURL += pszNext;
        }
        return osNewURL;
    }
    else
    {
        if (pszValue)
        {
            if (osURL[osURL.size()-1] != '&' && osURL[osURL.size()-1] != '?')
                osURL += '&';
            osURL += osKey;
            osURL += pszValue;
        }
        return osURL;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRWFSDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    if (bUpdateIn)
    {
        return FALSE;
    }

    if (!EQUALN(pszFilename, "WFS:http", 8) &&
        FindSubStringInsensitive(pszFilename, "SERVICE=WFS") == NULL)
    {
        return FALSE;
    }

    pszName = CPLStrdup(pszFilename);

    const char* pszBaseURL = pszFilename;
    if (EQUALN(pszFilename, "WFS:", 4))
        pszBaseURL += 4;

    CPLString osURL(pszBaseURL);
    osURL = AddKVToURL(osURL, "SERVICE", "WFS");
    osURL = AddKVToURL(osURL, "REQUEST", "GetCapabilities");
    CPLString osTypeName = FetchValueFromURL(osURL, "TYPENAME");
    osURL = AddKVToURL(osURL, "TYPENAME", NULL);
    osURL = AddKVToURL(osURL, "FILTER", NULL);
    osURL = AddKVToURL(osURL, "PROPERTYNAME", NULL);
    
    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = CPLHTTPFetch( osURL, NULL);
    if (psResult->nStatus != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                 psResult->pszErrBuf);
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }
    if (strstr(psResult->pszContentType, "xml") == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Non XML content returned by server : %s, %s",
                 psResult->pszContentType, psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }
    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );

    int bInvertAxisOrderIfLatLong = CSLTestBoolean(CPLGetConfigOption(
                                  "GML_INVERT_AXIS_ORDER_IF_LAT_LONG", "YES"));

    if (psXML)
    {
        CPLStripXMLNamespace( psXML, NULL, TRUE );
        CPLXMLNode* psRoot = CPLGetXMLNode( psXML, "=WFS_Capabilities" );
        if (psRoot == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find <WFS_Capabilities>");
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return FALSE;
        }

        osVersion = CPLGetXMLValue(psRoot, "version", "1.0.0");

        bGetFeatureSupportHits = DetectIfGetFeatureSupportHits(psRoot);
        
        CPLXMLNode* psChild = CPLGetXMLNode(psRoot, "FeatureTypeList");
        if (psChild == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find <FeatureTypeList>");
            CPLDestroyXMLNode( psXML );
            CPLHTTPDestroyResult(psResult);
            return FALSE;
        }

        CPLXMLNode* psChildIter;
        for(psChildIter = psChild->psChild;
            psChildIter != NULL;
            psChildIter = psChildIter->psNext)
        {
            if (psChildIter->eType == CXT_Element &&
                strcmp(psChildIter->pszValue, "FeatureType") == 0)
            {
                const char* pszNS = NULL;
                const char* pszNSVal = NULL;
                CPLXMLNode* psFeatureTypeIter = psChildIter->psChild;
                while(psFeatureTypeIter != NULL)
                {
                    if (psFeatureTypeIter->eType == CXT_Attribute)
                    {
                        pszNS = psFeatureTypeIter->pszValue;
                        pszNSVal = psFeatureTypeIter->psChild->pszValue;
                    }
                    psFeatureTypeIter = psFeatureTypeIter->psNext;
                }

                const char* pszName = CPLGetXMLValue(psChildIter, "Name", NULL);
                if (pszName != NULL &&
                    (osTypeName.size() == 0 ||
                     strcmp(osTypeName.c_str(), pszName) == 0))
                {
                    const char* pszDefaultSRS =
                            CPLGetXMLValue(psChildIter, "DefaultSRS", NULL);
                    if (pszDefaultSRS == NULL)
                        pszDefaultSRS = CPLGetXMLValue(psChildIter, "SRS", NULL);

                    OGRSpatialReference* poSRS = NULL;
                    int bAxisOrderAlreadyInverted = FALSE;
                    if (pszDefaultSRS)
                    {
                        OGRSpatialReference oSRS;
                        if (oSRS.SetFromUserInput(pszDefaultSRS) == OGRERR_NONE)
                        {
                            poSRS = oSRS.Clone();
                            if (bInvertAxisOrderIfLatLong &&
                                GML_IsSRSLatLongOrder(pszDefaultSRS))
                            {
                                bAxisOrderAlreadyInverted = TRUE;

                                OGR_SRSNode *poGEOGCS =
                                                poSRS->GetAttrNode( "GEOGCS" );
                                if( poGEOGCS != NULL )
                                {
                                    poGEOGCS->StripNodes( "AXIS" );
                                }
                            }
                        }
                    }

                    CPLXMLNode* psBBox = NULL;
                    CPLXMLNode* psLatLongBBox = NULL;
                    int bFoundBBox = FALSE;
                    double dfMinX = 0, dfMinY = 0, dfMaxX = 0, dfMaxY = 0;
                    if ((psBBox = CPLGetXMLNode(psChildIter, "WGS84BoundingBox")) != NULL)
                    {
                        const char* pszLC = CPLGetXMLValue(psBBox, "LowerCorner", NULL);
                        const char* pszUC = CPLGetXMLValue(psBBox, "UpperCorner", NULL);
                        if (pszLC != NULL && pszUC != NULL)
                        {
                            CPLString osConcat(pszLC);
                            osConcat += " ";
                            osConcat += pszUC;
                            char** papszTokens;
                            papszTokens = CSLTokenizeStringComplex(
                                                osConcat, " ,", FALSE, FALSE );
                            if (CSLCount(papszTokens) == 4)
                            {
                                bFoundBBox = TRUE;
                                dfMinX = CPLAtof(papszTokens[0]);
                                dfMinY = CPLAtof(papszTokens[1]);
                                dfMaxX = CPLAtof(papszTokens[2]);
                                dfMaxY = CPLAtof(papszTokens[3]);
                            }
                            CSLDestroy(papszTokens);
                        }
                    }
                    else if ((psLatLongBBox = CPLGetXMLNode(psChildIter,
                                                "LatLongBoundingBox")) != NULL)
                    {
                        const char* pszMinX =
                            CPLGetXMLValue(psLatLongBBox, "minx", NULL);
                        const char* pszMinY =
                            CPLGetXMLValue(psLatLongBBox, "miny", NULL);
                        const char* pszMaxX =
                            CPLGetXMLValue(psLatLongBBox, "maxx", NULL);
                        const char* pszMaxY =
                            CPLGetXMLValue(psLatLongBBox, "maxy", NULL);
                        if (pszMinX != NULL && pszMinY != NULL &&
                            pszMaxX != NULL && pszMaxY != NULL)
                        {
                            bFoundBBox = TRUE;
                            dfMinX = CPLAtof(pszMinX);
                            dfMinY = CPLAtof(pszMinY);
                            dfMaxX = CPLAtof(pszMaxX);
                            dfMaxY = CPLAtof(pszMaxY);
                        }
                    }

                    OGRWFSLayer* poLayer = new OGRWFSLayer(
                                this, poSRS, bAxisOrderAlreadyInverted,
                                pszBaseURL, pszName, pszNS, pszNSVal);

                    if (poSRS)
                    {
                        char* pszProj4 = NULL;
                        if (poSRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                        {
                            if (strcmp(pszProj4, "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs ") == 0)
                            {
                                poLayer->SetExtents(dfMinX, dfMinY, dfMaxX, dfMaxY);
                            }
#if 0
                            else
                            {
                                OGRSpatialReference oWGS84;
                                oWGS84.SetWellKnownGeogCS("WGS84");
                                OGRCoordinateTransformation* poCT;
                                poCT = OGRCreateCoordinateTransformation(&oWGS84, poSRS);
                                if (poCT)
                                {
                                    double dfULX = dfMinX;
                                    double dfULY = dfMaxY;
                                    double dfURX = dfMaxX;
                                    double dfURY = dfMaxY;
                                    double dfLLX = dfMinX;
                                    double dfLLY = dfMinY;
                                    double dfLRX = dfMaxX;
                                    double dfLRY = dfMinY;
                                    if (poCT->Transform(1, &dfULX, &dfULY, NULL) &&
                                        poCT->Transform(1, &dfURX, &dfURY, NULL) &&
                                        poCT->Transform(1, &dfLLX, &dfLLY, NULL) &&
                                        poCT->Transform(1, &dfLRX, &dfLRY, NULL))
                                    {
                                        dfMinX = dfULX;
                                        dfMinX = MIN(dfMinX, dfURX);
                                        dfMinX = MIN(dfMinX, dfLLX);
                                        dfMinX = MIN(dfMinX, dfLRX);

                                        dfMinY = dfULY;
                                        dfMinY = MIN(dfMinY, dfURY);
                                        dfMinY = MIN(dfMinY, dfLLY);
                                        dfMinY = MIN(dfMinY, dfLRY);

                                        dfMaxX = dfULX;
                                        dfMaxX = MAX(dfMaxX, dfURX);
                                        dfMaxX = MAX(dfMaxX, dfLLX);
                                        dfMaxX = MAX(dfMaxX, dfLRX);

                                        dfMaxY = dfULY;
                                        dfMaxY = MAX(dfMaxY, dfURY);
                                        dfMaxY = MAX(dfMaxY, dfLLY);
                                        dfMaxY = MAX(dfMaxY, dfLRY);

                                        poLayer->SetExtents(dfMinX, dfMinY, dfMaxX, dfMaxY);
                                    }
                                }
                                delete poCT;
                            }
#endif
                        }
                        CPLFree(pszProj4);
                    }

                    papoLayers = (OGRWFSLayer **)CPLRealloc(papoLayers,
                                        sizeof(OGRWFSLayer*) * (nLayers + 1));
                    papoLayers[nLayers ++] = poLayer;
                }
            }
        }

        CPLDestroyXMLNode( psXML );
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                 psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return FALSE;
    }
    CPLHTTPDestroyResult(psResult);
    
    return TRUE;
}


/************************************************************************/
/*                           IsOldDeegree()                             */
/************************************************************************/

int OGRWFSDataSource::IsOldDeegree(const char* pszErrorString)
{
    if (!bNeedNAMESPACE &&
        strstr(pszErrorString, "<ServiceException code=\"InvalidParameterValue\" locator=\"-\">Invalid \"TYPENAME\" parameter. No binding for prefix") != NULL)
    {
        bNeedNAMESPACE = TRUE;
        return TRUE;
    }
    return FALSE;
}
