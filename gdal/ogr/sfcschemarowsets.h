/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Definitions for OGIS specific, and generic OLE DB schema rowsets.
 *           Generally application code shouldn't need to include this file
 *           directly or indirectly.  It exists mostly to support the other
 *           SFC class implementations.
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
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef SFCSCHEMAROWSETS_H_INCLUDED
#define SFCSCHEMAROWSETS_H_INCLUDED

#include <atldbsch.h>

#include "oledbgis.h"

/************************************************************************/
/*                        COGISFeatureTableInfo                         */
/*                                                                      */
/*      Hardbound record for the OGIS Feature Tables schema rowset.     */
/*      Modelled on CTableInfo.                                         */
/************************************************************************/
class COGISFeatureTableInfo
{
public:
// Constructors
        COGISFeatureTableInfo()
        {
                memset(this, 0, sizeof(*this));
        }

// Attributes

        TCHAR   m_szFeatureTableAlias[129];
        TCHAR   m_szCatalog[129];
        TCHAR   m_szSchema[129];
        TCHAR   m_szName[129];
        TCHAR   m_szIdColumnName[129];
        TCHAR   m_szDGColumnName[129];


// Binding Map
BEGIN_COLUMN_MAP(COGISFeatureTableInfo)
        COLUMN_ENTRY(1, m_szFeatureTableAlias)
        COLUMN_ENTRY(2, m_szCatalog)
        COLUMN_ENTRY(3, m_szSchema)
        COLUMN_ENTRY(4, m_szName)
        COLUMN_ENTRY(5, m_szIdColumnName)
        COLUMN_ENTRY(6, m_szDGColumnName)
END_COLUMN_MAP()
};

class COGISFeatureTables:
      public CSchemaRowset<CAccessor<COGISFeatureTableInfo>,0>

{
  public:
    HRESULT Open(const CSession& session, bool bBind = true )
    {
        USES_CONVERSION;
        return CSchemaRowset<CAccessor<COGISFeatureTableInfo>,0>::
            Open(session, DBSCHEMA_OGIS_FEATURE_TABLES /*, bBind */);
    }
};

/************************************************************************/
/*                       COGISGeometryColumnInfo                        */
/*                                                                      */
/*      Hardbound record for the OGIS column info schema rowset.        */
/*      Modelled on CTableInfo.                                         */
/************************************************************************/

class COGISGeometryColumnInfo
{
public:
// Constructors
        COGISGeometryColumnInfo()
        {
                memset(this, 0, sizeof(*this));
        }

// Attributes

        TCHAR   m_szCatalog[129];
        TCHAR   m_szSchema[129];
        TCHAR   m_szName[129];
        TCHAR   m_szColumnName[129];
        ULONG   m_nGeomType;
        ULONG   m_nSRS_ID;

// Binding Map
BEGIN_COLUMN_MAP(COGISGeometryColumnInfo)
        COLUMN_ENTRY(1, m_szCatalog)
        COLUMN_ENTRY(2, m_szSchema)
        COLUMN_ENTRY(3, m_szName)
        COLUMN_ENTRY(4, m_szColumnName)
        COLUMN_ENTRY(5, m_nGeomType)
        COLUMN_ENTRY(6, m_nSRS_ID)
END_COLUMN_MAP()
};

class COGISGeometryColumnTable:
      public CSchemaRowset<CAccessor<COGISGeometryColumnInfo>,0>

{
  public:
    HRESULT Open(const CSession& session, bool bBind = true )
    {
        USES_CONVERSION;
        return CSchemaRowset<CAccessor<COGISGeometryColumnInfo>,0>::
            Open(session, DBSCHEMA_OGIS_GEOMETRY_COLUMNS/*, bBind*/);
    }
};

/************************************************************************/
/*                      COGISSpatialRefSystemsInfo                      */
/*                                                                      */
/*      Hardbound record for the OGIS SRS schema rowset record.         */
/*      Modelled on CTableInfo.                                         */
/************************************************************************/

class COGISSpatialRefSystemsInfo
{
public:
// Constructors
        COGISSpatialRefSystemsInfo()
        {
                memset(this, 0, sizeof(*this));
        }

// Attributes

        ULONG   m_nSRS_ID;
        TCHAR   m_szAuthorityName[129];
        ULONG   m_nAuthorityID;
        TCHAR   m_szSpatialRefSystemWKT[2048];

// Binding Map
BEGIN_COLUMN_MAP(COGISSpatialRefSystemsInfo)
        COLUMN_ENTRY(1, m_nSRS_ID)
        COLUMN_ENTRY(2, m_szAuthorityName)
        COLUMN_ENTRY(3, m_nAuthorityID)
        COLUMN_ENTRY(4, m_szSpatialRefSystemWKT)
END_COLUMN_MAP()
};

class COGISSpatialRefSystemsTable:
      public CSchemaRowset<CAccessor<COGISSpatialRefSystemsInfo>,0>

{
  public:
    HRESULT Open(const CSession& session, bool bBind = true )
    {
        USES_CONVERSION;
        return CSchemaRowset<CAccessor<COGISSpatialRefSystemsInfo>,0>::
            Open(session, DBSCHEMA_OGIS_SPATIAL_REF_SYSTEMS /*, bBind */);
    }
};

#endif /* ndef SFCSCHEMAROWSETS_H_INCLUDED */
