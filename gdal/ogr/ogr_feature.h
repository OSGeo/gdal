/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Class for representing a whole feature, and layer schemas.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
 * Definition of an attribute of an OGRFeatureDefn. A field is described by :
 * <ul>
 * <li>a name. See SetName() / GetNameRef()</li>
 * <li>a type: OFTString, OFTInteger, OFTReal, ... See SetType() / GetType()</li>
 * <li>a subtype (optional): OFSTBoolean, ... See SetSubType() / GetSubType()</li>
 * <li>a width (optional): maximal number of characters. See SetWidth() / GetWidth()</li>
 * <li>a precision (optional): number of digits after decimal point. See SetPrecision() / GetPrecision()</li>
 * <li>a NOT NULL constraint (optional). See SetNullable() / IsNullable()</li>
 * <li>a default value (optional).  See SetDefault() / GetDefault()</li>
 * <li>a boolean to indicate whether it should be ignored when retrieving features.  See SetIgnored() / IsIgnored()</li>
 * </ul>
 */

class CPL_DLL OGRFieldDefn
{
  private:
    char                *pszName;
    OGRFieldType        eType;                  
    OGRJustification    eJustify;               
    int                 nWidth;                 /* zero is variable */
    int                 nPrecision;
    char                *pszDefault;
    
    int                 bIgnore;
    OGRFieldSubType     eSubType;
    
    int                 bNullable;

    void                Initialize( const char *, OGRFieldType );
    
  public:
                        OGRFieldDefn( const char *, OGRFieldType );
                        OGRFieldDefn( OGRFieldDefn * );
                        ~OGRFieldDefn();

    void                SetName( const char * );
    const char         *GetNameRef() { return pszName; }

    OGRFieldType        GetType() { return eType; }
    void                SetType( OGRFieldType eTypeIn );
    static const char  *GetFieldTypeName( OGRFieldType );

    OGRFieldSubType     GetSubType() { return eSubType; }
    void                SetSubType( OGRFieldSubType eSubTypeIn );
    static const char  *GetFieldSubTypeName( OGRFieldSubType );

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

    void                SetDefault( const char* );
    const char         *GetDefault() const;
    int                 IsDefaultDriverSpecific() const;

    int                 IsIgnored() { return bIgnore; }
    void                SetIgnored( int bIgnoreIn ) { bIgnore = bIgnoreIn; }

    int                 IsNullable() const { return bNullable; }
    void                SetNullable( int bNullableIn ) { bNullable = bNullableIn; }

    int                 IsSame( const OGRFieldDefn * ) const;
};

/************************************************************************/
/*                          OGRGeomFieldDefn                            */
/************************************************************************/

/**
 * Definition of a geometry field of an OGRFeatureDefn. A geometry field is
 * described by :
 * <ul>
 * <li>a name. See SetName() / GetNameRef()</li>
 * <li>a type: wkbPoint, wkbLineString, ... See SetType() / GetType()</li>
 * <li>a spatial reference system (optional). See SetSpatialRef() / GetSpatialRef()</li>
 * <li>a NOT NULL constraint (optional). See SetNullable() / IsNullable()</li>
 * <li>a boolean to indicate whether it should be ignored when retrieving features.  See SetIgnored() / IsIgnored()</li>
 * </ul>
 *
 * @since OGR 1.11
 */

class CPL_DLL OGRGeomFieldDefn
{
protected:
        char                *pszName;
        OGRwkbGeometryType   eGeomType; /* all values possible except wkbNone */
        OGRSpatialReference* poSRS;

        int                 bIgnore;
        int                 bNullable;

        void                Initialize( const char *, OGRwkbGeometryType );

public:
                            OGRGeomFieldDefn(const char *pszNameIn,
                                             OGRwkbGeometryType eGeomTypeIn);
                            OGRGeomFieldDefn( OGRGeomFieldDefn * );
        virtual            ~OGRGeomFieldDefn();

        void                SetName( const char * );
        const char         *GetNameRef() { return pszName; }

        OGRwkbGeometryType  GetType() { return eGeomType; }
        void                SetType( OGRwkbGeometryType eTypeIn );

        virtual OGRSpatialReference* GetSpatialRef();
        void                 SetSpatialRef(OGRSpatialReference* poSRSIn);

        int                 IsIgnored() { return bIgnore; }
        void                SetIgnored( int bIgnoreIn ) { bIgnore = bIgnoreIn; }

        int                 IsNullable() const { return bNullable; }
        void                SetNullable( int bNullableIn ) { bNullable = bNullableIn; }

        int                 IsSame( OGRGeomFieldDefn * );
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
 * This object also can contain some other information such as a name and
 * potentially other metadata.
 *
 * It is essentially a collection of field descriptions (OGRFieldDefn class).
 * Starting with GDAL 1.11, in addition to attribute fields, it can also
 * contain multiple geometry fields (OGRGeomFieldDefn class).
 *
 * It is reasonable for different translators to derive classes from
 * OGRFeatureDefn with additional translator specific information. 
 */

class CPL_DLL OGRFeatureDefn
{
  protected:
    volatile int nRefCount;
    
    int         nFieldCount;
    OGRFieldDefn **papoFieldDefn;

    int                nGeomFieldCount;
    OGRGeomFieldDefn **papoGeomFieldDefn;

    char        *pszFeatureClassName;

    int         bIgnoreStyle;
    
  public:
                OGRFeatureDefn( const char * pszName = NULL );
    virtual    ~OGRFeatureDefn();

    virtual const char  *GetName();

    virtual int         GetFieldCount();
    virtual OGRFieldDefn *GetFieldDefn( int i );
    virtual int         GetFieldIndex( const char * );

    virtual void        AddFieldDefn( OGRFieldDefn * );
    virtual OGRErr      DeleteFieldDefn( int iField );
    virtual OGRErr      ReorderFieldDefns( int* panMap );

    virtual int         GetGeomFieldCount();
    virtual OGRGeomFieldDefn *GetGeomFieldDefn( int i );
    virtual int         GetGeomFieldIndex( const char * );

    virtual void        AddGeomFieldDefn( OGRGeomFieldDefn *, int bCopy = TRUE );
    virtual OGRErr      DeleteGeomFieldDefn( int iGeomField );

    virtual OGRwkbGeometryType GetGeomType();
    virtual void        SetGeomType( OGRwkbGeometryType );

    virtual OGRFeatureDefn *Clone();

    int         Reference() { return CPLAtomicInc(&nRefCount); }
    int         Dereference() { return CPLAtomicDec(&nRefCount); }
    int         GetReferenceCount() { return nRefCount; }
    void        Release();

    virtual int         IsGeometryIgnored();
    virtual void        SetGeometryIgnored( int bIgnore );
    virtual int        IsStyleIgnored() { return bIgnoreStyle; }
    virtual void        SetStyleIgnored( int bIgnore ) { bIgnoreStyle = bIgnore; }

    virtual int         IsSame( OGRFeatureDefn * poOtherFeatureDefn );

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

    GIntBig              nFID;
    OGRFeatureDefn      *poDefn;
    OGRGeometry        **papoGeometries;
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
    OGRGeometry        *GetGeometryRef();
    OGRGeometry        *StealGeometry();

    int                 GetGeomFieldCount()
                                { return poDefn->GetGeomFieldCount(); }
    OGRGeomFieldDefn   *GetGeomFieldDefnRef( int iField )
                                { return poDefn->GetGeomFieldDefn(iField); }
    int                 GetGeomFieldIndex( const char * pszName)
                                { return poDefn->GetGeomFieldIndex(pszName); }

    OGRGeometry*        GetGeomFieldRef(int iField);
    OGRGeometry*        StealGeometry(int iField);
    OGRGeometry*        GetGeomFieldRef(const char* pszFName);
    OGRErr              SetGeomFieldDirectly( int iField, OGRGeometry * );
    OGRErr              SetGeomField( int iField, OGRGeometry * );

    OGRFeature         *Clone();
    virtual OGRBoolean  Equal( OGRFeature * poFeature );

    int                 GetFieldCount() { return poDefn->GetFieldCount(); }
    OGRFieldDefn       *GetFieldDefnRef( int iField )
                                      { return poDefn->GetFieldDefn(iField); }
    int                 GetFieldIndex( const char * pszName)
                                      { return poDefn->GetFieldIndex(pszName);}

    int                 IsFieldSet( int iField );
    
    void                UnsetField( int iField );
    
    OGRField           *GetRawFieldRef( int i ) { return pauFields + i; }

    int                 GetFieldAsInteger( int i );
    GIntBig             GetFieldAsInteger64( int i );
    double              GetFieldAsDouble( int i );
    const char         *GetFieldAsString( int i );
    const int          *GetFieldAsIntegerList( int i, int *pnCount );
    const GIntBig      *GetFieldAsInteger64List( int i, int *pnCount );
    const double       *GetFieldAsDoubleList( int i, int *pnCount );
    char              **GetFieldAsStringList( int i );
    GByte              *GetFieldAsBinary( int i, int *pnCount );
    int                 GetFieldAsDateTime( int i, 
                                     int *pnYear, int *pnMonth, int *pnDay,
                                     int *pnHour, int *pnMinute, int *pnSecond, 
                                     int *pnTZFlag );
    int                 GetFieldAsDateTime( int i, 
                                     int *pnYear, int *pnMonth, int *pnDay,
                                     int *pnHour, int *pnMinute, float *pfSecond, 
                                     int *pnTZFlag );

    int                 GetFieldAsInteger( const char *pszFName )
                      { return GetFieldAsInteger( GetFieldIndex(pszFName) ); }
    GIntBig             GetFieldAsInteger64( const char *pszFName )
                      { return GetFieldAsInteger64( GetFieldIndex(pszFName) ); }
    double              GetFieldAsDouble( const char *pszFName )
                      { return GetFieldAsDouble( GetFieldIndex(pszFName) ); }
    const char         *GetFieldAsString( const char *pszFName )
                      { return GetFieldAsString( GetFieldIndex(pszFName) ); }
    const int          *GetFieldAsIntegerList( const char *pszFName,
                                               int *pnCount )
                      { return GetFieldAsIntegerList( GetFieldIndex(pszFName),
                                                      pnCount ); }
    const GIntBig      *GetFieldAsInteger64List( const char *pszFName,
                                               int *pnCount )
                      { return GetFieldAsInteger64List( GetFieldIndex(pszFName),
                                                      pnCount ); }
    const double       *GetFieldAsDoubleList( const char *pszFName,
                                              int *pnCount )
                      { return GetFieldAsDoubleList( GetFieldIndex(pszFName),
                                                     pnCount ); }
    char              **GetFieldAsStringList( const char *pszFName )
                      { return GetFieldAsStringList(GetFieldIndex(pszFName)); }

    void                SetField( int i, int nValue );
    void                SetField( int i, GIntBig nValue );
    void                SetField( int i, double dfValue );
    void                SetField( int i, const char * pszValue );
    void                SetField( int i, int nCount, int * panValues );
    void                SetField( int i, int nCount, const GIntBig * panValues );
    void                SetField( int i, int nCount, double * padfValues );
    void                SetField( int i, char ** papszValues );
    void                SetField( int i, OGRField * puValue );
    void                SetField( int i, int nCount, GByte * pabyBinary );
    void                SetField( int i, int nYear, int nMonth, int nDay,
                                  int nHour=0, int nMinute=0, float fSecond=0.f, 
                                  int nTZFlag = 0 );

    void                SetField( const char *pszFName, int nValue )
                           { SetField( GetFieldIndex(pszFName), nValue ); }
    void                SetField( const char *pszFName, GIntBig nValue )
                           { SetField( GetFieldIndex(pszFName), nValue ); }
    void                SetField( const char *pszFName, double dfValue )
                           { SetField( GetFieldIndex(pszFName), dfValue ); }
    void                SetField( const char *pszFName, const char * pszValue)
                           { SetField( GetFieldIndex(pszFName), pszValue ); }
    void                SetField( const char *pszFName, int nCount,
                                  int * panValues )
                         { SetField(GetFieldIndex(pszFName),nCount,panValues);}
    void                SetField( const char *pszFName, int nCount,
                                  const GIntBig * panValues )
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
                                  int nHour=0, int nMinute=0, float fSecond=0.f, 
                                  int nTZFlag = 0 )
                           { SetField( GetFieldIndex(pszFName), 
                                       nYear, nMonth, nDay, 
                                       nHour, nMinute, fSecond, nTZFlag ); }

    GIntBig             GetFID() { return nFID; }
    virtual OGRErr      SetFID( GIntBig nFIDIn );

    void                DumpReadable( FILE *, char** papszOptions = NULL );

    OGRErr              SetFrom( OGRFeature *, int = TRUE);
    OGRErr              SetFrom( OGRFeature *, int *, int = TRUE );
    OGRErr              SetFieldsFrom( OGRFeature *, int *, int = TRUE ); 

    OGRErr              RemapFields( OGRFeatureDefn *poNewDefn, 
                                     int *panRemapSource );
    OGRErr              RemapGeomFields( OGRFeatureDefn *poNewDefn, 
                                     int *panRemapSource );

    int                 Validate( int nValidateFlags,
                                  int bEmitError );
    void                FillUnsetWithDefault(int bNotNullableOnly,
                                             char** papszOptions );

    virtual const char *GetStyleString();
    virtual void        SetStyleString( const char * );
    virtual void        SetStyleStringDirectly( char * );
    virtual OGRStyleTable *GetStyleTable() { return m_poStyleTable; }
    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);
    virtual void        SetStyleTableDirectly(OGRStyleTable *poStyleTable);

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

    GIntBig       *EvaluateAgainstIndices( swq_expr_node*, OGRLayer *, GIntBig& nFIDCount);
    
    int         CanUseIndex( swq_expr_node*, OGRLayer * );
    
  public:
                OGRFeatureQuery();
                ~OGRFeatureQuery();

    OGRErr      Compile( OGRFeatureDefn *, const char * );
    int         Evaluate( OGRFeature * );

    GIntBig       *EvaluateAgainstIndices( OGRLayer *, OGRErr * );
    
    int         CanUseIndex( OGRLayer * );

    char      **GetUsedFields();

    void       *GetSWQExpr() { return pSWQExpr; }
};

#endif /* ndef _OGR_FEATURE_H_INCLUDED */
