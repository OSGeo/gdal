/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Class for representing a whole feature, and layer schemas.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef _OGR_FEATURE_H_INCLUDED
#define _OGR_FEATURE_H_INCLUDED

#include "ogr_geometry.h"
#include "ogr_featurestyle.h"
#include "cpl_atomic_ops.h"

/**
 * \file ogr_feature.h
 *
 * Simple feature classes.
 */

/************************************************************************/
/*                             OGRFieldDefn                             */
/************************************************************************/

/**
 * Definition of an attribute of an OGRFeatureDefn.
 */

class CPL_DLL OGRFieldDefn
{
  private:
    char                *pszName;
    OGRFieldType        eType;                  
    OGRJustification    eJustify;               
    int                 nWidth;                 /* zero is variable */
    int                 nPrecision;
    OGRField            uDefault;
    
    int                 bIgnore;

    void                Initialize( const char *, OGRFieldType );
    
  public:
                        OGRFieldDefn( const char *, OGRFieldType );
                        OGRFieldDefn( OGRFieldDefn * );
                        ~OGRFieldDefn();

    void                SetName( const char * );
    const char         *GetNameRef() { return pszName; }

    OGRFieldType        GetType() { return eType; }
    void                SetType( OGRFieldType eTypeIn ) { eType = eTypeIn;}
    static const char  *GetFieldTypeName( OGRFieldType );

    OGRJustification    GetJustify() { return eJustify; }
    void                SetJustify( OGRJustification eJustifyIn )
                                                { eJustify = eJustifyIn; }

    int                 GetWidth() { return nWidth; }
    void                SetWidth( int nWidthIn ) { nWidth = MAX(0,nWidthIn); }

    int                 GetPrecision() { return nPrecision; }
    void                SetPrecision( int nPrecisionIn )
                                                { nPrecision = nPrecisionIn; }

    void                Set( const char *, OGRFieldType, int = 0, int = 0,
                             OGRJustification = OJUndefined );

    void                SetDefault( const OGRField * );
    const OGRField     *GetDefaultRef() { return &uDefault; }
    
    int                 IsIgnored() { return bIgnore; }
    void                SetIgnored( int bIgnore ) { this->bIgnore = bIgnore; }

    int                 IsSame( const OGRFieldDefn * ) const;
};

/************************************************************************/
/*                            OGRFeatureDefn                            */
/************************************************************************/

/**
 * Definition of a feature class or feature layer.
 *
 * This object contains schema information for a set of OGRFeatures.  In
 * table based systems, an OGRFeatureDefn is essentially a layer.  In more
 * object oriented approaches (such as SF CORBA) this can represent a class
 * of features but doesn't necessarily relate to all of a layer, or just one
 * layer.
 *
 * This object also can contain some other information such as a name, the
 * base geometry type and potentially other metadata.
 *
 * It is reasonable for different translators to derive classes from
 * OGRFeatureDefn with additional translator specific information. 
 */

class CPL_DLL OGRFeatureDefn
{
  private:
    volatile int nRefCount;
    
    int         nFieldCount;
    OGRFieldDefn **papoFieldDefn;

    OGRwkbGeometryType eGeomType;

    char        *pszFeatureClassName;
    
    int         bIgnoreGeometry;
    int         bIgnoreStyle;
    
  public:
                OGRFeatureDefn( const char * pszName = NULL );
    virtual    ~OGRFeatureDefn();

    const char  *GetName() { return pszFeatureClassName; }

    int         GetFieldCount() { return nFieldCount; }
    OGRFieldDefn *GetFieldDefn( int i );
    int         GetFieldIndex( const char * );

    void        AddFieldDefn( OGRFieldDefn * );
    OGRErr      DeleteFieldDefn( int iField );
    OGRErr      ReorderFieldDefns( int* panMap );

    OGRwkbGeometryType GetGeomType() { return eGeomType; }
    void        SetGeomType( OGRwkbGeometryType );

    OGRFeatureDefn *Clone();

    int         Reference() { return CPLAtomicInc(&nRefCount); }
    int         Dereference() { return CPLAtomicDec(&nRefCount); }
    int         GetReferenceCount() { return nRefCount; }
    void        Release();

    int         IsGeometryIgnored() { return bIgnoreGeometry; }
    void        SetGeometryIgnored( int bIgnore ) { bIgnoreGeometry = bIgnore; }
    int        IsStyleIgnored() { return bIgnoreStyle; }
    void        SetStyleIgnored( int bIgnore ) { bIgnoreStyle = bIgnore; }

    int         IsSame( const OGRFeatureDefn * poOtherFeatureDefn ) const;

    static OGRFeatureDefn  *CreateFeatureDefn( const char *pszName = NULL );
    static void         DestroyFeatureDefn( OGRFeatureDefn * );
};

/************************************************************************/
/*                              OGRFeature                              */
/************************************************************************/

/**
 * A simple feature, including geometry and attributes.
 */

class CPL_DLL OGRFeature
{
  private:

    long                nFID;
    OGRFeatureDefn      *poDefn;
    OGRGeometry         *poGeometry;
    OGRField            *pauFields;

  protected: 
    char *              m_pszStyleString;
    OGRStyleTable       *m_poStyleTable;
    char *              m_pszTmpFieldValue;
    
  public:
                        OGRFeature( OGRFeatureDefn * );
    virtual            ~OGRFeature();                        

    OGRFeatureDefn     *GetDefnRef() { return poDefn; }
    
    OGRErr              SetGeometryDirectly( OGRGeometry * );
    OGRErr              SetGeometry( OGRGeometry * );
    OGRGeometry        *GetGeometryRef() { return poGeometry; }
    OGRGeometry        *StealGeometry();

    OGRFeature         *Clone();
    virtual OGRBoolean  Equal( OGRFeature * poFeature );

    int                 GetFieldCount() { return poDefn->GetFieldCount(); }
    OGRFieldDefn       *GetFieldDefnRef( int iField )
                                      { return poDefn->GetFieldDefn(iField); }
    int                 GetFieldIndex( const char * pszName)
                                      { return poDefn->GetFieldIndex(pszName);}

    int                 IsFieldSet( int iField ) const;
    
    void                UnsetField( int iField );
    
    OGRField           *GetRawFieldRef( int i ) { return pauFields + i; }

    int                 GetFieldAsInteger( int i );
    double              GetFieldAsDouble( int i );
    const char         *GetFieldAsString( int i );
    const int          *GetFieldAsIntegerList( int i, int *pnCount );
    const double       *GetFieldAsDoubleList( int i, int *pnCount );
    char              **GetFieldAsStringList( int i ) const;
    GByte              *GetFieldAsBinary( int i, int *pnCount );
    int                 GetFieldAsDateTime( int i, 
                                     int *pnYear, int *pnMonth, int *pnDay,
                                     int *pnHour, int *pnMinute, int *pnSecond, 
                                     int *pnTZFlag );

    int                 GetFieldAsInteger( const char *pszFName )
                      { return GetFieldAsInteger( GetFieldIndex(pszFName) ); }
    double              GetFieldAsDouble( const char *pszFName )
                      { return GetFieldAsDouble( GetFieldIndex(pszFName) ); }
    const char         *GetFieldAsString( const char *pszFName )
                      { return GetFieldAsString( GetFieldIndex(pszFName) ); }
    const int          *GetFieldAsIntegerList( const char *pszFName,
                                               int *pnCount )
                      { return GetFieldAsIntegerList( GetFieldIndex(pszFName),
                                                      pnCount ); }
    const double       *GetFieldAsDoubleList( const char *pszFName,
                                              int *pnCount )
                      { return GetFieldAsDoubleList( GetFieldIndex(pszFName),
                                                     pnCount ); }
    char              **GetFieldAsStringList( const char *pszFName )
                      { return GetFieldAsStringList(GetFieldIndex(pszFName)); }

    void                SetField( int i, int nValue );
    void                SetField( int i, double dfValue );
    void                SetField( int i, const char * pszValue );
    void                SetField( int i, int nCount, int * panValues );
    void                SetField( int i, int nCount, double * padfValues );
    void                SetField( int i, char ** papszValues );
    void                SetField( int i, OGRField * puValue );
    void                SetField( int i, int nCount, GByte * pabyBinary );
    void                SetField( int i, int nYear, int nMonth, int nDay,
                                  int nHour=0, int nMinute=0, int nSecond=0, 
                                  int nTZFlag = 0 );

    void                SetField( const char *pszFName, int nValue )
                           { SetField( GetFieldIndex(pszFName), nValue ); }
    void                SetField( const char *pszFName, double dfValue )
                           { SetField( GetFieldIndex(pszFName), dfValue ); }
    void                SetField( const char *pszFName, const char * pszValue)
                           { SetField( GetFieldIndex(pszFName), pszValue ); }
    void                SetField( const char *pszFName, int nCount,
                                  int * panValues )
                         { SetField(GetFieldIndex(pszFName),nCount,panValues);}
    void                SetField( const char *pszFName, int nCount,
                                  double * padfValues )
                         {SetField(GetFieldIndex(pszFName),nCount,padfValues);}
    void                SetField( const char *pszFName, char ** papszValues )
                           { SetField( GetFieldIndex(pszFName), papszValues); }
    void                SetField( const char *pszFName, OGRField * puValue )
                           { SetField( GetFieldIndex(pszFName), puValue ); }
    void                SetField( const char *pszFName, 
                                  int nYear, int nMonth, int nDay,
                                  int nHour=0, int nMinute=0, int nSecond=0, 
                                  int nTZFlag = 0 )
                           { SetField( GetFieldIndex(pszFName), 
                                       nYear, nMonth, nDay, 
                                       nHour, nMinute, nSecond, nTZFlag ); }

    long                GetFID() { return nFID; }
    virtual OGRErr      SetFID( long nFID );

    void                DumpReadable( FILE *, char** papszOptions = NULL );

    OGRErr              SetFrom( OGRFeature *, int = TRUE);
    OGRErr              SetFrom( OGRFeature *, int *, int = TRUE );
    OGRErr              SetFieldsFrom( OGRFeature *, int *, int = TRUE ); 

    OGRErr              RemapFields( OGRFeatureDefn *poNewDefn, 
                                     int *panRemapSource );

    virtual const char *GetStyleString();
    virtual void        SetStyleString( const char * );
    virtual void        SetStyleStringDirectly( char * );
    virtual OGRStyleTable *GetStyleTable() { return m_poStyleTable; }
    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);
    virtual void        SetStyleTableDirectly(OGRStyleTable *poStyleTable)
                            { if ( m_poStyleTable ) delete m_poStyleTable;
                              m_poStyleTable = poStyleTable; }

    static OGRFeature  *CreateFeature( OGRFeatureDefn * );
    static void         DestroyFeature( OGRFeature * );
};

/************************************************************************/
/*                           OGRFeatureQuery                            */
/************************************************************************/

class OGRLayer;
class swq_expr_node;

class CPL_DLL OGRFeatureQuery
{
  private:
    OGRFeatureDefn *poTargetDefn;
    void           *pSWQExpr;

    char          **FieldCollector( void *, char ** );

    long       *EvaluateAgainstIndices( swq_expr_node*, OGRLayer *, int& nFIDCount);
    
  public:
                OGRFeatureQuery();
                ~OGRFeatureQuery();

    OGRErr      Compile( OGRFeatureDefn *, const char * );
    int         Evaluate( OGRFeature * );

    long       *EvaluateAgainstIndices( OGRLayer *, OGRErr * );

    char      **GetUsedFields();

    void       *GetSWGExpr() { return pSWQExpr; }
};

#endif /* ndef _OGR_FEATURE_H_INCLUDED */
