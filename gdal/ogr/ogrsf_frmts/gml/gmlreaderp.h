/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Private Declarations for OGR free GML Reader code.
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

#ifndef _CPL_GMLREADERP_H_INCLUDED
#define _CPL_GMLREADERP_H_INCLUDED

#include "gmlreader.h"

// This works around problems with math.h on some platforms #defining INFINITY
#ifdef INFINITY
#undef  INFINITY
#define INFINITY INFINITY_XERCES
#endif

#include <util/PlatformUtils.hpp>
#include <sax2/DefaultHandler.hpp>
#include <sax2/ContentHandler.hpp>
#include <sax2/SAX2XMLReader.hpp>
#include <sax2/XMLReaderFactory.hpp>
#include <sax2/Attributes.hpp>

#ifdef XERCES_CPP_NAMESPACE_USE
XERCES_CPP_NAMESPACE_USE
#endif

/************************************************************************/
/*          XMLCh / char translation functions - trstring.cpp           */
/************************************************************************/
int tr_strcmp( const char *, const XMLCh * );
void tr_strcpy( XMLCh *, const char * );
void tr_strcpy( char *, const XMLCh * );
char *tr_strdup( const XMLCh * );
int tr_strlen( const XMLCh * );


class GMLReader;

/************************************************************************/
/*                            GMLHandler                                */
/************************************************************************/
class GMLHandler : public DefaultHandler 
{
    GMLReader  *m_poReader;

    char       *m_pszCurField;

    char       *m_pszGeometry;
    int        m_nGeomAlloc;
    int        m_nGeomLen;

    int        m_nGeometryDepth;
    int        IsGeometryElement( const char * );

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

    int         MatchPath( const char *pszPathInput );
    const char  *GetPath() const { return m_pszPath; }
    const char  *GetLastComponent() const;

    GMLFeature  *m_poFeature;
    GMLReadState *m_poParentState;

    char        *m_pszPath; // element path ... | as separator.

    int         m_nPathLength;
    char        **m_papszPathComponents;
};

/************************************************************************/
/*                              GMLReader                               */
/************************************************************************/

class GMLReader : public IGMLReader 
{
private:
    int           m_bClassListLocked;

    int         m_nClassCount;
    GMLFeatureClass **m_papoClass;

    char          *m_pszFilename;

    GMLHandler    *m_poGMLHandler;
    SAX2XMLReader *m_poSAXReader;
    int           m_bReadStarted;
    XMLPScanToken m_oToFill;

    GMLReadState *m_poState;

    GMLFeature   *m_poCompleteFeature;

    int           SetupParser();
    void          CleanupParser();

public:
                GMLReader();
    virtual     ~GMLReader();

    int              IsClassListLocked() const { return m_bClassListLocked; }
    void             SetClassListLocked( int bFlag )
        { m_bClassListLocked = bFlag; }

    void             SetSourceFile( const char *pszFilename );

    int              GetClassCount() const { return m_nClassCount; }
    GMLFeatureClass *GetClass( int i ) const;
    GMLFeatureClass *GetClass( const char *pszName ) const;

    int              AddClass( GMLFeatureClass *poClass );
    void             ClearClasses();

    GMLFeature       *NextFeature();

    int              LoadClasses( const char *pszFile = NULL );
    int              SaveClasses( const char *pszFile = NULL );

    int              ParseXSD( const char *pszFile );

    int              PrescanForSchema(int bGetExtents = TRUE );
    void             ResetReading();

// --- 

    GMLReadState     *GetState() const { return m_poState; }
    void             PopState();
    void             PushState( GMLReadState * );

    int         IsFeatureElement( const char *pszElement );
    int         IsAttributeElement( const char *pszElement );

    void        PushFeature( const char *pszElement, 
                             const Attributes &attrs );

    void        SetFeatureProperty( const char *pszElement,
                                    const char *pszValue );

};

#endif /* _CPL_GMLREADERP_H_INCLUDED */
