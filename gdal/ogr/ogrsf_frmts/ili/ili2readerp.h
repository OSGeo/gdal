/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 2 Reader
 * Purpose:  Private Declarations for Reader code.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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

#ifndef _CPL_ILI2READERP_H_INCLUDED
#define _CPL_ILI2READERP_H_INCLUDED

// This works around problems with math.h on some platforms #defining INFINITY
#ifdef INFINITY
#undef  INFINITY
#define INFINITY INFINITY_XERCES
#endif

#include "ili2reader.h"
#include "ogr_ili2.h"

#include <string>
#include <set>

#include <util/PlatformUtils.hpp>
#include <sax2/DefaultHandler.hpp>
#include <sax2/ContentHandler.hpp>
#include <sax2/SAX2XMLReader.hpp>
#include <sax2/XMLReaderFactory.hpp>
#include <dom/DOM.hpp>
#include <util/XMLString.hpp>

#if _XERCES_VERSION >= 30000
# include <sax2/Attributes.hpp>
#endif

#ifdef XERCES_CPP_NAMESPACE_USE
XERCES_CPP_NAMESPACE_USE
#endif

int cmpStr(std::string s1, std::string s2);

std::string ltrim(std::string tmpstr);
std::string rtrim(std::string tmpstr);
std::string trim(std::string tmpstr);


class ILI2Reader;

/************************************************************************/
/*                            ILI2Handler                                */
/************************************************************************/
class ILI2Handler : public DefaultHandler 
{
    ILI2Reader  *m_poReader;

    int level;
    
    DOMDocument *dom_doc;
    DOMElement *dom_elem;
    
    int m_nEntityCounter;

public:
    ILI2Handler( ILI2Reader *poReader );
    ~ILI2Handler();
    
    void startDocument();
    void endDocument();
    
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
#if XERCES_VERSION_MAJOR >= 3
    void characters( const XMLCh *const chars,
                     const XMLSize_t length ); // xerces 3
#else
    void characters( const XMLCh *const chars,
                     const unsigned int length ); // xerces 2
#endif

    void startEntity (const XMLCh *const name);

    void fatalError(const SAXParseException&);
};


/************************************************************************/
/*                              ILI2Reader                               */
/************************************************************************/

class ILI2Reader : public IILI2Reader
{    
private:
    int      SetupParser();
    void     CleanupParser();
    
    char    *m_pszFilename;
    
    std::list<std::string> m_missAttrs;

    ILI2Handler *m_poILI2Handler;
    SAX2XMLReader *m_poSAXReader;
    int      m_bReadStarted;
    double   arcIncr;
    
    std::list<OGRLayer *> m_listLayer;

public:
             ILI2Reader();
            ~ILI2Reader();

    void     SetArcDegrees(double arcDegrees);
    void     SetSourceFile( const char *pszFilename );
    int      ReadModel( ImdReader *poImdReader, char *modelFilename );
    int      SaveClasses( const char *pszFile );
    
    std::list<OGRLayer *> GetLayers();
    int      GetLayerCount();
    
    int      AddFeature(DOMElement *elem);
    void     SetFieldValues(OGRFeature *feature, DOMElement* elem);
    const char* GetLayerName(/*IOM_BASKET model, IOM_OBJECT table*/);
    void     AddField(OGRLayer* layer/*, IOM_BASKET model, IOM_OBJECT obj*/);
    OGRLineString *getArc(DOMElement *elem);
    OGRGeometry *getGeometry(DOMElement *elem, int type);
    void     setFieldDefn(OGRFeatureDefn *featureDef, DOMElement* elem);
};

#endif
