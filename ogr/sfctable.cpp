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
 * Revision 1.4  1999/06/10 14:00:15  warmerda
 * Added request for IStream from IUnknowns that don't give an ISequentialStream.
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
#include "ogr_geometry.h"
#include "assert.h"
#include "oledb_sup.h"

/************************************************************************/
/*                              SFCTable()                              */
/************************************************************************/
 
SFCTable::SFCTable()

{
    iGeomColumn = -1;
    bTriedToIdentify = FALSE;

    pabyLastGeometry = NULL;
}

/************************************************************************/
/*                             ~SFCTable()                              */
/************************************************************************/

SFCTable::~SFCTable()

{
    if( pabyLastGeometry != NULL )
        CoTaskMemFree( pabyLastGeometry );
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
/*                                                                      */
/************************************************************************/

void SFCTable::IdentifyGeometry()

{
    if( m_spRowset == NULL || bTriedToIdentify )
        return;

    bTriedToIdentify = TRUE;

/* -------------------------------------------------------------------- */
/*      Search for a column called OGIS_GEOMETRY (officially            */
/*      preferred name) or WKB_GEOMETRY (one provided sample            */
/*      database).                                                      */
/* -------------------------------------------------------------------- */
    for( int iCol = 1; iCol <= GetColumnCount(); iCol++ )
    {
        if( GetColumnName(iCol) == NULL )
            continue;

        if( wcsicmp( L"WKB_GEOMETRY", GetColumnName(iCol) ) == 0 
         || wcsicmp( L"OGIS_GEOMETRY", GetColumnName(iCol) ) == 0 )
            break;
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
    for( int i = 0; i < GetColumnCount(); i++ )
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

    if( OGRGeometryFactory::createFromWkb( pabyData, NULL, &poGeom, 
                                           nBytesRead ) == OGRERR_NONE )
        return poGeom;
    else
        return NULL;
}

