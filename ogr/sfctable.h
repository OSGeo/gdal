/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  SFCTable class, client side abstraction for an OLE DB spatial
 *           table based on ATL CTable. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Les Technologies SoftMap Inc.
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
 ****************************************************************************/

#ifndef SFCTABLE_H_INCLUDED
#define SFCTABLE_H_INCLUDED

#include <atldbcli.h>

class OGRFeature;
class OGRFeatureDefn;
class OGRGeometry;
class OGRSpatialReference;

/************************************************************************/
/*                               SFCTable                               */
/************************************************************************/

/**
 * Abstract representation of a rowset (table) with spatial features.
 *
 * This class is intended to simplify access to spatial rowsets, and to
 * centralize all the rules for selecting geometry columns, getting the
 * spatial reference system of a rowset, and special feature access short
 * cuts with selected providers.  It is based on the ATL CTable class
 * with a dynamic accessor. 
 */

class SFCTable : public CTable<CDynamicAccessor>
{
  private:
    int         bTriedToIdentify;
    int         iBindColumn;       
    int         iGeomColumn;       /* -1 means there is none
                                      this is paoColumnInfo index, not ord. */

    void        IdentifyGeometry(); /* find the geometry column */

    BYTE        *pabyLastGeometry;

    int         nGeomType;
    ULONG       nSRS_ID;

    int         ReadOGISColumnInfo( CSession * poCSession,
                                    const char * pszColumnName = NULL );
    int         FetchDefGeomColumn( CSession * poCSession );

    char        *pszTableName;
    char        *pszDefGeomColumn;

    OGRSpatialReference * poSRS;

    OGRFeatureDefn * poDefn;
    ULONG        *panColOrdinal;

  public:
                SFCTable();
    virtual     ~SFCTable();

    HRESULT     OpenFromRowset( IRowset * pIRowset );
    
    HRESULT     Open( const CSession& session, DBID& dbid,
                      DBPROPSET* pPropSet = NULL );

    void        SetTableName( const char * );
    const char *GetTableName();
    
    int         ReadSchemaInfo( CDataSource *, CSession * = NULL );

    void        ReleaseIUnknowns();
    
    int         GetSpatialRefID();
    OGRSpatialReference *GetSpatialRef() { return poSRS; }

    int         GetGeometryColumn();

    int         HasGeometry();

    int         GetGeometryType();

    BYTE        *GetWKBGeometry( int * pnSize );

    OGRGeometry *GetOGRGeometry();

    OGRFeature  *GetOGRFeature();

    OGRFeatureDefn *GetOGRFeatureDefn();
};

#endif /* ndef SFCTABLE_H_INCLUDED */
