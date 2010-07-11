/******************************************************************************
 * $Id: gmlreader.cpp 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  NAS Reader
 * Purpose:  Implementation of NASReader class.
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
/* ==================================================================== */
/*                  With XERCES Library                                 */
/* ==================================================================== */
/************************************************************************/

#include "nasreaderp.h"
#include "cpl_conv.h"

/************************************************************************/
/*                          CreateGMLReader()                           */
/************************************************************************/

IGMLReader *CreateNASReader()

{
    return new NASReader();
}

/************************************************************************/
/*                             GMLReader()                              */
/************************************************************************/

NASReader::NASReader()

{
    m_nClassCount = 0;
    m_papoClass = NULL;

    m_bClassListLocked = FALSE;

    m_poNASHandler = NULL;
    m_poSAXReader = NULL;
    m_bReadStarted = FALSE;
    
    m_poState = NULL;
    m_poCompleteFeature = NULL;

    m_pszFilename = NULL;
}

/************************************************************************/
/*                             ~NASReader()                             */
/************************************************************************/

NASReader::~NASReader()

{
    ClearClasses();

    CPLFree( m_pszFilename );

    CleanupParser();
}

/************************************************************************/
/*                          SetSourceFile()                             */
/************************************************************************/

void NASReader::SetSourceFile( const char *pszFilename )

{
    CPLFree( m_pszFilename );
    m_pszFilename = CPLStrdup( pszFilename );
}

/************************************************************************/
/*                       GetSourceFileName()                            */
/************************************************************************/

const char* NASReader::GetSourceFileName()

{
    return m_pszFilename;
}

/************************************************************************/
/*                            SetupParser()                             */
/************************************************************************/

int NASReader::SetupParser()

{
    static int bXercesInitialized = FALSE;

    if( !bXercesInitialized )
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
        bXercesInitialized = TRUE;
    }

    // Cleanup any old parser.
    if( m_poSAXReader != NULL )
        CleanupParser();

    // Create and initialize parser.
    XMLCh* xmlUriValid = NULL;
    XMLCh* xmlUriNS = NULL;

    try{
        m_poSAXReader = XMLReaderFactory::createXMLReader();
    
        m_poNASHandler = new NASHandler( this );

        m_poSAXReader->setContentHandler( m_poNASHandler );
        m_poSAXReader->setErrorHandler( m_poNASHandler );
        m_poSAXReader->setLexicalHandler( m_poNASHandler );
        m_poSAXReader->setEntityResolver( m_poNASHandler );
        m_poSAXReader->setDTDHandler( m_poNASHandler );

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

    m_bReadStarted = FALSE;

    // Push an empty state.
    PushState( new GMLReadState() );

    return TRUE;
}

/************************************************************************/
/*                           CleanupParser()                            */
/************************************************************************/

void NASReader::CleanupParser()

{
    if( m_poSAXReader == NULL )
        return;

    while( m_poState )
        PopState();

    delete m_poSAXReader;
    m_poSAXReader = NULL;

    delete m_poNASHandler;
    m_poNASHandler = NULL;

    m_bReadStarted = FALSE;
}

/************************************************************************/
/*                            NextFeature()                             */
/************************************************************************/

GMLFeature *NASReader::NextFeature()

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
               && m_poSAXReader->parseNext( m_oToFill ) ) {}

        poReturn = m_poCompleteFeature;
        m_poCompleteFeature = NULL;

    }
    catch (const XMLException& toCatch)
    {
        CPLDebug( "GML", 
                  "Error during NextFeature()! Message:\n%s", 
                  tr_strdup( toCatch.getMessage() ) );
    }

    return poReturn;
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

void NASReader::PushFeature( const char *pszElement, 
                             const Attributes &attrs )

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
/*      Create a feature of this feature class.                         */
/* -------------------------------------------------------------------- */
    GMLFeature *poFeature = new GMLFeature( GetClass( iClass ) );

/* -------------------------------------------------------------------- */
/*      Create and push a new read state.                               */
/* -------------------------------------------------------------------- */
    GMLReadState *poState;

    poState = new GMLReadState();
    poState->m_poFeature = poFeature;
    PushState( poState );

/* -------------------------------------------------------------------- */
/*      Check for gml:id, and if found push it as an attribute named    */
/*      gml_id.                                                         */
/* -------------------------------------------------------------------- */
    int nFIDIndex;
    XMLCh   anFID[100];

    tr_strcpy( anFID, "gml:id" );
    nFIDIndex = attrs.getIndex( anFID );
    if( nFIDIndex != -1 )
    {
        char *pszFID = tr_strdup( attrs.getValue( nFIDIndex ) );
        SetFeatureProperty( "gml_id", pszFID );
        CPLFree( pszFID );
    }

}

/************************************************************************/
/*                          IsFeatureElement()                          */
/*                                                                      */
/*      Based on context and the element name, is this element a new    */
/*      GML feature element?                                            */
/************************************************************************/

int NASReader::IsFeatureElement( const char *pszElement )

{
    CPLAssert( m_poState != NULL );

    const char *pszLast = m_poState->GetLastComponent();
    int        nLen = strlen(pszLast);

    // There seem to be two major NAS classes of feature identifiers 
    // -- either a wfs:Insert or a gml:featureMember. 

    if( (nLen < 6 || !EQUAL(pszLast+nLen-6,"Insert")) 
        && (nLen < 13 || !EQUAL(pszLast+nLen-13,"featureMember")) )
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

int NASReader::IsAttributeElement( const char *pszElement )

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

void NASReader::PopState()

{
    if( m_poState != NULL )
    {
        if( m_poState->m_poFeature != NULL && m_poCompleteFeature == NULL )
        {
            m_poCompleteFeature = m_poState->m_poFeature;
            m_poState->m_poFeature = NULL;
        }

        GMLReadState *poParent;

        poParent = m_poState->m_poParentState;
        
        delete m_poState;
        m_poState = poParent;
    }
}

/************************************************************************/
/*                             PushState()                              */
/************************************************************************/

void NASReader::PushState( GMLReadState *poState )

{
    poState->m_poParentState = m_poState;
    m_poState = poState;
}

/************************************************************************/
/*                              GetClass()                              */
/************************************************************************/

GMLFeatureClass *NASReader::GetClass( int iClass ) const

{
    if( iClass < 0 || iClass >= m_nClassCount )
        return NULL;
    else
        return m_papoClass[iClass];
}

/************************************************************************/
/*                              GetClass()                              */
/************************************************************************/

GMLFeatureClass *NASReader::GetClass( const char *pszName ) const

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

int NASReader::AddClass( GMLFeatureClass *poNewClass )

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

void NASReader::ClearClasses()

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

void NASReader::SetFeatureProperty( const char *pszElement, 
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
        AnalysePropertyValue(poClass->GetProperty(iProperty),
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

int NASReader::LoadClasses( const char *pszFile )

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

int NASReader::SaveClasses( const char *pszFile )

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

int NASReader::PrescanForSchema( int bGetExtents )

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
                poGeometry = (OGRGeometry *) OGR_G_CreateFromGML3( 
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
                poGeometry->getEnvelope( &sEnvelope );
                delete poGeometry;
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
            else 
            {
                if( poClass->GetGeometryType() == (int) wkbUnknown 
                    && poClass->GetFeatureCount() == 1 )
                    poClass->SetGeometryType( wkbNone );
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

void NASReader::ResetReading()

{
    CleanupParser();
}

/************************************************************************/
/*                        AnalysePropertyValue()                        */
/*                                                                      */
/*      Examine the passed property value, and see if we need to        */
/*      make the field type more specific, or more general.             */
/************************************************************************/

void NASReader::AnalysePropertyValue( GMLPropertyDefn *poDefn,
                                      const char *pszValue,
                                      const char *pszOldValue )

{
/* -------------------------------------------------------------------- */
/*      If it is a zero length string, just return.  We can't deduce    */
/*      much from this.                                                 */
/* -------------------------------------------------------------------- */
    if( *pszValue == '\0' )
        return;

/* -------------------------------------------------------------------- */
/*      Does the string consist entirely of numeric values?             */
/* -------------------------------------------------------------------- */
    int bIsReal = FALSE;
    GMLPropertyType eType = poDefn->GetType();

    CPLValueType valueType = CPLGetValueType(pszValue);
    if (valueType == CPL_VALUE_STRING
        && eType != GMLPT_String 
        && eType != GMLPT_StringList )
    {
        if( eType == GMLPT_IntegerList
            || eType == GMLPT_RealList )
            eType = GMLPT_StringList;
        else
            eType = GMLPT_String;
    }
    else
        bIsReal = (valueType == CPL_VALUE_REAL);

    if( eType == GMLPT_String )
    {
        /* grow the Width to the length of the string passed in */
        int nWidth;
        nWidth = strlen(pszValue);
        if ( poDefn->GetWidth() < nWidth ) 
            poDefn->SetWidth( nWidth );
    }
    else if( eType == GMLPT_Untyped || eType == GMLPT_Integer )
    {
        if( bIsReal )
            eType = GMLPT_Real;
        else
            eType = GMLPT_Integer;
    }
    else if( eType == GMLPT_IntegerList && bIsReal )
    {
        eType = GMLPT_RealList;
    }

/* -------------------------------------------------------------------- */
/*      If we already had a value then we are dealing with a list       */
/*      type value.                                                     */
/* -------------------------------------------------------------------- */
    if( pszOldValue != NULL && strlen(pszOldValue) > 0 )
    {
        if( eType == GMLPT_Integer )
            eType = GMLPT_IntegerList;
        if( eType == GMLPT_Real )
            eType = GMLPT_RealList;
        if( eType == GMLPT_String )
        {
            eType = GMLPT_StringList;
            poDefn->SetWidth( 0 );
        }
    }

    poDefn->SetType( eType );
}

/************************************************************************/
/*                         CheckForRelations()                          */
/************************************************************************/

void NASReader::CheckForRelations( const char *pszElement, 
                                   const Attributes &attrs )

{
    GMLFeature *poFeature = GetState()->m_poFeature;

    CPLAssert( poFeature  != NULL );

    int nIndex;
    XMLCh  Name[100];

    tr_strcpy( Name, "xlink:href" );
    nIndex = attrs.getIndex( Name );

    if( nIndex != -1 )
    {
        char *pszHRef = tr_strdup( attrs.getValue( nIndex ) );

        if( EQUALN(pszHRef,"urn:adv:oid:", 12 ) )
            poFeature->AddOBProperty( pszElement, pszHRef );

        CPLFree( pszHRef );
    }
}

/************************************************************************/
/*                           ResolveXlinks()                            */
/*      Returns TRUE for success                                        */
/************************************************************************/

int NASReader::ResolveXlinks( const char *pszFile,
                              int* pbOutIsTempFile,
                              char **papszSkip,
                              const int bStrict )

{
    CPLDebug( "NAS", "ResolveXlinks() not currently implemented for NAS." );
    return FALSE;
}

