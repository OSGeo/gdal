/******************************************************************************
 *
 * Project:  WFS Translator
 * Purpose:  Implements OGRWFSDataSource class
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "swq.h"
#include "ogr_p.h"

#include <algorithm>

CPL_CVSID("$Id$")

static const int DEFAULT_BASE_START_INDEX = 0;
static const int DEFAULT_PAGE_SIZE = 100;

typedef struct
{
    const char* pszPath;
    const char* pszMDI;
} MetadataItem;

static const MetadataItem asMetadata[] =
{
    { "Service.Title", "TITLE" }, /*1.0 */
    { "ServiceIdentification.Title", "TITLE" }, /* 1.1 or 2.0 */
    { "Service.Abstract", "ABSTRACT" }, /* 1.0 */
    { "ServiceIdentification.Abstract", "ABSTRACT" }, /* 1.1 or 2.0 */
    { "ServiceProvider.ProviderName", "PROVIDER_NAME" }, /* 1.1 or 2.0 */
};

/************************************************************************/
/*                            WFSFindNode()                             */
/************************************************************************/

CPLXMLNode* WFSFindNode( CPLXMLNode* psXML, const char* pszRootName )
{
    CPLXMLNode* psIter = psXML;
    do
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
    } while(psIter);

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
    GDALDataset *poDS;
    OGRLayer    *poLayer;

    public:
        OGRWFSWrappedResultLayer( GDALDataset* poDSIn, OGRLayer* poLayerIn ) :
            poDS(poDSIn),
            poLayer(poLayerIn)
        {}
        ~OGRWFSWrappedResultLayer()
        {
            delete poDS;
        }

        virtual void        ResetReading() override { poLayer->ResetReading(); }
        virtual OGRFeature *GetNextFeature() override { return poLayer->GetNextFeature(); }
        virtual OGRErr      SetNextByIndex( GIntBig nIndex ) override { return poLayer->SetNextByIndex(nIndex); }
        virtual OGRFeature *GetFeature( GIntBig nFID ) override { return poLayer->GetFeature(nFID); }
        virtual OGRFeatureDefn *GetLayerDefn() override { return poLayer->GetLayerDefn(); }
        virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override { return poLayer->GetFeatureCount(bForce); }
        virtual int         TestCapability( const char * pszCap ) override  { return poLayer->TestCapability(pszCap); }
};

/************************************************************************/
/*                          OGRWFSDataSource()                          */
/************************************************************************/

OGRWFSDataSource::OGRWFSDataSource() :
    pszName(NULL),
    bRewriteFile(false),
    psFileXML(NULL),
    papoLayers(NULL),
    nLayers(0),
    bUpdate(false),
    bGetFeatureSupportHits(false),
    bNeedNAMESPACE(false),
    bHasMinOperators(false),
    bHasNullCheck(false),
    // Advertized by deegree but not implemented.
    bPropertyIsNotEqualToSupported(true),
    bUseFeatureId(false),  // CubeWerx doesn't like GmlObjectId.
    bGmlObjectIdNeedsGMLPrefix(false),
    bRequiresEnvelopeSpatialFilter(false),
    bTransactionSupport(false),
    papszIdGenMethods(NULL),
    bUseHttp10(false),
    papszHttpOptions(NULL),
    bPagingAllowed(CPLTestBool(
        CPLGetConfigOption("OGR_WFS_PAGING_ALLOWED", "OFF"))),
    nPageSize(DEFAULT_PAGE_SIZE),
    nBaseStartIndex(DEFAULT_BASE_START_INDEX),
    bStandardJoinsWFS2(false),
    bLoadMultipleLayerDefn(CPLTestBool(
        CPLGetConfigOption("OGR_WFS_LOAD_MULTIPLE_LAYER_DEFN", "TRUE"))),
    poLayerMetadataDS(NULL),
    poLayerMetadataLayer(NULL),
    poLayerGetCapabilitiesDS(NULL),
    poLayerGetCapabilitiesLayer(NULL),
    bKeepLayerNamePrefix(false),
    bEmptyAsNull(true),
    bInvertAxisOrderIfLatLong(true),
    bExposeGMLId(true)
{
    if( bPagingAllowed )
    {
        const char* pszOption = CPLGetConfigOption("OGR_WFS_PAGE_SIZE", NULL);
        if( pszOption != NULL )
        {
            nPageSize = atoi(pszOption);
            if (nPageSize <= 0)
                nPageSize = DEFAULT_PAGE_SIZE;
        }

        pszOption = CPLGetConfigOption("OGR_WFS_BASE_START_INDEX", NULL);
        if( pszOption != NULL )
            nBaseStartIndex = atoi(pszOption);
    }

    apszGetCapabilities[0] = NULL;
    apszGetCapabilities[1] = NULL;
}

/************************************************************************/
/*                         ~OGRWFSDataSource()                          */
/************************************************************************/

OGRWFSDataSource::~OGRWFSDataSource()

{
    if( psFileXML )
    {
        if( bRewriteFile )
        {
            CPLSerializeXMLTreeToFile(psFileXML, pszName);
        }

        CPLDestroyXMLNode(psFileXML);
    }

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if (!osLayerMetadataTmpFileName.empty())
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

int OGRWFSDataSource::TestCapability( CPL_UNUSED const char * pszCap )
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

OGRLayer* OGRWFSDataSource::GetLayerByName(const char* pszNameIn)
{
    if ( ! pszNameIn )
        return NULL;

    if (EQUAL(pszNameIn, "WFSLayerMetadata"))
    {
        if (!osLayerMetadataTmpFileName.empty())
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
    else if (EQUAL(pszNameIn, "WFSGetCapabilities"))
    {
        if (poLayerGetCapabilitiesLayer != NULL)
            return poLayerGetCapabilitiesLayer;

        GDALDriver* poMEMDrv = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("Memory");
        if (poMEMDrv == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot load 'Memory' driver");
            return NULL;
        }

        poLayerGetCapabilitiesDS = poMEMDrv->Create("WFSGetCapabilities", 0, 0, 0, GDT_Unknown, NULL);
        poLayerGetCapabilitiesLayer = poLayerGetCapabilitiesDS->CreateLayer("WFSGetCapabilities", NULL, wkbNone, NULL);
        OGRFieldDefn oFDefn("content", OFTString);
        poLayerGetCapabilitiesLayer->CreateField(&oFDefn);
        OGRFeature* poFeature = new OGRFeature(poLayerGetCapabilitiesLayer->GetLayerDefn());
        poFeature->SetField(0, osGetCapabilities);
        CPL_IGNORE_RET_VAL(poLayerGetCapabilitiesLayer->CreateFeature(poFeature));
        delete poFeature;

        return poLayerGetCapabilitiesLayer;
    }

    int nIndex = GetLayerIndex(pszNameIn);
    if (nIndex < 0)
        return NULL;
    else
        return papoLayers[nIndex];
}

/************************************************************************/
/*                        GetMetadataDomainList()                       */
/************************************************************************/

char** OGRWFSDataSource::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "", "xml:capabilities", NULL);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char** OGRWFSDataSource::GetMetadata( const char * pszDomain )
{
    if( pszDomain != NULL && EQUAL(pszDomain, "xml:capabilities") )
    {
        apszGetCapabilities[0] = osGetCapabilities.c_str();
        apszGetCapabilities[1] = NULL;
        return (char**) apszGetCapabilities;
    }
    return GDALDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                          GetLayerIndex()                             */
/************************************************************************/

int OGRWFSDataSource::GetLayerIndex(const char* pszNameIn)
{
    bool bHasFoundLayerWithColon = false;

    /* first a case sensitive check */
    for( int i = 0; i < nLayers; i++ )
    {
        OGRWFSLayer *poLayer = papoLayers[i];

        if( strcmp( pszNameIn, poLayer->GetName() ) == 0 )
            return i;

        bHasFoundLayerWithColon |= strchr( poLayer->GetName(), ':') != NULL;
    }

    /* then case insensitive */
    for( int i = 0; i < nLayers; i++ )
    {
        OGRWFSLayer *poLayer = papoLayers[i];

        if( EQUAL( pszNameIn, poLayer->GetName() ) )
            return i;
    }

    /* now try looking after the colon character */
    if( !bKeepLayerNamePrefix &&
        bHasFoundLayerWithColon &&
        strchr(pszNameIn, ':') == NULL )
    {
        for( int i = 0; i < nLayers; i++ )
        {
            OGRWFSLayer *poLayer = papoLayers[i];

            const char* pszAfterColon = strchr( poLayer->GetName(), ':');
            if( pszAfterColon && EQUAL( pszNameIn, pszAfterColon + 1 ) )
                return i;
        }
    }

    return -1;
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

static bool DetectIfGetFeatureSupportHits( CPLXMLNode* psRoot )
{
    CPLXMLNode* psOperationsMetadata =
        CPLGetXMLNode(psRoot, "OperationsMetadata");
    if (!psOperationsMetadata)
    {
        CPLDebug("WFS", "Could not find <OperationsMetadata>");
        return false;
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
        return false;
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
        return false;
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
                    return true;
                }
                psChild2 = psChild2->psNext;
            }
        }
        psChild = psChild->psNext;
    }

    return false;
}

/************************************************************************/
/*                   DetectRequiresEnvelopeSpatialFilter()              */
/************************************************************************/

bool OGRWFSDataSource::DetectRequiresEnvelopeSpatialFilter( CPLXMLNode* psRoot )
{
    // This is a heuristic to detect Deegree 3 servers, such as
    // http://deegree3-demo.deegree.org:80/deegree-utah-demo/services that are
    // very GML3 strict, and don't like <gml:Box> in a <Filter><BBOX> request,
    // but requires instead <gml:Envelope>, but some servers (such as MapServer)
    // don't like <gml:Envelope> so we are obliged to detect the kind of server.

    CPLXMLNode* psGeometryOperands =
        CPLGetXMLNode(
            psRoot,
            "Filter_Capabilities.Spatial_Capabilities.GeometryOperands");
    if (!psGeometryOperands)
    {
        return false;
    }

    int nCount = 0;
    CPLXMLNode* psChild = psGeometryOperands->psChild;
    while( psChild )
    {
        nCount++;
        psChild = psChild->psNext;
    }
    // Magic number... Might be fragile.
    return nCount == 19;
}

/************************************************************************/
/*                       GetPostTransactionURL()                        */
/************************************************************************/

CPLString OGRWFSDataSource::GetPostTransactionURL()
{
    if (!osPostTransactionURL.empty() )
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

bool OGRWFSDataSource::DetectTransactionSupport( CPLXMLNode* psRoot )
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

        bTransactionSupport = true;
        return true;
    }

    CPLXMLNode* psOperationsMetadata =
        CPLGetXMLNode(psRoot, "OperationsMetadata");
    if (!psOperationsMetadata)
    {
        return false;
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
        return false;
    }

    bTransactionSupport = true;
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
        return true;
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

    return true;
}

/************************************************************************/
/*                    DetectSupportPagingWFS2()                         */
/************************************************************************/

bool OGRWFSDataSource::DetectSupportPagingWFS2( CPLXMLNode* psRoot )
{
    const char* pszPagingAllowed = CPLGetConfigOption("OGR_WFS_PAGING_ALLOWED", NULL);
    if( pszPagingAllowed != NULL && !CPLTestBool(pszPagingAllowed) )
        return false;

    CPLXMLNode* psOperationsMetadata =
        CPLGetXMLNode(psRoot, "OperationsMetadata");
    if (!psOperationsMetadata)
    {
        return false;
    }

    CPLXMLNode* psChild = psOperationsMetadata->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Constraint") == 0 &&
            strcmp(CPLGetXMLValue(psChild, "name", ""), "ImplementsResultPaging") == 0)
        {
            if( !EQUAL(CPLGetXMLValue(psChild, "DefaultValue", ""), "TRUE") )
            {
                psChild = NULL;
                break;
            }
            break;
        }
        psChild = psChild->psNext;
    }
    if (!psChild)
    {
        CPLDebug("WFS", "No paging support");
        return false;
    }

    psChild = psOperationsMetadata->psChild;
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
    if (psChild && CPLGetConfigOption("OGR_WFS_PAGE_SIZE", NULL) == NULL)
    {
        psChild = psChild->psChild;
        while(psChild)
        {
            if (psChild->eType == CXT_Element &&
                strcmp(psChild->pszValue, "Constraint") == 0 &&
                strcmp(CPLGetXMLValue(psChild, "name", ""), "CountDefault") == 0)
            {
                int nVal = atoi(CPLGetXMLValue(psChild, "DefaultValue", "0"));
                if( nVal > 0 )
                    nPageSize = nVal;

                break;
            }
            psChild = psChild->psNext;
        }
    }
    const char* pszOption = CPLGetConfigOption("OGR_WFS_PAGE_SIZE", NULL);
    if( pszOption != NULL )
    {
        nPageSize = atoi(pszOption);
        if (nPageSize <= 0)
            nPageSize = DEFAULT_PAGE_SIZE;
    }

    CPLDebug("WFS", "Paging support with page size %d", nPageSize);
    bPagingAllowed = true;

    return true;
}

/************************************************************************/
/*                   DetectSupportStandardJoinsWFS2()                   */
/************************************************************************/

bool OGRWFSDataSource::DetectSupportStandardJoinsWFS2(CPLXMLNode* psRoot)
{
    CPLXMLNode* psOperationsMetadata =
        CPLGetXMLNode(psRoot, "OperationsMetadata");
    if( !psOperationsMetadata )
    {
        return false;
    }

    CPLXMLNode* psChild = psOperationsMetadata->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "Constraint") == 0 &&
            strcmp(CPLGetXMLValue(psChild, "name", ""), "ImplementsStandardJoins") == 0)
        {
            if( !EQUAL(CPLGetXMLValue(psChild, "DefaultValue", ""), "TRUE") )
            {
                psChild = NULL;
                break;
            }
            break;
        }
        psChild = psChild->psNext;
    }
    if (!psChild)
    {
        CPLDebug("WFS", "No ImplementsStandardJoins support");
        return false;
    }
    bStandardJoinsWFS2 = true;
    return true;
}

/************************************************************************/
/*                      FindComparisonOperator()                        */
/************************************************************************/

static bool FindComparisonOperator( CPLXMLNode* psNode, const char* pszVal )
{
    CPLXMLNode* psChild = psNode->psChild;
    while(psChild)
    {
        if (psChild->eType == CXT_Element &&
            strcmp(psChild->pszValue, "ComparisonOperator") == 0)
        {
            if (strcmp(CPLGetXMLValue(psChild, NULL, ""), pszVal) == 0)
                return true;

            /* For WFS 2.0.0 */
            const char* pszName = CPLGetXMLValue(psChild, "name", NULL);
            if (pszName != NULL && STARTS_WITH(pszName, "PropertyIs") &&
                strcmp(pszName + 10, pszVal) == 0)
                return true;
        }
        psChild = psChild->psNext;
    }
    return false;
}

/************************************************************************/
/*                          LoadFromFile()                              */
/************************************************************************/

CPLXMLNode* OGRWFSDataSource::LoadFromFile( const char * pszFilename )
{
    VSIStatBufL sStatBuf;
    if (VSIStatExL( pszFilename, &sStatBuf, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) != 0 ||
        VSI_ISDIR(sStatBuf.st_mode))
        return NULL;

    VSILFILE *fp = VSIFOpenL( pszFilename, "rb" );

    if( fp == NULL )
        return NULL;

    char achHeader[1024] = {};
    const int nRead =
        static_cast<int>(VSIFReadL( achHeader, 1, sizeof(achHeader) - 1, fp ));
    if( nRead == 0 )
    {
        VSIFCloseL( fp );
        return NULL;
    }
    achHeader[nRead] = 0;

    if( !STARTS_WITH_CI(achHeader, "<OGRWFSDataSource>") &&
        strstr(achHeader,"<WFS_Capabilities") == NULL &&
        strstr(achHeader,"<wfs:WFS_Capabilities") == NULL)
    {
        VSIFCloseL( fp );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      It is the right file, now load the full XML definition.         */
/* -------------------------------------------------------------------- */
    VSIFSeekL( fp, 0, SEEK_END );
    const int nLen = (int) VSIFTellL( fp );
    VSIFSeekL( fp, 0, SEEK_SET );

    char* pszXML = (char *) VSI_MALLOC_VERBOSE(nLen+1);
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
        bUseFeatureId = true;
    }
    else if (strstr(pszXML, "deegree"))
    {
        bGmlObjectIdNeedsGMLPrefix = true;
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
    if( osTypeName.empty() )
        osTypeName = CPLURLGetValue(osURL, "TYPENAMES");
    osURL = CPLURLAddKVP(osURL, "TYPENAME", NULL);
    osURL = CPLURLAddKVP(osURL, "TYPENAMES", NULL);
    osURL = CPLURLAddKVP(osURL, "FILTER", NULL);
    osURL = CPLURLAddKVP(osURL, "PROPERTYNAME", NULL);
    osURL = CPLURLAddKVP(osURL, "MAXFEATURES", NULL);
    osURL = CPLURLAddKVP(osURL, "OUTPUTFORMAT", NULL);

    CPLDebug("WFS", "%s", osURL.c_str());

    CPLHTTPResult* psResult = HTTPFetch( osURL, NULL);
    if (psResult == NULL)
    {
        return NULL;
    }

    if (strstr((const char*)psResult->pabyData,
                                    "<ServiceExceptionReport") != NULL ||
        strstr((const char*)psResult->pabyData,
                                    "<ows:ExceptionReport") != NULL ||
        strstr((const char*)psResult->pabyData,
                                    "<ExceptionReport") != NULL)
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

int OGRWFSDataSource::Open( const char * pszFilename, int bUpdateIn,
                            char** papszOpenOptionsIn )

{
    bUpdate = CPL_TO_BOOL(bUpdateIn);
    CPLFree(pszName);
    pszName = CPLStrdup(pszFilename);

    CPLXMLNode* psWFSCapabilities = NULL;
    CPLXMLNode* psXML = LoadFromFile(pszFilename);
    CPLString osTypeName;
    const char* pszBaseURL = NULL;

    bEmptyAsNull = CPLFetchBool(papszOpenOptionsIn, "EMPTY_AS_NULL", true);

    if (psXML == NULL)
    {
        if (!STARTS_WITH_CI(pszFilename, "WFS:") &&
            FindSubStringInsensitive(pszFilename, "SERVICE=WFS") == NULL)
        {
            return FALSE;
        }

        pszBaseURL = CSLFetchNameValue(papszOpenOptionsIn, "URL");
        if( pszBaseURL == NULL )
        {
            pszBaseURL = pszFilename;
            if (STARTS_WITH_CI(pszFilename, "WFS:"))
                pszBaseURL += 4;
        }

        osBaseURL = pszBaseURL;

        if (!STARTS_WITH(pszBaseURL, "http://") &&
            !STARTS_WITH(pszBaseURL, "https://") &&
            !STARTS_WITH(pszBaseURL, "/vsimem/"))
            return FALSE;

        CPLString strOriginalTypeName = "";
        CPLHTTPResult* psResult = SendGetCapabilities(pszBaseURL, strOriginalTypeName);
        osTypeName = WFS_DecodeURL(strOriginalTypeName);
        if (psResult == NULL)
        {
            return FALSE;
        }

        if (strstr((const char*) psResult->pabyData, "CubeWerx"))
        {
            /* At least true for CubeWerx Suite 4.15.1 */
            bUseFeatureId = true;
        }
        else if (strstr((const char*) psResult->pabyData, "deegree"))
        {
            bGmlObjectIdNeedsGMLPrefix = true;
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
        const char *pszParm = CPLGetXMLValue( psRoot, "Timeout", NULL );
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

        pszParm = CPLGetXMLValue( psRoot, "COOKIE", NULL );
        if( pszParm )
            papszHttpOptions =
                CSLSetNameValue( papszHttpOptions,
                                "COOKIE", pszParm );

        pszParm = CPLGetXMLValue( psRoot, "Version", NULL );
        if( pszParm )
            osVersion = pszParm;

        pszParm = CPLGetXMLValue( psRoot, "PagingAllowed", NULL );
        if( pszParm )
            bPagingAllowed = CPLTestBool(pszParm);

        pszParm = CPLGetXMLValue( psRoot, "PageSize", NULL );
        if( pszParm )
        {
            nPageSize = atoi(pszParm);
            if (nPageSize <= 0)
                nPageSize = DEFAULT_PAGE_SIZE;
        }

        pszParm = CPLGetXMLValue( psRoot, "BaseStartIndex", NULL );
        if( pszParm )
            nBaseStartIndex = atoi(pszParm);

        CPLString strOriginalTypeName = CPLURLGetValue(pszBaseURL, "TYPENAME");
        if( strOriginalTypeName.empty() )
            strOriginalTypeName = CPLURLGetValue(pszBaseURL, "TYPENAMES");
        osTypeName = WFS_DecodeURL(strOriginalTypeName);

        psWFSCapabilities = WFSFindNode( psRoot, "WFS_Capabilities" );
        if (psWFSCapabilities == NULL)
        {
            CPLHTTPResult* psResult = SendGetCapabilities(pszBaseURL, strOriginalTypeName);
            osTypeName = WFS_DecodeURL(strOriginalTypeName);

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

            const bool bOK =
                CPL_TO_BOOL(CPLSerializeXMLTreeToFile(psXML, pszFilename));

            CPLDestroyXMLNode( psXML );
            CPLDestroyXMLNode( psXML2 );

            if( bOK )
                return Open(pszFilename, bUpdate, papszOpenOptionsIn);

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

    bInvertAxisOrderIfLatLong =
        CPLTestBool(CSLFetchNameValueDef(papszOpenOptionsIn,
            "INVERT_AXIS_ORDER_IF_LAT_LONG",
            CPLGetConfigOption("GML_INVERT_AXIS_ORDER_IF_LAT_LONG", "YES")));
    osConsiderEPSGAsURN =
        CSLFetchNameValueDef(papszOpenOptionsIn,
            "CONSIDER_EPSG_AS_URN",
            CPLGetConfigOption("GML_CONSIDER_EPSG_AS_URN", "AUTO"));
    bExposeGMLId =
        CPLTestBool(CSLFetchNameValueDef(papszOpenOptionsIn,
            "EXPOSE_GML_ID",
            CPLGetConfigOption("GML_EXPOSE_GML_ID", "YES")));

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
            if (!psFileXML) CPLDestroyXMLNode( psXML );
            CPLDestroyXMLNode( psStrippedXML );
            return FALSE;
        }

        osBaseURL = pszBaseURL;
    }

    pszBaseURL = NULL;

    for( int i=0; i < (int)(sizeof(asMetadata) / sizeof(asMetadata[0])); i++ )
    {
        const char* pszVal = CPLGetXMLValue( psWFSCapabilities, asMetadata[i].pszPath, NULL );
        if( pszVal )
            SetMetadataItem(asMetadata[i].pszMDI, pszVal);
    }

    if( osVersion.empty() )
        osVersion = CPLGetXMLValue(psWFSCapabilities, "version", "1.0.0");
    if( strcmp(osVersion.c_str(), "1.0.0") == 0 )
    {
        bUseFeatureId = true;
    }
    else
    {
        /* Some servers happen to support RESULTTYPE=hits in 1.0.0, but there */
        /* is no way to advertises this */
        if (atoi(osVersion) >= 2)
            bGetFeatureSupportHits = true;  /* WFS >= 2.0.0 supports hits */
        else
            bGetFeatureSupportHits = DetectIfGetFeatureSupportHits(psWFSCapabilities);
        bRequiresEnvelopeSpatialFilter =
            DetectRequiresEnvelopeSpatialFilter(psWFSCapabilities);
    }

    if ( atoi(osVersion) >= 2 )
    {
        CPLString osMaxFeatures = CPLURLGetValue(osBaseURL, "COUNT" );
        /* Ok, people are used to MAXFEATURES, so be nice to recognize it if it is used for WFS 2.0 ... */
        if (osMaxFeatures.empty() )
        {
            osMaxFeatures = CPLURLGetValue(osBaseURL, "MAXFEATURES");
            if( !osMaxFeatures.empty() &&
                CPLTestBool(CPLGetConfigOption("OGR_WFS_FIX_MAXFEATURES", "YES")) )
            {
                CPLDebug("WFS", "MAXFEATURES wrongly used for WFS 2.0. Using COUNT instead");
                osBaseURL = CPLURLAddKVP(osBaseURL, "MAXFEATURES", NULL);
                osBaseURL = CPLURLAddKVP(osBaseURL, "COUNT", osMaxFeatures);
            }
        }

        DetectSupportPagingWFS2(psWFSCapabilities);
        DetectSupportStandardJoinsWFS2(psWFSCapabilities);
    }

    DetectTransactionSupport(psWFSCapabilities);

    if( bUpdate && !bTransactionSupport )
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
        bHasMinOperators =
            CPLGetXMLNode(psFilterCap, "LogicalOperators") != NULL ||
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
            bHasMinOperators = false;
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

    /* Check if there are layer names whose identical except their prefix */
    std::set<CPLString> aosSetLayerNames;
    for( CPLXMLNode* psChildIter = psChild->psChild;
        psChildIter != NULL;
        psChildIter = psChildIter->psNext)
    {
        if (psChildIter->eType == CXT_Element &&
            strcmp(psChildIter->pszValue, "FeatureType") == 0)
        {
            const char* l_pszName = CPLGetXMLValue(psChildIter, "Name", NULL);
            if (l_pszName != NULL)
            {
                const char* pszShortName = strchr(l_pszName, ':');
                if (pszShortName)
                    l_pszName = pszShortName + 1;
                if (aosSetLayerNames.find(l_pszName) != aosSetLayerNames.end())
                {
                    bKeepLayerNamePrefix = true;
                    CPLDebug(
                        "WFS",
                        "At least 2 layers have names that are only "
                        "distinguishable by keeping the prefix");
                    break;
                }
                aosSetLayerNames.insert(l_pszName);
            }
        }
    }

    char** papszTypenames = NULL;
    if (!osTypeName.empty())
        papszTypenames = CSLTokenizeStringComplex( osTypeName, ",", FALSE, FALSE );

    for( CPLXMLNode* psChildIter = psChild->psChild;
         psChildIter != NULL;
         psChildIter = psChildIter->psNext )
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

            const char* l_pszName = CPLGetXMLValue(psChildIter, "Name", NULL);
            const char* pszTitle = CPLGetXMLValue(psChildIter, "Title", NULL);
            const char* pszAbstract = CPLGetXMLValue(psChildIter, "Abstract", NULL);
            if (l_pszName != NULL &&
                (papszTypenames == NULL ||
                 CSLFindString(papszTypenames, l_pszName) != -1))
            {
                const char* pszDefaultSRS =
                        CPLGetXMLValue(psChildIter, "DefaultSRS", NULL);
                if (pszDefaultSRS == NULL)
                    pszDefaultSRS = CPLGetXMLValue(psChildIter, "SRS", NULL);
                if (pszDefaultSRS == NULL)
                    pszDefaultSRS = CPLGetXMLValue(psChildIter, "DefaultCRS", NULL); /* WFS 2.0.0 */

                CPLXMLNode* psOutputFormats = CPLGetXMLNode(psChildIter, "OutputFormats");
                CPLString osOutputFormat;
                if (psOutputFormats)
                {
                    std::vector<CPLString> osFormats;
                    CPLXMLNode* psOutputFormatIter = psOutputFormats->psChild;
                    while(psOutputFormatIter)
                    {
                        if (psOutputFormatIter->eType == CXT_Element &&
                            EQUAL(psOutputFormatIter->pszValue, "Format") &&
                            psOutputFormatIter->psChild != NULL &&
                            psOutputFormatIter->psChild->eType == CXT_Text)
                        {
                            osFormats.push_back(psOutputFormatIter->psChild->pszValue);
                        }
                        psOutputFormatIter = psOutputFormatIter->psNext;
                    }

                    if (strcmp(osVersion.c_str(), "1.1.0") == 0 && !osFormats.empty())
                    {
                        bool bFoundGML31 = false;
                        for(size_t i=0;i<osFormats.size();i++)
                        {
                            if (strstr(osFormats[i].c_str(), "3.1") != NULL)
                            {
                                bFoundGML31 = true;
                                break;
                            }
                        }

                        /* If we didn't find any mention to GML 3.1, then arbitrarily */
                        /* use the first output format */
                        if( !bFoundGML31 )
                            osOutputFormat = osFormats[0].c_str();
                    }
                }

                OGRSpatialReference* poSRS = NULL;
                bool bAxisOrderAlreadyInverted = false;

                /* If a SRSNAME parameter has been encoded in the URL, use it as the SRS */
                CPLString osSRSName = CPLURLGetValue(osBaseURL, "SRSNAME");
                if (!osSRSName.empty())
                {
                    pszDefaultSRS = osSRSName.c_str();
                }

                if (pszDefaultSRS)
                {
                    OGRSpatialReference oSRS;
                    if (oSRS.SetFromUserInput(pszDefaultSRS) == OGRERR_NONE)
                    {
                        poSRS = oSRS.Clone();
                        if( bInvertAxisOrderIfLatLong &&
                            GML_IsSRSLatLongOrder(pszDefaultSRS) )
                        {
                            bAxisOrderAlreadyInverted = true;

                            OGR_SRSNode *poGEOGCS =
                                            poSRS->GetAttrNode( "GEOGCS" );
                            if( poGEOGCS != NULL )
                                poGEOGCS->StripNodes( "AXIS" );

                            OGR_SRSNode *poPROJCS = poSRS->GetAttrNode( "PROJCS" );
                            if (poPROJCS != NULL && poSRS->EPSGTreatsAsNorthingEasting())
                                poPROJCS->StripNodes( "AXIS" );
                        }
                    }
                }

                CPLXMLNode* psBBox = NULL;
                CPLXMLNode* psLatLongBBox = NULL;
                /* bool bFoundBBox = false; */
                double dfMinX = 0.0;
                double dfMinY = 0.0;
                double dfMaxX = 0.0;
                double dfMaxY = 0.0;
                if ((psBBox = CPLGetXMLNode(psChildIter, "WGS84BoundingBox")) != NULL)
                {
                    const char* pszLC = CPLGetXMLValue(psBBox, "LowerCorner", NULL);
                    const char* pszUC = CPLGetXMLValue(psBBox, "UpperCorner", NULL);
                    if (pszLC != NULL && pszUC != NULL)
                    {
                        CPLString osConcat(pszLC);
                        osConcat += " ";
                        osConcat += pszUC;
                        char **papszTokens = CSLTokenizeStringComplex(
                            osConcat, " ,", FALSE, FALSE );
                        if (CSLCount(papszTokens) == 4)
                        {
                            // bFoundBBox = true;
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
                        // bFoundBBox = true;
                        dfMinX = CPLAtof(pszMinX);
                        dfMinY = CPLAtof(pszMinY);
                        dfMaxX = CPLAtof(pszMaxX);
                        dfMaxY = CPLAtof(pszMaxY);
                    }
                }

                char* pszCSVEscaped = CPLEscapeString(l_pszName, -1, CPLES_CSV);
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
                    osBaseURL, l_pszName, pszNS, pszNSVal);
                if (!osOutputFormat.empty() )
                    poLayer->SetRequiredOutputFormat(osOutputFormat);

                if( pszTitle )
                    poLayer->SetMetadataItem("TITLE", pszTitle);
                if( pszAbstract )
                    poLayer->SetMetadataItem("ABSTRACT", pszAbstract);
                CPLXMLNode* psKeywords = CPLGetXMLNode(psChildIter, "Keywords");
                if( psKeywords )
                {
                    int nKeywordCounter = 1;
                    for( CPLXMLNode* psKeyword = psKeywords->psChild;
                         psKeyword != NULL; psKeyword = psKeyword->psNext )
                    {
                        if( psKeyword->eType == CXT_Element && psKeyword->psChild != NULL )
                        {
                            poLayer->SetMetadataItem(CPLSPrintf("KEYWORD_%d", nKeywordCounter),
                                                     psKeyword->psChild->pszValue);
                            nKeywordCounter ++;
                        }
                        else if( psKeyword->eType == CXT_Text )
                        {
                            poLayer->SetMetadataItem("KEYWORDS",
                                                     psKeyword->pszValue);
                        }
                    }
                }

                if (poSRS)
                {
                    char* pszProj4 = NULL;
                    if (poSRS->exportToProj4(&pszProj4) == OGRERR_NONE)
                    {
                        /* See http://trac.osgeo.org/gdal/ticket/4041 */
                        const bool bTrustBounds =
                            CPLFetchBool(
                                papszOpenOptionsIn,
                                "TRUST_CAPABILITIES_BOUNDS",
                                CPLTestBool(CPLGetConfigOption(
                                    "OGR_WFS_TRUST_CAPABILITIES_BOUNDS",
                                    "FALSE")));

                        if (((bTrustBounds || (dfMinX == -180 && dfMinY == -90 && dfMaxX == 180 && dfMaxY == 90)) &&
                            (strcmp(pszProj4, "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs ") == 0 ||
                             strcmp(pszProj4, "+proj=longlat +datum=WGS84 +no_defs ") == 0)) ||
                            strcmp(pszDefaultSRS, "urn:ogc:def:crs:OGC:1.3:CRS84") == 0)
                        {
                            poLayer->SetExtents(dfMinX, dfMinY, dfMaxX, dfMaxY);
                        }

                        else if( bTrustBounds )
                        {
                            OGRSpatialReference oWGS84;
                            oWGS84.SetWellKnownGeogCS("WGS84");
                            CPLPushErrorHandler(CPLQuietErrorHandler);
                            OGRCoordinateTransformation* poCT =
                                OGRCreateCoordinateTransformation(&oWGS84,
                                                                  poSRS);
                            if( poCT )
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
                                    dfMinX = std::min(dfMinX, dfURX);
                                    dfMinX = std::min(dfMinX, dfLLX);
                                    dfMinX = std::min(dfMinX, dfLRX);

                                    dfMinY = dfULY;
                                    dfMinY = std::min(dfMinY, dfURY);
                                    dfMinY = std::min(dfMinY, dfLLY);
                                    dfMinY = std::min(dfMinY, dfLRY);

                                    dfMaxX = dfULX;
                                    dfMaxX = std::max(dfMaxX, dfURX);
                                    dfMaxX = std::max(dfMaxX, dfLLX);
                                    dfMaxX = std::max(dfMaxX, dfLRX);

                                    dfMaxY = dfULY;
                                    dfMaxY = std::max(dfMaxY, dfURY);
                                    dfMaxY = std::max(dfMaxY, dfLLY);
                                    dfMaxY = std::max(dfMaxY, dfLRY);

                                    poLayer->SetExtents(dfMinX, dfMinY, dfMaxX, dfMaxY);
                                }
                            }
                            delete poCT;
                            CPLPopErrorHandler();
                            CPLErrorReset();
                        }
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
                            strcmp(CPLGetXMLValue(psIter, "name", ""), l_pszName) == 0)
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

    CSLDestroy(papszTypenames);

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
    if( !bLoadMultipleLayerDefn )
        return;

    if (aoSetAlreadyTriedLayers.find(pszLayerName) != aoSetAlreadyTriedLayers.end())
        return;

    char* pszPrefix = CPLStrdup(pszLayerName);
    char* pszColumn = strchr(pszPrefix, ':');
    if (pszColumn)
        *pszColumn = 0;
    else
        *pszPrefix = 0;

    OGRWFSLayer* poRefLayer = (OGRWFSLayer*)GetLayerByName(pszLayerName);
    if (poRefLayer == NULL)
        return;

    const char* pszRequiredOutputFormat = poRefLayer->GetRequiredOutputFormat();

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
            const char* l_pszName = papoLayers[i]->GetName();
            if (((pszPrefix[0] == 0 && strchr(l_pszName, ':') == NULL) ||
                (pszPrefix[0] != 0 && strncmp(l_pszName, pszPrefix, strlen(pszPrefix)) == 0 &&
                 l_pszName[strlen(pszPrefix)] == ':')) &&
                ((pszRequiredOutputFormat == NULL && papoLayers[i]->GetRequiredOutputFormat() == NULL) ||
                 (pszRequiredOutputFormat != NULL && papoLayers[i]->GetRequiredOutputFormat() != NULL &&
                  strcmp(pszRequiredOutputFormat, papoLayers[i]->GetRequiredOutputFormat()) == 0)))
            {
                if (aoSetAlreadyTriedLayers.find(l_pszName) != aoSetAlreadyTriedLayers.end())
                    continue;
                aoSetAlreadyTriedLayers.insert(l_pszName);

#if USE_GET_FOR_DESCRIBE_FEATURE_TYPE == 1
                if (nLayersToFetch > 0)
                    osLayerToFetch += ",";
                osLayerToFetch += papoLayers[i]->GetName();
#else
                osTypeNameToPost += "  <TypeName>";
                osTypeNameToPost += l_pszName;
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
    osURL = CPLURLAddKVP(osURL, "TYPENAME", WFS_EscapeURL(osLayerToFetch));
    osURL = CPLURLAddKVP(osURL, "PROPERTYNAME", NULL);
    osURL = CPLURLAddKVP(osURL, "MAXFEATURES", NULL);
    osURL = CPLURLAddKVP(osURL, "FILTER", NULL);
    osURL = CPLURLAddKVP(osURL, "OUTPUTFORMAT", pszRequiredOutputFormat ? WFS_EscapeURL(pszRequiredOutputFormat).c_str() : NULL);

    if (pszNS && GetNeedNAMESPACE())
    {
        /* Older Deegree version require NAMESPACE */
        /* This has been now corrected */
        CPLString osValue("xmlns(");
        osValue += pszNS;
        osValue += "=";
        osValue += pszNSVal;
        osValue += ")";
        osURL = CPLURLAddKVP(osURL, "NAMESPACE", WFS_EscapeURL(osValue));
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
    const char* pszRequiredOutputFormat = poRefLayer->GetRequiredOutputFormat();
    if (pszRequiredOutputFormat)
    {
        osPost += "\n";
        osPost += "                 outputFormat=\"";
        osPost += pszRequiredOutputFormat;
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
        bLoadMultipleLayerDefn = false;
        return;
    }

    if (strstr((const char*)psResult->pabyData, "<ServiceExceptionReport") != NULL)
    {
        if( IsOldDeegree((const char*)psResult->pabyData) )
        {
            /* just silently forgive */
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Error returned by server : %s",
                    psResult->pabyData);
        }
        CPLHTTPDestroyResult(psResult);
        bLoadMultipleLayerDefn = false;
        return;
    }

    CPLXMLNode* psXML = CPLParseXMLString( (const char*) psResult->pabyData );
    if (psXML == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid XML content : %s",
                psResult->pabyData);
        CPLHTTPDestroyResult(psResult);
        bLoadMultipleLayerDefn = false;
        return;
    }
    CPLHTTPDestroyResult(psResult);

    CPLXMLNode* psSchema = WFSFindNode(psXML, "schema");
    if (psSchema == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find <Schema>");
        CPLDestroyXMLNode( psXML );
        bLoadMultipleLayerDefn = false;
        return;
    }

    CPLString osTmpFileName;

    osTmpFileName = CPLSPrintf("/vsimem/tempwfs_%p/file.xsd", this);
    CPLSerializeXMLTreeToFile(psSchema, osTmpFileName);

    std::vector<GMLFeatureClass*> aosClasses;
    bool bFullyUnderstood = false;
    GMLParseXSD( osTmpFileName, aosClasses, bFullyUnderstood );

    int nLayersFound = 0;
    if (!(int)aosClasses.empty())
    {
        std::vector<GMLFeatureClass*>::const_iterator oIter = aosClasses.begin();
        std::vector<GMLFeatureClass*>::const_iterator oEndIter = aosClasses.end();
        while (oIter != oEndIter)
        {
            GMLFeatureClass* poClass = *oIter;
            ++oIter;

            OGRWFSLayer* poLayer = NULL;

            if( bKeepLayerNamePrefix && pszNS != NULL &&
                strchr(poClass->GetName(), ':') == NULL )
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
                    nLayersFound ++;

                    CPLXMLNode* psSchemaForLayer = CPLCloneXMLTree(psSchema);
                    CPLStripXMLNamespace( psSchemaForLayer, NULL, TRUE );
                    CPLXMLNode* psIter = psSchemaForLayer->psChild;
                    bool bHasAlreadyImportedGML = false;
                    bool bFoundComplexType = false;
                    bool bFoundElement = false;
                    while(psIter != NULL)
                    {
                        CPLXMLNode* psIterNext = psIter->psNext;
                        if (psIter->eType == CXT_Element &&
                            strcmp(psIter->pszValue,"complexType") == 0)
                        {
                            const char* l_pszName = CPLGetXMLValue(psIter, "name", "");
                            CPLString osExpectedName(poLayer->GetShortName());
                            osExpectedName += "Type";
                            CPLString osExpectedName2(poLayer->GetShortName());
                            osExpectedName2 += "_Type";
                            if (strcmp(l_pszName, osExpectedName) == 0 ||
                                strcmp(l_pszName, osExpectedName2) == 0 ||
                                strcmp(l_pszName, poLayer->GetShortName()) == 0)
                            {
                                bFoundComplexType = true;
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
                            const char* l_pszName = CPLGetXMLValue(psIter, "name", "");
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
                                bFoundElement = true;
                            }
                            else if (*pszType == '\0' &&
                                     CPLGetXMLNode(psIter, "complexType") != NULL &&
                                     (strcmp(l_pszName, osExpectedName) == 0 ||
                                      strcmp(l_pszName, osExpectedName2) == 0 ||
                                      strcmp(l_pszName, poLayer->GetShortName()) == 0) )
                            {
                                bFoundElement = true;
                                bFoundComplexType = true;
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
                            if( bHasAlreadyImportedGML )
                            {
                                CPLRemoveXMLChild( psSchemaForLayer, psIter );
                                CPLDestroyXMLNode(psIter);
                            }
                            else
                            {
                                bHasAlreadyImportedGML = true;
                            }
                        }
                        psIter = psIterNext;
                    }

                    if( bFoundComplexType && bFoundElement )
                    {
                        OGRFeatureDefn* poSrcFDefn
                            = poLayer->ParseSchema(psSchemaForLayer);
                        if (poSrcFDefn)
                        {
                            poLayer->BuildLayerDefn(poSrcFDefn);
                            SaveLayerSchema(poLayer->GetName(),
                                            psSchemaForLayer);
                        }
                    }

                    CPLDestroyXMLNode(psSchemaForLayer);
                }
                else
                {
                    CPLDebug( "WFS",
                              "Found several time schema for layer %s in "
                              "server response. Should not happen",
                             poClass->GetName());
                }
            }
            delete poClass;
        }
    }

    if (nLayersFound != nLayersToFetch)
    {
        CPLDebug( "WFS",
                  "Turn off loading of multiple layer definitions at a "
                  "single time");
        bLoadMultipleLayerDefn = false;
    }

    VSIUnlink(osTmpFileName);

    CPLDestroyXMLNode( psXML );
}

/************************************************************************/
/*                         SaveLayerSchema()                            */
/************************************************************************/

void OGRWFSDataSource::SaveLayerSchema( const char* pszLayerName,
                                        CPLXMLNode* psSchema )
{
    if (psFileXML != NULL)
    {
        bRewriteFile = true;
        CPLXMLNode* psLayerNode =
            CPLCreateXMLNode(NULL, CXT_Element, "OGRWFSLayer");
        CPLSetXMLValue(psLayerNode, "#name", pszLayerName);
        CPLAddXMLChild(psLayerNode, CPLCloneXMLTree(psSchema));
        CPLAddXMLChild(psFileXML, psLayerNode);
    }
}

/************************************************************************/
/*                           IsOldDeegree()                             */
/************************************************************************/

bool OGRWFSDataSource::IsOldDeegree(const char* pszErrorString)
{
    if( !bNeedNAMESPACE &&
        strstr(pszErrorString,
               "Invalid \"TYPENAME\" parameter. "
               "No binding for prefix") != NULL )
    {
        bNeedNAMESPACE = true;
        return true;
    }
    return false;
}

/************************************************************************/
/*                         WFS_EscapeURL()                              */
/************************************************************************/

CPLString WFS_EscapeURL(const char* pszURL)
{
    CPLString osEscapedURL;

    /* Difference with CPLEscapeString(, CPLES_URL) : we do not escape */
    /* colon (:) or comma (,). Causes problems with servers such as http://www.mapinfo.com/miwfs? */

    for( int i = 0; pszURL[i] != '\0' ; i++ )
    {
        char ch = pszURL[i];
        if( (ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= '0' && ch <= '9')
            || ch == '_' || ch == '.'
            || ch == ':' || ch == ',' )
        {
            osEscapedURL += ch;
        }
        else
        {
            char szPercentEncoded[10];
            snprintf( szPercentEncoded, sizeof(szPercentEncoded),
                      "%%%02X", ((unsigned char*)pszURL)[i] );
            osEscapedURL += szPercentEncoded;
        }
    }

    return osEscapedURL;
}

/************************************************************************/
/*                         WFS_DecodeURL()                              */
/************************************************************************/

CPLString WFS_DecodeURL(const CPLString &osSrc)
{
    CPLString ret;
    for( size_t i=0; i<osSrc.length(); i++ )
    {
        if (osSrc[i]=='%' && i+2 < osSrc.length())
        {
            unsigned int ii = 0;
            sscanf(osSrc.substr(i+1,2).c_str(), "%x", &ii);
            char ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        }
        else
        {
            ret+=osSrc[i];
        }
    }
    return ret;
}

/************************************************************************/
/*                            HTTPFetch()                               */
/************************************************************************/

CPLHTTPResult* OGRWFSDataSource::HTTPFetch( const char* pszURL, char** papszOptions )
{
    char** papszNewOptions = CSLDuplicate(papszOptions);
    if( bUseHttp10 )
        papszNewOptions = CSLAddNameValue(papszNewOptions, "HTTP_VERSION", "1.0");
    if (papszHttpOptions)
        papszNewOptions = CSLMerge(papszNewOptions, papszHttpOptions);
    CPLHTTPResult* psResult = CPLHTTPFetch( pszURL, papszNewOptions );
    CSLDestroy(papszNewOptions);

    if (psResult == NULL)
    {
        return NULL;
    }
    if (psResult->nStatus != 0 || psResult->pszErrBuf != NULL)
    {
        // A few buggy servers return chunked data with erroneous
        // remaining bytes value curl does not like this. Retry with
        // HTTP 1.0 protocol instead that does not support chunked
        // data.
        if( psResult->pszErrBuf &&
            strstr(psResult->pszErrBuf,
                   "transfer closed with outstanding read data remaining") &&
            !bUseHttp10 )
        {
            CPLDebug("WFS", "Probably buggy remote server. Retrying with HTTP 1.0 protocol");
            bUseHttp10 = true;
            CPLHTTPDestroyResult(psResult);
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
    swq_select_parse_options oParseOptions;
    oParseOptions.poCustomFuncRegistrar = WFSGetCustomFuncRegistrar();

/* -------------------------------------------------------------------- */
/*      Use generic implementation for recognized dialects              */
/* -------------------------------------------------------------------- */
    if( IsGenericSQLDialect(pszDialect) )
    {
        OGRLayer* poResLayer = GDALDataset::ExecuteSQL( pszSQLCommand,
                                                        poSpatialFilter,
                                                        pszDialect,
                                                        &oParseOptions );
        oMap[poResLayer] = NULL;
        return poResLayer;
    }

/* -------------------------------------------------------------------- */
/*      Deal with "SELECT _LAST_INSERTED_FIDS_ FROM layername" statement */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "SELECT _LAST_INSERTED_FIDS_ FROM ") )
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

        GDALDriver* poMEMDrv = OGRSFDriverRegistrar::GetRegistrar()->GetDriverByName("Memory");
        if (poMEMDrv == NULL)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot load 'Memory' driver");
            return NULL;
        }

        GDALDataset* poMEMDS = poMEMDrv->Create("dummy_name", 0, 0, 0, GDT_Unknown, NULL);
        OGRLayer* poMEMLayer = poMEMDS->CreateLayer("FID_LIST", NULL, wkbNone, NULL);
        OGRFieldDefn oFDefn("gml_id", OFTString);
        poMEMLayer->CreateField(&oFDefn);

        const std::vector<CPLString>& aosFIDList = poLayer->GetLastInsertedFIDList();
        std::vector<CPLString>::const_iterator oIter = aosFIDList.begin();
        std::vector<CPLString>::const_iterator oEndIter = aosFIDList.end();
        while (oIter != oEndIter)
        {
            const CPLString& osFID = *oIter;
            OGRFeature* poFeature = new OGRFeature(poMEMLayer->GetLayerDefn());
            poFeature->SetField(0, osFID);
            CPL_IGNORE_RET_VAL(poMEMLayer->CreateFeature(poFeature));
            delete poFeature;
            ++oIter;
        }

        OGRLayer* poResLayer = new OGRWFSWrappedResultLayer(poMEMDS, poMEMLayer);
        oMap[poResLayer] = NULL;
        return poResLayer;
    }

/* -------------------------------------------------------------------- */
/*      Deal with "DELETE FROM layer_name WHERE expression" statement   */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH_CI(pszSQLCommand, "DELETE FROM ") )
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

        while(*pszIter == ' ')
            pszIter ++;
        if (!STARTS_WITH_CI(pszIter, "WHERE "))
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
        swq_expr_node* poNode = (swq_expr_node*) oQuery.GetSWQExpr();
        poNode->ReplaceBetweenByGEAndLERecurse();
        CPLString osOGCFilter = WFS_TurnSQLFilterToOGCFilter(
            poNode,
            NULL,
            poLayer->GetLayerDefn(),
            nVersion,
            bPropertyIsNotEqualToSupported,
            bUseFeatureId,
            bGmlObjectIdNeedsGMLPrefix,
            "",
            &bNeedsNullCheck);
        if (bNeedsNullCheck && !HasNullCheck())
            osOGCFilter = "";

        if (osOGCFilter.empty())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot convert WHERE clause into a OGC filter");
            return NULL;
        }

        poLayer->DeleteFromFilter(osOGCFilter);

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Deal with "SELECT xxxx ORDER BY" statement                      */
/* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszSQLCommand, "SELECT"))
    {
        swq_select* psSelectInfo = new swq_select();
        if( psSelectInfo->preparse( pszSQLCommand, TRUE ) != CE_None )
        {
            delete psSelectInfo;
            return NULL;
        }
        int iLayer = 0;
        if( strcmp(GetVersion(),"1.0.0") != 0 &&
            psSelectInfo->table_count == 1 &&
            psSelectInfo->table_defs[0].data_source == NULL &&
            (iLayer = GetLayerIndex( psSelectInfo->table_defs[0].table_name )) >= 0 &&
            psSelectInfo->join_count == 0 &&
            psSelectInfo->order_specs > 0 &&
            psSelectInfo->poOtherSelect == NULL )
        {
            OGRWFSLayer* poSrcLayer = papoLayers[iLayer];
            std::vector<OGRWFSSortDesc> aoSortColumns;
            int i = 0;  // Used after for.
            for( ; i < psSelectInfo->order_specs; i++ )
            {
                int nFieldIndex = poSrcLayer->GetLayerDefn()->GetFieldIndex(
                                        psSelectInfo->order_defs[i].field_name);
                if (poSrcLayer->HasGotApproximateLayerDefn() || nFieldIndex < 0)
                    break;

                /* Make sure to have the right case */
                const char* pszFieldName = poSrcLayer->GetLayerDefn()->
                    GetFieldDefn(nFieldIndex)->GetNameRef();

                OGRWFSSortDesc oSortDesc(pszFieldName,
                                    psSelectInfo->order_defs[i].ascending_flag);
                aoSortColumns.push_back(oSortDesc);
            }

            if( i == psSelectInfo->order_specs )
            {
                OGRWFSLayer* poDupLayer = poSrcLayer->Clone();

                poDupLayer->SetOrderBy(aoSortColumns);
                int nBackup = psSelectInfo->order_specs;
                psSelectInfo->order_specs = 0;
                char* pszSQLWithoutOrderBy = psSelectInfo->Unparse();
                CPLDebug("WFS", "SQL without ORDER BY: %s", pszSQLWithoutOrderBy);
                psSelectInfo->order_specs = nBackup;
                delete psSelectInfo;
                psSelectInfo = NULL;

                /* Just set poDupLayer in the papoLayers for the time of the */
                /* base ExecuteSQL(), so that the OGRGenSQLResultsLayer references */
                /* that temporary layer */
                papoLayers[iLayer] = poDupLayer;

                OGRLayer* poResLayer = GDALDataset::ExecuteSQL( pszSQLWithoutOrderBy,
                                                                poSpatialFilter,
                                                                pszDialect,
                                                                &oParseOptions );
                papoLayers[iLayer] = poSrcLayer;

                CPLFree(pszSQLWithoutOrderBy);

                if (poResLayer != NULL)
                    oMap[poResLayer] = poDupLayer;
                else
                    delete poDupLayer;
                return poResLayer;
            }
        }
        else if( bStandardJoinsWFS2 &&
                 psSelectInfo->join_count > 0 &&
                 psSelectInfo->poOtherSelect == NULL )
        {
            // Just to make sure everything is valid, but we won't use
            // that one as we want to run the join on server-side
            oParseOptions.bAllowFieldsInSecondaryTablesInWhere = TRUE;
            oParseOptions.bAddSecondaryTablesGeometryFields = TRUE;
            oParseOptions.bAlwaysPrefixWithTableName = TRUE;
            oParseOptions.bAllowDistinctOnGeometryField = TRUE;
            oParseOptions.bAllowDistinctOnMultipleFields = TRUE;
            GDALSQLParseInfo* psParseInfo = BuildParseInfo(psSelectInfo,
                                                           &oParseOptions);
            oParseOptions.bAllowFieldsInSecondaryTablesInWhere = FALSE;
            oParseOptions.bAddSecondaryTablesGeometryFields = FALSE;
            oParseOptions.bAlwaysPrefixWithTableName = FALSE;
            oParseOptions.bAllowDistinctOnGeometryField = FALSE;
            oParseOptions.bAllowDistinctOnMultipleFields = FALSE;
            const bool bOK = psParseInfo != NULL;
            DestroyParseInfo(psParseInfo);

            OGRLayer* poResLayer = NULL;
            if( bOK )
            {
                poResLayer = OGRWFSJoinLayer::Build(this, psSelectInfo);
                oMap[poResLayer] = NULL;
            }

            delete psSelectInfo;
            return poResLayer;
        }

        delete psSelectInfo;
    }

    OGRLayer* poResLayer = OGRDataSource::ExecuteSQL( pszSQLCommand,
                                                      poSpatialFilter,
                                                      pszDialect,
                                                      &oParseOptions );
    oMap[poResLayer] = NULL;
    return poResLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRWFSDataSource::ReleaseResultSet( OGRLayer * poResultsSet )
{
    if (poResultsSet == NULL)
        return;

    std::map<OGRLayer*, OGRLayer*>::iterator oIter = oMap.find(poResultsSet);
    if (oIter != oMap.end())
    {
        /* Destroy first the result layer, because it still references */
        /* the poDupLayer (oIter->second) */
        delete poResultsSet;

        delete oIter->second;
        oMap.erase(oIter);
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Trying to destroy an invalid result set !");
    }
}
