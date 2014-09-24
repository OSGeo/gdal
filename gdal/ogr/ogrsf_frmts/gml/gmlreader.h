/******************************************************************************
 * $Id$
 *
 * Project:  GML Reader
 * Purpose:  Public Declarations for OGR free GML Reader code.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _GMLREADER_H_INCLUDED
#define _GMLREADER_H_INCLUDED

#include "cpl_port.h"
#include "cpl_vsi.h"
#include "cpl_minixml.h"

#include <vector>

typedef enum {
    GMLPT_Untyped = 0,
    GMLPT_String = 1,
    GMLPT_Integer = 2,
    GMLPT_Real = 3,
    GMLPT_Complex = 4,
    GMLPT_StringList = 5,
    GMLPT_IntegerList = 6, 
    GMLPT_RealList = 7,
    GMLPT_FeatureProperty = 8,
    GMLPT_FeaturePropertyList = 9
} GMLPropertyType;

/************************************************************************/
/*                           GMLPropertyDefn                            */
/************************************************************************/

typedef struct
{
    int     nSubProperties;
    char**  papszSubProperties;
    char*   aszSubProperties[2]; /* Optimization in the case of nSubProperties == 1 */
} GMLProperty;

class CPL_DLL GMLPropertyDefn
{
    char             *m_pszName;
    GMLPropertyType   m_eType;
    int               m_nWidth;
    int               m_nPrecision;
    char             *m_pszSrcElement;
    size_t            m_nSrcElementLen;
    char             *m_pszCondition;

public:
    
        GMLPropertyDefn( const char *pszName, const char *pszSrcElement=NULL );
       ~GMLPropertyDefn();

    const char *GetName() const { return m_pszName; } 

    GMLPropertyType GetType() const { return m_eType; } 
    void        SetType( GMLPropertyType eType ) { m_eType = eType; }
    void        SetWidth( int nWidth) { m_nWidth = nWidth; }
    int         GetWidth() const { return m_nWidth; }
    void        SetPrecision( int nPrecision) { m_nPrecision = nPrecision; }
    int         GetPrecision() const { return m_nPrecision; }
    void        SetSrcElement( const char *pszSrcElement );
    const char *GetSrcElement() const { return m_pszSrcElement; }
    size_t      GetSrcElementLen() const { return m_nSrcElementLen; }

    void        SetCondition( const char *pszCondition );
    const char *GetCondition() const { return m_pszCondition; }

    void        AnalysePropertyValue( const GMLProperty* psGMLProperty,
                                      int bSetWidth = TRUE );

    static bool IsSimpleType( GMLPropertyType eType )
    { return eType == GMLPT_String || eType == GMLPT_Integer || eType == GMLPT_Real; }
};

/************************************************************************/
/*                    GMLGeometryPropertyDefn                           */
/************************************************************************/

class CPL_DLL GMLGeometryPropertyDefn
{
    char       *m_pszName;
    char       *m_pszSrcElement;
    int         m_nGeometryType;
    int         m_nAttributeIndex;
    
public:
        GMLGeometryPropertyDefn( const char *pszName, const char *pszSrcElement,
                                 int nType, int nAttributeIndex = -1 );
       ~GMLGeometryPropertyDefn();

        const char *GetName() const { return m_pszName; } 

        int GetType() const { return m_nGeometryType; }
        void SetType(int nType) { m_nGeometryType = nType; }
        const char *GetSrcElement() const { return m_pszSrcElement; }
        
        int GetAttributeIndex() const { return m_nAttributeIndex; }
};

/************************************************************************/
/*                           GMLFeatureClass                            */
/************************************************************************/
class CPL_DLL GMLFeatureClass
{
    char        *m_pszName;
    char        *m_pszElementName;
    int          n_nNameLen;
    int          n_nElementNameLen;
    int         m_nPropertyCount;
    GMLPropertyDefn **m_papoProperty;
    
    int         m_nGeometryPropertyCount;
    GMLGeometryPropertyDefn **m_papoGeometryProperty;

    int         m_bSchemaLocked;

    int         m_nFeatureCount;

    char        *m_pszExtraInfo;

    int         m_bHaveExtents;
    double      m_dfXMin;
    double      m_dfXMax;
    double      m_dfYMin;
    double      m_dfYMax;

    char       *m_pszSRSName;
    int         m_bSRSNameConsistant;

public:
            GMLFeatureClass( const char *pszName = "" );
           ~GMLFeatureClass();

    const char *GetElementName() const;
    size_t      GetElementNameLen() const;
    void        SetElementName( const char *pszElementName );

    const char *GetName() const { return m_pszName; }
    void        SetName(const char* pszNewName);
    int         GetPropertyCount() const { return m_nPropertyCount; }
    GMLPropertyDefn *GetProperty( int iIndex ) const;
    int GetPropertyIndex( const char *pszName ) const;
    GMLPropertyDefn *GetProperty( const char *pszName ) const 
        { return GetProperty( GetPropertyIndex(pszName) ); }
    int         GetPropertyIndexBySrcElement( const char *pszElement, int nLen ) const;
    
    int         GetGeometryPropertyCount() const { return m_nGeometryPropertyCount; }
    GMLGeometryPropertyDefn *GetGeometryProperty( int iIndex ) const;
    int         GetGeometryPropertyIndexBySrcElement( const char *pszElement ) const;

    int         HasFeatureProperties();

    int         AddProperty( GMLPropertyDefn * );
    int         AddGeometryProperty( GMLGeometryPropertyDefn * );
    void        ClearGeometryProperties();

    int         IsSchemaLocked() const { return m_bSchemaLocked; }
    void        SetSchemaLocked( int bLock ) { m_bSchemaLocked = bLock; }

    const char  *GetExtraInfo();
    void        SetExtraInfo( const char * );

    int         GetFeatureCount();
    void        SetFeatureCount( int );

    int         HasExtents() const { return m_bHaveExtents; }
    void        SetExtents( double dfXMin, double dfXMax, 
                            double dFYMin, double dfYMax );
    int         GetExtents( double *pdfXMin, double *pdfXMax, 
                            double *pdFYMin, double *pdfYMax );

    void        SetSRSName( const char* pszSRSName );
    void        MergeSRSName( const char* pszSRSName );
    const char *GetSRSName() { return m_pszSRSName; }

    CPLXMLNode *SerializeToXML();
    int         InitializeFromXML( CPLXMLNode * );
};

/************************************************************************/
/*                              GMLFeature                              */
/************************************************************************/

class CPL_DLL GMLFeature
{
    GMLFeatureClass *m_poClass;
    char            *m_pszFID;

    int              m_nPropertyCount;
    GMLProperty     *m_pasProperties;

    int              m_nGeometryCount;
    CPLXMLNode     **m_papsGeometry; /* NULL-terminated. Alias to m_apsGeometry if m_nGeometryCount <= 1 */
    CPLXMLNode      *m_apsGeometry[2]; /* NULL-terminated */

    // string list of named non-schema properties - used by NAS driver.
    char           **m_papszOBProperties;

public:
                    GMLFeature( GMLFeatureClass * );
                   ~GMLFeature();

    GMLFeatureClass*GetClass() const { return m_poClass; }

    void            SetGeometryDirectly( CPLXMLNode* psGeom );
    void            SetGeometryDirectly( int nIdx, CPLXMLNode* psGeom );
    void            AddGeometry( CPLXMLNode* psGeom );
    const CPLXMLNode* const * GetGeometryList() const { return m_papsGeometry; }
    const CPLXMLNode* GetGeometryRef( int nIdx ) const;

    void            SetPropertyDirectly( int i, char *pszValue );

    const GMLProperty*GetProperty( int i ) const { return (i < m_nPropertyCount) ? &m_pasProperties[i] : NULL; }

    const char      *GetFID() const { return m_pszFID; }
    void             SetFID( const char *pszFID );

    void             Dump( FILE *fp );

    // Out of Band property handling - special stuff like relations for NAS.
    void             AddOBProperty( const char *pszName, const char *pszValue );
    const char      *GetOBProperty( const char *pszName );
    char           **GetOBProperties();
};

/************************************************************************/
/*                              IGMLReader                              */
/************************************************************************/
class CPL_DLL IGMLReader
{
public:
    virtual     ~IGMLReader();

    virtual int  IsClassListLocked() const = 0;
    virtual void SetClassListLocked( int bFlag ) = 0;

    virtual void SetSourceFile( const char *pszFilename ) = 0;
    virtual void SetFP( CPL_UNUSED VSILFILE* fp ) { }
    virtual const char* GetSourceFileName() = 0;

    virtual int  GetClassCount() const = 0;
    virtual GMLFeatureClass *GetClass( int i ) const = 0;
    virtual GMLFeatureClass *GetClass( const char *pszName ) const = 0;

    virtual int        AddClass( GMLFeatureClass *poClass ) = 0;
    virtual void       ClearClasses() = 0;

    virtual GMLFeature *NextFeature() = 0;
    virtual void       ResetReading() = 0;

    virtual int  LoadClasses( const char *pszFile = NULL ) = 0;
    virtual int  SaveClasses( const char *pszFile = NULL ) = 0;

    virtual int  ResolveXlinks( const char *pszFile,
                                int* pbOutIsTempFile,
                                char **papszSkip = NULL,
                                const int bStrict = FALSE ) = 0;

    virtual int  HugeFileResolver( const char *pszFile,
                                   int pbSqlitIsTempFile,
                                   int iSqliteCacheMB ) = 0;

    virtual int PrescanForSchema( int bGetExtents = TRUE, int bAnalyzeSRSPerFeature = TRUE ) = 0;
    virtual int PrescanForTemplate( void ) = 0;

    virtual int HasStoppedParsing() = 0;

    virtual void  SetGlobalSRSName( CPL_UNUSED const char* pszGlobalSRSName ) {}
    virtual const char* GetGlobalSRSName() = 0;
    virtual int CanUseGlobalSRSName() = 0;

    virtual int SetFilteredClassName(const char* pszClassName) = 0;
    virtual const char* GetFilteredClassName() = 0;

    virtual int IsSequentialLayers() const { return FALSE; }
};

IGMLReader *CreateGMLReader(int bUseExpatParserPreferably,
                            int bInvertAxisOrderIfLatLong,
                            int bConsiderEPSGAsURN,
                            int bGetSecondaryGeometryOption);


#endif /* _GMLREADER_H_INCLUDED */
