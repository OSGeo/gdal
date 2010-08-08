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
#include "ogr_api.h"

class GMLReader;

/************************************************************************/
/*                              GMLHandler                              */
/************************************************************************/
class GMLHandler
{
    char       *m_pszCurField;

    char       *m_pszGeometry;
    size_t     m_nGeomAlloc;
    size_t     m_nGeomLen;

    int        m_nGeometryDepth;

    int        m_nDepth;
    int        m_nDepthFeature;

    int        m_bInBoundedBy;
    int        m_inBoundedByDepth;

    int        m_bInCityGMLGenericAttr;
    char      *m_pszCityGMLGenericAttrName;
    int        m_inCityGMLGenericAttrDepth;
protected:
    GMLReader  *m_poReader;

public:
    GMLHandler( GMLReader *poReader );
    virtual ~GMLHandler();

    virtual OGRErr      startElement(const char *pszName, void* attr);
    virtual OGRErr      endElement(const char *pszName);
    virtual OGRErr      dataHandler(const char *data, int nLen);
    virtual char*       GetFID(void* attr) = 0;
    virtual char*       GetAttributes(void* attr) = 0;
    virtual char*       GetAttributeValue(void* attr, const char* pszAttributeName) = 0;

    int         IsGeometryElement( const char *pszElement );
};


#if HAVE_XERCES == 1

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
#include <sax/InputSource.hpp>
#include <util/BinInputStream.hpp>

#ifdef XERCES_CPP_NAMESPACE_USE
XERCES_CPP_NAMESPACE_USE
#endif

/************************************************************************/
/*                        GMLBinInputStream                             */
/************************************************************************/
class GMLBinInputStream : public BinInputStream
{
    FILE* fp;
    XMLCh emptyString;

public :

             GMLBinInputStream(FILE* fp);
    virtual ~GMLBinInputStream();

#if XERCES_VERSION_MAJOR >= 3
    virtual XMLFilePos curPos() const;
    virtual XMLSize_t readBytes(XMLByte* const toFill, const XMLSize_t maxToRead);
    virtual const XMLCh* getContentType() const ;
#else
    virtual unsigned int curPos() const;
    virtual unsigned int readBytes(XMLByte* const toFill, const unsigned int maxToRead);
#endif
};

/************************************************************************/
/*                           GMLInputSource                             */
/************************************************************************/

class GMLInputSource : public InputSource
{
    GMLBinInputStream* binInputStream;

public:
             GMLInputSource(FILE* fp, 
                            MemoryManager* const manager = XMLPlatformUtils::fgMemoryManager);
    virtual ~GMLInputSource();

    virtual BinInputStream* makeStream() const;
};


/************************************************************************/
/*          XMLCh / char translation functions - trstring.cpp           */
/************************************************************************/
int tr_strcmp( const char *, const XMLCh * );
void tr_strcpy( XMLCh *, const char * );
void tr_strcpy( char *, const XMLCh * );
char *tr_strdup( const XMLCh * );
int tr_strlen( const XMLCh * );

/************************************************************************/
/*                         GMLXercesHandler                             */
/************************************************************************/
class GMLXercesHandler : public DefaultHandler, public GMLHandler
{
    int        m_nEntityCounter;

public:
    GMLXercesHandler( GMLReader *poReader );
    
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
                     const XMLSize_t length );
#else
    void characters( const XMLCh *const chars,
                     const unsigned int length );
#endif

    void fatalError(const SAXParseException&);

    void startEntity (const XMLCh *const name);

    virtual char*       GetFID(void* attr);
    virtual char*       GetAttributes(void* attr);
    virtual char*       GetAttributeValue(void* attr, const char* pszAttributeName);
};

#elif defined(HAVE_EXPAT)

#include "ogr_expat.h"

/************************************************************************/
/*                           GMLExpatHandler                            */
/************************************************************************/
class GMLExpatHandler : public GMLHandler
{
    XML_Parser m_oParser;
    int        m_bStopParsing;
    int        m_nDataHandlerCounter;

public:
    GMLExpatHandler( GMLReader *poReader, XML_Parser oParser );

    virtual OGRErr      startElement(const char *pszName, void* attr);
    virtual OGRErr      endElement(const char *pszName);
    virtual OGRErr      dataHandler(const char *data, int nLen);

    int         HasStoppedParsing() { return m_bStopParsing; }

    void        ResetDataHandlerCounter() { m_nDataHandlerCounter = 0; }
    int         GetDataHandlerCounter() { return m_nDataHandlerCounter; }

    virtual char*       GetFID(void* attr);
    virtual char*       GetAttributes(void* attr);
    virtual char*       GetAttributeValue(void* attr, const char* pszAttributeName);
};

#endif

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
    static int    m_bXercesInitialized;
    static int    m_nInstanceCount;
    int           m_bClassListLocked;

    int         m_nClassCount;
    GMLFeatureClass **m_papoClass;

    char          *m_pszFilename;

#if HAVE_XERCES == 1
    GMLXercesHandler    *m_poGMLHandler;
    SAX2XMLReader *m_poSAXReader;
    XMLPScanToken m_oToFill;
    GMLFeature   *m_poCompleteFeature;
    GMLInputSource *m_GMLInputSource;
#else
    GMLExpatHandler    *m_poGMLHandler;
    XML_Parser    oParser;
    GMLFeature ** ppoFeatureTab;
    int           nFeatureTabLength;
    int           nFeatureTabIndex;
#endif
    FILE*         fpGML;
    int           m_bReadStarted;

    GMLReadState *m_poState;

    int           m_bStopParsing;

    int           SetupParser();
    void          CleanupParser();

public:
                GMLReader();
    virtual     ~GMLReader();

    int              IsClassListLocked() const { return m_bClassListLocked; }
    void             SetClassListLocked( int bFlag )
        { m_bClassListLocked = bFlag; }

    void             SetSourceFile( const char *pszFilename );
    const char*      GetSourceFileName();

    int              GetClassCount() const { return m_nClassCount; }
    GMLFeatureClass *GetClass( int i ) const;
    GMLFeatureClass *GetClass( const char *pszName ) const;

    int              AddClass( GMLFeatureClass *poClass );
    void             ClearClasses();

    GMLFeature       *NextFeature();

    int              LoadClasses( const char *pszFile = NULL );
    int              SaveClasses( const char *pszFile = NULL );

    int              ParseXSD( const char *pszFile );

    int              ResolveXlinks( const char *pszFile,
                                    int* pbOutIsTempFile,
                                    char **papszSkip = NULL,
                                    const int bStrict = FALSE );

    int              PrescanForSchema(int bGetExtents = TRUE );
    void             ResetReading();

// --- 

    GMLReadState     *GetState() const { return m_poState; }
    void             PopState();
    void             PushState( GMLReadState * );

    int         IsFeatureElement( const char *pszElement );
    int         IsAttributeElement( const char *pszElement );
    int         IsCityGMLGenericAttributeElement( const char *pszElement, void* attr );

    void        PushFeature( const char *pszElement, 
                             const char *pszFID );

    void        SetFeatureProperty( const char *pszElement,
                                    const char *pszValue );

    int         HasStoppedParsing() { return m_bStopParsing; }

};

#endif /* _CPL_GMLREADERP_H_INCLUDED */
