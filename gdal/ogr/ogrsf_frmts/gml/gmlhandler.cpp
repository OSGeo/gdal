/**********************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Implementation of GMLHandler class.
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
#include "gmlreaderp.h"
#include "cpl_conv.h"

/* Must be a multiple of 4 */
#define MAX_TOKEN_SIZE  1000

/* Maximum size in bytes of the geometry. Must be big enough */
/* as some real-world GML files have huge geometries (see #2818) */
#define MAX_GML_GEOMETRY_SIZE (10*1000*1000)

/************************************************************************/
/*                             GMLHandler()                             */
/************************************************************************/

GMLHandler::GMLHandler( GMLReader *poReader )

{
    m_poReader = poReader;
    m_pszCurField = NULL;
    m_pszGeometry = NULL;
    m_nGeomAlloc = m_nGeomLen = 0;
    m_nEntityCounter = 0;
}

/************************************************************************/
/*                            ~GMLHandler()                             */
/************************************************************************/

GMLHandler::~GMLHandler()

{
    CPLFree( m_pszCurField );
    CPLFree( m_pszGeometry );
}


/************************************************************************/
/*                            startElement()                            */
/************************************************************************/

void GMLHandler::startElement(const XMLCh* const    uri,
                              const XMLCh* const    localname,
                              const XMLCh* const    qname,
                              const Attributes& attrs )

{
    char        szElementName[MAX_TOKEN_SIZE];
    GMLReadState *poState = m_poReader->GetState();

    m_nEntityCounter = 0;

    /* A XMLCh character can expand to 4 bytes in UTF-8 */
    if (4 * tr_strlen( localname ) >= MAX_TOKEN_SIZE)
    {
        static int bWarnOnce = FALSE;
        XMLCh* tempBuffer = (XMLCh*) CPLMalloc(sizeof(XMLCh) * (MAX_TOKEN_SIZE / 4 + 1));
        memcpy(tempBuffer, localname, sizeof(XMLCh) * (MAX_TOKEN_SIZE / 4));
        tempBuffer[MAX_TOKEN_SIZE / 4] = 0;
        tr_strcpy( szElementName, tempBuffer );
        CPLFree(tempBuffer);
        if (!bWarnOnce)
        {
            bWarnOnce = TRUE;
            CPLError(CE_Warning, CPLE_AppDefined, "A too big element name has been truncated");
        }
    }
    else
        tr_strcpy( szElementName, localname );

    int nLNLenBytes = strlen(szElementName);

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
        /* should save attributes too! */

        if( m_pszGeometry == NULL )
            m_nGeometryDepth = poState->m_nPathLength;
        
        if( m_nGeomLen + nLNLenBytes + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nLNLenBytes + 1000);
            m_pszGeometry = (char *) 
                VSIRealloc( m_pszGeometry, m_nGeomAlloc);
            if (m_pszGeometry == NULL)
            {
                throw SAXNotSupportedException("Out of memory");
            }
        }

        strcpy( m_pszGeometry+m_nGeomLen, "<" );
        strcpy( m_pszGeometry+m_nGeomLen+1, szElementName );
        strcat( m_pszGeometry+m_nGeomLen+nLNLenBytes+1, ">" );
        m_nGeomLen += nLNLenBytes + 2;
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
    }

/* -------------------------------------------------------------------- */
/*      Push the element onto the current state's path.                 */
/* -------------------------------------------------------------------- */
    poState->PushPath( szElementName );
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
void GMLHandler::endElement(const   XMLCh* const    uri,
                            const   XMLCh* const    localname,
                            const   XMLCh* const    qname )

{
    char        szElementName[MAX_TOKEN_SIZE];
    GMLReadState *poState = m_poReader->GetState();

    m_nEntityCounter = 0;

    /* A XMLCh character can expand to 4 bytes in UTF-8 */
    if (4 * tr_strlen( localname ) >= MAX_TOKEN_SIZE)
    {
        XMLCh* tempBuffer = (XMLCh*) CPLMalloc(sizeof(XMLCh) * (MAX_TOKEN_SIZE / 4 + 1));
        memcpy(tempBuffer, localname, sizeof(XMLCh) * (MAX_TOKEN_SIZE / 4));
        tempBuffer[MAX_TOKEN_SIZE / 4] = 0;
        tr_strcpy( szElementName, tempBuffer );
        CPLFree(tempBuffer);
    }
    else
        tr_strcpy( szElementName, localname );

    int nLNLenBytes = strlen(szElementName);

/* -------------------------------------------------------------------- */
/*      Is this closing off an attribute value?  We assume so if        */
/*      we are collecting an attribute value and got to this point.     */
/*      We don't bother validating that the closing tag matches the     */
/*      opening tag.                                                    */
/* -------------------------------------------------------------------- */
    if( m_pszCurField != NULL )
    {
        CPLAssert( poState->m_poFeature != NULL );
        
        m_poReader->SetFeatureProperty( szElementName, m_pszCurField );
        CPLFree( m_pszCurField );
        m_pszCurField = NULL;
    }

/* -------------------------------------------------------------------- */
/*      If we are collecting Geometry than store it, and consider if    */
/*      this is the end of the geometry.                                */
/* -------------------------------------------------------------------- */
    if( m_pszGeometry != NULL )
    {
        /* should save attributes too! */

        if( m_nGeomLen + nLNLenBytes + 4 > m_nGeomAlloc )
        {
            m_nGeomAlloc = (int) (m_nGeomAlloc * 1.3 + nLNLenBytes + 1000);
            m_pszGeometry = (char *) 
                VSIRealloc( m_pszGeometry, m_nGeomAlloc);
            if (m_pszGeometry == NULL)
            {
                throw SAXNotSupportedException("Out of memory");
            }
        }

        strcat( m_pszGeometry+m_nGeomLen, "</" );
        strcpy( m_pszGeometry+m_nGeomLen+2, szElementName );
        strcat( m_pszGeometry+m_nGeomLen+nLNLenBytes+2, ">" );
        m_nGeomLen += nLNLenBytes + 3;

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
        && strcmp(szElementName,
                 poState->m_poFeature->GetClass()->GetElementName()) == 0 )
    {
        m_poReader->PopState();
    }

/* -------------------------------------------------------------------- */
/*      Otherwise, we just pop the element off the local read states    */
/*      element stack.                                                  */
/* -------------------------------------------------------------------- */
    else
    {
        if( strcmp(szElementName,poState->GetLastComponent()) == 0 )
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

void GMLHandler::characters(const XMLCh* const chars_in,
                                const unsigned int length )

{
    const XMLCh *chars = chars_in;

    if( m_pszCurField != NULL )
    {
        int     nCurFieldLength = strlen(m_pszCurField);
        if (nCurFieldLength > MAX_GML_GEOMETRY_SIZE)
        {
            throw SAXNotSupportedException("Too much data inside one element. File probably corrupted");
        }

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
                VSIRealloc( m_pszCurField, 
                            nCurFieldLength+strlen(pszTranslated)+1 );
            if (m_pszCurField == NULL)
            {
                CPLFree( pszTranslated );
                throw SAXNotSupportedException("Out of memory");
            }
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
                VSIRealloc( m_pszGeometry, m_nGeomAlloc);
            if (m_pszGeometry == NULL)
            {
                throw SAXNotSupportedException("Out of memory");
            }
        }

        tr_strcpy( m_pszGeometry+m_nGeomLen, chars );
        m_nGeomLen += strlen(m_pszGeometry+m_nGeomLen);

        if (m_nGeomLen > MAX_GML_GEOMETRY_SIZE)
        {
            throw SAXNotSupportedException("Too much data inside one element. File probably corrupted");
        }
    }
}

/************************************************************************/
/*                             fatalError()                             */
/************************************************************************/

void GMLHandler::fatalError( const SAXParseException &exception)

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

int GMLHandler::IsGeometryElement( const char *pszElement )

{
    return strcmp(pszElement,"Polygon") == 0
        || strcmp(pszElement,"MultiPolygon") == 0 
        || strcmp(pszElement,"MultiPoint") == 0 
        || strcmp(pszElement,"MultiLineString") == 0 
        || strcmp(pszElement,"GeometryCollection") == 0
        || strcmp(pszElement,"Point") == 0 
        || strcmp(pszElement,"LineString") == 0;
}

/************************************************************************/
/*                             startEntity()                            */
/************************************************************************/

void GMLHandler::startEntity (const XMLCh *const name)
{
    m_nEntityCounter ++;
    if (m_nEntityCounter > 1000 && !m_poReader->HasStoppedParsing())
    {
        throw SAXNotSupportedException("File probably corrupted (million laugh pattern)");
    }
}
