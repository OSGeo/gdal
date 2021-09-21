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

#include "ogr_p.h"

#include "cpl_json_header.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                        GMLASBinInputStream                           */
/************************************************************************/

class GMLASBinInputStream : public BinInputStream
{
    VSILFILE*         m_fp;

public :

    explicit GMLASBinInputStream(VSILFILE* fp);
    virtual ~GMLASBinInputStream();

    virtual XMLFilePos curPos() const override;
    virtual XMLSize_t readBytes(XMLByte* const toFill, const XMLSize_t maxToRead) override;
    virtual const XMLCh* getContentType() const override ;
};

/************************************************************************/
/*                        GMLASBinInputStream()                         */
/************************************************************************/

GMLASBinInputStream::GMLASBinInputStream(VSILFILE* fp)
{
    m_fp = fp;
    VSIFSeekL(fp, 0, SEEK_SET);
}

/************************************************************************/
/*                       ~GMLASBinInputStream()                         */
/************************************************************************/

GMLASBinInputStream::~ GMLASBinInputStream()
{
}

/************************************************************************/
/*                                curPos()                              */
/************************************************************************/

XMLFilePos GMLASBinInputStream::curPos() const
{
    return (XMLFilePos)VSIFTellL(m_fp);
}

/************************************************************************/
/*                               readBytes()                            */
/************************************************************************/

XMLSize_t GMLASBinInputStream::readBytes(XMLByte* const toFill,
                                         const XMLSize_t maxToRead)
{
    return (XMLSize_t)VSIFReadL(toFill, 1, maxToRead, m_fp);
}

/************************************************************************/
/*                            getContentType()                          */
/************************************************************************/

const XMLCh* GMLASBinInputStream::getContentType() const
{
    return nullptr;
}

/************************************************************************/
/*                          GMLASInputSource()                          */
/************************************************************************/

GMLASInputSource::GMLASInputSource(const char* pszFilename,
                                   VSILFILE* fp,
                                   bool bOwnFP,
                                   MemoryManager* const manager)
    : InputSource(manager),
      m_osFilename( pszFilename )
{
    m_fp = fp;
    m_bOwnFP = bOwnFP;
    try
    {
        XMLCh* pFilename = XMLString::transcode(pszFilename);
        setPublicId(pFilename);
        setSystemId(pFilename);
        XMLString::release( &pFilename );
    }
    catch( const TranscodingException& e )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "TranscodingException: %s",
                 transcode(e.getMessage()).c_str());
    }
    m_nCounter = 0;
    m_pnCounter = &m_nCounter;
    m_cbk = nullptr;
}

/************************************************************************/
/*                        SetClosingCallback()                          */
/************************************************************************/

void GMLASInputSource::SetClosingCallback( IGMLASInputSourceClosing* cbk )
{
    m_cbk = cbk;
}

/************************************************************************/
/*                         ~GMLASInputSource()                          */
/************************************************************************/

GMLASInputSource::~GMLASInputSource()
{
    if( m_cbk )
        m_cbk->notifyClosing( m_osFilename );
    if( m_bOwnFP && m_fp )
        VSIFCloseL(m_fp);
}

/************************************************************************/
/*                              makeStream()                            */
/************************************************************************/

BinInputStream* GMLASInputSource::makeStream() const
{
    // This is a lovely cheating around the const qualifier of this method !
    // We cannot modify m_nCounter directly, but we can change the value
    // pointed by m_pnCounter...
    if( *m_pnCounter != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "makeStream() called several times on same GMLASInputSource");
        return nullptr;
    }
    (*m_pnCounter) ++;
    if( m_fp == nullptr )
        return nullptr;
    return new GMLASBinInputStream(m_fp);
}

/************************************************************************/
/*                            warning()                                 */
/************************************************************************/

void GMLASErrorHandler::warning (const SAXParseException& e)
{
    handle (e, CE_Warning);
}

/************************************************************************/
/*                             error()                                  */
/************************************************************************/

void GMLASErrorHandler::error (const SAXParseException& e)
{
    m_bFailed = true;
    handle (e, CE_Failure);
}

/************************************************************************/
/*                          fatalError()                                */
/************************************************************************/

void GMLASErrorHandler::fatalError (const SAXParseException& e)
{
    m_bFailed = true;
    handle (e, CE_Failure);
}

/************************************************************************/
/*                            handle()                                  */
/************************************************************************/

void GMLASErrorHandler::handle (const SAXParseException& e, CPLErr eErr)
{
    const XMLCh* resourceId (e.getPublicId());

    if ( resourceId == nullptr || resourceId[0] == 0 )
        resourceId = e.getSystemId();

    CPLString osErrorMsg(transcode(e.getMessage()));
    if( m_bSchemaFullChecking &&
        osErrorMsg.find("forbidden restriction of any particle") !=
                                                            std::string::npos )
    {
        osErrorMsg += ". You may retry with the " +
                      CPLString(szSCHEMA_FULL_CHECKING_OPTION) +
                      "=NO open option";
    }
    else if( !m_bHandleMultipleImports && osErrorMsg.find("not found") !=
                                                            std::string::npos )
    {
        osErrorMsg += ". You may retry with the " +
                      CPLString(szHANDLE_MULTIPLE_IMPORTS_OPTION) +
                      "=YES open option";
    }
    CPLError(eErr, CPLE_AppDefined, "%s:%d:%d %s",
             transcode(resourceId).c_str(),
             static_cast<int>(e.getLineNumber()),
             static_cast<int>(e.getColumnNumber()),
             osErrorMsg.c_str());
}

/************************************************************************/
/*                     GMLASBaseEntityResolver()                        */
/************************************************************************/

GMLASBaseEntityResolver::GMLASBaseEntityResolver(const CPLString& osBasePath,
                                                 GMLASXSDCache& oCache)
    : m_oCache(oCache)
{
    m_aosPathStack.push_back(osBasePath);
}

/************************************************************************/
/*                    ~GMLASBaseEntityResolver()                        */
/************************************************************************/

GMLASBaseEntityResolver::~GMLASBaseEntityResolver()
{
    CPLAssert( m_aosPathStack.size() == 1 );
}

/************************************************************************/
/*                            notifyClosing()                           */
/************************************************************************/

/* Called by GMLASInputSource destructor. This is useful for use to */
/* know where a .xsd has been finished from processing. Note that we */
/* strongly depend on Xerces behavior here... */
void GMLASBaseEntityResolver::notifyClosing(const CPLString& osFilename )
{
    CPLDebug("GMLAS", "Closing %s", osFilename.c_str());

    CPLAssert( m_aosPathStack.back() ==
                                CPLString(CPLGetDirname(osFilename)) );
    m_aosPathStack.pop_back();
}

/************************************************************************/
/*                            SetBasePath()                             */
/************************************************************************/

void GMLASBaseEntityResolver::SetBasePath(const CPLString& osBasePath)
{
    CPLAssert( m_aosPathStack.size() == 1 );
    m_aosPathStack[0] = osBasePath;
}

/************************************************************************/
/*                         DoExtraSchemaProcessing()                    */
/************************************************************************/

void GMLASBaseEntityResolver::DoExtraSchemaProcessing(
                                             const CPLString& /*osFilename*/,
                                             VSILFILE* /*fp*/)
{
}

/************************************************************************/
/*                         resolveEntity()                              */
/************************************************************************/

InputSource* GMLASBaseEntityResolver::resolveEntity(
                                                const XMLCh* const /*publicId*/,
                                                const XMLCh* const systemId)
{
    // Can happen on things like <xs:import namespace="http://www.w3.org/XML/1998/namespace"/>
    if( systemId == nullptr )
        return nullptr;

    CPLString osSystemId(transcode(systemId));

    if( osSystemId.find("/gml/2.1.2/") != std::string::npos )
        m_osGMLVersionFound = "2.1.2";
    else if( osSystemId.find("/gml/3.1.1/") != std::string::npos )
        m_osGMLVersionFound = "3.1.1";
    else if( osSystemId.find("/gml/3.2.1/") != std::string::npos )
        m_osGMLVersionFound = "3.2.1";

    CPLString osNewPath;
    VSILFILE* fp = m_oCache.Open(osSystemId,
                                 m_aosPathStack.back(),
                                 osNewPath);

    if( fp != nullptr )
    {
        if( osNewPath.find("/vsicurl_streaming/") == 0 )
            m_oSetSchemaURLs.insert(
                            osNewPath.substr(strlen("/vsicurl_streaming/")));
        else
            m_oSetSchemaURLs.insert(osNewPath);

        CPLDebug("GMLAS", "Opening %s", osNewPath.c_str());
        DoExtraSchemaProcessing( osNewPath, fp );
    }

    m_aosPathStack.push_back( CPLGetDirname(osNewPath) );
    GMLASInputSource* poIS = new GMLASInputSource(osNewPath, fp, true);
    poIS->SetClosingCallback(this);
    return poIS;
}

/************************************************************************/
/*                             Dump()                                   */
/************************************************************************/

void GMLASReader::Context::Dump() const
{
    CPLDebug("GMLAS", "Context");
    CPLDebug("GMLAS", "  m_nLevel = %d", m_nLevel);
    CPLDebug("GMLAS", "  m_poFeature = %p", m_poFeature);
    const char* pszDebug = CPLGetConfigOption("CPL_DEBUG", "OFF");
    if( EQUAL(pszDebug, "ON") || EQUAL(pszDebug, "GMLAS") )
    {
        if( m_poFeature )
            m_poFeature->DumpReadable(stderr);
    }
    CPLDebug("GMLAS", "  m_poLayer = %p (%s)",
             m_poLayer, m_poLayer ? m_poLayer->GetName() : "");
    CPLDebug("GMLAS", "  m_poGroupLayer = %p (%s)",
             m_poGroupLayer, m_poGroupLayer ? m_poGroupLayer->GetName() : "");
    CPLDebug("GMLAS", "  m_nGroupLayerLevel = %d", m_nGroupLayerLevel);
    CPLDebug("GMLAS", "  m_nLastFieldIdxGroupLayer = %d",
             m_nLastFieldIdxGroupLayer);
    CPLDebug("GMLAS", "  m_osCurSubXPath = %s",
             m_osCurSubXPath.c_str());
}

/************************************************************************/
/*                             GMLASReader()                            */
/************************************************************************/

GMLASReader::GMLASReader(GMLASXSDCache& oCache,
                         const GMLASXPathMatcher& oIgnoredXPathMatcher,
                         GMLASXLinkResolver& oXLinkResolver)
    : m_oCache(oCache)
    , m_oIgnoredXPathMatcher(oIgnoredXPathMatcher)
    , m_oXLinkResolver(oXLinkResolver)
{
    m_bParsingError = false;
    m_poSAXReader = nullptr;
    m_fp = nullptr;
    m_GMLInputSource = nullptr;
    m_bFirstIteration = true;
    m_bEOF = false;
    m_bInterrupted = false;
    m_papoLayers = nullptr;
    m_nLevel = 0;
    m_oCurCtxt.m_nLevel = 0;
    m_oCurCtxt.m_poLayer = nullptr;
    m_oCurCtxt.m_poGroupLayer = nullptr;
    m_oCurCtxt.m_nGroupLayerLevel = -1;
    m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;
    m_oCurCtxt.m_poFeature = nullptr;
    m_nCurFieldIdx = -1;
    m_nCurGeomFieldIdx = -1;
    m_nCurFieldLevel = 0;
    m_bIsXMLBlob = false;
    m_bIsXMLBlobIncludeUpper = false;
    m_nTextContentListEstimatedSize = 0;
    m_poLayerOfInterest = nullptr;
    m_nMaxLevel = atoi(CPLGetConfigOption("GMLAS_XML_MAX_LEVEL", "100"));
    m_nMaxContentSize = static_cast<size_t>(
          atoi(CPLGetConfigOption("GMLAS_XML_MAX_CONTENT_SIZE", "512000000")));
    m_bValidate = false;
    m_poEntityResolver = nullptr;
    m_nLevelSilentIgnoredXPath = -1;
    m_eSwapCoordinates = GMLAS_SWAP_AUTO;
    m_bInitialPass = false;
    m_bProcessSWEDataArray = false;
    m_bProcessSWEDataRecord = false;
    m_nSWEDataArrayLevel = -1;
    m_nSWEDataRecordLevel = -1;
    m_poFieldsMetadataLayer = nullptr;
    m_poLayersMetadataLayer = nullptr;
    m_poRelationshipsLayer = nullptr;
    m_nFileSize = 0;
    m_bWarnUnexpected =
        CPLTestBool(CPLGetConfigOption("GMLAS_WARN_UNEXPECTED", "FALSE"));
    m_nSWEDataArrayLayerIdx = 0;
}

/************************************************************************/
/*                            ~GMLASReader()                            */
/************************************************************************/

GMLASReader::~GMLASReader()
{
    delete m_poSAXReader;
    delete m_GMLInputSource;
    if( m_oCurCtxt.m_poFeature != nullptr && !m_aoStackContext.empty() &&
        m_oCurCtxt.m_poFeature != m_aoStackContext.back().m_poFeature )
    {
        CPLDebug("GMLAS", "Delete feature m_oCurCtxt.m_poFeature=%p",
                 m_oCurCtxt.m_poFeature);
        delete m_oCurCtxt.m_poFeature;
    }
    for( size_t i = 0; i < m_aoStackContext.size(); i++ )
    {
        if( i == 0 ||
            m_aoStackContext[i].m_poFeature !=
                                        m_aoStackContext[i-1].m_poFeature )
        {
            CPLDebug("GMLAS",
                     "Delete feature m_aoStackContext[%d].m_poFeature=%p",
                    static_cast<int>(i), m_aoStackContext[i].m_poFeature);
            delete m_aoStackContext[i].m_poFeature;
        }
    }
    {
        int i = 0;
        for( auto& feature: m_aoFeaturesReady )
        {
            CPLDebug("GMLAS", "Delete feature m_aoFeaturesReady[%d].first=%p",
                     i, feature.first);
            delete feature.first;
            ++i;
        }
    }
    if( !m_apsXMLNodeStack.empty() )
    {
        CPLDestroyXMLNode(m_apsXMLNodeStack[0].psNode);
    }
    // No need to take care of m_apoSWEDataArrayLayers. Ownerships belongs to
    // the datasource.
    delete m_poEntityResolver;
}

/************************************************************************/
/*                          SetLayerOfInterest()                        */
/************************************************************************/

void GMLASReader::SetLayerOfInterest( OGRGMLASLayer* poLayer )
{
    m_poLayerOfInterest = poLayer;
}

/************************************************************************/
/*                        SetSWEDataArrayLayers()                       */
/************************************************************************/

void GMLASReader::SetSWEDataArrayLayers( const std::vector<OGRGMLASLayer*>& ar )
{
    m_apoSWEDataArrayLayers = ar;
    m_bProcessSWEDataArray = !ar.empty();
}

/************************************************************************/
/*                          LoadXSDInParser()                           */
/************************************************************************/

bool GMLASReader::LoadXSDInParser( SAX2XMLReader* poParser,
                                   GMLASXSDCache& oCache,
                                   GMLASBaseEntityResolver& oXSDEntityResolver,
                                   const CPLString& osBaseDirname,
                                   const CPLString& osXSDFilename,
                                   Grammar** ppoGrammar,
                                   bool bSchemaFullChecking,
                                   bool bHandleMultipleImports )
{
    if( ppoGrammar != nullptr )
        *ppoGrammar = nullptr;

    const CPLString osModifXSDFilename(
        (osXSDFilename.find("http://") != 0 &&
        osXSDFilename.find("https://") != 0 &&
        CPLIsFilenameRelative(osXSDFilename)) ?
            CPLString(CPLFormFilename(osBaseDirname, osXSDFilename, nullptr)) :
            osXSDFilename );
    CPLString osResolvedFilename;
    VSILFILE* fpXSD = oCache.Open( osModifXSDFilename, CPLString(),
                                   osResolvedFilename );
    if( fpXSD == nullptr )
    {
        return false;
    }

    poParser->setFeature (XMLUni::fgXercesSchemaFullChecking,
                            bSchemaFullChecking);
    poParser->setFeature( XMLUni::fgXercesHandleMultipleImports,
                            bHandleMultipleImports );

    // Install a temporary entity resolved based on the current XSD
    CPLString osXSDDirname( CPLGetDirname(osModifXSDFilename) );
    if( osXSDFilename.find("http://") == 0 ||
        osXSDFilename.find("https://") == 0 )
    {
        osXSDDirname = CPLGetDirname(("/vsicurl_streaming/" +
                                     osModifXSDFilename).c_str());
    }
    oXSDEntityResolver.SetBasePath(osXSDDirname);
    oXSDEntityResolver.DoExtraSchemaProcessing( osResolvedFilename, fpXSD );

    EntityResolver* poOldEntityResolver = poParser->getEntityResolver();
    poParser->setEntityResolver( &oXSDEntityResolver );

    // Install a temporary error handler
    GMLASErrorHandler oErrorHandler;
    oErrorHandler.SetSchemaFullCheckingEnabled( bSchemaFullChecking );
    oErrorHandler.SetHandleMultipleImportsEnabled( bHandleMultipleImports );
    ErrorHandler* poOldErrorHandler = poParser->getErrorHandler();
    poParser->setErrorHandler( &oErrorHandler);

    GMLASInputSource oSource(osResolvedFilename, fpXSD, false);
    const bool bCacheGrammar = true;
    Grammar* poGrammar = nullptr;
    std::string osLoadGrammarErrorMsg("loadGrammar failed");

    const int nMaxMem = std::min(2048, std::max(0, atoi(
        CPLGetConfigOption("OGR_GMLAS_XERCES_MAX_MEMORY", "500"))));
    const std::string osMsgMaxMem = CPLSPrintf(
        "Xerces-C memory allocation exceeds %d MB. "
        "This can happen on schemas with a big value for maxOccurs. "
        "Define the OGR_GMLAS_XERCES_MAX_MEMORY configuration option to a "
        "bigger value (in MB) to increase that limitation, "
        "or 0 to remove it completely.",
        nMaxMem);
    const double dfTimeout = CPLAtof(
        CPLGetConfigOption("OGR_GMLAS_XERCES_MAX_TIME", "2"));
    const std::string osMsgTimeout = CPLSPrintf(
        "Processing in Xerces exceeded maximum allowed of %.3f s. "
        "This can happen on schemas with a big value for maxOccurs. "
        "Define the OGR_GMLAS_XERCES_MAX_TIME configuration option to a "
        "bigger value (in second) to increase that limitation, "
        "or 0 to remove it completely.",
        dfTimeout);
    OGRStartXercesLimitsForThisThread(static_cast<size_t>(nMaxMem) * 1024 * 1024,
                                      osMsgMaxMem.c_str(),
                                      dfTimeout,
                                      osMsgTimeout.c_str());
    try
    {
        poGrammar = poParser->loadGrammar(oSource,
                                            Grammar::SchemaGrammarType,
                                            bCacheGrammar);
    }
    catch( const SAXException& e )
    {
        osLoadGrammarErrorMsg += ": "+ transcode(e.getMessage());
    }
    catch( const XMLException& e )
    {
        osLoadGrammarErrorMsg += ": "+ transcode(e.getMessage());
    }
    catch( const OutOfMemoryException& e )
    {
        if( strstr(CPLGetLastErrorMsg(), "configuration option") == nullptr )
        {
            osLoadGrammarErrorMsg += ": "+ transcode(e.getMessage());
        }
    }
    catch( const DOMException& e )
    {
        // Can happen with a .xsd that has a bad <?xml version="
        // declaration.
        osLoadGrammarErrorMsg += ": "+ transcode(e.getMessage());
    }
    OGRStopXercesLimitsForThisThread();

    // Restore previous handlers
    poParser->setEntityResolver( poOldEntityResolver );
    poParser->setErrorHandler( poOldErrorHandler );
    VSIFCloseL(fpXSD);

    if( poGrammar == nullptr )
    {
        if( !osLoadGrammarErrorMsg.empty() )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "%s",
                     osLoadGrammarErrorMsg.c_str());
        }
        return false;
    }
    if( oErrorHandler.hasFailed() )
    {
        return false;
    }

    if( ppoGrammar != nullptr )
        *ppoGrammar = poGrammar;

    return true;
}

/************************************************************************/
/*                                  Init()                              */
/************************************************************************/

bool GMLASReader::Init(const char* pszFilename,
                       VSILFILE* fp,
                       const std::map<CPLString, CPLString>& oMapURIToPrefix,
                       std::vector<OGRGMLASLayer*>* papoLayers,
                       bool bValidate,
                       const std::vector<PairURIFilename>& aoXSDs,
                       bool bSchemaFullChecking,
                       bool bHandleMultipleImports)
{
    m_oMapURIToPrefix = oMapURIToPrefix;
    m_papoLayers = papoLayers;
    m_bValidate = bValidate;

    m_poSAXReader = XMLReaderFactory::createXMLReader();

    // Commonly useful configuration.
    //
    m_poSAXReader->setFeature (XMLUni::fgSAX2CoreNameSpaces, true);
    m_poSAXReader->setFeature (XMLUni::fgSAX2CoreNameSpacePrefixes, true);

    m_poSAXReader->setContentHandler( this );
    m_poSAXReader->setLexicalHandler( this );
    m_poSAXReader->setDTDHandler( this );

    m_oErrorHandler.SetSchemaFullCheckingEnabled( bSchemaFullChecking );
    m_oErrorHandler.SetHandleMultipleImportsEnabled( bHandleMultipleImports );
    m_poSAXReader->setErrorHandler(&m_oErrorHandler);

    m_poSAXReader->setFeature (XMLUni::fgXercesSchemaFullChecking,
                            bSchemaFullChecking);
    m_poSAXReader->setFeature( XMLUni::fgXercesHandleMultipleImports,
                            bHandleMultipleImports );

    if( bValidate )
    {
        // Enable validation.
        m_poSAXReader->setFeature (XMLUni::fgSAX2CoreValidation, true);
        m_poSAXReader->setFeature (XMLUni::fgXercesSchema, true);

        // We want all errors to be reported
        // coverity[unsafe_xml_parse_config]
        m_poSAXReader->setFeature (XMLUni::fgXercesValidationErrorAsFatal, false);

        CPLString osBaseDirname( CPLGetDirname(pszFilename) );

        // In the case the schemas are explicitly passed, we must do special
        // processing
        if( !aoXSDs.empty() )
        {
            GMLASBaseEntityResolver oXSDEntityResolver( CPLString(), m_oCache );
            for( size_t i = 0; i < aoXSDs.size(); i++ )
            {
                const CPLString osXSDFilename(aoXSDs[i].second);
                if( !LoadXSDInParser( m_poSAXReader, m_oCache,
                                      oXSDEntityResolver,
                                      osBaseDirname, osXSDFilename,
                                      nullptr,
                                      bSchemaFullChecking,
                                      bHandleMultipleImports) )
                {
                    return false;
                }
            }

            // Make sure our previously loaded schemas are used
            m_poSAXReader->setFeature (XMLUni::fgXercesUseCachedGrammarInParse,
                                       true);

            // Don't load schemas from any other source (e.g., from XML document's
            // xsi:schemaLocation attributes).
            //
            m_poSAXReader->setFeature (XMLUni::fgXercesLoadSchema, false);
        }

        // Install entity resolver based on XML file
        m_poEntityResolver = new GMLASBaseEntityResolver(
                                                osBaseDirname,
                                                m_oCache );
        m_poSAXReader->setEntityResolver( m_poEntityResolver );
    }
    else
    {
        // Don't load schemas from any other source (e.g., from XML document's
        // xsi:schemaLocation attributes).
        //
        m_poSAXReader->setFeature (XMLUni::fgXercesLoadSchema, false);
        m_poSAXReader->setEntityResolver( this );
    }

    m_fp = fp;
    m_GMLInputSource = new GMLASInputSource(pszFilename, fp, false);

    return true;
}

/************************************************************************/
/*                             IsArrayType()                            */
/************************************************************************/

static bool IsArrayType( OGRFieldType eType )
{
    return eType == OFTIntegerList ||
           eType == OFTInteger64List ||
           eType == OFTRealList ||
           eType == OFTStringList;
}

/************************************************************************/
/*                                SetField()                            */
/************************************************************************/

void GMLASReader::SetField( OGRFeature* poFeature,
                            OGRGMLASLayer* poLayer,
                            int nAttrIdx,
                            const CPLString& osAttrValue )
{
    const OGRFieldType eType(poFeature->GetFieldDefnRef(nAttrIdx)->GetType());
    if( osAttrValue.empty() )
    {
        if( eType == OFTString &&
            !poFeature->GetFieldDefnRef(nAttrIdx)->IsNullable() )
        {
            poFeature->SetField( nAttrIdx, "" );
        }
    }
    else if( eType == OFTDate || eType == OFTDateTime )
    {
        OGRField sField;
        if( OGRParseXMLDateTime(
                (m_bInitialPass) ? "1970-01-01T00:00:00" : osAttrValue.c_str(),
                &sField ) )
        {
            poFeature->SetField( nAttrIdx, &sField );
        }
    }
    // Transform boolean values to something that OGR understands
    else if( eType == OFTInteger &&
             poFeature->GetFieldDefnRef(nAttrIdx)->GetSubType() == OFSTBoolean )
    {
        if( osAttrValue == "true" )
            poFeature->SetField( nAttrIdx, TRUE );
        else
            poFeature->SetField( nAttrIdx, FALSE );
    }
    else if( eType == OFTBinary )
    {
        const int nFCFieldIdx =
            poLayer->GetFCFieldIndexFromOGRFieldIdx(nAttrIdx);
        if( nFCFieldIdx >= 0 )
        {
            const GMLASField& oField(
                poLayer->GetFeatureClass().GetFields()[nFCFieldIdx]);
            if( m_bInitialPass )
            {
                poFeature->SetField( nAttrIdx, 1, (GByte*)("X") );
            }
            else if( oField.GetType() == GMLAS_FT_BASE64BINARY )
            {
                GByte* pabyBuffer = reinterpret_cast<GByte*>(
                                                    CPLStrdup(osAttrValue));
                int nBytes = CPLBase64DecodeInPlace(pabyBuffer);
                poFeature->SetField( nAttrIdx, nBytes, pabyBuffer );
                CPLFree(pabyBuffer);
            }
            else
            {
                int nBytes = 0;
                GByte* pabyBuffer = CPLHexToBinary( osAttrValue, &nBytes );
                poFeature->SetField( nAttrIdx, nBytes, pabyBuffer );
                CPLFree(pabyBuffer);
            }
        }
    }
    else if( IsArrayType(eType) )
    {
        const int nFCFieldIdx =
            poLayer->GetFCFieldIndexFromOGRFieldIdx(nAttrIdx);
        if( nFCFieldIdx >= 0 &&
            poLayer->GetFeatureClass().GetFields()[nFCFieldIdx].IsList() )
        {
            char** papszTokens = CSLTokenizeString2( osAttrValue.c_str(), " ", 0 );
            if( eType == OFTIntegerList &&
                poFeature->GetFieldDefnRef(nAttrIdx)->GetSubType() == OFSTBoolean )
            {
                for( char** papszIter = papszTokens; *papszIter != nullptr; ++papszIter )
                {
                    if( strcmp(*papszIter, "true") == 0 )
                    {
                        (*papszIter)[0] = '1';
                        (*papszIter)[1] = '\0';
                    }
                    else if( strcmp(*papszIter, "false") == 0 )
                    {
                        (*papszIter)[0] = '0';
                        (*papszIter)[1] = '\0';
                    }
                }
            }
            poFeature->SetField( nAttrIdx, papszTokens );
            CSLDestroy(papszTokens);
        }
        else
        {
            poFeature->SetField( nAttrIdx, osAttrValue.c_str() );
        }
    }
    else
    {
        poFeature->SetField( nAttrIdx, osAttrValue.c_str() );
    }
}

/************************************************************************/
/*                          PushFeatureReady()                          */
/************************************************************************/

void GMLASReader::PushFeatureReady( OGRFeature* poFeature,
                                    OGRGMLASLayer* poLayer )
{
#ifdef DEBUG_VERBOSE
    CPLDebug("GMLAS", "PushFeatureReady(%p / %s / %s)",
             poFeature, poFeature->GetDefnRef()->GetName(), poLayer->GetName());
#endif

    m_aoFeaturesReady.push_back(
        std::pair<OGRFeature*, OGRGMLASLayer*>(poFeature, poLayer) );
}

/************************************************************************/
/*                          CreateNewFeature                            */
/************************************************************************/

void GMLASReader::CreateNewFeature(const CPLString& osLocalname)
{
    m_oCurCtxt.m_poFeature = new OGRFeature(
                m_oCurCtxt.m_poLayer->GetLayerDefn() );
#ifdef DEBUG_VERBOSE
    CPLDebug("GMLAS", "CreateNewFeature(element=%s / layer=%s) = %p",
             osLocalname.c_str(), m_oCurCtxt.m_poLayer->GetName(),
             m_oCurCtxt.m_poFeature);
#endif
    // Assign FID (1, ...). Only for OGR compliance, but definitely
    // not a unique ID among datasets with the same schema
    ++m_oMapGlobalCounter[m_oCurCtxt.m_poLayer];
    const int nGlobalCounter =
                    m_oMapGlobalCounter[m_oCurCtxt.m_poLayer];
    m_oCurCtxt.m_poFeature->SetFID(nGlobalCounter);

    // Find parent ID
    CPLString osParentId;
    if( !m_aoStackContext.empty() &&
        m_oCurCtxt.m_poLayer->GetParentIDFieldIdx() >= 0 )
    {
        CPLAssert(m_aoStackContext.back().
                            m_poLayer->GetIDFieldIdx() >= 0 );
        osParentId = m_aoStackContext.back().m_poFeature->
            GetFieldAsString(
            m_aoStackContext.back().m_poLayer->GetIDFieldIdx() );
        m_oCurCtxt.m_poFeature->SetField(
            m_oCurCtxt.m_poLayer->GetParentIDFieldIdx(),
            osParentId.c_str() );
    }

    // Should we generate a unique (child) ID from the parent ID ?
    if( m_oCurCtxt.m_poLayer->IsGeneratedIDField() )
    {
        // Local IDs (ie related to a parent feature are fine, but when
        // we might have cycles, that doesn't work anymore
        /*
        ++m_oCurCtxt.m_oMapCounter[m_oCurCtxt.m_poLayer];
        const int nCounter =
            m_oCurCtxt.m_oMapCounter[m_oCurCtxt.m_poLayer];*/
        const int nCounter = nGlobalCounter;

        CPLString osGeneratedID = (osParentId.empty() ? m_osHash : osParentId) +
                                   "_" + osLocalname +
                                   CPLSPrintf("_%d", nCounter);
        m_oCurCtxt.m_poFeature->SetField(
                        m_oCurCtxt.m_poLayer->GetIDFieldIdx(),
                        osGeneratedID.c_str() );
    }

    m_nCurFieldIdx = -1;
}

/************************************************************************/
/*                         AttachAsLastChild()                          */
/************************************************************************/

/* Attach element as the last child of its parent */
void GMLASReader::AttachAsLastChild(CPLXMLNode* psNode)
{
    NodeLastChild& sNodeLastChild = m_apsXMLNodeStack.back();
    CPLXMLNode* psLastChildParent = sNodeLastChild.psLastChild;

    if (psLastChildParent == nullptr)
    {
        CPLAssert( sNodeLastChild.psNode );
        sNodeLastChild.psNode->psChild = psNode;
    }
    else
    {
        psLastChildParent->psNext = psNode;
    }
    sNodeLastChild.psLastChild = psNode;
}

/************************************************************************/
/*                         BuildXMLBlobStartElement()                   */
/************************************************************************/

void GMLASReader::BuildXMLBlobStartElement(const CPLString& osXPath,
                                           const  Attributes& attrs)
{
    if( FillTextContent() )
    {
        m_osTextContent += "<";
        m_osTextContent += osXPath;
    }

    CPLXMLNode* psNode = nullptr;
    if( m_nCurGeomFieldIdx >= 0 || m_nSWEDataArrayLevel >= 0 ||
        m_nSWEDataRecordLevel >= 0 )
    {
        psNode = CPLCreateXMLNode( nullptr, CXT_Element, osXPath );
        if( !m_apsXMLNodeStack.empty() )
        {
            AttachAsLastChild(psNode);
        }
    }

    CPLXMLNode* psLastChild = nullptr;
    for(unsigned int i=0; i < attrs.getLength(); i++)
    {
        const CPLString& osAttrNSPrefix( m_osAttrNSPrefix =
            m_oMapURIToPrefix[ transcode( attrs.getURI(i), m_osAttrNSUri ) ] );
        const CPLString& osAttrLocalname(
                        transcode(attrs.getLocalName(i), m_osAttrLocalName) );
        const CPLString& osAttrValue(
                                transcode(attrs.getValue(i), m_osAttrValue) );
        CPLString& osAttrXPath( m_osAttrXPath );
        if( !osAttrNSPrefix.empty() )
        {
            osAttrXPath.reserve(
                        osAttrNSPrefix.size() + 1 + osAttrLocalname.size() );
            osAttrXPath = osAttrNSPrefix;
            osAttrXPath += ":";
            osAttrXPath += osAttrLocalname;
        }
        else
        {
            osAttrXPath = osAttrLocalname;
        }

        if( psNode != nullptr )
        {
            CPLXMLNode* psAttrNode = CPLCreateXMLNode( nullptr, CXT_Attribute,
                                                       osAttrXPath );
            CPLCreateXMLNode(psAttrNode, CXT_Text, osAttrValue);

            if( psLastChild == nullptr )
            {
                psNode->psChild = psAttrNode;
            }
            else
            {
                psLastChild->psNext = psAttrNode;
            }
            psLastChild = psAttrNode;
        }

        if( FillTextContent() )
        {
            m_osTextContent += " ";
            m_osTextContent += osAttrXPath;
            m_osTextContent += "=\"";
            char* pszEscaped = CPLEscapeString( osAttrValue.c_str(),
                                        static_cast<int>(osAttrValue.size()),
                                        CPLES_XML );
            m_osTextContent += pszEscaped;
            CPLFree(pszEscaped);
            m_osTextContent += '"';
        }
    }
    if( FillTextContent() )
        m_osTextContent += ">";

    if( psNode != nullptr )
    {
        /* Push the element on the stack */
        NodeLastChild sNewNodeLastChild;
        sNewNodeLastChild.psNode = psNode;
        sNewNodeLastChild.psLastChild = psLastChild;
        m_apsXMLNodeStack.push_back(sNewNodeLastChild);
#ifdef DEBUG_VERBOSE
        CPLDebug("GMLAS", "m_apsXMLNodeStack.push_back()");
#endif
    }

    if( m_osTextContent.size() > m_nMaxContentSize )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                "Too much data in a single element");
        m_bParsingError = true;
    }
}

/************************************************************************/
/*                          GetLayerByXPath()                           */
/************************************************************************/

OGRGMLASLayer* GMLASReader::GetLayerByXPath( const CPLString& osXPath )
{
    for(size_t i = 0; i < m_papoLayers->size(); i++ )
    {
        if( (*m_papoLayers)[i]->GetFeatureClass().GetXPath() == osXPath )
        {
            return (*m_papoLayers)[i];
        }
    }
    return nullptr;
}

/************************************************************************/
/*                            PushContext()                             */
/************************************************************************/

void GMLASReader::PushContext( const Context& oContext )
{
    m_aoStackContext.push_back( oContext );
#ifdef DEBUG_VERBOSE
    CPLDebug("GMLAS", "Pushing new context:");
    oContext.Dump();
#endif
}

/************************************************************************/
/*                            PopContext()                              */
/************************************************************************/

void GMLASReader::PopContext()
{
#ifdef DEBUG_VERBOSE
    if( !m_aoStackContext.empty() )
    {
        CPLDebug("GMLAS", "Popping up context:");
        m_aoStackContext.back().Dump();
    }
#endif
    m_aoStackContext.pop_back();
#ifdef DEBUG_VERBOSE
    if( !m_aoStackContext.empty() )
    {
        CPLDebug("GMLAS", "New top of stack is:");
        m_aoStackContext.back().Dump();
    }
#endif
}

/************************************************************************/
/*                             startElement()                           */
/************************************************************************/

/* <xs:group ref="somegroup" maxOccurs="unbounded"/> are particularly hard to
   deal with since we cannot easily know when the corresponding subfeature
   is exactly terminated.

   Let's consider:

        <xs:group name="somegroup">
            <xs:choice>
                <xs:element name="first_elt_of_group" type="xs:string"/>
                <xs:element name="second_elt_of_group" type="xs:string"/>
            </xs:choice>
        </xs:group>

        <xs:group name="another_group">
            <xs:choice>
                <xs:element name="first_elt_of_another_group" type="xs:string"/>
            </xs:choice>
        </xs:group>

   There are different cases :
    *
              <first_elt_of_group>...</first_elt_of_group>
              <second_elt_of_group>...</first_elt_of_group>
              <first_elt_of_group>  <!-- we are here at startElement() -->
                ...
              </first_elt_of_group>

    *
              <first_elt_of_group>...</first_elt_of_group>
              <first_elt_of_group>  <!-- we are here at startElement() -->
                ...</first_elt_of_group>

    *
              <first_elt_of_group>...</first_elt_of_group>
              <first_elt_of_another_group>  <!-- we are here at startElement() -->
                ...</first_elt_of_another_group>

    *
              <first_elt_of_group>...</first_elt_of_group>
              <some_other_elt>  <!-- we are here at startElement() -->
                ...</some_other_elt>

    *
            <first_elt>...</first_elt>
            <second_elt><sub>...</sub></second_elt>
            <first_elt> <-- here -->
                ...</first_elt>
    *
                <first_elt_of_group>...</first_elt_of_group>
            </end_of_enclosing_element>   <!-- we are here at endElement() -->
*/
void GMLASReader::startElement(
            const   XMLCh* const    uri,
            const   XMLCh* const    localname,
            const   XMLCh* const
#ifdef DEBUG_VERBOSE
                                    qname
#endif
            , const   Attributes& attrs
        )
{
    const CPLString& osLocalname( transcode(localname, m_osLocalname) );
    const CPLString& osNSURI( transcode(uri, m_osNSUri) );
    const CPLString& osNSPrefix( m_osNSPrefix = m_oMapURIToPrefix[osNSURI] );
    if( osNSPrefix.empty() )
        m_osXPath = osLocalname;
    else
    {
        m_osXPath.reserve( osNSPrefix.size() + 1 + osLocalname.size() );
        m_osXPath = osNSPrefix;
        m_osXPath += ":";
        m_osXPath += osLocalname;
    }
    const CPLString& osXPath( m_osXPath );
#ifdef DEBUG_VERBOSE
    CPLDebug("GMLAS", "startElement(%s / %s)",
             transcode(qname).c_str(), osXPath.c_str());
#endif
    m_anStackXPathLength.push_back(osXPath.size());
    if( !m_osCurXPath.empty() )
        m_osCurXPath += "/";
    m_osCurXPath += osXPath;

#if 0
    CPLString osSubXPathBefore(m_osCurSubXPath);
#endif
    if( !m_osCurSubXPath.empty() )
    {
        m_osCurSubXPath += "/";
        m_osCurSubXPath += osXPath;
    }

    if( m_bProcessSWEDataArray && m_nSWEDataArrayLevel < 0 &&
        m_nSWEDataRecordLevel < 0 && m_nCurGeomFieldIdx < 0 )
    {
        if( osNSURI == szSWE_URI &&
            (osLocalname == "DataArray" || osLocalname == "DataStream") )
        {
            if( m_nCurFieldIdx >= 0 )
            {
                m_osSWEDataArrayParentField =
                    m_oCurCtxt.m_poFeature->
                                GetFieldDefnRef(m_nCurFieldIdx)->GetNameRef();
            }
            else
            {
                m_osSWEDataArrayParentField.clear();
            }
            m_nSWEDataArrayLevel = m_nLevel;
        }
    }

    // Deal with XML content
    if( m_bIsXMLBlob || m_nSWEDataArrayLevel >= 0 ||
        m_nSWEDataRecordLevel >= 0 )
    {
        BuildXMLBlobStartElement(osXPath, attrs);
    }

    if( m_bIsXMLBlob )
    {
        m_nLevel ++;
        return;
    }

    if( m_nLevel == m_nMaxLevel )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Too deeply nested XML content");
        m_bParsingError = true;
        return;
    }

    CPLAssert(m_aoFeaturesReady.empty());

    // Look which layer might match the current XPath
    for(size_t i = 0; i < m_papoLayers->size(); i++ )
    {
        const CPLString* posLayerXPath =
            &((*m_papoLayers)[i]->GetFeatureClass().GetXPath());
        if( (*m_papoLayers)[i]->GetFeatureClass().IsRepeatedSequence() )
        {
            size_t iPosExtra = posLayerXPath->find(szEXTRA_SUFFIX);
            if (iPosExtra != std::string::npos)
            {
                m_osLayerXPath = *posLayerXPath;
                m_osLayerXPath.resize(iPosExtra);
                posLayerXPath = &m_osLayerXPath;
            }
        }

        const bool bIsGroup = (*m_papoLayers)[i]->GetFeatureClass().IsGroup();

        // Are we entering or staying in a group ?
        const bool bIsMatchingGroup =
            (bIsGroup &&
             (*m_papoLayers)[i]->GetOGRFieldIndexFromXPath(m_osCurSubXPath) != -1 );

        const bool bIsMatchingRepeatedSequence =
             ((*m_papoLayers)[i]->GetFeatureClass().IsRepeatedSequence()  &&
             m_oCurCtxt.m_poLayer != nullptr &&
             m_oCurCtxt.m_poLayer != (*m_papoLayers)[i] &&
             m_oCurCtxt.m_poLayer->GetFeatureClass().GetXPath() ==
                    *posLayerXPath &&
             (*m_papoLayers)[i]->GetOGRFieldIndexFromXPath(m_osCurSubXPath) >= 0);

        int nTmpIdx;
        if( // Case where we haven't yet entered the top-level element, which may
            // be in container elements
            (m_osCurSubXPath.empty() &&
             *posLayerXPath == osXPath && !bIsGroup) ||

            // Case where we are a sub-element of a top-level feature
            (!m_osCurSubXPath.empty() &&
             *posLayerXPath == m_osCurSubXPath && !bIsGroup) ||

            // Case where we are a sub-element of a (repeated) group of a
            // top-level feature
            bIsMatchingGroup ||

            // Needed to handle sequence_1_unbounded_non_simplifiable.subelement case of data/gmlas_test1.xml
            bIsMatchingRepeatedSequence ||

            // Case where we go back from a sub-element of a (repeated) group
            // of a top-level feature to a regular sub-element of that top-level
            // feature
            (m_oCurCtxt.m_poGroupLayer != nullptr &&
             ((nTmpIdx = (*m_papoLayers)[i]->GetOGRFieldIndexFromXPath(m_osCurSubXPath)) >= 0 ||
              nTmpIdx == IDX_COMPOUND_FOLDED)) )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("GMLAS", "Matches layer %s (%s)",
                     (*m_papoLayers)[i]->GetName(),
                     (*m_papoLayers)[i]->GetFeatureClass().GetXPath().c_str());
#endif

            if( (*m_papoLayers)[i]->GetParent() != nullptr &&
                (*m_papoLayers)[i]->GetParent()->GetFeatureClass().IsRepeatedSequence() &&
                m_oCurCtxt.m_poGroupLayer != (*m_papoLayers)[i]->GetParent() )
            {
                // Yuck! Simulate top-level element of a group if we directly jump
                // into a nested class of it !
                /* Something like
                    <xs:group name="group">
                        <xs:sequence>
                            <xs:element name="optional_elt" type="xs:string" minOccurs="0"/>
                            <xs:element name="elt">
                                <xs:complexType>
                                    <xs:sequence>
                                        <xs:element name="subelt"  type="xs:dateTime" maxOccurs="unbounded"/>
                                    </xs:sequence>
                                </xs:complexType>
                            </xs:element>
                        </xs:sequence>
                    </xs:group>

                    <top_element>
                        <elt><subelt>...</subelt></elt>
                    </top_element>
                */
                m_oCurCtxt.m_poLayer = (*m_papoLayers)[i]->GetParent();
                m_oCurCtxt.m_poGroupLayer = m_oCurCtxt.m_poLayer;
                m_oCurCtxt.m_nLevel = m_nLevel;
                m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;
                CreateNewFeature( m_oCurCtxt.m_poLayer->GetName() );
            }

            bool bPushNewState = true;
            if( bIsMatchingGroup )
            {
                int nFieldIdx =
                    (*m_papoLayers)[i]->GetOGRFieldIndexFromXPath(m_osCurSubXPath);
                bool bPushNewFeature = false;
                if( m_oCurCtxt.m_poGroupLayer == nullptr )
                {
                    m_oCurCtxt.m_poFeature = nullptr;
                }
                else if( nFieldIdx < 0 )
                {
                    bPushNewState = false;
                }
                else if ( m_oCurCtxt.m_nGroupLayerLevel == m_nLevel &&
                          m_oCurCtxt.m_poGroupLayer != (*m_papoLayers)[i] )
                {
#ifdef DEBUG_VERBOSE
                    CPLDebug("GMLAS", "new feature: group case 1");
#endif
                    /* Case like:
                            <first_elt_of_group>...</first_elt_of_group>
                            <first_elt_of_another_group>  <!-- we are here at startElement() -->
                                ...</first_elt_of_group>
                    */
                    bPushNewFeature = true;
                }
                else if( m_oCurCtxt.m_nGroupLayerLevel == m_nLevel &&
                         m_oCurCtxt.m_poGroupLayer == (*m_papoLayers)[i] &&
                         nFieldIdx == m_oCurCtxt.m_nLastFieldIdxGroupLayer &&
                         !IsArrayType(m_oCurCtxt.m_poFeature->
                                        GetFieldDefnRef(nFieldIdx)->GetType()))
                {
#ifdef DEBUG_VERBOSE
                    CPLDebug("GMLAS", "new feature: group case 2");
#endif
                    /* Case like:
                        <first_elt>...</first_elt>
                        <first_elt> <-- here -->
                    */
                    bPushNewFeature = true;
                }
                else if ( m_oCurCtxt.m_nGroupLayerLevel == m_nLevel &&
                          nFieldIdx < m_oCurCtxt.m_nLastFieldIdxGroupLayer )
                {
#ifdef DEBUG_VERBOSE
                    CPLDebug("GMLAS", "new feature: group case nFieldIdx < m_oCurCtxt.m_nLastFieldIdxGroupLayer" );
#endif
                    /* Case like:
                            <first_elt_of_group>...</first_elt_of_group>
                            <second_elt_of_group>...</first_elt_of_group>
                            <first_elt_of_group>  <!-- we are here at startElement() -->
                                ...
                            </first_elt_of_group>
                    */
                    bPushNewFeature = true;
                }
                else if ( m_oCurCtxt.m_nGroupLayerLevel == m_nLevel + 1 &&
                          m_oCurCtxt.m_poGroupLayer == (*m_papoLayers)[i] )
                {
#ifdef DEBUG_VERBOSE
                    CPLDebug("GMLAS", "new feature: group case 3");
#endif
                    /* Case like:
                        <first_elt>...</first_elt>
                        <second_elt><sub>...</sub></second_elt>
                        <first_elt> <-- here -->
                            ...</first_elt>
                    */
                    bPushNewFeature = true;
                }
                if( bPushNewFeature )
                {
                    CPLAssert( m_oCurCtxt.m_poFeature );
                    CPLAssert( m_oCurCtxt.m_poGroupLayer );
                    //CPLDebug("GMLAS", "Feature ready");
                    PushFeatureReady(m_oCurCtxt.m_poFeature,
                                     m_oCurCtxt.m_poGroupLayer);
                    m_oCurCtxt.m_poFeature = nullptr;
                    m_nCurFieldIdx = -1;
                }
                m_oCurCtxt.m_poLayer = (*m_papoLayers)[i];
                m_oCurCtxt.m_poGroupLayer = (*m_papoLayers)[i];
                m_oCurCtxt.m_nGroupLayerLevel = m_nLevel;
                if( nFieldIdx >= 0 )
                    m_oCurCtxt.m_nLastFieldIdxGroupLayer = nFieldIdx;
            }
            else
            {
                if( m_oCurCtxt.m_nGroupLayerLevel == m_nLevel &&
                    (*m_papoLayers)[i] == m_aoStackContext.back().m_poLayer )
                {
                    // This is the case where we switch from an element that was
                    // in a group to a regular element of the same level

                    // Push group feature as ready
                    CPLAssert( m_oCurCtxt.m_poFeature );

                    //CPLDebug("GMLAS", "Feature ready");
                    PushFeatureReady(m_oCurCtxt.m_poFeature,
                                     m_oCurCtxt.m_poGroupLayer);

                    // Restore "top-level" context
                    CPLAssert( !m_aoStackContext.empty() );
                    m_oCurCtxt = m_aoStackContext.back();
                    bPushNewState = false;
                }
                else
                {
                    if( m_oCurCtxt.m_poGroupLayer )
                    {
                        Context oContext;
                        oContext = m_oCurCtxt;
                        oContext.m_nLevel = -1;
                        oContext.Dump();
                        PushContext( oContext );
                    }

                    m_oCurCtxt.m_poFeature = nullptr;
                    m_oCurCtxt.m_poGroupLayer = nullptr;
                    m_oCurCtxt.m_nGroupLayerLevel = -1;
                    m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;
                    m_oCurCtxt.m_poLayer = (*m_papoLayers)[i];
                    if( m_aoStackContext.empty() )
                        m_osCurSubXPath = osXPath;
                }
            }

            if( m_oCurCtxt.m_poFeature == nullptr )
            {
                CPLAssert( bPushNewState );
                CreateNewFeature(osLocalname);
            }

            if( bPushNewState )
            {
                Context oContext;
                oContext = m_oCurCtxt;
                oContext.m_nLevel = m_nLevel;
                PushContext( oContext );
                m_oCurCtxt.m_oMapCounter.clear();
            }
            break;
        }
    }

    if( m_oCurCtxt.m_poLayer )
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("GMLAS", "Current layer: %s", m_oCurCtxt.m_poLayer->GetName() );
#endif

        bool bHasProcessedAttributes = false;

        // Find if we can match this element with one of our fields
        int idx = m_oCurCtxt.m_poLayer->
                            GetOGRFieldIndexFromXPath(m_osCurSubXPath);
        int geom_idx = m_oCurCtxt.m_poLayer->
                            GetOGRGeomFieldIndexFromXPath(m_osCurSubXPath);

        if( idx < 0 && idx != IDX_COMPOUND_FOLDED )
        {
            /* Special case for a layer that matches everything, as found */
            /* in swe:extension */
            idx = m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(
              m_oCurCtxt.m_poLayer->GetFeatureClass().GetXPath() + szMATCH_ALL);
            if( idx >= 0 &&
                m_oCurCtxt.m_poLayer->GetFeatureClass().GetFields().size() > 1 )
            {
                // But only match this wildcard field if it is the only child
                // of the feature class, otherwise that is going to prevent
                // matching regular fields
                // Practical case  the <any processContents="lax" minOccurs="0" maxOccurs="unbounded">
                // declaratin of
                // http://schemas.earthresourceml.org/earthresourceml-lite/1.0/erml-lite.xsd
                // http://services.ga.gov.au/earthresource/ows?service=wfs&version=2.0.0&request=GetFeature&typenames=erl:CommodityResourceView&count=10
                // FIXME: currently we will thus ignore those extra content
                // See ogr_gmlas_any_field_at_end_of_declaration test case
                idx = -1;
            }
        }
        if( idx < 0 && geom_idx < 0 && geom_idx != IDX_COMPOUND_FOLDED )
        {
            /* Special case for a layer that is a made of only a geometry */
            geom_idx = m_oCurCtxt.m_poLayer->GetOGRGeomFieldIndexFromXPath(
              m_oCurCtxt.m_poLayer->GetFeatureClass().GetXPath() + szMATCH_ALL);
        }

        if( idx >= 0 || geom_idx >= 0 )
        {
            // Sanity check. Shouldn't normally happen !
            if( m_oCurCtxt.m_poFeature == nullptr ||
                m_oCurCtxt.m_poLayer->GetLayerDefn() !=
                                        m_oCurCtxt.m_poFeature->GetDefnRef() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                            "Inconsistent m_poLayer / m_poFeature state");
                m_bParsingError = true;
                return;
            }

            bool bPushNewFeature = false;
            const int nFCFieldIdx = (idx >= 0) ?
                m_oCurCtxt.m_poLayer->GetFCFieldIndexFromOGRFieldIdx(idx) :
                m_oCurCtxt.m_poLayer->GetFCFieldIndexFromOGRGeomFieldIdx(geom_idx);

            /* For cases like
                    <xs:element name="element_compound">
                        <xs:complexType>
                            <xs:sequence maxOccurs="unbounded">
                                <xs:element name="subelement1" type="xs:string"/>
                                <xs:element name="subelement2" type="xs:string"/>
                            </xs:sequence>
                        </xs:complexType>
                    </xs:element>

                    <element_compound>
                        <subelement1>a</subelement>
                        <subelement2>b</subelement>
                        <subelement1>c</subelement>
                        <subelement2>d</subelement>
                    </element_compound>
            */

            if( idx >= 0 && idx < m_nCurFieldIdx )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS", "new feature: idx < m_nCurFieldIdx" );
#endif
                bPushNewFeature = true;
            }

            /* For cases like
                    <xs:element name="element_compound">
                        <xs:complexType>
                            <xs:sequence maxOccurs="unbounded">
                                <xs:element name="subelement" type="xs:dateTime"/>
                            </xs:sequence>
                        </xs:complexType>
                    </xs:element>

                    <element_compound>
                        <subelement>2012-01-01T12:34:56Z</subelement>
                        <subelement>2012-01-02T12:34:56Z</subelement>
                    </element_compound>
            */
            else if( idx >= 0 && idx == m_nCurFieldIdx &&
                     !IsArrayType(m_oCurCtxt.m_poFeature->
                                GetFieldDefnRef(m_nCurFieldIdx)->GetType()) &&
                     // Make sure this isn't a repeated geometry as well
                     !( geom_idx >= 0 && nFCFieldIdx >= 0 &&
                        m_oCurCtxt.m_poLayer->GetFeatureClass().GetFields()[
                                        nFCFieldIdx].GetMaxOccurs() > 1 ) )
            {
                bPushNewFeature = true;
            }

            // Make sure we are in a repeated sequence, otherwise this is
            // invalid XML
            if( bPushNewFeature &&
                !m_oCurCtxt.m_poLayer->GetFeatureClass().IsRepeatedSequence() &&
                // Case of element within xs:choice
                !(idx >= 0 && nFCFieldIdx >= 0 &&
                    m_oCurCtxt.m_poLayer->GetFeatureClass().
                        GetFields()[nFCFieldIdx].MayAppearOutOfOrder()) )
            {
                bPushNewFeature = false;
                CPLError(CE_Warning, CPLE_AppDefined,
                            "Unexpected element %s",
                            m_osCurSubXPath.c_str());
            }

            if( bPushNewFeature )
            {
                //CPLDebug("GMLAS", "Feature ready");
                PushFeatureReady(m_oCurCtxt.m_poFeature, m_oCurCtxt.m_poLayer);
                Context oContext = m_aoStackContext.back();
                m_aoStackContext.pop_back();
                CreateNewFeature(osLocalname);
                oContext.m_poFeature = m_oCurCtxt.m_poFeature;
                m_aoStackContext.push_back( oContext );
                m_oCurCtxt.m_oMapCounter.clear();
            }

            if( m_nCurFieldIdx != idx )
            {
                m_osTextContentList.Clear();
                m_nTextContentListEstimatedSize = 0;
            }
            m_nCurFieldIdx = idx;
            m_nCurGeomFieldIdx = geom_idx;
            m_nCurFieldLevel = m_nLevel + 1;
            m_osTextContent.clear();
            m_bIsXMLBlob = false;
            m_bIsXMLBlobIncludeUpper = false;

#ifdef DEBUG_VERBOSE
            if( idx >= 0 )
            {
                CPLDebug("GMLAS", "Matches field %s", m_oCurCtxt.m_poLayer->
                         GetLayerDefn()->GetFieldDefn(idx)->GetNameRef() );
            }
            if( geom_idx >= 0 )
            {
                CPLDebug("GMLAS", "Matches geometry field %s", m_oCurCtxt.m_poLayer->
                         GetLayerDefn()->GetGeomFieldDefn(geom_idx)->GetNameRef() );
            }
#endif
            if( nFCFieldIdx >= 0 )
            {
                const GMLASField& oField(
                    m_oCurCtxt.m_poLayer->GetFeatureClass().GetFields()[
                                                                nFCFieldIdx]);
                if( m_nSWEDataArrayLevel < 0 && m_nSWEDataRecordLevel < 0 )
                {
                    m_bIsXMLBlob = (oField.GetType() == GMLAS_FT_ANYTYPE ||
                                    m_nCurGeomFieldIdx != -1 );
                }
                m_bIsXMLBlobIncludeUpper = m_bIsXMLBlob &&
                                            oField.GetIncludeThisEltInBlob();
                if( m_bIsXMLBlobIncludeUpper )
                {
                    BuildXMLBlobStartElement(osXPath, attrs);
                    m_nLevel ++;
                    return;
                }

                // Figure out if it is an element that calls for a related
                // top-level feature (but without junction table)
                if( oField.GetCategory() ==
                                GMLASField::PATH_TO_CHILD_ELEMENT_WITH_LINK )
                {
                    const CPLString& osNestedXPath(oField.GetRelatedClassXPath());
                    CPLAssert( !osNestedXPath.empty() );
                    OGRGMLASLayer* poSubLayer = GetLayerByXPath(osNestedXPath);
                    if( poSubLayer && m_nCurFieldIdx >= 0 )
                    {
                        int nOldCurFieldIdx = m_nCurFieldIdx;
                        OGRFeature* poOldCurFeature = m_oCurCtxt.m_poFeature;
                        OGRGMLASLayer* poOldLayer = m_oCurCtxt.m_poLayer;
                        m_oCurCtxt.m_poLayer = poSubLayer;
                        CreateNewFeature(osLocalname);

                        m_oCurCtxt.m_poGroupLayer = nullptr;
                        m_oCurCtxt.m_nGroupLayerLevel = -1;
                        m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;

                        // Install new context
                        Context oContext;
                        oContext = m_oCurCtxt;
                        oContext.m_nLevel = m_nLevel;
                        oContext.m_osCurSubXPath = m_osCurSubXPath;
                        m_osCurSubXPath = osNestedXPath;
#ifdef DEBUG_VERBOSE
                        CPLDebug("GMLAS",
                                 "Installing new m_osCurSubXPath from %s to %s",
                                 oContext.m_osCurSubXPath.c_str(),
                                 m_osCurSubXPath.c_str());
#endif
                        PushContext( oContext );
                        m_oCurCtxt.m_oMapCounter.clear();

                        // Process attributes now because we might need to
                        // fetch the child id from them
                        ProcessAttributes(attrs);
                        bHasProcessedAttributes = true;

                        CPLString osChildId(
                            m_oCurCtxt.m_poFeature->GetFieldAsString(
                                    m_oCurCtxt.m_poLayer->GetIDFieldIdx()));
                        SetField( poOldCurFeature,
                                  poOldLayer,
                                  nOldCurFieldIdx,
                                  osChildId );

                        if( m_bProcessSWEDataRecord && !m_bIsXMLBlob &&
                            m_nSWEDataArrayLevel < 0 &&
                            m_nSWEDataRecordLevel < 0 &&
                            osNestedXPath == "swe:DataRecord" )
                        {
                            m_nSWEDataRecordLevel = m_nLevel;
                            BuildXMLBlobStartElement(osXPath, attrs);
                        }
                    }
                }
            }
        }

#if 0
        // Case where we have an abstract type and don't know its realizations
        else if ( idx != IDX_COMPOUND_FOLDED &&
            (idx = m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(
                                    osSubXPathBefore + "/" + "*")) >= 0 &&
            m_oCurCtxt.m_poGroupLayer == NULL )
        {
            m_nCurFieldIdx = idx;
            m_nCurFieldLevel = m_nLevel + 1;
            m_osTextContent.clear();
            m_bIsXMLBlob = true;
            m_bIsXMLBlobIncludeUpper = true;
            BuildXMLBlobStartElement(osNSPrefix, osLocalname, attrs);
            m_nLevel ++;
            return;
        }
#endif

        else if( m_nLevel > m_aoStackContext.back().m_nLevel )
        {
            // Figure out if it is an element that calls from a related
            // top-level feature with a junction table
            const std::vector<GMLASField>& aoFields =
                    m_oCurCtxt.m_poLayer->GetFeatureClass().GetFields();
            for( size_t i = 0; i < aoFields.size(); ++i )
            {
                if( aoFields[i].GetCategory() ==
                        GMLASField::PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE &&
                    aoFields[i].GetXPath() == m_osCurSubXPath )
                {
                    const CPLString& osAbstractElementXPath(
                                        aoFields[i].GetAbstractElementXPath());
                    const CPLString& osNestedXPath(
                                        aoFields[i].GetRelatedClassXPath());
                    CPLAssert( !osAbstractElementXPath.empty() );
                    CPLAssert( !osNestedXPath.empty() );

                    OGRGMLASLayer* poJunctionLayer = GetLayerByXPath(
                        GMLASSchemaAnalyzer::BuildJunctionTableXPath(
                            osAbstractElementXPath, osNestedXPath));
                    OGRGMLASLayer* poSubLayer = GetLayerByXPath(osNestedXPath);

                    if( poSubLayer && poJunctionLayer )
                    {
                        CPLString osParentId(
                            m_oCurCtxt.m_poFeature->GetFieldAsString(
                                    m_oCurCtxt.m_poLayer->GetIDFieldIdx()));

                        // Create child feature
                        m_oCurCtxt.m_poLayer = poSubLayer;
                        CreateNewFeature(osLocalname);

                        ++m_oMapGlobalCounter[poJunctionLayer];
                        const int nGlobalCounter =
                                        m_oMapGlobalCounter[poJunctionLayer];

                        ++m_oCurCtxt.m_oMapCounter[poJunctionLayer];
                        const int nCounter =
                            m_oCurCtxt.m_oMapCounter[poJunctionLayer];

                        m_oCurCtxt.m_poGroupLayer = nullptr;
                        m_oCurCtxt.m_nGroupLayerLevel = -1;
                        m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;

                        // Install new context
                        Context oContext;
                        oContext = m_oCurCtxt;
                        oContext.m_nLevel = m_nLevel;
                        oContext.m_osCurSubXPath = m_osCurSubXPath;
                        m_osCurSubXPath = osNestedXPath;
#ifdef DEBUG_VERBOSE
                        CPLDebug("GMLAS",
                                 "Installing new m_osCurSubXPath from %s to %s",
                                 oContext.m_osCurSubXPath.c_str(),
                                 m_osCurSubXPath.c_str());
#endif
                        PushContext( oContext );
                        m_oCurCtxt.m_oMapCounter.clear();

                        // Process attributes now because we might need to
                        // fetch the child id from them
                        ProcessAttributes(attrs);
                        bHasProcessedAttributes = true;

                        CPLString osChildId(
                            m_oCurCtxt.m_poFeature->GetFieldAsString(
                                    m_oCurCtxt.m_poLayer->GetIDFieldIdx()));

                        // Create junction feature
                        OGRFeature* poJunctionFeature =
                                new OGRFeature(poJunctionLayer->GetLayerDefn());
                        poJunctionFeature->SetFID(nGlobalCounter);
                        poJunctionFeature->SetField(szOCCURRENCE, nCounter);
                        poJunctionFeature->SetField(szPARENT_PKID, osParentId);
                        poJunctionFeature->SetField(szCHILD_PKID, osChildId);
                        PushFeatureReady(poJunctionFeature, poJunctionLayer);
                    }
                    idx = IDX_COMPOUND_FOLDED;

                    break;
                }
            }

            m_nCurFieldIdx = -1;
            m_nCurGeomFieldIdx = -1;
            if( idx != IDX_COMPOUND_FOLDED && m_nLevelSilentIgnoredXPath < 0 &&

                // Detect if we are in a situation where elements like
                // <foo xsi:nil="true"/> have no corresponding OGR field
                // because of the use of remove_unused_fields=true
                !( m_oCurCtxt.m_poLayer->
                            GetFCFieldIndexFromXPath(m_osCurSubXPath) >= 0 &&
                    attrs.getLength() == 1 &&
                    m_oMapURIToPrefix[ transcode( attrs.getURI(0) ) ] == szXSI_PREFIX &&
                    transcode(attrs.getLocalName(0)) == szNIL ) )
            {
                CPLString osMatchedXPath;
                if( m_oIgnoredXPathMatcher.MatchesRefXPath(
                                        m_osCurSubXPath, osMatchedXPath) )
                {
                    if( m_oMapIgnoredXPathToWarn[osMatchedXPath] )
                    {
                        CPLError(CE_Warning, CPLE_AppDefined,
                                "Element with xpath=%s found in document but "
                                "ignored according to configuration",
                                m_osCurSubXPath.c_str());
                    }
                    else
                    {
                        CPLDebug("GMLAS",
                                "Element with xpath=%s found in document but "
                                "ignored according to configuration",
                                m_osCurSubXPath.c_str());
                    }
                    m_nLevelSilentIgnoredXPath = m_nLevel;
                }
                else
                {
                    if( m_bWarnUnexpected )
                    {
                      CPLError(CE_Warning, CPLE_AppDefined,
                         "Unexpected element with xpath=%s (subxpath=%s) found",
                          m_osCurXPath.c_str(),
                          m_osCurSubXPath.c_str());
                    }
                    else
                    {
                       CPLDebug("GMLAS",
                         "Unexpected element with xpath=%s (subxpath=%s) found",
                          m_osCurXPath.c_str(),
                          m_osCurSubXPath.c_str());
                    }
                }
            }
        }
        else
        {
            m_nCurFieldIdx = -1;
            m_nCurGeomFieldIdx = -1;
        }

        if( !bHasProcessedAttributes && m_nLevelSilentIgnoredXPath < 0 )
            ProcessAttributes(attrs);
    }
    else
    {
        m_nCurFieldIdx = -1;
        m_nCurGeomFieldIdx = -1;
    }

    m_nLevel ++;
}

/************************************************************************/
/*                          ProcessAttributes()                         */
/************************************************************************/

void GMLASReader::ProcessAttributes(const Attributes& attrs)
{
    // Browse through attributes and match them with one of our fields
    const int nWildcardAttrIdx =
        m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(m_osCurSubXPath + "/" +
                                                        szAT_ANY_ATTR);
    json_object* poWildcard = nullptr;

    for(unsigned int i=0; i < attrs.getLength(); i++)
    {
        const CPLString& osAttrNSPrefix( m_osAttrNSPrefix =
            m_oMapURIToPrefix[ transcode( attrs.getURI(i), m_osAttrNSUri ) ] );
        const CPLString& osAttrLocalname(
                        transcode(attrs.getLocalName(i), m_osAttrLocalName) );
        const CPLString& osAttrValue(
                                transcode(attrs.getValue(i), m_osAttrValue) );
        CPLString& osAttrXPath( m_osAttrXPath );
        if( !osAttrNSPrefix.empty() )
        {
            osAttrXPath.reserve( m_osCurSubXPath.size() + 2 +
                        osAttrNSPrefix.size() + 1 + osAttrLocalname.size() );
            osAttrXPath = m_osCurSubXPath;
            osAttrXPath += "/@";
            osAttrXPath += osAttrNSPrefix;
            osAttrXPath += ":";
            osAttrXPath += osAttrLocalname;
        }
        else
        {
            osAttrXPath.reserve( m_osCurSubXPath.size() + 2 +
                                 osAttrLocalname.size() );
            osAttrXPath = m_osCurSubXPath;
            osAttrXPath += "/@";
            osAttrXPath += osAttrLocalname;
        }

        //CPLDebug("GMLAS", "Attr %s=%s", osAttrXPath.c_str(), osAttrValue.c_str());

        const int nAttrIdx = m_oCurCtxt.m_poLayer->
                                    GetOGRFieldIndexFromXPath(osAttrXPath);
        int nFCIdx;
        if( nAttrIdx >= 0 )
        {
            const OGRFieldType eType(
                m_oCurCtxt.m_poFeature->GetFieldDefnRef(nAttrIdx)->GetType());
            if( osAttrValue.empty() && eType == OFTString  )
            {
                m_oCurCtxt.m_poFeature->SetField( nAttrIdx, "" );
            }
            else
            {
                SetField( m_oCurCtxt.m_poFeature,
                          m_oCurCtxt.m_poLayer,
                          nAttrIdx, osAttrValue );
            }

            if( osAttrNSPrefix == szXLINK_PREFIX &&
                osAttrLocalname == szHREF &&
                !osAttrValue.empty() )
            {
                ProcessXLinkHref( nAttrIdx, osAttrXPath, osAttrValue );
            }

            if( m_oXLinkResolver.GetConf().m_bResolveInternalXLinks &&
                m_bInitialPass )
            {
                nFCIdx = m_oCurCtxt.m_poLayer->
                        GetFCFieldIndexFromXPath(osAttrXPath);
                if( nFCIdx >= 0 &&
                    m_oCurCtxt.m_poLayer->GetFeatureClass().
                        GetFields()[nFCIdx].GetType() == GMLAS_FT_ID )
                {
                    // We don't check that there's no existing id in the map
                    // This is normally forbidden by the xs:ID rules
                    // If not respected by the document, this should not lead to
                    // crashes
                    m_oMapElementIdToLayer[ osAttrValue ] = m_oCurCtxt.m_poLayer;

                    if( m_oCurCtxt.m_poLayer->IsGeneratedIDField() )
                    {
                        CPLString osFeaturePKID(
                            m_oCurCtxt.m_poFeature->GetFieldAsString(
                                m_oCurCtxt.m_poLayer->GetIDFieldIdx() ));
                        m_oMapElementIdToPKID[ osAttrValue ] = osFeaturePKID;
                    }
                }
            }
        }

        else if( osAttrNSPrefix == szXSI_PREFIX &&
                 osAttrLocalname == szNIL )
        {
            if( osAttrValue == "true" )
            {
                const int nMainAttrIdx = m_oCurCtxt.m_poLayer->
                                        GetOGRFieldIndexFromXPath(m_osCurSubXPath);
                if( nMainAttrIdx >= 0 )
                {
                    m_oCurCtxt.m_poFeature->SetFieldNull( nMainAttrIdx );
                }
                else
                {
                    const int nHrefAttrIdx = m_oCurCtxt.m_poLayer->
                            GetOGRFieldIndexFromXPath(m_osCurSubXPath +
                                        "/@" + szXLINK_PREFIX + ":" + szHREF);
                    if( nHrefAttrIdx >= 0 )
                    {
                        m_oCurCtxt.m_poFeature->SetFieldNull( nHrefAttrIdx );
                    }
                }
            }
        }

        else if( osAttrNSPrefix != szXMLNS_PREFIX &&
                 osAttrLocalname != szXMLNS_PREFIX &&
                    !(osAttrNSPrefix == szXSI_PREFIX &&
                        osAttrLocalname == szSCHEMA_LOCATION) &&
                    !(osAttrNSPrefix == szXSI_PREFIX &&
                        osAttrLocalname == szNO_NAMESPACE_SCHEMA_LOCATION) &&
                    // Do not warn about fixed attributes on geometry properties
                    !(m_nCurGeomFieldIdx >= 0 && (
                    (osAttrNSPrefix == szXLINK_PREFIX &&
                     osAttrLocalname == szTYPE) ||
                    (osAttrNSPrefix == "" && osAttrLocalname == szOWNS))) )
        {
            CPLString osMatchedXPath;
            if( nWildcardAttrIdx >= 0 )
            {
                if( poWildcard == nullptr )
                    poWildcard = json_object_new_object();
                CPLString osKey;
                if( !osAttrNSPrefix.empty() )
                    osKey = osAttrNSPrefix + ":" + osAttrLocalname;
                else
                    osKey = osAttrLocalname;
                json_object_object_add(poWildcard,
                    osKey,
                    json_object_new_string(osAttrValue));
            }
            else if( m_bValidate &&
                     (nFCIdx = m_oCurCtxt.m_poLayer->
                        GetFCFieldIndexFromXPath(osAttrXPath)) >= 0 &&
                     !m_oCurCtxt.m_poLayer->GetFeatureClass().
                        GetFields()[nFCIdx].GetFixedValue().empty() )
            {
                // In validation mode, fixed attributes not present in the
                // document are still reported, which cause spurious warnings
            }
            else if( m_bValidate &&
                     (nFCIdx = m_oCurCtxt.m_poLayer->
                        GetFCFieldIndexFromXPath(osAttrXPath)) >= 0 &&
                     !m_oCurCtxt.m_poLayer->GetFeatureClass().
                        GetFields()[nFCIdx].GetDefaultValue().empty() &&
                     m_oCurCtxt.m_poLayer->GetFeatureClass().
                        GetFields()[nFCIdx].GetDefaultValue() == m_osAttrValue )
            {
                // In validation mode, default attributes not present in the
                // document are still reported, which cause spurious warnings
            }
            else if( m_oIgnoredXPathMatcher.MatchesRefXPath(
                                        osAttrXPath, osMatchedXPath) )
            {
                if( m_oMapIgnoredXPathToWarn[osMatchedXPath] )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Attribute with xpath=%s found in document but "
                            "ignored according to configuration",
                            osAttrXPath.c_str());
                }
                else
                {
                    CPLDebug("GMLAS",
                            "Attribute with xpath=%s found in document but "
                            "ignored according to configuration",
                            osAttrXPath.c_str());
                }
            }
            else
            {
                if( m_bWarnUnexpected )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unexpected attribute with xpath=%s found",
                             osAttrXPath.c_str());
                }
                else
                {
                    // Emit debug message if unexpected attribute
                    CPLDebug("GMLAS",
                                "Unexpected attribute with xpath=%s found",
                            osAttrXPath.c_str());
                }
            }
        }
    }

    // Store wildcard attributes
    if( poWildcard != nullptr )
    {
        SetField( m_oCurCtxt.m_poFeature,
                    m_oCurCtxt.m_poLayer,
                    nWildcardAttrIdx,
                    json_object_get_string(poWildcard) );
        json_object_put(poWildcard);
    }

    // Process fixed and default values, except when doing the initial scan
    // so as to avoid the bRemoveUnusedFields logic to be confused
    if( !m_bInitialPass )
    {
        const int nFieldCount = m_oCurCtxt.m_poFeature->GetFieldCount();
        const std::vector<GMLASField>& aoFields =
                        m_oCurCtxt.m_poLayer->GetFeatureClass().GetFields();
        for( int i=0; i < nFieldCount; i++ )
        {
            const int nFCIdx =
                    m_oCurCtxt.m_poLayer->GetFCFieldIndexFromOGRFieldIdx(i);
            if( nFCIdx >= 0 &&
                aoFields[nFCIdx].GetXPath().find('@') != std::string::npos )
            {
                // We process fixed as default. In theory, to be XSD compliant,
                // the user shouldn't have put a different value than the fixed
                // one, but just in case he did, then honour it instead of
                // overwriting it.
                CPLString osFixedDefaultValue = aoFields[nFCIdx].GetFixedValue();
                if( osFixedDefaultValue.empty() )
                    osFixedDefaultValue = aoFields[nFCIdx].GetDefaultValue();
                if( !osFixedDefaultValue.empty() &&
                    !m_oCurCtxt.m_poFeature->IsFieldSetAndNotNull(i) )
                {
                    SetField( m_oCurCtxt.m_poFeature,
                                m_oCurCtxt.m_poLayer,
                                i, osFixedDefaultValue);
                }
            }
        }
    }
}

/************************************************************************/
/*                           ProcessXLinkHref()                         */
/************************************************************************/

void GMLASReader::ProcessXLinkHref( int nAttrIdx,
                                    const CPLString& osAttrXPath,
                                    const CPLString& osAttrValue )
{
    // If we are a xlink:href attribute, and that the link value is
    // a internal link, then find if we have
    // a field that does a relation to a targetElement
    if( osAttrValue[0] == '#' )
    {
        const int nAttrIdx2 =
            m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(
                GMLASField::MakePKIDFieldXPathFromXLinkHrefXPath(
                                                    osAttrXPath));
        if( nAttrIdx2 >= 0 )
        {
            SetField( m_oCurCtxt.m_poFeature,
                        m_oCurCtxt.m_poLayer,
                        nAttrIdx2, osAttrValue.substr(1) );
        }
        else if( m_oXLinkResolver.GetConf().m_bResolveInternalXLinks )
        {
            const CPLString osReferringField(
                m_oCurCtxt.m_poLayer->GetLayerDefn()->
                    GetFieldDefn(nAttrIdx)->GetNameRef());
            const CPLString osId(osAttrValue.substr(1));
            if( m_bInitialPass )
            {
                std::pair<OGRGMLASLayer*, CPLString> oReferringPair(
                    m_oCurCtxt.m_poLayer, osReferringField);
                m_oMapFieldXPathToLinkValue[oReferringPair].push_back(osId);
            }
            else
            {
                const auto oIter = m_oMapElementIdToLayer.find(osId);
                if( oIter != m_oMapElementIdToLayer.end() )
                {
                    OGRGMLASLayer* poTargetLayer = oIter->second;
                    const CPLString osLinkFieldXPath =
                        m_oCurCtxt.m_poLayer->GetXPathOfFieldLinkForAttrToOtherLayer(
                            osReferringField,
                            poTargetLayer->GetFeatureClass().GetXPath());
                    const int nLinkFieldOGRId =
                        m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(
                            osLinkFieldXPath);
                    if( nLinkFieldOGRId >= 0 )
                    {
                        const auto oIter2 = m_oMapElementIdToPKID.find(osId);
                        if( oIter2 != m_oMapElementIdToPKID.end() )
                        {
                            m_oCurCtxt.m_poFeature->SetField(nLinkFieldOGRId,
                                                             oIter2->second);
                        }
                        else
                        {
                            m_oCurCtxt.m_poFeature->SetField(nLinkFieldOGRId,
                                                             osId);
                        }
                    }
                }
            }
        }
    }
    else
    {
        const int nRuleIdx =
                        m_oXLinkResolver.GetMatchingResolutionRule(osAttrValue);
        if( nRuleIdx >= 0 )
        {
            const GMLASXLinkResolutionConf::URLSpecificResolution& oRule(
                    m_oXLinkResolver.GetConf().m_aoURLSpecificRules[nRuleIdx] );
            if( m_bInitialPass )
            {
                m_oMapXLinkFields[m_oCurCtxt.m_poLayer][osAttrXPath].insert(
                                                            nRuleIdx );
            }
            else if( oRule.m_eResolutionMode ==
                        GMLASXLinkResolutionConf::RawContent )
            {
                const int nAttrIdx2 =
                  m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(
                    GMLASField::MakeXLinkRawContentFieldXPathFromXLinkHrefXPath(
                                                                    osAttrXPath));
                CPLAssert( nAttrIdx2 >= 0 );

                const CPLString osRawContent(
                    m_oXLinkResolver.GetRawContentForRule(osAttrValue, nRuleIdx));
                if( !osRawContent.empty() )
                {
                    SetField( m_oCurCtxt.m_poFeature,
                              m_oCurCtxt.m_poLayer,
                              nAttrIdx2,
                              osRawContent );
                }
            }
            else if( oRule.m_eResolutionMode ==
                        GMLASXLinkResolutionConf::FieldsFromXPath )
            {
                const CPLString osRawContent(
                    m_oXLinkResolver.GetRawContentForRule(osAttrValue, nRuleIdx));
                if( !osRawContent.empty() )
                {
                    CPLXMLNode* psNode = CPLParseXMLString( osRawContent );
                    if( psNode != nullptr )
                    {
                        std::vector<CPLString> aoXPaths;
                        std::map<CPLString, size_t> oMapFieldXPathToIdx;
                        for(size_t i=0; i < oRule.m_aoFields.size(); ++i )
                        {
                            const CPLString& osXPathRule(
                                            oRule.m_aoFields[i].m_osXPath);
                            aoXPaths.push_back(osXPathRule);
                            oMapFieldXPathToIdx[osXPathRule] = i;
                        }
                        GMLASXPathMatcher oMatcher;
                        oMatcher.SetRefXPaths(std::map<CPLString, CPLString>(),
                                              aoXPaths);
                        oMatcher.SetDocumentMapURIToPrefix(std::map<CPLString, CPLString>());

                        CPLXMLNode* psIter = psNode;
                        for( ; psIter != nullptr; psIter = psIter->psNext )
                        {
                            if( psIter->eType == CXT_Element &&
                                psIter->pszValue[0] != '?' )
                            {
                                ExploreXMLDoc( osAttrXPath,
                                               oRule,
                                               psIter,
                                               CPLString(),
                                               oMatcher,
                                               oMapFieldXPathToIdx );
                            }
                        }
                    }
                    CPLDestroyXMLNode(psNode);
                }
            }
        }
        else if( m_oXLinkResolver.IsRawContentResolutionEnabled() )
        {
            const int nAttrIdx2 =
              m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(
                GMLASField::MakeXLinkRawContentFieldXPathFromXLinkHrefXPath(
                                                                osAttrXPath));
            CPLAssert( nAttrIdx2 >= 0 );

            const CPLString osRawContent(
                m_oXLinkResolver.GetRawContent(osAttrValue));
            if( !osRawContent.empty() )
            {
                SetField( m_oCurCtxt.m_poFeature,
                        m_oCurCtxt.m_poLayer,
                        nAttrIdx2,
                        osRawContent );
            }
        }
    }
}

/************************************************************************/
/*                            ExploreXMLDoc()                           */
/************************************************************************/

void GMLASReader::ExploreXMLDoc( const CPLString& osAttrXPath,
                                 const GMLASXLinkResolutionConf::URLSpecificResolution& oRule,
                                 CPLXMLNode* psNode,
                                 const CPLString& osParentXPath,
                                 const GMLASXPathMatcher& oMatcher,
                                 const std::map<CPLString, size_t>&
                                                        oMapFieldXPathToIdx )
{
    CPLString osXPath;
    if( osParentXPath.empty() )
        osXPath = psNode->pszValue;
    else if( psNode->eType == CXT_Element )
        osXPath = osParentXPath + "/" + psNode->pszValue;
    else
    {
        CPLAssert( psNode->eType == CXT_Attribute );
        osXPath = osParentXPath + "/@" + psNode->pszValue;
    }

    CPLString osMatchedXPathRule;
    if( oMatcher.MatchesRefXPath(osXPath, osMatchedXPathRule) )
    {
        const auto oIter = oMapFieldXPathToIdx.find(osMatchedXPathRule);
        CPLAssert( oIter != oMapFieldXPathToIdx.end() );
        const size_t nFieldRuleIdx = oIter->second;
        const CPLString osDerivedFieldXPath(
            GMLASField::MakeXLinkDerivedFieldXPathFromXLinkHrefXPath(
                osAttrXPath, oRule.m_aoFields[nFieldRuleIdx].m_osName) );
        const int nAttrIdx =
              m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(osDerivedFieldXPath);
        CPLAssert( nAttrIdx >= 0 );
        CPLString osVal;
        if( psNode->eType == CXT_Element &&
            psNode->psChild != nullptr &&
            psNode->psChild->eType == CXT_Text &&
            psNode->psChild->psNext == nullptr )
        {
            osVal = psNode->psChild->pszValue;
        }
        else if( psNode->eType == CXT_Attribute )
        {
            osVal = psNode->psChild->pszValue;
        }
        else
        {
            char* pszContent = CPLSerializeXMLTree( psNode->psChild );
            osVal = pszContent;
            CPLFree(pszContent);
        }
        if( m_oCurCtxt.m_poFeature->IsFieldSetAndNotNull(nAttrIdx) &&
            m_oCurCtxt.m_poFeature->GetFieldDefnRef(nAttrIdx)->GetType() == OFTString )
        {
            osVal = m_oCurCtxt.m_poFeature->GetFieldAsString(nAttrIdx) +
                    CPLString(" ") + osVal;
        }
        SetField( m_oCurCtxt.m_poFeature, m_oCurCtxt.m_poLayer,
                    nAttrIdx, osVal );
    }

    CPLXMLNode* psIter = psNode->psChild;
    for( ; psIter != nullptr; psIter = psIter->psNext )
    {
        if( psIter->eType == CXT_Element || psIter->eType == CXT_Attribute )
        {
            ExploreXMLDoc( osAttrXPath, oRule, psIter, osXPath, oMatcher,
                        oMapFieldXPathToIdx );
        }
    }
}

/************************************************************************/
/*                              endElement()                            */
/************************************************************************/

void GMLASReader::endElement(
            const   XMLCh* const    uri,
            const   XMLCh* const    localname,
            const   XMLCh* const
#ifdef DEBUG_VERBOSE
                                    qname
#endif
        )
{
    m_nLevel --;

#ifdef DEBUG_VERBOSE
    CPLDebug("GMLAS", "m_nLevel = %d", m_nLevel);
#endif

#ifdef DEBUG_VERBOSE
    {
        const CPLString& osLocalname( transcode(localname, m_osLocalname) );
        const CPLString& osNSPrefix( m_osNSPrefix =
                                m_oMapURIToPrefix[ transcode(uri, m_osNSUri) ] );
        if( osNSPrefix.empty() )
            m_osXPath = osLocalname;
        else
        {
            m_osXPath.reserve( osNSPrefix.size() + 1 + osLocalname.size() );
            m_osXPath = osNSPrefix;
            m_osXPath += ":";
            m_osXPath += osLocalname;
        }
    }
    CPLDebug("GMLAS", "endElement(%s / %s)",
             transcode(qname).c_str(), m_osXPath.c_str());
#endif

    if( m_nLevelSilentIgnoredXPath == m_nLevel )
    {
        m_nLevelSilentIgnoredXPath = -1;
    }

    // Make sure to set field only if we are at the expected nesting level
    if( m_nCurFieldIdx >= 0 && m_nLevel == m_nCurFieldLevel - 1 )
    {
        const OGRFieldType eType(
            m_nCurFieldIdx >= 0 ?
                m_oCurCtxt.m_poFeature->GetFieldDefnRef(m_nCurFieldIdx)->GetType() :
            OFTString );

        // Assign XML content to field value
        if( IsArrayType(eType) )
        {
            const int nFCFieldIdx =
                m_oCurCtxt.m_poLayer->GetFCFieldIndexFromOGRFieldIdx(m_nCurFieldIdx);
            if( nFCFieldIdx >= 0 &&
                m_oCurCtxt.m_poLayer->GetFeatureClass().GetFields()[nFCFieldIdx].IsList() )
            {
                SetField( m_oCurCtxt.m_poFeature,
                          m_oCurCtxt.m_poLayer,
                          m_nCurFieldIdx, m_osTextContent );
            }
            else if( m_nTextContentListEstimatedSize > m_nMaxContentSize )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Too much repeated data in a single element");
                m_bParsingError = true;
            }
            else
            {
                // Transform boolean values to something that OGR understands
                if( eType == OFTIntegerList &&
                    m_oCurCtxt.m_poFeature->GetFieldDefnRef(m_nCurFieldIdx)->
                                                GetSubType() == OFSTBoolean )
                {
                    if( m_osTextContent == "true" )
                        m_osTextContent = "1";
                    else
                        m_osTextContent = "0";
                }

                m_osTextContentList.AddString( m_osTextContent );
                // 16 is an arbitrary number for the cost of a new entry in the
                // string list
                m_nTextContentListEstimatedSize += 16 + m_osTextContent.size();
                m_oCurCtxt.m_poFeature->SetField( m_nCurFieldIdx,
                                        m_osTextContentList.List() );
            }
        }
        else
        {
            if( m_bIsXMLBlobIncludeUpper && FillTextContent() )
            {
                const CPLString& osLocalname(
                                transcode(localname, m_osLocalname) );
                const CPLString& osNSPrefix(
                                m_oMapURIToPrefix[ transcode(uri, m_osNSUri) ] );

                m_osTextContent += "</";
                if( !osNSPrefix.empty() )
                {
                    m_osTextContent += osNSPrefix;
                    m_osTextContent += ":";
                }
                m_osTextContent += osLocalname;
                m_osTextContent += ">";
            }

            SetField( m_oCurCtxt.m_poFeature,
                        m_oCurCtxt.m_poLayer,
                        m_nCurFieldIdx, m_osTextContent );
        }
    }

    // Make sure to set field only if we are at the expected nesting level
    if( m_nCurGeomFieldIdx >= 0 && m_nLevel == m_nCurFieldLevel - 1 )
    {
        if( !m_apsXMLNodeStack.empty() )
        {
            CPLAssert( m_apsXMLNodeStack.size() == 1 );
            CPLXMLNode* psRoot = m_apsXMLNodeStack[0].psNode;
            ProcessGeometry(psRoot);
            CPLDestroyXMLNode(psRoot);
            m_apsXMLNodeStack.clear();
        }
    }

    if( (m_nCurFieldIdx >= 0 || m_nCurGeomFieldIdx >= 0) &&
        m_nLevel == m_nCurFieldLevel - 1 )
    {
        m_bIsXMLBlob = false;
        m_bIsXMLBlobIncludeUpper = false;
    }

    if( m_bIsXMLBlob )
    {
        if( m_nCurGeomFieldIdx >= 0 )
        {
            if( m_apsXMLNodeStack.size() > 1 )
            {
#ifdef DEBUG_VERBOSE
                CPLDebug("GMLAS", "m_apsXMLNodeStack.pop_back()");
#endif
                m_apsXMLNodeStack.pop_back();
            }
        }

        if( FillTextContent() )
        {
            const CPLString& osLocalname(
                            transcode(localname, m_osLocalname) );
            const CPLString& osNSPrefix(
                            m_oMapURIToPrefix[ transcode(uri, m_osNSUri) ] );

            m_osTextContent += "</";
            if( !osNSPrefix.empty() )
            {
                m_osTextContent += osNSPrefix;
                m_osTextContent += ":";
            }
            m_osTextContent += osLocalname;
            m_osTextContent += ">";

            if( m_osTextContent.size() > m_nMaxContentSize )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                        "Too much data in a single element");
                m_bParsingError = true;
            }
        }
    }
    else
    {
        m_osTextContent.clear();
    }

    if( m_nSWEDataArrayLevel >= 0)
    {
        if( m_nLevel > m_nSWEDataArrayLevel )
        {
            CPLAssert( m_apsXMLNodeStack.size() > 1 );
            m_apsXMLNodeStack.pop_back();
        }
        else
        {
            CPLAssert( m_apsXMLNodeStack.size() == 1 );
            CPLXMLNode* psRoot = m_apsXMLNodeStack[0].psNode;
            ProcessSWEDataArray(psRoot);
            m_nSWEDataArrayLevel = -1;
            CPLDestroyXMLNode(psRoot);
            m_apsXMLNodeStack.clear();
        }
    }

    // The while and not just if is needed when a group is at the end of an
    // element
    while( !m_aoStackContext.empty() &&
           m_aoStackContext.back().m_nLevel >= m_nLevel )
    {
        std::map<OGRLayer*, int> oMapCounter = m_aoStackContext.back().m_oMapCounter;
        if( !m_aoStackContext.back().m_osCurSubXPath.empty() )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("GMLAS", "Restoring m_osCurSubXPath from %s to %s",
                     m_osCurSubXPath.c_str(),
                     m_aoStackContext.back().m_osCurSubXPath.c_str());
#endif
            m_osCurSubXPath = m_aoStackContext.back().m_osCurSubXPath;
        }

        if( m_oCurCtxt.m_poGroupLayer == m_oCurCtxt.m_poLayer )
        {
            PopContext();
            CPLAssert( !m_aoStackContext.empty() );
            m_oCurCtxt.m_poLayer = m_aoStackContext.back().m_poLayer;
        }
        else
        {
            if( m_oCurCtxt.m_poGroupLayer )
            {
                /* Case like
                        <first_elt_of_group>...</first_elt_of_group>
                    </end_of_enclosing_element>   <!-- we are here at endElement() -->
                */

                //CPLDebug("GMLAS", "Feature ready");
                PushFeatureReady(m_oCurCtxt.m_poFeature,
                                 m_oCurCtxt.m_poGroupLayer);
                //CPLDebug("GMLAS", "Feature ready");
                PushFeatureReady(m_aoStackContext.back().m_poFeature,
                                 m_aoStackContext.back().m_poLayer);
            }
            else
            {
                //CPLDebug("GMLAS", "Feature ready");
                PushFeatureReady(m_oCurCtxt.m_poFeature,
                                 m_oCurCtxt.m_poLayer);
            }
            PopContext();
            if( !m_aoStackContext.empty() )
            {
                m_oCurCtxt = m_aoStackContext.back();
                m_oCurCtxt.m_osCurSubXPath.clear();
                if( m_oCurCtxt.m_nLevel < 0 )
                {
                    PopContext();
                    CPLAssert( !m_aoStackContext.empty() );
                    m_oCurCtxt.m_poLayer = m_aoStackContext.back().m_poLayer;
                }
            }
            else
            {
                m_oCurCtxt.m_poFeature = nullptr;
                m_oCurCtxt.m_poLayer = nullptr;
                m_oCurCtxt.m_poGroupLayer = nullptr;
                m_oCurCtxt.m_nGroupLayerLevel = -1;
                m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;
            }
            m_nCurFieldIdx = -1;
        }
        m_oCurCtxt.m_oMapCounter = oMapCounter;

#ifdef DEBUG_VERBOSE
        CPLDebug("GMLAS", "m_oCurCtxt = ");
        m_oCurCtxt.Dump();
#endif
    }

    size_t nLastXPathLength = m_anStackXPathLength.back();
    m_anStackXPathLength.pop_back();
    if( m_anStackXPathLength.empty())
        m_osCurXPath.clear();
    else
        m_osCurXPath.resize( m_osCurXPath.size() - 1 - nLastXPathLength);

    if( m_osCurSubXPath.size() >= 1 + nLastXPathLength )
        m_osCurSubXPath.resize( m_osCurSubXPath.size() - 1 - nLastXPathLength);
    else if( m_osCurSubXPath.size() == nLastXPathLength )
         m_osCurSubXPath.clear();

    if( m_nSWEDataRecordLevel >= 0)
    {
        if( m_nLevel > m_nSWEDataRecordLevel )
        {
            CPLAssert( m_apsXMLNodeStack.size() > 1 );
            m_apsXMLNodeStack.pop_back();
        }
        else
        {
            CPLAssert( m_apsXMLNodeStack.size() == 1 );
            CPLXMLNode* psRoot = m_apsXMLNodeStack[0].psNode;
            ProcessSWEDataRecord(psRoot);
            m_nSWEDataRecordLevel = -1;
            CPLDestroyXMLNode(psRoot);
            m_apsXMLNodeStack.clear();
        }
    }
}

/************************************************************************/
/*                             SetSWEValue()                            */
/************************************************************************/

static void SetSWEValue(OGRFeature* poFeature, int iField, CPLString& osValue)
{
    if( !osValue.empty() )
    {
        OGRFieldDefn* poFieldDefn = poFeature->GetFieldDefnRef(iField);
        OGRFieldType eType(poFieldDefn->GetType());
        OGRFieldSubType eSubType(poFieldDefn->GetSubType());
        if( eType == OFTReal || eType == OFTInteger)
        {
            osValue.Trim();
            if( eSubType == OFSTBoolean )
            {
                osValue = EQUAL(osValue, "1") ||
                          EQUAL(osValue, "True") ? "1" : "0";
            }
        }
        poFeature->SetField(iField, osValue.c_str());
    }
}

/************************************************************************/
/*                              SkipSpace()                             */
/************************************************************************/

static size_t SkipSpace( const char* pszValues, size_t i )
{
    while( isspace( static_cast<int>(pszValues[i]) ) )
        i ++;
    return i;
}

/************************************************************************/
/*                         ProcessSWEDataArray()                        */
/************************************************************************/

void GMLASReader::ProcessSWEDataArray(CPLXMLNode* psRoot)
{
    if( m_oCurCtxt.m_poLayer == nullptr )
        return;

    CPLStripXMLNamespace( psRoot, "swe", true );
    CPLXMLNode* psElementType = CPLGetXMLNode(psRoot, "elementType");
    if( psElementType == nullptr )
        return;
    CPLXMLNode* psDataRecord = CPLGetXMLNode(psElementType, "DataRecord");
    if( psDataRecord == nullptr )
        return;
    const char* pszValues = CPLGetXMLValue(psRoot, "values", nullptr);
    if( pszValues == nullptr )
        return;
    CPLXMLNode* psTextEncoding = CPLGetXMLNode(psRoot,
                                               "encoding.TextEncoding");
    if( psTextEncoding == nullptr )
        return;
    //CPLString osDecimalSeparator =
    //    CPLGetXMLValue(psTextEncoding, "decimalSeparator", ".");
    CPLString osBlockSeparator =
        CPLGetXMLValue(psTextEncoding, "blockSeparator", "");
    CPLString osTokenSeparator =
        CPLGetXMLValue(psTextEncoding, "tokenSeparator", "");
    if( osBlockSeparator.empty() || osTokenSeparator.empty() )
        return;

    if( m_bInitialPass )
    {
        CPLString osLayerName;
        osLayerName.Printf("DataArray_%d", m_nSWEDataArrayLayerIdx+1);
        const char* pszElementTypeName =
                                CPLGetXMLValue(psElementType, "name", nullptr);
        if( pszElementTypeName != nullptr )
        {
            osLayerName += "_";
            osLayerName += pszElementTypeName;
        }
        osLayerName = osLayerName.tolower();
        OGRGMLASLayer* poLayer = new OGRGMLASLayer(osLayerName);

        // Register layer in _ogr_layers_metadata
        {
            OGRFeature* poLayerDescFeature =
                        new OGRFeature(m_poLayersMetadataLayer->GetLayerDefn());
            poLayerDescFeature->SetField( szLAYER_NAME, osLayerName );
            poLayerDescFeature->SetField( szLAYER_CATEGORY, szSWE_DATA_ARRAY );

            CPLString osFieldName(szPARENT_PREFIX);
            osFieldName += m_oCurCtxt.m_poLayer->GetLayerDefn()->GetFieldDefn(
                        m_oCurCtxt.m_poLayer->GetIDFieldIdx())->GetNameRef();
            poLayerDescFeature->SetField( szLAYER_PARENT_PKID_NAME,
                                          osFieldName.c_str() );
            CPL_IGNORE_RET_VAL(
                m_poLayersMetadataLayer->CreateFeature(poLayerDescFeature));
            delete poLayerDescFeature;
        }

        // Register layer relationship in _ogr_layer_relationships
        {
            OGRFeature* poRelationshipsFeature =
                new OGRFeature(m_poRelationshipsLayer->GetLayerDefn());
            poRelationshipsFeature->SetField( szPARENT_LAYER,
                                              m_oCurCtxt.m_poLayer->GetName() );
            poRelationshipsFeature->SetField( szPARENT_PKID,
                    m_oCurCtxt.m_poLayer->GetLayerDefn()->GetFieldDefn(
                        m_oCurCtxt.m_poLayer->GetIDFieldIdx())->GetNameRef() );
            if( !m_osSWEDataArrayParentField.empty() )
            {
                poRelationshipsFeature->SetField( szPARENT_ELEMENT_NAME,
                                                  m_osSWEDataArrayParentField );
            }
            poRelationshipsFeature->SetField(szCHILD_LAYER,
                                             osLayerName);
            CPL_IGNORE_RET_VAL(m_poRelationshipsLayer->CreateFeature(
                                            poRelationshipsFeature));
            delete poRelationshipsFeature;
        }

        m_apoSWEDataArrayLayers.push_back(poLayer);
        poLayer->ProcessDataRecordOfDataArrayCreateFields(m_oCurCtxt.m_poLayer,
                                                          psDataRecord,
                                                          m_poFieldsMetadataLayer);
    }
    else
    {
        CPLAssert( m_nSWEDataArrayLayerIdx <
                    static_cast<int>(m_apoSWEDataArrayLayers.size()) );
        OGRGMLASLayer* poLayer =
                        m_apoSWEDataArrayLayers[m_nSWEDataArrayLayerIdx];
        // -1 because first field is parent id
        const int nFieldCount = poLayer->GetLayerDefn()->GetFieldCount() - 1;
        int nFID = 1;
        int iField = 0;
        const size_t nLen = strlen(pszValues);
        OGRFeature* poFeature = nullptr;
        const bool bSameSep = (osTokenSeparator == osBlockSeparator);
        size_t nLastValid = SkipSpace(pszValues, 0);
        size_t i = nLastValid;
        while( i < nLen )
        {
            if( poFeature == nullptr )
            {
                poFeature = new OGRFeature( poLayer->GetLayerDefn() );
                poFeature->SetFID( nFID );
                poFeature->SetField( 0,
                    m_oCurCtxt.m_poFeature->GetFieldAsString(
                        m_oCurCtxt.m_poLayer->GetIDFieldIdx()));
                nFID ++;
                iField = 0;
            }
            if( strncmp( pszValues + i, osTokenSeparator,
                         osTokenSeparator.size() ) == 0 )
            {
                if( bSameSep && iField == nFieldCount )
                {
                    PushFeatureReady( poFeature, poLayer );
                    poFeature = new OGRFeature( poLayer->GetLayerDefn() );
                    poFeature->SetFID( nFID );
                    poFeature->SetField( 0,
                        m_oCurCtxt.m_poFeature->GetFieldAsString(
                            m_oCurCtxt.m_poLayer->GetIDFieldIdx()));
                    nFID ++;
                    iField = 0;
                }

                if( iField < nFieldCount )
                {
                    CPLString osValue( pszValues + nLastValid,
                                       i - nLastValid );
                    // +1 because first field is parent id
                    SetSWEValue(poFeature, iField+1, osValue);
                    iField ++;
                }
                nLastValid = i + osTokenSeparator.size();
                nLastValid = SkipSpace(pszValues, nLastValid);
                i = nLastValid;
            }
            else if( strncmp( pszValues + i, osBlockSeparator,
                              osBlockSeparator.size() ) == 0 )
            {
                if( iField < nFieldCount )
                {
                    CPLString osValue( pszValues + nLastValid,
                                       i - nLastValid );
                    // +1 because first field is parent id
                    SetSWEValue(poFeature, iField+1, osValue);
                    iField ++;
                }
                PushFeatureReady( poFeature, poLayer );
                poFeature = nullptr;
                nLastValid = i + osBlockSeparator.size();
                nLastValid = SkipSpace(pszValues, nLastValid);
                i = nLastValid;
            }
            else
            {
                i++;
            }
        }
        if( poFeature != nullptr )
        {
            if( iField < nFieldCount )
            {
                CPLString osValue( pszValues + nLastValid,
                                    nLen - nLastValid );
                // +1 because first field is parent id
                SetSWEValue(poFeature, iField+1, osValue);
                //iField ++;
            }
            PushFeatureReady( poFeature, poLayer );
        }
    }
    m_nSWEDataArrayLayerIdx ++;
}


/************************************************************************/
/*                        ProcessSWEDataRecord()                        */
/************************************************************************/

void GMLASReader::ProcessSWEDataRecord(CPLXMLNode* psRoot)
{
    CPLStripXMLNamespace( psRoot, "swe", true );
    if( m_bInitialPass )
    {
        // Collect existing live features of this layer, so that we can
        // patch them
        std::vector<OGRFeature*> apoFeatures;
        apoFeatures.push_back(m_oCurCtxt.m_poFeature);
        for(auto& feature: m_aoFeaturesReady )
        {
            if( feature.second == m_oCurCtxt.m_poLayer )
                apoFeatures.push_back(feature.first);
        }
        m_oCurCtxt.m_poLayer->ProcessDataRecordCreateFields(
            psRoot, apoFeatures, m_poFieldsMetadataLayer);
    }
    else
    {
        m_oCurCtxt.m_poLayer->ProcessDataRecordFillFeature(
            psRoot, m_oCurCtxt.m_poFeature);
    }
}

/************************************************************************/
/*                            GMLASGetSRSName()                         */
/************************************************************************/

static const char* GMLASGetSRSName(CPLXMLNode* psNode)
{
    const char* pszSRSName = CPLGetXMLValue(psNode, szSRS_NAME, nullptr);
    if( pszSRSName == nullptr )
    {
        // Case of a gml:Point where the srsName is on the gml:pos
        pszSRSName = CPLGetXMLValue(psNode, "gml:pos.srsName", nullptr);
    }
    return pszSRSName;
}

/************************************************************************/
/*                            ProcessGeometry()                         */
/************************************************************************/

void GMLASReader::ProcessGeometry(CPLXMLNode* psRoot)
{
    OGRGeomFieldDefn* poGeomFieldDefn =
        m_oCurCtxt.m_poFeature->GetGeomFieldDefnRef(
                                    m_nCurGeomFieldIdx);

    if( m_bInitialPass )
    {
        const char* pszSRSName = GMLASGetSRSName(psRoot);
        if( pszSRSName != nullptr )
        {
            // If we are doing a first pass, store the SRS of the geometry
            // column
            if( !m_oSetGeomFieldsWithUnknownSRS.empty() &&
                 m_oSetGeomFieldsWithUnknownSRS.find(poGeomFieldDefn) !=
                        m_oSetGeomFieldsWithUnknownSRS.end() )
            {
                OGRSpatialReference* poSRS =
                                new OGRSpatialReference();
                poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

                if( poSRS->SetFromUserInput( pszSRSName, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS ) == OGRERR_NONE )
                {
                    m_oMapGeomFieldDefnToSRSName[poGeomFieldDefn] = pszSRSName;
                    poGeomFieldDefn->SetSpatialRef(poSRS);
                }
                poSRS->Release();
                m_oSetGeomFieldsWithUnknownSRS.erase(poGeomFieldDefn);
            }
        }
        return;
    }

#ifdef DEBUG_VERBOSE
    {
        char* pszXML = CPLSerializeXMLTree(psRoot);
        CPLDebug("GML", "geometry = %s", pszXML);
        CPLFree(pszXML);
    }
#endif

    OGRGeometry* poGeom = reinterpret_cast<OGRGeometry*>
                    (OGR_G_CreateFromGMLTree( psRoot ));
    if( poGeom != nullptr )
    {
        const char* pszSRSName = GMLASGetSRSName(psRoot);

        bool bSwapXY = false;
        if( pszSRSName != nullptr )
        {
            // Check if the srsName indicates unusual axis order,
            // and if so swap x and y coordinates.
            const auto oIter = m_oMapSRSNameToInvertedAxis.find(pszSRSName);
            if( oIter == m_oMapSRSNameToInvertedAxis.end() )
            {
                OGRSpatialReference oSRS;
                oSRS.SetFromUserInput( pszSRSName, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS );
                bSwapXY = !STARTS_WITH_CI(pszSRSName, "EPSG:") &&
                    (CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong()) ||
                     CPL_TO_BOOL(oSRS.EPSGTreatsAsNorthingEasting()));
                m_oMapSRSNameToInvertedAxis[ pszSRSName ] = bSwapXY;
            }
            else
            {
                bSwapXY = oIter->second;
            }
        }
        if( (bSwapXY && m_eSwapCoordinates == GMLAS_SWAP_AUTO) ||
            m_eSwapCoordinates == GMLAS_SWAP_YES )
        {
            poGeom->swapXY();
        }

        // Do we need to do reprojection ?
        if( pszSRSName != nullptr && poGeomFieldDefn->GetSpatialRef() != nullptr &&
            m_oMapGeomFieldDefnToSRSName[poGeomFieldDefn] != pszSRSName )
        {
            bool bReprojectionOK = false;
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput( pszSRSName, OGRSpatialReference::SET_FROM_USER_INPUT_LIMITATIONS ) == OGRERR_NONE )
            {
                OGRCoordinateTransformation* poCT =
                    OGRCreateCoordinateTransformation( &oSRS,
                                            poGeomFieldDefn->GetSpatialRef() );
                if( poCT != nullptr )
                {
                    bReprojectionOK = (poGeom->transform( poCT ) == OGRERR_NONE);
                    delete poCT;
                }
            }
            if( !bReprojectionOK )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Reprojection from %s to %s failed",
                         pszSRSName,
                         m_oMapGeomFieldDefnToSRSName[poGeomFieldDefn].c_str());
                delete poGeom;
                poGeom = nullptr;
            }
#ifdef DEBUG_VERBOSE
            else
            {
                CPLDebug("GMLAS", "Reprojected geometry from %s to %s",
                         pszSRSName,
                         m_oMapGeomFieldDefnToSRSName[poGeomFieldDefn].c_str());
            }
#endif
        }

        if( poGeom != nullptr )
        {
            // Deal with possibly repeated geometries by building
            // a geometry collection. We could also create a
            // nested table, but that would probably be less
            // convenient to use.
            OGRGeometry* poPrevGeom = m_oCurCtxt.m_poFeature->
                                    StealGeometry(m_nCurGeomFieldIdx);
            if( poPrevGeom != nullptr )
            {
                if( poPrevGeom->getGeometryType() ==
                                        wkbGeometryCollection )
                {
                    poPrevGeom->toGeometryCollection()->
                                        addGeometryDirectly(poGeom);
                    poGeom = poPrevGeom;
                }
                else
                {
                    OGRGeometryCollection* poGC =
                                    new OGRGeometryCollection();
                    poGC->addGeometryDirectly(poPrevGeom);
                    poGC->addGeometryDirectly(poGeom);
                    poGeom = poGC;
                }
            }
            poGeom->assignSpatialReference(
                                poGeomFieldDefn->GetSpatialRef());
            m_oCurCtxt.m_poFeature->SetGeomFieldDirectly(
                m_nCurGeomFieldIdx, poGeom );
        }
    }
    else
    {
        char* pszXML = CPLSerializeXMLTree(psRoot);
        CPLDebug("GMLAS", "Non-recognized geometry: %s",
                    pszXML);
        CPLFree(pszXML);
    }
}

/************************************************************************/
/*                              characters()                            */
/************************************************************************/

void GMLASReader::characters( const XMLCh *const chars,
                              const XMLSize_t length )
{
    bool bTextMemberUpdated = false;
    if( ((m_bIsXMLBlob && m_nCurGeomFieldIdx >= 0 && !m_bInitialPass) ||
        m_nSWEDataArrayLevel >= 0 || m_nSWEDataRecordLevel >= 0) &&
        // Check the stack is not empty in case of space chars before the
        // starting node
        !m_apsXMLNodeStack.empty() )
    {
        bTextMemberUpdated = true;
        const CPLString& osText( transcode(chars, m_osText,
                                           static_cast<int>(length) ) );

        // Merge content in current text node if it exists
        NodeLastChild& sNodeLastChild = m_apsXMLNodeStack.back();
        if( sNodeLastChild.psLastChild != nullptr &&
            sNodeLastChild.psLastChild->eType == CXT_Text )
        {
            CPLXMLNode* psNode = sNodeLastChild.psLastChild;
            const size_t nOldLength = strlen(psNode->pszValue);
            char* pszNewValue = reinterpret_cast<char*>(VSIRealloc(
                    psNode->pszValue, nOldLength + osText.size() + 1));
            if( pszNewValue )
            {
                psNode->pszValue = pszNewValue;
                memcpy( pszNewValue + nOldLength, osText.c_str(),
                        osText.size() + 1);
            }
            else
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
                m_bParsingError = true;
            }
        }
        // Otherwise create a new text node
        else
        {
            CPLXMLNode* psNode = reinterpret_cast<CPLXMLNode*>
                                        ( CPLMalloc(sizeof(CPLXMLNode)) );
            psNode->eType = CXT_Text;
            psNode->pszValue = reinterpret_cast<char*>
                                        ( CPLMalloc( osText.size() + 1 ) );
            memcpy(psNode->pszValue, osText.c_str(), osText.size() + 1);
            psNode->psNext = nullptr;
            psNode->psChild = nullptr;
            AttachAsLastChild( psNode );
        }
    }

    if( !FillTextContent() )
    {
        m_osTextContent = "1"; // dummy
        return;
    }

    if( m_bIsXMLBlob )
    {
        if( m_nCurFieldIdx >= 0 )
        {
            const CPLString& osText( bTextMemberUpdated ? m_osText:
                    transcode(chars, m_osText, static_cast<int>(length) ) );

            char* pszEscaped = CPLEscapeString( osText.c_str(),
                                            static_cast<int>(osText.size()),
                                            CPLES_XML );
            try
            {
                m_osTextContent += pszEscaped;
            }
            catch( const std::bad_alloc& )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
                m_bParsingError = true;
            }
            CPLFree(pszEscaped);
        }
    }
    // Make sure to set content only if we are at the expected nesting level
    else if( m_nLevel == m_nCurFieldLevel )
    {
        const CPLString& osText(
                        transcode(chars, m_osText, static_cast<int>(length) ) );
        try
        {
            m_osTextContent += osText;
        }
        catch( const std::bad_alloc& )
        {
            CPLError(CE_Failure, CPLE_OutOfMemory, "Out of memory");
            m_bParsingError = true;
        }
    }

    if( m_osTextContent.size() > m_nMaxContentSize )
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
                 "Too much data in a single element");
        m_bParsingError = true;
    }
}

/************************************************************************/
/*                            GetNextFeature()                          */
/************************************************************************/

OGRFeature* GMLASReader::GetNextFeature( OGRGMLASLayer** ppoBelongingLayer,
                                         GDALProgressFunc pfnProgress,
                                         void* pProgressData )
{
    while( !m_aoFeaturesReady.empty() )
    {
        OGRFeature* m_poFeatureReady = m_aoFeaturesReady.front().first;
        OGRGMLASLayer* m_poFeatureReadyLayer = m_aoFeaturesReady.front().second;
        m_aoFeaturesReady.erase( m_aoFeaturesReady.begin() );

        if( m_poLayerOfInterest == nullptr ||
            m_poLayerOfInterest == m_poFeatureReadyLayer )
        {
            if( ppoBelongingLayer )
                *ppoBelongingLayer = m_poFeatureReadyLayer;
            return m_poFeatureReady;
        }
        delete m_poFeatureReady;
    }

    if( m_bEOF )
        return nullptr;

    try
    {
        if( m_bFirstIteration )
        {
            m_bFirstIteration = false;
            if( !m_poSAXReader->parseFirst( *m_GMLInputSource, m_oToFill ) )
            {
                m_bParsingError = true;
                m_bEOF = true;
                return nullptr;
            }
        }

        vsi_l_offset nLastOffset = VSIFTellL(m_fp);
        while( m_poSAXReader->parseNext( m_oToFill ) )
        {
            if( pfnProgress && VSIFTellL(m_fp) - nLastOffset > 100 * 1024 )
            {
                nLastOffset = VSIFTellL(m_fp);
                double dfPct = -1;
                if( m_nFileSize )
                    dfPct = 1.0 * nLastOffset / m_nFileSize;
                if( !pfnProgress( dfPct, "", pProgressData ) )
                {
                    m_bInterrupted = true;
                    break;
                }
            }
            if( m_bParsingError )
                break;

            while( !m_aoFeaturesReady.empty() )
            {
                OGRFeature* m_poFeatureReady = m_aoFeaturesReady.front().first;
                OGRGMLASLayer* m_poFeatureReadyLayer =
                                               m_aoFeaturesReady.front().second;
                m_aoFeaturesReady.erase( m_aoFeaturesReady.begin() );

                if( m_poLayerOfInterest == nullptr ||
                    m_poLayerOfInterest == m_poFeatureReadyLayer )
                {
                    if( ppoBelongingLayer )
                        *ppoBelongingLayer = m_poFeatureReadyLayer;

                    if( pfnProgress )
                    {
                        nLastOffset = VSIFTellL(m_fp);
                        double dfPct = -1;
                        if( m_nFileSize )
                            dfPct = 1.0 * nLastOffset / m_nFileSize;
                        if( !pfnProgress( dfPct, "", pProgressData ) )
                        {
                            delete m_poFeatureReady;
                            m_bInterrupted = true;
                            m_bEOF = true;
                            return nullptr;
                        }
                    }

                    return m_poFeatureReady;
                }
                delete m_poFeatureReady;
            }
        }

        m_bEOF = true;
    }
    catch (const XMLException& toCatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 transcode( toCatch.getMessage() ).c_str() );
        m_bParsingError = true;
        m_bEOF = true;
    }
    catch (const SAXException& toCatch)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s",
                 transcode( toCatch.getMessage() ).c_str() );
        m_bParsingError = true;
        m_bEOF = true;
    }

    return nullptr;
}

/************************************************************************/
/*                              RunFirstPass()                          */
/************************************************************************/

bool GMLASReader::RunFirstPass(GDALProgressFunc pfnProgress,
                               void* pProgressData,
                               bool bRemoveUnusedLayers,
                               bool bRemoveUnusedFields,
                               bool bProcessSWEDataArray,
                               OGRLayer* poFieldsMetadataLayer,
                               OGRLayer* poLayersMetadataLayer,
                               OGRLayer* poRelationshipsLayer,
                               std::set<CPLString>& aoSetRemovedLayerNames)
{
    m_bInitialPass = true;
    m_bProcessSWEDataArray = bProcessSWEDataArray;
    m_poFieldsMetadataLayer = poFieldsMetadataLayer;
    m_poLayersMetadataLayer = poLayersMetadataLayer;
    m_poRelationshipsLayer = poRelationshipsLayer;

    // Store in m_oSetGeomFieldsWithUnknownSRS the geometry fields
    std::set<OGRGMLASLayer*> oSetUnreferencedLayers;
    std::map<OGRGMLASLayer*, std::set<CPLString> > oMapUnusedFields;
    for(size_t i=0; i < m_papoLayers->size(); i++ )
    {
        OGRGMLASLayer* poLayer = (*m_papoLayers)[i];
        OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
        oSetUnreferencedLayers.insert( poLayer );
        for(int j=0; j< poFDefn->GetGeomFieldCount(); j++ )
        {
            m_oSetGeomFieldsWithUnknownSRS.insert(
                                            poFDefn->GetGeomFieldDefn(j));
        }
        for(int j=0; j< poFDefn->GetFieldCount(); j++ )
        {
            oMapUnusedFields[poLayer].insert(
                poFDefn->GetFieldDefn(j)->GetNameRef());
        }
    }

    CPLDebug("GMLAS", "Start of first pass");

    // Do we need to do a full scan of the file ?
    const bool bHasURLSpecificRules =
                !m_oXLinkResolver.GetConf().m_aoURLSpecificRules.empty();
    const bool bDoFullPass =
        (m_bValidate ||
         bRemoveUnusedLayers ||
         bRemoveUnusedFields ||
         bHasURLSpecificRules ||
         bProcessSWEDataArray ||
         m_oXLinkResolver.GetConf().m_bResolveInternalXLinks);

    // Loop on features until we have determined the SRS of all geometry
    // columns, or potentially on the whole file for the above reasons
    OGRGMLASLayer* poLayer;
    OGRFeature* poFeature;
    while( (bDoFullPass || !m_oSetGeomFieldsWithUnknownSRS.empty() ) &&
           (poFeature = GetNextFeature(&poLayer, pfnProgress, pProgressData)) != nullptr )
    {
        if( bRemoveUnusedLayers )
            oSetUnreferencedLayers.erase( poLayer );
        if( bRemoveUnusedFields )
        {
            std::set<CPLString>& oSetUnusedFields = oMapUnusedFields[poLayer];
            OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
            int nFieldCount = poFDefn->GetFieldCount();
            for(int j=0; j< nFieldCount; j++ )
            {
                if( poFeature->IsFieldSetAndNotNull(j) )
                    oSetUnusedFields.erase(poFDefn->GetFieldDefn(j)->GetNameRef());
            }
        }
        delete poFeature;
    }

    CPLDebug("GMLAS", "End of first pass");

    ProcessInternalXLinkFirstPass(bRemoveUnusedFields, oMapUnusedFields);

    if( bRemoveUnusedLayers )
    {
        std::vector<OGRGMLASLayer*> apoNewLayers;
        for(size_t i=0; i < m_papoLayers->size(); i++ )
        {
            poLayer = (*m_papoLayers)[i];
            if( oSetUnreferencedLayers.find( poLayer ) ==
                    oSetUnreferencedLayers.end() )
            {
                apoNewLayers.push_back( poLayer );
            }
            else
            {
                aoSetRemovedLayerNames.insert( poLayer->GetName() );
                delete poLayer;
            }
        }
        *m_papoLayers = apoNewLayers;
    }
    if( bRemoveUnusedFields )
    {
        for(size_t i=0; i < m_papoLayers->size(); i++ )
        {
            poLayer = (*m_papoLayers)[i];
            for( const auto& oIter: oMapUnusedFields[poLayer] )
            {
                poLayer->RemoveField(
                    poLayer->GetLayerDefn()->GetFieldIndex(oIter) );
            }

            // We need to run this again since we may have delete the
            // element that holds attributes, like in
            // <foo xsi:nil="true" nilReason="unknown"/> where foo will be
            // eliminated, but foo_nilReason kept.
            poLayer->CreateCompoundFoldedMappings();
        }
    }

    // Add fields coming from matching URL specific rules
    if( bHasURLSpecificRules )
    {
        CreateFieldsForURLSpecificRules();
    }

    // Clear the set even if we didn't manage to determine all the SRS
    m_oSetGeomFieldsWithUnknownSRS.clear();

    return !m_bInterrupted;
}

/************************************************************************/
/*                    ProcessInternalXLinkFirstPass()                   */
/************************************************************************/

void GMLASReader::ProcessInternalXLinkFirstPass(
    bool bRemoveUnusedFields,
    std::map<OGRGMLASLayer*, std::set<CPLString> >&oMapUnusedFields)
{
    for( const auto& oIter: m_oMapFieldXPathToLinkValue )
    {
        OGRGMLASLayer* poReferringLayer = oIter.first.first;
        const CPLString& osReferringField = oIter.first.second;
        const std::vector<CPLString>& aosLinks = oIter.second;
        std::set<OGRGMLASLayer*> oSetTargetLayers;
        for( size_t i = 0; i < aosLinks.size(); i++ )
        {
            const auto oIter2 = m_oMapElementIdToLayer.find(aosLinks[i]);
            if( oIter2 == m_oMapElementIdToLayer.end() )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "%s:%s = '#%s' has no corresponding target "
                         "element in this document",
                         poReferringLayer->GetName(),
                         osReferringField.c_str(),
                         aosLinks[i].c_str());
            }
            else if( oSetTargetLayers.find(oIter2->second) ==
                                                    oSetTargetLayers.end() )
            {
                OGRGMLASLayer* poTargetLayer = oIter2->second;
                oSetTargetLayers.insert(poTargetLayer);
                CPLString osLinkFieldName =
                    poReferringLayer->CreateLinkForAttrToOtherLayer(
                        osReferringField,
                        poTargetLayer->GetFeatureClass().GetXPath());
                if( bRemoveUnusedFields )
                {
                    oMapUnusedFields[poReferringLayer].erase(osLinkFieldName);
                }
            }
        }
    }
}

/************************************************************************/
/*                    CreateFieldsForURLSpecificRules()                 */
/************************************************************************/

void GMLASReader::CreateFieldsForURLSpecificRules()
{
    for( const auto& oIter: m_oMapXLinkFields )
    {
        OGRGMLASLayer* poLayer = oIter.first;
        const auto& oMap2 = oIter.second;
        for( const auto& oIter2: oMap2 )
        {
            const CPLString& osFieldXPath(oIter2.first);
            // Note that CreateFieldsForURLSpecificRule() running on a previous
            // iteration will have inserted new OGR fields, so we really need
            // to compute that index now.
            const int nFieldIdx = poLayer->GetOGRFieldIndexFromXPath(osFieldXPath);
            CPLAssert(nFieldIdx >= 0);
            int nInsertFieldIdx = nFieldIdx + 1;
            const auto& oSetRuleIndex = oIter2.second;
            for( const auto& nRuleIdx: oSetRuleIndex )
            {
                const GMLASXLinkResolutionConf::URLSpecificResolution& oRule =
                    m_oXLinkResolver.GetConf().m_aoURLSpecificRules[nRuleIdx];
                CreateFieldsForURLSpecificRule( poLayer, nFieldIdx,
                                                osFieldXPath,
                                                nInsertFieldIdx,
                                                oRule );
            }
        }
    }
}

/************************************************************************/
/*                    CreateFieldsForURLSpecificRule()                  */
/************************************************************************/

void GMLASReader::CreateFieldsForURLSpecificRule(
                OGRGMLASLayer* poLayer,
                int nFieldIdx,
                const CPLString& osFieldXPath,
                int& nInsertFieldIdx,
                const GMLASXLinkResolutionConf::URLSpecificResolution& oRule )
{
    if( oRule.m_eResolutionMode ==
                        GMLASXLinkResolutionConf::RawContent )
    {
        const CPLString osRawContentXPath(
            GMLASField::MakeXLinkRawContentFieldXPathFromXLinkHrefXPath(
                                                                osFieldXPath) );
        if( poLayer->GetOGRFieldIndexFromXPath( osRawContentXPath ) < 0 )
        {
            const CPLString osOGRFieldName(
                poLayer->GetLayerDefn()->
                        GetFieldDefn(nFieldIdx)->GetNameRef() );
            CPLString osRawContentFieldname(osOGRFieldName);
            size_t nPos = osRawContentFieldname.find("_href");
            if( nPos != std::string::npos )
                osRawContentFieldname.resize(nPos);
            osRawContentFieldname += "_rawcontent";
            OGRFieldDefn oFieldDefnRaw( osRawContentFieldname,
                                        OFTString );
            poLayer->InsertNewField( nInsertFieldIdx,
                                        oFieldDefnRaw,
                                        osRawContentXPath );
            nInsertFieldIdx ++;
        }
    }
    else if ( oRule.m_eResolutionMode ==
                    GMLASXLinkResolutionConf::FieldsFromXPath )
    {
        for( size_t i=0; i < oRule.m_aoFields.size(); ++i )
        {
            const CPLString osDerivedFieldXPath(
                GMLASField::MakeXLinkDerivedFieldXPathFromXLinkHrefXPath(
                    osFieldXPath, oRule.m_aoFields[i].m_osName) );
            if( poLayer->GetOGRFieldIndexFromXPath( osDerivedFieldXPath ) < 0 )
            {
                const CPLString osOGRFieldName(
                    poLayer->GetLayerDefn()->
                            GetFieldDefn(nFieldIdx)->GetNameRef() );
                CPLString osNewFieldname(osOGRFieldName);
                size_t nPos = osNewFieldname.find("_href");
                if( nPos != std::string::npos )
                    osNewFieldname.resize(nPos);
                osNewFieldname += "_" + oRule.m_aoFields[i].m_osName;

                OGRFieldType eType = OFTString;
                const CPLString& osType(oRule.m_aoFields[i].m_osType);
                if( osType == "integer" )
                    eType = OFTInteger;
                else if( osType == "long" )
                    eType = OFTInteger64;
                else if( osType == "double" )
                    eType = OFTReal;
                else if( osType == "dateTime" )
                    eType = OFTDateTime;

                OGRFieldDefn oFieldDefnRaw( osNewFieldname,
                                            eType );
                poLayer->InsertNewField( nInsertFieldIdx,
                                            oFieldDefnRaw,
                                            osDerivedFieldXPath );
                nInsertFieldIdx ++;
            }
        }
    }
}
