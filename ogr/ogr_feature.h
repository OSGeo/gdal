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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.34  2006/09/20 13:01:51  osemykin
 * Make the functions GetFieldAsStringList(...) and IsFieldSet(...) const
 *
 * Revision 1.33  2006/04/02 18:25:59  fwarmerdam
 * added OFTDateTime, and OFTTime support
 *
 * Revision 1.32  2006/02/15 04:25:37  fwarmerdam
 * added date support
 *
 * Revision 1.31  2005/09/21 00:50:08  fwarmerdam
 * Added Release
 *
 * Revision 1.30  2005/08/30 23:52:35  fwarmerdam
 * implement preliminary OFTBinary support
 *
 * Revision 1.29  2004/02/23 21:47:23  warmerda
 * Added GetUsedFields() and GetSWGExpr() methods on OGRFeatureQuery class
 *
 * Revision 1.28  2003/05/28 19:16:42  warmerda
 * fixed up argument names and stuff for docs
 *
 * Revision 1.27  2003/04/08 20:57:28  warmerda
 * added RemapFields on OGRFeature
 *
 * Revision 1.26  2003/03/04 05:46:31  warmerda
 * added EvaluateAgainstIndices for OGRFeatureQuery
 *
 * Revision 1.25  2003/01/08 22:03:44  warmerda
 * added StealGeometry() method on OGRFeature
 *
 * Revision 1.24  2002/10/09 14:31:06  warmerda
 * dont permit negative widths to be assigned to field definition
 *
 * Revision 1.23  2002/09/26 18:13:17  warmerda
 * moved some defs to ogr_core.h for sharing with ogr_api.h
 *
 * Revision 1.22  2002/08/07 21:37:47  warmerda
 * added indirect OGRFeaturedefn constructor/destructor
 *
 * Revision 1.21  2001/11/01 16:54:16  warmerda
 * added DestroyFeature
 *
 * Revision 1.20  2001/07/19 18:25:07  warmerda
 * expanded tabs
 *
 * Revision 1.19  2001/06/19 15:48:36  warmerda
 * added feature attribute query support
 *
 * Revision 1.18  2001/06/01 14:33:00  warmerda
 * added CreateFeature factory method
 *
 * Revision 1.17  2001/02/06 17:10:28  warmerda
 * export entry points from DLL
 *
 * Revision 1.16  2001/01/19 21:10:47  warmerda
 * replaced tabs
 *
 * Revision 1.15  2000/12/07 03:40:13  danmo
 * Removed stray comma in OGRFieldType enum defn
 *
 * Revision 1.14  2000/10/03 19:19:56  danmo
 * Made m_pszStyleString protected (was private)
 *
 * Revision 1.13  2000/10/03 18:14:29  danmo
 * Made OGRFeature::Get/SetStyleString() virtual
 *
 * Revision 1.12  2000/08/18 21:26:53  svillene
 * Add representation
 *
 * Revision 1.11  1999/11/26 03:05:38  warmerda
 * added unset field support
 *
 * Revision 1.10  1999/11/18 19:02:20  warmerda
 * expanded tabs
 *
 * Revision 1.9  1999/11/04 21:05:49  warmerda
 * Added the Set() method on OGRFieldDefn to set all info in one call,
 * and the SetFrom() method on OGRFeature to copy the contents of one
 * feature to another, even if of a different OGRFeatureDefn.
 *
 * Revision 1.8  1999/10/01 14:47:05  warmerda
 * added full family of get/set field methods with field names
 *
 * Revision 1.7  1999/08/30 14:52:33  warmerda
 * added support for StringList fields
 *
 * Revision 1.6  1999/08/26 17:38:00  warmerda
 * added support for real and integer lists
 *
 * Revision 1.5  1999/07/27 00:47:37  warmerda
 * Added FID to OGRFeature class
 *
 * Revision 1.4  1999/07/07 04:23:07  danmo
 * Fixed typo in  #define _OGR_..._H_INCLUDED  line
 *
 * Revision 1.3  1999/07/05 17:18:39  warmerda
 * added docs
 *
 * Revision 1.2  1999/06/11 19:21:27  warmerda
 * Fleshed out operational definitions
 *
 * Revision 1.1  1999/05/31 17:14:53  warmerda
 * New
 *
 */

#ifndef _OGR_FEATURE_H_INCLUDED
#define _OGR_FEATURE_H_INCLUDED

#include "ogr_geometry.h"

class OGRStyleTable;

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
    int         nRefCount;
    
    int         nFieldCount;
    OGRFieldDefn **papoFieldDefn;

    OGRwkbGeometryType eGeomType;

    char        *pszFeatureClassName;
    
  public:
                OGRFeatureDefn( const char * pszName = NULL );
    virtual    ~OGRFeatureDefn();

    const char  *GetName() { return pszFeatureClassName; }

    int         GetFieldCount() { return nFieldCount; }
    OGRFieldDefn *GetFieldDefn( int i );
    int         GetFieldIndex( const char * );

    void        AddFieldDefn( OGRFieldDefn * );

    OGRwkbGeometryType GetGeomType() { return eGeomType; }
    void        SetGeomType( OGRwkbGeometryType );

    OGRFeatureDefn *Clone();

    int         Reference() { return ++nRefCount; }
    int         Dereference() { return --nRefCount; }
    int         GetReferenceCount() { return nRefCount; }
    void        Release();

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

    int                 IsFieldSet( int iField ) const
                        { return
                              pauFields[iField].Set.nMarker1 != OGRUnsetMarker
                           || pauFields[iField].Set.nMarker2 != OGRUnsetMarker;
                              }
    
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

    void                DumpReadable( FILE * );

    OGRErr              SetFrom( OGRFeature *, int = TRUE);

    OGRErr              RemapFields( OGRFeatureDefn *poNewDefn, 
                                     int *panRemapSource );

    virtual const char *GetStyleString();
    virtual void        SetStyleString(const char *);
    virtual void        SetStyleTable(OGRStyleTable *poStyleTable);

    static OGRFeature  *CreateFeature( OGRFeatureDefn * );
    static void         DestroyFeature( OGRFeature * );
};

/************************************************************************/
/*                           OGRFeatureQuery                            */
/************************************************************************/

class OGRLayer;

class CPL_DLL OGRFeatureQuery
{
  private:
    OGRFeatureDefn *poTargetDefn;
    void           *pSWQExpr;

    char          **FieldCollector( void *, char ** );
    
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
