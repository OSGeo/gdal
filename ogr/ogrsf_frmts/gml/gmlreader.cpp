/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLReader class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
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

#include "gmlreader.h"
#include "cpl_error.h"
#include "cpl_string.h"

#define SUPPORT_GEOMETRY

#ifdef SUPPORT_GEOMETRY
#  include "ogr_geometry.h"
#endif

/************************************************************************/
/*                            ~IGMLReader()                             */
/************************************************************************/

IGMLReader::~IGMLReader()

{
}

/************************************************************************/
/* ==================================================================== */
/*                  No XERCES or EXPAT Library                          */
/* ==================================================================== */
/************************************************************************/
#if HAVE_XERCES == 0 && !defined(HAVE_EXPAT)

/************************************************************************/
/*                          CreateGMLReader()                           */
/************************************************************************/

IGMLReader *CreateGMLReader()

{
    CPLError( CE_Failure, CPLE_AppDefined,
              "Unable to create Xerces C++ or Expat based GML reader, Xerces or Expat support\n"
              "not configured into GDAL/OGR." );
    return NULL;
}

/************************************************************************/
/* ==================================================================== */
/*                  With XERCES or EXPAT Library                        */
/* ==================================================================== */
/************************************************************************/
#else /* HAVE_XERCES == 1 or HAVE_EXPAT */

#include "gmlreaderp.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          CreateGMLReader()                           */
/************************************************************************/

IGMLReader *CreateGMLReader()

{
    return new GMLReader();
}

int GMLReader::m_bXercesInitialized = FALSE;
int GMLReader::m_nInstanceCount = 0;

/************************************************************************/
/*                             GMLReader()                              */
/************************************************************************/

GMLReader::GMLReader()

{
    m_nInstanceCount++;
    m_nClassCount = 0;
    m_papoClass = NULL;

    m_bClassListLocked = FALSE;

    m_poGMLHandler = NULL;
#if HAVE_XERCES == 1
    m_poSAXReader = NULL;
    m_poCompleteFeature = NULL;
#else
    oParser = NULL;
    ppoFeatureTab = NULL;
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    fpGML = NULL;
#endif
    m_bReadStarted = FALSE;
    
    m_poState = NULL;

    m_pszFilename = NULL;

    m_bStopParsing = FALSE;
}

/************************************************************************/
/*                             ~GMLReader()                             */
/************************************************************************/

GMLReader::~GMLReader()

{
    ClearClasses();

    CPLFree( m_pszFilename );

    CleanupParser();

    --m_nInstanceCount;
#if HAVE_XERCES == 1
    if( m_nInstanceCount == 0 && m_bXercesInitialized )
    {
        XMLPlatformUtils::Terminate();
        m_bXercesInitialized = FALSE;
    }
#endif

#ifdef HAVE_EXPAT
    if (fpGML)
        VSIFCloseL(fpGML);
    fpGML = NULL;
#endif
}

/************************************************************************/
/*                             SetSource()                              */
/************************************************************************/

void GMLReader::SetSourceFile( const char *pszFilename )

{
    CPLFree( m_pszFilename );
    m_pszFilename = CPLStrdup( pszFilename );
}

#ifdef HAVE_EXPAT

static void XMLCALL startElementCbk(void *pUserData, const char *pszName,
                                    const char **ppszAttr)
{
    ((GMLHandler*)pUserData)->startElement(pszName, ppszAttr);
}

static void XMLCALL endElementCbk(void *pUserData, const char *pszName)
{
    ((GMLHandler*)pUserData)->endElement(pszName);
}

static void XMLCALL dataHandlerCbk(void *pUserData, const char *data, int nLen)
{
    ((GMLHandler*)pUserData)->dataHandler(data, nLen);
}

#endif

/************************************************************************/
/*                            SetupParser()                             */
/************************************************************************/

int GMLReader::SetupParser()

{
#if HAVE_XERCES == 1

    if( !m_bXercesInitialized )
    {
        try
        {
            XMLPlatformUtils::Initialize();
        }
        
        catch (const XMLException& toCatch)
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Exception initializing Xerces based GML reader.\n%s", 
                      tr_strdup(toCatch.getMessage()) );
            return FALSE;
        }
        m_bXercesInitialized = TRUE;
    }

    // Cleanup any old parser.
    if( m_poSAXReader != NULL )
        CleanupParser();

    // Create and initialize parser.
    XMLCh* xmlUriValid = NULL;
    XMLCh* xmlUriNS = NULL;

    try{
        m_poSAXReader = XMLReaderFactory::createXMLReader();
    
        m_poGMLHandler = new GMLXercesHandler( this );

        m_poSAXReader->setContentHandler( m_poGMLHandler );
        m_poSAXReader->setErrorHandler( m_poGMLHandler );
        m_poSAXReader->setLexicalHandler( m_poGMLHandler );
        m_poSAXReader->setEntityResolver( m_poGMLHandler );
        m_poSAXReader->setDTDHandler( m_poGMLHandler );

        xmlUriValid = XMLString::transcode("http://xml.org/sax/features/validation");
        xmlUriNS = XMLString::transcode("http://xml.org/sax/features/namespaces");

#if (OGR_GML_VALIDATION)
        m_poSAXReader->setFeature( xmlUriValid, true);
        m_poSAXReader->setFeature( xmlUriNS, true);

        m_poSAXReader->setFeature( XMLUni::fgSAX2CoreNameSpaces, true );
        m_poSAXReader->setFeature( XMLUni::fgXercesSchema, true );

//    m_poSAXReader->setDoSchema(true);
//    m_poSAXReader->setValidationSchemaFullChecking(true);
#else
        m_poSAXReader->setFeature( XMLUni::fgSAX2CoreValidation, false);

#if XERCES_VERSION_MAJOR >= 3
        m_poSAXReader->setFeature( XMLUni::fgXercesSchema, false);
#else
        m_poSAXReader->setFeature( XMLUni::fgSAX2CoreNameSpaces, false);
#endif

#endif
        XMLString::release( &xmlUriValid );
        XMLString::release( &xmlUriNS );
    }
    catch (...)
    {
        XMLString::release( &xmlUriValid );
        XMLString::release( &xmlUriNS );

        CPLError( CE_Warning, CPLE_AppDefined,
                  "Exception initializing Xerces based GML reader.\n" );
        return FALSE;
    }
#else
    // Cleanup any old parser.
    if( oParser != NULL )
        CleanupParser();

    oParser = OGRCreateExpatXMLParser();
    m_poGMLHandler = new GMLExpatHandler( this, oParser );

    XML_SetElementHandler(oParser, ::startElementCbk, ::endElementCbk);
    XML_SetCharacterDataHandler(oParser, ::dataHandlerCbk);
    XML_SetUserData(oParser, m_poGMLHandler);

    if (fpGML != NULL)
        VSIFSeekL( fpGML, 0, SEEK_SET );
#endif

    m_bReadStarted = FALSE;

    // Push an empty state.
    PushState( new GMLReadState() );

    return TRUE;
}

/************************************************************************/
/*                           CleanupParser()                            */
/************************************************************************/

void GMLReader::CleanupParser()

{
#if HAVE_XERCES == 1
    if( m_poSAXReader == NULL )
        return;
#else
    if (oParser == NULL )
        return;
#endif

    while( m_poState )
        PopState();

#if HAVE_XERCES == 1
    delete m_poSAXReader;
    m_poSAXReader = NULL;
#else
    if (oParser)
        XML_ParserFree(oParser);
    oParser = NULL;

    int i;
    for(i=nFeatureTabIndex;i<nFeatureTabLength;i++)
        delete ppoFeatureTab[i];
    CPLFree(ppoFeatureTab);
    nFeatureTabIndex = 0;
    nFeatureTabLength = 0;
    ppoFeatureTab = NULL;

#endif

    delete m_poGMLHandler;
    m_poGMLHandler = NULL;

    m_bReadStarted = FALSE;
}

/************************************************************************/
/*                            NextFeature()                             */
/************************************************************************/

#if HAVE_XERCES == 1
GMLFeature *GMLReader::NextFeature()

{
    GMLFeature *poReturn = NULL;

    try
    {
        if( !m_bReadStarted )
        {
            if( m_poSAXReader == NULL )
                SetupParser();

            if( !m_poSAXReader->parseFirst( m_pszFilename, m_oToFill ) )
                return NULL;
            m_bReadStarted = TRUE;
        }

        while( m_poCompleteFeature == NULL 
               && !m_bStopParsing
               && m_poSAXReader->parseNext( m_oToFill ) ) {}

        poReturn = m_poCompleteFeature;
        m_poCompleteFeature = NULL;

    }
    catch (const XMLException& toCatch)
    {
        char *pszErrorMessage = tr_strdup( toCatch.getMessage() );
        CPLDebug( "GML", 
                  "Error during NextFeature()! Message:\n%s", 
                  pszErrorMessage );
        CPLFree(pszErrorMessage);
        m_bStopParsing = TRUE;
    }
    catch (const SAXException& toCatch)
    {
        char *pszErrorMessage = tr_strdup( toCatch.getMessage() );
        CPLError(CE_Failure, CPLE_AppDefined, "%s", pszErrorMessage);
        CPLFree(pszErrorMessage);
        m_bStopParsing = TRUE;
    }

    return poReturn;
}
#else
GMLFeature *GMLReader::NextFeature()

{
    if (!m_bReadStarted)
    {
        if (oParser == NULL)
            SetupParser();

        if (fpGML == NULL)
            fpGML = VSIFOpenL(m_pszFilename, "rt");

        m_bReadStarted = TRUE;
    }

    if (fpGML == NULL || m_bStopParsing)
        return NULL;

    if (nFeatureTabIndex < nFeatureTabLength)
    {
        return ppoFeatureTab[nFeatureTabIndex++];
    }

    if (VSIFEofL(fpGML))
        return NULL;

    char aBuf[BUFSIZ];

    CPLFree(ppoFeatureTab);
    ppoFeatureTab = NULL;
    nFeatureTabLength = 0;
    nFeatureTabIndex = 0;

    int nDone;
    do
    {
        m_poGMLHandler->ResetDataHandlerCounter();

        unsigned int nLen =
                (unsigned int)VSIFReadL( aBuf, 1, sizeof(aBuf), fpGML );
        nDone = VSIFEofL(fpGML);
        if (XML_Parse(oParser, aBuf, nLen, nDone) == XML_STATUS_ERROR)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "XML parsing of GML file failed : %s "
                     "at line %d, column %d",
                     XML_ErrorString(XML_GetErrorCode(oParser)),
                     (int)XML_GetCurrentLineNumber(oParser),
                     (int)XML_GetCurrentColumnNumber(oParser));
            m_bStopParsing = TRUE;
        }
        if (!m_bStopParsing)
            m_bStopParsing = m_poGMLHandler->HasStoppedParsing();

    } while (!nDone && !m_bStopParsing && nFeatureTabLength == 0);

    return (nFeatureTabLength) ? ppoFeatureTab[nFeatureTabIndex++] : NULL;
}
#endif

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
                             const char *pszFID )

{
    int iClass;

/* -------------------------------------------------------------------- */
/*      Find the class of this element.                                 */
/* -------------------------------------------------------------------- */
    for( iClass = 0; iClass < GetClassCount(); iClass++ )
    {
        if( EQUAL(pszElement,GetClass(iClass)->GetElementName()) )
            break;
    }

/* -------------------------------------------------------------------- */
/*      Create a new feature class for this element, if there is no     */
/*      existing class for it.                                          */
/* -------------------------------------------------------------------- */
    if( iClass == GetClassCount() )
    {
        CPLAssert( !IsClassListLocked() );

        GMLFeatureClass *poNewClass = new GMLFeatureClass( pszElement );

        AddClass( poNewClass );
    }

/* -------------------------------------------------------------------- */
/*      Create a feature of this feature class.  Try to set the fid     */
/*      if available.                                                   */
/* -------------------------------------------------------------------- */
    GMLFeature *poFeature = new GMLFeature( GetClass( iClass ) );
    if( pszFID != NULL )
    {
        poFeature->SetFID( pszFID );
    }

/* -------------------------------------------------------------------- */
/*      Create and push a new read state.                               */
/* -------------------------------------------------------------------- */
    GMLReadState *poState;

    poState = new GMLReadState();
    poState->m_poFeature = poFeature;
    PushState( poState );
}

/************************************************************************/
/*                          IsFeatureElement()                          */
/*                                                                      */
/*      Based on context and the element name, is this element a new    */
/*      GML feature element?                                            */
/************************************************************************/

int GMLReader::IsFeatureElement( const char *pszElement )

{
    CPLAssert( m_poState != NULL );

    const char *pszLast = m_poState->GetLastComponent();
    int        nLen = strlen(pszLast);

    if (strcmp(pszLast, "dane") == 0)
    {
         /* Polish TBD GML */
    }
    else if( nLen < 6 || !(EQUAL(pszLast+nLen-6,"member") ||
                      EQUAL(pszLast+nLen-7,"members")) )
        return FALSE;

    // If the class list isn't locked, any element that is a featureMember
    // will do. 
    if( !IsClassListLocked() )
        return TRUE;

    // otherwise, find a class with the desired element name.
    for( int i = 0; i < GetClassCount(); i++ )
    {
        if( EQUAL(pszElement,GetClass(i)->GetElementName()) )
            return TRUE;
    }

    return FALSE;
}

/************************************************************************/
/*                         IsAttributeElement()                         */
/************************************************************************/

int GMLReader::IsAttributeElement( const char *pszElement )

{
    if( m_poState->m_poFeature == NULL )
        return FALSE;

    GMLFeatureClass *poClass = m_poState->m_poFeature->GetClass();

    // If the schema is not yet locked, then any simple element
    // is potentially an attribute.
    if( !poClass->IsSchemaLocked() )
        return TRUE;

    // Otherwise build the path to this element into a single string
    // and compare against known attributes.
    CPLString osElemPath;

    if( m_poState->m_nPathLength == 0 )
        osElemPath = pszElement;
    else
    {
        osElemPath = m_poState->m_pszPath;
        osElemPath += "|";
        osElemPath += pszElement;
    }

    for( int i = 0; i < poClass->GetPropertyCount(); i++ )
        if( EQUAL(poClass->GetProperty(i)->GetSrcElement(),osElemPath) )
            return TRUE;

    return FALSE;
}

/************************************************************************/
/*                              PopState()                              */
/************************************************************************/

void GMLReader::PopState()

{
    if( m_poState != NULL )
    {
#if HAVE_XERCES == 1
        if( m_poState->m_poFeature != NULL && m_poCompleteFeature == NULL )
        {
            m_poCompleteFeature = m_poState->m_poFeature;
            m_poState->m_poFeature = NULL;
        }
#else
        if ( m_poState->m_poFeature != NULL )
        {
            ppoFeatureTab = (GMLFeature**)
                    CPLRealloc(ppoFeatureTab,
                                sizeof(GMLFeature*) * (nFeatureTabLength + 1));
            ppoFeatureTab[nFeatureTabLength] = m_poState->m_poFeature;
            nFeatureTabLength++;

            m_poState->m_poFeature = NULL;
        }
#endif

        GMLReadState *poParent;

        poParent = m_poState->m_poParentState;
        
        delete m_poState;
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
        return NULL;
    else
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

    return NULL;
}

/************************************************************************/
/*                              AddClass()                              */
/************************************************************************/

int GMLReader::AddClass( GMLFeatureClass *poNewClass )

{
    CPLAssert( GetClass( poNewClass->GetName() ) == NULL );

    m_nClassCount++;
    m_papoClass = (GMLFeatureClass **) 
        CPLRealloc( m_papoClass, sizeof(void*) * m_nClassCount );
    m_papoClass[m_nClassCount-1] = poNewClass;

    return m_nClassCount-1;
}

/************************************************************************/
/*                            ClearClasses()                            */
/************************************************************************/

void GMLReader::ClearClasses()

{
    for( int i = 0; i < m_nClassCount; i++ )
        delete m_papoClass[i];
    CPLFree( m_papoClass );

    m_nClassCount = 0;
    m_papoClass = NULL;
}

/************************************************************************/
/*                         SetFeatureProperty()                         */
/*                                                                      */
/*      Set the property value on the current feature, adding the       */
/*      property name to the GMLFeatureClass if required.               */
/*      Eventually this function may also "refine" the property         */
/*      type based on what is encountered.                              */
/************************************************************************/

void GMLReader::SetFeatureProperty( const char *pszElement, 
                                    const char *pszValue )

{
    GMLFeature *poFeature = GetState()->m_poFeature;

    CPLAssert( poFeature  != NULL );

/* -------------------------------------------------------------------- */
/*      Does this property exist in the feature class?  If not, add     */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    GMLFeatureClass *poClass = poFeature->GetClass();
    int      iProperty;

    for( iProperty=0; iProperty < poClass->GetPropertyCount(); iProperty++ )
    {
        if( EQUAL(poClass->GetProperty( iProperty )->GetSrcElement(),
                  pszElement ) )
            break;
    }
    
    if( iProperty == poClass->GetPropertyCount() )
    {
        if( poClass->IsSchemaLocked() )
        {
            CPLDebug("GML","Encountered property missing from class schema.");
            return;
        }

        CPLString osFieldName;
        
        if( strchr(pszElement,'|') == NULL )
            osFieldName = pszElement;
        else
        {
            osFieldName = strrchr(pszElement,'|') + 1;
            if( poClass->GetPropertyIndex(osFieldName) != -1 )
                osFieldName = pszElement;
        }

        // Does this conflict with an existing property name? 
        while( poClass->GetProperty(osFieldName) != NULL )
        {
            osFieldName += "_";
        }

        GMLPropertyDefn *poPDefn = new GMLPropertyDefn(osFieldName,pszElement);

        if( EQUAL(CPLGetConfigOption( "GML_FIELDTYPES", ""), "ALWAYS_STRING") )
            poPDefn->SetType( GMLPT_String );

        poClass->AddProperty( poPDefn );
    }

/* -------------------------------------------------------------------- */
/*      Do we need to update the property type?                         */
/* -------------------------------------------------------------------- */
    if( !poClass->IsSchemaLocked() )
    {
        poClass->GetProperty(iProperty)->AnalysePropertyValue(
                             pszValue, 
                             poFeature->GetProperty(iProperty));
    }

/* -------------------------------------------------------------------- */
/*      Set the property                                                */
/* -------------------------------------------------------------------- */
    switch( poClass->GetProperty( iProperty )->GetType() )
    {
      case GMLPT_StringList:
      case GMLPT_IntegerList:
      case GMLPT_RealList:
      {
          if( poFeature->GetProperty( iProperty ) != NULL )
          {
              CPLString osJoinedValue = poFeature->GetProperty( iProperty );
              osJoinedValue += ",";
              osJoinedValue += pszValue;

              poFeature->SetProperty( iProperty, osJoinedValue );
          }
          else
              poFeature->SetProperty( iProperty, pszValue );
      }
      break;

      default:
        poFeature->SetProperty( iProperty, pszValue );
        break;
    }

}

/************************************************************************/
/*                            LoadClasses()                             */
/************************************************************************/

int GMLReader::LoadClasses( const char *pszFile )

{
    // Add logic later to determine reasonable default schema file. 
    if( pszFile == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Load the raw XML file.                                          */
/* -------------------------------------------------------------------- */
    FILE       *fp;
    int         nLength;
    char        *pszWholeText;

    fp = VSIFOpen( pszFile, "rb" );

    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open file %s.", pszFile );
        return FALSE;
    }

    VSIFSeek( fp, 0, SEEK_END );
    nLength = VSIFTell( fp );
    VSIFSeek( fp, 0, SEEK_SET );

    pszWholeText = (char *) VSIMalloc(nLength+1);
    if( pszWholeText == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to allocate %d byte buffer for %s,\n"
                  "is this really a GMLFeatureClassList file?",
                  nLength, pszFile );
        VSIFClose( fp );
        return FALSE;
    }
    
    if( VSIFRead( pszWholeText, nLength, 1, fp ) != 1 )
    {
        VSIFree( pszWholeText );
        VSIFClose( fp );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Read failed on %s.", pszFile );
        return FALSE;
    }
    pszWholeText[nLength] = '\0';

    VSIFClose( fp );

    if( strstr( pszWholeText, "<GMLFeatureClassList>" ) == NULL )
    {
        VSIFree( pszWholeText );
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "File %s does not contain a GMLFeatureClassList tree.",
                  pszFile );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Convert to XML parse tree.                                      */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot;

    psRoot = CPLParseXMLString( pszWholeText );
    VSIFree( pszWholeText );

    // We assume parser will report errors via CPL.
    if( psRoot == NULL )
        return FALSE;

    if( psRoot->eType != CXT_Element 
        || !EQUAL(psRoot->pszValue,"GMLFeatureClassList") )
    {
        CPLDestroyXMLNode(psRoot);
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "File %s is not a GMLFeatureClassList document.",
                  pszFile );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Extract feature classes for all definitions found.              */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psThis;

    for( psThis = psRoot->psChild; psThis != NULL; psThis = psThis->psNext )
    {
        if( psThis->eType == CXT_Element 
            && EQUAL(psThis->pszValue,"GMLFeatureClass") )
        {
            GMLFeatureClass   *poClass;

            poClass = new GMLFeatureClass();

            if( !poClass->InitializeFromXML( psThis ) )
            {
                delete poClass;
                CPLDestroyXMLNode( psRoot );
                return FALSE;
            }

            poClass->SetSchemaLocked( TRUE );

            AddClass( poClass );
        }
    }

    CPLDestroyXMLNode( psRoot );
    
    SetClassListLocked( TRUE );

    return TRUE;
}

/************************************************************************/
/*                            SaveClasses()                             */
/************************************************************************/

int GMLReader::SaveClasses( const char *pszFile )

{
    // Add logic later to determine reasonable default schema file. 
    if( pszFile == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Create in memory schema tree.                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot;

    psRoot = CPLCreateXMLNode( NULL, CXT_Element, "GMLFeatureClassList" );

    for( int iClass = 0; iClass < GetClassCount(); iClass++ )
    {
        GMLFeatureClass *poClass = GetClass( iClass );
        
        CPLAddXMLChild( psRoot, poClass->SerializeToXML() );
    }

/* -------------------------------------------------------------------- */
/*      Serialize to disk.                                              */
/* -------------------------------------------------------------------- */
    FILE        *fp;
    int         bSuccess = TRUE;
    char        *pszWholeText = CPLSerializeXMLTree( psRoot );
    
    CPLDestroyXMLNode( psRoot );
 
    fp = VSIFOpen( pszFile, "wb" );
    
    if( fp == NULL )
        bSuccess = FALSE;
    else if( VSIFWrite( pszWholeText, strlen(pszWholeText), 1, fp ) != 1 )
        bSuccess = FALSE;
    else
        VSIFClose( fp );

    CPLFree( pszWholeText );

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

int GMLReader::PrescanForSchema( int bGetExtents )

{
    GMLFeature  *poFeature;

    if( m_pszFilename == NULL )
        return FALSE;

    SetClassListLocked( FALSE );

    ClearClasses();
    if( !SetupParser() )
        return FALSE;

    while( (poFeature = NextFeature()) != NULL )
    {
        GMLFeatureClass *poClass = poFeature->GetClass();

        if( poClass->GetFeatureCount() == -1 )
            poClass->SetFeatureCount( 1 );
        else
            poClass->SetFeatureCount( poClass->GetFeatureCount() + 1 );

#ifdef SUPPORT_GEOMETRY
        if( bGetExtents )
        {
            OGRGeometry *poGeometry = NULL;

            if( poFeature->GetGeometry() != NULL 
                && strlen(poFeature->GetGeometry()) != 0 )
            {
                poGeometry = OGRGeometryFactory::createFromGML( 
                    poFeature->GetGeometry() );
            }

            if( poGeometry != NULL )
            {
                double  dfXMin, dfXMax, dfYMin, dfYMax;
                OGREnvelope sEnvelope;
                OGRwkbGeometryType eGType = (OGRwkbGeometryType) 
                    poClass->GetGeometryType();

                // Merge geometry type into layer.
                if( poClass->GetFeatureCount() == 1 && eGType == wkbUnknown )
                    eGType = wkbNone;

                poClass->SetGeometryType( 
                    (int) OGRMergeGeometryTypes(
                        eGType, poGeometry->getGeometryType() ) );

                // merge extents.
                if (!poGeometry->IsEmpty())
                {
                    poGeometry->getEnvelope( &sEnvelope );
                    if( poClass->GetExtents(&dfXMin, &dfXMax, &dfYMin, &dfYMax) )
                    {
                        dfXMin = MIN(dfXMin,sEnvelope.MinX);
                        dfXMax = MAX(dfXMax,sEnvelope.MaxX);
                        dfYMin = MIN(dfYMin,sEnvelope.MinY);
                        dfYMax = MAX(dfYMax,sEnvelope.MaxY);
                    }
                    else
                    {
                        dfXMin = sEnvelope.MinX;
                        dfXMax = sEnvelope.MaxX;
                        dfYMin = sEnvelope.MinY;
                        dfYMax = sEnvelope.MaxY;
                    }

                    poClass->SetExtents( dfXMin, dfXMax, dfYMin, dfYMax );
                }
                delete poGeometry;

            }
#endif /* def SUPPORT_GEOMETRY */
        }
        
        delete poFeature;
    }

    CleanupParser();

    return GetClassCount() > 0;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void GMLReader::ResetReading()

{
    CleanupParser();
}

#endif /* HAVE_XERCES == 1 or HAVE_EXPAT */

