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

#ifndef CPL_ILI2READERP_H_INCLUDED
#define CPL_ILI2READERP_H_INCLUDED

#include "xercesc_headers.h"
#include "ogr_xerces.h"

#include "ili2reader.h"
#include "ogr_ili2.h"

#include <string>
#include <set>

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
    explicit ILI2Handler( ILI2Reader *poReader );
    ~ILI2Handler();

    void startDocument() override;
    void endDocument() override;

    void startElement(
        const   XMLCh* const    uri,
        const   XMLCh* const    localname,
        const   XMLCh* const    qname,
        const   Attributes& attrs
    ) override;
    void endElement(
        const   XMLCh* const    uri,
        const   XMLCh* const    localname,
        const   XMLCh* const    qname
    ) override;
    void characters( const XMLCh *const chars,
                     const XMLSize_t length ) override; // xerces 3

    void startEntity (const XMLCh *const name) override;

    void fatalError(const SAXParseException&) override;
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

    std::list<OGRLayer *> m_listLayer;

    bool     m_bXercesInitialized;

public:
             ILI2Reader();
            ~ILI2Reader();

    void     SetSourceFile( const char *pszFilename ) override;
    int      ReadModel( ImdReader *poImdReader, const char *modelFilename ) override;
    int      SaveClasses( const char *pszFile ) override;

    std::list<OGRLayer *> GetLayers() override;
    int      GetLayerCount() override;
    OGRLayer* GetLayer(const char* pszName);

    int      AddFeature(DOMElement *elem);
    void     SetFieldValues(OGRFeature *feature, DOMElement* elem);
    const char* GetLayerName(/*IOM_BASKET model, IOM_OBJECT table*/);
    void     AddField(OGRLayer* layer/*, IOM_BASKET model, IOM_OBJECT obj*/);
    OGRCircularString *getArc(DOMElement *elem);
    OGRGeometry *getGeometry(DOMElement *elem, int type);
    void     setFieldDefn(OGRFeatureDefn *featureDef, DOMElement* elem);
};

#endif
