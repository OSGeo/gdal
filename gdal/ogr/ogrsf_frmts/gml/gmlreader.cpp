/******************************************************************************
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReader class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_port.h"
#include "gmlreaderp.h"
#include "gmlreader.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <set>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "gmlutils.h"
#include "ogr_geometry.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            ~IGMLReader()                             */
/************************************************************************/

IGMLReader::~IGMLReader() {}

/************************************************************************/
/* ==================================================================== */
/*                  No XERCES or EXPAT Library                          */
/* ==================================================================== */
/************************************************************************/
#if !defined(HAVE_XERCES) && !defined(HAVE_EXPAT)

/************************************************************************/
/*                          CreateGMLReader()                           */
/************************************************************************/

IGMLReader *CreateGMLReader(bool /*bUseExpatParserPreferably*/,
                            bool /*bInvertAxisOrderIfLatLong*/,
                            bool /*bConsiderEPSGAsURN*/,
                            GMLSwapCoordinatesEnum /* eSwapCoordinates */,
                            bool /*bGetSecondaryGeometryOption*/)
{
    CPLError(CE_Failure, CPLE_AppDefined,
             "Unable to create Xerces C++ or Expat based GML reader, Xerces "
             "or Expat support not configured into GDAL/OGR.");
    return nullptr;
}

/************************************************************************/
/* ==================================================================== */
/*                  With XERCES or EXPAT Library                        */
/* ==================================================================== */
/************************************************************************/
#else /* defined(HAVE_XERCES) || defined(HAVE_EXPAT) */

/************************************************************************/
/*                          CreateGMLReader()                           */
/************************************************************************/

IGMLReader *CreateGMLReader(bool bUseExpatParserPreferably,
                            bool bInvertAxisOrderIfLatLong,
                            bool bConsiderEPSGAsURN,
                            GMLSwapCoordinatesEnum eSwapCoordinates,
                            bool bGetSecondaryGeometryOption)

{
    return new GMLReader(bUseExpatParserPreferably,
                         bInvertAxisOrderIfLatLong,
                         bConsiderEPSGAsURN,
                         eSwapCoordinates,
                         bGetSecondaryGeometryOption);
}

#endif

CPLMutex *GMLReader::hMutex = nullptr;

/************************************************************************/
/*                             GMLReader()                              */
/************************************************************************/

GMLReader::GMLReader(
#if !defined(HAVE_EXPAT) || !defined(HAVE_XERCES)
CPL_UNUSED
#endif
                     bool bUseExpatParserPreferably,
                     bool bInvertAxisOrderIfLatLong,
                     bool bConsiderEPSGAsURN,
                     GMLSwapCoordinatesEnum eSwapCoordinates,
                     bool bGetSecondaryGeometryOption ) :
    m_bClassListLocked(false),
    m_nClassCount(0),
    m_papoClass(nullptr),
    m_bLookForClassAtAnyLevel(false),
    m_pszFilename(nullptr),
#ifndef HAVE_XERCES
    bUseExpatReader(true),
#else
    bUseExpatReader(false),
#endif
    m_poGMLHandler(nullptr),
#ifdef HAVE_XERCES
    m_poSAXReader(nullptr),
    m_poCompleteFeature(nullptr),
    m_GMLInputSource(nullptr),
    m_bEOF(false),
    m_bXercesInitialized(false),
#endif
#ifdef HAVE_EXPAT
    oParser(nullptr),
    ppoFeatureTab(nullptr),
    nFeatureTabLength(0),
    nFeatureTabIndex(0),
    nFeatureTabAlloc(0),
    pabyBuf(nullptr),
#endif
    fpGML(nullptr),
    m_bReadStarted(false),
    m_poState(nullptr),
    m_poRecycledState(nullptr),
    m_bStopParsing(false),
    // Experimental. Not publicly advertized. See commented doc in drv_gml.html
    m_bFetchAllGeometries(
        CPLTestBool(CPLGetConfigOption("GML_FETCH_ALL_GEOMETRIES", "NO"))),
    m_bInvertAxisOrderIfLatLong(bInvertAxisOrderIfLatLong),
    m_bConsiderEPSGAsURN(bConsiderEPSGAsURN),
    m_eSwapCoordinates(eSwapCoordinates),
    m_bGetSecondaryGeometryOption(bGetSecondaryGeometryOption),
    m_pszGlobalSRSName(nullptr),
    m_bCanUseGlobalSRSName(false),
    m_pszFilteredClassName(nullptr),
    m_nFilteredClassIndex(-1),
    m_nHasSequentialLayers(-1),
    // Must be in synced in OGR_G_CreateFromGML(), OGRGMLLayer::OGRGMLLayer(),
    // and GMLReader::GMLReader().
    m_bFaceHoleNegative(CPLTestBool(CPLGetConfigOption("GML_FACE_HOLE_NEGATIVE", "NO"))),
    m_bSetWidthFlag(true),
    m_bReportAllAttributes(false),
    m_bIsWFSJointLayer(false),
    m_bEmptyAsNull(true)
{
#ifndef HAVE_XERCES
#else
#ifdef HAVE_EXPAT
    if( bUseExpatParserPreferably )
        bUseExpatReader = true;
#endif
#endif

#if defined(HAVE_EXPAT) && defined(HAVE_XERCES)
    if( bUseExpatReader )
        CPLDebug("GML", "Using Expat reader");
    else
        CPLDebug("GML", "Using Xerces reader");
#endif
}

/************************************************************************/
/*                             ~GMLReader()                             */
/************************************************************************/

GMLReader::~GMLReader()

{
    GMLReader::ClearClasses();

    CPLFree(m_pszFilename);

    CleanupParser();

    delete m_poRecycledState;

#ifdef HAVE_XERCES
    if( m_bXercesInitialized )
        OGRDeinitializeXerces();
#endif
#ifdef HAVE_EXPAT
    CPLFree(pabyBuf);
#endif

    if (fpGML)
        VSIFCloseL(fpGML);
    fpGML = nullptr;

    CPLFree(m_pszGlobalSRSName);

    CPLFree(m_pszFilteredClassName);
}

/************************************************************************/
/*                          SetSourceFile()                             */
/************************************************************************/

void GMLReader::SetSourceFile( const char *pszFilename )

{
    CPLFree(m_pszFilename);
    m_pszFilename = CPLStrdup(pszFilename);
}

/************************************************************************/
/*                       GetSourceFileName()                           */
/************************************************************************/

const char *GMLReader::GetSourceFileName() { return m_pszFilename; }

/************************************************************************/
/*                               SetFP()                                */
/************************************************************************/

void GMLReader::SetFP(VSILFILE *fp) { fpGML = fp; }

/************************************************************************/
/*                            SetupParser()                             */
/************************************************************************/

bool GMLReader::SetupParser()

{
    if (fpGML == nullptr)
        fpGML = VSIFOpenL(m_pszFilename, "rt");
    if (fpGML != nullptr)
        VSIFSeekL(fpGML, 0, SEEK_SET);

    int bRet = -1;
#ifdef HAVE_EXPAT
    if (bUseExpatReader)
        bRet = SetupParserExpat();
#endif

#ifdef HAVE_XERCES
    if (!bUseExpatReader)
        bRet = SetupParserXerces();
#endif
    if (bRet < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "SetupParser(): should not happen");
        return false;
    }

    if (!bRet)
        return false;

    m_bReadStarted = false;

    // Push an empty state.
    PushState(m_poRecycledState ? m_poRecycledState : new GMLReadState());
    m_poRecycledState = nullptr;

    return true;
}

#ifdef HAVE_XERCES
/************************************************************************/
/*                        SetupParserXerces()                           */
/************************************************************************/

bool GMLReader::SetupParserXerces()
{
    if( !m_bXercesInitialized )
    {
        if( !OGRInitializeXerces() )
            return false;
        m_bXercesInitialized = true;
    }

    // Cleanup any old parser.
    if( m_poSAXReader != nullptr )
        CleanupParser();

    // Create and initialize parser.
    XMLCh *xmlUriValid = nullptr;
    XMLCh *xmlUriNS = nullptr;

    try
    {
        m_poSAXReader = XMLReaderFactory::createXMLReader();

        GMLXercesHandler *poXercesHandler = new GMLXercesHandler(this);
        m_poGMLHandler = poXercesHandler;

        m_poSAXReader->setContentHandler(poXercesHandler);
        m_poSAXReader->setErrorHandler(poXercesHandler);
        m_poSAXReader->setLexicalHandler(poXercesHandler);
        m_poSAXReader->setEntityResolver(poXercesHandler);
        m_poSAXReader->setDTDHandler(poXercesHandler);

        xmlUriValid =
            XMLString::transcode("http://xml.org/sax/features/validation");
        xmlUriNS =
            XMLString::transcode("http://xml.org/sax/features/namespaces");

#if (OGR_GML_VALIDATION)
        m_poSAXReader->setFeature(xmlUriValid, true);
        m_poSAXReader->setFeature(xmlUriNS, true);

        m_poSAXReader->setFeature(XMLUni::fgSAX2CoreNameSpaces, true);
        m_poSAXReader->setFeature(XMLUni::fgXercesSchema, true);

        // m_poSAXReader->setDoSchema(true);
        // m_poSAXReader->setValidationSchemaFullChecking(true);
#else
        m_poSAXReader->setFeature(XMLUni::fgSAX2CoreValidation, false);

        m_poSAXReader->setFeature(XMLUni::fgXercesSchema, false);

#endif
        XMLString::release(&xmlUriValid);
        XMLString::release(&xmlUriNS);
    }
    catch (...)
    {
        XMLString::release(&xmlUriValid);
        XMLString::release(&xmlUriNS);

        CPLError(CE_Warning, CPLE_AppDefined,
                 "Exception initializing Xerces based GML reader.\n");
        return false;
    }

    if (m_GMLInputSource == nullptr && fpGML != nullptr)
        m_GMLInputSource = OGRCreateXercesInputSource(fpGML);

    return true;
}
#endif

/************************************************************************/
/*                        SetupParserExpat()                            */
/************************************************************************/

#ifdef HAVE_EXPAT
bool GMLReader::SetupParserExpat()
{
    // Cleanup any old parser.
    if( oParser != nullptr )
        CleanupParser();

    oParser = OGRCreateExpatXMLParser();
    m_poGMLHandler = new GMLExpatHandler(this, oParser);

    XML_SetElementHandler(oParser, GMLExpatHandler::startElementCbk,
                          GMLExpatHandler::endElementCbk);
    XML_SetCharacterDataHandler(oParser, GMLExpatHandler::dataHandlerCbk);
    XML_SetUserData(oParser, m_poGMLHandler);

    if (pabyBuf == nullptr)
        pabyBuf = static_cast<char *>(VSI_MALLOC_VERBOSE(PARSER_BUF_SIZE));
    if (pabyBuf == nullptr)
        return false;

    return true;
}
#endif

/************************************************************************/
/*                           CleanupParser()                            */
/************************************************************************/

void GMLReader::CleanupParser()

{
#ifdef HAVE_XERCES
    if( !bUseExpatReader && m_poSAXReader == nullptr )
        return;
#endif

#ifdef HAVE_EXPAT
    if ( bUseExpatReader && oParser == nullptr )
        return;
#endif

    while( m_poState )
        PopState();

#ifdef HAVE_XERCES
    delete m_poSAXReader;
    m_poSAXReader = nullptr;
    OGRDestroyXercesInputSource(m_GMLInputSource);
    m_GMLInputSource = nullptr;
    delete m_poCompleteFeature;
    m_poCompleteFeature = nullptr;
    m_bEOF = false;
#endif

#ifdef HAVE_EXPAT
    if (oParser)
        XML_ParserFree(oParser);
    oParser = nullptr;

    for( int i = nFeatureTabIndex; i < nFeatureTabLength; i++ )
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    nFeatureTabAlloc = 0;
    ppoFeatureTab = nullptr;
    m_osErrorMessage.clear();

#endif

    delete m_poGMLHandler;
    m_poGMLHandler = nullptr;

    m_bReadStarted = false;
}

/************************************************************************/
/*                        NextFeatureXerces()                           */
/************************************************************************/

#ifdef HAVE_XERCES
GMLFeature *GMLReader::NextFeatureXerces()

{
    GMLFeature *poReturn = nullptr;

    if (m_bEOF)
        return nullptr;

    try
    {
        if( !m_bReadStarted )
        {
            if( m_poSAXReader == nullptr )
                SetupParser();

            m_bReadStarted = true;

            if (m_poSAXReader == nullptr || m_GMLInputSource == nullptr)
                return nullptr;

            if( !m_poSAXReader->parseFirst( *m_GMLInputSource, m_oToFill ) )
                return nullptr;
        }

        while( m_poCompleteFeature == nullptr
               && !m_bStopParsing
               && m_poSAXReader->parseNext( m_oToFill ) ) {}

        if (m_poCompleteFeature == nullptr)
            m_bEOF = true;

        poReturn = m_poCompleteFeature;
        m_poCompleteFeature = nullptr;
    }
    catch (const XMLException &toCatch)
    {
        CPLString osErrMsg;
        transcode(toCatch.getMessage(), osErrMsg);
        CPLDebug("GML", "Error during NextFeature()! Message:\n%s",
                 osErrMsg.c_str());
        m_bStopParsing = true;
    }
    catch (const SAXException &toCatch)
    {
        CPLString osErrMsg;
        transcode(toCatch.getMessage(), osErrMsg);
        CPLError(CE_Failure, CPLE_AppDefined, "%s", osErrMsg.c_str());
        m_bStopParsing = true;
    }

    return poReturn;
}
#endif

#ifdef HAVE_EXPAT
GMLFeature *GMLReader::NextFeatureExpat()

{
    if (!m_bReadStarted)
    {
        if (oParser == nullptr)
            SetupParser();

        m_bReadStarted = true;
    }

    if (nFeatureTabIndex < nFeatureTabLength)
    {
        return ppoFeatureTab[nFeatureTabIndex++];
    }

    if( !m_osErrorMessage.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", m_osErrorMessage.c_str());
        m_osErrorMessage.clear();
        return nullptr;
    }

    if (fpGML == nullptr || m_bStopParsing || VSIFEofL(fpGML))
        return nullptr;

    nFeatureTabLength = 0;
    nFeatureTabIndex = 0;

    int nDone = 0;
    do
    {
        // Reset counter that is used to detect billion laugh attacks.
        static_cast<GMLExpatHandler *>(m_poGMLHandler)->
            ResetDataHandlerCounter();

        unsigned int nLen = static_cast<unsigned int>(
            VSIFReadL(pabyBuf, 1, PARSER_BUF_SIZE, fpGML));
        nDone = VSIFEofL(fpGML);

        // Some files, such as APT_AIXM.xml from
        // https://nfdc.faa.gov/webContent/56DaySub/2015-03-05/aixm5.1.zip
        // end with trailing nul characters. This test is not fully bullet-proof
        // in case the nul characters would occur at a buffer boundary.
        while( nDone && nLen > 0 && pabyBuf[nLen-1] == '\0' )
            nLen--;

        if (XML_Parse(oParser, pabyBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            // Defer emission of the error message until we have to return nullptr
            m_osErrorMessage.Printf(
                     "XML parsing of GML file failed : %s "
                     "at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            m_bStopParsing = true;
        }
        if (!m_bStopParsing)
            m_bStopParsing = static_cast<GMLExpatHandler*>(m_poGMLHandler)->
                HasStoppedParsing();
    } while (!nDone && !m_bStopParsing && nFeatureTabLength == 0);

    if( nFeatureTabLength )
        return ppoFeatureTab[nFeatureTabIndex++];

    if( !m_osErrorMessage.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", m_osErrorMessage.c_str());
        m_osErrorMessage.clear();
    }

    return nullptr;
}
#endif

GMLFeature *GMLReader::NextFeature()
{
#ifdef HAVE_EXPAT
    if (bUseExpatReader)
        return NextFeatureExpat();
#  ifdef HAVE_XERCES
    return NextFeatureXerces();
#  else
    CPLError(CE_Failure, CPLE_AppDefined, "NextFeature(): Should not happen");
    return nullptr;
#  endif
#else
#  ifdef HAVE_XERCES
    if (!bUseExpatReader)
        return NextFeatureXerces();
#  endif
    CPLError(CE_Failure, CPLE_AppDefined, "NextFeature(): Should not happen");
    return nullptr;
#endif
}

/************************************************************************/
/*                            PushFeature()                             */
/*                                                                      */
/*      Create a feature based on the named element.  If the            */
/*      corresponding feature class doesn't exist yet, then create      */
/*      it now.  A new GMLReadState will be created for the feature,    */
/*      and it will be placed within that state.  The state is          */
/*      pushed onto the readstate stack.                                */
/************************************************************************/

void GMLReader::PushFeature( const char *pszElement,
                             const char *pszFID,
                             int nClassIndex )

{
    int iClass = 0;

    if( nClassIndex != INT_MAX )
    {
        iClass = nClassIndex;
    }
    else
    {
    /* -------------------------------------------------------------------- */
    /*      Find the class of this element.                                 */
    /* -------------------------------------------------------------------- */
        for( ; iClass < m_nClassCount; iClass++ )
        {
            if( EQUAL(pszElement,m_papoClass[iClass]->GetElementName()) )
                break;
        }

    /* -------------------------------------------------------------------- */
    /*      Create a new feature class for this element, if there is no     */
    /*      existing class for it.                                          */
    /* -------------------------------------------------------------------- */
        if( iClass == m_nClassCount )
        {
            CPLAssert( !m_bClassListLocked );

            GMLFeatureClass *poNewClass = new GMLFeatureClass( pszElement );

            AddClass( poNewClass );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a feature of this feature class.  Try to set the fid     */
/*      if available.                                                   */
/* -------------------------------------------------------------------- */
    GMLFeature *poFeature = new GMLFeature( m_papoClass[iClass] );
    if( pszFID != nullptr )
    {
        poFeature->SetFID( pszFID );
    }

/* -------------------------------------------------------------------- */
/*      Create and push a new read state.                               */
/* -------------------------------------------------------------------- */
    GMLReadState *poState =
        m_poRecycledState ? m_poRecycledState : new GMLReadState();
    m_poRecycledState = nullptr;
    poState->m_poFeature = poFeature;
    PushState( poState );
}

/************************************************************************/
/*                          IsFeatureElement()                          */
/*                                                                      */
/*      Based on context and the element name, is this element a new    */
/*      GML feature element?                                            */
/************************************************************************/

int GMLReader::GetFeatureElementIndex( const char *pszElement,
                                       int nElementLength,
                                       GMLAppSchemaType eAppSchemaType )

{
    const char *pszLast = m_poState->GetLastComponent();
    const size_t nLenLast = m_poState->GetLastComponentLen();

    if( eAppSchemaType == APPSCHEMA_MTKGML )
    {
        if( m_poState->m_nPathLength != 1 )
            return -1;
    }
    else if( (nLenLast >= 6 && EQUAL(pszLast+nLenLast-6,"member")) ||
        (nLenLast >= 7 && EQUAL(pszLast+nLenLast-7,"members")) )
    {
        // Default feature name.
    }
    else
    {
        if (nLenLast == 4 && strcmp(pszLast, "dane") == 0)
        {
            // Polish TBD GML.
        }

        // Begin of OpenLS.
        else if (nLenLast == 19 && nElementLength == 15 &&
                 strcmp(pszLast, "GeocodeResponseList") == 0 &&
                 strcmp(pszElement, "GeocodedAddress") == 0)
        {
        }
        else if (nLenLast == 22 &&
                 strcmp(pszLast, "DetermineRouteResponse") == 0)
        {
            // We don't want the children of RouteInstructionsList
            // to be a single feature. We want each RouteInstruction
            // to be a feature.
            if (strcmp(pszElement, "RouteInstructionsList") == 0)
                return -1;
        }
        else if (nElementLength == 16 && nLenLast == 21 &&
                 strcmp(pszElement, "RouteInstruction") == 0 &&
                 strcmp(pszLast, "RouteInstructionsList") == 0)
        {
        }
        // End of OpenLS.

        else if (nLenLast > 6 &&
                 strcmp(pszLast + nLenLast - 6, "_layer") == 0 &&
                 nElementLength > 8 &&
                 strcmp(pszElement + nElementLength - 8, "_feature") == 0)
        {
            // GML answer of MapServer WMS GetFeatureInfo request.
        }

        // Begin of CSW SearchResults.
        else if (nElementLength == (int)strlen("BriefRecord") &&
                 nLenLast == strlen("SearchResults") &&
                 strcmp(pszElement, "BriefRecord") == 0 &&
                 strcmp(pszLast, "SearchResults") == 0)
        {
        }
        else if (nElementLength == (int)strlen("SummaryRecord") &&
                 nLenLast == strlen("SearchResults") &&
                 strcmp(pszElement, "SummaryRecord") == 0 &&
                 strcmp(pszLast, "SearchResults") == 0)
        {
        }
        else if (nElementLength == (int)strlen("Record") &&
                 nLenLast == strlen("SearchResults") &&
                 strcmp(pszElement, "Record") == 0 &&
                 strcmp(pszLast, "SearchResults") == 0)
        {
        }
        /* End of CSW SearchResults */

        else
        {
            if( m_bClassListLocked )
            {
                for( int i = 0; i < m_nClassCount; i++ )
                {
                    if( m_poState->osPath.size() + 1 + nElementLength == m_papoClass[i]->GetElementNameLen() &&
                        m_papoClass[i]->GetElementName()[m_poState->osPath.size()] == '|' &&
                        memcmp(m_poState->osPath.c_str(), m_papoClass[i]->GetElementName(), m_poState->osPath.size()) == 0 &&
                        memcmp(pszElement,m_papoClass[i]->GetElementName() + 1 + m_poState->osPath.size(), nElementLength) == 0 )
                    {
                        return i;
                    }
                }
                // Give a chance to find a feature class by element name
                // This is for example needed for
                // autotest/ogr/data/gml_jpfgd/BldA.xml that has a
                // feature at a low nesting level.
            }
            else {
                return -1;
            }
        }
    }

    // If the class list isn't locked, any element that is a featureMember
    // will do.
    if( !m_bClassListLocked )
        return INT_MAX;

    // otherwise, find a class with the desired element name.
    for( int i = 0; i < m_nClassCount; i++ )
    {
        if( nElementLength == (int)m_papoClass[i]->GetElementNameLen() &&
            memcmp(pszElement, m_papoClass[i]->GetElementName(), nElementLength) == 0 )
            return i;
    }

    return -1;
}

/************************************************************************/
/*                IsCityGMLGenericAttributeElement()                    */
/************************************************************************/

bool GMLReader::IsCityGMLGenericAttributeElement( const char *pszElement,
                                                  void* attr )

{
    if( strcmp(pszElement, "stringAttribute") != 0 &&
        strcmp(pszElement, "intAttribute") != 0 &&
        strcmp(pszElement, "doubleAttribute") != 0 )
        return false;

    char *pszVal = m_poGMLHandler->GetAttributeValue(attr, "name");
    if (pszVal == nullptr)
        return false;

    GMLFeatureClass *poClass = m_poState->m_poFeature->GetClass();

    // If the schema is not yet locked, then any simple element
    // is potentially an attribute.
    if( !poClass->IsSchemaLocked() )
    {
        CPLFree(pszVal);
        return true;
    }

    for( int i = 0; i < poClass->GetPropertyCount(); i++ )
    {
        if( strcmp(poClass->GetProperty(i)->GetSrcElement(),pszVal) == 0 )
        {
            CPLFree(pszVal);
            return true;
        }
    }

    CPLFree(pszVal);
    return false;
}

/************************************************************************/
/*                       GetAttributeElementIndex()                     */
/************************************************************************/

int GMLReader::GetAttributeElementIndex( const char *pszElement, int nLen,
                                         const char *pszAttrKey )

{
    GMLFeatureClass *poClass = m_poState->m_poFeature->GetClass();

    // If the schema is not yet locked, then any simple element
    // is potentially an attribute.
    if( !poClass->IsSchemaLocked() )
        return INT_MAX;

    // Otherwise build the path to this element into a single string
    // and compare against known attributes.
    if( m_poState->m_nPathLength == 0 )
    {
        if( pszAttrKey == nullptr )
            return poClass->GetPropertyIndexBySrcElement(pszElement, nLen);
        else
        {
            int nFullLen = nLen + 1 + static_cast<int>(strlen(pszAttrKey));
            osElemPath.reserve(nFullLen);
            osElemPath.assign(pszElement, nLen);
            osElemPath.append(1, '@');
            osElemPath.append(pszAttrKey);
            return poClass->GetPropertyIndexBySrcElement(osElemPath.c_str(), nFullLen);
        }
    }
    else
    {
        int nFullLen = nLen + static_cast<int>(m_poState->osPath.size()) + 1;
        if( pszAttrKey != nullptr )
            nFullLen += 1 + static_cast<int>(strlen(pszAttrKey));
        osElemPath.reserve(nFullLen);
        osElemPath.assign(m_poState->osPath);
        osElemPath.append(1, '|');
        osElemPath.append(pszElement, nLen);
        if( pszAttrKey != nullptr )
        {
            osElemPath.append(1, '@');
            osElemPath.append(pszAttrKey);
        }
        return poClass->GetPropertyIndexBySrcElement(osElemPath.c_str(), nFullLen);
    }
}

/************************************************************************/
/*                              PopState()                              */
/************************************************************************/

void GMLReader::PopState()

{
    if( m_poState != nullptr )
    {
#ifdef HAVE_XERCES
        if( !bUseExpatReader && m_poState->m_poFeature != nullptr &&
            m_poCompleteFeature == nullptr )
        {
            m_poCompleteFeature = m_poState->m_poFeature;
            m_poState->m_poFeature = nullptr;
        }
        else if( !bUseExpatReader && m_poState->m_poFeature != nullptr )
        {
            delete m_poState->m_poFeature;
            m_poState->m_poFeature = nullptr;
        }
#endif

#ifdef HAVE_EXPAT
        if ( bUseExpatReader && m_poState->m_poFeature != nullptr )
        {
            if (nFeatureTabLength >= nFeatureTabAlloc)
            {
                nFeatureTabAlloc = nFeatureTabLength * 4 / 3 + 16;
                ppoFeatureTab = (GMLFeature**)
                        CPLRealloc(ppoFeatureTab,
                                    sizeof(GMLFeature*) * (nFeatureTabAlloc));
            }
            ppoFeatureTab[nFeatureTabLength] = m_poState->m_poFeature;
            nFeatureTabLength++;

            m_poState->m_poFeature = nullptr;
        }
#endif

        GMLReadState *poParent = m_poState->m_poParentState;

        delete m_poRecycledState;
        m_poRecycledState = m_poState;
        m_poRecycledState->Reset();
        m_poState = poParent;
    }
}

/************************************************************************/
/*                             PushState()                              */
/************************************************************************/

void GMLReader::PushState( GMLReadState *poState )

{
    poState->m_poParentState = m_poState;
    m_poState = poState;
}

/************************************************************************/
/*                              GetClass()                              */
/************************************************************************/

GMLFeatureClass *GMLReader::GetClass( int iClass ) const

{
    if( iClass < 0 || iClass >= m_nClassCount )
        return nullptr;

    return m_papoClass[iClass];
}

/************************************************************************/
/*                              GetClass()                              */
/************************************************************************/

GMLFeatureClass *GMLReader::GetClass( const char *pszName ) const

{
    for( int iClass = 0; iClass < m_nClassCount; iClass++ )
    {
        if( EQUAL(GetClass(iClass)->GetName(),pszName) )
            return GetClass(iClass);
    }

    return nullptr;
}

/************************************************************************/
/*                              AddClass()                              */
/************************************************************************/

int GMLReader::AddClass( GMLFeatureClass *poNewClass )

{
    CPLAssert( GetClass( poNewClass->GetName() ) == nullptr );

    m_nClassCount++;
    m_papoClass = static_cast<GMLFeatureClass **>(
        CPLRealloc(m_papoClass, sizeof(void*) * m_nClassCount));
    m_papoClass[m_nClassCount-1] = poNewClass;

    if( poNewClass->HasFeatureProperties() )
        m_bLookForClassAtAnyLevel = true;

    return m_nClassCount - 1;
}

/************************************************************************/
/*                            ClearClasses()                            */
/************************************************************************/

void GMLReader::ClearClasses()

{
    for( int i = 0; i < m_nClassCount; i++ )
        delete m_papoClass[i];
    CPLFree(m_papoClass);

    m_nClassCount = 0;
    m_papoClass = nullptr;
    m_bLookForClassAtAnyLevel = false;
}

/************************************************************************/
/*                     SetFeaturePropertyDirectly()                     */
/*                                                                      */
/*      Set the property value on the current feature, adding the       */
/*      property name to the GMLFeatureClass if required.               */
/*      The pszValue ownership is passed to this function.              */
/************************************************************************/

void GMLReader::SetFeaturePropertyDirectly( const char *pszElement,
                                            char *pszValue,
                                            int iPropertyIn,
                                            GMLPropertyType eType )

{
    GMLFeature *poFeature = GetState()->m_poFeature;

    CPLAssert(poFeature != nullptr);

/* -------------------------------------------------------------------- */
/*      Does this property exist in the feature class?  If not, add     */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    GMLFeatureClass *poClass = poFeature->GetClass();
    int iProperty = 0;

    const int nPropertyCount = poClass->GetPropertyCount();
    if (iPropertyIn >= 0 && iPropertyIn < nPropertyCount)
    {
        iProperty = iPropertyIn;
    }
    else
    {
        for( ; iProperty < nPropertyCount; iProperty++ )
        {
            if( strcmp(poClass->GetProperty( iProperty )->GetSrcElement(),
                    pszElement ) == 0 )
                break;
        }

        if( iProperty == nPropertyCount )
        {
            if( poClass->IsSchemaLocked() )
            {
                CPLDebug("GML",
                         "Encountered property missing from class schema : %s.",
                         pszElement);
                CPLFree(pszValue);
                return;
            }

            CPLString osFieldName;

            if( IsWFSJointLayer() )
            {
                // At that point the element path should be
                // member|layer|property.

                // Strip member| prefix. Should always be true normally.
                if( STARTS_WITH(pszElement, "member|") )
                    osFieldName = pszElement + strlen("member|");

                // Replace layer|property by layer_property.
                size_t iPos = osFieldName.find('|');
                if( iPos != std::string::npos )
                    osFieldName[iPos] = '.';

                // Special case for gml:id on layer.
                iPos = osFieldName.find("@id");
                if( iPos != std::string::npos )
                {
                    osFieldName.resize(iPos);
                    osFieldName += ".gml_id";
                }
            }
            else if( strchr(pszElement,'|') == nullptr )
            {
                osFieldName = pszElement;
            }
            else
            {
                osFieldName = strrchr(pszElement,'|') + 1;
                if( poClass->GetPropertyIndex(osFieldName) != -1 )
                    osFieldName = pszElement;
            }

            size_t nPos = osFieldName.find("@");
            if( nPos != std::string::npos )
                osFieldName[nPos] = '_';

            // Does this conflict with an existing property name?
            for( int i = 0; poClass->GetProperty(osFieldName) != nullptr; i++ )
            {
                osFieldName += "_";
                if( i == 10 )
                {
                    CPLDebug("GML",
                             "Too many conflicting property names : %s.",
                             osFieldName.c_str());
                    CPLFree(pszValue);
                    return;
                }
            }

            GMLPropertyDefn *poPDefn =
                new GMLPropertyDefn(osFieldName, pszElement);

            if( EQUAL(CPLGetConfigOption("GML_FIELDTYPES", ""),
                      "ALWAYS_STRING") )
                poPDefn->SetType(GMLPT_String);
            else if( eType != GMLPT_Untyped )
                poPDefn->SetType( eType );

            if (poClass->AddProperty(poPDefn) < 0)
            {
                delete poPDefn;
                CPLFree(pszValue);
                return;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Set the property                                                */
/* -------------------------------------------------------------------- */
    poFeature->SetPropertyDirectly( iProperty, pszValue );

/* -------------------------------------------------------------------- */
/*      Do we need to update the property type?                         */
/* -------------------------------------------------------------------- */
    if( !poClass->IsSchemaLocked() && !EQUAL(pszValue, OGR_GML_NULL) )
    {
        auto poClassProperty = poClass->GetProperty(iProperty);
        if( poClassProperty )
        {
            poClassProperty->AnalysePropertyValue(
                poFeature->GetProperty(iProperty), m_bSetWidthFlag );
        }
        else
        {
            CPLAssert(false);
        }
    }
}

/************************************************************************/
/*                            LoadClasses()                             */
/************************************************************************/

bool GMLReader::LoadClasses( const char *pszFile )

{
    // Add logic later to determine reasonable default schema file.
    if( pszFile == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Load the raw XML file.                                          */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = VSIFOpenL(pszFile, "rb");

    if( fp == nullptr )
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open file %s.", pszFile);
        return false;
    }

    VSIFSeekL(fp, 0, SEEK_END);
    int nLength = static_cast<int>(VSIFTellL(fp));
    VSIFSeekL(fp, 0, SEEK_SET);

    char *pszWholeText = static_cast<char *>(VSIMalloc(nLength + 1));
    if( pszWholeText == nullptr )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Failed to allocate %d byte buffer for %s,\n"
                 "is this really a GMLFeatureClassList file?",
                 nLength, pszFile);
        VSIFCloseL(fp);
        return false;
    }

    if( VSIFReadL( pszWholeText, nLength, 1, fp ) != 1 )
    {
        VSIFree(pszWholeText);
        VSIFCloseL(fp);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Read failed on %s.", pszFile);
        return false;
    }
    pszWholeText[nLength] = '\0';

    VSIFCloseL(fp);

    if( strstr(pszWholeText, "<GMLFeatureClassList") == nullptr )
    {
        VSIFree(pszWholeText);
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File %s does not contain a GMLFeatureClassList tree.",
                 pszFile);
        return false;
    }

/* -------------------------------------------------------------------- */
/*      Convert to XML parse tree.                                      */
/* -------------------------------------------------------------------- */
    CPLXMLTreeCloser psRoot(CPLParseXMLString(pszWholeText));
    VSIFree(pszWholeText);

    // We assume parser will report errors via CPL.
    if( psRoot.get() == nullptr )
        return false;

    if( psRoot->eType != CXT_Element
        || !EQUAL(psRoot->pszValue, "GMLFeatureClassList") )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File %s is not a GMLFeatureClassList document.",
                 pszFile);
        return false;
    }

    const char* pszSequentialLayers =
        CPLGetXMLValue(psRoot.get(), "SequentialLayers", nullptr);
    if (pszSequentialLayers)
        m_nHasSequentialLayers = CPLTestBool(pszSequentialLayers);

/* -------------------------------------------------------------------- */
/*      Extract feature classes for all definitions found.              */
/* -------------------------------------------------------------------- */
    for( CPLXMLNode *psThis = psRoot->psChild;
         psThis != nullptr;
         psThis = psThis->psNext )
    {
        if( psThis->eType == CXT_Element
            && EQUAL(psThis->pszValue, "GMLFeatureClass") )
        {
            GMLFeatureClass *poClass = new GMLFeatureClass();

            if( !poClass->InitializeFromXML(psThis) )
            {
                delete poClass;
                return false;
            }

            poClass->SetSchemaLocked(true);

            AddClass(poClass);
        }
    }

    SetClassListLocked(true);

    return true;
}

/************************************************************************/
/*                            SaveClasses()                             */
/************************************************************************/

bool GMLReader::SaveClasses( const char *pszFile )

{
    // Add logic later to determine reasonable default schema file.
    if( pszFile == nullptr )
        return false;

/* -------------------------------------------------------------------- */
/*      Create in memory schema tree.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot =
        CPLCreateXMLNode(nullptr, CXT_Element, "GMLFeatureClassList");

    if (m_nHasSequentialLayers != -1 && m_nClassCount > 1)
    {
        CPLCreateXMLElementAndValue(psRoot, "SequentialLayers",
                                    m_nHasSequentialLayers ? "true" : "false");
    }

    for( int iClass = 0; iClass < m_nClassCount; iClass++ )
    {
        CPLAddXMLChild( psRoot, m_papoClass[iClass]->SerializeToXML() );
    }

/* -------------------------------------------------------------------- */
/*      Serialize to disk.                                              */
/* -------------------------------------------------------------------- */
    char *pszWholeText = CPLSerializeXMLTree(psRoot);

    CPLDestroyXMLNode(psRoot);

    VSILFILE *fp = VSIFOpenL(pszFile, "wb");

    bool bSuccess = true;
    if( fp == nullptr )
        bSuccess = false;
    else if( VSIFWriteL(pszWholeText, strlen(pszWholeText), 1, fp) != 1 )
        bSuccess = false;
    else
        VSIFCloseL(fp);

    CPLFree(pszWholeText);

    return bSuccess;
}

/************************************************************************/
/*                          PrescanForSchema()                          */
/*                                                                      */
/*      For now we use a pretty dumb approach of just doing a normal    */
/*      scan of the whole file, building up the schema information.     */
/*      Eventually we hope to do a more efficient scan when just        */
/*      looking for schema information.                                 */
/************************************************************************/

bool GMLReader::PrescanForSchema( bool bGetExtents,
                                  bool bOnlyDetectSRS )

{
    if( m_pszFilename == nullptr )
        return false;

    if( !bOnlyDetectSRS )
    {
        SetClassListLocked(false);
        ClearClasses();
    }

    if( !SetupParser() )
        return false;

    m_bCanUseGlobalSRSName = true;

    GMLFeatureClass *poLastClass = nullptr;

    m_nHasSequentialLayers = TRUE;

    void* hCacheSRS = GML_BuildOGRGeometryFromList_CreateCache();

    std::string osWork;

    for( int i = 0; i < m_nClassCount; i++ )
    {
        m_papoClass[i]->SetFeatureCount(-1);
        m_papoClass[i]->SetSRSName(nullptr);
    }

    GMLFeature *poFeature = nullptr;
    std::set<GMLFeatureClass*> knownClasses;
    bool bFoundPerFeatureSRSName = false;

    while( (poFeature = NextFeature()) != nullptr )
    {
        GMLFeatureClass *poClass = poFeature->GetClass();

        if( knownClasses.find(poClass) == knownClasses.end() )
        {
            knownClasses.insert(poClass);
            if( m_pszGlobalSRSName && GML_IsLegitSRSName(m_pszGlobalSRSName) )
            {
                poClass->SetSRSName(m_pszGlobalSRSName);
            }
        }

        if (poLastClass != nullptr && poClass != poLastClass &&
            poClass->GetFeatureCount() != -1)
            m_nHasSequentialLayers = false;
        poLastClass = poClass;

        if( poClass->GetFeatureCount() == -1 )
            poClass->SetFeatureCount(1);
        else
            poClass->SetFeatureCount(poClass->GetFeatureCount() + 1);

        const CPLXMLNode* const * papsGeometry = poFeature->GetGeometryList();
        if( !bOnlyDetectSRS && papsGeometry != nullptr && papsGeometry[0] != nullptr )
        {
            if( poClass->GetGeometryPropertyCount() == 0 )
            {
                std::string osGeomName(m_osSingleGeomElemPath);
                const auto nPos = osGeomName.rfind('|');
                if( nPos != std::string::npos )
                    osGeomName = osGeomName.substr(nPos + 1);
                poClass->AddGeometryProperty(
                    new GMLGeometryPropertyDefn(osGeomName.c_str(),
                                                m_osSingleGeomElemPath.c_str(),
                                                wkbUnknown, -1, true));
            }
        }

        if( bGetExtents && papsGeometry != nullptr )
        {
            OGRGeometry *poGeometry = GML_BuildOGRGeometryFromList(
                papsGeometry, true, m_bInvertAxisOrderIfLatLong,
                nullptr, m_bConsiderEPSGAsURN,
                m_eSwapCoordinates,
                m_bGetSecondaryGeometryOption,
                hCacheSRS, m_bFaceHoleNegative );

            if( poGeometry != nullptr && poClass->GetGeometryPropertyCount() > 0 )
            {
                OGRwkbGeometryType eGType = static_cast<OGRwkbGeometryType>(
                    poClass->GetGeometryProperty(0)->GetType());

                const char* pszSRSName =
                    GML_ExtractSrsNameFromGeometry(papsGeometry,
                                                    osWork,
                                                    m_bConsiderEPSGAsURN);
                if( pszSRSName != nullptr )
                    bFoundPerFeatureSRSName = true;

                if (pszSRSName != nullptr && m_pszGlobalSRSName != nullptr &&
                    !EQUAL(pszSRSName, m_pszGlobalSRSName))
                {
                    m_bCanUseGlobalSRSName = false;
                }
                if( m_pszGlobalSRSName == nullptr || pszSRSName != nullptr)
                {
                    poClass->MergeSRSName(pszSRSName);
                }

                // Merge geometry type into layer.
                if( poClass->GetFeatureCount() == 1 && eGType == wkbUnknown )
                    eGType = wkbNone;

                poClass->GetGeometryProperty(0)->SetType(
                    static_cast<int>(OGRMergeGeometryTypesEx(
                        eGType, poGeometry->getGeometryType(), true)));

                // Merge extents.
                if (!poGeometry->IsEmpty())
                {
                    double dfXMin = 0.0;
                    double dfXMax = 0.0;
                    double dfYMin = 0.0;
                    double dfYMax = 0.0;

                    OGREnvelope sEnvelope;

                    poGeometry->getEnvelope( &sEnvelope );
                    if( poClass->GetExtents(&dfXMin, &dfXMax,
                                            &dfYMin, &dfYMax) )
                    {
                        dfXMin = std::min(dfXMin, sEnvelope.MinX);
                        dfXMax = std::max(dfXMax, sEnvelope.MaxX);
                        dfYMin = std::min(dfYMin, sEnvelope.MinY);
                        dfYMax = std::max(dfYMax, sEnvelope.MaxY);
                    }
                    else
                    {
                        dfXMin = sEnvelope.MinX;
                        dfXMax = sEnvelope.MaxX;
                        dfYMin = sEnvelope.MinY;
                        dfYMax = sEnvelope.MaxY;
                    }

                    poClass->SetExtents(dfXMin, dfXMax, dfYMin, dfYMax);
                }
                delete poGeometry;
            }
        }

        delete poFeature;
    }

    GML_BuildOGRGeometryFromList_DestroyCache(hCacheSRS);

    if( bGetExtents && m_bCanUseGlobalSRSName && m_pszGlobalSRSName &&
        !bFoundPerFeatureSRSName && m_bInvertAxisOrderIfLatLong &&
        GML_IsLegitSRSName(m_pszGlobalSRSName) &&
        GML_IsSRSLatLongOrder(m_pszGlobalSRSName) )
    {
        /* So when we have computed the extent, we didn't know yet */
        /* the SRS to use. Now we know it, we have to fix the extent */
        /* order */

        for( int i = 0; i < m_nClassCount; i++ )
        {
            GMLFeatureClass *poClass = m_papoClass[i];
            if( poClass->HasExtents() )
            {
                double dfXMin = 0.0;
                double dfXMax = 0.0;
                double dfYMin = 0.0;
                double dfYMax = 0.0;
                if( poClass->GetExtents(&dfXMin, &dfXMax, &dfYMin, &dfYMax) )
                    poClass->SetExtents(dfYMin, dfYMax, dfXMin, dfXMax);
            }
        }
    }

    CleanupParser();

    return true;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void GMLReader::ResetReading()

{
    CleanupParser();
    SetFilteredClassName(nullptr);
}

/************************************************************************/
/*                          SetGlobalSRSName()                          */
/************************************************************************/

void GMLReader::SetGlobalSRSName( const char* pszGlobalSRSName )
{
    if (m_pszGlobalSRSName == nullptr && pszGlobalSRSName != nullptr)
    {
        const char* pszVertCS_EPSG = nullptr;
        if( STARTS_WITH(pszGlobalSRSName, "EPSG:") &&
            (pszVertCS_EPSG = strstr(pszGlobalSRSName, ", EPSG:")) != nullptr )
        {
            m_pszGlobalSRSName = CPLStrdup(CPLSPrintf("EPSG:%d+%d",
                    atoi(pszGlobalSRSName + 5),
                    atoi(pszVertCS_EPSG + 7)));
        }
        else if (STARTS_WITH(pszGlobalSRSName, "EPSG:") &&
                 m_bConsiderEPSGAsURN)
        {
            m_pszGlobalSRSName = CPLStrdup(
                CPLSPrintf("urn:ogc:def:crs:EPSG::%s", pszGlobalSRSName + 5));
        }
        else
        {
            m_pszGlobalSRSName = CPLStrdup(pszGlobalSRSName);
        }
    }
}

/************************************************************************/
/*                       SetFilteredClassName()                         */
/************************************************************************/

bool GMLReader::SetFilteredClassName(const char* pszClassName)
{
    CPLFree(m_pszFilteredClassName);
    m_pszFilteredClassName = pszClassName ? CPLStrdup(pszClassName) : nullptr;

    m_nFilteredClassIndex = -1;
    if( m_pszFilteredClassName != nullptr )
    {
        for( int i = 0; i < m_nClassCount; i++ )
        {
            if( strcmp(m_papoClass[i]->GetElementName(),
                       m_pszFilteredClassName) == 0 )
            {
                m_nFilteredClassIndex = i;
                break;
            }
        }
    }

    return true;
}
