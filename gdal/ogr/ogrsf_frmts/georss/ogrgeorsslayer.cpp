/******************************************************************************
 * $Id$
 *
 * Project:  GeoRSS Translator
 * Purpose:  Implements OGRGeoRSSLayer class.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_georss.h"
#include "cpl_conv.h"
#include "cpl_minixml.h"
#include "ogr_api.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

static const char* apszAllowedATOMFieldNamesWithSubElements[] = { "author", "contributor", NULL };

static
const char* apszAllowedRSSFieldNames[] = {  "title", "link", "description", "author",
                                            "category", "category_domain",
                                            "comments",
                                            "enclosure_url", "enclosure_length", "enclosure_type",
                                            "guid", "guid_isPermaLink",
                                            "pubDate",
                                            "source", "source_url", NULL};

static
const char* apszAllowedATOMFieldNames[] = { "category_term", "category_scheme", "category_label",
                                            "content", "content_type", "content_xml_lang", "content_xml_base",
                                            "summary", "summary_type", "summary_xml_lang", "summary_xml_base",
                                            "author_name", "author_uri", "author_email",
                                            "contributor_name", "contributor_uri", "contributor_email",
                                            "link_href", "link_rel", "link_type", "link_length",
                                            "id", "published", "rights", "source",
                                            "title", "updated", NULL };

#define IS_LAT_ELEMENT(pszName) (strncmp(pszName, "geo:lat", strlen("geo:lat")) == 0 || \
                                 strncmp(pszName, "icbm:lat", strlen("icbm:lat")) == 0 || \
                                 strncmp(pszName, "geourl:lat", strlen("geourl:lat")) == 0)

#define IS_LON_ELEMENT(pszName) (strncmp(pszName, "geo:lon", strlen("geo:lon")) == 0 || \
                                 strncmp(pszName, "icbm:lon", strlen("icbm:lon")) == 0 || \
                                 strncmp(pszName, "geourl:lon", strlen("geourl:lon")) == 0)

#define IS_GEO_ELEMENT(pszName) (strcmp(pszName, "georss:point") == 0 || \
                                 strcmp(pszName, "georss:line") == 0 || \
                                 strcmp(pszName, "georss:box") == 0 || \
                                 strcmp(pszName, "georss:polygon") == 0 || \
                                 strcmp(pszName, "georss:where") == 0 || \
                                 strncmp(pszName, "gml:", strlen("gml:")) == 0 || \
                                 strncmp(pszName, "geo:", strlen("geo:")) == 0 || \
                                 strncmp(pszName, "icbm:", strlen("icbm:")) == 0 || \
                                 strncmp(pszName, "geourl:", strlen("geourl:")) == 0)

/************************************************************************/
/*                            OGRGeoRSSLayer()                          */
/************************************************************************/

OGRGeoRSSLayer::OGRGeoRSSLayer( const char* pszFilename,
                                const char* pszLayerName,
                                OGRGeoRSSDataSource* poDS,
                                OGRSpatialReference *poSRSIn,
                                int bWriteMode)

{
    eof = FALSE;
    nNextFID = 0;

    this->poDS = poDS;
    this->bWriteMode = bWriteMode;

    eFormat = poDS->GetFormat();

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    poSRS = poSRSIn;
    if (poSRS)
    {
        poSRS->Reference();
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    }

    nTotalFeatureCount = 0;

    ppoFeatureTab = NULL;
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    pszSubElementName = NULL;
    pszSubElementValue = NULL;
    nSubElementValueLen = 0;
    pszGMLSRSName = NULL;
    pszTagWithSubTag = NULL;
    bStopParsing = FALSE;
    bHasReadSchema = FALSE;
    setOfFoundFields = NULL;
    poGlobalGeom = NULL;
    hasFoundLat = FALSE;
    hasFoundLon = FALSE;

    poFeature = NULL;

#ifdef HAVE_EXPAT
    oParser = NULL;
#endif

    if (bWriteMode == FALSE)
    {
        fpGeoRSS = VSIFOpenL( pszFilename, "r" );
        if( fpGeoRSS == NULL )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszFilename);
            return;
        }
    }
    else
        fpGeoRSS = NULL;

    ResetReading();
}

/************************************************************************/
/*                            ~OGRGeoRSSLayer()                            */
/************************************************************************/

OGRGeoRSSLayer::~OGRGeoRSSLayer()

{
#ifdef HAVE_EXPAT
    if (oParser)
        XML_ParserFree(oParser);
#endif
    poFeatureDefn->Release();
    
    if( poSRS != NULL )
        poSRS->Release();

    CPLFree(pszSubElementName);
    CPLFree(pszSubElementValue);
    CPLFree(pszGMLSRSName);
    CPLFree(pszTagWithSubTag);
    if (setOfFoundFields)
        CPLHashSetDestroy(setOfFoundFields);
    if (poGlobalGeom)
        delete poGlobalGeom;

    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);

    if (poFeature)
        delete poFeature;

    if (fpGeoRSS)
        VSIFCloseL( fpGeoRSS );
}


/************************************************************************/
/*                            GetLayerDefn()                            */
/************************************************************************/

OGRFeatureDefn * OGRGeoRSSLayer::GetLayerDefn()
{
    if (!bHasReadSchema)
        LoadSchema();

    return poFeatureDefn;
}



#ifdef HAVE_EXPAT

static void XMLCALL startElementCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((OGRGeoRSSLayer*)pUserData)->startElementCbk(pszName, ppszAttr);
}

static void XMLCALL endElementCbk(void *pUserData, const char *pszName)
{
    ((OGRGeoRSSLayer*)pUserData)->endElementCbk(pszName);
}

static void XMLCALL dataHandlerCbk(void *pUserData, const char *data, int nLen)
{
    ((OGRGeoRSSLayer*)pUserData)->dataHandlerCbk(data, nLen);
}

#endif

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGeoRSSLayer::ResetReading()

{
    if (bWriteMode)
        return;

    eof = FALSE;
    nNextFID = 0;
    if (fpGeoRSS)
    {
        VSIFSeekL( fpGeoRSS, 0, SEEK_SET );
#ifdef HAVE_EXPAT
        if (oParser)
            XML_ParserFree(oParser);
        
        oParser = OGRCreateExpatXMLParser();
        XML_SetElementHandler(oParser, ::startElementCbk, ::endElementCbk);
        XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
        XML_SetUserData(oParser, this);
#endif
    }
    bInFeature = FALSE;
    hasFoundLat = FALSE;
    hasFoundLon = FALSE;
    bInSimpleGeometry = FALSE;
    bInGMLGeometry = FALSE;
    bInGeoLat = FALSE;
    bInGeoLong = FALSE;
    eGeomType = wkbUnknown;
    CPLFree(pszSubElementName);
    pszSubElementName = NULL;
    CPLFree(pszSubElementValue);
    pszSubElementValue = NULL;
    nSubElementValueLen = 0;
    CPLFree(pszGMLSRSName);
    pszGMLSRSName = NULL;

    if (setOfFoundFields)
        CPLHashSetDestroy(setOfFoundFields);
    setOfFoundFields = NULL;

    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    ppoFeatureTab = NULL;
    if (poFeature)
        delete poFeature;
    poFeature = NULL;

    currentDepth = 0;
    featureDepth = 0;
    geometryDepth = 0;
    bInTagWithSubTag = FALSE;
    CPLFree(pszTagWithSubTag);
    pszTagWithSubTag = NULL;
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                      AddStrToSubElementValue()                       */
/************************************************************************/

void OGRGeoRSSLayer::AddStrToSubElementValue(const char* pszStr)
{
    int len = strlen(pszStr);
    char* pszNewSubElementValue = (char*)
            VSIRealloc(pszSubElementValue, nSubElementValueLen + len + 1);
    if (pszNewSubElementValue == NULL)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
        XML_StopParser(oParser, XML_FALSE);
        bStopParsing = TRUE;
        return;
    }
    pszSubElementValue = pszNewSubElementValue;

    memcpy(pszSubElementValue + nSubElementValueLen, pszStr, len);
    nSubElementValueLen += len;
}

/************************************************************************/
/*              OGRGeoRSS_GetOGRCompatibleTagName()                     */
/************************************************************************/

/** Replace ':' from XML NS element name by '_' more OGR friendly */
static char* OGRGeoRSS_GetOGRCompatibleTagName(const char* pszName)
{
    char* pszModName = CPLStrdup(pszName);
    int i;
    for(i=0;pszModName[i] != 0;i++)
    {
        if (pszModName[i] == ':')
            pszModName[i] = '_';
    }
    return pszModName;
}

/************************************************************************/
/*               OGRGeoRSSLayerATOMTagHasSubElement()                   */
/************************************************************************/

static int OGRGeoRSSLayerATOMTagHasSubElement(const char* pszName)
{
    unsigned int i;
    for(i=0;apszAllowedATOMFieldNamesWithSubElements[i] != NULL;i++)
    {
        if (strcmp(pszName, apszAllowedATOMFieldNamesWithSubElements[i]) == 0)
            return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                        startElementCbk()                            */
/************************************************************************/

void OGRGeoRSSLayer::startElementCbk(const char *pszName, const char **ppszAttr)
{
    int bSerializeTag = FALSE;
    const char* pszNoNSName = pszName;
    const char* pszColon = strchr(pszNoNSName, ':');
    if( pszColon )
        pszNoNSName = pszColon + 1;

    if (bStopParsing) return;

    if ((eFormat == GEORSS_ATOM && currentDepth == 1 && strcmp(pszNoNSName, "entry") == 0) ||
        ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) && !bInFeature &&
        (currentDepth == 1 || currentDepth == 2) && strcmp(pszNoNSName, "item") == 0))
    {
        featureDepth = currentDepth;

        if (poFeature)
            delete poFeature;

        poFeature = new OGRFeature( poFeatureDefn );
        poFeature->SetFID( nNextFID++ );

        bInFeature = TRUE;
        hasFoundLat = FALSE;
        hasFoundLon = FALSE;
        bInSimpleGeometry = FALSE;
        bInGMLGeometry = FALSE;
        bInGeoLat = FALSE;
        bInGeoLong = FALSE;
        eGeomType = wkbUnknown;
        geometryDepth = 0;
        bInTagWithSubTag = FALSE;

        if (setOfFoundFields)
            CPLHashSetDestroy(setOfFoundFields);
        setOfFoundFields = CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);
    }
    else if (bInFeature && bInTagWithSubTag && currentDepth == 3)
    {
        char* pszFieldName = CPLStrdup(CPLSPrintf("%s_%s", pszTagWithSubTag, pszNoNSName));

        CPLFree(pszSubElementName);
        pszSubElementName = NULL;
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;

        iCurrentField = poFeatureDefn->GetFieldIndex(pszFieldName);
        if (iCurrentField >= 0)
            pszSubElementName = CPLStrdup(pszFieldName);

        CPLFree(pszFieldName);
    }
    else if (bInFeature && eFormat == GEORSS_ATOM &&
             currentDepth == 2 && OGRGeoRSSLayerATOMTagHasSubElement(pszNoNSName))
    {
        CPLFree(pszTagWithSubTag);
        pszTagWithSubTag = CPLStrdup(pszNoNSName);

        int count = 1;
        while(CPLHashSetLookup(setOfFoundFields, pszTagWithSubTag) != NULL)
        {
            count ++;
            CPLFree(pszTagWithSubTag);
            pszTagWithSubTag = CPLStrdup(CPLSPrintf("%s%d", pszNoNSName, count));
        }
        CPLHashSetInsert(setOfFoundFields, CPLStrdup(pszTagWithSubTag));

        bInTagWithSubTag = TRUE;
    }
    else if (bInGMLGeometry)
    {
        bSerializeTag = TRUE;
    }
    else if (bInSimpleGeometry || bInGeoLat || bInGeoLong)
    {
        /* Shouldn't happen for a valid document */
    }
    else if (IS_LAT_ELEMENT(pszName))
    {
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;
        bInGeoLat = TRUE;
    }
    else if (IS_LON_ELEMENT(pszName))
    {
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;
        bInGeoLong = TRUE;
    }
    else if (strcmp(pszName, "georss:point") == 0 ||
             strcmp(pszName, "georss:line") == 0 ||
             strcmp(pszName, "geo:line") == 0 ||
             strcmp(pszName, "georss:polygon") == 0 ||
             strcmp(pszName, "georss:box") == 0)
    {
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;
        eGeomType = strcmp(pszName, "georss:point") == 0 ?   wkbPoint :
                      (strcmp(pszName, "georss:line") == 0 ||
                       strcmp(pszName, "geo:line") == 0)  ?  wkbLineString :
                      (strcmp(pszName, "georss:polygon") == 0  ||
                       strcmp(pszName, "georss:box") == 0) ? wkbPolygon :
                                                             wkbUnknown;
        bInSimpleGeometry = TRUE;
        geometryDepth = currentDepth;
    }
    else if (strcmp(pszName, "gml:Point") == 0 ||
             strcmp(pszName, "gml:LineString") == 0 ||
             strcmp(pszName, "gml:Polygon") == 0 ||
             strcmp(pszName, "gml:MultiPoint") == 0 ||
             strcmp(pszName, "gml:MultiLineString") == 0 ||
             strcmp(pszName, "gml:MultiPolygon") == 0 ||
             strcmp(pszName, "gml:Envelope") == 0)
    {
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;
        AddStrToSubElementValue(CPLSPrintf("<%s>", pszName));
        bInGMLGeometry = TRUE;
        geometryDepth = currentDepth;
        CPLFree(pszGMLSRSName);
        pszGMLSRSName = NULL;
        for (int i = 0; ppszAttr[i]; i += 2)
        {
            if (strcmp(ppszAttr[i], "srsName") == 0)
            {
                if (pszGMLSRSName == NULL)
                    pszGMLSRSName = CPLStrdup(ppszAttr[i+1]);
            }
        }
    }
    else if (bInFeature && currentDepth == featureDepth + 1)
    {
        CPLFree(pszSubElementName);
        pszSubElementName = NULL;
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;
        iCurrentField = -1;

        if( pszName != pszNoNSName && strncmp(pszName, "atom:", 5) == 0 )
            pszName = pszNoNSName;

        pszSubElementName = CPLStrdup(pszName);
        int count = 1;
        while(CPLHashSetLookup(setOfFoundFields, pszSubElementName) != NULL)
        {
            count ++;
            CPLFree(pszSubElementName);
            pszSubElementName = CPLStrdup(CPLSPrintf("%s%d", pszName, count));
        }
        CPLHashSetInsert(setOfFoundFields, CPLStrdup(pszSubElementName));

        char* pszCompatibleName = OGRGeoRSS_GetOGRCompatibleTagName(pszSubElementName);
        iCurrentField = poFeatureDefn->GetFieldIndex(pszCompatibleName);
        CPLFree(pszSubElementName);

        for(int i = 0; ppszAttr[i] != NULL && ppszAttr[i+1] != NULL; i+=2)
        {
            char* pszAttrCompatibleName =
                    OGRGeoRSS_GetOGRCompatibleTagName(CPLSPrintf("%s_%s", pszCompatibleName, ppszAttr[i]));
            int iAttrField = poFeatureDefn->GetFieldIndex(pszAttrCompatibleName);
            if (iAttrField >= 0)
            {
                if (poFeatureDefn->GetFieldDefn(iAttrField)->GetType() == OFTReal)
                    poFeature->SetField( iAttrField, CPLAtof(ppszAttr[i+1]) );
                else
                    poFeature->SetField( iAttrField, ppszAttr[i+1] );
            }
            CPLFree(pszAttrCompatibleName);
        }

        if (iCurrentField < 0)
        {
            pszSubElementName = NULL;
        }
        else
        {
            pszSubElementName = CPLStrdup(pszCompatibleName);
        }
        CPLFree(pszCompatibleName);
    }
    else if (bInFeature && currentDepth > featureDepth + 1 && pszSubElementName != NULL)
    {
        bSerializeTag = TRUE;
    }

    if (bSerializeTag)
    {
        AddStrToSubElementValue("<");
        AddStrToSubElementValue(pszName);
        for(int i = 0; ppszAttr[i] != NULL && ppszAttr[i+1] != NULL; i+=2)
        {
            AddStrToSubElementValue(" ");
            AddStrToSubElementValue(ppszAttr[i]);
            AddStrToSubElementValue("=\"");
            AddStrToSubElementValue(ppszAttr[i+1]);
            AddStrToSubElementValue("\"");
        }
        AddStrToSubElementValue(">");
    }

    currentDepth++;
}

/************************************************************************/
/*            OGRGeoRSSLayerTrimLeadingAndTrailingSpaces()              */
/************************************************************************/

static void OGRGeoRSSLayerTrimLeadingAndTrailingSpaces(char* pszStr)
{
    int i;

    /* Trim leading spaces, tabs and newlines */
    i = 0;
    while(pszStr[i] != '\0' &&
          (pszStr[i] == ' ' || pszStr[i] == '\t' || pszStr[i] == '\n'))
        i ++;
    memmove(pszStr, pszStr + i, strlen(pszStr + i) + 1);

    /* Trim trailing spaces, tabs and newlines */
    i = strlen(pszStr) - 1;
    while(i >= 0 &&
          (pszStr[i] == ' ' || pszStr[i] == '\t' || pszStr[i] == '\n'))
    {
        pszStr[i] = '\0';
        i --;
    }
}

/************************************************************************/
/*                           endElementCbk()                            */
/************************************************************************/

void OGRGeoRSSLayer::endElementCbk(const char *pszName)
{
    OGRGeometry* poGeom = NULL;

    if (bStopParsing) return;

    currentDepth--;
    const char* pszNoNSName = pszName;
    const char* pszColon = strchr(pszNoNSName, ':');
    if( pszColon )
        pszNoNSName = pszColon + 1;

    if ((eFormat == GEORSS_ATOM && currentDepth == 1 && strcmp(pszNoNSName, "entry") == 0) ||
        ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) &&
         (currentDepth == 1 || currentDepth == 2) && strcmp(pszNoNSName, "item") == 0))
    {
        bInFeature = FALSE;
        bInTagWithSubTag = FALSE;

        if (hasFoundLat && hasFoundLon)
            poFeature->SetGeometryDirectly( new OGRPoint( lonVal, latVal ) );
        else if (poFeature->GetGeometryRef() == NULL && poGlobalGeom != NULL)
            poFeature->SetGeometry(poGlobalGeom);

        hasFoundLat = FALSE;
        hasFoundLon = FALSE;

        if (poSRS != NULL && poFeature->GetGeometryRef() != NULL)
            poFeature->GetGeometryRef()->assignSpatialReference(poSRS);

        if( (m_poFilterGeom == NULL
                || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature )) )
        {
            ppoFeatureTab = (OGRFeature**)
                    CPLRealloc(ppoFeatureTab,
                                sizeof(OGRFeature*) * (nFeatureTabLength + 1));
            ppoFeatureTab[nFeatureTabLength] = poFeature;
            nFeatureTabLength++;
        }
        else
        {
            delete poFeature;
        }
        poFeature = NULL;
        return;
    }

    if (bInTagWithSubTag && currentDepth == 3)
    {
        char* pszFieldName = CPLStrdup(CPLSPrintf("%s_%s", pszTagWithSubTag, pszNoNSName));

        if (iCurrentField != -1 && pszSubElementName &&
            strcmp(pszFieldName, pszSubElementName) == 0 && poFeature &&
            pszSubElementValue && nSubElementValueLen)
        {
            pszSubElementValue[nSubElementValueLen] = 0;
            if (poFeatureDefn->GetFieldDefn(iCurrentField)->GetType() == OFTReal)
                poFeature->SetField( iCurrentField, CPLAtof(pszSubElementValue) );
            else
                poFeature->SetField( iCurrentField, pszSubElementValue);
        }

        CPLFree(pszSubElementName);
        pszSubElementName = NULL;
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;

        CPLFree(pszFieldName);
    }
    else if (bInFeature && eFormat == GEORSS_ATOM &&
             currentDepth == 2 && OGRGeoRSSLayerATOMTagHasSubElement(pszNoNSName))
    {
        bInTagWithSubTag = FALSE;
    }
    else if (bInGMLGeometry)
    {
        AddStrToSubElementValue("</");
        AddStrToSubElementValue(pszName);
        AddStrToSubElementValue(">");
        if (currentDepth > geometryDepth)
        {
        }
        else
        {
            pszSubElementValue[nSubElementValueLen] = 0;
            CPLAssert(strncmp(pszName, "gml:", 4) == 0);
            poGeom = (OGRGeometry*) OGR_G_CreateFromGML(pszSubElementValue);

            if (poGeom != NULL && !poGeom->IsEmpty() )
            {
                int bSwapCoordinates = FALSE;
                if (pszGMLSRSName)
                {
                    OGRSpatialReference* poSRSFeature = new OGRSpatialReference();
                    poSRSFeature->importFromURN(pszGMLSRSName);
                    poGeom->assignSpatialReference(poSRSFeature);
                    poSRSFeature->Release();
                }
                else
                    bSwapCoordinates = TRUE; /* lat, lon WGS 84 */

                if (bSwapCoordinates)
                {
                    poGeom->swapXY();
                }
            }
            bInGMLGeometry = FALSE;
        }
    }
    else if (bInSimpleGeometry)
    {
        if (currentDepth > geometryDepth)
        {
            /* Shouldn't happen for a valid document */
        }
        else
        {
            if (pszSubElementValue)
            {
                pszSubElementValue[nSubElementValueLen] = 0;

                /* Trim any leading and trailing spaces, tabs, newlines, etc... */
                OGRGeoRSSLayerTrimLeadingAndTrailingSpaces(pszSubElementValue);

                /* Caution : Order is latitude, longitude */
                char** papszTokens =
                        CSLTokenizeStringComplex( pszSubElementValue,
                                                    " ,", TRUE, FALSE );

                int nTokens = CSLCount(papszTokens);
                if ((nTokens % 2) != 0 ||
                     (eGeomType == wkbPoint && nTokens != 2) ||
                     (eGeomType == wkbLineString && nTokens < 4) ||
                     (strcmp(pszName, "georss:polygon") == 0 && nTokens < 6) ||
                     (strcmp(pszName, "georss:box") == 0 && nTokens != 4))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong number of coordinates in %s",
                             pszSubElementValue);
                }
                else if (eGeomType == wkbPoint)
                {
                    poGeom = new OGRPoint( CPLAtof(papszTokens[1]),
                                           CPLAtof(papszTokens[0]) );
                }
                else if (eGeomType == wkbLineString)
                {
                    OGRLineString* poLineString = new OGRLineString ();
                    poGeom = poLineString;
                    int i;
                    for(i=0;i<nTokens;i+=2)
                    {
                        poLineString->addPoint( CPLAtof(papszTokens[i+1]),
                                              CPLAtof(papszTokens[i]) );
                    }
                }
                else if (eGeomType == wkbPolygon)
                {
                    OGRPolygon* poPolygon = new OGRPolygon();
                    OGRLinearRing* poLinearRing = new OGRLinearRing();
                    poGeom = poPolygon;
                    poPolygon->addRingDirectly(poLinearRing);
                    if (strcmp(pszName, "georss:polygon") == 0)
                    {
                        int i;
                        for(i=0;i<nTokens;i+=2)
                        {
                            poLinearRing->addPoint( CPLAtof(papszTokens[i+1]),
                                                    CPLAtof(papszTokens[i]) );
                        }
                    }
                    else
                    {
                        double lat1 = CPLAtof(papszTokens[0]);
                        double lon1 = CPLAtof(papszTokens[1]);
                        double lat2 = CPLAtof(papszTokens[2]);
                        double lon2 = CPLAtof(papszTokens[3]);
                        poLinearRing->addPoint( lon1, lat1 );
                        poLinearRing->addPoint( lon1, lat2 );
                        poLinearRing->addPoint( lon2, lat2 );
                        poLinearRing->addPoint( lon2, lat1 );
                        poLinearRing->addPoint( lon1, lat1 );
                    }
                }

                CSLDestroy(papszTokens);
            }
            bInSimpleGeometry = FALSE;
        }
    }
    else if (IS_LAT_ELEMENT(pszName))
    {
        if (pszSubElementValue)
        {
            hasFoundLat = TRUE;
            pszSubElementValue[nSubElementValueLen] = 0;
            latVal = CPLAtof(pszSubElementValue);
        }
        bInGeoLat = FALSE;
    }
    else if (IS_LON_ELEMENT(pszName))
    {
        if (pszSubElementValue)
        {
            hasFoundLon = TRUE;
            pszSubElementValue[nSubElementValueLen] = 0;
            lonVal = CPLAtof(pszSubElementValue);
        }
        bInGeoLong = FALSE;
    }
    else if (bInFeature && currentDepth == featureDepth + 1)
    {
        if (iCurrentField != -1 && pszSubElementName &&
            poFeature && pszSubElementValue && nSubElementValueLen)
        {
            pszSubElementValue[nSubElementValueLen] = 0;
            if (poFeatureDefn->GetFieldDefn(iCurrentField)->GetType() == OFTDateTime)
            {
                OGRField sField;
                if (OGRParseRFC822DateTime(pszSubElementValue, &sField))
                {
                    poFeature->SetField(iCurrentField, &sField);
                }
                else if (OGRParseXMLDateTime(pszSubElementValue, &sField))
                {
                    poFeature->SetField(iCurrentField, &sField);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                                "Could not parse %s as a valid dateTime", pszSubElementValue);
                }
            }
            else
            {
                if (poFeatureDefn->GetFieldDefn(iCurrentField)->GetType() == OFTReal)
                    poFeature->SetField( iCurrentField, CPLAtof(pszSubElementValue) );
                else
                    poFeature->SetField( iCurrentField, pszSubElementValue);
            }
        }

        CPLFree(pszSubElementName);
        pszSubElementName = NULL;
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;
    }
    else if (bInFeature && currentDepth > featureDepth + 1 && pszSubElementName != NULL)
    {
        AddStrToSubElementValue("</");
        AddStrToSubElementValue(pszName);
        AddStrToSubElementValue(">");
    }

    if (poGeom != NULL)
    {
        if (poFeature != NULL)
        {
            poFeature->SetGeometryDirectly(poGeom);
        }
        else if (!bInFeature)
        {
            if (poGlobalGeom != NULL)
                delete poGlobalGeom;
            poGlobalGeom = poGeom;
        }
        else
            delete poGeom;
    }
    else if (!bInFeature && hasFoundLat && hasFoundLon)
    {
        if (poGlobalGeom != NULL)
                delete poGlobalGeom;
        poGlobalGeom = new OGRPoint( lonVal, latVal );
        hasFoundLat = hasFoundLon = FALSE;
    }
}

/************************************************************************/
/*                          dataHandlerCbk()                            */
/************************************************************************/

void OGRGeoRSSLayer::dataHandlerCbk(const char *data, int nLen)
{
    if (bStopParsing) return;

    if (bInGMLGeometry == TRUE || bInSimpleGeometry == TRUE ||
        bInGeoLat == TRUE || bInGeoLong == TRUE ||
        pszSubElementName != NULL)
    {
        char* pszNewSubElementValue = (char*) VSIRealloc(pszSubElementValue,
                                               nSubElementValueLen + nLen + 1);
        if (pszNewSubElementValue == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
            XML_StopParser(oSchemaParser, XML_FALSE);
            bStopParsing = TRUE;
            return;
        }
        pszSubElementValue = pszNewSubElementValue;
        memcpy(pszSubElementValue + nSubElementValueLen, data, nLen);
        nSubElementValueLen += nLen;
    }
}
#endif

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGeoRSSLayer::GetNextFeature()
{
    if (bWriteMode)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot read features when writing a GeoRSS file");
        return NULL;
    }

    if (fpGeoRSS == NULL)
        return NULL;

    if (!bHasReadSchema)
        LoadSchema();

    if (bStopParsing)
        return NULL;

#ifdef HAVE_EXPAT
    if (nFeatureTabIndex < nFeatureTabLength)
    {
        return ppoFeatureTab[nFeatureTabIndex++];
    }
    
    if (VSIFEofL(fpGeoRSS))
        return NULL;
    
    char aBuf[BUFSIZ];
    
    CPLFree(ppoFeatureTab);
    ppoFeatureTab = NULL;
    nFeatureTabLength = 0;
    nFeatureTabIndex = 0;

    int nDone;
    do
    {
        unsigned int nLen =
                (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpGeoRSS );
        nDone = VSIFEofL(fpGeoRSS);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of GeoRSS file failed : %s "
                     "at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            bStopParsing = TRUE;
        }
    } while (!nDone && !bStopParsing && nFeatureTabLength == 0);
    
    return (nFeatureTabLength) ? ppoFeatureTab[nFeatureTabIndex++] : NULL;
#else
    return NULL;
#endif
}

/************************************************************************/
/*              OGRGeoRSSLayerIsStandardFieldInternal()                 */
/************************************************************************/

static int OGRGeoRSSLayerIsStandardFieldInternal(const char* pszName,
                                                 const char** papszNames)
{
    unsigned int i;
    for( i = 0; papszNames[i] != NULL; i++)
    {
        if (strcmp(pszName, papszNames[i]) == 0)
        {
            return TRUE;
        }

        const char* pszUnderscore = strchr(papszNames[i], '_');
        if (pszUnderscore == NULL)
        {
            int nLen = strlen(papszNames[i]);
            if (strncmp(pszName, papszNames[i], nLen) == 0)
            {
                int k = nLen;
                while(pszName[k] >= '0' && pszName[k] <= '9')
                    k++;
                if (pszName[k] == '\0')
                    return TRUE;
            }
        }
        else
        {
            int nLen = pszUnderscore - papszNames[i];
            if (strncmp(pszName, papszNames[i], nLen) == 0)
            {
                int k = nLen;
                while(pszName[k] >= '0' && pszName[k] <= '9')
                    k++;
                if (pszName[k] == '_' && strcmp(pszName + k, pszUnderscore) == 0)
                    return TRUE;
            }
        }
    }
    return FALSE;
}

/************************************************************************/
/*               OGRGeoRSSLayer::IsStandardField()                      */
/************************************************************************/

int OGRGeoRSSLayer::IsStandardField(const char* pszName)
{
    if (eFormat == GEORSS_RSS)
    {
        return OGRGeoRSSLayerIsStandardFieldInternal(pszName,
                apszAllowedRSSFieldNames);
    }
    else
    {
        return OGRGeoRSSLayerIsStandardFieldInternal(pszName,
                apszAllowedATOMFieldNames);
    }
}

/************************************************************************/
/*                 OGRGeoRSSLayerSplitComposedField()                   */
/************************************************************************/

static void OGRGeoRSSLayerSplitComposedField(const char* pszName,
                                             char** ppszElementName,
                                             char** ppszNumber,
                                             char** ppszAttributeName)
{
    *ppszElementName = CPLStrdup(pszName);

    int i = 0;
    while(pszName[i] != '\0' && pszName[i] != '_' &&
          !(pszName[i] >= '0' && pszName[i] <= '9'))
    {
        i++;
    }

    (*ppszElementName)[i] = '\0';

    if (pszName[i] >= '0' && pszName[i] <= '9')
    {
        *ppszNumber = CPLStrdup(pszName + i);
        char* pszUnderscore = strchr(*ppszNumber, '_');
        if (pszUnderscore)
        {
            *pszUnderscore = '\0';
            *ppszAttributeName = CPLStrdup(pszUnderscore + 1);
        }
        else
        {
            *ppszAttributeName = NULL;
        }
    }
    else
    {
        *ppszNumber = CPLStrdup("");
        if (pszName[i] == '_')
        {
            *ppszAttributeName = CPLStrdup(pszName + i + 1);
        }
        else
        {
            *ppszAttributeName = NULL;
        }
    }
}

/************************************************************************/
/*                 OGRGeoRSSLayerWriteSimpleElement()                   */
/************************************************************************/

static void OGRGeoRSSLayerWriteSimpleElement(VSILFILE* fp,
                                             const char* pszElementName,
                                             const char* pszNumber,
                                             const char** papszNames,
                                             OGRFeatureDefn* poFeatureDefn,
                                             OGRFeature* poFeature)
{
    VSIFPrintfL(fp, "      <%s", pszElementName);

    unsigned k;
    for( k = 0; papszNames[k] != NULL ; k++)
    {
        if (strncmp(papszNames[k], pszElementName, strlen(pszElementName)) == 0 &&
            papszNames[k][strlen(pszElementName)] == '_')
        {
            const char* pszAttributeName = papszNames[k] + strlen(pszElementName) + 1;
            char* pszFieldName = CPLStrdup(CPLSPrintf("%s%s_%s", pszElementName, pszNumber, pszAttributeName));
            int iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
            if (iIndex != -1 && poFeature->IsFieldSet( iIndex ))
            {
                char* pszValue =
                        OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( iIndex ));
                VSIFPrintfL(fp, " %s=\"%s\"", pszAttributeName, pszValue);
                CPLFree(pszValue);
            }
            CPLFree(pszFieldName);
        }
    }

    char* pszFieldName = CPLStrdup(CPLSPrintf("%s%s", pszElementName, pszNumber));
    int iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
    if (iIndex != -1 && poFeature->IsFieldSet( iIndex ))
    {
        VSIFPrintfL(fp, ">");

        char* pszValue =
                OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( iIndex ));
        VSIFPrintfL(fp, "%s", pszValue);
        CPLFree(pszValue);

        VSIFPrintfL(fp, "</%s>\n", pszElementName);
    }
    else
    {
        VSIFPrintfL(fp, "/>\n");
    }
    CPLFree(pszFieldName);
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRGeoRSSLayer::ICreateFeature( OGRFeature *poFeature )

{
    VSILFILE* fp = poDS->GetOutputFP();
    if (fp == NULL)
        return CE_Failure;

    nNextFID ++;

    /* Verify that compulsory feeds are set. Otherwise put some default value in them */
    if (eFormat == GEORSS_RSS)
    {
        int iFieldTitle = poFeatureDefn->GetFieldIndex( "title" );
        int iFieldDescription = poFeatureDefn->GetFieldIndex( "description" );

        VSIFPrintfL(fp, "    <item>\n");

        if ((iFieldTitle == -1 || poFeature->IsFieldSet( iFieldTitle ) == FALSE) &&
            (iFieldDescription == -1 || poFeature->IsFieldSet( iFieldDescription ) == FALSE))
        {
            VSIFPrintfL(fp, "      <title>Feature %d</title>\n", nNextFID);
        }
    }
    else
    {
        VSIFPrintfL(fp, "    <entry>\n");

        int iFieldId = poFeatureDefn->GetFieldIndex( "id" );
        int iFieldTitle = poFeatureDefn->GetFieldIndex( "title" );
        int iFieldUpdated = poFeatureDefn->GetFieldIndex( "updated" );

        if (iFieldId == -1 || poFeature->IsFieldSet( iFieldId ) == FALSE)
        {
            VSIFPrintfL(fp, "      <id>Feature %d</id>\n", nNextFID);
        }

        if (iFieldTitle == -1 || poFeature->IsFieldSet( iFieldTitle ) == FALSE)
        {
            VSIFPrintfL(fp, "      <title>Title for feature %d</title>\n", nNextFID);
        }

        if (iFieldUpdated == -1 || poFeature->IsFieldSet(iFieldUpdated ) == FALSE)
        {
            VSIFPrintfL(fp, "      <updated>2009-01-01T00:00:00Z</updated>\n");
        }
    }

    int nFieldCount = poFeatureDefn->GetFieldCount();
    int* pbUsed = (int*)CPLCalloc(sizeof(int), nFieldCount);

    for(int i = 0; i < nFieldCount; i ++)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( i );
        const char* pszName = poFieldDefn->GetNameRef();

        if ( ! poFeature->IsFieldSet( i ) )
            continue;

        char* pszElementName;
        char* pszNumber;
        char* pszAttributeName;
        OGRGeoRSSLayerSplitComposedField(pszName, &pszElementName, &pszNumber, &pszAttributeName);

        int bWillSkip = FALSE;
        /* Handle Atom entries with elements with sub-elements like */
        /* <author><name>...</name><uri>...</uri></author */
        if (eFormat == GEORSS_ATOM)
        {
            unsigned int k;
            for (k=0;apszAllowedATOMFieldNamesWithSubElements[k] != NULL;k++)
            {
                if (strcmp(pszElementName, apszAllowedATOMFieldNamesWithSubElements[k]) == 0 &&
                    pszAttributeName != NULL)
                {
                    bWillSkip = TRUE;
                    if (pbUsed[i])
                        break;

                    VSIFPrintfL(fp, "      <%s>\n", pszElementName);

                    int j;
                    for(j = i; j < nFieldCount; j ++)
                    {
                        poFieldDefn = poFeatureDefn->GetFieldDefn( j );
                        if ( ! poFeature->IsFieldSet( j ) )
                            continue;

                        char* pszElementName2;
                        char* pszNumber2;
                        char* pszAttributeName2;
                        OGRGeoRSSLayerSplitComposedField(poFieldDefn->GetNameRef(),
                                &pszElementName2, &pszNumber2, &pszAttributeName2);

                        if (strcmp(pszElementName2, pszElementName) == 0 &&
                            strcmp(pszNumber, pszNumber2) == 0 && pszAttributeName2 != NULL)
                        {
                            pbUsed[j] = TRUE;

                            char* pszValue =
                                    OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( j ));
                            VSIFPrintfL(fp, "        <%s>%s</%s>\n", pszAttributeName2, pszValue, pszAttributeName2);
                            CPLFree(pszValue);
                        }
                        CPLFree(pszElementName2);
                        CPLFree(pszNumber2);
                        CPLFree(pszAttributeName2);
                    }

                    VSIFPrintfL(fp, "      </%s>\n", pszElementName);

                    break;
                }
            }
        }

        if (bWillSkip)
        {
            /* Do nothing */
        }
        else if (eFormat == GEORSS_RSS &&
            strcmp(pszName, "pubDate") == 0)
        {
            const OGRField* psField = poFeature->GetRawFieldRef(i);
            char* pszDate = OGRGetRFC822DateTime(psField);
            VSIFPrintfL(fp, "      <%s>%s</%s>\n",
                    pszName, pszDate, pszName);
            CPLFree(pszDate);
        }
        else if (eFormat == GEORSS_ATOM &&
                 (strcmp(pszName, "updated") == 0 || strcmp(pszName, "published") == 0))
        {
            const OGRField* psField = poFeature->GetRawFieldRef(i);
            char* pszDate = OGRGetXMLDateTime(psField);
            VSIFPrintfL(fp, "      <%s>%s</%s>\n",
                    pszName, pszDate, pszName);
            CPLFree(pszDate);
        }
        else if (strcmp(pszName, "dc_date") == 0)
        {
            const OGRField* psField = poFeature->GetRawFieldRef(i);
            char* pszDate = OGRGetXMLDateTime(psField);
            VSIFPrintfL(fp, "      <%s>%s</%s>\n",
                    "dc:date", pszDate, "dc:date");
            CPLFree(pszDate);
        }
        /* RSS fields with content and attributes */
        else if (eFormat == GEORSS_RSS &&
                 (strcmp(pszElementName, "category") == 0 ||
                  strcmp(pszElementName, "guid") == 0 ||
                  strcmp(pszElementName, "source") == 0 ))
        {
            if (pszAttributeName == NULL)
            {
                OGRGeoRSSLayerWriteSimpleElement(fp, pszElementName, pszNumber,
                                       apszAllowedRSSFieldNames, poFeatureDefn, poFeature);
            }
        }
        /* RSS field with attribute only */
        else if (eFormat == GEORSS_RSS &&
                 strcmp(pszElementName, "enclosure") == 0)
        {
            if (pszAttributeName != NULL && strcmp(pszAttributeName, "url") == 0)
            {
                OGRGeoRSSLayerWriteSimpleElement(fp, pszElementName, pszNumber,
                                       apszAllowedRSSFieldNames, poFeatureDefn, poFeature);
            }
        }
        /* ATOM fields with attribute only */
        else if (eFormat == GEORSS_ATOM &&
                 (strcmp(pszElementName, "category") == 0 || strcmp(pszElementName, "link") == 0))
        {
            if (pszAttributeName != NULL &&
                ((strcmp(pszElementName, "category") == 0 && strcmp(pszAttributeName, "term") == 0) ||
                 (strcmp(pszElementName, "link") == 0 && strcmp(pszAttributeName, "href") == 0)))
            {
                OGRGeoRSSLayerWriteSimpleElement(fp, pszElementName, pszNumber,
                                       apszAllowedATOMFieldNames, poFeatureDefn, poFeature);
            }
        }
        else if (eFormat == GEORSS_ATOM &&
                 (strncmp(pszName, "content", strlen("content")) == 0 ||
                  strncmp(pszName, "summary", strlen("summary")) == 0))
        {
            char* pszFieldName;
            int iIndex;
            if (strchr(pszName, '_') == NULL)
            {
                VSIFPrintfL(fp, "      <%s", pszName);

                int bIsXHTML = FALSE;
                pszFieldName = CPLStrdup(CPLSPrintf("%s_%s", pszName, "type"));
                iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
                if (iIndex != -1 && poFeature->IsFieldSet( iIndex ))
                {
                    bIsXHTML = strcmp(poFeature->GetFieldAsString( iIndex ), "xhtml") == 0;
                    char* pszValue =
                            OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( iIndex ));
                    VSIFPrintfL(fp, " %s=\"%s\"", "type", pszValue);
                    CPLFree(pszValue);
                }
                CPLFree(pszFieldName);

                pszFieldName = CPLStrdup(CPLSPrintf("%s_%s", pszName, "xml_lang"));
                iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
                if (iIndex != -1 && poFeature->IsFieldSet( iIndex ))
                {
                    char* pszValue =
                            OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( iIndex ));
                    VSIFPrintfL(fp, " %s=\"%s\"", "xml:lang", pszValue);
                    CPLFree(pszValue);
                }
                CPLFree(pszFieldName);

                pszFieldName = CPLStrdup(CPLSPrintf("%s_%s", pszName, "xml_base"));
                iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
                if (iIndex != -1 && poFeature->IsFieldSet( iIndex ))
                {
                    char* pszValue =
                            OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( iIndex ));
                    VSIFPrintfL(fp, " %s=\"%s\"", "xml:base", pszValue);
                    CPLFree(pszValue);
                }
                CPLFree(pszFieldName);

                VSIFPrintfL(fp, ">");
                if (bIsXHTML)
                    VSIFPrintfL(fp, "%s", poFeature->GetFieldAsString(i));
                else
                {
                    char* pszValue =
                            OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( i ));
                    VSIFPrintfL(fp, "%s", pszValue);
                    CPLFree(pszValue);
                }
                VSIFPrintfL(fp, "      </%s>\n", pszName);
            }
        }
        else if (strncmp(pszName, "dc_subject", strlen("dc_subject")) == 0)
        {
            char* pszFieldName;
            int iIndex;
            if (strchr(pszName+strlen("dc_subject"), '_') == NULL)
            {
                VSIFPrintfL(fp, "      <%s", "dc:subject");

                pszFieldName = CPLStrdup(CPLSPrintf("%s_%s", pszName, "xml_lang"));
                iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
                if (iIndex != -1 && poFeature->IsFieldSet( iIndex ))
                {
                    char* pszValue =
                            OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( iIndex ));
                    VSIFPrintfL(fp, " %s=\"%s\"", "xml:lang", pszValue);
                    CPLFree(pszValue);
                }
                CPLFree(pszFieldName);

                char* pszValue =
                        OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( i ));
                VSIFPrintfL(fp, ">%s</%s>\n", pszValue, "dc:subject");
                CPLFree(pszValue);
            }
        }
        else
        {
            char* pszTagName = CPLStrdup(pszName);
            if (IsStandardField(pszName) == FALSE)
            {
                int j;
                int nCountUnderscore = 0;
                for(j=0;pszTagName[j] != 0;j++)
                {
                    if (pszTagName[j] == '_')
                    {
                        if (nCountUnderscore == 0)
                            pszTagName[j] = ':';
                        nCountUnderscore ++;
                    }
                    else if (pszTagName[j] == ' ')
                        pszTagName[j] = '_';
                }
                if (nCountUnderscore == 0)
                {
                    char* pszTemp = CPLStrdup(CPLSPrintf("ogr:%s", pszTagName));
                    CPLFree(pszTagName);
                    pszTagName = pszTemp;
                }
            }
            char* pszValue =
                        OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString( i ));
            VSIFPrintfL(fp, "      <%s>%s</%s>\n", pszTagName, pszValue, pszTagName);
            CPLFree(pszValue);
            CPLFree(pszTagName);
        }

        CPLFree(pszElementName);
        CPLFree(pszNumber);
        CPLFree(pszAttributeName);
    }

    CPLFree(pbUsed);

    OGRGeoRSSGeomDialect eGeomDialect = poDS->GetGeomDialect();
    OGRGeometry* poGeom = poFeature->GetGeometryRef();
    if ( poGeom != NULL && !poGeom->IsEmpty() )
    {
        char* pszURN = NULL;
        int bSwapCoordinates = FALSE;
        if (eGeomDialect == GEORSS_GML)
        {
            if (poSRS != NULL)
            {
                const char* pszAuthorityName = poSRS->GetAuthorityName(NULL);
                const char* pszAuthorityCode = poSRS->GetAuthorityCode(NULL);
                if (pszAuthorityName != NULL && EQUAL(pszAuthorityName, "EPSG") &&
                    pszAuthorityCode != NULL)
                {
                    if (!EQUAL(pszAuthorityCode, "4326"))
                        pszURN = CPLStrdup(CPLSPrintf("urn:ogc:def:crs:EPSG::%s", pszAuthorityCode));

                    /* In case the SRS is a geographic SRS and that we have no axis */
                    /* defintion, we assume that the order is lon/lat */
                    const char* pszAxisName = poSRS->GetAxis(NULL, 0, NULL);
                    if (poSRS->IsGeographic() &&
                        (pszAxisName == NULL || EQUALN(pszAxisName, "Lon", 3)))
                    {
                        bSwapCoordinates = TRUE;
                    }
                }
                else
                {
                    static int bOnce = FALSE;
                    if (!bOnce)
                    {
                        bOnce = TRUE;
                        CPLError(CE_Warning, CPLE_AppDefined, "Could not translate SRS into GML urn");
                    }
                }
            }
            else
            {
                bSwapCoordinates = TRUE;
            }
        }

        char szCoord[75];
        switch( wkbFlatten(poGeom->getGeometryType()) )
        {
            case wkbPoint:
            {
                OGRPoint* poPoint = (OGRPoint*)poGeom;
                double x = poPoint->getX();
                double y = poPoint->getY();
                if (eGeomDialect == GEORSS_GML)
                {
                    VSIFPrintfL(fp, "      <georss:where><gml:Point");
                    if (pszURN != NULL)
                        VSIFPrintfL(fp, " srsName=\"%s\"", pszURN);
                    if (poGeom->getCoordinateDimension() == 3)
                    {
                        OGRMakeWktCoordinate(szCoord, (bSwapCoordinates) ? y : x, (bSwapCoordinates) ? x : y,
                                             poPoint->getZ(), 3);
                        VSIFPrintfL(fp, " srsDimension=\"3\"><gml:pos>%s", szCoord);
                    }
                    else
                    {
                        OGRMakeWktCoordinate(szCoord, (bSwapCoordinates) ? y : x, (bSwapCoordinates) ? x : y,
                                             0, 2);
                        VSIFPrintfL(fp, "><gml:pos>%s", szCoord);
                    }
                    VSIFPrintfL(fp, "</gml:pos></gml:Point></georss:where>\n");
                }
                else if (eGeomDialect == GEORSS_SIMPLE)
                {
                    OGRMakeWktCoordinate(szCoord, y, x, 0, 2);
                    VSIFPrintfL(fp, "      <georss:point>%s</georss:point>\n", szCoord);
                }
                else if (eGeomDialect == GEORSS_W3C_GEO)
                {
                    OGRFormatDouble( szCoord, sizeof(szCoord), y, '.' );
                    VSIFPrintfL(fp, "      <geo:lat>%s</geo:lat>\n", szCoord);
                    OGRFormatDouble( szCoord, sizeof(szCoord), x, '.' );
                    VSIFPrintfL(fp, "      <geo:long>%s</geo:long>\n", szCoord);
                }
                break;
            }

            case wkbLineString:
            {
                OGRLineString* poLineString = (OGRLineString*)poGeom;
                if (eGeomDialect == GEORSS_GML)
                {
                    VSIFPrintfL(fp, "      <georss:where><gml:LineString");
                    if (pszURN != NULL)
                        VSIFPrintfL(fp, " srsName=\"%s\"", pszURN);
                    VSIFPrintfL(fp, "><gml:posList>\n");
                    int n = poLineString->getNumPoints();
                    for(int i=0;i<n;i++)
                    {
                        double x = poLineString->getX(i);
                        double y = poLineString->getY(i);
                        OGRMakeWktCoordinate(szCoord, (bSwapCoordinates) ? y : x, (bSwapCoordinates) ? x : y,
                                             0, 2);
                        VSIFPrintfL(fp, "%s ", szCoord);
                    }
                    VSIFPrintfL(fp, "</gml:posList></gml:LineString></georss:where>\n");
                }
                else if (eGeomDialect == GEORSS_SIMPLE)
                {
                    VSIFPrintfL(fp, "      <georss:line>\n");
                    int n = poLineString->getNumPoints();
                    for(int i=0;i<n;i++)
                    {
                        double x = poLineString->getX(i);
                        double y = poLineString->getY(i);
                        OGRMakeWktCoordinate(szCoord, y, x, 0, 2);
                        VSIFPrintfL(fp, "%s ", szCoord);
                    }
                    VSIFPrintfL(fp, "</georss:line>\n");
                }
                else
                {
                    /* Not supported */
                }
                break;
            }

            case wkbPolygon:
            {
                OGRPolygon* poPolygon = (OGRPolygon*)poGeom;
                OGRLineString* poLineString = poPolygon->getExteriorRing();
                if (poLineString == NULL)
                    break;

                if (eGeomDialect == GEORSS_GML)
                {
                    VSIFPrintfL(fp, "      <georss:where><gml:Polygon");
                    if (pszURN != NULL)
                        VSIFPrintfL(fp, " srsName=\"%s\"", pszURN);
                    VSIFPrintfL(fp, "><gml:exterior><gml:LinearRing><gml:posList>\n");
                    int n = poLineString->getNumPoints();
                    for(int i=0;i<n;i++)
                    {
                        double x = poLineString->getX(i);
                        double y = poLineString->getY(i);
                        OGRMakeWktCoordinate(szCoord, (bSwapCoordinates) ? y : x, (bSwapCoordinates) ? x : y,
                                             0, 2);
                        VSIFPrintfL(fp, "%s ", szCoord);
                    }
                    VSIFPrintfL(fp, "</gml:posList></gml:LinearRing></gml:exterior></gml:Polygon></georss:where>\n");
                }
                else if (eGeomDialect == GEORSS_SIMPLE)
                {
                    VSIFPrintfL(fp, "      <georss:polygon>\n");
                    int n = poLineString->getNumPoints();
                    for(int i=0;i<n;i++)
                    {
                        double x = poLineString->getX(i);
                        double y = poLineString->getY(i);
                        OGRMakeWktCoordinate(szCoord, y, x, 0, 2);
                        VSIFPrintfL(fp, "%s ", szCoord);
                    }
                    VSIFPrintfL(fp, "</georss:polygon>\n");
                }
                else
                {
                    /* Not supported */
                }
                break;
            }

            default:
                /* Not supported */
                break;
        }
        CPLFree(pszURN);
    }

    if (eFormat == GEORSS_RSS)
        VSIFPrintfL(fp, "    </item>\n");
    else
        VSIFPrintfL(fp, "    </entry>\n");

    return OGRERR_NONE;
}



/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGeoRSSLayer::CreateField( OGRFieldDefn *poFieldDefn,
                                    CPL_UNUSED int bApproxOK )
{
    const char* pszName = poFieldDefn->GetNameRef();
    if (((eFormat == GEORSS_RSS && strcmp(pszName, "pubDate") == 0) ||
         (eFormat == GEORSS_ATOM && (strcmp(pszName, "updated") == 0 ||
                                     strcmp(pszName, "published") == 0 )) ||
          strcmp(pszName, "dc:date") == 0) &&
        poFieldDefn->GetType() != OFTDateTime)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s", pszName);
        return OGRERR_FAILURE;
    }

    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if (strcmp(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                   pszName ) == 0)
        {
            return OGRERR_FAILURE;
        }
    }

    if (IsStandardField(pszName))
    {
        poFeatureDefn->AddFieldDefn( poFieldDefn );
        return OGRERR_NONE;
    }

    if (poDS->GetUseExtensions() == FALSE)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                "Field of name '%s' is not supported in %s schema. "
                 "Use USE_EXTENSIONS creation option to allow use of extensions.",
                 pszName, (eFormat == GEORSS_RSS) ? "RSS" : "ATOM");
        return OGRERR_FAILURE;
    }
    else
    {
        poFeatureDefn->AddFieldDefn( poFieldDefn );
        return OGRERR_NONE;
    }
}

#ifdef HAVE_EXPAT

static void XMLCALL startElementLoadSchemaCbk(void *pUserData, const char *pszName, const char **ppszAttr)
{
    ((OGRGeoRSSLayer*)pUserData)->startElementLoadSchemaCbk(pszName, ppszAttr);
}

static void XMLCALL endElementLoadSchemaCbk(void *pUserData, const char *pszName)
{
    ((OGRGeoRSSLayer*)pUserData)->endElementLoadSchemaCbk(pszName);
}

static void XMLCALL dataHandlerLoadSchemaCbk(void *pUserData, const char *data, int nLen)
{
    ((OGRGeoRSSLayer*)pUserData)->dataHandlerLoadSchemaCbk(data, nLen);
}


/************************************************************************/
/*                       LoadSchema()                         */
/************************************************************************/

/** This function parses the whole file to detect the fields */
void OGRGeoRSSLayer::LoadSchema()
{
    if (bHasReadSchema)
        return;

    bHasReadSchema = TRUE;

    if (fpGeoRSS == NULL)
        return;

    oSchemaParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oSchemaParser, ::startElementLoadSchemaCbk, ::endElementLoadSchemaCbk);
    XML_SetCharacterDataHandler(oSchemaParser, ::dataHandlerLoadSchemaCbk);
    XML_SetUserData(oSchemaParser, this);

    VSIFSeekL( fpGeoRSS, 0, SEEK_SET );

    bInFeature = FALSE;
    currentDepth = 0;
    currentFieldDefn = NULL;
    pszSubElementName = NULL;
    pszSubElementValue = NULL;
    nSubElementValueLen = 0;
    bSameSRS = TRUE;
    CPLFree(pszGMLSRSName);
    pszGMLSRSName = NULL;
    eGeomType = wkbUnknown;
    bFoundGeom = FALSE;
    bInTagWithSubTag = FALSE;
    pszTagWithSubTag = NULL;
    bStopParsing = FALSE;
    nWithoutEventCounter = 0;
    nTotalFeatureCount = 0;
    setOfFoundFields = NULL;

    char aBuf[BUFSIZ];
    int nDone;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen = (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpGeoRSS );
        nDone = VSIFEofL(fpGeoRSS);
        if (XML_Parse(oSchemaParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of GeoRSS file failed : %s at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oSchemaParser)),
                     (int)XML_GetCurrentLineNumber(oSchemaParser),
                     (int)XML_GetCurrentColumnNumber(oSchemaParser));
            bStopParsing = TRUE;
        }
        nWithoutEventCounter ++;
    } while (!nDone && !bStopParsing && nWithoutEventCounter < 10);

    XML_ParserFree(oSchemaParser);

    if (nWithoutEventCounter == 10)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = TRUE;
    }

    CPLAssert(poSRS == NULL);
    if (bSameSRS && bFoundGeom)
    {
        if (pszGMLSRSName == NULL)
        {
            poSRS = new OGRSpatialReference();
            poSRS->SetWellKnownGeogCS( "WGS84" ); /* no AXIS definition ! */
        }
        else
        {
            poSRS = new OGRSpatialReference();
            poSRS->importFromURN(pszGMLSRSName);
        }
    }

    if (eGeomType != wkbUnknown)
        poFeatureDefn->SetGeomType(eGeomType);
    if( poFeatureDefn->GetGeomFieldCount() != 0 )
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);

    if (setOfFoundFields)
        CPLHashSetDestroy(setOfFoundFields);
    setOfFoundFields = NULL;
    CPLFree(pszGMLSRSName);
    pszGMLSRSName = NULL;
    CPLFree(pszTagWithSubTag);
    pszTagWithSubTag = NULL;

    VSIFSeekL( fpGeoRSS, 0, SEEK_SET );
}


/************************************************************************/
/*                         OGRGeoRSSIsInt()                             */
/************************************************************************/

static int OGRGeoRSSIsInt(const char* pszStr)
{
    int i;

    while(*pszStr == ' ')
        pszStr++;

    for(i=0;pszStr[i];i++)
    {
        if (pszStr[i] == '+' || pszStr[i] == '-')
        {
            if (i != 0)
                return FALSE;
        }
        else if (!(pszStr[i] >= '0' && pszStr[i] <= '9'))
            return FALSE;
    }
    return TRUE;
}

/************************************************************************/
/*                  startElementLoadSchemaCbk()                         */
/************************************************************************/

void OGRGeoRSSLayer::startElementLoadSchemaCbk(const char *pszName, const char **ppszAttr)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;
    const char* pszNoNSName = pszName;
    const char* pszColon = strchr(pszNoNSName, ':');
    if( pszColon )
        pszNoNSName = pszColon + 1;

    if ((eFormat == GEORSS_ATOM && currentDepth == 1 && strcmp(pszNoNSName, "entry") == 0) ||
        ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) && !bInFeature &&
        (currentDepth == 1 || currentDepth == 2) && strcmp(pszNoNSName, "item") == 0))
    {
        bInFeature = TRUE;
        featureDepth = currentDepth;

        nTotalFeatureCount ++;

        if (setOfFoundFields)
            CPLHashSetDestroy(setOfFoundFields);
        setOfFoundFields = CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);
    }
    else if (bInTagWithSubTag && currentDepth == 3)
    {
        char* pszFieldName = CPLStrdup(CPLSPrintf("%s_%s", pszTagWithSubTag, pszNoNSName));
        if (poFeatureDefn->GetFieldIndex(pszFieldName) == -1)
        {
            OGRFieldDefn newFieldDefn(pszFieldName, OFTString);
            poFeatureDefn->AddFieldDefn(&newFieldDefn);

            if (poFeatureDefn->GetFieldCount() == 100)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Too many fields. File probably corrupted");
                XML_StopParser(oSchemaParser, XML_FALSE);
                bStopParsing = TRUE;
            }
        }
        CPLFree(pszFieldName);
    }
    else if (bInFeature && eFormat == GEORSS_ATOM &&
             currentDepth == 2 && OGRGeoRSSLayerATOMTagHasSubElement(pszNoNSName))
    {
        CPLFree(pszTagWithSubTag);
        pszTagWithSubTag = CPLStrdup(pszNoNSName);

        int count = 1;
        while(CPLHashSetLookup(setOfFoundFields, pszTagWithSubTag) != NULL)
        {
            count ++;
            CPLFree(pszTagWithSubTag);
            pszTagWithSubTag = CPLStrdup(CPLSPrintf("%s%d", pszNoNSName, count));
            if (pszTagWithSubTag[0] == 0)
            {
                XML_StopParser(oSchemaParser, XML_FALSE);
                bStopParsing = TRUE;
                break;
            }
        }
        CPLHashSetInsert(setOfFoundFields, CPLStrdup(pszTagWithSubTag));

        bInTagWithSubTag = TRUE;
    }
    else if (bInFeature && currentDepth == featureDepth + 1 && !IS_GEO_ELEMENT(pszName))
    {
        if( pszName != pszNoNSName && strncmp(pszName, "atom:", 5) == 0 )
            pszName = pszNoNSName;

        CPLFree(pszSubElementName);
        pszSubElementName = CPLStrdup(pszName);

        int count = 1;
        while(CPLHashSetLookup(setOfFoundFields, pszSubElementName) != NULL)
        {
            count ++;
            CPLFree(pszSubElementName);
            pszSubElementName = CPLStrdup(CPLSPrintf("%s%d", pszName, count));
        }
        CPLHashSetInsert(setOfFoundFields, CPLStrdup(pszSubElementName));

        /* Create field definition for element */
        char* pszCompatibleName = OGRGeoRSS_GetOGRCompatibleTagName(pszSubElementName);
        int iField = poFeatureDefn->GetFieldIndex(pszCompatibleName);
        if (iField >= 0)
        {
            currentFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        }
        else if ( ! ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) && strcmp(pszNoNSName, "enclosure") == 0) &&
                  ! (eFormat == GEORSS_ATOM && strcmp(pszNoNSName, "link") == 0) &&
                  ! (eFormat == GEORSS_ATOM && strcmp(pszNoNSName, "category") == 0))
        {
            OGRFieldType eFieldType;
            if (((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) && strcmp(pszNoNSName, "pubDate") == 0) ||
                (eFormat == GEORSS_ATOM && strcmp(pszNoNSName, "updated") == 0) ||
                (eFormat == GEORSS_ATOM && strcmp(pszNoNSName, "published") == 0) ||
                strcmp(pszName, "dc:date") == 0)
                eFieldType = OFTDateTime;
            else
                eFieldType = OFTInteger;

            OGRFieldDefn newFieldDefn(pszCompatibleName, eFieldType);
            poFeatureDefn->AddFieldDefn(&newFieldDefn);
            currentFieldDefn = poFeatureDefn->GetFieldDefn(poFeatureDefn->GetFieldCount() - 1);

            if (poFeatureDefn->GetFieldCount() == 100)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                        "Too many fields. File probably corrupted");
                XML_StopParser(oSchemaParser, XML_FALSE);
                bStopParsing = TRUE;
            }
        }

        /* Create field definitions for attributes */
        for(int i=0; ppszAttr[i] != NULL && ppszAttr[i+1] != NULL && !bStopParsing; i+= 2)
        {
            char* pszAttrCompatibleName =
                    OGRGeoRSS_GetOGRCompatibleTagName(CPLSPrintf("%s_%s", pszSubElementName, ppszAttr[i]));
            iField = poFeatureDefn->GetFieldIndex(pszAttrCompatibleName);
            OGRFieldDefn* currentAttrFieldDefn;
            if (iField >= 0)
            {
                currentAttrFieldDefn = poFeatureDefn->GetFieldDefn(iField);
            }
            else
            {
                OGRFieldDefn newFieldDefn(pszAttrCompatibleName, OFTInteger);
                poFeatureDefn->AddFieldDefn(&newFieldDefn);
                currentAttrFieldDefn = poFeatureDefn->GetFieldDefn(poFeatureDefn->GetFieldCount() - 1);

                if (poFeatureDefn->GetFieldCount() == 100)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                            "Too many fields. File probably corrupted");
                    XML_StopParser(oSchemaParser, XML_FALSE);
                    bStopParsing = TRUE;
                }
            }
            if (currentAttrFieldDefn->GetType() == OFTInteger ||
                currentAttrFieldDefn->GetType() == OFTReal)
            {
                char* pszRemainingStr = NULL;
                CPLStrtod(ppszAttr[i + 1], &pszRemainingStr);
                if (pszRemainingStr == NULL ||
                    *pszRemainingStr == 0 ||
                    *pszRemainingStr == ' ')
                {
                    if (currentAttrFieldDefn->GetType() == OFTInteger)
                    {
                        if (OGRGeoRSSIsInt(ppszAttr[i + 1]) == FALSE)
                        {
                            currentAttrFieldDefn->SetType(OFTReal);
                        }
                    }
                }
                else
                {
                    currentAttrFieldDefn->SetType(OFTString);
                }
            }
            CPLFree(pszAttrCompatibleName);
        }

        CPLFree(pszCompatibleName);
    }
    else if (strcmp(pszName, "georss:point") == 0 ||
             strcmp(pszName, "georss:line") == 0 ||
             strcmp(pszName, "geo:line") == 0 ||
             IS_LAT_ELEMENT(pszName) ||
             strcmp(pszName, "georss:polygon") == 0 ||
             strcmp(pszName, "georss:box") == 0)
    {
        if (bSameSRS)
        {
            if (pszGMLSRSName != NULL)
                bSameSRS = FALSE;
        }
    }
    else if (strcmp(pszName, "gml:Point") == 0 ||
             strcmp(pszName, "gml:LineString") == 0 ||
             strcmp(pszName, "gml:Polygon") == 0 ||
             strcmp(pszName, "gml:MultiPoint") == 0 ||
             strcmp(pszName, "gml:MultiLineString") == 0 ||
             strcmp(pszName, "gml:MultiPolygon") == 0 ||
             strcmp(pszName, "gml:Envelope") == 0)
    {
        if (bSameSRS)
        {
            int bFoundSRS = FALSE;
            for(int i = 0; ppszAttr[i] != NULL; i+=2)
            {
                if (strcmp(ppszAttr[i], "srsName") == 0)
                {
                    bFoundSRS = TRUE;
                    if (pszGMLSRSName != NULL)
                    {
                        if (strcmp(pszGMLSRSName , ppszAttr[i+1]) != 0)
                            bSameSRS = FALSE;
                    }
                    else
                        pszGMLSRSName = CPLStrdup(ppszAttr[i+1]);
                    break;
                }
            }
            if (!bFoundSRS && pszGMLSRSName != NULL)
                bSameSRS = FALSE;
        }
    }

    if (!bInFeature || currentDepth >= featureDepth + 1)
    {
        int nDimension = 2;
        for(int i = 0; ppszAttr[i] != NULL; i+=2)
        {
            if (strcmp(ppszAttr[i], "srsDimension") == 0)
            {
                nDimension = atoi(ppszAttr[i+1]);
                break;
            }
        }

        OGRwkbGeometryType eFoundGeomType = wkbUnknown;
        if (strcmp(pszName, "georss:point") == 0 ||
            IS_LAT_ELEMENT(pszName) ||
            strcmp(pszName, "gml:Point") == 0)
        {
            eFoundGeomType = wkbPoint;
        }
        else if (strcmp(pszName, "gml:MultiPoint") == 0)
        {
            eFoundGeomType = wkbMultiPoint;
        }
        else if (strcmp(pszName, "georss:line") == 0 ||
                strcmp(pszName, "geo:line") == 0 ||
                strcmp(pszName, "gml:LineString") == 0)
        {
            eFoundGeomType = wkbLineString;
        }
        else if (strcmp(pszName, "gml:MultiLineString") == 0)
        {
            eFoundGeomType = wkbMultiLineString;
        }
        else if (strcmp(pszName, "georss:polygon") == 0 ||
                 strcmp(pszName, "gml:Polygon") == 0 ||
                 strcmp(pszName, "gml:Envelope") == 0 ||
                 strcmp(pszName, "georss:box") == 0)
        {
            eFoundGeomType = wkbPolygon;
        }
        else if (strcmp(pszName, "gml:MultiPolygon") == 0)
        {
            eFoundGeomType = wkbMultiPolygon;
        }

        if (eFoundGeomType != wkbUnknown)
        {
            if (!bFoundGeom)
            {
                eGeomType = eFoundGeomType;
                bFoundGeom = TRUE;
            }
            else if (wkbFlatten(eGeomType) != eFoundGeomType)
                eGeomType = wkbUnknown;

            if (nDimension == 3)
                eGeomType = wkbSetZ(eGeomType);
        }
    }

    currentDepth++;
}

/************************************************************************/
/*                   endElementLoadSchemaCbk()                          */
/************************************************************************/

void OGRGeoRSSLayer::endElementLoadSchemaCbk(const char *pszName)
{
    if (bStopParsing) return;

    nWithoutEventCounter = 0;

    currentDepth--;

    if (!bInFeature)
        return;

    const char* pszNoNSName = pszName;
    const char* pszColon = strchr(pszNoNSName, ':');
    if( pszColon )
        pszNoNSName = pszColon + 1;

    if ((eFormat == GEORSS_ATOM && currentDepth == 1 && strcmp(pszNoNSName, "entry") == 0) ||
        ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) &&
        (currentDepth == 1 || currentDepth == 2) && strcmp(pszNoNSName, "item") == 0))
    {
        bInFeature = FALSE;
    }
    else if (bInFeature && eFormat == GEORSS_ATOM &&
                currentDepth == 2 && OGRGeoRSSLayerATOMTagHasSubElement(pszNoNSName))
    {
        bInTagWithSubTag = FALSE;
    }
    else if (currentDepth == featureDepth + 1 && pszSubElementName)
    {
        /* Patch field type */
        if (pszSubElementValue && nSubElementValueLen && currentFieldDefn)
        {
            pszSubElementValue[nSubElementValueLen] = 0;
            if (currentFieldDefn->GetType() == OFTInteger ||
                currentFieldDefn->GetType() == OFTReal)
            {
                char* pszRemainingStr = NULL;
                CPLStrtod(pszSubElementValue, &pszRemainingStr);
                if (pszRemainingStr == NULL ||
                    *pszRemainingStr == 0 ||
                    *pszRemainingStr == ' ')
                {
                    if (currentFieldDefn->GetType() == OFTInteger)
                    {
                        if (OGRGeoRSSIsInt(pszSubElementValue) == FALSE)
                        {
                            currentFieldDefn->SetType(OFTReal);
                        }
                    }
                }
                else
                {
                    currentFieldDefn->SetType(OFTString);
                }
            }
        }

        CPLFree(pszSubElementName);
        pszSubElementName = NULL;
        CPLFree(pszSubElementValue);
        pszSubElementValue = NULL;
        nSubElementValueLen = 0;
        currentFieldDefn = NULL;
    }
}

/************************************************************************/
/*                   dataHandlerLoadSchemaCbk()                         */
/************************************************************************/

void OGRGeoRSSLayer::dataHandlerLoadSchemaCbk(const char *data, int nLen)
{
    if (bStopParsing) return;

    nDataHandlerCounter ++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "File probably corrupted (million laugh pattern)");
        XML_StopParser(oSchemaParser, XML_FALSE);
        bStopParsing = TRUE;
        return;
    }

    nWithoutEventCounter = 0;

    if (pszSubElementName)
    {
        char* pszNewSubElementValue = (char*) VSIRealloc(pszSubElementValue, nSubElementValueLen + nLen + 1);
        if (pszNewSubElementValue == NULL)
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
            XML_StopParser(oSchemaParser, XML_FALSE);
            bStopParsing = TRUE;
            return;
        }
        pszSubElementValue = pszNewSubElementValue;
        memcpy(pszSubElementValue + nSubElementValueLen, data, nLen);
        nSubElementValueLen += nLen;
        if (nSubElementValueLen > 100000)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too much data inside one element. File probably corrupted");
            XML_StopParser(oSchemaParser, XML_FALSE);
            bStopParsing = TRUE;
        }
    }
}
#else
void OGRGeoRSSLayer::LoadSchema()
{
}
#endif

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGeoRSSLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCFastFeatureCount) )
        return !bWriteMode && bHasReadSchema &&
                m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return bWriteMode;
    else if( EQUAL(pszCap,OLCCreateField) )
        return bWriteMode;
    else 
        return FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRGeoRSSLayer::GetFeatureCount( int bForce )

{
    if (bWriteMode)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot read features when writing a GeoRSS file");
        return 0;
    }

    if (!bHasReadSchema)
        LoadSchema();

    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
        return nTotalFeatureCount;
}
