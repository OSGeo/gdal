/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements the SFCTable class, a spatially extended table
 *           ATL CTable<CDynamicAccessor>.
 *
 * Author:   Frank Warmerdam, warmerda@home.com
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
 * Revision 1.11  1999/09/07 14:05:06  warmerda
 * Ignore bPrecision for real/double fields as it's interpretation is
 * unclear.  Use it as the field width for integers.  Note many types are
 * missing from the type interpreation switch statement.
 *
 * Revision 1.10  1999/09/01 12:08:34  warmerda
 * Ensure the geometry type gets set on the OGRFeatureDefn.
 *
 * Revision 1.9  1999/07/09 13:05:01  warmerda
 * figure out geometry column before building OGRFeatureDefn
 *
 * Revision 1.8  1999/07/08 20:27:25  warmerda
 * Implemented creation on an OGRFeatureDefn for the layer, and
 * implemented GetOGRFeature().
 *
 * Revision 1.7  1999/06/26 05:34:17  warmerda
 * Added support for poSRS, and use creating geometry
 *
 * Revision 1.6  1999/06/10 19:21:20  warmerda
 * Fixed doc typo.
 *
 * Revision 1.5  1999/06/10 19:18:22  warmerda
 * added support for the spatial ref schema rowset
 *
 * Revision 1.4  1999/06/10 14:00:15  warmerda
 * Add request for IStream from IUnknowns that don't give an ISequentialStream.
 *
 * Revision 1.3  1999/06/08 17:50:20  warmerda
 * Fixed some off-by-one errors, and updated bytes|byref case.
 *
 * Revision 1.2  1999/06/08 15:41:16  warmerda
 * added working blob/geometry support
 *
 * Revision 1.1  1999/06/08 03:50:43  warmerda
 * New
 *
 */

#include "sfctable.h"
#include "sfcschemarowsets.h"
#include "sfcdatasource.h"
#include "ogr_feature.h"
#include "assert.h"
#include "oledb_sup.h"
#include "cpl_string.h"

/************************************************************************/
/*                              SFCTable()                              */
/************************************************************************/
 
SFCTable::SFCTable()

{
    iGeomColumn = -1;
    bTriedToIdentify = FALSE;

    pabyLastGeometry = NULL;
    
    nSRS_ID = -1;
    nGeomType = wkbUnknown;
    poSRS = NULL;

    poDefn = NULL;
    panColOrdinal = NULL;

    pszTableName = NULL;
    pszDefGeomColumn = NULL;
}

/************************************************************************/
/*                             ~SFCTable()                              */
/************************************************************************/

SFCTable::~SFCTable()

{
    if( poDefn != NULL )
        delete poDefn;

    CPLFree( panColOrdinal );
    CPLFree( pszTableName );
    CPLFree( pszDefGeomColumn );

    if( pabyLastGeometry != NULL )
        CoTaskMemFree( pabyLastGeometry );

    if( poSRS != NULL )
        delete poSRS;
}

/************************************************************************/
/*                            GetTableName()                            */
/************************************************************************/

/** 
 * Get the name of this rowsets table.
 *
 * @return a pointer to an internal table name.  May be NULL if not known.
 * Should not be modified or freed by the application.
 */

const char * SFCTable::GetTableName()

{
    return pszTableName;
}

/************************************************************************/
/*                            SetTableName()                            */
/************************************************************************/

/** 
 * Set the table name.
 *
 * This is primarily needed if the SFCTable is created by means other
 * than SFCDataSource::CreateSFCTable().  The table name is needed to
 * collect information from the ogis columns schema rowset. 
 *
 * @param pszTableName the name of the table from which this SFCTable is 
 * derived. 
 */

void SFCTable::SetTableName( const char * pszTableName )

{
    CPLFree( this->pszTableName );
    this->pszTableName = CPLStrdup( pszTableName );
}

/************************************************************************/
/*                           ReadSchemaInfo()                           */
/*                                                                      */
/*      At some point in the future we might want to actually keep      */
/*      our session around for the life of the SFCTable so we can       */
/*      use it for other things.                                        */
/************************************************************************/

/**
 * Read required schema rowset information.
 * 
 * This method is normally called by SFCDataSource::CreateSFCTable(), but
 * if the SFCTable is created by another means, it is necessary so that
 * the SFCTable can get information from the schema rowsets about the
 * geometry column, SRS and so forth.  
 *
 * If an SFCTable is instantiated wihtout this method ever being called
 * a number of the OpenGIS related aspects of the table will not be
 * operational.
 *
 * @param poDS a CDataSource (or SFCDataSource) on which this SFCTable
 * was created.
 *
 * @return TRUE if reading of schema information succeeds. 
 */

int SFCTable::ReadSchemaInfo( CDataSource * poDS )

{
/* -------------------------------------------------------------------- */
/*      Read the geometry column information.                           */
/* -------------------------------------------------------------------- */
    CSession      oSession;
    int           bSuccess = TRUE;

    if( FAILED(oSession.Open( *poDS ) ) )
        bSuccess = FALSE;
    else if( !FetchDefGeomColumn( &oSession ) )
        bSuccess = FALSE;
    else
        bSuccess = ReadOGISColumnInfo( &oSession );

    HasGeometry();

/* -------------------------------------------------------------------- */
/*      Prepare a definition for the columns of this table as an        */
/*      OGRFeatureDefn.                                                 */
/* -------------------------------------------------------------------- */
    poDefn = new OGRFeatureDefn( GetTableName() );
    poDefn->SetGeomType( (OGRwkbGeometryType) GetGeometryType() );
    panColOrdinal = (ULONG *) CPLMalloc(sizeof(ULONG) * GetColumnCount());

    for( ULONG iColumn = 0; iColumn < GetColumnCount(); iColumn++ )
    {
        DBCOLUMNINFO*     psCInfo = m_pColumnInfo + iColumn;
        OGRFieldDefn      oField( "", OFTInteger );
        char              *pszName = NULL;

        if( psCInfo->iOrdinal == (ULONG) iGeomColumn )
            continue;

        UnicodeToAnsi( psCInfo->pwszName, &pszName );
        oField.SetName( pszName );
        CoTaskMemFree( pszName );
        pszName = NULL;

        switch( psCInfo->wType )
        {
            case DBTYPE_I4:
                oField.SetType( OFTInteger );
                if( psCInfo->bPrecision != 255 )
                    oField.SetWidth( psCInfo->bPrecision );
                else
                    oField.SetWidth( 11 );
                break;

            case DBTYPE_R4:
                oField.SetType( OFTReal );
                // for now we ignore the provided precision information
                // because we aren't sure how to interprete it.
                break;

            case DBTYPE_R8:
                oField.SetType( OFTReal );
                // for now we ignore the provided precision information
                // because we aren't sure how to interprete it.
                break;

            case DBTYPE_STR:
                oField.SetType( OFTString );
                if( psCInfo->ulColumnSize < 100000 )
                    oField.SetWidth( psCInfo->ulColumnSize );
                break;

            default:
                oField.SetType( OFTString );
                oField.SetWidth( 1 );
                break;
        }

        poDefn->AddFieldDefn( &oField );
        panColOrdinal[poDefn->GetFieldCount()-1] = psCInfo->iOrdinal;
    }

    return bSuccess;
}

/************************************************************************/
/*                         FetchDefGeomColumn()                         */
/*                                                                      */
/*      Try to get the default geometry column from the feature         */
/*      tables schema rowset.                                           */
/************************************************************************/

int SFCTable::FetchDefGeomColumn( CSession * poSession )

{
    COGISFeatureTables oTables;

    if( pszTableName == NULL )
        return FALSE;

    if( FAILED(oTables.Open(*poSession)) )
    {
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Search for a matching table name.                               */
/* -------------------------------------------------------------------- */
    while( oTables.MoveNext() == S_OK )
    {
        if( EQUAL(oTables.m_szName, pszTableName) )
        {
            CPLFree( pszDefGeomColumn );
            pszDefGeomColumn = CPLStrdup( oTables.m_szDGColumnName );
        }
    }

    if( pszDefGeomColumn == NULL )
    {
        CPLDebug( "SFC", "Failed to find table `%s' in COGISFeatureTables.\n", 
                  pszTableName );
    }
    
    return pszDefGeomColumn != NULL;
}

/************************************************************************/
/*                         ReadOGISColumnInfo()                         */
/*                                                                      */
/*      Read information about a geometry column for this table in      */
/*      the OGIS column info schema rowset.                             */
/************************************************************************/

int SFCTable::ReadOGISColumnInfo( CSession * poSession, 
                                  const char * pszColumnName )

{
/* -------------------------------------------------------------------- */
/*      If we have no table name, we can't do anything.  Eventually     */
/*      we could try to use the IRowsetInfo interface to fetch the      */
/*      table name property, but that's too much work for now.          */
/* -------------------------------------------------------------------- */
    if( pszTableName == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      If we aren't given a column name, then we will get the          */
/*      default column from the schema.  We coul                        */
/* -------------------------------------------------------------------- */
    if( pszColumnName == NULL )
        pszColumnName = pszDefGeomColumn;

    if( pszColumnName == NULL )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Open the column info schema rowset, and try to find a match     */
/*      for our table and column.                                       */
/* -------------------------------------------------------------------- */
    COGISGeometryColumnTable oColumns;

    if( FAILED(oColumns.Open(*poSession)) )
    {
        return FALSE;
    }

    while( oColumns.MoveNext() == S_OK )
    {
        if( EQUAL(oColumns.m_szName, pszTableName)
            && EQUAL(oColumns.m_szColumnName,pszColumnName) )
        {
            char      *pszSRS_WKT, *pszWKTCopy;

            nSRS_ID = oColumns.m_nSRS_ID;
            nGeomType = oColumns.m_nGeomType;

            if( poSRS != NULL )
            {
                if( poSRS->Dereference() == 0 )
                    delete poSRS;
            }

            pszSRS_WKT = SFCDataSource::GetWKTFromSRSId( poSession, nSRS_ID );
            poSRS = new OGRSpatialReference();
            pszWKTCopy = pszSRS_WKT;
            if( poSRS->importFromWkt( &pszWKTCopy ) != OGRERR_NONE )
            {
                delete poSRS;
                poSRS = NULL;
            }

            CoTaskMemFree( pszSRS_WKT );
        }
    }

    if( nSRS_ID == -1 )
    {
        CPLDebug( "SFC", 
                 "Failed to find %s/%s in COGISGeometryColumnTable, no SRS.\n",
                  pszTableName, pszColumnName );
    }

    return nSRS_ID != -1;
}

/************************************************************************/
/*                            HasGeometry()                             */
/************************************************************************/

/**
 * Does this table have geometry?
 *
 * @return this method returns TRUE if this table has an identifiable 
 * geometry column.
 */

int SFCTable::HasGeometry() 
{
    if( m_spRowset == NULL )
        return FALSE;

    if( !bTriedToIdentify )
        IdentifyGeometry();

    return iGeomColumn != -1;
}

/************************************************************************/
/*                          IdentifyGeometry()                          */
/*                                                                      */
/*      This method should eventually try to use the table of           */
/*      spatial tables to identify the geometry column - only           */
/*      falling back to column names if that fails.                     */
/************************************************************************/

void SFCTable::IdentifyGeometry()

{
    ULONG    iCol;

    if( m_spRowset == NULL || bTriedToIdentify )
        return;

    bTriedToIdentify = TRUE;

/* -------------------------------------------------------------------- */
/*      If we have a default geometry column name from the schema       */
/*      rowsets, search for it.                                         */
/* -------------------------------------------------------------------- */
    if( pszDefGeomColumn != NULL )
    {
        LPOLESTR      pwszColumnName;
        AnsiToUnicode( pszDefGeomColumn, &pwszColumnName );

        for( iCol = 1; iCol <= GetColumnCount(); iCol++ )
        {
            if( GetColumnName(iCol) == NULL )
                continue;
            
            if( wcsicmp( pwszColumnName, GetColumnName(iCol) ) == 0 )
                break;
        }

        CoTaskMemFree( pwszColumnName );
    }

/* -------------------------------------------------------------------- */
/*      Search for a column called OGIS_GEOMETRY (officially            */
/*      preferred name) or WKB_GEOMETRY (one provided sample            */
/*      database).                                                      */
/* -------------------------------------------------------------------- */
    else
    {
        for( iCol = 1; iCol <= GetColumnCount(); iCol++ )
        {
            if( GetColumnName(iCol) == NULL )
                continue;
            
            if( wcsicmp( L"WKB_GEOMETRY", GetColumnName(iCol) ) == 0 
                || wcsicmp( L"OGIS_GEOMETRY", GetColumnName(iCol) ) == 0 )
                break;
        }
    }

    if( iCol > GetColumnCount() )
        return;

/* -------------------------------------------------------------------- */
/*      Verify that the type is acceptable.                             */
/* -------------------------------------------------------------------- */
    DBTYPE      nType;

    if( !GetColumnType(iCol, &nType) 
        || (nType != DBTYPE_BYTES
            && nType != DBTYPE_IUNKNOWN
            && nType != (DBTYPE_BYTES | DBTYPE_BYREF)) )
        return;

/* -------------------------------------------------------------------- */
/*      We have found a column that looks OK.                           */
/* -------------------------------------------------------------------- */
    iGeomColumn = iCol;
}

/************************************************************************/
/*                           GetWKBGeometry()                           */
/************************************************************************/

/** 
 * Fetch geometry binary column binary data.
 *
 * Note that the returned pointer is to an internal buffer, and will
 * be invalidated by the next record read operation.  The data should
 * not be freed or modified.
 *
 * @param pnSize pointer to an integer into which the number of bytes 
 * returned may be put.  This may be NULL.
 *
 * @return a pointer to the binary data or NULL if the fetch fails.
 */

BYTE *SFCTable::GetWKBGeometry( int * pnSize )

{
    if( pnSize != NULL )
        *pnSize = 0;

    if( !HasGeometry() )
    {
        return NULL;
    }

    /* do we have a record?  How to test? */

/* -------------------------------------------------------------------- */
/*      Cleanup the previous passes geometry binary data, if necessary. */
/* -------------------------------------------------------------------- */
    if( pabyLastGeometry != NULL )
    {
        CoTaskMemFree( pabyLastGeometry );
        pabyLastGeometry = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Get the geometry column datatype.                               */
/* -------------------------------------------------------------------- */
    DBTYPE      nGeomType;

    GetColumnType( iGeomColumn, &nGeomType );

/* -------------------------------------------------------------------- */
/*      If the column is bound as DBTYPE_BYTES, just return a           */
/*      pointer to the internal buffer.  We also check to see if the    */
/*      data was truncated.  If so we emit a debugging error            */
/*      message, but otherwise try to continue.                         */
/* -------------------------------------------------------------------- */
    if( nGeomType == DBTYPE_BYTES)
    {
        if( pnSize != NULL )
        {
            ULONG       dwLength;
            GetLength( iGeomColumn, &dwLength );
            *pnSize = dwLength;
        }

        return( (BYTE *) GetValue(iGeomColumn) );
    }

/* -------------------------------------------------------------------- */
/*      If the column is bound as DBTYPE_BYTES and DBTYPE_BYREF then    */
/*      the pointer is contained in the data.                           */
/* -------------------------------------------------------------------- */
    if( nGeomType == (DBTYPE_BYTES|DBTYPE_BYREF) )
    {
        BYTE *pRetVal;

        // note: this case hasn't been tested since it was adapted from
        // oledbsup_sf.cpp

        GetValue( iGeomColumn, &pRetVal );

        if( pnSize != NULL )
        {
            ULONG      dwLength;
            GetLength( iGeomColumn, &dwLength );
            *pnSize = dwLength;
        }
            
        return(pRetVal);
    }

/* -------------------------------------------------------------------- */
/*      The remaining case is that the column is bound as an            */
/*      IUnknown, and we need to get an IStream interface for it to     */
/*      read the data.                                                  */
/* -------------------------------------------------------------------- */
    IUnknown * pIUnknown;
    ISequentialStream *  pIStream = NULL;
    HRESULT    hr;

    assert( nGeomType == DBTYPE_IUNKNOWN );

    GetValue( iGeomColumn, &pIUnknown );
            
    hr = pIUnknown->QueryInterface( IID_ISequentialStream,
                                    (void**)&pIStream );

    // for some reason the Cadcorp provider can return an IStream but
    // not an ISequentialStream!
    if( FAILED(hr) )
    {
        hr = pIUnknown->QueryInterface( IID_IStream,
                                        (void**)&pIStream );
    }
    
    if( FAILED(hr) )
    {
        DumpErrorHResult( hr, "Can't get IStream interface to geometry" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Read data in chunks, reallocating buffer larger as needed.      */
/* -------------------------------------------------------------------- */
    BYTE      abyChunk[32];
    ULONG     nBytesRead;
    int       nSize;
    
    nSize = 0;
    do 
    {
        pIStream->Read( abyChunk, sizeof(abyChunk), &nBytesRead );
        if( nBytesRead > 0 )
        {
            nSize += nBytesRead;
            pabyLastGeometry = (BYTE *) 
                CoTaskMemRealloc(pabyLastGeometry, nSize);

            memcpy( pabyLastGeometry + nSize - nBytesRead, 
                    abyChunk, nBytesRead );
        }
    }
    while( nBytesRead == sizeof(abyChunk) );
    
    pIStream->Release();

/* -------------------------------------------------------------------- */
/*      Return the number of bytes read, if requested.                  */
/* -------------------------------------------------------------------- */
    if( pnSize != NULL && pabyLastGeometry != NULL )
        *pnSize = nSize;

    return pabyLastGeometry;
}

/************************************************************************/
/*                          ReleaseIUnknowns()                          */
/************************************************************************/

/** 
 * Release any IUnknowns in current record. 
 *
 * It is very important that this be called once, and only once for each
 * record read on an SFCTable if there may be IUnknowns (generally 
 * ISequentialStreams for the geometry column).  
 *
 * Unfortunately, the CRowset::ReleaseRows() doesn't take care of this
 * itself.  
 */

void SFCTable::ReleaseIUnknowns()

{
    for( ULONG i = 0; i < GetColumnCount(); i++ )
    {
        DBTYPE      nType;

        GetColumnType( i, &nType );
        if( nType == DBTYPE_IUNKNOWN )
        {
            IUnknown      *pIUnknown;

            GetValue( i, &pIUnknown );
            
            if( pIUnknown != NULL )
                pIUnknown->Release();
        } 
    }
}

/************************************************************************/
/*                           GetOGRGeometry()                           */
/************************************************************************/

/**
 * Fetch the OGRGeometry for this record.
 *
 * The reading of the BLOB column, and translation into an OGRGeometry
 * subclass is handled automatically.  The returned object becomes the
 * responsibility of the caller and should be destroyed with delete.
 *
 * @return pointer to an OGRGeometry, or NULL if the read fails.
 */

OGRGeometry * SFCTable::GetOGRGeometry()

{
    BYTE      *pabyData;
    int       nBytesRead;
    OGRGeometry *poGeom;

    pabyData = GetWKBGeometry( &nBytesRead );
    if( pabyData == NULL )
        return NULL;

    if( OGRGeometryFactory::createFromWkb( pabyData, poSRS, &poGeom, 
                                           nBytesRead ) == OGRERR_NONE )
        return poGeom;
    else
        return NULL;
}

/************************************************************************/
/*                         GetOGRFeatureDefn()                          */
/************************************************************************/

OGRFeatureDefn *SFCTable::GetOGRFeatureDefn()

{
    return poDefn;
}

/************************************************************************/
/*                           GetOGRFeature()                            */
/************************************************************************/

OGRFeature *SFCTable::GetOGRFeature()

{
    OGRFeature      *poFeature;

    if( poDefn == NULL )
        return NULL;

    poFeature = new OGRFeature( poDefn );
    poFeature->SetGeometryDirectly( GetOGRGeometry() );

    for( int iColumn = 0; iColumn < poDefn->GetFieldCount(); iColumn++ )
    {
        ULONG             iColOrdinal = panColOrdinal[iColumn];
        DBTYPE            wType;

        GetColumnType( iColOrdinal, &wType );
        switch( wType )
        {
            case DBTYPE_I4:
                int            nValue;
                GetValue( iColOrdinal, &nValue );
                poFeature->SetField( iColumn, nValue );
                break;

            case DBTYPE_R4:
                float      fValue;
                GetValue( iColOrdinal, &fValue );
                poFeature->SetField( iColumn, (double) fValue );
                break;

            case DBTYPE_R8:
                double      dfValue;
                GetValue( iColOrdinal, &dfValue );
                poFeature->SetField( iColumn, dfValue );
                break;

            case DBTYPE_STR:
                char        *pszValue;
                ULONG       nLength;

                GetLength( iColOrdinal, &nLength );
                pszValue = (char *) CPLMalloc(nLength+1);
                strncpy( pszValue, (const char *) GetValue(iColOrdinal), 
                         nLength );
                pszValue[nLength] = '\0';

                poFeature->SetField( iColumn, pszValue );

                CPLFree( pszValue );
                break;

            default:
                poFeature->SetField( iColumn, "" );
                break;
        }
    }

    return poFeature;
}

/************************************************************************/
/*                          GetGeometryType()                           */
/************************************************************************/

/**
 * Fetch the geometry type of this table.
 *
 * This method returns the well known binary type of the geometry in
 * this table.  This integer can be cast to the type OGRwkbGeometryType
 * in order to use symbolic constants.  The intent is that all objects
 * in this table would be of the returned class, or a derived class. 
 * It will generally be zero (wkbUnknown) if nothing is known about the
 * geometry types in the table.
 *
 * Zero (wkbUnknown) will be returned if there is no column info schema
 * rowset (from which this value is normally extracted). 
 *
 * @return well known geometry type.
 */ 

int SFCTable::GetGeometryType()

{
    return nGeomType;
}

/************************************************************************/
/*                          GetSpatialRefID()                           */
/************************************************************************/

/**
 * Fetch the spatial reference system id of this table.
 *
 * This method returns the id of the spatial reference system for this
 * table.  All geometries in this table should have this spatial reference
 * system.  The SFCDataSource::GetWKTFromSRSId() method can be used to
 * transform this id into a useful form. 
 *
 * @return spatial reference system id.  The value will be -1 if not known.
 */ 

int SFCTable::GetSpatialRefID()

{
    return nSRS_ID;
}

