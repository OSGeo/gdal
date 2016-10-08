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

#include "ogr_gmlas.h"
#include "ogr_mem.h"
#include "cpl_sha256.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRGMLASDataSource()                        */
/************************************************************************/

OGRGMLASDataSource::OGRGMLASDataSource()
{
    // FIXME
    XMLPlatformUtils::Initialize();
    m_bExposeMetadataLayers = false;
    m_fpGML = NULL;
    m_fpGMLParser = NULL;
    m_bLayerInitFinished = false;
    m_bValidate = false;
    m_bRemoveUnusedLayers = false;
    m_bRemoveUnusedFields = false;
    m_bFirstPassDone = false;
    m_eSwapCoordinates = GMLAS_SWAP_AUTO;
    m_nFileSize = 0;
    m_poReader = NULL;

    m_poFieldsMetadataLayer = new OGRMemLayer
                                    ("_ogr_fields_metadata", NULL, wkbNone );
    {
        OGRFieldDefn oFieldDefn("layer_name", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_name", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_xpath", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_type", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_is_list", OFTInteger);
        oFieldDefn.SetSubType(OFSTBoolean);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_min_occurs", OFTInteger);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_max_occurs", OFTInteger);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_default_value", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_fixed_value", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_category", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_related_layer", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_junction_layer", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("field_documentation", OFTString);
        m_poFieldsMetadataLayer->CreateField(&oFieldDefn);
    }

    m_poLayersMetadataLayer = new OGRMemLayer
                                    ("_ogr_layers_metadata", NULL, wkbNone );
    {
        OGRFieldDefn oFieldDefn("layer_name", OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("layer_xpath", OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("layer_category", OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("layer_documentation", OFTString);
        m_poLayersMetadataLayer->CreateField(&oFieldDefn);
    }

    m_poRelationshipsLayer = new OGRMemLayer("_ogr_layer_relationships",
                                             NULL, wkbNone );
    {
        OGRFieldDefn oFieldDefn("parent_layer", OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("parent_pkid", OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("parent_element_name", OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("child_layer", OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
    }
    {
        OGRFieldDefn oFieldDefn("child_pkid", OFTString);
        m_poRelationshipsLayer->CreateField(&oFieldDefn);
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
    if( m_fpGML != NULL )
        VSIFCloseL(m_fpGML);
    if( m_fpGMLParser != NULL )
        VSIFCloseL(m_fpGMLParser);
    delete m_poReader;

    // FIXME
    XMLPlatformUtils::Terminate();
}

/************************************************************************/
/*                            GetLayerCount()                           */
/************************************************************************/

int         OGRGMLASDataSource::GetLayerCount()
{
    return static_cast<int>(m_apoLayers.size() +
                            (m_bExposeMetadataLayers ? 3 : 0));
}

/************************************************************************/
/*                                GetLayer()                            */
/************************************************************************/

OGRLayer    *OGRGMLASDataSource::GetLayer(int i)
{
    if( m_bExposeMetadataLayers && i == static_cast<int>(m_apoLayers.size()) )
        return m_poFieldsMetadataLayer;
    if( m_bExposeMetadataLayers && i == 1 + static_cast<int>(m_apoLayers.size()) )
        return m_poLayersMetadataLayer;
    if( m_bExposeMetadataLayers && i == 2 + static_cast<int>(m_apoLayers.size()) )
        return m_poRelationshipsLayer;

    if( i < 0 || i >= static_cast<int>(m_apoLayers.size()) )
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

    if( EQUAL(pszName, m_poFieldsMetadataLayer->GetName()) )
        return m_poFieldsMetadataLayer;

    if( EQUAL(pszName, m_poLayersMetadataLayer->GetName()) )
        return m_poLayersMetadataLayer;

    if( EQUAL(pszName, m_poRelationshipsLayer->GetName()) )
        return m_poRelationshipsLayer;

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
        );

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

        if( osAttrURIPrefix == pszXSI_URI &&
            osAttrLocalname == "schemaLocation" )
        {
            CPLDebug("GMLAS", "schemaLocation=%s", osAttrValue.c_str());

            char** papszTokens = CSLTokenizeString2(osAttrValue, " ", 0 );
            int nTokens = CSLCount(papszTokens);
            if( (nTokens % 2) == 0 )
            {
                for( int j = 0; j < nTokens; j+= 2 )
                {
                    if( !STARTS_WITH(papszTokens[j], pszWFS_URI) &&
                        !STARTS_WITH(papszTokens[j], pszGML_URI) )
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
        else if( osAttrURIPrefix == pszXSI_URI &&
                 osAttrLocalname == "noNamespaceSchemaLocation" )
        {
            CPLDebug("GMLAS", "noNamespaceSchemaLocation=%s", osAttrValue.c_str());
            m_aoFilenames.push_back( PairURIFilename( "", osAttrValue ) );
        }
    }

    if( m_nStartElementCounter == 1 )
        m_bFinish = true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

bool OGRGMLASDataSource::Open(GDALOpenInfo* poOpenInfo)
{
    const char* pszConfigFile = CSLFetchNameValue(poOpenInfo->papszOpenOptions,
                                                  "CONFIG_FILE");
    if( pszConfigFile == NULL )
        pszConfigFile = CPLFindFile("gdal", "gmlasconf.xml");
    if( pszConfigFile == NULL )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "No configuration file found. Using hard-coded defaults");
        m_oConf.Finalize();
    }
    else
    {
        if( !m_oConf.Load(pszConfigFile) )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Loading of configuration failed");
            return false;
        }
    }

    m_oCache.SetCacheDirectory( m_oConf.m_osXSDCacheDirectory );
    const bool bRefreshCache(CPLTestBool(
                            CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                "REFRESH_CACHE", "NO") ));
    m_oCache.SetRefreshMode( bRefreshCache );
    m_oCache.SetAllowDownload( m_oConf.m_bAllowRemoteSchemaDownload );

    m_oIgnoredXPathMatcher.SetRefXPaths(
                                    m_oConf.m_oMapPrefixToURIIgnoredXPaths,
                                    m_oConf.m_aosIgnoredXPaths );

    GMLASSchemaAnalyzer oAnalyzer(m_oIgnoredXPathMatcher);
    oAnalyzer.SetUseArrays(m_oConf.m_bUseArrays);
    oAnalyzer.SetInstantiateGMLFeaturesOnly(m_oConf.m_bInstantiateGMLFeaturesOnly);

    m_osGMLFilename = STARTS_WITH_CI(poOpenInfo->pszFilename, "GMLAS:") ?
        poOpenInfo->pszFilename + strlen("GMLAS:") : poOpenInfo->pszFilename;

    CPLString osXSDFilename = CSLFetchNameValueDef(poOpenInfo->papszOpenOptions,
                                                   "XSD", "");

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
    else if( osXSDFilename.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "XSD open option must be provided when no XML data file is passed");
        return false;
    }

    std::vector<PairURIFilename> aoXSDs;
    if( osXSDFilename.empty() )
    {
        GMLASGuessXSDFilename guesser;
        aoXSDs = guesser.Guess(m_osGMLFilename, fpGML);
    }
    else
    {
        char** papszTokens = CSLTokenizeString2(osXSDFilename," ,",0);
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
        m_aoXSDs = aoXSDs;
    }
    if( fpGML )
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

        VSIFSeekL(fpGML, 0, SEEK_SET);

        PushUnusedGMLFilePointer(fpGML);
    }

    if( aoXSDs.empty() )
    {
        if( osXSDFilename.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                    "No schema locations found when analyzing data file: XSD open "
                    "option must be provided");
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No schema locations found");
        }
        return false;
    }
    bool bRet = oAnalyzer.Analyze( m_oCache,
                                   CPLGetDirname(m_osGMLFilename),
                                   aoXSDs );
    if( !bRet )
    {
        return false;
    }

    m_oMapURIToPrefix = oAnalyzer.GetMapURIToPrefix();

    m_bExposeMetadataLayers = CPLFetchBool(poOpenInfo->papszOpenOptions,
                                           "EXPOSE_METADATA_LAYERS",
                                           m_oConf.m_bExposeMetadataLayers);

    const char* pszSwapCoordinates = CSLFetchNameValueDef(
                                           poOpenInfo->papszOpenOptions,
                                           "SWAP_COORDINATES",
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
                              "VALIDATE",
                               m_oConf.m_bValidate);

    m_bRemoveUnusedLayers = CPLFetchBool(poOpenInfo->papszOpenOptions,
                              "REMOVE_UNUSED_LAYERS",
                               m_oConf.m_bRemoveUnusedLayers);

    m_bRemoveUnusedFields = CPLFetchBool(poOpenInfo->papszOpenOptions,
                              "REMOVE_UNUSED_FIELDS",
                               m_oConf.m_bRemoveUnusedFields);

    m_oXLinkResolver.SetConf( m_oConf.m_oXLinkResolution );
    m_oXLinkResolver.SetRefreshMode( bRefreshCache );

    if( m_bValidate || m_bRemoveUnusedLayers )
    {
        CPLErrorReset();
        RunFirstPassIfNeeded( NULL, NULL, NULL );
        if( CPLFetchBool( poOpenInfo->papszOpenOptions,
                          "FAIL_IF_VALIDATION_ERROR",
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
/*                            ResetReading()                            */
/************************************************************************/

void OGRGMLASDataSource::ResetReading()
{
    delete m_poReader;
    m_poReader = NULL;
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
                      false );

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
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature* OGRGMLASDataSource::GetNextFeature( OGRLayer** ppoBelongingLayer,
                                                double* pdfProgressPct,
                                                GDALProgressFunc pfnProgress,
                                                void* pProgressData )
{
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
                *pdfProgressPct = dfInitialScanRatio +
                    (1.0 - dfInitialScanRatio) * VSIFTellL(m_fpGMLParser) / m_nFileSize;
            }
            GDALDestroyScaledProgress(pScaledProgress);
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
                                 m_aoXSDs );

        poReaderFirstPass->SetFileSize( m_nFileSize );

        poReaderFirstPass->SetMapIgnoredXPathToWarn(
                                    m_oConf.m_oMapIgnoredXPathToWarn);
        // No need to warn afterwards
        m_oConf.m_oMapIgnoredXPathToWarn.clear();

        m_bFirstPassDone = poReaderFirstPass->RunFirstPass(pfnProgress,
                                                           pProgressData,
                                                           m_bRemoveUnusedLayers,
                                                           m_bRemoveUnusedFields);

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
