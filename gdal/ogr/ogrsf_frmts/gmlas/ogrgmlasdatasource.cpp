/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

// Must be first for DEBUG_BOOL case
#include "ogr_gmlas.h"

#include "ogr_mem.h"
#include "cpl_sha256.h"

#include <algorithm>

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRGMLASDataSource()                        */
/************************************************************************/

OGRGMLASDataSource::OGRGMLASDataSource()
{
    OGRInitializeXerces();

    m_fpGML = NULL;
    m_fpGMLParser = NULL;
    m_bLayerInitFinished = false;
    m_bValidate = false;
    m_bSchemaFullChecking = false;
    m_bHandleMultipleImports = false;
    m_bRemoveUnusedLayers = false;
    m_bRemoveUnusedFields = false;
    m_bFirstPassDone = false;
    m_eSwapCoordinates = GMLAS_SWAP_AUTO;
    m_nFileSize = 0;
    m_poReader = NULL;
    m_bEndOfReaderLayers = false;
    m_nCurMetadataLayerIdx = -1;
    m_poFieldsMetadataLayer = new OGRMemLayer
                                    (szOGR_FIELDS_METADATA, NULL, wkbNone );
    {
        OGRFieldDefn oFieldDefn(szLAYER_NAME, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_INDEX, OFTInteger);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_NAME, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_XPATH, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_TYPE, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_IS_LIST, OFTInteger);
        oFieldDefn.SetSubType(OFSTBoolean);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_MIN_OCCURS, OFTInteger);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_MAX_OCCURS, OFTInteger);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_REPETITION_ON_SEQUENCE, OFTInteger);
        oFieldDefn.SetSubType(OFSTBoolean);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_DEFAULT_VALUE, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_FIXED_VALUE, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_CATEGORY, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_RELATED_LAYER, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_JUNCTION_LAYER, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szFIELD_DOCUMENTATION, OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }

    m_poLayersMetadataLayer = new OGRMemLayer
                                    (szOGR_LAYERS_METADATA, NULL, wkbNone );
    {
        OGRFieldDefn oFieldDefn(szLAYER_NAME, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_XPATH, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_CATEGORY, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_PKID_NAME, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_PARENT_PKID_NAME, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szLAYER_DOCUMENTATION, OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }

    m_poRelationshipsLayer = new OGRMemLayer(szOGR_LAYER_RELATIONSHIPS,
                                             NULL, wkbNone );
    {
        OGRFieldDefn oFieldDefn(szPARENT_LAYER, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szPARENT_PKID, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szPARENT_ELEMENT_NAME, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szCHILD_LAYER, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szCHILD_PKID, OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    m_poOtherMetadataLayer = new OGRMemLayer(szOGR_OTHER_METADATA,
                                             NULL, wkbNone );
    {
        OGRFieldDefn oFieldDefn(szKEY, OFTString);
        m_poOtherMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn(szVALUE, OFTString);
        m_poOtherMetadataLayer->CreateField(&oFieldDefn);
    }
}

/************************************************************************/
/*                         ~OGRGMLASDataSource()                        */
/************************************************************************/

OGRGMLASDataSource::~OGRGMLASDataSource()
{
    for(size_t i=0;i<m_apoLayers.size();i++)
        delete m_apoLayers[i];
    delete m_poFieldsMetadataLayer;
    delete m_poLayersMetadataLayer;
    delete m_poRelationshipsLayer;
    delete m_poOtherMetadataLayer;
    if( m_fpGML != NULL )
        VSIFCloseL(m_fpGML);
    if( m_fpGMLParser != NULL )
        VSIFCloseL(m_fpGMLParser);
    delete m_poReader;

    OGRDeinitializeXerces();
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int         OGRGMLASDataSource::GetLayerCount()
{
    return static_cast<int>(m_apoLayers.size() + m_apoRequestedMetadataLayers.size());
}

/************************************************************************/
/*                                GetLayer()                            */
/************************************************************************/

OGRLayer    *OGRGMLASDataSource::GetLayer(int i)
{
    const int nBaseLayers = static_cast<int>(m_apoLayers.size());
    if( i >= nBaseLayers )
    {
        if( i - nBaseLayers < static_cast<int>(m_apoRequestedMetadataLayers.size()) )
            return m_apoRequestedMetadataLayers[i - nBaseLayers];
    }

    if( i < 0 || i >= nBaseLayers )
        return NULL;
    return m_apoLayers[i];
}

/************************************************************************/
/*                             GetLayerByName()                         */
/************************************************************************/

OGRLayer    *OGRGMLASDataSource::GetLayerByName(const char* pszName)
{
    OGRLayer* poLayer = GDALDataset::GetLayerByName(pszName);
    if( poLayer )
        return poLayer;

    OGRLayer* apoLayers[4];
    apoLayers[0] = m_poFieldsMetadataLayer;
    apoLayers[1] = m_poLayersMetadataLayer;
    apoLayers[2] = m_poRelationshipsLayer;
    apoLayers[3] = m_poOtherMetadataLayer;
    for( size_t i=0; i < CPL_ARRAYSIZE(apoLayers); ++i)
    {
        if( EQUAL(pszName, apoLayers[i]->GetName()) )
        {
            if ( std::find(m_apoRequestedMetadataLayers.begin(),
                        m_apoRequestedMetadataLayers.end(),
                        apoLayers[i]) == m_apoRequestedMetadataLayers.end() )
            {
                m_apoRequestedMetadataLayers.push_back(apoLayers[i]);
            }
            return apoLayers[i];
        }
    }

    return NULL;
}

/************************************************************************/
/*                           TranslateClasses()                         */
/************************************************************************/

void OGRGMLASDataSource::TranslateClasses( OGRGMLASLayer* poParentLayer,
                                           const GMLASFeatureClass& oFC )
{
    const std::vector<GMLASFeatureClass>& aoClasses = oFC.GetNestedClasses();

    //CPLDebug("GMLAS", "TranslateClasses(%s,%s)",
    //         oFC.GetName().c_str(), oFC.GetXPath().c_str());

    OGRGMLASLayer* poLayer = new OGRGMLASLayer(this, oFC, poParentLayer,
                                               m_oConf.m_bAlwaysGenerateOGRId);
    m_apoLayers.push_back(poLayer);

    for( size_t i=0; i<aoClasses.size(); ++i )
    {
        TranslateClasses( poLayer, aoClasses[i] );
    }
}

/************************************************************************/
/*                         GMLASGuessXSDFilename                        */
/************************************************************************/

class GMLASGuessXSDFilename : public DefaultHandler
{
            std::vector<PairURIFilename>  m_aoFilenames;
            int         m_nStartElementCounter;
            bool        m_bFinish;

    public:
                        GMLASGuessXSDFilename();

                        virtual ~GMLASGuessXSDFilename() {}

        std::vector<PairURIFilename> Guess(const CPLString& osFilename,
                                     VSILFILE* fp);

        virtual void startElement(
            const   XMLCh* const    uri,
            const   XMLCh* const    localname,
            const   XMLCh* const    qname,
            const   Attributes& attrs
        ) override;
};

/************************************************************************/
/*                          GMLASGuessXSDFilename()                     */
/************************************************************************/

GMLASGuessXSDFilename::GMLASGuessXSDFilename()
    : m_nStartElementCounter(0),
        m_bFinish(false)
{
}

/************************************************************************/
/*                               Guess()                                */
/************************************************************************/

std::vector<PairURIFilename>
        GMLASGuessXSDFilename::Guess(const CPLString& osFilename,
                                                    VSILFILE* fp)
{
    SAX2XMLReader* poSAXReader = XMLReaderFactory::createXMLReader();

    poSAXReader->setFeature (XMLUni::fgSAX2CoreNameSpaces, true);
    poSAXReader->setFeature (XMLUni::fgSAX2CoreNameSpacePrefixes, true);

    poSAXReader->setContentHandler( this );
    poSAXReader->setLexicalHandler( this );
    poSAXReader->setDTDHandler( this );

    poSAXReader->setFeature (XMLUni::fgXercesLoadSchema, false);

    GMLASErrorHandler oErrorHandler;
    poSAXReader->setErrorHandler(&oErrorHandler);

    GMLASInputSource* poIS = new GMLASInputSource(osFilename, fp, false);

    try
    {
        XMLPScanToken     oToFill;
        if( poSAXReader->parseFirst( *poIS, oToFill ) )
        {
            while( !m_bFinish &&
                   poSAXReader->parseNext( oToFill ) )
            {
                // do nothing
            }
        }
    }
    catch (const XMLException& toCatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 transcode( toCatch.getMessage() ).c_str() );
    }
    catch (const SAXException& toCatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 transcode( toCatch.getMessage() ).c_str() );
    }

    delete poSAXReader;
    delete poIS;

    return m_aoFilenames;
}

/************************************************************************/
/*                             startElement()                           */
/************************************************************************/

void GMLASGuessXSDFilename::startElement(
                            const   XMLCh* const /*uri*/,
                            const   XMLCh* const /*localname*/,
                            const   XMLCh* const /*qname*/,
                            const   Attributes& attrs )
{
    m_nStartElementCounter ++;

    for(unsigned int i=0; i < attrs.getLength(); i++)
    {
        CPLString osAttrURIPrefix( transcode(attrs.getURI(i)) );
        CPLString osAttrLocalname( transcode(attrs.getLocalName(i)) );
        CPLString osAttrValue( transcode(attrs.getValue(i)) );

        if( osAttrURIPrefix == szXSI_URI &&
            osAttrLocalname == szSCHEMA_LOCATION )
        {
            CPLDebug("GMLAS", "%s=%s", szSCHEMA_LOCATION, osAttrValue.c_str());

            char** papszTokens = CSLTokenizeString2(osAttrValue, " ", 0 );
            int nTokens = CSLCount(papszTokens);
            if( (nTokens % 2) == 0 )
            {
                for( int j = 0; j < nTokens; j+= 2 )
                {
                    if( !STARTS_WITH(papszTokens[j], szWFS_URI) &&
                        !(EQUAL(papszTokens[j], szGML_URI) ||
                          STARTS_WITH(papszTokens[j],
                                    (CPLString(szGML_URI)+ "/").c_str())) )
                    {
                        CPLDebug("GMLAS", "Schema to analyze: %s -> %s",
                                 papszTokens[j], papszTokens[j+1]);
                        m_aoFilenames.push_back( PairURIFilename(
                            papszTokens[j], papszTokens[j+1]) );
                    }
                }
            }
            CSLDestroy(papszTokens);
        }
        else if( osAttrURIPrefix == szXSI_URI &&
                 osAttrLocalname == szNO_NAMESPACE_SCHEMA_LOCATION )
        {
            CPLDebug("GMLAS", "%s=%s",
                     szNO_NAMESPACE_SCHEMA_LOCATION, osAttrValue.c_str());
            m_aoFilenames.push_back( PairURIFilename( "", osAttrValue ) );
        }
    }

    if( m_nStartElementCounter == 1 )
        m_bFinish = true;
}

/************************************************************************/
/*                         FillOtherMetadataLayer()                     */
/************************************************************************/

void OGRGMLASDataSource::FillOtherMetadataLayer(
                                GDALOpenInfo* poOpenInfo,
                                const CPLString& osConfigFile,
                                const std::vector<PairURIFilename>& aoXSDs,
                                const std::set<CPLString>& oSetSchemaURLs)
{
    // 2 "secret" options just used for tests
    const bool bKeepRelativePathsForMetadata =
        CPLTestBool( CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                            szKEEP_RELATIVE_PATHS_FOR_METADATA_OPTION, "NO") );

    const bool bExposeConfiguration =
        CPLTestBool( CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                            szEXPOSE_CONFIGURATION_IN_METADATA_OPTION, "YES") );

    const bool bExposeSchemaNames =
        CPLTestBool( CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                            szEXPOSE_SCHEMAS_NAME_IN_METADATA_OPTION, "YES") );

    OGRFeatureDefn* poFDefn = m_poOtherMetadataLayer->GetLayerDefn();

    if( !osConfigFile.empty() && bExposeConfiguration )
    {
        if( STARTS_WITH(osConfigFile, "<Configuration") )
        {
            OGRFeature* poFeature = new OGRFeature(poFDefn);
            poFeature->SetField( szKEY, szCONFIGURATION_INLINED );
            poFeature->SetField( szVALUE, osConfigFile.c_str());
            CPL_IGNORE_RET_VAL(
                            m_poOtherMetadataLayer->CreateFeature(poFeature) );
            delete poFeature;
        }
        else
        {
            OGRFeature* poFeature = new OGRFeature(poFDefn);
            poFeature->SetField( szKEY, szCONFIGURATION_FILENAME );
            char* pszCurDir = CPLGetCurrentDir();
            if( !bKeepRelativePathsForMetadata &&
                CPLIsFilenameRelative(osConfigFile) && pszCurDir != NULL)
            {
                poFeature->SetField( szVALUE,
                                     CPLFormFilename(pszCurDir,
                                                     osConfigFile, NULL));
            }
            else
            {
                poFeature->SetField( szVALUE, osConfigFile.c_str());
            }
            CPLFree(pszCurDir);
            CPL_IGNORE_RET_VAL(
                            m_poOtherMetadataLayer->CreateFeature(poFeature) );
            delete poFeature;

            GByte* pabyRet = NULL;
            if( VSIIngestFile( NULL, osConfigFile, &pabyRet, NULL, -1 ) )
            {
                poFeature = new OGRFeature(poFDefn);
                poFeature->SetField( szKEY, szCONFIGURATION_INLINED );
                poFeature->SetField( szVALUE, reinterpret_cast<char*>(pabyRet));
                CPL_IGNORE_RET_VAL(
                            m_poOtherMetadataLayer->CreateFeature(poFeature) );
                delete poFeature;
            }
            VSIFree(pabyRet);
        }
    }

    const char* const apszMeaningfulOptionsToStoreInMD[] =
    {
        szSWAP_COORDINATES_OPTION,
        szREMOVE_UNUSED_LAYERS_OPTION,
        szREMOVE_UNUSED_FIELDS_OPTION
    };
    for( size_t i=0; i < CPL_ARRAYSIZE(apszMeaningfulOptionsToStoreInMD); ++i )
    {
        const char* pszKey = apszMeaningfulOptionsToStoreInMD[i];
        const char* pszVal = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                               pszKey);
        if( pszVal )
        {
            OGRFeature* poFeature = new OGRFeature(
                                        poFDefn);
            poFeature->SetField( szKEY, pszKey );
            poFeature->SetField( szVALUE, pszVal );
            CPL_IGNORE_RET_VAL(
                            m_poOtherMetadataLayer->CreateFeature(poFeature) );
            delete poFeature;
        }
    }

    CPLString osAbsoluteGMLFilename;
    if( !m_osGMLFilename.empty() )
    {
        OGRFeature* poFeature = new OGRFeature(poFDefn);
        poFeature->SetField( szKEY, szDOCUMENT_FILENAME );
        char* pszCurDir = CPLGetCurrentDir();
        if( !bKeepRelativePathsForMetadata &&
            CPLIsFilenameRelative(m_osGMLFilename) && pszCurDir != NULL)
        {
            osAbsoluteGMLFilename = CPLFormFilename(pszCurDir,
                                                    m_osGMLFilename, NULL);
        }
        else
            osAbsoluteGMLFilename = m_osGMLFilename;
        poFeature->SetField( szVALUE, osAbsoluteGMLFilename.c_str() );
        CPLFree(pszCurDir);
        CPL_IGNORE_RET_VAL( m_poOtherMetadataLayer->CreateFeature(poFeature) );
        delete poFeature;
    }

    int nNSIdx = 1;
    std::set<CPLString> oSetVisitedURI;
    for( int i = 0; i < static_cast<int>(aoXSDs.size()); i++ )
    {
        const CPLString osURI(aoXSDs[i].first);
        const CPLString osXSDFilename(aoXSDs[i].second);

        oSetVisitedURI.insert(osURI);

        if( osURI == szOGRGMLAS_URI )
            continue;

        OGRFeature* poFeature = new OGRFeature(poFDefn);
        poFeature->SetField( szKEY, CPLSPrintf(szNAMESPACE_URI_FMT, nNSIdx) );
        poFeature->SetField( szVALUE, osURI.c_str());
        CPL_IGNORE_RET_VAL( m_poOtherMetadataLayer->CreateFeature(poFeature) );
        delete poFeature;

        poFeature = new OGRFeature(poFDefn);
        poFeature->SetField( szKEY,
                             CPLSPrintf(szNAMESPACE_LOCATION_FMT, nNSIdx) );

        const CPLString osAbsoluteXSDFilename(
            (osXSDFilename.find("http://") != 0 &&
             osXSDFilename.find("https://") != 0 &&
             CPLIsFilenameRelative(osXSDFilename)) ?
                CPLString(CPLFormFilename(CPLGetDirname(osAbsoluteGMLFilename),
                                        osXSDFilename, NULL)) :
                osXSDFilename );
        poFeature->SetField( szVALUE, osAbsoluteXSDFilename.c_str());
        CPL_IGNORE_RET_VAL( m_poOtherMetadataLayer->CreateFeature(poFeature) );
        delete poFeature;

        if( m_oMapURIToPrefix.find(osURI) != m_oMapURIToPrefix.end() )
        {
            poFeature = new OGRFeature(poFDefn);
            poFeature->SetField( szKEY,
                                 CPLSPrintf(szNAMESPACE_PREFIX_FMT, nNSIdx) );
            poFeature->SetField( szVALUE, m_oMapURIToPrefix[osURI].c_str());
            CPL_IGNORE_RET_VAL(
                            m_poOtherMetadataLayer->CreateFeature(poFeature) );
            delete poFeature;
        }

        nNSIdx ++;
    }

    std::map<CPLString,CPLString>::const_iterator oIter =
                                                m_oMapURIToPrefix.begin();
    for( ; oIter != m_oMapURIToPrefix.end(); ++oIter )
    {
        const CPLString& osURI( oIter->first );
        const CPLString& osPrefix( oIter->second );

        if( oSetVisitedURI.find( osURI ) == oSetVisitedURI.end() &&
            osURI != szXML_URI &&
            osURI != szXS_URI &&
            osURI != szXSI_URI &&
            osURI != szXMLNS_URI &&
            osURI != szOGRGMLAS_URI )
        {
            OGRFeature* poFeature = new OGRFeature(poFDefn);
            poFeature->SetField( szKEY,
                                 CPLSPrintf(szNAMESPACE_URI_FMT, nNSIdx) );
            poFeature->SetField( szVALUE, osURI.c_str());
            CPL_IGNORE_RET_VAL(
                            m_poOtherMetadataLayer->CreateFeature(poFeature) );
            delete poFeature;

            poFeature = new OGRFeature(poFDefn);
            poFeature->SetField( szKEY,
                                 CPLSPrintf(szNAMESPACE_PREFIX_FMT, nNSIdx) );
            poFeature->SetField( szVALUE, osPrefix.c_str());
            CPL_IGNORE_RET_VAL(
                            m_poOtherMetadataLayer->CreateFeature(poFeature) );
            delete poFeature;

            nNSIdx ++;
        }
    }

    if( !m_osGMLVersionFound.empty() )
    {
        OGRFeature* poFeature = new OGRFeature(poFDefn);
        poFeature->SetField( szKEY, szGML_VERSION );
        poFeature->SetField( szVALUE, m_osGMLVersionFound );
        CPL_IGNORE_RET_VAL( m_poOtherMetadataLayer->CreateFeature(poFeature) );
        delete poFeature;
    }

    std::set<CPLString>::const_iterator oIterSet = oSetSchemaURLs.begin();
    int nSchemaIdx = 1;
    for( ; bExposeSchemaNames &&
           oIterSet != oSetSchemaURLs.end(); ++oIterSet )
    {
        OGRFeature* poFeature = new OGRFeature(poFDefn);
        poFeature->SetField( szKEY, CPLSPrintf(szSCHEMA_NAME_FMT, nSchemaIdx) );
        poFeature->SetField( szVALUE, (*oIterSet).c_str() );
        CPL_IGNORE_RET_VAL( m_poOtherMetadataLayer->CreateFeature(poFeature) );
        delete poFeature;

        nSchemaIdx ++;
    }
}

/************************************************************************/
/*                         BuildXSDVector()                             */
/************************************************************************/

std::vector<PairURIFilename> OGRGMLASDataSource::BuildXSDVector(
                                            const CPLString& osXSDFilenames)
{
    std::vector<PairURIFilename> aoXSDs;
    char** papszTokens = CSLTokenizeString2(osXSDFilenames," ,",0);
    char* pszCurDir = CPLGetCurrentDir();
    for( int i=0; papszTokens != NULL && papszTokens[i] != NULL; i++ )
    {
        if( !STARTS_WITH(papszTokens[i], "http://") &&
            !STARTS_WITH(papszTokens[i], "https://") &&
            CPLIsFilenameRelative(papszTokens[i]) &&
            pszCurDir != NULL )
        {
            aoXSDs.push_back(PairURIFilename("",
                        CPLFormFilename(pszCurDir, papszTokens[i], NULL)));
        }
        else
        {
            aoXSDs.push_back(PairURIFilename("",papszTokens[i]));
        }
    }
    CPLFree(pszCurDir);
    CSLDestroy(papszTokens);
    return aoXSDs;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRGMLASDataSource::Open(GDALOpenInfo* poOpenInfo)
{
    CPLString osConfigFile = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                  szCONFIG_FILE_OPTION, "");
    if( osConfigFile.empty() )
    {
        const char* pszConfigFile = CPLFindFile("gdal",
                                                szDEFAULT_CONF_FILENAME);
        if( pszConfigFile )
            osConfigFile = pszConfigFile;
    }
    if( osConfigFile.empty() )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No configuration file found. Using hard-coded defaults");
        m_oConf.Finalize();
    }
    else
    {
        if( !m_oConf.Load(osConfigFile) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Loading of configuration failed");
            return false;
        }
    }

    m_oCache.SetCacheDirectory( m_oConf.m_osXSDCacheDirectory );
    const bool bRefreshCache(CPLTestBool(
                       CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                            szREFRESH_CACHE_OPTION, "NO") ));
    m_oCache.SetRefreshMode( bRefreshCache );
    m_oCache.SetAllowDownload( m_oConf.m_bAllowRemoteSchemaDownload );

    m_oIgnoredXPathMatcher.SetRefXPaths(
                                    m_oConf.m_oMapPrefixToURIIgnoredXPaths,
                                    m_oConf.m_aosIgnoredXPaths );

    GMLASSchemaAnalyzer oAnalyzer(m_oIgnoredXPathMatcher);
    oAnalyzer.SetUseArrays(m_oConf.m_bUseArrays);
    oAnalyzer.SetInstantiateGMLFeaturesOnly(
                                        m_oConf.m_bInstantiateGMLFeaturesOnly);
    oAnalyzer.SetIdentifierMaxLength(m_oConf.m_nIdentifierMaxLength);
    oAnalyzer.SetCaseInsensitiveIdentifier(
                                        m_oConf.m_bCaseInsensitiveIdentifier);
    oAnalyzer.SetPGIdentifierLaundering(m_oConf.m_bPGIdentifierLaundering);

    m_osGMLFilename = STARTS_WITH_CI(poOpenInfo->pszFilename, szGMLAS_PREFIX) ?
        poOpenInfo->pszFilename + strlen(szGMLAS_PREFIX) :
        poOpenInfo->pszFilename;

    CPLString osXSDFilenames = CSLFetchNameValueDef(
                                poOpenInfo->papszOpenOptions, szXSD_OPTION, "");

    VSILFILE* fpGML = NULL;
    if( !m_osGMLFilename.empty() )
    {
        fpGML = VSIFOpenL(m_osGMLFilename, "rb");
        if( fpGML == NULL )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Cannot open %s",
                     m_osGMLFilename.c_str());
            return false;
        }
    }
    else if( osXSDFilenames.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s open option must be provided when no "
                 "XML data file is passed",
                 szXSD_OPTION);
        return false;
    }

    std::vector<PairURIFilename> aoXSDs;
    if( osXSDFilenames.empty() )
    {
        GMLASGuessXSDFilename guesser;
        aoXSDs = guesser.Guess(m_osGMLFilename, fpGML);
    }
    else
    {
        aoXSDs = BuildXSDVector(osXSDFilenames);
    }
    if( fpGML )
    {
        m_osHash = CSLFetchNameValueDef(
                                    poOpenInfo->papszOpenOptions, "HASH", "");
        if( m_osHash.empty() )
        {
            VSIFSeekL(fpGML, 0, SEEK_SET);
            std::string osBuffer;
            osBuffer.resize(8192);
            size_t nRead = VSIFReadL(&osBuffer[0], 1, 8192, fpGML);
            osBuffer.resize(nRead);
            size_t nPos = osBuffer.find("timeStamp=\"");
            if( nPos != std::string::npos )
            {
                size_t nPos2 = osBuffer.find('"', nPos+strlen("timeStamp=\""));
                if( nPos2 != std::string::npos )
                    osBuffer.replace(nPos, nPos2-nPos+1, nPos2-nPos+1, ' ');
            }
            CPL_SHA256Context ctxt;
            CPL_SHA256Init(&ctxt);
            CPL_SHA256Update(&ctxt, osBuffer.data(), osBuffer.size());

            VSIStatBufL sStat;
            if( VSIStatL(m_osGMLFilename, &sStat) == 0 )
            {
                m_nFileSize = sStat.st_size;
                CPL_SHA256Update(&ctxt, &(sStat.st_size), sizeof(sStat.st_size));
            }

            GByte abyHash[CPL_SHA256_HASH_SIZE];
            CPL_SHA256Final(&ctxt, abyHash);
            // Half of the hash should be enough for our purpose
            char* pszHash = CPLBinaryToHex(CPL_SHA256_HASH_SIZE / 2, abyHash);
            m_osHash = pszHash;
            CPLFree(pszHash);
        }

        VSIFSeekL(fpGML, 0, SEEK_SET);
        PushUnusedGMLFilePointer(fpGML);
    }

    if( aoXSDs.empty() )
    {
        if( osXSDFilenames.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No schema locations found when analyzing data file: "
                     "%s open option must be provided",
                     szXSD_OPTION);
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No schema locations found");
        }
        return false;
    }

    m_bSchemaFullChecking = CPLFetchBool(
            poOpenInfo->papszOpenOptions,
            szSCHEMA_FULL_CHECKING_OPTION,
            m_oConf.m_bSchemaFullChecking );

    m_bHandleMultipleImports = CPLFetchBool(
            poOpenInfo->papszOpenOptions,
            szHANDLE_MULTIPLE_IMPORTS_OPTION,
            m_oConf.m_bHandleMultipleImports );

    bool bRet = oAnalyzer.Analyze( m_oCache,
                                   CPLGetDirname(m_osGMLFilename),
                                   aoXSDs,
                                   m_bSchemaFullChecking,
                                   m_bHandleMultipleImports );
    if( !bRet )
    {
        return false;
    }

    if( !osXSDFilenames.empty() )
        m_aoXSDsManuallyPassed = aoXSDs;

    m_oMapURIToPrefix = oAnalyzer.GetMapURIToPrefix();

    m_osGMLVersionFound = oAnalyzer.GetGMLVersionFound();

    const std::set<CPLString>& oSetSchemaURLs = oAnalyzer.GetSchemaURLS();

    FillOtherMetadataLayer(poOpenInfo, osConfigFile, aoXSDs, oSetSchemaURLs);

    if( CPLFetchBool(poOpenInfo->papszOpenOptions,
                     szEXPOSE_METADATA_LAYERS_OPTION,
                     m_oConf.m_bExposeMetadataLayers) )
    {
        m_apoRequestedMetadataLayers.push_back(m_poFieldsMetadataLayer);
        m_apoRequestedMetadataLayers.push_back(m_poLayersMetadataLayer);
        m_apoRequestedMetadataLayers.push_back(m_poRelationshipsLayer);
        m_apoRequestedMetadataLayers.push_back(m_poOtherMetadataLayer);
    }

    const char* pszSwapCoordinates = CSLFetchNameValueDef(
                                           poOpenInfo->papszOpenOptions,
                                           szSWAP_COORDINATES_OPTION,
                                           "AUTO");
    if( EQUAL(pszSwapCoordinates, "AUTO") )
    {
        m_eSwapCoordinates = GMLAS_SWAP_AUTO;
    }
    else if( CPLTestBool(pszSwapCoordinates) )
    {
        m_eSwapCoordinates = GMLAS_SWAP_YES;
    }
    else
    {
        m_eSwapCoordinates = GMLAS_SWAP_NO;
    }

    const std::vector<GMLASFeatureClass>& aoClasses = oAnalyzer.GetClasses();

    // First "standard" tables
    for( size_t i=0; i<aoClasses.size(); ++i )
    {
        if( aoClasses[i].GetParentXPath().empty() )
            TranslateClasses( NULL, aoClasses[i] );
    }
    // Then junction tables
    for( size_t i=0; i<aoClasses.size(); ++i )
    {
        if( !aoClasses[i].GetParentXPath().empty() )
            TranslateClasses( NULL, aoClasses[i] );
    }

    // And now do initialization since we need to have instantiated everything
    // to be able to do cross-layer links
    for( size_t i = 0; i < m_apoLayers.size(); i++ )
    {
        m_apoLayers[i]->PostInit( m_oConf.m_bIncludeGeometryXML );
    }
    m_bLayerInitFinished = true;

    // Do optional validation
    m_bValidate = CPLFetchBool(poOpenInfo->papszOpenOptions,
                               szVALIDATE_OPTION,
                               m_oConf.m_bValidate);

    m_bRemoveUnusedLayers = CPLFetchBool(poOpenInfo->papszOpenOptions,
                               szREMOVE_UNUSED_LAYERS_OPTION,
                               m_oConf.m_bRemoveUnusedLayers);

    m_bRemoveUnusedFields = CPLFetchBool(poOpenInfo->papszOpenOptions,
                               szREMOVE_UNUSED_FIELDS_OPTION,
                               m_oConf.m_bRemoveUnusedFields);

    m_oXLinkResolver.SetConf( m_oConf.m_oXLinkResolution );
    m_oXLinkResolver.SetRefreshMode( bRefreshCache );

    if( m_bValidate || m_bRemoveUnusedLayers )
    {
        CPLErrorReset();
        RunFirstPassIfNeeded( NULL, NULL, NULL );
        if( CPLFetchBool( poOpenInfo->papszOpenOptions,
                          szFAIL_IF_VALIDATION_ERROR_OPTION,
                          m_oConf.m_bFailIfValidationError ) &&
            CPLGetLastErrorType() != CE_None )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Validation errors encountered");
            return false;
        }
    }
    if( CPLGetLastErrorType() == CE_Failure )
        CPLErrorReset();

    return true;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMLASDataSource::TestCapability( const char * pszCap )
{
    return EQUAL(pszCap, ODsCRandomLayerRead);
}

/************************************************************************/
/*                           CreateReader()                             */
/************************************************************************/

GMLASReader* OGRGMLASDataSource::CreateReader( VSILFILE*& fpGML,
                                               GDALProgressFunc pfnProgress,
                                               void* pProgressData )
{
    if( fpGML == NULL )
    {
        // Try recycling an already opened and unused file pointer
        fpGML = PopUnusedGMLFilePointer();
        if( fpGML == NULL )
            fpGML = VSIFOpenL(GetGMLFilename(), "rb");
        if( fpGML == NULL )
            return NULL;
    }

    GMLASReader* poReader = new GMLASReader( GetCache(),
                                             GetIgnoredXPathMatcher(),
                                             m_oXLinkResolver );
    poReader->Init( GetGMLFilename(),
                      fpGML,
                      GetMapURIToPrefix(),
                      GetLayers(),
                      false,
                      std::vector<PairURIFilename>(),
                      m_bSchemaFullChecking,
                      m_bHandleMultipleImports );

    poReader->SetSwapCoordinates( GetSwapCoordinates() );

    poReader->SetFileSize( m_nFileSize );

    if( !RunFirstPassIfNeeded( poReader, pfnProgress, pProgressData ) )
    {
        delete poReader;
        return NULL;
    }

    poReader->SetMapIgnoredXPathToWarn( GetMapIgnoredXPathToWarn());

    poReader->SetHash( GetHash() );

    return poReader;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGMLASDataSource::ResetReading()
{
    delete m_poReader;
    m_poReader = NULL;
    for(size_t i=0; i<m_apoRequestedMetadataLayers.size();++i)
        m_apoRequestedMetadataLayers[i]->ResetReading();
    m_bEndOfReaderLayers = false;
    m_nCurMetadataLayerIdx = -1;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGRGMLASDataSource::GetNextFeature( OGRLayer** ppoBelongingLayer,
                                                double* pdfProgressPct,
                                                GDALProgressFunc pfnProgress,
                                                void* pProgressData )
{
    if( m_bEndOfReaderLayers )
    {
        if( m_nCurMetadataLayerIdx >= 0 &&
            m_nCurMetadataLayerIdx <
                    static_cast<int>(m_apoRequestedMetadataLayers.size()) )
        {
            while( true )
            {
                OGRLayer* poLayer =
                    m_apoRequestedMetadataLayers[m_nCurMetadataLayerIdx];
                OGRFeature* poFeature = poLayer->GetNextFeature();
                if( poFeature != NULL )
                {
                    if( pdfProgressPct != NULL )
                        *pdfProgressPct = 1.0;
                    if( ppoBelongingLayer != NULL )
                        *ppoBelongingLayer = poLayer;
                    return poFeature;
                }
                if( m_nCurMetadataLayerIdx + 1
                     < static_cast<int>(m_apoRequestedMetadataLayers.size()) )
                {
                    m_nCurMetadataLayerIdx ++;
                }
                else
                {
                    m_nCurMetadataLayerIdx = -1;
                    break;
                }
            }
        }

        if( pdfProgressPct != NULL )
            *pdfProgressPct = 1.0;
        if( ppoBelongingLayer != NULL )
            *ppoBelongingLayer = NULL;
        return NULL;
    }

    const double dfInitialScanRatio = 0.1;
    if( m_poReader == NULL )
    {
        void* pScaledProgress = GDALCreateScaledProgress( 0.0, dfInitialScanRatio,
                                                          pfnProgress,
                                                          pProgressData );

        m_poReader = CreateReader(m_fpGMLParser,
                                  pScaledProgress ? GDALScaledProgress : NULL,
                                  pScaledProgress);

        GDALDestroyScaledProgress(pScaledProgress);

        if( m_poReader == NULL )
        {
            if( pdfProgressPct != NULL )
                *pdfProgressPct = 1.0;
            if( ppoBelongingLayer != NULL )
                *ppoBelongingLayer = NULL;
            return NULL;
        }
    }

    void* pScaledProgress = GDALCreateScaledProgress( dfInitialScanRatio, 1.0,
                                                      pfnProgress,
                                                      pProgressData );

    while( true )
    {
        OGRGMLASLayer* poBelongingLayer = NULL;
        OGRFeature* poFeature = m_poReader->GetNextFeature(
                    &poBelongingLayer,
                    pScaledProgress ? GDALScaledProgress : NULL,
                    pScaledProgress);
        if( poFeature == NULL ||
            poBelongingLayer->EvaluateFilter(poFeature) )
        {
            if( ppoBelongingLayer != NULL )
                *ppoBelongingLayer = poBelongingLayer;
            if( pdfProgressPct != NULL )
            {
                const vsi_l_offset nOffset = VSIFTellL(m_fpGMLParser);
                if( nOffset == m_nFileSize )
                    *pdfProgressPct = 1.0;
                else
                    *pdfProgressPct = dfInitialScanRatio +
                        (1.0 - dfInitialScanRatio) * nOffset / m_nFileSize;
            }
            GDALDestroyScaledProgress(pScaledProgress);
            if( poFeature == NULL )
            {
                m_bEndOfReaderLayers = true;
                if( !m_apoRequestedMetadataLayers.empty() )
                {
                    m_nCurMetadataLayerIdx = 0;
                    return GetNextFeature( ppoBelongingLayer, pdfProgressPct,
                                        pfnProgress, pProgressData );
                }
                else
                {
                    return NULL;
                }
            }
            else
                return poFeature;
        }
        delete poFeature;
    }
}

/************************************************************************/
/*                          GetLayerByXPath()                           */
/************************************************************************/

OGRGMLASLayer* OGRGMLASDataSource::GetLayerByXPath( const CPLString& osXPath )
{
    for(size_t i = 0; i < m_apoLayers.size(); i++ )
    {
        if( m_apoLayers[i]->GetFeatureClass().GetXPath() == osXPath )
        {
            return m_apoLayers[i];
        }
    }
    return NULL;
}

/************************************************************************/
/*                       PushUnusedGMLFilePointer()                     */
/************************************************************************/

void OGRGMLASDataSource::PushUnusedGMLFilePointer( VSILFILE* fpGML )
{
    if( m_fpGML == NULL )
        m_fpGML = fpGML;
    else
    {
        VSIFCloseL(fpGML);
    }
}

/************************************************************************/
/*                        PopUnusedGMLFilePointer()                     */
/************************************************************************/

VSILFILE* OGRGMLASDataSource::PopUnusedGMLFilePointer()
{
    VSILFILE* fpGML = m_fpGML;
    m_fpGML = NULL;
    return fpGML;
}

/************************************************************************/
/*                          RunFirstPassIfNeeded()                      */
/************************************************************************/

bool OGRGMLASDataSource::RunFirstPassIfNeeded( GMLASReader* poReader,
                                               GDALProgressFunc pfnProgress,
                                               void* pProgressData )
{
    if( m_bFirstPassDone )
    {
        if( poReader != NULL )
        {
            poReader->SetMapSRSNameToInvertedAxis(m_oMapSRSNameToInvertedAxis);
            poReader->SetMapGeomFieldDefnToSRSName(m_oMapGeomFieldDefnToSRSName);
        }
        return true;
    }

    m_bFirstPassDone = true;

    // Determine if we have geometry fields in any layer
    bool bHasGeomFields = false;
    for(size_t i=0;i<m_apoLayers.size();i++)
    {
        m_apoLayers[i]->SetLayerDefnFinalized(true);
        if( m_apoLayers[i]->GetLayerDefn()->GetGeomFieldCount() > 0 )
        {
            bHasGeomFields = true;
            break;
        }
    }

    // If so, do an initial pass to determine the SRS of those geometry fields.
    const bool bHasURLSpecificRules =
                !m_oXLinkResolver.GetConf().m_aoURLSpecificRules.empty();
    if( bHasGeomFields || m_bValidate || m_bRemoveUnusedLayers ||
        m_bRemoveUnusedFields || bHasURLSpecificRules )
    {
        bool bJustOpenedFiled =false;
        VSILFILE* fp = NULL;
        if( poReader )
            fp = poReader->GetFP();
        else
        {
            fp = VSIFOpenL(GetGMLFilename(), "rb");
            if( fp == NULL )
            {
                return false;
            }
            bJustOpenedFiled = true;
        }

        GMLASReader* poReaderFirstPass = new GMLASReader(m_oCache,
                                                         m_oIgnoredXPathMatcher,
                                                         m_oXLinkResolver);
        poReaderFirstPass->Init( GetGMLFilename(),
                                 fp,
                                 GetMapURIToPrefix(),
                                 GetLayers(),
                                 m_bValidate,
                                 m_aoXSDsManuallyPassed,
                                 m_bSchemaFullChecking,
                                 m_bHandleMultipleImports );

        poReaderFirstPass->SetFileSize( m_nFileSize );

        poReaderFirstPass->SetMapIgnoredXPathToWarn(
                                    m_oConf.m_oMapIgnoredXPathToWarn);
        // No need to warn afterwards
        m_oConf.m_oMapIgnoredXPathToWarn.clear();

        std::set<CPLString> aoSetRemovedLayerNames;
        m_bFirstPassDone = poReaderFirstPass->RunFirstPass(pfnProgress,
                                                           pProgressData,
                                                           m_bRemoveUnusedLayers,
                                                           m_bRemoveUnusedFields,
                                                           aoSetRemovedLayerNames);

        // If we have removed layers, we also need to cleanup our special
        // metadata layers
        if( !aoSetRemovedLayerNames.empty() )
        {
            // Removing features while iterating works here given the layers
            // are MEM layers
            OGRFeature* poFeature;
            m_poLayersMetadataLayer->ResetReading();
            while( (poFeature = m_poLayersMetadataLayer->GetNextFeature() )
                                                                    != NULL )
            {
                const char* pszLayerName =
                                    poFeature->GetFieldAsString(szLAYER_NAME);
                if( aoSetRemovedLayerNames.find(pszLayerName) !=
                                                aoSetRemovedLayerNames.end() )
                {
                    CPL_IGNORE_RET_VAL(m_poLayersMetadataLayer->
                                        DeleteFeature(poFeature->GetFID()));
                }
                delete poFeature;
            }
            m_poLayersMetadataLayer->ResetReading();

            m_poFieldsMetadataLayer->ResetReading();
            while( (poFeature = m_poFieldsMetadataLayer->GetNextFeature() )
                                                                    != NULL )
            {
                const char* pszLayerName =
                                    poFeature->GetFieldAsString(szLAYER_NAME);
                const char* pszRelatedLayerName =
                            poFeature->GetFieldAsString(szFIELD_RELATED_LAYER);
                if( aoSetRemovedLayerNames.find(pszLayerName) !=
                                            aoSetRemovedLayerNames.end() ||
                    aoSetRemovedLayerNames.find(pszRelatedLayerName) !=
                                                aoSetRemovedLayerNames.end() )
                {
                    CPL_IGNORE_RET_VAL(m_poFieldsMetadataLayer->
                                        DeleteFeature(poFeature->GetFID()));
                }
                delete poFeature;
            }
            m_poFieldsMetadataLayer->ResetReading();

            m_poRelationshipsLayer->ResetReading();
            while( (poFeature = m_poRelationshipsLayer->GetNextFeature() )
                                                                    != NULL )
            {
                const char* pszParentLayerName =
                                    poFeature->GetFieldAsString(szPARENT_LAYER);
                const char* pszChildLayerName =
                                    poFeature->GetFieldAsString(szCHILD_LAYER);
                if( aoSetRemovedLayerNames.find(pszParentLayerName) !=
                                                aoSetRemovedLayerNames.end() ||
                    aoSetRemovedLayerNames.find(pszChildLayerName) !=
                                                aoSetRemovedLayerNames.end() )
                {
                    CPL_IGNORE_RET_VAL(m_poRelationshipsLayer->
                                        DeleteFeature(poFeature->GetFID()));
                }
                delete poFeature;
            }
            m_poRelationshipsLayer->ResetReading();
        }

        // Store 2 maps to reinject them in real readers
        m_oMapSRSNameToInvertedAxis =
                        poReaderFirstPass->GetMapSRSNameToInvertedAxis();
        m_oMapGeomFieldDefnToSRSName =
                        poReaderFirstPass->GetMapGeomFieldDefnToSRSName();

        delete poReaderFirstPass;

        VSIFSeekL(fp, 0, SEEK_SET);
        if( bJustOpenedFiled )
            PushUnusedGMLFilePointer(fp);

        if( poReader != NULL )
        {
            poReader->SetMapSRSNameToInvertedAxis(m_oMapSRSNameToInvertedAxis);
            poReader->SetMapGeomFieldDefnToSRSName(m_oMapGeomFieldDefnToSRSName);
        }
    }

    return m_bFirstPassDone;
}
