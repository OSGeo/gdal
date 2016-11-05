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

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#pragma clang diagnostic ignored "-Wdocumentation"
#endif
#include "json.h"
#ifdef __clang
#pragma clang diagnostic pop
#endif


CPL_CVSID("$Id$");

/************************************************************************/
/*                        GMLASBinInputStream                           */
/************************************************************************/

class GMLASBinInputStream : public BinInputStream
{
    VSILFILE*         m_fp;

public :

             GMLASBinInputStream(VSILFILE* fp);
    virtual ~GMLASBinInputStream();

    virtual XMLFilePos curPos() const;
    virtual XMLSize_t readBytes(XMLByte* const toFill, const XMLSize_t maxToRead);
    virtual const XMLCh* getContentType() const ;
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
    return NULL;
}

/************************************************************************/
/*                          GMLASInputSource()                          */
/************************************************************************/

GMLASInputSource::GMLASInputSource(const char* pszFilename,
                                   VSILFILE* fp,
                                   bool bOwnFP,
                                   MemoryManager* const manager)
    : InputSource(manager)
{
    m_fp = fp;
    m_bOwnFP = bOwnFP;
    m_osFilename = pszFilename;
    XMLCh* pFilename = XMLString::transcode(pszFilename);
    setPublicId(pFilename);
    setSystemId(pFilename);
    XMLString::release( &pFilename );
    m_nCounter = 0;
    m_pnCounter = &m_nCounter;
    m_cbk = NULL;
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
        return NULL;
    }
    (*m_pnCounter) ++;
    if( m_fp == NULL )
        return NULL;
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

    if ( resourceId == NULL || resourceId[0] == 0 )
        resourceId = e.getSystemId();

    CPLError(eErr, CPLE_AppDefined, "%s:%d:%d %s",
             transcode(resourceId).c_str(),
             static_cast<int>(e.getLineNumber()),
             static_cast<int>(e.getColumnNumber()),
             transcode(e.getMessage()).c_str());
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
/* strongly depend on Xerces behaviour here... */
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
    CPLString osSystemId(transcode(systemId));

    CPLString osNewPath;
    VSILFILE* fp = m_oCache.Open(osSystemId,
                                 m_aosPathStack.back(),
                                 osNewPath);

    if( fp != NULL )
    {
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

void GMLASReader::Context::Dump()
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
    m_poSAXReader = NULL;
    m_fp = NULL;
    m_GMLInputSource = NULL;
    m_bFirstIteration = true;
    m_bEOF = false;
    m_bInterrupted = false;
    m_papoLayers = NULL;
    m_nLevel = 0;
    m_oCurCtxt.m_nLevel = 0;
    m_oCurCtxt.m_poLayer = NULL;
    m_oCurCtxt.m_poGroupLayer = NULL;
    m_oCurCtxt.m_nGroupLayerLevel = -1;
    m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;
    m_oCurCtxt.m_poFeature = NULL;
    m_nCurFieldIdx = -1;
    m_nCurGeomFieldIdx = -1;
    m_nCurFieldLevel = 0;
    m_bIsXMLBlob = false;
    m_bIsXMLBlobIncludeUpper = false;
    m_nTextContentListEstimatedSize = 0;
    m_poLayerOfInterest = NULL;
    m_nMaxLevel = atoi(CPLGetConfigOption("GMLAS_XML_MAX_LEVEL", "100"));
    m_nMaxContentSize = static_cast<size_t>(
          atoi(CPLGetConfigOption("GMLAS_XML_MAX_CONTENT_SIZE", "512000000")));
    m_bValidate = false;
    m_poEntityResolver = NULL;
    m_nLevelSilentIgnoredXPath = -1;
    m_eSwapCoordinates = GMLAS_SWAP_AUTO;
    m_bInitialPass = false;
    m_nFileSize = 0;
    m_bWarnUnexpected =
        CPLTestBool(CPLGetConfigOption("GMLAS_WARN_UNEXPECTED", "FALSE"));
}

/************************************************************************/
/*                            ~GMLASReader()                            */
/************************************************************************/

GMLASReader::~GMLASReader()
{
    delete m_poSAXReader;
    delete m_GMLInputSource;
    if( m_oCurCtxt.m_poFeature != NULL && !m_aoStackContext.empty() &&
        m_oCurCtxt.m_poFeature != m_aoStackContext.back().m_poFeature )
    {
        delete m_oCurCtxt.m_poFeature;
    }
    for( size_t i = 0; i < m_aoStackContext.size(); i++ )
    {
        delete m_aoStackContext[i].m_poFeature;
    }
    for( size_t i = 0; i < m_aoFeaturesReady.size(); i++ )
    {
        delete m_aoFeaturesReady[i].first;
    }
    if( !m_apsXMLNodeStack.empty() )
    {
        CPLDestroyXMLNode(m_apsXMLNodeStack[0].psNode);
    }
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
/*                          LoadXSDInParser()                           */
/************************************************************************/

bool GMLASReader::LoadXSDInParser( SAX2XMLReader* poParser,
                                   GMLASXSDCache& oCache,
                                   GMLASBaseEntityResolver& oXSDEntityResolver,
                                   const CPLString& osBaseDirname,
                                   const CPLString& osXSDFilename,
                                   Grammar** ppoGrammar )
{
    if( ppoGrammar != NULL )
        *ppoGrammar = NULL;

    const CPLString osModifXSDFilename(
        (osXSDFilename.find("http://") != 0 &&
        osXSDFilename.find("https://") != 0 &&
        CPLIsFilenameRelative(osXSDFilename)) ?
            CPLString(CPLFormFilename(osBaseDirname, osXSDFilename, NULL)) :
            osXSDFilename );
    CPLString osResolvedFilename;
    VSILFILE* fpXSD = oCache.Open( osModifXSDFilename, CPLString(),
                                   osResolvedFilename );
    if( fpXSD == NULL )
    {
        return false;
    }

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
    ErrorHandler* poOldErrorHandler = poParser->getErrorHandler();
    poParser->setErrorHandler( &oErrorHandler);

    GMLASInputSource oSource(osResolvedFilename, fpXSD, false);
    const bool bCacheGrammar = true;
    Grammar* poGrammar = poParser->loadGrammar(oSource,
                                            Grammar::SchemaGrammarType,
                                            bCacheGrammar);

    // Restore previous handlers
    poParser->setEntityResolver( poOldEntityResolver );
    poParser->setErrorHandler( poOldErrorHandler );
    VSIFCloseL(fpXSD);

    if( poGrammar == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "loadGrammar failed");
        return false;
    }
    if( oErrorHandler.hasFailed() )
    {
        return false;
    }

    if( ppoGrammar != NULL )
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
                       const std::vector<PairURIFilename>& aoXSDs)
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

    m_poSAXReader->setErrorHandler(&m_oErrorHandler);

    if( bValidate )
    {
        // Enable validation.
        m_poSAXReader->setFeature (XMLUni::fgSAX2CoreValidation, true);
        m_poSAXReader->setFeature (XMLUni::fgXercesSchema, true);
        m_poSAXReader->setFeature (XMLUni::fgXercesSchemaFullChecking, true);

        // We want all errors to be reported
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
                                      osBaseDirname, osXSDFilename ) )
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
    else if( eType == OFTDateTime )
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
    m_aoFeaturesReady.push_back(
        std::pair<OGRFeature*, OGRGMLASLayer*>(poFeature, poLayer) );
}

/************************************************************************/
/*                          CreateNewFeature                            */
/************************************************************************/

void GMLASReader::CreateNewFeature(const CPLString& osLocalname)
{
#ifdef DEBUG_VERBOSE
    CPLDebug("GMLAS", "CreateNewFeature(%s)", osLocalname.c_str());
#endif
    m_oCurCtxt.m_poFeature = new OGRFeature(
                m_oCurCtxt.m_poLayer->GetLayerDefn() );

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

    if (psLastChildParent == NULL)
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
    if( !m_bInitialPass )
    {
        m_osTextContent += "<";
        m_osTextContent += osXPath;
    }

    CPLXMLNode* psNode = NULL;
    if( m_nCurGeomFieldIdx >= 0 )
    {
        psNode = CPLCreateXMLNode( NULL, CXT_Element, osXPath );
        if( !m_apsXMLNodeStack.empty() )
        {
            AttachAsLastChild(psNode);
        }
    }

    CPLXMLNode* psLastChild = NULL;
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

        if( psNode != NULL )
        {
            CPLXMLNode* psAttrNode = CPLCreateXMLNode( NULL, CXT_Attribute,
                                                       osAttrXPath );
            CPLCreateXMLNode(psAttrNode, CXT_Text, osAttrValue);

            if( psLastChild == NULL )
            {
                psNode->psChild = psAttrNode;
            }
            else
            {
                psLastChild->psNext = psAttrNode;
            }
            psLastChild = psAttrNode;
        }

        if( !m_bInitialPass )
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
    if( !m_bInitialPass )
        m_osTextContent += ">";

    if( psNode != NULL )
    {
        /* Push the element on the stack */
        NodeLastChild sNewNodeLastChild;
        sNewNodeLastChild.psNode = psNode;
        sNewNodeLastChild.psLastChild = psLastChild;
        m_apsXMLNodeStack.push_back(sNewNodeLastChild);
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
    return NULL;
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
                ...</first_elt_of_group>

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

    // Deal with XML content
    if( m_bIsXMLBlob )
    {
        BuildXMLBlobStartElement(osXPath, attrs);
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
        CPLString& osLayerXPath(m_osLayerXPath);
        osLayerXPath = (*m_papoLayers)[i]->GetFeatureClass().GetXPath();
        if( (*m_papoLayers)[i]->GetFeatureClass().IsRepeatedSequence() )
        {
            size_t iPosExtra = osLayerXPath.find(";extra=");
            if (iPosExtra != std::string::npos)
            {
                osLayerXPath.resize(iPosExtra);
            }
        }

        const bool bIsGroup = (*m_papoLayers)[i]->GetFeatureClass().IsGroup();

        // Are we entering or staying in a group ?
        const bool bIsMatchingGroup =
            (bIsGroup &&
             (*m_papoLayers)[i]->GetOGRFieldIndexFromXPath(m_osCurSubXPath) != -1 );

        const bool bIsMatchingRepeatedSequence =
             ((*m_papoLayers)[i]->GetFeatureClass().IsRepeatedSequence()  &&
             m_oCurCtxt.m_poLayer != NULL &&
             m_oCurCtxt.m_poLayer != (*m_papoLayers)[i] &&
             m_oCurCtxt.m_poLayer->GetFeatureClass().GetXPath() ==
                    osLayerXPath &&
             (*m_papoLayers)[i]->GetOGRFieldIndexFromXPath(m_osCurSubXPath) >= 0);

        if( // Case where we haven't yet entered the top-level element, which may
            // be in container elements
            (m_osCurSubXPath.empty() &&
             osLayerXPath == osXPath && !bIsGroup) ||

            // Case where we are a sub-element of a top-level feature
            (!m_osCurSubXPath.empty() &&
             osLayerXPath == m_osCurSubXPath && !bIsGroup) ||

            // Case where we are a sub-element of a (repeated) group of a
            // top-level feature
            bIsMatchingGroup ||

            // Needed to handle sequence_1_unbounded_non_simplifiable.subelement case of data/gmlas_test1.xml
            bIsMatchingRepeatedSequence ||

            // Case where we go back from a sub-element of a (repeated) group
            // of a top-level feature to a regular sub-element of that top-level
            // feature
            (m_oCurCtxt.m_poGroupLayer != NULL &&
             (*m_papoLayers)[i]->GetOGRFieldIndexFromXPath(m_osCurSubXPath) >= 0) )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("GMLAS", "Matches layer %s (%s)",
                     (*m_papoLayers)[i]->GetName(),
                     (*m_papoLayers)[i]->GetFeatureClass().GetXPath().c_str());
#endif

            if( (*m_papoLayers)[i]->GetParent() != NULL &&
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
                if( m_oCurCtxt.m_poGroupLayer == NULL )
                {
                    m_oCurCtxt.m_poFeature = NULL;
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
                    m_oCurCtxt.m_poFeature = NULL;
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
                    // Cf group_case_C in above doc

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
                        m_aoStackContext.push_back( oContext );
                    }

                    m_oCurCtxt.m_poFeature = NULL;
                    m_oCurCtxt.m_poGroupLayer = NULL;
                    m_oCurCtxt.m_nGroupLayerLevel = -1;
                    m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;
                    m_oCurCtxt.m_poLayer = (*m_papoLayers)[i];
                    if( m_aoStackContext.empty() )
                        m_osCurSubXPath = osXPath;
                }
            }

            if( m_oCurCtxt.m_poFeature == NULL )
            {
                CPLAssert( bPushNewState );
                CreateNewFeature(osLocalname);
            }

            if( bPushNewState )
            {
                Context oContext;
                oContext = m_oCurCtxt;
                oContext.m_nLevel = m_nLevel;
                m_aoStackContext.push_back( oContext );
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
        if( idx >= 0 || geom_idx >= 0 )
        {

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
                !m_oCurCtxt.m_poLayer->GetFeatureClass().IsRepeatedSequence() )
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
                m_bIsXMLBlob = (oField.GetType() == GMLAS_FT_ANYTYPE ||
                                m_nCurGeomFieldIdx != -1 );
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
                        m_aoStackContext.push_back( oContext );
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
                                osAbstractElementXPath + "|" + osNestedXPath);
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
                        m_aoStackContext.push_back( oContext );
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
                        poJunctionFeature->SetField("occurrence", nCounter);
                        poJunctionFeature->SetField("parent_pkid", osParentId);
                        poJunctionFeature->SetField("child_pkid", osChildId);
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
                    m_oMapURIToPrefix[ transcode( attrs.getURI(0) ) ] == "xsi" &&
                    transcode(attrs.getLocalName(0)) == "nil" ) )
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

        if( !bHasProcessedAttributes )
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
        m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(m_osCurSubXPath + "/@*");
    json_object* poWildcard = NULL;

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

        int nAttrIdx = m_oCurCtxt.m_poLayer->GetOGRFieldIndexFromXPath(osAttrXPath);
        int nFCIdx;
        if( nAttrIdx >= 0 )
        {
            SetField( m_oCurCtxt.m_poFeature,
                        m_oCurCtxt.m_poLayer,
                        nAttrIdx, osAttrValue );

            if( osAttrNSPrefix == "xlink" &&
                osAttrLocalname == "href" &&
                !osAttrValue.empty() )
            {
                ProcessXLinkHref( osAttrXPath, osAttrValue );
            }
        }

        else if( osAttrNSPrefix != "xmlns" &&
                 osAttrLocalname != "xmlns" &&
                    !(osAttrNSPrefix == "xsi" &&
                        osAttrLocalname == "schemaLocation") &&
                    !(osAttrNSPrefix == "xsi" &&
                        osAttrLocalname == "noNamespaceSchemaLocation") &&
                    !(osAttrNSPrefix == "xsi" &&
                        osAttrLocalname == "nil") &&
                    // Do not warn about fixed attributes on geometry properties
                    !(m_nCurGeomFieldIdx >= 0 && (
                    (osAttrNSPrefix == "xlink" && osAttrLocalname == "type") ||
                    (osAttrNSPrefix == "" && osAttrLocalname == "owns"))) )
        {
            CPLString osMatchedXPath;
            if( nWildcardAttrIdx >= 0 )
            {
                if( poWildcard == NULL )
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
    if( poWildcard != NULL )
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
                    !m_oCurCtxt.m_poFeature->IsFieldSet(i) )
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

void GMLASReader::ProcessXLinkHref( const CPLString& osAttrXPath,
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
    }
    else
    {
        const int nRuleIdx =
                        m_oXLinkResolver.GetMachingResolutionRule(osAttrValue);
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
                    if( psNode != NULL )
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
                        for( ; psIter != NULL; psIter = psIter->psNext )
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
        std::map<CPLString, size_t>::const_iterator oIter =
                        oMapFieldXPathToIdx.find(osMatchedXPathRule);
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
            psNode->psChild != NULL &&
            psNode->psChild->eType == CXT_Text &&
            psNode->psChild->psNext == NULL )
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
        if( m_oCurCtxt.m_poFeature->IsFieldSet(nAttrIdx) &&
            m_oCurCtxt.m_poFeature->GetFieldDefnRef(nAttrIdx)->GetType() == OFTString )
        {
            osVal = m_oCurCtxt.m_poFeature->GetFieldAsString(nAttrIdx) +
                    CPLString(" ") + osVal;
        }
        SetField( m_oCurCtxt.m_poFeature, m_oCurCtxt.m_poLayer,
                    nAttrIdx, osVal );
    }

    CPLXMLNode* psIter = psNode->psChild;
    for( ; psIter != NULL; psIter = psIter->psNext )
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
            const   XMLCh* const    /*qname*/
        )
{
    m_nLevel --;

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

        // Transform boolean values to something that OGR understands
        if( (eType == OFTIntegerList || eType == OFTInteger) &&
             m_oCurCtxt.m_poFeature->GetFieldDefnRef(m_nCurFieldIdx)->GetSubType() ==
                                                                   OFSTBoolean )
        {
            if( m_osTextContent == "true" )
                m_osTextContent = "1";
            else
                m_osTextContent = "0";
        }

        // Assign XML content to field value
        if( IsArrayType(eType) )
        {
            const int nFCFieldIdx =
                m_oCurCtxt.m_poLayer->GetFCFieldIndexFromOGRFieldIdx(m_nCurFieldIdx);
            if( nFCFieldIdx >= 0 &&
                m_oCurCtxt.m_poLayer->GetFeatureClass().GetFields()[nFCFieldIdx].IsList() )
            {
                char** papszTokens = CSLTokenizeString2( m_osTextContent.c_str(), " ", 0 );
                m_oCurCtxt.m_poFeature->SetField( m_nCurFieldIdx, papszTokens );
                CSLDestroy(papszTokens);
            }
            else if( m_nTextContentListEstimatedSize > m_nMaxContentSize )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Too much repeated data in a single element");
                m_bParsingError = true;
            }
            else
            {
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
            if( m_bIsXMLBlobIncludeUpper && !m_bInitialPass )
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
        if( m_bIsXMLBlobIncludeUpper )
        {
            m_apsXMLNodeStack.pop_back();
        }

        if( !m_apsXMLNodeStack.empty() )
        {
            CPLAssert( m_apsXMLNodeStack.size() == 1 );

            ProcessGeometry();
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
            if( m_nLevel >= m_nCurFieldLevel + 1 )
                m_apsXMLNodeStack.pop_back();
        }

        if(  !m_bInitialPass )
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

    if( !m_aoStackContext.empty() &&
        m_aoStackContext.back().m_nLevel == m_nLevel )
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
            m_aoStackContext.pop_back();
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
            m_aoStackContext.pop_back();
            if( !m_aoStackContext.empty() )
            {
                m_oCurCtxt = m_aoStackContext.back();
                m_oCurCtxt.m_osCurSubXPath.clear();
                if( m_oCurCtxt.m_nLevel < 0 )
                {
                    m_aoStackContext.pop_back();
                    CPLAssert( !m_aoStackContext.empty() );
                    m_oCurCtxt.m_poLayer = m_aoStackContext.back().m_poLayer;
                }
            }
            else
            {
                m_oCurCtxt.m_poFeature = NULL;
                m_oCurCtxt.m_poLayer = NULL;
                m_oCurCtxt.m_poGroupLayer = NULL;
                m_oCurCtxt.m_nGroupLayerLevel = -1;
                m_oCurCtxt.m_nLastFieldIdxGroupLayer = -1;
            }
            m_nCurFieldIdx = -1;
        }
        m_oCurCtxt.m_oMapCounter = oMapCounter;
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
}

/************************************************************************/
/*                            ProcessGeometry()                         */
/************************************************************************/

void GMLASReader::ProcessGeometry()
{
    CPLXMLNode* psInterestNode = m_apsXMLNodeStack.back().psNode;
    m_apsXMLNodeStack.pop_back();

    OGRGeomFieldDefn* poGeomFieldDefn =
        m_oCurCtxt.m_poFeature->GetGeomFieldDefnRef(
                                    m_nCurGeomFieldIdx);

    if( m_bInitialPass )
    {
        const char* pszSRSName = CPLGetXMLValue(psInterestNode,
                                                "srsName", NULL);
        if( pszSRSName != NULL )
        {
            // If we are doing a first pass, store the SRS of the geometry
            // column
            if( !m_oSetGeomFieldsWithUnknownSRS.empty() &&
                 m_oSetGeomFieldsWithUnknownSRS.find(poGeomFieldDefn) !=
                        m_oSetGeomFieldsWithUnknownSRS.end() )
            {
                OGRSpatialReference* poSRS =
                                new OGRSpatialReference();
                if( poSRS->SetFromUserInput( pszSRSName ) == OGRERR_NONE )
                {
                    OGR_SRSNode *poGEOGCS = poSRS->GetAttrNode( "GEOGCS" );
                    if( poGEOGCS != NULL )
                        poGEOGCS->StripNodes( "AXIS" );

                    OGR_SRSNode *poPROJCS = poSRS->GetAttrNode( "PROJCS" );
                    if (poPROJCS != NULL &&
                        poSRS->EPSGTreatsAsNorthingEasting())
                    {
                        poPROJCS->StripNodes( "AXIS" );
                    }

                    m_oMapGeomFieldDefnToSRSName[poGeomFieldDefn] = pszSRSName;
                    poGeomFieldDefn->SetSpatialRef(poSRS);
                }
                poSRS->Release();
                m_oSetGeomFieldsWithUnknownSRS.erase(poGeomFieldDefn);
            }
        }
        CPLDestroyXMLNode( psInterestNode );
        return;
    }

#ifdef DEBUG_VERBOSE
    {
        char* pszXML = CPLSerializeXMLTree(psInterestNode);
        CPLDebug("GML", "geometry = %s", pszXML);
        CPLFree(pszXML);
    }
#endif

    OGRGeometry* poGeom = reinterpret_cast<OGRGeometry*>
                    (OGR_G_CreateFromGMLTree( psInterestNode ));
    if( poGeom != NULL )
    {
        const char* pszSRSName = CPLGetXMLValue(psInterestNode,
                                                "srsName", NULL);
        bool bSwapXY = false;
        if( pszSRSName != NULL )
        {
            // Check if the srsName indicates unusual axis order,
            // and if so swap x and y coordinates.
            std::map<CPLString, bool>::iterator oIter =
                m_oMapSRSNameToInvertedAxis.find(pszSRSName);
            if( oIter == m_oMapSRSNameToInvertedAxis.end() )
            {
                OGRSpatialReference oSRS;
                oSRS.SetFromUserInput( pszSRSName );
                bSwapXY = CPL_TO_BOOL(oSRS.EPSGTreatsAsLatLong()) ||
                            CPL_TO_BOOL(oSRS.EPSGTreatsAsNorthingEasting());
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
        if( pszSRSName != NULL && poGeomFieldDefn->GetSpatialRef() != NULL &&
            m_oMapGeomFieldDefnToSRSName[poGeomFieldDefn] != pszSRSName )
        {
            bool bReprojectionOK = false;
            OGRSpatialReference oSRS;
            if( oSRS.SetFromUserInput( pszSRSName ) == OGRERR_NONE )
            {
                OGRCoordinateTransformation* poCT =
                    OGRCreateCoordinateTransformation( &oSRS,
                                            poGeomFieldDefn->GetSpatialRef() );
                if( poCT != NULL )
                {
                    bReprojectionOK = (poGeom->transform( poCT ) == OGRERR_NONE);
                    delete poCT;
                }
            }
            if( !bReprojectionOK )
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Reprojection fom %s to %s failed",
                         pszSRSName,
                         m_oMapGeomFieldDefnToSRSName[poGeomFieldDefn].c_str());
                delete poGeom;
                poGeom = NULL;
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

        if( poGeom != NULL )
        {
            // Deal with possibly repeated geometries by building
            // a geometry collection. We could also create a
            // nested table, but that would probably be less
            // convenient to use.
            OGRGeometry* poPrevGeom = m_oCurCtxt.m_poFeature->
                                    StealGeometry(m_nCurGeomFieldIdx);
            if( poPrevGeom != NULL )
            {
                if( poPrevGeom->getGeometryType() ==
                                        wkbGeometryCollection )
                {
                    reinterpret_cast<OGRGeometryCollection*>(
                        poPrevGeom)->addGeometryDirectly(poGeom);
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
        char* pszXML = CPLSerializeXMLTree(psInterestNode);
        CPLDebug("GMLAS", "Non-recognized geometry: %s",
                    pszXML);
        CPLFree(pszXML);
    }
    CPLDestroyXMLNode( psInterestNode );
}

/************************************************************************/
/*                              characters()                            */
/************************************************************************/

void GMLASReader::characters( const XMLCh *const chars,
                              const XMLSize_t length )
{
    if( m_bInitialPass )
    {
        m_osTextContent = "1"; // dummy
        return;
    }

    if( m_bIsXMLBlob )
    {
        const CPLString& osText( transcode(chars, m_osText,
                                           static_cast<int>(length) ) );

        if( m_nCurGeomFieldIdx >= 0 &&
            // Check the stack is not empty in case of space chars before the
            // starting node
            !m_apsXMLNodeStack.empty() )
        {
            // Merge content in current text node if it exists
            NodeLastChild& sNodeLastChild = m_apsXMLNodeStack.back();
            if( sNodeLastChild.psLastChild != NULL &&
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
                psNode->psNext = NULL;
                psNode->psChild = NULL;
                AttachAsLastChild( psNode );
            }
        }

        if( m_nCurFieldIdx >= 0 )
        {
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
    // In practice we will never have more than 2 features
    while( !m_aoFeaturesReady.empty() )
    {
        OGRFeature* m_poFeatureReady = m_aoFeaturesReady[0].first;
        OGRGMLASLayer* m_poFeatureReadyLayer = m_aoFeaturesReady[0].second;
        m_aoFeaturesReady.erase( m_aoFeaturesReady.begin() );

        if( m_poLayerOfInterest == NULL ||
            m_poLayerOfInterest == m_poFeatureReadyLayer )
        {
            if( ppoBelongingLayer )
                *ppoBelongingLayer = m_poFeatureReadyLayer;
            return m_poFeatureReady;
        }
        delete m_poFeatureReady;
    }

    if( m_bEOF )
        return NULL;

    try
    {
        if( m_bFirstIteration )
        {
            m_bFirstIteration = false;
            if( !m_poSAXReader->parseFirst( *m_GMLInputSource, m_oToFill ) )
            {
                m_bParsingError = true;
                m_bEOF = true;
                return NULL;
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

            // In practice we will never have more than 2 features
            while( !m_aoFeaturesReady.empty() )
            {
                OGRFeature* m_poFeatureReady = m_aoFeaturesReady[0].first;
                OGRGMLASLayer* m_poFeatureReadyLayer =
                                               m_aoFeaturesReady[0].second;
                m_aoFeaturesReady.erase( m_aoFeaturesReady.begin() );

                if( m_poLayerOfInterest == NULL ||
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
                            return NULL;
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

    return NULL;
}

/************************************************************************/
/*                              RunFirstPass()                          */
/************************************************************************/

bool GMLASReader::RunFirstPass(GDALProgressFunc pfnProgress,
                               void* pProgressData,
                               bool bRemoveUnusedLayers,
                               bool bRemoveUnusedFields)
{
    m_bInitialPass = true;

    // Store in m_oSetGeomFieldsWithUnknownSRS the geometry fields
    std::set<OGRGMLASLayer*> oSetUnreferencedLayers;
    std::map<OGRGMLASLayer*, std::set<int> > oMapUnusedFields;
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
            oMapUnusedFields[poLayer].insert(j);
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
         bHasURLSpecificRules);

    // Loop on features until we have determined the SRS of all geometry
    // columns, or potentially on the whole file for the above reasons
    OGRGMLASLayer* poLayer;
    OGRFeature* poFeature;
    while( (bDoFullPass || !m_oSetGeomFieldsWithUnknownSRS.empty() ) &&
           (poFeature = GetNextFeature(&poLayer, pfnProgress, pProgressData)) != NULL )
    {
        if( bRemoveUnusedLayers )
            oSetUnreferencedLayers.erase( poLayer );
        if( bRemoveUnusedFields )
        {
            std::set<int>& oSetUnusedFields = oMapUnusedFields[poLayer];
            OGRFeatureDefn* poFDefn = poLayer->GetLayerDefn();
            int nFieldCount = poFDefn->GetFieldCount();
            for(int j=0; j< nFieldCount; j++ )
            {
                if( poFeature->IsFieldSet(j) )
                    oSetUnusedFields.erase(j);
            }
        }
        delete poFeature;
    }

    CPLDebug("GMLAS", "End of first pass");

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
            std::set<int>& oSetUnusedFields = oMapUnusedFields[poLayer];
            std::set<int>::iterator oIter = oSetUnusedFields.begin();
            int nShiftIndex = 0;
            for( ; oIter != oSetUnusedFields.end(); ++oIter )
            {
                if( poLayer->RemoveField( (*oIter) - nShiftIndex ) )
                {
                    nShiftIndex ++;
                }
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
/*                    CreateFieldsForURLSpecificRules()                 */
/************************************************************************/

void GMLASReader::CreateFieldsForURLSpecificRules()
{
    std::map<OGRGMLASLayer*, std::map<CPLString, std::set<int> > >::const_iterator
        oIter = m_oMapXLinkFields.begin();
    for( ; oIter != m_oMapXLinkFields.end(); ++oIter )
    {
        OGRGMLASLayer* poLayer = oIter->first;
        const std::map<CPLString, std::set<int> >& oMap2 = oIter->second;
        std::map<CPLString, std::set<int> >::const_iterator oIter2 =
                                                            oMap2.begin();
        for( ; oIter2 != oMap2.end(); ++oIter2 )
        {
            const CPLString& osFieldXPath(oIter2->first);
            // Note that CreateFieldsForURLSpecificRule() running on a previous
            // iteration will have inserted new OGR fields, so we really need
            // to compute that index now.
            const int nFieldIdx = poLayer->GetOGRFieldIndexFromXPath(osFieldXPath);
            CPLAssert(nFieldIdx >= 0);
            int nInsertFieldIdx = nFieldIdx + 1;
            const std::set<int>& oSetRuleIndex = oIter2->second;
            std::set<int>::const_iterator oIter3 = oSetRuleIndex.begin();
            for( ; oIter3 != oSetRuleIndex.end(); ++ oIter3 )
            {
                const GMLASXLinkResolutionConf::URLSpecificResolution& oRule =
                    m_oXLinkResolver.GetConf().m_aoURLSpecificRules[*oIter3];
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
