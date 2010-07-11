/**********************************************************************
 * $Id: gmlhandler.cpp 13760 2008-02-11 17:48:30Z warmerdam $
 *
 * Project:  GML Reader
 * Purpose:  Implementation of NASHandler class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
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

#include <ctype.h>
#include "nasreaderp.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#define MAX_TOKEN_SIZE  1000

/************************************************************************/
/*                             NASHandler()                             */
/************************************************************************/

NASHandler::NASHandler( NASReader *poReader )

{
    m_poReader = poReader;
    m_pszCurField = NULL;
    m_pszGeometry = NULL;
    m_nGeomAlloc = m_nGeomLen = 0;
}

/************************************************************************/
/*                            ~NASHandler()                             */
/************************************************************************/

NASHandler::~NASHandler()

{
    CPLFree( m_pszCurField );
    CPLFree( m_pszGeometry );
}


/************************************************************************/
/*                            startElement()                            */
/************************************************************************/

void NASHandler::startElement(const XMLCh* const    uri,
                              const XMLCh* const    localname,
                              const XMLCh* const    qname,
                              const Attributes& attrs )

{
    char        szElementName[MAX_TOKEN_SIZE];
    GMLReadState *poState = m_poReader->GetState();

    tr_strcpy( szElementName, localname );

/* -------------------------------------------------------------------- */
/*      If we are in the midst of collecting a feature attribute        */
/*      value, then this must be a complex attribute which we don't     */
/*      try to collect for now, so just terminate the field             */
/*      collection.                                                     */
/* -------------------------------------------------------------------- */
    if( m_pszCurField != NULL )
    {
        CPLFree( m_pszCurField );
        m_pszCurField = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting geometry, or if we determine this is a     */
/*      geometry element then append to the geometry info.              */
/* -------------------------------------------------------------------- */
    if( m_pszGeometry != NULL 
        || IsGeometryElement( szElementName ) )
    {
        int nLNLen = tr_strlen( localname );

        /* should save attributes too! */

        if( m_pszGeometry == NULL )
            m_nGeometryDepth = poState->m_nPathLength;
        
        if( m_nGeomLen + nLNLen + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nLNLen + 1000);
            m_pszGeometry = (char *) 
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        strcpy( m_pszGeometry+m_nGeomLen, "<" );
        tr_strcpy( m_pszGeometry+m_nGeomLen+1, localname );
        strcat( m_pszGeometry+m_nGeomLen+nLNLen+1, ">" );
        m_nGeomLen += strlen(m_pszGeometry+m_nGeomLen);
    }
    
/* -------------------------------------------------------------------- */
/*      Is it a feature?  If so push a whole new state, and return.     */
/* -------------------------------------------------------------------- */
    else if( m_poReader->IsFeatureElement( szElementName ) )
    {
        m_poReader->PushFeature( szElementName, attrs );
        return;
    }

/* -------------------------------------------------------------------- */
/*      If it is (or at least potentially is) a simple attribute,       */
/*      then start collecting it.                                       */
/* -------------------------------------------------------------------- */
    else if( m_poReader->IsAttributeElement( szElementName ) )
    {
        CPLFree( m_pszCurField );
        m_pszCurField = CPLStrdup("");

        // Capture href as OB property.
        m_poReader->CheckForRelations( szElementName, attrs );
    }

/* -------------------------------------------------------------------- */
/*      Push the element onto the current state's path.                 */
/* -------------------------------------------------------------------- */
    poState->PushPath( szElementName );
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
void NASHandler::endElement(const   XMLCh* const    uri,
                            const   XMLCh* const    localname,
                            const   XMLCh* const    qname )

{
    char        szElementName[MAX_TOKEN_SIZE];
    GMLReadState *poState = m_poReader->GetState();

    tr_strcpy( szElementName, localname );

/* -------------------------------------------------------------------- */
/*      Is this closing off an attribute value?  We assume so if        */
/*      we are collecting an attribute value and got to this point.     */
/*      We don't bother validating that the closing tag matches the     */
/*      opening tag.                                                    */
/* -------------------------------------------------------------------- */
    if( m_pszCurField != NULL )
    {
        CPLAssert( poState->m_poFeature != NULL );
        
        m_poReader->SetFeatureProperty( poState->m_pszPath, m_pszCurField );
        CPLFree( m_pszCurField );
        m_pszCurField = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting Geometry than store it, and consider if    */
/*      this is the end of the geometry.                                */
/* -------------------------------------------------------------------- */
    if( m_pszGeometry != NULL )
    {
        int nLNLen = tr_strlen( localname );

        /* should save attributes too! */

        if( m_nGeomLen + nLNLen + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nLNLen + 1000);
            m_pszGeometry = (char *) 
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        strcat( m_pszGeometry+m_nGeomLen, "</" );
        tr_strcpy( m_pszGeometry+m_nGeomLen+2, localname );
        strcat( m_pszGeometry+m_nGeomLen+nLNLen+2, ">" );
        m_nGeomLen += strlen(m_pszGeometry+m_nGeomLen);

        if( poState->m_nPathLength == m_nGeometryDepth+1 )
        {
            if( poState->m_poFeature != NULL )
                poState->m_poFeature->SetGeometryDirectly( m_pszGeometry );
            else
                CPLFree( m_pszGeometry );

            m_pszGeometry = NULL;
            m_nGeomAlloc = m_nGeomLen = 0;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting a feature, and this element tag matches    */
/*      element name for the class, then we have finished the           */
/*      feature, and we pop the feature read state.                     */
/* -------------------------------------------------------------------- */
    if( poState->m_poFeature != NULL
        && EQUAL(szElementName,
                 poState->m_poFeature->GetClass()->GetElementName()) )
    {
        m_poReader->PopState();
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, we just pop the element off the local read states    */
/*      element stack.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        if( EQUAL(szElementName,poState->GetLastComponent()) )
            poState->PopPath();
        else
        {
            CPLAssert( FALSE );
        }
    }
}

/************************************************************************/
/*                             characters()                             */
/************************************************************************/

#if XERCES_VERSION_MAJOR >= 3
void NASHandler::characters( const XMLCh *const chars_in,
                             const XMLSize_t length )
#else
void NASHandler::characters(const XMLCh* const chars_in,
                            const unsigned int length )
#endif

{
    const XMLCh *chars = chars_in;

    if( m_pszCurField != NULL )
    {
        int     nCurFieldLength = strlen(m_pszCurField);

        while( *chars == ' ' || *chars == 10 || *chars == 13 || *chars == '\t')
            chars++;

        char *pszTranslated = tr_strdup(chars);
        
        if( m_pszCurField == NULL )
        {
            m_pszCurField = pszTranslated;
            nCurFieldLength = strlen(m_pszCurField);
        }
        else
        {
            m_pszCurField = (char *) 
                CPLRealloc( m_pszCurField, 
                            nCurFieldLength+strlen(pszTranslated)+1 );
            strcpy( m_pszCurField + nCurFieldLength, pszTranslated );
            CPLFree( pszTranslated );
        }
    }
    else if( m_pszGeometry != NULL )
    {
        // Ignore white space
        while( *chars == ' ' || *chars == 10 || *chars == 13 || *chars == '\t')
            chars++;
        
        int nCharsLen = tr_strlen(chars);

        if( m_nGeomLen + nCharsLen*4 + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nCharsLen*4 + 1000);
            m_pszGeometry = (char *) 
                CPLRealloc( m_pszGeometry, m_nGeomAlloc);
        }

        tr_strcpy( m_pszGeometry+m_nGeomLen, chars );
        m_nGeomLen += strlen(m_pszGeometry+m_nGeomLen);
    }
}

/************************************************************************/
/*                             fatalError()                             */
/************************************************************************/

void NASHandler::fatalError( const SAXParseException &exception)

{
    char *pszErrorMessage;

    pszErrorMessage = tr_strdup( exception.getMessage() );
    CPLError( CE_Failure, CPLE_AppDefined, 
              "XML Parsing Error: %s\n", 
              pszErrorMessage );

    CPLFree( pszErrorMessage );
}

/************************************************************************/
/*                         IsGeometryElement()                          */
/************************************************************************/

int NASHandler::IsGeometryElement( const char *pszElement )

{
    return strcmp(pszElement,"Polygon") == 0
        || strcmp(pszElement,"MultiPolygon") == 0 
        || strcmp(pszElement,"MultiPoint") == 0 
        || strcmp(pszElement,"MultiLineString") == 0 
        || strcmp(pszElement,"MultiSurface") == 0 
        || strcmp(pszElement,"GeometryCollection") == 0
        || strcmp(pszElement,"Point") == 0 
        || strcmp(pszElement,"Curve") == 0 
        || strcmp(pszElement,"Surface") == 0 
        || strcmp(pszElement,"PolygonPatch") == 0 
        || strcmp(pszElement,"LineString") == 0;
}
