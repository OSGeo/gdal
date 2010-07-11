/******************************************************************************
 * $Id: gmlreaderp.h 10645 2007-01-18 02:22:39Z warmerdam $
 *
 * Project:  NAS Reader
 * Purpose:  Private Declarations for OGR NAS Reader code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
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

#ifndef _CPL_NASREADERP_H_INCLUDED
#define _CPL_NASREADERP_H_INCLUDED

#include "gmlreader.h"
#include "gmlreaderp.h"
#include "ogr_api.h"

IGMLReader *CreateNASReader();

class NASReader;
class OGRNASRelationLayer;

CPL_C_START
OGRGeometryH OGR_G_CreateFromGML3( const char *pszGML );
CPL_C_END

/************************************************************************/
/*                              NASHandler                              */
/************************************************************************/
class NASHandler : public DefaultHandler 
{
    NASReader  *m_poReader;

    char       *m_pszCurField;

    char       *m_pszGeometry;
    int        m_nGeomAlloc;
    int        m_nGeomLen;

    int        m_nGeometryDepth;
    int        IsGeometryElement( const char * );

public:
    NASHandler( NASReader *poReader );
    virtual ~NASHandler();
    
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
};

/************************************************************************/
/*                             GMLReadState                             */
/************************************************************************/

// for now, use existing gmlreadstate.
#ifdef notdef 
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
#endif

/************************************************************************/
/*                              NASReader                               */
/************************************************************************/

class NASReader : public IGMLReader 
{
private:
    int           m_bClassListLocked;

    int         m_nClassCount;
    GMLFeatureClass **m_papoClass;

    char          *m_pszFilename;

    NASHandler    *m_poNASHandler;
    SAX2XMLReader *m_poSAXReader;
    int           m_bReadStarted;
    XMLPScanToken m_oToFill;

    GMLReadState *m_poState;

    GMLFeature   *m_poCompleteFeature;

    int           SetupParser();
    void          CleanupParser();

public:
                NASReader();
    virtual     ~NASReader();

    int              IsClassListLocked() const { return m_bClassListLocked; }
    void             SetClassListLocked( int bFlag )
        { m_bClassListLocked = bFlag; }

    void             SetSourceFile( const char *pszFilename );
    const char      *GetSourceFileName();

    int              GetClassCount() const { return m_nClassCount; }
    GMLFeatureClass *GetClass( int i ) const;
    GMLFeatureClass *GetClass( const char *pszName ) const;

    int              AddClass( GMLFeatureClass *poClass );
    void             ClearClasses();

    GMLFeature       *NextFeature();

    int              LoadClasses( const char *pszFile = NULL );
    int              SaveClasses( const char *pszFile = NULL );

    int              PrescanForSchema(int bGetExtents = TRUE );
    void             ResetReading();

    int              ParseXSD( const char *pszFile ) { return FALSE; }

    int              ResolveXlinks( const char *pszFile,
                                    int* pbOutIsTempFile,
                                    char **papszSkip = NULL,
                                    const int bStrict = FALSE );

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


    void       AnalysePropertyValue( GMLPropertyDefn *poDefn,
                                     const char *pszValue, 
                                     const char *pszOldValue );

    int         HasStoppedParsing() { return FALSE; }

    void        CheckForRelations( const char *pszElement, 
                                   const Attributes &attrs );
};

#endif /* _CPL_NASREADERP_H_INCLUDED */
