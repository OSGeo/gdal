/**********************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Private Declarations for OGR free GML Reader code.
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

#ifndef _CPL_GMLREADERP_H_INCLUDED
#define _CPL_GMLREADERP_H_INCLUDED

#include "gmlreader.h"

#include <dom/DOMString.hpp>
#include <util/PlatformUtils.hpp>
#include <sax2/DefaultHandler.hpp>
#include <sax2/ContentHandler.hpp>
#include <sax2/SAX2XMLReader.hpp>
#include <sax2/XMLReaderFactory.hpp>

/************************************************************************/
/*                             MyXMLString                              */
/************************************************************************/

class TrString
{
private:
    char	*pszCString;
    DOMString   oXMLString;
    
public:
    TrString( const char *pszIn );
    TrString( const XMLCh *pachInput );
    ~TrString();

    const char *GetCString() const { return pszCString; }
    const XMLCh *GetXMLString() const { return oXMLString.rawBuffer(); }
    operator const char *(void) const { return pszCString; }
    operator const XMLCh *(void) const { return oXMLString.rawBuffer(); }
};


class GMLReader;

/************************************************************************/
/*                            GMLHandler                                */
/************************************************************************/
class GMLHandler : public DefaultHandler 
{
    GMLReader  *m_poReader;

    char       *m_pszCurField;

public:
    GMLHandler( GMLReader *poReader );
    virtual ~GMLHandler();
    
    void startElement(
        const   XMLCh* const    uri,
        const   XMLCh* const    localname,
        const   XMLCh* const    qname,
        const   Attributes& attrs
    );
    void endElement(
        const   XMLCh* const    uri,
        const   XMLCh* const    localname,
        const   XMLCh* const    qname
    );
    void characters( const XMLCh *const chars,
                     const unsigned int length );

    void fatalError(const SAXParseException&);
};

/************************************************************************/
/*                             GMLReadState                             */
/************************************************************************/

class GMLReadState
{
    void        RebuildPath();

public:
    GMLReadState();
    ~GMLReadState();

    void        PushPath( const char *pszElement );
    void        PopPath();

    const char  *GetPath() const { return m_pszPath; }
    const char  *GetLastComponent() const;

    GMLFeature	*m_poFeature;
    GMLReadState *m_poParentState;

    char	*m_pszPath; // element path ... | as separator.

    int         m_nPathLength;
    char        **m_papszPathComponents;
};

/************************************************************************/
/*                              GMLReader                               */
/************************************************************************/

class GMLReader : public IGMLReader 
{
private:
    int		  m_bClassListLocked;

    int		m_nClassCount;
    GMLFeatureClass **m_papoClass;

    char          *m_pszFilename;

    GMLHandler    *m_poGMLHandler;
    SAX2XMLReader *m_poSAXReader;
    int		  m_bReadStarted;
    XMLPScanToken m_oToFill;

    GMLReadState *m_poState;

    GMLFeature   *m_poCompleteFeature;

    int		  SetupParser();
    void          CleanupParser();

public:
    		GMLReader();
    virtual 	~GMLReader();

    int              IsClassListLocked() const { return m_bClassListLocked; }
    void             SetClassListLocked( int bFlag )
        { m_bClassListLocked = bFlag; }

    void 	     SetSourceFile( const char *pszFilename );

    int	 	     GetClassCount() const { return m_nClassCount; }
    GMLFeatureClass *GetClass( int i ) const;
    GMLFeatureClass *GetClass( const char *pszName ) const;

    int              AddClass( GMLFeatureClass *poClass );

    GMLFeature       *NextFeature();

    GMLReadState     *GetState() const { return m_poState; }
    void             PopState();
    void             PushState( GMLReadState * );

    int         IsFeatureElement( const char *pszElement );
    int         IsAttributeElement( const char *pszElement );

    void	PushFeature( const char *pszElement, 
                             const Attributes &attrs );

};

#endif /* _CPL_GMLREADERP_H_INCLUDED */
