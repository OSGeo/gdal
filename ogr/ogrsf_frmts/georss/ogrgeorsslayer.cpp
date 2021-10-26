/******************************************************************************
 *
 * Project:  GeoRSS Translator
 * Purpose:  Implements OGRGeoRSSLayer class.
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_port.h"
#include "ogr_georss.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_hash_set.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#ifdef HAVE_EXPAT
#  include "expat.h"
#endif
#include "ogr_api.h"
#include "ogr_core.h"
#include "ogr_expat.h"
#include "ogr_feature.h"
#include "ogr_geometry.h"
#include "ogr_p.h"
#include "ogr_spatialref.h"
#include "ogrsf_frmts.h"

CPL_CVSID("$Id$")

static const char* const apszAllowedATOMFieldNamesWithSubElements[] =
    { "author", "contributor", nullptr };

static
const char* const apszAllowedRSSFieldNames[] =
    {  "title", "link", "description", "author",
       "category", "category_domain",
       "comments",
       "enclosure_url", "enclosure_length", "enclosure_type",
       "guid", "guid_isPermaLink",
       "pubDate",
       "source", "source_url", nullptr};

static
const char* const apszAllowedATOMFieldNames[] =
    { "category_term", "category_scheme", "category_label",
      "content", "content_type", "content_xml_lang", "content_xml_base",
      "summary", "summary_type", "summary_xml_lang", "summary_xml_base",
      "author_name", "author_uri", "author_email",
      "contributor_name", "contributor_uri", "contributor_email",
      "link_href", "link_rel", "link_type", "link_length",
      "id", "published", "rights", "source",
      "title", "updated", nullptr };

#define IS_LAT_ELEMENT(pszName) (STARTS_WITH(pszName, "geo:lat") || \
                                 STARTS_WITH(pszName, "icbm:lat") || \
                                 STARTS_WITH(pszName, "geourl:lat"))

#define IS_LON_ELEMENT(pszName) (STARTS_WITH(pszName, "geo:lon") || \
                                 STARTS_WITH(pszName, "icbm:lon") || \
                                 STARTS_WITH(pszName, "geourl:lon"))

#define IS_GEO_ELEMENT(pszName) (strcmp(pszName, "georss:point") == 0 || \
                                 strcmp(pszName, "georss:line") == 0 || \
                                 strcmp(pszName, "georss:box") == 0 || \
                                 strcmp(pszName, "georss:polygon") == 0 || \
                                 strcmp(pszName, "georss:where") == 0 || \
                                 STARTS_WITH(pszName, "gml:") || \
                                 STARTS_WITH(pszName, "geo:") || \
                                 STARTS_WITH(pszName, "icbm:") || \
                                 STARTS_WITH(pszName, "geourl:"))

/************************************************************************/
/*                            OGRGeoRSSLayer()                          */
/************************************************************************/

OGRGeoRSSLayer::OGRGeoRSSLayer( const char* pszFilename,
                                const char* pszLayerName,
                                OGRGeoRSSDataSource* poDS_,
                                OGRSpatialReference *poSRSIn,
                                bool bWriteMode_) :
    poFeatureDefn(new OGRFeatureDefn( pszLayerName )),
    poSRS(poSRSIn),
    poDS(poDS_),
    eFormat(poDS_->GetFormat()),
    bWriteMode(bWriteMode_),
    nTotalFeatureCount(0),
    eof(false),
    nNextFID(0),
    fpGeoRSS(nullptr),
    bHasReadSchema(false),
#ifdef HAVE_EXPAT
    oParser(nullptr),
    oSchemaParser(nullptr),
#endif
    poGlobalGeom(nullptr),
    bStopParsing(false),
    bInFeature(false),
    hasFoundLat(false),
    hasFoundLon(false),
#ifdef HAVE_EXPAT
    latVal(0.0),
    lonVal(0.0),
#endif
    pszSubElementName(nullptr),
    pszSubElementValue(nullptr),
    nSubElementValueLen(0),
#ifdef HAVE_EXPAT
    iCurrentField(0),
#endif
    bInSimpleGeometry(false),
    bInGMLGeometry(false),
    bInGeoLat(false),
    bInGeoLong(false),
#ifdef HAVE_EXPAT
    bFoundGeom(false),
    bSameSRS(false),
#endif
    eGeomType(wkbUnknown),
    pszGMLSRSName(nullptr),
    bInTagWithSubTag(false),
    pszTagWithSubTag(nullptr),
    currentDepth(0),
    featureDepth(0),
    geometryDepth(0),
#ifdef HAVE_EXPAT
    currentFieldDefn(nullptr),
    nWithoutEventCounter(0),
    nDataHandlerCounter(0),
#endif
    setOfFoundFields(nullptr),
    poFeature(nullptr),
    ppoFeatureTab(nullptr),
    nFeatureTabLength(0),
    nFeatureTabIndex(0)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    if( poSRS )
    {
        poSRS->Reference();
        poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    }

    if( !bWriteMode )
    {
        fpGeoRSS = VSIFOpenL( pszFilename, "r" );
        if( fpGeoRSS == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot open %s", pszFilename);
            return;
        }
    }

    OGRGeoRSSLayer::ResetReading();
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

    if(poSRS)
        poSRS->Release();

    CPLFree(pszSubElementName);
    CPLFree(pszSubElementValue);
    CPLFree(pszGMLSRSName);
    CPLFree(pszTagWithSubTag);
    if( setOfFoundFields )
        CPLHashSetDestroy(setOfFoundFields);
    if( poGlobalGeom )
        delete poGlobalGeom;

    for( int i = nFeatureTabIndex; i < nFeatureTabLength; i++ )
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
    if( !bHasReadSchema )
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

    eof = false;
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
    bInFeature = false;
    hasFoundLat = false;
    hasFoundLon = false;
    bInSimpleGeometry = false;
    bInGMLGeometry = false;
    bInGeoLat = false;
    bInGeoLong = false;
    eGeomType = wkbUnknown;
    CPLFree(pszSubElementName);
    pszSubElementName = nullptr;
    CPLFree(pszSubElementValue);
    pszSubElementValue = nullptr;
    nSubElementValueLen = 0;
    CPLFree(pszGMLSRSName);
    pszGMLSRSName = nullptr;

    if (setOfFoundFields)
        CPLHashSetDestroy(setOfFoundFields);
    setOfFoundFields = nullptr;

    for( int i=nFeatureTabIndex; i < nFeatureTabLength; i++ )
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    ppoFeatureTab = nullptr;
    if (poFeature)
        delete poFeature;
    poFeature = nullptr;

    currentDepth = 0;
    featureDepth = 0;
    geometryDepth = 0;
    bInTagWithSubTag = false;
    CPLFree(pszTagWithSubTag);
    pszTagWithSubTag = nullptr;
}

#ifdef HAVE_EXPAT

/************************************************************************/
/*                      AddStrToSubElementValue()                       */
/************************************************************************/

void OGRGeoRSSLayer::AddStrToSubElementValue(const char* pszStr)
{
    int len = static_cast<int>(strlen(pszStr));
    char* pszNewSubElementValue = static_cast<char *>(
            VSI_REALLOC_VERBOSE(pszSubElementValue,
                                nSubElementValueLen + len + 1));
    if (pszNewSubElementValue == nullptr)
    {
        XML_StopParser(oParser, XML_FALSE);
        bStopParsing = true;
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
    for( int i=0; pszModName[i] != 0; i++ )
    {
        if (pszModName[i] == ':')
            pszModName[i] = '_';
    }
    return pszModName;
}

/************************************************************************/
/*               OGRGeoRSSLayerATOMTagHasSubElement()                   */
/************************************************************************/

static bool OGRGeoRSSLayerATOMTagHasSubElement( const char* pszName )
{
    for( unsigned int i = 0;
         apszAllowedATOMFieldNamesWithSubElements[i] != nullptr;
         i++ )
    {
        if (strcmp(pszName, apszAllowedATOMFieldNamesWithSubElements[i]) == 0)
            return true;
    }
    return false;
}

/************************************************************************/
/*                        startElementCbk()                            */
/************************************************************************/

void OGRGeoRSSLayer::startElementCbk(const char *pszName, const char **ppszAttr)
{
    bool bSerializeTag = false;
    const char* pszNoNSName = pszName;
    const char* pszColon = strchr(pszNoNSName, ':');
    if( pszColon )
        pszNoNSName = pszColon + 1;

    if( bStopParsing ) return;

    if( (eFormat == GEORSS_ATOM && currentDepth == 1 &&
         strcmp(pszNoNSName, "entry") == 0) ||
        ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) && !bInFeature &&
        (currentDepth == 1 || currentDepth == 2) &&
         strcmp(pszNoNSName, "item") == 0) )
    {
        featureDepth = currentDepth;

        if (poFeature)
            delete poFeature;

        poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFID(nNextFID++);

        bInFeature = true;
        hasFoundLat = false;
        hasFoundLon = false;
        bInSimpleGeometry = false;
        bInGMLGeometry = false;
        bInGeoLat = false;
        bInGeoLong = false;
        eGeomType = wkbUnknown;
        geometryDepth = 0;
        bInTagWithSubTag = false;

        if (setOfFoundFields)
            CPLHashSetDestroy(setOfFoundFields);
        setOfFoundFields =
            CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr, CPLFree);
    }
    else if( bInFeature && bInTagWithSubTag && currentDepth == 3 )
    {
        char* pszFieldName =
            CPLStrdup(CPLSPrintf("%s_%s", pszTagWithSubTag, pszNoNSName));

        CPLFree(pszSubElementName);
        pszSubElementName = nullptr;
        CPLFree(pszSubElementValue);
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;

        iCurrentField = poFeatureDefn->GetFieldIndex(pszFieldName);
        if (iCurrentField >= 0)
            pszSubElementName = CPLStrdup(pszFieldName);

        CPLFree(pszFieldName);
    }
    else if( bInFeature &&
             eFormat == GEORSS_ATOM &&
             currentDepth == 2 &&
             OGRGeoRSSLayerATOMTagHasSubElement(pszNoNSName) )
    {
        CPLFree(pszTagWithSubTag);
        pszTagWithSubTag = CPLStrdup(pszNoNSName);

        int count = 1;
        while(CPLHashSetLookup(setOfFoundFields, pszTagWithSubTag) != nullptr)
        {
            count ++;
            CPLFree(pszTagWithSubTag);
            pszTagWithSubTag =
                CPLStrdup(CPLSPrintf("%s%d", pszNoNSName, count));
        }
        CPLHashSetInsert(setOfFoundFields, CPLStrdup(pszTagWithSubTag));

        bInTagWithSubTag = true;
    }
    else if( bInGMLGeometry )
    {
        bSerializeTag = true;
    }
    else if( bInSimpleGeometry || bInGeoLat || bInGeoLong )
    {
        /* Should not happen for a valid document. */
    }
    else if (IS_LAT_ELEMENT(pszName))
    {
        CPLFree(pszSubElementValue);
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;
        bInGeoLat = true;
    }
    else if (IS_LON_ELEMENT(pszName))
    {
        CPLFree(pszSubElementValue);
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;
        bInGeoLong = true;
    }
    else if (strcmp(pszName, "georss:point") == 0 ||
             strcmp(pszName, "georss:line") == 0 ||
             strcmp(pszName, "geo:line") == 0 ||
             strcmp(pszName, "georss:polygon") == 0 ||
             strcmp(pszName, "georss:box") == 0)
    {
        CPLFree(pszSubElementValue);
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;
        eGeomType = strcmp(pszName, "georss:point") == 0 ?   wkbPoint :
                      (strcmp(pszName, "georss:line") == 0 ||
                       strcmp(pszName, "geo:line") == 0)  ?  wkbLineString :
                      (strcmp(pszName, "georss:polygon") == 0  ||
                       strcmp(pszName, "georss:box") == 0) ? wkbPolygon :
                                                             wkbUnknown;
        bInSimpleGeometry = true;
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
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;
        AddStrToSubElementValue(CPLSPrintf("<%s>", pszName));
        bInGMLGeometry = true;
        geometryDepth = currentDepth;
        CPLFree(pszGMLSRSName);
        pszGMLSRSName = nullptr;
        for (int i = 0; ppszAttr[i]; i += 2)
        {
            if (strcmp(ppszAttr[i], "srsName") == 0)
            {
                if (pszGMLSRSName == nullptr)
                    pszGMLSRSName = CPLStrdup(ppszAttr[i+1]);
            }
        }
    }
    else if( bInFeature && currentDepth == featureDepth + 1 )
    {
        CPLFree(pszSubElementName);
        pszSubElementName = nullptr;
        CPLFree(pszSubElementValue);
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;
        iCurrentField = -1;

        if( pszName != pszNoNSName && STARTS_WITH(pszName, "atom:") )
            pszName = pszNoNSName;

        pszSubElementName = CPLStrdup(pszName);
        int count = 1;
        while(CPLHashSetLookup(setOfFoundFields, pszSubElementName) != nullptr)
        {
            count ++;
            if( count == 100 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too many repeated fields");
                CPLFree(pszSubElementName);
                pszSubElementName = nullptr;
                break;
            }
            CPLFree(pszSubElementName);
            pszSubElementName = CPLStrdup(CPLSPrintf("%s%d", pszName, count));
        }
        if( pszSubElementName )
        {
            CPLHashSetInsert(setOfFoundFields, CPLStrdup(pszSubElementName));

            char* pszCompatibleName =
                OGRGeoRSS_GetOGRCompatibleTagName(pszSubElementName);
            iCurrentField = poFeatureDefn->GetFieldIndex(pszCompatibleName);
            CPLFree(pszSubElementName);

            for( int i = 0; ppszAttr[i] != nullptr && ppszAttr[i+1] != nullptr;
                i += 2 )
            {
                char* pszAttrCompatibleName =
                        OGRGeoRSS_GetOGRCompatibleTagName(
                            CPLSPrintf("%s_%s", pszCompatibleName, ppszAttr[i]));
                const int iAttrField =
                    poFeatureDefn->GetFieldIndex(pszAttrCompatibleName);
                if (iAttrField >= 0)
                {
                    if( poFeatureDefn->GetFieldDefn(iAttrField)->GetType() ==
                            OFTReal)
                        poFeature->SetField(iAttrField, CPLAtof(ppszAttr[i+1]));
                    else
                        poFeature->SetField(iAttrField, ppszAttr[i+1]);
                }            CPLFree(pszAttrCompatibleName);
            }

            if (iCurrentField < 0)
            {
                pszSubElementName = nullptr;
            }
            else
            {
                pszSubElementName = CPLStrdup(pszCompatibleName);
            }
            CPLFree(pszCompatibleName);
        }
    }
    else if( bInFeature &&
             currentDepth > featureDepth + 1 &&
             pszSubElementName != nullptr )
    {
        bSerializeTag = true;
    }

    if( bSerializeTag )
    {
        AddStrToSubElementValue("<");
        AddStrToSubElementValue(pszName);
        for( int i = 0; ppszAttr[i] != nullptr && ppszAttr[i+1] != nullptr;
             i += 2 )
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

static void OGRGeoRSSLayerTrimLeadingAndTrailingSpaces( char* pszStr )
{
    // Trim leading spaces, tabs and newlines.
    int i = 0;
    while( pszStr[i] != '\0' &&
           (pszStr[i] == ' ' || pszStr[i] == '\t' || pszStr[i] == '\n') )
        i++;
    memmove(pszStr, pszStr + i, strlen(pszStr + i) + 1);

    // Trim trailing spaces, tabs and newlines.
    i = static_cast<int>(strlen(pszStr)) - 1;
    while( i >= 0 &&
           (pszStr[i] == ' ' || pszStr[i] == '\t' || pszStr[i] == '\n') )
    {
        pszStr[i] = '\0';
        i--;
    }
}

/************************************************************************/
/*                           endElementCbk()                            */
/************************************************************************/

void OGRGeoRSSLayer::endElementCbk(const char *pszName)
{
    OGRGeometry* poGeom = nullptr;

    if( bStopParsing ) return;

    currentDepth--;
    const char* pszNoNSName = pszName;
    const char* pszColon = strchr(pszNoNSName, ':');
    if( pszColon )
        pszNoNSName = pszColon + 1;

    if( bInFeature && currentDepth == featureDepth )
    {
        bInFeature = false;
        bInTagWithSubTag = false;

        if( hasFoundLat && hasFoundLon )
            poFeature->SetGeometryDirectly( new OGRPoint( lonVal, latVal ) );
        else if( poFeature->GetGeometryRef() == nullptr &&
                 poGlobalGeom != nullptr )
            poFeature->SetGeometry(poGlobalGeom);

        hasFoundLat = false;
        hasFoundLon = false;

        if( poSRS != nullptr && poFeature->GetGeometryRef() != nullptr )
            poFeature->GetGeometryRef()->assignSpatialReference(poSRS);

        if( (m_poFilterGeom == nullptr
                || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == nullptr
                || m_poAttrQuery->Evaluate( poFeature )) )
        {
            ppoFeatureTab = static_cast<OGRFeature **>(
                    CPLRealloc(ppoFeatureTab,
                               sizeof(OGRFeature*) * (nFeatureTabLength + 1)));
            ppoFeatureTab[nFeatureTabLength] = poFeature;
            nFeatureTabLength++;
        }
        else
        {
            delete poFeature;
        }
        poFeature = nullptr;
        return;
    }

    if( bInTagWithSubTag && currentDepth == 3 )
    {
        char* pszFieldName =
            CPLStrdup(CPLSPrintf("%s_%s", pszTagWithSubTag, pszNoNSName));

        if (iCurrentField != -1 && pszSubElementName &&
            strcmp(pszFieldName, pszSubElementName) == 0 && poFeature &&
            pszSubElementValue && nSubElementValueLen)
        {
            pszSubElementValue[nSubElementValueLen] = 0;
            if (poFeatureDefn->GetFieldDefn(iCurrentField)->GetType() ==
                    OFTReal)
                poFeature->SetField(iCurrentField, CPLAtof(pszSubElementValue));
            else
                poFeature->SetField( iCurrentField, pszSubElementValue);
        }

        CPLFree(pszSubElementName);
        pszSubElementName = nullptr;
        CPLFree(pszSubElementValue);
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;

        CPLFree(pszFieldName);
    }
    else if( bInFeature &&
             eFormat == GEORSS_ATOM &&
             currentDepth == 2 &&
             OGRGeoRSSLayerATOMTagHasSubElement(pszNoNSName) )
    {
        bInTagWithSubTag = false;
    }
    else if( bInGMLGeometry )
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
            CPLAssert(STARTS_WITH(pszName, "gml:"));
            poGeom = (OGRGeometry*) OGR_G_CreateFromGML(pszSubElementValue);

            if (poGeom != nullptr && !poGeom->IsEmpty() )
            {
                bool bSwapCoordinates = false;
                if (pszGMLSRSName)
                {
                    OGRSpatialReference* poSRSFeature =
                        new OGRSpatialReference();
                    poSRSFeature->importFromURN(pszGMLSRSName);
                    poGeom->assignSpatialReference(poSRSFeature);
                    poSRSFeature->Release();
                }
                else
                {
                    bSwapCoordinates = true; /* lat, lon WGS 84 */
                }

                if( bSwapCoordinates )
                {
                    poGeom->swapXY();
                }
            }
            bInGMLGeometry = false;
        }
    }
    else if( bInSimpleGeometry )
    {
        if (currentDepth > geometryDepth)
        {
            // Should not happen for a valid document.
        }
        else
        {
            if (pszSubElementValue)
            {
                pszSubElementValue[nSubElementValueLen] = 0;

                // Trim any leading and trailing spaces, tabs, newlines, etc.
                OGRGeoRSSLayerTrimLeadingAndTrailingSpaces(pszSubElementValue);

                // Caution: Order is latitude, longitude.
                char** papszTokens =
                     CSLTokenizeStringComplex(pszSubElementValue,
                                              " ,", TRUE, FALSE);

                const int nTokens = CSLCount(papszTokens);
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
                    OGRLineString* poLineString = new OGRLineString();
                    poGeom = poLineString;
                    for( int i = 0; i < nTokens; i += 2 )
                    {
                        poLineString->addPoint(CPLAtof(papszTokens[i+1]),
                                               CPLAtof(papszTokens[i]));
                    }
                }
                else if (eGeomType == wkbPolygon)
                {
                    OGRPolygon* poPolygon = new OGRPolygon();
                    OGRLinearRing* poLinearRing = new OGRLinearRing();
                    poGeom = poPolygon;
                    poPolygon->addRingDirectly(poLinearRing);
                    if( strcmp(pszName, "georss:polygon") == 0 )
                    {
                        for( int i=0; i < nTokens; i += 2 )
                        {
                            poLinearRing->addPoint(CPLAtof(papszTokens[i+1]),
                                                   CPLAtof(papszTokens[i]));
                        }
                    }
                    else
                    {
                        const double lat1 = CPLAtof(papszTokens[0]);
                        const double lon1 = CPLAtof(papszTokens[1]);
                        const double lat2 = CPLAtof(papszTokens[2]);
                        const double lon2 = CPLAtof(papszTokens[3]);
                        poLinearRing->addPoint(lon1, lat1);
                        poLinearRing->addPoint(lon1, lat2);
                        poLinearRing->addPoint(lon2, lat2);
                        poLinearRing->addPoint(lon2, lat1);
                        poLinearRing->addPoint(lon1, lat1);
                    }
                }

                CSLDestroy(papszTokens);
            }
            bInSimpleGeometry = false;
        }
    }
    else if (IS_LAT_ELEMENT(pszName))
    {
        if (pszSubElementValue)
        {
            hasFoundLat = true;
            pszSubElementValue[nSubElementValueLen] = 0;
            latVal = CPLAtof(pszSubElementValue);
        }
        bInGeoLat = false;
    }
    else if (IS_LON_ELEMENT(pszName))
    {
        if (pszSubElementValue)
        {
            hasFoundLon = true;
            pszSubElementValue[nSubElementValueLen] = 0;
            lonVal = CPLAtof(pszSubElementValue);
        }
        bInGeoLong = false;
    }
    else if( bInFeature && currentDepth == featureDepth + 1 )
    {
        if (iCurrentField != -1 && pszSubElementName &&
            poFeature && pszSubElementValue && nSubElementValueLen)
        {
            pszSubElementValue[nSubElementValueLen] = 0;
            if( poFeatureDefn->GetFieldDefn(iCurrentField)->GetType() ==
                  OFTDateTime )
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
                             "Could not parse %s as a valid dateTime",
                             pszSubElementValue);
                }
            }
            else
            {
                if( poFeatureDefn->GetFieldDefn(iCurrentField)->GetType() ==
                      OFTReal )
                    poFeature->SetField(iCurrentField,
                                        CPLAtof(pszSubElementValue));
                else
                    poFeature->SetField(iCurrentField, pszSubElementValue);
            }
        }

        CPLFree(pszSubElementName);
        pszSubElementName = nullptr;
        CPLFree(pszSubElementValue);
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;
    }
    else if( bInFeature &&
             currentDepth > featureDepth + 1 &&
             pszSubElementName != nullptr )
    {
        AddStrToSubElementValue("</");
        AddStrToSubElementValue(pszName);
        AddStrToSubElementValue(">");
    }

    if( poGeom != nullptr )
    {
        if( poFeature != nullptr )
        {
            poFeature->SetGeometryDirectly(poGeom);
        }
        else if( !bInFeature )
        {
            if( poGlobalGeom != nullptr )
                delete poGlobalGeom;
            poGlobalGeom = poGeom;
        }
        else
        {
            delete poGeom;
        }
    }
    else if( !bInFeature && hasFoundLat && hasFoundLon )
    {
        if( poGlobalGeom != nullptr )
                delete poGlobalGeom;
        poGlobalGeom = new OGRPoint(lonVal, latVal);
        hasFoundLat = false;
        hasFoundLon = false;
    }
}

/************************************************************************/
/*                          dataHandlerCbk()                            */
/************************************************************************/

void OGRGeoRSSLayer::dataHandlerCbk( const char *data, int nLen )
{
    if( bStopParsing ) return;

    if( bInGMLGeometry ||
        bInSimpleGeometry ||
        bInGeoLat ||
        bInGeoLong  ||
        pszSubElementName != nullptr )
    {
        char* pszNewSubElementValue = static_cast<char *>(
            VSI_REALLOC_VERBOSE(pszSubElementValue,
                                nSubElementValueLen + nLen + 1));
        if (pszNewSubElementValue == nullptr)
        {
            XML_StopParser(oSchemaParser, XML_FALSE);
            bStopParsing = true;
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
    if( bWriteMode )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot read features when writing a GeoRSS file");
        return nullptr;
    }

    if( fpGeoRSS == nullptr )
        return nullptr;

    if( !bHasReadSchema )
        LoadSchema();

    if( bStopParsing )
        return nullptr;

#ifdef HAVE_EXPAT
    if( nFeatureTabIndex < nFeatureTabLength )
    {
        return ppoFeatureTab[nFeatureTabIndex++];
    }

    if( VSIFEofL(fpGeoRSS) )
        return nullptr;

    CPLFree(ppoFeatureTab);
    ppoFeatureTab = nullptr;
    nFeatureTabLength = 0;
    nFeatureTabIndex = 0;

    int nDone = 0;
    char aBuf[BUFSIZ];
    do
    {
        unsigned int nLen = static_cast<unsigned int>(
            VSIFReadL(aBuf, 1, sizeof(aBuf), fpGeoRSS));
        nDone = VSIFEofL(fpGeoRSS);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of GeoRSS file failed : %s "
                     "at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     static_cast<int>(XML_GetCurrentLineNumber(oParser)),
                     static_cast<int>(XML_GetCurrentColumnNumber(oParser)));
            bStopParsing = true;
        }
    } while( !nDone && !bStopParsing && nFeatureTabLength == 0 );

    return nFeatureTabLength ? ppoFeatureTab[nFeatureTabIndex++] : nullptr;
#else
    return nullptr;
#endif
}

/************************************************************************/
/*              OGRGeoRSSLayerIsStandardFieldInternal()                 */
/************************************************************************/

static bool OGRGeoRSSLayerIsStandardFieldInternal(
    const char* pszName,
    const char* const * papszNames )
{
    for( unsigned int i = 0; papszNames[i] != nullptr; i++)
    {
        if (strcmp(pszName, papszNames[i]) == 0)
        {
            return true;
        }

        const char* pszUnderscore = strchr(papszNames[i], '_');
        if (pszUnderscore == nullptr)
        {
            size_t nLen = strlen(papszNames[i]);
            if( strncmp(pszName, papszNames[i], nLen) == 0 )
            {
                size_t k = nLen;
                while( pszName[k] >= '0' && pszName[k] <= '9' )
                    k++;
                if( pszName[k] == '\0' )
                    return true;
            }
        }
        else
        {
            const size_t nLen = static_cast<size_t>(pszUnderscore - papszNames[i]);
            if( strncmp(pszName, papszNames[i], nLen) == 0 )
            {
                size_t k = nLen;
                while( pszName[k] >= '0' && pszName[k] <= '9' )
                    k++;
                if( pszName[k] == '_' &&
                    strcmp(pszName + k, pszUnderscore) == 0 )
                    return true;
            }
        }
    }
    return false;
}

/************************************************************************/
/*               OGRGeoRSSLayer::IsStandardField()                      */
/************************************************************************/

bool OGRGeoRSSLayer::IsStandardField( const char* pszName )
{
    if( eFormat == GEORSS_RSS )
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

static void OGRGeoRSSLayerSplitComposedField( const char* pszName,
                                              std::string& osElementName,
                                              std::string& osNumber,
                                              std::string& osAttributeName )
{
    osElementName = pszName;

    int i = 0;
    while(pszName[i] != '\0' && pszName[i] != '_' &&
          !(pszName[i] >= '0' && pszName[i] <= '9'))
    {
        i++;
    }

    osElementName.resize(i);

    if (pszName[i] >= '0' && pszName[i] <= '9')
    {
        osNumber = pszName + i;
        const auto nPos = osNumber.find('_');
        if( nPos != std::string::npos )
        {
            osAttributeName = osNumber.substr(nPos + 1);
            osNumber.resize(nPos);
        }
        else
        {
            osAttributeName.clear();
        }
    }
    else
    {
        osNumber.clear();
        if( pszName[i] == '_' )
        {
            osAttributeName = pszName + i + 1;
        }
        else
        {
            osAttributeName.clear();
        }
    }
}

/************************************************************************/
/*                 OGRGeoRSSLayerWriteSimpleElement()                   */
/************************************************************************/

static void OGRGeoRSSLayerWriteSimpleElement( VSILFILE* fp,
                                              const char* pszElementName,
                                              const char* pszNumber,
                                              const char* const * papszNames,
                                              OGRFeatureDefn* poFeatureDefn,
                                              OGRFeature* poFeature )
{
    VSIFPrintfL(fp, "      <%s", pszElementName);

    for( unsigned int k = 0; papszNames[k] != nullptr ; k++)
    {
        if (strncmp(papszNames[k], pszElementName,
                    strlen(pszElementName)) == 0 &&
            papszNames[k][strlen(pszElementName)] == '_')
        {
            const char* pszAttributeName =
                papszNames[k] + strlen(pszElementName) + 1;
            char* pszFieldName =
                CPLStrdup(CPLSPrintf("%s%s_%s", pszElementName,
                                     pszNumber, pszAttributeName));
            int iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
            if (iIndex != -1 && poFeature->IsFieldSetAndNotNull( iIndex ))
            {
                char* pszValue =
                    OGRGetXML_UTF8_EscapedString(
                        poFeature->GetFieldAsString(iIndex));
                VSIFPrintfL(fp, " %s=\"%s\"", pszAttributeName, pszValue);
                CPLFree(pszValue);
            }
            CPLFree(pszFieldName);
        }
    }

    char* pszFieldName =
        CPLStrdup(CPLSPrintf("%s%s", pszElementName, pszNumber));
    const int iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
    if( iIndex != -1 && poFeature->IsFieldSetAndNotNull(iIndex) )
    {
        VSIFPrintfL(fp, ">");

        char* pszValue =
            OGRGetXML_UTF8_EscapedString(poFeature->GetFieldAsString(iIndex));
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

OGRErr OGRGeoRSSLayer::ICreateFeature( OGRFeature *poFeatureIn )

{
    VSILFILE* fp = poDS->GetOutputFP();
    if( fp == nullptr )
        return OGRERR_FAILURE;

    nNextFID++;

    // Verify that compulsory feeds are set.
    // Otherwise put some default value in them.
    if (eFormat == GEORSS_RSS)
    {
        const int iFieldTitle = poFeatureDefn->GetFieldIndex("title");
        const int iFieldDescription =
            poFeatureDefn->GetFieldIndex("description");

        VSIFPrintfL(fp, "    <item>\n");

        if( (iFieldTitle == -1 ||
             !poFeatureIn->IsFieldSetAndNotNull( iFieldTitle )) &&
            (iFieldDescription == -1 ||
             !poFeatureIn->IsFieldSetAndNotNull( iFieldDescription )) )
        {
            VSIFPrintfL(fp, "      <title>Feature %d</title>\n", nNextFID);
        }
    }
    else
    {
        VSIFPrintfL(fp, "    <entry>\n");

        const int iFieldId = poFeatureDefn->GetFieldIndex("id");
        const int iFieldTitle = poFeatureDefn->GetFieldIndex("title");
        const int iFieldUpdated = poFeatureDefn->GetFieldIndex("updated");

        if( iFieldId == -1 || !poFeatureIn->IsFieldSetAndNotNull( iFieldId ) )
        {
            VSIFPrintfL(fp, "      <id>Feature %d</id>\n", nNextFID);
        }

        if( iFieldTitle == -1 ||
            !poFeatureIn->IsFieldSetAndNotNull(iFieldTitle) )
        {
            VSIFPrintfL(fp, "      <title>Title for feature %d</title>\n",
                        nNextFID);
        }

        if( iFieldUpdated == -1 ||
            !poFeatureIn->IsFieldSetAndNotNull(iFieldUpdated) )
        {
            VSIFPrintfL(fp, "      <updated>2009-01-01T00:00:00Z</updated>\n");
        }
    }

    const int nFieldCount = poFeatureDefn->GetFieldCount();
    int* pbUsed = static_cast<int *>(CPLCalloc(sizeof(int), nFieldCount));

    for(int i = 0; i < nFieldCount; i ++)
    {
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( i );
        const char* pszName = poFieldDefn->GetNameRef();

        if ( ! poFeatureIn->IsFieldSetAndNotNull( i ) )
            continue;

        std::string osElementName;
        std::string osNumber;
        std::string osAttributeName;
        OGRGeoRSSLayerSplitComposedField(pszName, osElementName, osNumber,
                                         osAttributeName);

        bool bWillSkip = false;
        // Handle Atom entries with elements with sub-elements like
        // <author><name>...</name><uri>...</uri></author>
        if (eFormat == GEORSS_ATOM)
        {
            for( unsigned int k=0;
                 apszAllowedATOMFieldNamesWithSubElements[k] != nullptr;
                 k++ )
            {
                if( osElementName == apszAllowedATOMFieldNamesWithSubElements[k] &&
                    !osAttributeName.empty() )
                {
                    bWillSkip = true;
                    if( pbUsed[i] )
                        break;

                    VSIFPrintfL(fp, "      <%s>\n", osElementName.c_str());

                    for( int j = i; j < nFieldCount; j++ )
                    {
                        poFieldDefn = poFeatureDefn->GetFieldDefn(j);
                        if ( ! poFeatureIn->IsFieldSetAndNotNull(j) )
                            continue;

                        std::string osElementName2;
                        std::string osNumber2;
                        std::string osAttributeName2;
                        OGRGeoRSSLayerSplitComposedField(
                            poFieldDefn->GetNameRef(),
                            osElementName2, osNumber2,osAttributeName2);

                        if( osElementName2 == osElementName &&
                            osNumber2 == osNumber &&
                            !osAttributeName2.empty() )
                        {
                            pbUsed[j] = TRUE;

                            char* pszValue =
                                OGRGetXML_UTF8_EscapedString(
                                    poFeatureIn->GetFieldAsString(j));
                            VSIFPrintfL(fp, "        <%s>%s</%s>\n",
                                        osAttributeName2.c_str(), pszValue,
                                        osAttributeName2.c_str());
                            CPLFree(pszValue);
                        }
                    }

                    VSIFPrintfL(fp, "      </%s>\n", osElementName.c_str());

                    break;
                }
            }
        }

        if( bWillSkip )
        {
            // Do nothing
        }
        else if( eFormat == GEORSS_RSS &&
                 strcmp(pszName, "pubDate") == 0 )
        {
            const OGRField* psField = poFeatureIn->GetRawFieldRef(i);
            char* pszDate = OGRGetRFC822DateTime(psField);
            VSIFPrintfL(fp, "      <%s>%s</%s>\n",
                    pszName, pszDate, pszName);
            CPLFree(pszDate);
        }
        else if( eFormat == GEORSS_ATOM &&
                 (strcmp(pszName, "updated") == 0 ||
                  strcmp(pszName, "published") == 0) )
        {
            const OGRField* psField = poFeatureIn->GetRawFieldRef(i);
            char* pszDate = OGRGetXMLDateTime(psField);
            VSIFPrintfL(fp, "      <%s>%s</%s>\n",
                    pszName, pszDate, pszName);
            CPLFree(pszDate);
        }
        else if( strcmp(pszName, "dc_date") == 0 )
        {
            const OGRField* psField = poFeatureIn->GetRawFieldRef(i);
            char* pszDate = OGRGetXMLDateTime(psField);
            VSIFPrintfL(fp, "      <%s>%s</%s>\n",
                        "dc:date", pszDate, "dc:date");
            CPLFree(pszDate);
        }
        // RSS fields with content and attributes.
        else if( eFormat == GEORSS_RSS &&
                 (osElementName == "category" ||
                  osElementName == "guid" ||
                  osElementName == "source" ) )
        {
            if( osAttributeName.empty() )
            {
                OGRGeoRSSLayerWriteSimpleElement(
                    fp, osElementName.c_str(), osNumber.c_str(),
                    apszAllowedRSSFieldNames, poFeatureDefn, poFeatureIn);
            }
        }
        // RSS field with attribute only.
        else if( eFormat == GEORSS_RSS &&
                 osElementName == "enclosure" )
        {
            if( osAttributeName == "url" )
            {
                OGRGeoRSSLayerWriteSimpleElement(
                    fp, osElementName.c_str(), osNumber.c_str(),
                    apszAllowedRSSFieldNames, poFeatureDefn, poFeatureIn);
            }
        }
        /* ATOM fields with attribute only */
        else if( eFormat == GEORSS_ATOM &&
                 (osElementName == "category" ||
                  osElementName == "link") )
        {
            if( (osElementName == "category" &&
                 osAttributeName == "term") ||
                (osElementName == "link" &&
                 osAttributeName == "href") )
            {
                OGRGeoRSSLayerWriteSimpleElement(
                    fp, osElementName.c_str(), osNumber.c_str(),
                    apszAllowedATOMFieldNames, poFeatureDefn, poFeatureIn);
            }
        }
        else if( eFormat == GEORSS_ATOM &&
                 (STARTS_WITH(pszName, "content") ||
                  STARTS_WITH(pszName, "summary")) )
        {
            if( strchr(pszName, '_') == nullptr )
            {
                VSIFPrintfL(fp, "      <%s", pszName);

                bool bIsXHTML = false;
                char* pszFieldName =
                    CPLStrdup(CPLSPrintf("%s_%s", pszName, "type"));
                int iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
                if (iIndex != -1 && poFeatureIn->IsFieldSetAndNotNull(iIndex))
                {
                    bIsXHTML = strcmp(poFeatureIn->GetFieldAsString(iIndex),
                                      "xhtml") == 0;
                    char* pszValue =
                        OGRGetXML_UTF8_EscapedString(
                            poFeatureIn->GetFieldAsString(iIndex));
                    VSIFPrintfL(fp, " %s=\"%s\"", "type", pszValue);
                    CPLFree(pszValue);
                }
                CPLFree(pszFieldName);

                pszFieldName =
                    CPLStrdup(CPLSPrintf("%s_%s", pszName, "xml_lang"));
                iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
                if( iIndex != -1 && poFeatureIn->IsFieldSetAndNotNull(iIndex) )
                {
                    char* pszValue =
                        OGRGetXML_UTF8_EscapedString(
                            poFeatureIn->GetFieldAsString( iIndex ));
                    VSIFPrintfL(fp, " %s=\"%s\"", "xml:lang", pszValue);
                    CPLFree(pszValue);
                }
                CPLFree(pszFieldName);

                pszFieldName =
                    CPLStrdup(CPLSPrintf("%s_%s", pszName, "xml_base"));
                iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
                if( iIndex != -1 && poFeatureIn->IsFieldSetAndNotNull(iIndex) )
                {
                    char* pszValue =
                        OGRGetXML_UTF8_EscapedString(
                            poFeatureIn->GetFieldAsString(iIndex));
                    VSIFPrintfL(fp, " %s=\"%s\"", "xml:base", pszValue);
                    CPLFree(pszValue);
                }
                CPLFree(pszFieldName);

                VSIFPrintfL(fp, ">");
                if( bIsXHTML )
                {
                    VSIFPrintfL(fp, "%s", poFeatureIn->GetFieldAsString(i));
                }
                else
                {
                    char* pszValue =
                        OGRGetXML_UTF8_EscapedString(
                            poFeatureIn->GetFieldAsString(i));
                    VSIFPrintfL(fp, "%s", pszValue);
                    CPLFree(pszValue);
                }
                VSIFPrintfL(fp, "      </%s>\n", pszName);
            }
        }
        else if( STARTS_WITH(pszName, "dc_subject") )
        {
            if( strchr(pszName+strlen("dc_subject"), '_') == nullptr )
            {
                VSIFPrintfL(fp, "      <%s", "dc:subject");

                char* pszFieldName =
                    CPLStrdup(CPLSPrintf("%s_%s", pszName, "xml_lang"));
                int iIndex = poFeatureDefn->GetFieldIndex(pszFieldName);
                if (iIndex != -1 && poFeatureIn->IsFieldSetAndNotNull(iIndex))
                {
                    char* pszValue =
                        OGRGetXML_UTF8_EscapedString(
                            poFeatureIn->GetFieldAsString(iIndex));
                    VSIFPrintfL(fp, " %s=\"%s\"", "xml:lang", pszValue);
                    CPLFree(pszValue);
                }
                CPLFree(pszFieldName);

                char* pszValue =
                    OGRGetXML_UTF8_EscapedString(
                        poFeatureIn->GetFieldAsString(i));
                VSIFPrintfL(fp, ">%s</%s>\n", pszValue, "dc:subject");
                CPLFree(pszValue);
            }
        }
        else
        {
            char* pszTagName = CPLStrdup(pszName);
            if( IsStandardField(pszName) == FALSE )
            {
                int nCountUnderscore = 0;
                for( int j=0; pszTagName[j] != 0; j++ )
                {
                    if( pszTagName[j] == '_' )
                    {
                        if( nCountUnderscore == 0 )
                            pszTagName[j] = ':';
                        nCountUnderscore ++;
                    }
                    else if( pszTagName[j] == ' ' )
                        pszTagName[j] = '_';
                }
                if( nCountUnderscore == 0 )
                {
                    char* pszTemp = CPLStrdup(CPLSPrintf("ogr:%s", pszTagName));
                    CPLFree(pszTagName);
                    pszTagName = pszTemp;
                }
            }
            char* pszValue =
                OGRGetXML_UTF8_EscapedString(poFeatureIn->GetFieldAsString(i));
            VSIFPrintfL(fp, "      <%s>%s</%s>\n",
                        pszTagName, pszValue, pszTagName);
            CPLFree(pszValue);
            CPLFree(pszTagName);
        }
    }

    CPLFree(pbUsed);

    OGRGeoRSSGeomDialect eGeomDialect = poDS->GetGeomDialect();
    OGRGeometry* poGeom = poFeatureIn->GetGeometryRef();
    if( poGeom != nullptr && !poGeom->IsEmpty() )
    {
        char* pszURN = nullptr;
        bool bSwapCoordinates = false;
        if( eGeomDialect == GEORSS_GML )
        {
            if( poSRS != nullptr )
            {
                const char* pszAuthorityName = poSRS->GetAuthorityName(nullptr);
                const char* pszAuthorityCode = poSRS->GetAuthorityCode(nullptr);
                if (pszAuthorityName != nullptr &&
                    EQUAL(pszAuthorityName, "EPSG") &&
                    pszAuthorityCode != nullptr)
                {
                    if( !EQUAL(pszAuthorityCode, "4326") )
                        pszURN = CPLStrdup(
                            CPLSPrintf( "urn:ogc:def:crs:EPSG::%s",
                                        pszAuthorityCode ) );

                    /* In case the SRS is a geographic SRS and that we have */
                    /* no axis definition, we assume that the order is */
                    /* lon/lat. */
                    const char* pszAxisName =
                        poSRS->GetAxis(nullptr, 0, nullptr);
                    if (poSRS->IsGeographic() &&
                        (pszAxisName == nullptr ||
                         STARTS_WITH_CI(pszAxisName, "Lon")))
                    {
                        bSwapCoordinates = true;
                    }
                }
                else
                {
                    static bool bOnce = false;
                    if( !bOnce )
                    {
                        bOnce = true;
                        CPLError(CE_Warning, CPLE_AppDefined,
                                 "Could not translate SRS into GML urn");
                    }
                }
            }
            else
            {
                bSwapCoordinates = true;
            }
        }

        char szCoord[75] = {};
        switch( wkbFlatten(poGeom->getGeometryType()) )
        {
            case wkbPoint:
            {
                OGRPoint* poPoint = poGeom->toPoint();
                const double x = poPoint->getX();
                const double y = poPoint->getY();
                if( eGeomDialect == GEORSS_GML )
                {
                    VSIFPrintfL(fp, "      <georss:where><gml:Point");
                    if( pszURN != nullptr)
                        VSIFPrintfL(fp, " srsName=\"%s\"", pszURN);
                    if( poGeom->getCoordinateDimension() == 3 )
                    {
                        OGRMakeWktCoordinate(
                            szCoord,
                            bSwapCoordinates ? y : x,
                            bSwapCoordinates ? x : y,
                            poPoint->getZ(), 3);
                        VSIFPrintfL(fp, " srsDimension=\"3\"><gml:pos>%s",
                                    szCoord);
                    }
                    else
                    {
                        OGRMakeWktCoordinate(szCoord,
                                             bSwapCoordinates ? y : x,
                                             bSwapCoordinates ? x : y,
                                             0, 2);
                        VSIFPrintfL(fp, "><gml:pos>%s", szCoord);
                    }
                    VSIFPrintfL(fp, "</gml:pos></gml:Point></georss:where>\n");
                }
                else if( eGeomDialect == GEORSS_SIMPLE )
                {
                    OGRMakeWktCoordinate(szCoord, y, x, 0, 2);
                    VSIFPrintfL(fp, "      <georss:point>%s</georss:point>\n",
                                szCoord);
                }
                else if (eGeomDialect == GEORSS_W3C_GEO)
                {
                    OGRFormatDouble(szCoord, sizeof(szCoord), y, '.');
                    VSIFPrintfL(fp, "      <geo:lat>%s</geo:lat>\n", szCoord);
                    OGRFormatDouble(szCoord, sizeof(szCoord), x, '.');
                    VSIFPrintfL(fp, "      <geo:long>%s</geo:long>\n", szCoord);
                }
                break;
            }

            case wkbLineString:
            {
                OGRLineString* poLineString = poGeom->toLineString();
                if( eGeomDialect == GEORSS_GML )
                {
                    VSIFPrintfL(fp, "      <georss:where><gml:LineString");
                    if( pszURN != nullptr )
                        VSIFPrintfL(fp, " srsName=\"%s\"", pszURN);
                    VSIFPrintfL(fp, "><gml:posList>\n");
                    const int n = poLineString->getNumPoints();
                    for( int i = 0; i < n; i++ )
                    {
                        const double x = poLineString->getX(i);
                        const double y = poLineString->getY(i);
                        OGRMakeWktCoordinate(szCoord,
                                             bSwapCoordinates ? y : x,
                                             bSwapCoordinates ? x : y,
                                             0, 2);
                        VSIFPrintfL(fp, "%s ", szCoord);
                    }
                    VSIFPrintfL(
                        fp, "</gml:posList></gml:LineString></georss:where>\n");
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
                    // Not supported.
                }
                break;
            }

            case wkbPolygon:
            {
                OGRPolygon* poPolygon = poGeom->toPolygon();
                OGRLineString* poLineString = poPolygon->getExteriorRing();
                if( poLineString == nullptr )
                    break;

                if( eGeomDialect == GEORSS_GML )
                {
                    VSIFPrintfL(fp, "      <georss:where><gml:Polygon");
                    if( pszURN != nullptr )
                        VSIFPrintfL(fp, " srsName=\"%s\"", pszURN);
                    VSIFPrintfL(
                        fp, "><gml:exterior><gml:LinearRing><gml:posList>\n");
                    const int n = poLineString->getNumPoints();
                    for( int i = 0; i < n; i++ )
                    {
                        const double x = poLineString->getX(i);
                        const double y = poLineString->getY(i);
                        OGRMakeWktCoordinate(szCoord,
                                             bSwapCoordinates ? y : x,
                                             bSwapCoordinates ? x : y,
                                             0, 2);
                        VSIFPrintfL(fp, "%s ", szCoord);
                    }
                    VSIFPrintfL(fp,
                                "</gml:posList></gml:LinearRing></gml:exterior>"
                                "</gml:Polygon></georss:where>\n");
                }
                else if( eGeomDialect == GEORSS_SIMPLE )
                {
                    VSIFPrintfL(fp, "      <georss:polygon>\n");
                    const int n = poLineString->getNumPoints();
                    for( int i = 0; i < n; i++ )
                    {
                        const double x = poLineString->getX(i);
                        const double y = poLineString->getY(i);
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

    if( eFormat == GEORSS_RSS )
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
    if( ((eFormat == GEORSS_RSS && strcmp(pszName, "pubDate") == 0) ||
         (eFormat == GEORSS_ATOM && (strcmp(pszName, "updated") == 0 ||
                                     strcmp(pszName, "published") == 0 )) ||
          strcmp(pszName, "dc:date") == 0) &&
        poFieldDefn->GetType() != OFTDateTime )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Wrong field type for %s", pszName);
        return OGRERR_FAILURE;
    }

    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if( strcmp(poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
                   pszName ) == 0 )
        {
            return OGRERR_FAILURE;
        }
    }

    if( IsStandardField(pszName) )
    {
        poFeatureDefn->AddFieldDefn( poFieldDefn );
        return OGRERR_NONE;
    }

    if( poDS->GetUseExtensions() == FALSE )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Field of name '%s' is not supported in %s schema. "
            "Use USE_EXTENSIONS creation option to allow use of extensions.",
            pszName, (eFormat == GEORSS_RSS) ? "RSS" : "ATOM");
        return OGRERR_FAILURE;
    }
    else
    {
        poFeatureDefn->AddFieldDefn(poFieldDefn);
        return OGRERR_NONE;
    }
}

#ifdef HAVE_EXPAT

static void XMLCALL startElementLoadSchemaCbk( void *pUserData,
                                               const char *pszName,
                                               const char **ppszAttr )
{
    ((OGRGeoRSSLayer*)pUserData)->startElementLoadSchemaCbk(pszName, ppszAttr);
}

static void XMLCALL endElementLoadSchemaCbk( void *pUserData,
                                             const char *pszName )
{
    ((OGRGeoRSSLayer*)pUserData)->endElementLoadSchemaCbk(pszName);
}

static void XMLCALL dataHandlerLoadSchemaCbk( void *pUserData, const char *data,
                                              int nLen )
{
    ((OGRGeoRSSLayer*)pUserData)->dataHandlerLoadSchemaCbk(data, nLen);
}

/************************************************************************/
/*                       LoadSchema()                         */
/************************************************************************/

/** This function parses the whole file to detect the fields */
void OGRGeoRSSLayer::LoadSchema()
{
    if( bHasReadSchema )
        return;

    bHasReadSchema = true;

    if( fpGeoRSS == nullptr )
        return;

    oSchemaParser = OGRCreateExpatXMLParser();
    XML_SetElementHandler(oSchemaParser, ::startElementLoadSchemaCbk,
                          ::endElementLoadSchemaCbk);
    XML_SetCharacterDataHandler(oSchemaParser, ::dataHandlerLoadSchemaCbk);
    XML_SetUserData(oSchemaParser, this);

    VSIFSeekL( fpGeoRSS, 0, SEEK_SET );

    bInFeature = false;
    currentDepth = 0;
    currentFieldDefn = nullptr;
    pszSubElementName = nullptr;
    pszSubElementValue = nullptr;
    nSubElementValueLen = 0;
    bSameSRS = true;
    CPLFree(pszGMLSRSName);
    pszGMLSRSName = nullptr;
    eGeomType = wkbUnknown;
    bFoundGeom = false;
    bInTagWithSubTag = false;
    pszTagWithSubTag = nullptr;
    bStopParsing = false;
    nWithoutEventCounter = 0;
    nTotalFeatureCount = 0;
    setOfFoundFields = nullptr;

    char aBuf[BUFSIZ] = {};
    int nDone = 0;
    do
    {
        nDataHandlerCounter = 0;
        unsigned int nLen = (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpGeoRSS );
        nDone = VSIFEofL(fpGeoRSS);
        if (XML_Parse(oSchemaParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "XML parsing of GeoRSS file failed : %s at line %d, column %d",
                XML_ErrorString(XML_GetErrorCode(oSchemaParser)),
                static_cast<int>(XML_GetCurrentLineNumber(oSchemaParser)),
                static_cast<int>(XML_GetCurrentColumnNumber(oSchemaParser)));
            bStopParsing = true;
        }
        nWithoutEventCounter ++;
    } while( !nDone && !bStopParsing && nWithoutEventCounter < 10 );

    XML_ParserFree(oSchemaParser);

    if( nWithoutEventCounter == 10 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too much data inside one element. File probably corrupted");
        bStopParsing = true;
    }

    CPLAssert(poSRS == nullptr);
    if( bSameSRS && bFoundGeom )
    {
        if( pszGMLSRSName == nullptr )
        {
            poSRS = new OGRSpatialReference();
            poSRS->SetWellKnownGeogCS( "WGS84" );
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
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
    setOfFoundFields = nullptr;
    CPLFree(pszGMLSRSName);
    pszGMLSRSName = nullptr;
    CPLFree(pszTagWithSubTag);
    pszTagWithSubTag = nullptr;

    VSIFSeekL( fpGeoRSS, 0, SEEK_SET );
}

/************************************************************************/
/*                  startElementLoadSchemaCbk()                         */
/************************************************************************/

void OGRGeoRSSLayer::startElementLoadSchemaCbk( const char *pszName,
                                                const char **ppszAttr )
{
    if( bStopParsing ) return;

    nWithoutEventCounter = 0;
    const char* pszNoNSName = pszName;
    const char* pszColon = strchr(pszNoNSName, ':');
    if( pszColon )
        pszNoNSName = pszColon + 1;

    if( (eFormat == GEORSS_ATOM && currentDepth == 1 &&
         strcmp(pszNoNSName, "entry") == 0) ||
        ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) && !bInFeature &&
        (currentDepth == 1 || currentDepth == 2) &&
         strcmp(pszNoNSName, "item") == 0) )
    {
        bInFeature = true;
        featureDepth = currentDepth;

        nTotalFeatureCount ++;

        if (setOfFoundFields)
            CPLHashSetDestroy(setOfFoundFields);
        setOfFoundFields = CPLHashSetNew(CPLHashSetHashStr, CPLHashSetEqualStr,
                                         CPLFree);
    }
    else if( bInTagWithSubTag && currentDepth == 3 )
    {
        char* pszFieldName =
            CPLStrdup(CPLSPrintf("%s_%s", pszTagWithSubTag, pszNoNSName));
        if( poFeatureDefn->GetFieldIndex(pszFieldName) == -1 )
        {
            OGRFieldDefn newFieldDefn(pszFieldName, OFTString);
            poFeatureDefn->AddFieldDefn(&newFieldDefn);

            if( poFeatureDefn->GetFieldCount() == 100 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too many fields. File probably corrupted");
                XML_StopParser(oSchemaParser, XML_FALSE);
                bStopParsing = true;
            }
        }
        CPLFree(pszFieldName);
    }
    else if( bInFeature &&
             eFormat == GEORSS_ATOM &&
             currentDepth == 2 &&
             OGRGeoRSSLayerATOMTagHasSubElement(pszNoNSName) )
    {
        CPLFree(pszTagWithSubTag);
        pszTagWithSubTag = CPLStrdup(pszNoNSName);

        int count = 1;
        while(CPLHashSetLookup(setOfFoundFields, pszTagWithSubTag) != nullptr)
        {
            count++;
            CPLFree(pszTagWithSubTag);
            pszTagWithSubTag =
                CPLStrdup(CPLSPrintf("%s%d", pszNoNSName, count));
            if (pszTagWithSubTag[0] == 0)
            {
                XML_StopParser(oSchemaParser, XML_FALSE);
                bStopParsing = true;
                break;
            }
        }
        CPLHashSetInsert(setOfFoundFields, CPLStrdup(pszTagWithSubTag));

        bInTagWithSubTag = true;
    }
    else if( bInFeature &&
             currentDepth == featureDepth + 1 &&
             !IS_GEO_ELEMENT(pszName) )
    {
        if( pszName != pszNoNSName && STARTS_WITH(pszName, "atom:") )
            pszName = pszNoNSName;

        CPLFree(pszSubElementName);
        pszSubElementName = CPLStrdup(pszName);

        int count = 1;
        while( CPLHashSetLookup(setOfFoundFields,
                                pszSubElementName) != nullptr )
        {
            count++;
            CPLFree(pszSubElementName);
            pszSubElementName = CPLStrdup(CPLSPrintf("%s%d", pszName, count));
        }
        CPLHashSetInsert(setOfFoundFields, CPLStrdup(pszSubElementName));

        // Create field definition for element.
        char* pszCompatibleName =
            OGRGeoRSS_GetOGRCompatibleTagName(pszSubElementName);
        int iField = poFeatureDefn->GetFieldIndex(pszCompatibleName);
        if (iField >= 0)
        {
            currentFieldDefn = poFeatureDefn->GetFieldDefn(iField);
        }
        else if (!((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) &&
                   strcmp(pszNoNSName, "enclosure") == 0) &&
                 !(eFormat == GEORSS_ATOM &&
                   strcmp(pszNoNSName, "link") == 0) &&
                 !(eFormat == GEORSS_ATOM &&
                   strcmp(pszNoNSName, "category") == 0))
        {
            OGRFieldType eFieldType;
            if( ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) &&
                 strcmp(pszNoNSName, "pubDate") == 0) ||
                (eFormat == GEORSS_ATOM &&
                 strcmp(pszNoNSName, "updated") == 0) ||
                (eFormat == GEORSS_ATOM &&
                 strcmp(pszNoNSName, "published") == 0) ||
                strcmp(pszName, "dc:date") == 0 )
                eFieldType = OFTDateTime;
            else
                eFieldType = OFTInteger;

            OGRFieldDefn newFieldDefn(pszCompatibleName, eFieldType);
            poFeatureDefn->AddFieldDefn(&newFieldDefn);
            currentFieldDefn =
                poFeatureDefn->GetFieldDefn(poFeatureDefn->GetFieldCount() - 1);

            if( poFeatureDefn->GetFieldCount() == 100 )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Too many fields. File probably corrupted");
                XML_StopParser(oSchemaParser, XML_FALSE);
                bStopParsing = true;
            }
        }

        // Create field definitions for attributes.
        for( int i=0;
             ppszAttr[i] != nullptr && ppszAttr[i+1] != nullptr &&
             !bStopParsing;
             i+= 2 )
        {
            char* pszAttrCompatibleName =
                OGRGeoRSS_GetOGRCompatibleTagName(
                    CPLSPrintf("%s_%s", pszSubElementName, ppszAttr[i]));
            iField = poFeatureDefn->GetFieldIndex(pszAttrCompatibleName);
            OGRFieldDefn* currentAttrFieldDefn = nullptr;
            if (iField >= 0)
            {
                currentAttrFieldDefn = poFeatureDefn->GetFieldDefn(iField);
            }
            else
            {
                OGRFieldDefn newFieldDefn(pszAttrCompatibleName, OFTInteger);
                poFeatureDefn->AddFieldDefn(&newFieldDefn);
                currentAttrFieldDefn =
                    poFeatureDefn->GetFieldDefn(
                        poFeatureDefn->GetFieldCount() - 1);

                if( poFeatureDefn->GetFieldCount() == 100 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Too many fields. File probably corrupted");
                    XML_StopParser(oSchemaParser, XML_FALSE);
                    bStopParsing = true;
                }
            }
            if( currentAttrFieldDefn->GetType() == OFTInteger ||
                currentAttrFieldDefn->GetType() == OFTReal )
            {
                const CPLValueType eType = CPLGetValueType(ppszAttr[i+1]);
                if( eType == CPL_VALUE_REAL )
                {
                    currentAttrFieldDefn->SetType(OFTReal);
                }
                else if( eType == CPL_VALUE_STRING )
                {
                    currentAttrFieldDefn->SetType(OFTString);
                }
            }
            CPLFree(pszAttrCompatibleName);
        }

        CPLFree(pszCompatibleName);
    }
    else if( strcmp(pszName, "georss:point") == 0 ||
             strcmp(pszName, "georss:line") == 0 ||
             strcmp(pszName, "geo:line") == 0 ||
             IS_LAT_ELEMENT(pszName) ||
             strcmp(pszName, "georss:polygon") == 0 ||
             strcmp(pszName, "georss:box") == 0 )
    {
        if( bSameSRS )
        {
            if( pszGMLSRSName != nullptr )
                bSameSRS = false;
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
        if( bSameSRS )
        {
            bool bFoundSRS = false;
            for(int i = 0; ppszAttr[i] != nullptr; i+=2)
            {
                if (strcmp(ppszAttr[i], "srsName") == 0)
                {
                    bFoundSRS = true;
                    if (pszGMLSRSName != nullptr)
                    {
                        if (strcmp(pszGMLSRSName , ppszAttr[i+1]) != 0)
                            bSameSRS = false;
                    }
                    else
                        pszGMLSRSName = CPLStrdup(ppszAttr[i+1]);
                    break;
                }
            }
            if( !bFoundSRS && pszGMLSRSName != nullptr )
                bSameSRS = false;
        }
    }

    if( !bInFeature || currentDepth >= featureDepth + 1 )
    {
        int nDimension = 2;
        for(int i = 0; ppszAttr[i] != nullptr && ppszAttr[i+1] != nullptr; i+=2)
        {
            if (strcmp(ppszAttr[i], "srsDimension") == 0)
            {
                nDimension = atoi(ppszAttr[i+1]);
                break;
            }
        }

        OGRwkbGeometryType eFoundGeomType = wkbUnknown;
        if( strcmp(pszName, "georss:point") == 0 ||
            IS_LAT_ELEMENT(pszName) ||
            strcmp(pszName, "gml:Point") == 0 )
        {
            eFoundGeomType = wkbPoint;
        }
        else if( strcmp(pszName, "gml:MultiPoint") == 0 )
        {
            eFoundGeomType = wkbMultiPoint;
        }
        else if( strcmp(pszName, "georss:line") == 0 ||
                strcmp(pszName, "geo:line") == 0 ||
                strcmp(pszName, "gml:LineString") == 0 )
        {
            eFoundGeomType = wkbLineString;
        }
        else if (strcmp(pszName, "gml:MultiLineString") == 0)
        {
            eFoundGeomType = wkbMultiLineString;
        }
        else if( strcmp(pszName, "georss:polygon") == 0 ||
                 strcmp(pszName, "gml:Polygon") == 0 ||
                 strcmp(pszName, "gml:Envelope") == 0 ||
                 strcmp(pszName, "georss:box") == 0 )
        {
            eFoundGeomType = wkbPolygon;
        }
        else if( strcmp(pszName, "gml:MultiPolygon") == 0 )
        {
            eFoundGeomType = wkbMultiPolygon;
        }

        if( eFoundGeomType != wkbUnknown )
        {
            if( !bFoundGeom )
            {
                eGeomType = eFoundGeomType;
                bFoundGeom = true;
            }
            else if( wkbFlatten(eGeomType) != eFoundGeomType )
            {
                eGeomType = wkbUnknown;
            }

            if( nDimension == 3 )
                eGeomType = wkbSetZ(eGeomType);
        }
    }

    currentDepth++;
}

/************************************************************************/
/*                   endElementLoadSchemaCbk()                          */
/************************************************************************/

void OGRGeoRSSLayer::endElementLoadSchemaCbk( const char *pszName )
{
    if( bStopParsing ) return;

    nWithoutEventCounter = 0;

    currentDepth--;

    if( !bInFeature )
        return;

    const char* pszNoNSName = pszName;
    const char* pszColon = strchr(pszNoNSName, ':');
    if( pszColon )
        pszNoNSName = pszColon + 1;

    if( (eFormat == GEORSS_ATOM && currentDepth == 1 &&
         strcmp(pszNoNSName, "entry") == 0) ||
        ((eFormat == GEORSS_RSS || eFormat == GEORSS_RSS_RDF) &&
        (currentDepth == 1 || currentDepth == 2) &&
         strcmp(pszNoNSName, "item") == 0) )
    {
        bInFeature = false;
    }
    else if( eFormat == GEORSS_ATOM &&
             currentDepth == 2 &&
             OGRGeoRSSLayerATOMTagHasSubElement(pszNoNSName) )
    {
        bInTagWithSubTag = false;
    }
    else if( currentDepth == featureDepth + 1 && pszSubElementName )
    {
        // Patch field type.
        if( pszSubElementValue && nSubElementValueLen && currentFieldDefn )
        {
            pszSubElementValue[nSubElementValueLen] = 0;
            if( currentFieldDefn->GetType() == OFTInteger ||
                currentFieldDefn->GetType() == OFTReal )
            {
                const CPLValueType eType = CPLGetValueType(pszSubElementValue);
                if( eType == CPL_VALUE_REAL )
                {
                    currentFieldDefn->SetType(OFTReal);
                }
                else if( eType == CPL_VALUE_STRING )
                {
                    currentFieldDefn->SetType(OFTString);
                }
             }
        }

        CPLFree(pszSubElementName);
        pszSubElementName = nullptr;
        CPLFree(pszSubElementValue);
        pszSubElementValue = nullptr;
        nSubElementValueLen = 0;
        currentFieldDefn = nullptr;
    }
}

/************************************************************************/
/*                   dataHandlerLoadSchemaCbk()                         */
/************************************************************************/

void OGRGeoRSSLayer::dataHandlerLoadSchemaCbk( const char *data, int nLen )
{
    if( bStopParsing ) return;

    nDataHandlerCounter++;
    if (nDataHandlerCounter >= BUFSIZ)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File probably corrupted (million laugh pattern)");
        XML_StopParser(oSchemaParser, XML_FALSE);
        bStopParsing = true;
        return;
    }

    nWithoutEventCounter = 0;

    if( pszSubElementName )
    {
        char* pszNewSubElementValue = static_cast<char *>(
            VSI_REALLOC_VERBOSE(pszSubElementValue,
                                nSubElementValueLen + nLen + 1));
        if( pszNewSubElementValue == nullptr )
        {
            XML_StopParser(oSchemaParser, XML_FALSE);
            bStopParsing = true;
            return;
        }
        pszSubElementValue = pszNewSubElementValue;
        memcpy(pszSubElementValue + nSubElementValueLen, data, nLen);
        nSubElementValueLen += nLen;
        if( nSubElementValueLen > 100000 )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Too much data inside one element. "
                     "File probably corrupted");
            XML_StopParser(oSchemaParser, XML_FALSE);
            bStopParsing = true;
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

int OGRGeoRSSLayer::TestCapability( const char *pszCap )

{
    if( EQUAL(pszCap,OLCFastFeatureCount) )
        return !bWriteMode && bHasReadSchema &&
               m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;

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
    if( bWriteMode )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot read features when writing a GeoRSS file");
        return 0;
    }

    if( !bHasReadSchema )
        LoadSchema();

    if( m_poFilterGeom != nullptr || m_poAttrQuery != nullptr )
        return OGRLayer::GetFeatureCount(bForce);

    return nTotalFeatureCount;
}
