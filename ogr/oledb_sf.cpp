/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements the OledbSFTable class.  An abstraction on top of
 *           OledbSupRowset to make it easy to extract geometry.
 *
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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
 * Notes:
 *
 * This class depends on the OledbSupRowset doing appropriate binding 
 * for the geometry column ... eventually there should be a way for classes
 * derived from OledbSupRowset to modify the default binding logic.
 * 
 * $Log$
 * Revision 1.3  1999/06/04 04:00:23  kshih
 * Changes to work with geometry
 *
 * Revision 1.2  1999/05/14 13:31:42  warmerda
 * add best testing when reading from stream
 *
 * Revision 1.1  1999/04/01 20:49:09  warmerda
 * New
 *
 */

#include "oledb_sf.h"

/************************************************************************/
/*                            OledbSFTable()                            */
/************************************************************************/
 
OledbSFTable::OledbSFTable()

{
    iGeomColumn = -1;
    iBindColumn = -1;
    bTriedToIdentify = FALSE;

    pabyLastGeometry = NULL;
}

/************************************************************************/
/*                           ~OledbSFTable()                            */
/************************************************************************/

OledbSFTable::~OledbSFTable()

{
    if( pabyLastGeometry == NULL )
        CoTaskMemFree( pabyLastGeometry );
}

/************************************************************************/
/*                            HasGeometry()                             */
/************************************************************************/

int OledbSFTable::HasGeometry() 
{
    if( GetIRowset() == NULL )
        return FALSE;

    if( !bTriedToIdentify )
        IdentifyGeometry();

    return iGeomColumn != -1;
}

/************************************************************************/
/*                          IdentifyGeometry()                          */
/*                                                                      */
/*      This method attempts to identify the column containing the      */
/*      geometry.   Currently it is somewhat simple, but eventually     */
/*      it should use properties and other clues in addition to         */
/*      column names.                                                   */
/************************************************************************/

void OledbSFTable::IdentifyGeometry()

{
    if( GetIRowset() == NULL || bTriedToIdentify )
        return;

    bTriedToIdentify = TRUE;

/* -------------------------------------------------------------------- */
/*      Search for a column called OGIS_GEOMETRY (officially            */
/*      preferred name) or WKB_GEOMETRY (one provided sample            */
/*      database).                                                      */
/* -------------------------------------------------------------------- */
    ULONG      iCol;

    for( iCol = 0; iCol < nColumns; iCol++ )
    {
		if( paoColumnInfo[iCol].pwszName == NULL )
			continue;

        if( wcsicmp( L"WKB_GEOMETRY", paoColumnInfo[iCol].pwszName ) == 0 
         || wcsicmp( L"OGIS_GEOMETRY", paoColumnInfo[iCol].pwszName ) == 0 )
            break;
    }

    if( iCol == nColumns )
        return;

/* -------------------------------------------------------------------- */
/*      Verify that the type is acceptable.                             */
/* -------------------------------------------------------------------- */
    if( paoColumnInfo[iCol].wType != DBTYPE_BYTES 
        && paoColumnInfo[iCol].wType != DBTYPE_IUNKNOWN 
		&& paoColumnInfo[iCol].wType != (DBTYPE_BYTES | DBTYPE_BYREF))
    {
        return;
    }

/* -------------------------------------------------------------------- */
/*      We have found a column that looks OK.                           */
/* -------------------------------------------------------------------- */
    iGeomColumn = iCol;
}

/************************************************************************/
/*                           GetWKBGeometry()                           */
/*                                                                      */
/*      The pointer returned is to a block of binary data.  The data    */
/*      pointer returned is to memory internal to this class.  It       */
/*      should not be free, or altered.  It is only valid till the      */
/*      next row fetch.                                                 */
/*      CoTaskMemFree().                                                */
/************************************************************************/

BYTE *OledbSFTable::GetWKBGeometry( int * pnSize )

{
    if( pnSize != NULL )
        *pnSize = 0;

    if( !HasGeometry() )
        return NULL;

    if( pabyRecord == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Cleanup the previous passes geometry binary data, if necessary. */
/* -------------------------------------------------------------------- */
    if( pabyLastGeometry != NULL )
    {
        CoTaskMemFree( pabyLastGeometry );
        pabyLastGeometry = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Find the binding column if we don't already know it.  We        */
/*      have put this off to here, so that the binding will have        */
/*      been done.  It is normally done on the first row fetch on       */
/*      the OledbSupTable.                                              */
/* -------------------------------------------------------------------- */
    if( iBindColumn == -1 )
    {
        for( iBindColumn = 0; iBindColumn < (int) nBindings; iBindColumn++ )
        {
            if( paoBindings[iBindColumn].iOrdinal 
                == paoColumnInfo[iGeomColumn].iOrdinal )
                break;
        }
        if( iBindColumn == (int) nBindings )
        {
            iBindColumn = -1;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get access to the data in our data record.                      */
/* -------------------------------------------------------------------- */
    COLUMNDATA      *poCData;
    
    poCData = (COLUMNDATA *) (pabyRecord + paoBindings[iBindColumn].obLength);
        
/* -------------------------------------------------------------------- */
/*      If the column is bound as DBTYPE_BYTES, just return a           */
/*      pointer to the internal buffer.  We also check to see if the    */
/*      data was truncated.  If so we emit a debugging error            */
/*      message, but otherwise try to continue.                         */
/* -------------------------------------------------------------------- */
    if( paoBindings[iBindColumn].wType == DBTYPE_BYTES)
    {
        if( pnSize != NULL )
            *pnSize = poCData->dwLength;

        return(poCData->bData);
    }

/* -------------------------------------------------------------------- */
/*      If the column is bound as DBTYPE_BYTES and DBTYPE_BYREF then    */
/*      the pointer is contained in the data.  The size of the buffer   */
/*      follows.(This is an internal convention)                        */
/* -------------------------------------------------------------------- */
    if( paoBindings[iBindColumn].wType & DBTYPE_BYTES)
    {
		BYTE *pRetVal;

        if( pnSize != NULL )
			memcpy(pnSize,poCData->bData+4,4);
            
		memcpy(&pRetVal,poCData->bData,4);

        return(pRetVal);
    }

/* -------------------------------------------------------------------- */
/*      The remaining case is that the column is bound as an            */
/*      IUnknown, and we need to get an IStream interface for it to     */
/*      read the data.                                                  */
/* -------------------------------------------------------------------- */
    IUnknown * pIUnknown = *((IUnknown **) poCData->bData);
    IStream *  pIStream = NULL;
    HRESULT      hr;

    assert( paoBindings[iBindColumn].wType == DBTYPE_IUNKNOWN );
            
    hr = pIUnknown->QueryInterface( IID_IStream,
                                    (void**)&pIStream );
    if( FAILED(hr) )
    {
        DumpErrorHResult( hr, "Can't get IStream interface to geometry" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Stat the stream, to get it's length.                            */
/* -------------------------------------------------------------------- */
    STATSTG      oStreamStat;
    ULONG        nSize;

    hr = pIStream->Stat( &oStreamStat, STATFLAG_NONAME );
    if( FAILED(hr) )
    {
        DumpErrorHResult( hr, "IStream::Stat()" );
        pIStream->Release();
        return NULL;
    }

    nSize = oStreamStat.cbSize.LowPart;

/* -------------------------------------------------------------------- */
/*      Allocate the data and read from the stream.                     */
/* -------------------------------------------------------------------- */
	ULONG		nBytesActuallyRead = 0;

    pabyLastGeometry = (BYTE *) CoTaskMemAlloc( nSize );
    pabyLastGeometry[0] = 0;
	pabyLastGeometry[1] = 0;

    hr = pIStream->Read( pabyLastGeometry, nSize, &nBytesActuallyRead );
    if( FAILED( hr ) || nBytesActuallyRead < nSize )
    {
        DumpErrorHResult( hr, "IStream::Read()" ); 
        CoTaskMemFree( pabyLastGeometry );
        pabyLastGeometry = NULL;
    }

    pIStream->Release();
    pIUnknown->Release();

    if( pnSize != NULL && pabyLastGeometry != NULL )
        *pnSize = nSize;

    return pabyLastGeometry;
}

/************************************************************************/
/*                        SelectGeometryColumn()                        */
/************************************************************************/

int OledbSFTable::SelectGeometryColumn( const char * pszColumnName )

{
    int      iOrdinal = GetColumnOrdinal( pszColumnName );

    if( iOrdinal < 0 )
        return FALSE;

    for( ULONG i = 0; i < nColumns; i++ )
    {
        if( paoColumnInfo[i].iOrdinal == (ULONG) iOrdinal )
        {
            bTriedToIdentify = TRUE;
            iBindColumn = -1;
            iGeomColumn = i;
        }
    }

    return FALSE;
}
    
