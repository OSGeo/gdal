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

#include "cpl_port.h"
#include "ogr_wfs.h"
#include "ogr_api.h"
#include "cpl_minixml.h"
#include "cpl_http.h"
#include "gmlutils.h"
#include "parsexsd.h"

CPL_CVSID("$Id$");


/************************************************************************/
/*                            WFSFindNode()                             */
/************************************************************************/

CPLXMLNode* WFSFindNode(CPLXMLNode* psXML, const char* pszRootName)
{
    CPLXMLNode* psIter = psXML;
    while(psIter)
    {
        if (psIter->eType == CXT_Element)
        {
            const char* pszNodeName = psIter->pszValue;
            const char* pszSep = strchr(pszNodeName, ':');
            if (pszSep)
                pszNodeName = pszSep + 1;
            if (EQUAL(pszNodeName, pszRootName))
            {
                return psIter;
            }
        }
        psIter = psIter->psNext;
    }

    psIter = psXML->psChild;
    while(psIter)
    {
        if (psIter->eType == CXT_Element)
        {
            const char* pszNodeName = psIter->pszValue;
            const char* pszSep = strchr(pszNodeName, ':');
            if (pszSep)
                pszNodeName = pszSep + 1;
            if (EQUAL(pszNodeName, pszRootName))
            {
                return psIter;
            }
        }
        psIter = psIter->psNext;
    }
    return NULL;
}

/************************************************************************/
/*                       OGRWFSWrappedResultLayer                       */
/************************************************************************/

class OGRWFSWrappedResultLayer : public OGRLayer
{
    OGRDataSource *poDS;
    OGRLayer      *poLayer;

    public:
        OGRWFSWrappedResultLayer(OGRDataSource* poDS, OGRLayer* poLayer)
        {
            this->poDS = poDS;
            this->poLayer = poLayer;
        }
        ~OGRWFSWrappedResultLayer()
        {
            delete poDS;
        }

        virtual void        ResetReading() { poLayer->ResetReading(); }
        virtual OGRFeature *GetNextFeature() { return poLayer->GetNextFeature(); }
        virtual OGRErr      SetNextByIndex( long nIndex ) { return poLayer->SetNextByIndex(nIndex); }
        virtual OGRFeature *GetFeature( long nFID ) { return poLayer->GetFeature(nFID); }
        virtual OGRFeatureDefn *GetLayerDefn() { return poLayer->GetLayerDefn(); }
        virtual int         GetFeatureCount( int bForce = TRUE ) { return poLayer->GetFeatureCount(bForce); }
        virtual int         TestCapability( const char * pszCap )  { return poLayer->TestCapability(pszCap); }
};


/************************************************************************/
/*                          OGRWFSDataSource()                          */
/************************************************************************/

OGRWFSDataSource::OGRWFSDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;

    bUpdate = FALSE;
    bGetFeatureSupportHits = FALSE;
    bNeedNAMESPACE = FALSE;
    bHasMinOperators = FALSE;
    bHasNullCheck = FALSE;
    bPropertyIsNotEqualToSupported = TRUE; /* advertized by deegree but not implemented */
    bTransactionSupport = FALSE;
    papszIdGenMethods = NULL;
    bUseFeatureId = FALSE; /* CubeWerx doesn't like GmlObjectId */
    bGmlObjectIdNeedsGMLPrefix = FALSE;
    bRequiresEnvelopeSpatialFilter = FALSE;

    bRewriteFile = FALSE;
    psFileXML = NULL;

    bUseHttp10 = FALSE;
    papszHttpOptions = NULL;

    bPagingAllowed = CSLTestBoolean(CPLGetConfigOption("OGR_WFS_PAGING_ALLOWED", "OFF"));
    nPageSize = 0;
    if (bPagingAllowed)
    {
        nPageSize = atoi(CPLGetConfigOption("OGR_WFS_PAGE_SIZE", "100"));
        if (nPageSize <= 0)
            nPageSize = 100;
    }

    bIsGEOSERVER = FALSE;

    bLoadMultipleLayerDefn = CSLTestBoolean(CPLGetConfigOption("OGR_WFS_LOAD_MULTIPLE_LAYER_DEFN", "TRUE"));

    poLayerMetadataDS = NULL;
    poLayerMetadataLayer = NULL;

    poLayerGetCapabilitiesDS = NULL;
    poLayerGetCapabilitiesLayer = NULL;

    bKeepLayerNamePrefix = FALSE;
}

/************************************************************************/
/*                         ~OGRWFSDataSource()                          */
/************************************************************************/

OGRWFSDataSource::~OGRWFSDataSource()

{
    if (psFileXML)
    {
        if (bRewriteFile)
        {
            CPLSerializeXMLTreeToFile(psFileXML, pszName);
        }

        CPLDestroyXMLNode(psFileXML);
    }

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );
    if (osLayerMetadataTmpFileName.size() != 0)
        VSIUnlink(osLayerMetadataTmpFileName);
    delete poLayerMetadataDS;
    delete poLayerGetCapabilitiesDS;

    CPLFree( pszName );
    CSLDestroy( papszIdGenMethods );
    CSLDestroy( papszHttpOptions );
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
    int  bHasFoundLayerWithColon = FALSE;

    if (EQUAL(pszName, "WFSLayerMetadata"))
    {
        if (osLayerMetadataTmpFileName.size() != 0)
            return poLayerMetadataLayer;

        osLayerMetadataTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p/WFSLayerMetadata.csv", this);
        osLayerMetadataCSV = "layer_name,title,abstract\n" + osLayerMetadataCSV;

        VSIFCloseL(VSIFileFromMemBuffer(osLayerMetadataTmpFileName,
                                        (GByte*) osLayerMetadataCSV.c_str(),
                                        osLayerMetadataCSV.size(), FALSE));
        poLayerMetadataDS = (OGRDataSource*) OGROpen(osLayerMetadataTmpFileName,
                                                     FALSE, NULL);
        if (poLayerMetadataDS)
            poLayerMetadataLayer = poLayerMetadataDS->GetLayer(0);
        return poLayerMetadataLayer;
    }
    else if (EQUAL(pszName, "WFSGetCapabilities"))
    {
        if (poLayerGetCapabilitiesLayer != NULL)
            return poLayerGetCapabilitiesLayer;

        OGRSFDriver* poMEMDrv = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("Memory");
        if (poMEMDrv == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot load 'Memory' driver");
            return NULL;
        }

        poLayerGetCapabilitiesDS = poMEMDrv->CreateDataSource("WFSGetCapabilities", NULL);
        poLayerGetCapabilitiesLayer = poLayerGetCapabilitiesDS->CreateLayer("WFSGetCapabilities", NULL, wkbNone, NULL);
        OGRFieldDefn oFDefn("content", OFTString);
        poLayerGetCapabilitiesLayer->CreateField(&oFDefn);
        OGRFeature* poFeature = new OGRFeature(poLayerGetCapabilitiesLayer->GetLayerDefn());
        poFeature->SetField(0, osGetCapabilities);
        poLayerGetCapabilitiesLayer->CreateFeature(poFeature);
        delete poFeature;

        return poLayerGetCapabilitiesLayer;
    }

    /* first a case sensitive check */
    for( i = 0; i < nLayers; i++ )
    {
        OGRWFSLayer *poLayer = papoLayers[i];

        if( strcmp( pszName, poLayer->GetName() ) == 0 )
            return poLayer;

        bHasFoundLayerWithColon |= (strchr( poLayer->GetName(), ':') != NULL);
    }

    /* then case insensitive */
    for( i = 0; i < nLayers; i++ )
    {
        OGRWFSLayer *poLayer = papoLayers[i];

        if( EQUAL( pszName, poLayer->GetName() ) )
            return poLayer;
    }

    /* now try looking after the colon character */
    if (!bKeepLayerNamePrefix && bHasFoundLayerWithColon && strchr(pszName, ':') == NULL)
    {
        for( i = 0; i < nLayers; i++ )
        {
            OGRWFSLayer *poLayer = papoLayers[i];

            const char* pszAfterColon = strchr( poLayer->GetName(), ':');
            if( pszAfterColon && EQUAL( pszName, pszAfterColon + 1 ) )
                return poLayer;
        }
    }

    return NULL;
}

/************************************************************************/
/*                    FindSubStringInsensitive()                        */
/************************************************************************/

const char* FindSubStringInsensitive(const char* pszStr,
                                     const char* pszSubStr)
{
    size_t nSubStrPos = CPLString(pszStr).ifind(pszSubStr);
    if (nSubStrPos == std::string::npos)
        return NULL;
    return pszStr + nSubStrPos;
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
/*                   DetectRequiredOutputFormat()                       */
/************************************************************************/

CPLString OGRWFSDataSource::DetectRequiredOutputFormat(CPLXMLNode* psRoot)
{
    CPLXMLNode* psOperationsMetadata =
        CPLGetXMLNode(psRoot, "OperationsMetadata");
    if (!psOperationsMetadata)
    {
        return "";
    }

    CPLXMLNode* psChild = psOperationsMetadata->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Operation") == 0 &&
            strcmp(CPLGetXMLValue(psChild, "name", ""), "DescribeFeatureType") == 0)
        {
            break;
        }
        psChild = psChild->psNext;
    }
    if (!psChild)
    {
        //CPLDebug("WFS", "Could not find <Operation name=\"DescribeFeatureType\">");
        return "";
    }

    psChild = psChild->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Parameter") == 0 &&
            strcmp(CPLGetXMLValue(psChild, "name", ""), "outputFormat") == 0)
        {
            break;
        }
        psChild = psChild->psNext;
    }
   if (!psChild)
    {
        //CPLDebug("WFS", "Could not find <Parameter name=\"outputFormat\">");
        return "";
    }

    psChild = psChild->psChild;
    int nCountValue = 0;
    const char* pszValue = NULL;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Value") == 0)
        {
            CPLXMLNode* psChild2 = psChild->psChild;
            while(psChild2)
            {
                if (psChild2->eType == CXT_Text)
                {
                    pszValue = psChild2->pszValue;
                    nCountValue ++;
                }
                psChild2 = psChild2->psNext;
            }
        }
        psChild = psChild->psNext;
    }

    /* If there's only one value and it is not GML 3.1.1, then we'll need */
    /* to specify it explicitely */
    /* This is the case for http://deegree3-testing.deegree.org/deegree-inspire-node/services */
    /* which only supports GML 3.2.1 */
    if (nCountValue == 1 && strcmp(pszValue, "text/xml; subtype=gml/3.1.1") != 0)
        return pszValue;

    return "";
}


/************************************************************************/
/*                   DetectRequiresEnvelopeSpatialFilter()              */
/************************************************************************/

int OGRWFSDataSource::DetectRequiresEnvelopeSpatialFilter(CPLXMLNode* psRoot)
{
    /* This is a heuristic to detect Deegree 3 servers, such as */
    /* http://deegree3-demo.deegree.org:80/deegree-utah-demo/services */
    /* that are very GML3 strict, and don't like <gml:Box> in a <Filter><BBOX> */
    /* request, but requires instead <gml:Envelope>, but some servers (such as MapServer) */
    /* don't like <gml:Envelope> so we are obliged to detect the kind of server */

    int nCount;
    CPLXMLNode* psChild;

    CPLXMLNode* psGeometryOperands =
        CPLGetXMLNode(psRoot, "Filter_Capabilities.Spatial_Capabilities.GeometryOperands");
    if (!psGeometryOperands)
    {
        return FALSE;
    }

    nCount = 0;
    psChild = psGeometryOperands->psChild;
    while(psChild)
    {
        nCount ++;
        psChild = psChild->psNext;
    }
    /* Magic number... Might be fragile */
    return (nCount == 19);
}

/************************************************************************/
/*                       GetPostTransactionURL()                        */
/************************************************************************/

CPLString OGRWFSDataSource::GetPostTransactionURL()
{
    if (osPostTransactionURL.size())
        return osPostTransactionURL;

    osPostTransactionURL = osBaseURL;
    const char* pszPostTransactionURL = osPostTransactionURL.c_str();
    const char* pszEsperluet = strchr(pszPostTransactionURL, '?');
    if (pszEsperluet)
        osPostTransactionURL.resize(pszEsperluet - pszPostTransactionURL);

    return osPostTransactionURL;
}

/************************************************************************/
/*                    DetectTransactionSupport()                        */
/************************************************************************/

int OGRWFSDataSource::DetectTransactionSupport(CPLXMLNode* psRoot)
{
    CPLXMLNode* psTransactionWFS100 =
        CPLGetXMLNode(psRoot, "Capability.Request.Transaction");
    if (psTransactionWFS100)
    {
        CPLXMLNode* psPostURL = CPLGetXMLNode(psTransactionWFS100, "DCPType.HTTP.Post");
        if (psPostURL)
        {
            const char* pszPOSTURL = CPLGetXMLValue(psPostURL, "onlineResource", NULL);
            if (pszPOSTURL)
            {
                osPostTransactionURL = pszPOSTURL;
            }
        }

        bTransactionSupport = TRUE;
        return TRUE;
    }
    
    CPLXMLNode* psOperationsMetadata =
        CPLGetXMLNode(psRoot, "OperationsMetadata");
    if (!psOperationsMetadata)
    {
        return FALSE;
    }

    CPLXMLNode* psChild = psOperationsMetadata->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Operation") == 0 &&
            strcmp(CPLGetXMLValue(psChild, "name", ""), "Transaction") == 0)
        {
            break;
        }
        psChild = psChild->psNext;
    }
    if (!psChild)
    {
        CPLDebug("WFS", "No transaction support");
        return FALSE;
    }

    bTransactionSupport = TRUE;
    CPLDebug("WFS", "Transaction support !");


    CPLXMLNode* psPostURL = CPLGetXMLNode(psChild, "DCP.HTTP.Post");
    if (psPostURL)
    {
        const char* pszPOSTURL = CPLGetXMLValue(psPostURL, "href", NULL);
        if (pszPOSTURL)
            osPostTransactionURL = pszPOSTURL;
    }

    psChild = psChild->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Parameter") == 0 &&
            strcmp(CPLGetXMLValue(psChild, "name", ""), "idgen") == 0)
        {
            break;
        }
        psChild = psChild->psNext;
    }
   if (!psChild)
    {
        papszIdGenMethods = CSLAddString(NULL, "GenerateNew");
        return TRUE;
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
                if (psChild2->eType == CXT_Text)
                {
                    papszIdGenMethods = CSLAddString(papszIdGenMethods,
                                                     psChild2->pszValue);
                }
                psChild2 = psChild2->psNext;
            }
        }
        psChild = psChild->psNext;
    }

    return TRUE;
}

/************************************************************************/
/*                      FindComparisonOperator()                        */
/************************************************************************/

static int FindComparisonOperator(CPLXMLNode* psNode, const char* pszVal)
{
    CPLXMLNode* psChild = psNode->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "ComparisonOperator") == 0)
        {
            if (strcmp(CPLGetXMLValue(psChild, NULL, ""), pszVal) == 0)
                return TRUE;

            /* For WFS 2.0.0 */
            const char* pszName = CPLGetXMLValue(psChild, "name", NULL);
            if (pszName != NULL && strncmp(pszName, "PropertyIs", 10) == 0 &&
                strcmp(pszName + 10, pszVal) == 0)
                return TRUE;
        }
        psChild = psChild->psNext;
    }
    return FALSE;
}

/************************************************************************/
/*                          LoadFromFile()                              */
/************************************************************************/

CPLXMLNode* OGRWFSDataSource::LoadFromFile( const char * pszFilename )
{
    VSILFILE *fp;
    char achHeader[1024];

    VSIStatBufL sStatBuf;
    if (VSIStatExL( pszFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) != 0 ||
        VSI_ISDIR(sStatBuf.st_mode))
        return NULL;

    fp = VSIFOpenL( pszFilename, "rb" );

    if( fp == NULL )
        return NULL;

    int nRead;
    if( (nRead = VSIFReadL( achHeader, 1, sizeof(achHeader) - 1, fp )) == 0 )
    {
        VSIFCloseL( fp );
        return NULL;
    }
    achHeader[nRead] = 0;

    if( !EQUALN(achHeader,"<OGRWFSDataSource>",18) &&
        strstr(achHeader,"<WFS_Capabilities") == NULL &&
        strstr(achHeader,"<wfs:WFS_Capabilities") == NULL)
    {
        VSIFCloseL( fp );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      It is the right file, now load the full XML definition.         */
/* -------------------------------------------------------------------- */
    int nLen;

    VSIFSeekL( fp, 0, SEEK_END );
    nLen = (int) VSIFTellL( fp );
    VSIFSeekL( fp, 0, SEEK_SET );

    char* pszXML = (char *) VSIMalloc(nLen+1);
    if (pszXML == NULL)
    {
        VSIFCloseL( fp );
        return NULL;
    }
    pszXML[nLen] = '\0';
    if( ((int) VSIFReadL( pszXML, 1, nLen, fp )) != nLen )
    {
        CPLFree( pszXML );
        VSIFCloseL( fp );

        return NULL;
    }
    VSIFCloseL( fp );

    if (strstr(pszXML, "CubeWerx"))
    {
        /* At least true for CubeWerx Suite 4.15.1 */
        bUseFeatureId = TRUE;
    }
    else if (strstr(pszXML, "deegree"))
    {
        bGmlObjectIdNeedsGMLPrefix = TRUE;
    }

    CPLXMLNode* psXML = CPLParseXMLString( pszXML );
    CPLFree( pszXML );

    return psXML;
}

/************************************************************************/
/*                          SendGetCapabilities()                       */
/************************************************************************/

CPLHTTPResult* OGRWFSDataSource::SendGetCapabilities(const char* pszBaseURL,
                                                     CPLString& osTypeName)
{
    CPLString osURL(pszBaseURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WFS");
    osURL = CPLURLAddKVP(osURL, "REQUEST", "GetCapabilities");
    osTypeName = CPLURLGetValue(osURL, "TYPENAME");
    osURL = CPLURLAddKVP(osURL, "TYPENAME", NULL);
    osURL = CPLURLAddKVP(osURL, "FILTER", NULL);
    osURL = CPLURLAddKVP(osURL, "PROPERTYNAME", NULL);
    osURL = CPLURLAddKVP(osURL, "MAXFEATURES", NULL);
    osURL = CPLURLAddKVP(osURL, "OUTPUTFORMAT", NULL);

    /* Don't accept WFS 2.0.0 for now, unless explicitely specified */
    if (CPLURLGetValue(osURL, "ACCEPTVERSIONS").size() == 0 &&
        CPLURLGetValue(osURL, "VERSION").size() == 0)
        osURL = CPLURLAddKVP(osURL, "ACCEPTVERSIONS", "1.1.0,1.0.0");

    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = HTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return NULL;
    }

    if (strstr((const char*)psResult->pabyData,
                                    "<ServiceExceptionReport") != NULL ||
        strstr((const char*)psResult->pabyData,
                                    "<ows:ExceptionReport") != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }

    return psResult;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRWFSDataSource::Open( const char * pszFilename, int bUpdateIn)

{
    bUpdate = bUpdateIn;
    CPLFree(pszName);
    pszName = CPLStrdup(pszFilename);

    CPLXMLNode* psWFSCapabilities = NULL;
    CPLXMLNode* psXML = LoadFromFile(pszFilename);
    CPLString osTypeName;
    const char* pszBaseURL = NULL;

    if (psXML == NULL)
    {
        if (!EQUALN(pszFilename, "WFS:", 4) &&
            FindSubStringInsensitive(pszFilename, "SERVICE=WFS") == NULL)
        {
            return FALSE;
        }

        pszBaseURL = pszFilename;
        if (EQUALN(pszFilename, "WFS:", 4))
            pszBaseURL += 4;

        osBaseURL = pszBaseURL;

        if (strncmp(pszBaseURL, "http://", 7) != 0 &&
            strncmp(pszBaseURL, "https://", 8) != 0)
            return FALSE;

        CPLHTTPResult* psResult = SendGetCapabilities(pszBaseURL,
                                                      osTypeName);
        if (psResult == NULL)
        {
            return FALSE;
        }

        if (strstr((const char*) psResult->pabyData, "CubeWerx"))
        {
            /* At least true for CubeWerx Suite 4.15.1 */
            bUseFeatureId = TRUE;
        }
        else if (strstr((const char*) psResult->pabyData, "deegree"))
        {
            bGmlObjectIdNeedsGMLPrefix = TRUE;
        }

        psXML = CPLParseXMLString( (const char*) psResult->pabyData );
        if (psXML == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                    psResult->pabyData);
            CPLHTTPDestroyResult(psResult);
            return FALSE;
        }
        osGetCapabilities = (const char*) psResult->pabyData;

        CPLHTTPDestroyResult(psResult);
    }
    else if ( WFSFindNode( psXML, "OGRWFSDataSource" ) == NULL &&
              WFSFindNode( psXML, "WFS_Capabilities" ) != NULL )
    {
        /* This is directly the Capabilities document */
        char* pszXML = CPLSerializeXMLTree(WFSFindNode( psXML, "WFS_Capabilities" ));
        osGetCapabilities = pszXML;
        CPLFree(pszXML);
    }
    else
    {
        CPLXMLNode* psRoot = WFSFindNode( psXML, "OGRWFSDataSource" );
        if (psRoot == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find <OGRWFSDataSource>");
            CPLDestroyXMLNode( psXML );
            return FALSE;
        }

        pszBaseURL = CPLGetXMLValue(psRoot, "URL", NULL);
        if (pszBaseURL == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot find <URL>");
            CPLDestroyXMLNode( psXML );
            return FALSE;
        }
        osBaseURL = pszBaseURL;

/* -------------------------------------------------------------------- */
/*      Capture other parameters.                                       */
/* -------------------------------------------------------------------- */
        const char  *pszParm;

        pszParm = CPLGetXMLValue( psRoot, "Timeout", NULL );
        if( pszParm )
            papszHttpOptions =
                CSLSetNameValue(papszHttpOptions,
                                "TIMEOUT", pszParm );

        pszParm = CPLGetXMLValue( psRoot, "HTTPAUTH", NULL );
        if( pszParm )
            papszHttpOptions =
                CSLSetNameValue( papszHttpOptions,
                                "HTTPAUTH", pszParm );

        pszParm = CPLGetXMLValue( psRoot, "USERPWD", NULL );
        if( pszParm )
            papszHttpOptions =
                CSLSetNameValue( papszHttpOptions,
                                "USERPWD", pszParm );

        pszParm = CPLGetXMLValue( psRoot, "Version", NULL );
        if( pszParm )
            osVersion = pszParm;

        pszParm = CPLGetXMLValue( psRoot, "PagingAllowed", NULL );
        if( pszParm )
            bPagingAllowed = CSLTestBoolean(pszParm);

        pszParm = CPLGetXMLValue( psRoot, "PageSize", NULL );
        if( pszParm )
        {
            nPageSize = atoi(pszParm);
            if (nPageSize <= 0)
                nPageSize = 100;
        }

        osTypeName = CPLURLGetValue(pszBaseURL, "TYPENAME");

        psWFSCapabilities = WFSFindNode( psRoot, "WFS_Capabilities" );
        if (psWFSCapabilities == NULL)
        {
            CPLHTTPResult* psResult = SendGetCapabilities(pszBaseURL,
                                                          osTypeName);
            if (psResult == NULL)
            {
                CPLDestroyXMLNode( psXML );
                return FALSE;
            }

            CPLXMLNode* psXML2 = CPLParseXMLString( (const char*) psResult->pabyData );
            if (psXML2 == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                    psResult->pabyData);
                CPLHTTPDestroyResult(psResult);
                CPLDestroyXMLNode( psXML );
                return FALSE;
            }

            CPLHTTPDestroyResult(psResult);

            psWFSCapabilities = WFSFindNode( psXML2, "WFS_Capabilities" );
            if (psWFSCapabilities == NULL )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find <WFS_Capabilities>");
                CPLDestroyXMLNode( psXML );
                CPLDestroyXMLNode( psXML2 );
                return FALSE;
            }

            CPLAddXMLChild(psXML, CPLCloneXMLTree(psWFSCapabilities));

            int bOK = CPLSerializeXMLTreeToFile(psXML, pszFilename);
            
            CPLDestroyXMLNode( psXML );
            CPLDestroyXMLNode( psXML2 );

            if (bOK)
                return Open(pszFilename, bUpdate);
            else
                return FALSE;
        }
        else
        {
            psFileXML = psXML;

            /* To avoid to have nodes after WFSCapabilities */
            CPLXMLNode* psAfterWFSCapabilities = psWFSCapabilities->psNext;
            psWFSCapabilities->psNext = NULL;
            char* pszXML = CPLSerializeXMLTree(psWFSCapabilities);
            psWFSCapabilities->psNext = psAfterWFSCapabilities;
            osGetCapabilities = pszXML;
            CPLFree(pszXML);
        }
    }

    int bInvertAxisOrderIfLatLong = CSLTestBoolean(CPLGetConfigOption(
                                  "GML_INVERT_AXIS_ORDER_IF_LAT_LONG", "YES"));

    CPLXMLNode* psStrippedXML = CPLCloneXMLTree(psXML);
    CPLStripXMLNamespace( psStrippedXML, NULL, TRUE );
    psWFSCapabilities = CPLGetXMLNode( psStrippedXML, "=WFS_Capabilities" );
    if (psWFSCapabilities == NULL)
    {
        psWFSCapabilities = CPLGetXMLNode( psStrippedXML, "=OGRWFSDataSource.WFS_Capabilities" );
    }
    if (psWFSCapabilities == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find <WFS_Capabilities>");
        if (!psFileXML) CPLDestroyXMLNode( psXML );
        CPLDestroyXMLNode( psStrippedXML );
        return FALSE;
    }

    if (pszBaseURL == NULL)
    {
        /* This is directly the Capabilities document */
        pszBaseURL = CPLGetXMLValue( psWFSCapabilities, "OperationsMetadata.Operation.DCP.HTTP.Get.href", NULL );
        if (pszBaseURL == NULL) /* WFS 1.0.0 variant */
            pszBaseURL = CPLGetXMLValue( psWFSCapabilities, "Capability.Request.GetCapabilities.DCPType.HTTP.Get.onlineResource", NULL );

        if (pszBaseURL == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                        "Cannot find base URL");
            CPLDestroyXMLNode( psStrippedXML );
            return FALSE;
        }

        osBaseURL = pszBaseURL;
    }

    if (osVersion.size() == 0)
        osVersion = CPLGetXMLValue(psWFSCapabilities, "version", "1.0.0");
    if (strcmp(osVersion.c_str(), "1.0.0") == 0)
        bUseFeatureId = TRUE;
    else
    {
        /* Some servers happen to support RESULTTYPE=hits in 1.0.0, but there */
        /* is no way to advertisze this */
        if (atoi(osVersion) >= 2)
            bGetFeatureSupportHits = TRUE;  /* WFS >= 2.0.0 supports hits */
        else
            bGetFeatureSupportHits = DetectIfGetFeatureSupportHits(psWFSCapabilities);
        osRequiredOutputFormat = DetectRequiredOutputFormat(psWFSCapabilities);
        bRequiresEnvelopeSpatialFilter = DetectRequiresEnvelopeSpatialFilter(psWFSCapabilities);
    }

    DetectTransactionSupport(psWFSCapabilities);

    /* Detect if server is GEOSERVER */
    CPLXMLNode* psKeywords = CPLGetXMLNode(psWFSCapabilities, "ServiceIdentification.Keywords");
    if (psKeywords)
    {
        CPLXMLNode* psKeyword = psKeywords->psChild;
        for(;psKeyword != NULL;psKeyword=psKeyword->psNext)
        {
            if (psKeyword->eType == CXT_Element &&
                psKeyword->pszValue != NULL &&
                EQUAL(psKeyword->pszValue, "Keyword") &&
                psKeyword->psChild != NULL &&
                psKeyword->psChild->pszValue != NULL &&
                EQUALN(psKeyword->psChild->pszValue, "GEOSERVER", 9))
            {
                bIsGEOSERVER = TRUE;
                break;
            }
        }
    }

    if (bUpdate && !bTransactionSupport)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Server is read-only WFS; no WFS-T feature advertized");
        if (!psFileXML) CPLDestroyXMLNode( psXML );
        CPLDestroyXMLNode( psStrippedXML );
        return FALSE;
    }

    CPLXMLNode* psFilterCap = CPLGetXMLNode(psWFSCapabilities, "Filter_Capabilities.Scalar_Capabilities");
    if (psFilterCap)
    {
        bHasMinOperators = CPLGetXMLNode(psFilterCap, "LogicalOperators") != NULL ||
                           CPLGetXMLNode(psFilterCap, "Logical_Operators") != NULL;
        if (CPLGetXMLNode(psFilterCap, "ComparisonOperators"))
            psFilterCap = CPLGetXMLNode(psFilterCap, "ComparisonOperators");
        else if (CPLGetXMLNode(psFilterCap, "Comparison_Operators"))
            psFilterCap = CPLGetXMLNode(psFilterCap, "Comparison_Operators");
        else
            psFilterCap = NULL;
        if (psFilterCap)
        {
            if (CPLGetXMLNode(psFilterCap, "Simple_Comparisons") == NULL)
            {
                bHasMinOperators &= FindComparisonOperator(psFilterCap, "LessThan");
                bHasMinOperators &= FindComparisonOperator(psFilterCap, "GreaterThan");
                if (atoi(osVersion) >= 2)
                {
                    bHasMinOperators &= FindComparisonOperator(psFilterCap, "LessThanOrEqualTo");
                    bHasMinOperators &= FindComparisonOperator(psFilterCap, "GreaterThanOrEqualTo");
                }
                else
                {
                    bHasMinOperators &= FindComparisonOperator(psFilterCap, "LessThanEqualTo");
                    bHasMinOperators &= FindComparisonOperator(psFilterCap, "GreaterThanEqualTo");
                }
                bHasMinOperators &= FindComparisonOperator(psFilterCap, "EqualTo");
                bHasMinOperators &= FindComparisonOperator(psFilterCap, "NotEqualTo");
                bHasMinOperators &= FindComparisonOperator(psFilterCap, "Like");
            }
            else
            {
                bHasMinOperators &= CPLGetXMLNode(psFilterCap, "Simple_Comparisons") != NULL &&
                                    CPLGetXMLNode(psFilterCap, "Like") != NULL;
            }
            bHasNullCheck = FindComparisonOperator(psFilterCap, "NullCheck") ||
                            FindComparisonOperator(psFilterCap, "Null") || /* WFS 2.0.0 */
                            CPLGetXMLNode(psFilterCap, "NullCheck") != NULL;
        }
        else
        {
            bHasMinOperators = FALSE;
        }
    }

    CPLXMLNode* psChild = CPLGetXMLNode(psWFSCapabilities, "FeatureTypeList");
    if (psChild == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                    "Cannot find <FeatureTypeList>");
        if (!psFileXML) CPLDestroyXMLNode( psXML );
        CPLDestroyXMLNode( psStrippedXML );
        return FALSE;
    }

    CPLXMLNode* psChildIter;

    /* Check if there are layer names whose identical except their prefix */
    std::set<CPLString> aosSetLayerNames;
    for(psChildIter = psChild->psChild;
        psChildIter != NULL;
        psChildIter = psChildIter->psNext)
    {
        if (psChildIter->eType == CXT_Element &&
            strcmp(psChildIter->pszValue, "FeatureType") == 0)
        {
            const char* pszName = CPLGetXMLValue(psChildIter, "Name", NULL);
            if (pszName != NULL)
            {
                const char* pszShortName = strchr(pszName, ':');
                if (pszShortName)
                    pszName = pszShortName + 1;
                if (aosSetLayerNames.find(pszName) != aosSetLayerNames.end())
                {
                    bKeepLayerNamePrefix = TRUE;
                    CPLDebug("WFS", "At least 2 layers have names that are only distinguishable by keeping the prefix");
                    break;
                }
                aosSetLayerNames.insert(pszName);
            }
        }
    }

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
            const char* pszTitle = CPLGetXMLValue(psChildIter, "Title", NULL);
            const char* pszAbstract = CPLGetXMLValue(psChildIter, "Abstract", NULL);
            if (pszName != NULL &&
                (osTypeName.size() == 0 ||
                    strcmp(osTypeName.c_str(), pszName) == 0))
            {
                const char* pszDefaultSRS =
                        CPLGetXMLValue(psChildIter, "DefaultSRS", NULL);
                if (pszDefaultSRS == NULL)
                    pszDefaultSRS = CPLGetXMLValue(psChildIter, "SRS", NULL);
                if (pszDefaultSRS == NULL)
                    pszDefaultSRS = CPLGetXMLValue(psChildIter, "DefaultCRS", NULL); /* WFS 2.0.0 */

                OGRSpatialReference* poSRS = NULL;
                int bAxisOrderAlreadyInverted = FALSE;

                /* If a SRSNAME parameter has been encoded in the URL, use it as the SRS */
                CPLString osSRSName = CPLURLGetValue(pszBaseURL, "SRSNAME");
                if (osSRSName.size() != 0)
                {
                    pszDefaultSRS = osSRSName.c_str();
                }

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

                char* pszCSVEscaped;

                pszCSVEscaped = CPLEscapeString(pszName, -1, CPLES_CSV);
                osLayerMetadataCSV += pszCSVEscaped;
                CPLFree(pszCSVEscaped);

                osLayerMetadataCSV += ",";
                if (pszTitle)
                {
                    pszCSVEscaped = CPLEscapeString(pszTitle, -1, CPLES_CSV);
                    osLayerMetadataCSV += pszCSVEscaped;
                    CPLFree(pszCSVEscaped);
                }
                osLayerMetadataCSV += ",";
                if (pszAbstract)
                {
                    pszCSVEscaped = CPLEscapeString(pszAbstract, -1, CPLES_CSV);
                    osLayerMetadataCSV += pszCSVEscaped;
                    CPLFree(pszCSVEscaped);
                }
                osLayerMetadataCSV += "\n";

                OGRWFSLayer* poLayer = new OGRWFSLayer(
                            this, poSRS, bAxisOrderAlreadyInverted,
                            pszBaseURL, pszName, pszNS, pszNSVal);

                if (poSRS)
                {
                    char* pszProj4 = NULL;
                    if (poSRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                    {
                        /* See http://trac.osgeo.org/gdal/ticket/4041 */
                        /* For now, we restrict to GEOSERVER as apparently the order is always longitude,latitude */
                        /* other servers might also qualify, so this should be relaxed */
                        /* Also accept when <wfs:DefaultCRS>urn:ogc:def:crs:OGC:1.3:CRS84</wfs:DefaultCRS> */
                        if ((bIsGEOSERVER &&
                            (strcmp(pszProj4, "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs ") == 0 ||
                             strcmp(pszProj4, "+proj=longlat +datum=WGS84 +no_defs ") == 0)) ||
                            strcmp(pszDefaultSRS, "urn:ogc:def:crs:OGC:1.3:CRS84") == 0)
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

                if (psFileXML != NULL)
                {
                    CPLXMLNode* psIter = psXML->psChild;
                    while(psIter)
                    {
                        if (psIter->eType == CXT_Element &&
                            EQUAL(psIter->pszValue, "OGRWFSLayer") &&
                            strcmp(CPLGetXMLValue(psIter, "name", ""), pszName) == 0)
                        {
                            CPLXMLNode* psSchema = WFSFindNode( psIter->psChild, "schema" );
                            if (psSchema)
                            {
                                OGRFeatureDefn* poSrcFDefn = poLayer->ParseSchema(psSchema);
                                if (poSrcFDefn)
                                    poLayer->BuildLayerDefn(poSrcFDefn);
                            }
                            break;
                        }
                        psIter = psIter->psNext;
                    }
                }
            }
        }
    }

    if (!psFileXML) CPLDestroyXMLNode( psXML );
    CPLDestroyXMLNode( psStrippedXML );

    return TRUE;
}

/************************************************************************/
/*                       LoadMultipleLayerDefn()                        */
/************************************************************************/

/* TinyOWS doesn't support POST, but MapServer, GeoServer and Deegree do */
#define USE_GET_FOR_DESCRIBE_FEATURE_TYPE 1

void OGRWFSDataSource::LoadMultipleLayerDefn(const char* pszLayerName,
                                             char* pszNS, char* pszNSVal)
{
    /*if (!bIsGEOSERVER)
        return;*/

    if (!bLoadMultipleLayerDefn)
        return;

    if (aoSetAlreadyTriedLayers.find(pszLayerName) != aoSetAlreadyTriedLayers.end())
        return;

    char* pszPrefix = CPLStrdup(pszLayerName);
    char* pszColumn = strchr(pszPrefix, ':');
    if (pszColumn)
        *pszColumn = 0;
    else
        *pszPrefix = 0;

#if USE_GET_FOR_DESCRIBE_FEATURE_TYPE == 1
    CPLString osLayerToFetch(pszLayerName);
#else
    CPLString osTypeNameToPost;
    osTypeNameToPost += "  <TypeName>";
    osTypeNameToPost += pszLayerName;
    osTypeNameToPost += "</TypeName>\n";
#endif

    int nLayersToFetch = 1;
    aoSetAlreadyTriedLayers.insert(pszLayerName);

    for(int i=0;i<nLayers;i++)
    {
        if (!papoLayers[i]->HasLayerDefn())
        {
            /* We must be careful to requests only layers with the same prefix/namespace */
            const char* pszName = papoLayers[i]->GetName();
            if ((pszPrefix[0] == 0 && strchr(pszName, ':') == NULL) ||
                (pszPrefix[0] != 0 && strncmp(pszName, pszPrefix, strlen(pszPrefix)) == 0 &&
                 pszName[strlen(pszPrefix)] == ':'))
            {
                if (aoSetAlreadyTriedLayers.find(pszName) != aoSetAlreadyTriedLayers.end())
                    continue;
                aoSetAlreadyTriedLayers.insert(pszName);

#if USE_GET_FOR_DESCRIBE_FEATURE_TYPE == 1
                if (nLayersToFetch > 0)
                    osLayerToFetch += ",";
                osLayerToFetch += papoLayers[i]->GetName();
#else
                osTypeNameToPost += "  <TypeName>";
                osTypeNameToPost += pszName;
                osTypeNameToPost += "</TypeName>\n";
#endif
                nLayersToFetch ++;

                /* Avoid fetching to many layer definition at a time */
                if (nLayersToFetch >= 50)
                    break;
            }
        }
    }

    CPLFree(pszPrefix);
    pszPrefix = NULL;

#if USE_GET_FOR_DESCRIBE_FEATURE_TYPE == 1
    CPLString osURL(osBaseURL);
    osURL = CPLURLAddKVP(osURL, "SERVICE", "WFS");
    osURL = CPLURLAddKVP(osURL, "VERSION", GetVersion());
    osURL = CPLURLAddKVP(osURL, "REQUEST", "DescribeFeatureType");
    osURL = CPLURLAddKVP(osURL, "TYPENAME", osLayerToFetch);
    osURL = CPLURLAddKVP(osURL, "PROPERTYNAME", NULL);
    osURL = CPLURLAddKVP(osURL, "MAXFEATURES", NULL);
    osURL = CPLURLAddKVP(osURL, "FILTER", NULL);
    osURL = CPLURLAddKVP(osURL, "OUTPUTFORMAT", GetRequiredOutputFormat());

    if (pszNS && GetNeedNAMESPACE())
    {
        /* Older Deegree version require NAMESPACE */
        /* This has been now corrected */
        CPLString osValue("xmlns(");
        osValue += pszNS;
        osValue += "=";
        osValue += pszNSVal;
        osValue += ")";
        osURL = CPLURLAddKVP(osURL, "NAMESPACE", osValue);
    }

    CPLHTTPResult* psResult = HTTPFetch( osURL, NULL);
#else
    CPLString osPost;
    osPost += "<?xml version=\"1.0\"?>\n";
    osPost += "<wfs:DescribeFeatureType xmlns:wfs=\"http://www.opengis.net/wfs\"\n";
    osPost += "                 xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n";
    osPost += "                 service=\"WFS\" version=\""; osPost += GetVersion(); osPost += "\"\n";
    osPost += "                 xmlns:gml=\"http://www.opengis.net/gml\"\n";
    osPost += "                 xmlns:ogc=\"http://www.opengis.net/ogc\"\n";
    if (pszNS && pszNSVal)
    {
        osPost += "                 xmlns:";
        osPost += pszNS;
        osPost += "=\"";
        osPost += pszNSVal;
        osPost += "\"\n";
    }
    osPost += "                 xsi:schemaLocation=\"http://www.opengis.net/wfs http://schemas.opengis.net/wfs/";
    osPost += GetVersion();
    osPost += "/wfs.xsd\"";
    if (osRequiredOutputFormat.size())
    {
        osPost += "\n";
        osPost += "                 outputFormat=\"";
        osPost += osRequiredOutputFormat;
        osPost += "\"";
    }
    osPost += ">\n";
    osPost += osTypeNameToPost;
    osPost += "</wfs:DescribeFeatureType>\n";

    //CPLDebug("WFS", "%s", osPost.c_str());

    char** papszOptions = NULL;
    papszOptions = CSLAddNameValue(papszOptions, "POSTFIELDS", osPost.c_str());
    papszOptions = CSLAddNameValue(papszOptions, "HEADERS",
                                   "Content-Type: application/xml; charset=UTF-8");

    CPLHTTPResult* psResult = HTTPFetch(GetPostTransactionURL(), papszOptions);
    CSLDestroy(papszOptions);
#endif

    if (psResult == NULL)
    {
        bLoadMultipleLayerDefn = FALSE;
        return;
    }

    if (strstr((const char*)psResult->pabyData, "<ServiceExceptionReport") != NULL)
    {
        if (IsOldDeegree((const char*)psResult->pabyData))
        {
            /* just silently forgive */
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                    psResult->pabyData);
        }
        CPLHTTPDestroyResult(psResult);
        bLoadMultipleLayerDefn = FALSE;
        return;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        bLoadMultipleLayerDefn = FALSE;
        return;
    }
    CPLHTTPDestroyResult(psResult);

    CPLXMLNode* psSchema = WFSFindNode(psXML, "schema");
    if (psSchema == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <Schema>");
        CPLDestroyXMLNode( psXML );
        bLoadMultipleLayerDefn = FALSE;
        return;
    }

    CPLString osTmpFileName;

    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", this);
    CPLSerializeXMLTreeToFile(psSchema, osTmpFileName);

    std::vector<GMLFeatureClass*> aosClasses;
    GMLParseXSD( osTmpFileName, aosClasses );

    if ((int)aosClasses.size() == nLayersToFetch)
    {
        std::vector<GMLFeatureClass*>::const_iterator iter = aosClasses.begin();
        std::vector<GMLFeatureClass*>::const_iterator eiter = aosClasses.end();
        while (iter != eiter)
        {
            GMLFeatureClass* poClass = *iter;
            iter ++;

            OGRWFSLayer* poLayer;

            if (bKeepLayerNamePrefix && pszNS != NULL && strchr(poClass->GetName(), ':') == NULL)
            {
                CPLString osWithPrefix(pszNS);
                osWithPrefix += ":";
                osWithPrefix += poClass->GetName();
                poLayer = (OGRWFSLayer* )GetLayerByName(osWithPrefix);
            }
            else
                poLayer = (OGRWFSLayer* )GetLayerByName(poClass->GetName());

            if (poLayer)
            {
                if (!poLayer->HasLayerDefn())
                {
                    CPLXMLNode* psSchemaForLayer = CPLCloneXMLTree(psSchema);
                    CPLStripXMLNamespace( psSchemaForLayer, NULL, TRUE );
                    CPLXMLNode* psIter = psSchemaForLayer->psChild;
                    int bHasAlreadyImportedGML = FALSE;
                    int bFoundComplexType = FALSE;
                    int bFoundElement = FALSE;
                    while(psIter != NULL)
                    {
                        CPLXMLNode* psIterNext = psIter->psNext;
                        if (psIter->eType == CXT_Element &&
                            strcmp(psIter->pszValue,"complexType") == 0)
                        {
                            const char* pszName = CPLGetXMLValue(psIter, "name", "");
                            CPLString osExpectedName(poLayer->GetShortName());
                            osExpectedName += "Type";
                            CPLString osExpectedName2(poLayer->GetShortName());
                            osExpectedName2 += "_Type";
                            if (strcmp(pszName, osExpectedName) == 0 ||
                                strcmp(pszName, osExpectedName2) == 0 ||
                                strcmp(pszName, poLayer->GetShortName()) == 0)
                            {
                                bFoundComplexType = TRUE;
                            }
                            else
                            {
                                CPLRemoveXMLChild( psSchemaForLayer, psIter );
                                CPLDestroyXMLNode(psIter);
                            }
                        }
                        else if (psIter->eType == CXT_Element &&
                                strcmp(psIter->pszValue,"element") == 0)
                        {
                            const char* pszName = CPLGetXMLValue(psIter, "name", "");
                            CPLString osExpectedName(poLayer->GetShortName());
                            osExpectedName += "Type";
                            CPLString osExpectedName2(poLayer->GetShortName());
                            osExpectedName2 += "_Type";

                            const char* pszType = CPLGetXMLValue(psIter, "type", "");
                            CPLString osExpectedType(poLayer->GetName());
                            osExpectedType += "Type";
                            CPLString osExpectedType2(poLayer->GetName());
                            osExpectedType2 += "_Type";
                            if (strcmp(pszType, osExpectedType) == 0 ||
                                strcmp(pszType, osExpectedType2) == 0 ||
                                strcmp(pszType, poLayer->GetName()) == 0 ||
                                (strchr(pszType, ':') &&
                                 (strcmp(strchr(pszType, ':') + 1, osExpectedType) == 0 ||
                                  strcmp(strchr(pszType, ':') + 1, osExpectedType2) == 0)))
                            {
                                bFoundElement = TRUE;
                            }
                            else if (*pszType == '\0' &&
                                     CPLGetXMLNode(psIter, "complexType") != NULL &&
                                     (strcmp(pszName, osExpectedName) == 0 ||
                                      strcmp(pszName, osExpectedName2) == 0 ||
                                      strcmp(pszName, poLayer->GetShortName()) == 0) )
                            {
                                bFoundElement = TRUE;
                                bFoundComplexType = TRUE;
                            }
                            else
                            {
                                CPLRemoveXMLChild( psSchemaForLayer, psIter );
                                CPLDestroyXMLNode(psIter);
                            }
                        }
                        else if (psIter->eType == CXT_Element &&
                                strcmp(psIter->pszValue,"import") == 0 &&
                                strcmp(CPLGetXMLValue(psIter, "namespace", ""),
                                        "http://www.opengis.net/gml") == 0)
                        {
                            if (bHasAlreadyImportedGML)
                            {
                                CPLRemoveXMLChild( psSchemaForLayer, psIter );
                                CPLDestroyXMLNode(psIter);
                            }
                            else
                                bHasAlreadyImportedGML = TRUE;
                        }
                        psIter = psIterNext;
                    }

                    if (bFoundComplexType && bFoundElement)
                    {
                        OGRFeatureDefn* poSrcFDefn = poLayer->ParseSchema(psSchemaForLayer);
                        if (poSrcFDefn)
                        {
                            poLayer->BuildLayerDefn(poSrcFDefn);
                            SaveLayerSchema(poLayer->GetName(), psSchemaForLayer);
                        }
                    }

                    CPLDestroyXMLNode(psSchemaForLayer);
                }
                else
                {
                    CPLDebug("WFS", "Found several time schema for layer %s in server response. Shouldn't happen",
                             poClass->GetName());
                }
            }
            else
            {
                CPLDebug("WFS", "Cannot find layer %s. Shouldn't happen",
                            poClass->GetName());
            }
            delete poClass;
        }
    }
    else if (aosClasses.size() > 0)
    {
        std::vector<GMLFeatureClass*>::const_iterator iter = aosClasses.begin();
        std::vector<GMLFeatureClass*>::const_iterator eiter = aosClasses.end();
        while (iter != eiter)
        {
            GMLFeatureClass* poClass = *iter;
            iter ++;
            delete poClass;
        }
    }
    else
    {
        CPLDebug("WFS", "Turn off loading of multiple layer definitions at a single time");
        bLoadMultipleLayerDefn = FALSE;
    }

    VSIUnlink(osTmpFileName);

    CPLDestroyXMLNode( psXML );
}

/************************************************************************/
/*                         SaveLayerSchema()                            */
/************************************************************************/

void OGRWFSDataSource::SaveLayerSchema(const char* pszLayerName, CPLXMLNode* psSchema)
{
    if (psFileXML != NULL)
    {
        bRewriteFile = TRUE;
        CPLXMLNode* psLayerNode = CPLCreateXMLNode(NULL, CXT_Element, "OGRWFSLayer");
        CPLSetXMLValue(psLayerNode, "#name", pszLayerName);
        CPLAddXMLChild(psLayerNode, CPLCloneXMLTree(psSchema));
        CPLAddXMLChild(psFileXML, psLayerNode);
    }
}

/************************************************************************/
/*                           IsOldDeegree()                             */
/************************************************************************/

int OGRWFSDataSource::IsOldDeegree(const char* pszErrorString)
{
    if (!bNeedNAMESPACE &&
        strstr(pszErrorString, "Invalid \"TYPENAME\" parameter. No binding for prefix") != NULL)
    {
        bNeedNAMESPACE = TRUE;
        return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                         WFS_EscapeURL()                              */
/************************************************************************/

static CPLString WFS_EscapeURL(CPLString osURL)
{
    CPLString osNewURL;
    size_t i;

    int bNeedsEscaping = FALSE;
    for(i=0;i<osURL.size();i++)
    {
        char ch = osURL[i];
        if (ch == '<' || ch == '>' || ch == ' ' || ch == '"')
        {
            bNeedsEscaping = TRUE;
            break;
        }
    }

    if (!bNeedsEscaping)
        return osURL;

    for(i=0;i<osURL.size();i++)
    {
        char ch = osURL[i];
        if (ch == '<')
            osNewURL += "%3C";
        else if (ch == '>')
            osNewURL += "%3E";
        else if (ch == ' ')
            osNewURL += "%20";
        else if (ch == '"')
            osNewURL += "%22";
        else if (ch == '%')
            osNewURL += "%25";
        else
            osNewURL += ch;
    }
    return osNewURL;
}

/************************************************************************/
/*                            HTTPFetch()                               */
/************************************************************************/

CPLHTTPResult* OGRWFSDataSource::HTTPFetch( const char* pszURL, char** papszOptions )
{
    char** papszNewOptions = CSLDuplicate(papszOptions);
    if (bUseHttp10)
        papszNewOptions = CSLAddNameValue(papszNewOptions, "HTTP_VERSION", "1.0");
    if (papszHttpOptions)
        papszNewOptions = CSLMerge(papszNewOptions, papszHttpOptions);
    CPLHTTPResult* psResult = CPLHTTPFetch( WFS_EscapeURL(pszURL), papszNewOptions );
    CSLDestroy(papszNewOptions);
    
    if (psResult == NULL)
    {
        return NULL;
    }
    if (psResult->nStatus != 0 || psResult->pszErrBuf != NULL)
    {
        /* A few buggy servers return chunked data with errouneous remaining bytes value */
        /* curl doesn't like this. Retry with HTTP 1.0 protocol instead that doesn't support */
        /* chunked data */
        if (psResult->pszErrBuf &&
            strstr(psResult->pszErrBuf, "transfer closed with outstanding read data remaining") &&
            !bUseHttp10)
        {
            CPLDebug("WFS", "Probably buggy remote server. Retrying with HTTP 1.0 protocol");
            bUseHttp10 = TRUE;
            return HTTPFetch(pszURL, papszOptions);
        }

        CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s (%d)",
                 (psResult->pszErrBuf) ? psResult->pszErrBuf : "unknown", psResult->nStatus);
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    if (psResult->pabyData == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Empty content returned by server");
        CPLHTTPDestroyResult(psResult);
        return NULL;
    }
    return psResult;
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer * OGRWFSDataSource::ExecuteSQL( const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect )

{
/* -------------------------------------------------------------------- */
/*      Use generic implementation for OGRSQL dialect.                  */
/* -------------------------------------------------------------------- */
    if( pszDialect != NULL && EQUAL(pszDialect,"OGRSQL") )
        return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                          poSpatialFilter,
                                          pszDialect );

/* -------------------------------------------------------------------- */
/*      Deal with "SELECT _LAST_INSERTED_FIDS_ FROM layername" statement */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand, "SELECT _LAST_INSERTED_FIDS_ FROM ", 33) )
    {
        const char* pszIter = pszSQLCommand + 33;
        while(*pszIter && *pszIter != ' ')
            pszIter ++;

        CPLString osName = pszSQLCommand + 33;
        osName.resize(pszIter - (pszSQLCommand + 33));
        OGRWFSLayer* poLayer = (OGRWFSLayer*)GetLayerByName(osName);
        if (poLayer == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown layer : %s", osName.c_str());
            return NULL;
        }

        OGRSFDriver* poMEMDrv = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("Memory");
        if (poMEMDrv == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot load 'Memory' driver");
            return NULL;
        }

        OGRDataSource* poMEMDS = poMEMDrv->CreateDataSource("dummy_name", NULL);
        OGRLayer* poMEMLayer = poMEMDS->CreateLayer("FID_LIST", NULL, wkbNone, NULL);
        OGRFieldDefn oFDefn("gml_id", OFTString);
        poMEMLayer->CreateField(&oFDefn);

        const std::vector<CPLString>& aosFIDList = poLayer->GetLastInsertedFIDList();
        std::vector<CPLString>::const_iterator iter = aosFIDList.begin();
        std::vector<CPLString>::const_iterator eiter = aosFIDList.end();
        while (iter != eiter)
        {
            const CPLString& osFID = *iter;
            OGRFeature* poFeature = new OGRFeature(poMEMLayer->GetLayerDefn());
            poFeature->SetField(0, osFID);
            poMEMLayer->CreateFeature(poFeature);
            delete poFeature;
            iter ++;
        }

        return new OGRWFSWrappedResultLayer(poMEMDS, poMEMLayer);
    }

/* -------------------------------------------------------------------- */
/*      Deal with "DELETE FROM layer_name WHERE expression" statement   */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszSQLCommand, "DELETE FROM ", 12) )
    {
        const char* pszIter = pszSQLCommand + 12;
        while(*pszIter && *pszIter != ' ')
            pszIter ++;
        if (*pszIter == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid statement");
            return NULL;
        }

        CPLString osName = pszSQLCommand + 12;
        osName.resize(pszIter - (pszSQLCommand + 12));
        OGRWFSLayer* poLayer = (OGRWFSLayer*)GetLayerByName(osName);
        if (poLayer == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unknown layer : %s", osName.c_str());
            return NULL;
        }

        while(*pszIter && *pszIter == ' ')
            pszIter ++;
        if (!EQUALN(pszIter, "WHERE ", 5))
        {
            CPLError(CE_Failure, CPLE_AppDefined, "WHERE clause missing");
            return NULL;
        }
        pszIter += 5;

        const char* pszQuery = pszIter;

        /* Check with the generic SQL engine that this is a valid WHERE clause */
        OGRFeatureQuery oQuery;
        OGRErr eErr = oQuery.Compile( poLayer->GetLayerDefn(), pszQuery );
        if( eErr != OGRERR_NONE )
        {
            return NULL;
        }

        /* Now turn this into OGC Filter language if possible */
        int bNeedsNullCheck = FALSE;
        int nVersion = (strcmp(GetVersion(),"1.0.0") == 0) ? 100 : 110;
        CPLString osOGCFilter = WFS_TurnSQLFilterToOGCFilter(pszQuery,
                                                             nVersion,
                                                             bPropertyIsNotEqualToSupported,
                                                             bUseFeatureId,
                                                             bGmlObjectIdNeedsGMLPrefix,
                                                             &bNeedsNullCheck);
        if (bNeedsNullCheck && !HasNullCheck())
            osOGCFilter = "";

        if (osOGCFilter.size() == 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot convert WHERE clause into a OGC filter");
            return NULL;
        }

        poLayer->DeleteFromFilter(osOGCFilter);

        return NULL;
    }

    return OGRDataSource::ExecuteSQL( pszSQLCommand,
                                      poSpatialFilter,
                                      pszDialect );

}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRWFSDataSource::ReleaseResultSet( OGRLayer * poResultsSet )
{
    delete poResultsSet;
}
