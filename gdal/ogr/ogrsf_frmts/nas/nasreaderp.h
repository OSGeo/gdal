/******************************************************************************
 * $Id$
 *
 * Project:  NAS Reader
 * Purpose:  Private Declarations for OGR NAS Reader code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2008, Frank Warmerdam
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef CPL_NASREADERP_H_INCLUDED
#define CPL_NASREADERP_H_INCLUDED

// Must be first for DEBUG_BOOL case
#include "xercesc_headers.h"
#include "ogr_xerces.h"

#include "gmlreader.h"
#include "gmlreaderp.h"
#include "ogr_api.h"
#include "ogr_geometry.h"
#include "cpl_string.h"
#include <list>

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
    int        m_nGeometryPropertyIndex;
    bool       IsGeometryElement( const char * );

    int        m_nDepth;
    int        m_nDepthFeature;
    bool       m_bIgnoreFeature;
    bool       m_bInUpdate;
    bool       m_bInUpdateProperty;
    int        m_nUpdateOrDeleteDepth;
    int        m_nUpdatePropertyDepth;
    int        m_nNameOrValueDepth;

    CPLString  m_osLastTypeName;
    CPLString  m_osLastReplacingFID;
    CPLString  m_osLastSafeToIgnore;
    CPLString  m_osLastPropertyName;
    CPLString  m_osLastPropertyValue;
    CPLString  m_osLastEnded;

    std::list<CPLString> m_LastOccasions;

    CPLString  m_osElementName;
    CPLString  m_osAttrName;
    CPLString  m_osAttrValue;
    CPLString  m_osCharacters;

public:
    explicit NASHandler( NASReader *poReader );
    virtual ~NASHandler();

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
#if XERCES_VERSION_MAJOR >= 3
    void characters( const XMLCh *const chars,
                     const XMLSize_t length ) override;
#else
    void characters( const XMLCh *const chars,
                     const unsigned int length );
#endif

    void fatalError(const SAXParseException&) override;

    CPLString GetAttributes( const Attributes* attr );
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
    bool         m_bClassListLocked;

    int         m_nClassCount;
    GMLFeatureClass **m_papoClass;

    char          *m_pszFilename;

    NASHandler    *m_poNASHandler;
    SAX2XMLReader *m_poSAXReader;
    bool          m_bReadStarted;
    bool          m_bXercesInitialized;
    XMLPScanToken m_oToFill;

    GMLReadState *m_poState;

    GMLFeature   *m_poCompleteFeature;
    VSILFILE     *m_fp;
    InputSource  *m_GMLInputSource;

    bool          SetupParser();
    void          CleanupParser();

    char         *m_pszFilteredClassName;

public:
                NASReader();
    virtual     ~NASReader();

    bool            IsClassListLocked() const override { return m_bClassListLocked; }
    void             SetClassListLocked( bool bFlag ) override
        { m_bClassListLocked = bFlag; }

    void             SetSourceFile( const char *pszFilename ) override;
    const char      *GetSourceFileName() override;

    int              GetClassCount() const override { return m_nClassCount; }
    GMLFeatureClass *GetClass( int i ) const override;
    GMLFeatureClass *GetClass( const char *pszName ) const override;

    int              AddClass( GMLFeatureClass *poClass ) override;
    void             ClearClasses() override;

    GMLFeature       *NextFeature() override;

    bool             LoadClasses( const char *pszFile = NULL ) override;
    bool             SaveClasses( const char *pszFile = NULL ) override;

    bool             PrescanForSchema(bool bGetExtents = true,
                                      bool bAnalyzeSRSPerFeature = true,
                                      bool bOnlyDetectSRS = false) override;
    bool             PrescanForTemplate() override;
    void             ResetReading() override;

    bool             ParseXSD( const char * /* pszFile */ ) { return false; }

    bool             ResolveXlinks( const char *pszFile,
                                    bool* pbOutIsTempFile,
                                    char **papszSkip = NULL,
                                    const bool bStrict = false ) override;

    bool             HugeFileResolver( const char *pszFile,
                                       bool bSqliteIsTempFile,
                                       int iSqliteCacheMB ) override;

// ---

    GMLReadState     *GetState() const { return m_poState; }
    void             PopState();
    void             PushState( GMLReadState * );

    bool        IsFeatureElement( const char *pszElement );
    bool        IsAttributeElement( const char *pszElement );

    void        PushFeature( const char *pszElement,
                             const Attributes &attrs );

    void        SetFeaturePropertyDirectly( const char *pszElement,
                                    char *pszValue );

    bool        HasStoppedParsing() override { return false; }

    void        CheckForFID( const Attributes &attrs, char **ppszCurField );
    void        CheckForRelations( const char *pszElement,
                                   const Attributes &attrs,
                                   char **ppszCurField );

    virtual const char* GetGlobalSRSName() override { return NULL; }

    virtual bool        CanUseGlobalSRSName() override { return false; }

    bool        SetFilteredClassName(const char* pszClassName) override;
    const char* GetFilteredClassName() override { return m_pszFilteredClassName; }

    static      OGRGeometry* ConvertGeometry(OGRGeometry*);
};

#endif /* CPL_NASREADERP_H_INCLUDED */
