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
 **********************************************************************
 *
 * $Log$
 * Revision 1.1  2002/01/04 19:46:30  warmerda
 * New
 *
 *
 **********************************************************************/

#include "gmlreaderp.h"
#include "cpl_conv.h"

/************************************************************************/
/*                             GMLHandler()                             */
/************************************************************************/

GMLHandler::GMLHandler( GMLReader *poReader )

{
    m_poReader = poReader;
    m_pszCurField = NULL;
}

/************************************************************************/
/*                            ~GMLHandler()                             */
/************************************************************************/

GMLHandler::~GMLHandler()

{
    CPLFree( m_pszCurField );
}


/************************************************************************/
/*                            startElement()                            */
/************************************************************************/

void GMLHandler::startElement(const XMLCh* const    uri,
                              const XMLCh* const    localname,
                              const XMLCh* const    qname,
                              const Attributes& attrs )

{
    TrString    oElementName( qname );
    GMLReadState *poState = m_poReader->GetState();

    if( m_poReader->IsFeatureElement( oElementName ) )
        m_poReader->PushFeature( oElementName, attrs );
    else
        poState->PushPath( oElementName );
}

/************************************************************************/
/*                             endElement()                             */
/************************************************************************/
void GMLHandler::endElement(const   XMLCh* const    uri,
                            const   XMLCh* const    localname,
                            const   XMLCh* const    qname )

{
    TrString    oElementName( qname );
    GMLReadState *poState = m_poReader->GetState();
    
    if( poState->m_poFeature != NULL
        && EQUAL(oElementName,
                 poState->m_poFeature->GetClass()->GetElementName()) )
    {
        m_poReader->PopState();
    }
    else
    {
        if( EQUAL(oElementName,poState->GetLastComponent()) )
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

void GMLHandler::characters(const XMLCh* const chars,
                                const unsigned int length )

{
}

/************************************************************************/
/*                             fatalError()                             */
/************************************************************************/

void GMLHandler::fatalError( const SAXParseException &exception)

{
}

