/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OledbSupRowset class ... easily read one column of
 *           all records of a table ... normally the geometry column.
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
 * $Log$
 * Revision 1.1  1999/03/30 19:02:07  warmerda
 * New
 *
 */

#include "oledb_sup.h"

/************************************************************************/
/*                        OledbSupRowset()                              */
/************************************************************************/

OledbSupRowset::OledbSupRowset()

{
   pIRowset = NULL;
   pIAccessor = NULL;
   hAccessor = NULL;

   nColumns = 0;
   paoColumnInfo = NULL;
   pwszColumnStringBuffer = NULL;

   nBindings = 0;
   paoBindings = NULL;
}

/************************************************************************/
/*                       ~OledbSupRowset()                              */
/************************************************************************/

OledbSupRowset::~OledbSupRowset()

{
   if( paoBindings != NULL )
      CoTaskMemFree( paoBindings );

   if( pabyRecord != NULL )
      CoTaskMemFree( pabyRecord );

   if( pwszColumnStringBuffer != NULL )
      CoTaskMemFree( pwszColumnStringBuffer );

   if( paoColumnInfo != NULL )
      CoTaskMemFree( paoColumnInfo );

   if( pIAccessor != NULL )
   {
      if( hAccessor != NULL )
      {
         pIAccessor->ReleaseAccessor( hAccessor, NULL );
         hAccessor = NULL;
      }

      pIAccessor->Release();
      pIAccessor = NULL;
   }

   if( pIRowset != NULL )
   {
      pIRowset->Release();
      pIRowset = NULL;
   }
}

/************************************************************************/
/*                             OpenTable()                              */
/*                                                                      */
/*      Open a table as a rowset, and establish the column              */
/*      information about that table.                                   */
/************************************************************************/

HRESULT OledbSupRowset::OpenTable( IOpenRowset * pIOpenRowset,
                                   LPWSTR pwszTableName )
   
{
   IColumnsInfo* 	pIColumnsInfo = NULL;
   HRESULT       hr;

   assert( pIRowset == NULL );

/* -------------------------------------------------------------------- */
/*      Get the rowset for the table.                                   */
/* -------------------------------------------------------------------- */
   hr = OledbSupGetTableRowset( pIOpenRowset, pwszTableName, &pIRowset );
   if( FAILED( hr ) )
      return hr;

/* -------------------------------------------------------------------- */
/*      Get the column information.                                     */
/* -------------------------------------------------------------------- */
   return EstablishColumnInfo();
}

/************************************************************************/
/*                        EstablishColumnInfo()                         */
/*                                                                      */
/*      Internal helper method to establish the column info about       */
/*      the current rowset.                                             */
/************************************************************************/

HRESULT OledbSupRowset::EstablishColumnInfo()
   
{
   IColumnsInfo* 	pIColumnsInfo = NULL;
   HRESULT       hr;

   assert( pIRowset != NULL );

/* -------------------------------------------------------------------- */
/*      Fetch the column interface.                                     */
/* -------------------------------------------------------------------- */
   hr = pIRowset->QueryInterface( IID_IColumnsInfo, (void **) &pIColumnsInfo );
   if (FAILED(hr))
   {
      DumpErrorHResult( hr, "IRowset::QI for IID_IColumnsInfo" );
      return hr;
   }

/* -------------------------------------------------------------------- */
/*      Fetch the column info.                                          */
/* -------------------------------------------------------------------- */
   hr = pIColumnsInfo->GetColumnInfo( &nColumns, 
                                      &paoColumnInfo,
                                      &pwszColumnStringBuffer );
   if (FAILED(hr))
   {
      DumpErrorHResult( hr, "pIColumnsInfo->GetColumnInfo" );
      return hr;
   }

/* -------------------------------------------------------------------- */
/*      Release the column info interface.                              */
/* -------------------------------------------------------------------- */
   pIColumnsInfo->Release();
   pIColumnsInfo = NULL;

   return ResultFromScode( S_OK );
}

/************************************************************************/
/*                     EstablishOneDefaultBinding()                     */
/*                                                                      */
/*      Establish a finding for one field.                              */
/************************************************************************/

HRESULT OledbSupRowset::EstablishOneDefaultBinding( 
            DBCOLUMNINFO *poColumnInfo,  // in
            DBBINDING * poBinding,       // out
            DWORD * pdwOffset )          // in-out

{
   if( poColumnInfo->wType & DBTYPE_VECTOR )
      return ResultFromScode( S_FALSE );

   poBinding->dwPart	= DBPART_VALUE | DBPART_LENGTH | DBPART_STATUS;
   poBinding->eParamIO  = DBPARAMIO_NOTPARAM;                              

   poBinding->dwMemOwner = DBMEMOWNER_CLIENTOWNED;

   poBinding->iOrdinal  = poColumnInfo->iOrdinal;
   poBinding->pTypeInfo = NULL;
   poBinding->obValue   = *pdwOffset + offsetof(COLUMNDATA,bData);
   poBinding->obLength  = *pdwOffset + offsetof(COLUMNDATA,dwLength);
   poBinding->obStatus  = *pdwOffset + offsetof(COLUMNDATA,dwStatus);

   if( poColumnInfo->wType & DBTYPE_BYTES )
   {
      poBinding->wType = DBTYPE_BYTES;
      poBinding->cbMaxLen  = 10000;
   }
   else
   {
      poBinding->wType	  = DBTYPE_STR;
      poBinding->cbMaxLen  = poColumnInfo->wType == DBTYPE_STR ? 
         poColumnInfo->ulColumnSize + sizeof(char) : DEFAULT_CBMAXLENGTH;
   }
   poBinding->pObject	= NULL;
   *pdwOffset += poBinding->cbMaxLen + offsetof( COLUMNDATA, bData );
   *pdwOffset = ROUND_UP( *pdwOffset, COLUMN_ALIGNVAL );

   return ResultFromScode( S_OK );
}

/************************************************************************/
/*                      EstablishDefaultBindings()                      */
/************************************************************************/

HRESULT OledbSupRowset::EstablishDefaultBindings()

{
   DWORD      dwOffset;
   HRESULT    hr = ResultFromScode( S_FALSE );

   assert( nColumns > 0 );

/* -------------------------------------------------------------------- */
/*      Allocate a binding information array capable of handling all    */
/*      columns.                                                        */
/* -------------------------------------------------------------------- */
   nBindings = 0;
   paoBindings = (DBBINDING *) CoTaskMemAlloc(sizeof(DBBINDING)*nColumns);

   if( paoBindings == NULL )
      return ResultFromScode( S_FALSE );

/* -------------------------------------------------------------------- */
/*      Setup default binding for each column.                          */
/* -------------------------------------------------------------------- */
   dwOffset = 0;
   for( int iCol = 0; iCol < (int) nColumns; iCol++ )
   {
      hr = EstablishOneDefaultBinding( paoColumnInfo + iCol, 
                                       paoBindings + nBindings,
                                       &dwOffset );
      if( FAILED( hr ) )
      {
         nBindings = 0;
         CoTaskMemFree( paoBindings );
         paoBindings = NULL;
         return hr;
      }

      nBindings++;
   }

   nMaxRecordSize = dwOffset;

   return ResultFromScode( S_OK );
}

/************************************************************************/
/*                         EstablishAccessor()                          */
/************************************************************************/

HRESULT OledbSupRowset::EstablishAccessor()

{
   HRESULT      hr;

/* -------------------------------------------------------------------- */
/*      If we don't have any bindings, then create default ones         */
/*      now.                                                            */
/* -------------------------------------------------------------------- */
   if( nBindings == NULL )
   {
      hr = EstablishDefaultBindings();
      if( FAILED( hr ) )
         return hr;
   }

/* -------------------------------------------------------------------- */
/*      Create an accessor.                                             */
/* -------------------------------------------------------------------- */
   hr = pIRowset->QueryInterface( IID_IAccessor,
                                  (void**)&pIAccessor );
   if( FAILED( hr ) )
   {
      DumpErrorHResult( hr, "pIRowset->QI for IID_IAccessor" );	
      return hr;
   }

   hr = pIAccessor->CreateAccessor( DBACCESSOR_ROWDATA, 
                                    nBindings, paoBindings,
                                    0, &hAccessor, NULL );

   if( FAILED( hr ) )
   {
      DumpErrorHResult( hr, "Create Accessor failed." );	
      return hr;
   }

   if( hAccessor == NULL )
   {
      DumpErrorMsg( "CreateAccessor failed." );
      return ResultFromScode( S_FALSE );
   }

/* -------------------------------------------------------------------- */
/*      Create a working record based on the binding size.              */
/* -------------------------------------------------------------------- */
   pabyRecord = (BYTE *) CoTaskMemAlloc(nMaxRecordSize);
   if( pabyRecord == NULL )
      return ResultFromScode( S_FALSE );

   return ResultFromScode( S_OK );
}

/************************************************************************/
/*                           GetNextRecord()                            */
/************************************************************************/

int OledbSupRowset::GetNextRecord( HRESULT * pnHRESULT )

{
   HRESULT       hr;

   if( pnHRESULT == NULL )
      pnHRESULT = &hr;

   *pnHRESULT = ResultFromScode( S_OK );

/* -------------------------------------------------------------------- */
/*      Is this our first record?  If so, we have to create an          */
/*      accessor.                                                       */
/* -------------------------------------------------------------------- */
   if( hAccessor == NULL )
   {
      hr = EstablishAccessor();
      if( FAILED( hr ) )
      {
         *pnHRESULT = hr;
         return FALSE;
      }
   }

/* -------------------------------------------------------------------- */
/*      Get the next record.  We really should be getting the           */
/*      records in bunches for efficiency, but that will complicate     */
/*      this function a bit.  This can be added later.                  */
/* -------------------------------------------------------------------- */
   ULONG      nRowsObtained;
   HROW       ahRows[1];
   HROW       *phRows = ahRows;

   hr = pIRowset->GetNextRows(
      NULL,				// hChapter
      0,				// cRowsToSkip
      1,				// cRowsDesired
      &nRowsObtained,                   // pcRowsObtained
      &phRows ); 			// filled in w/ row handles

   if (FAILED(hr))
   {
      DumpErrorHResult( hr, "pIRowset->GetNextRows" );
      *pnHRESULT = hr;
      return FALSE;
   }

   // we should be able to distinguish eof from an error, but don't now.
   if( nRowsObtained == 0 )
   {
      return FALSE;
   }

/* -------------------------------------------------------------------- */
/*      Fetch this record from a grouping into our record buffer.       */
/* -------------------------------------------------------------------- */
   hr = pIRowset->GetData(ahRows[0],
                          hAccessor,
                          pabyRecord );
   if (FAILED(hr))
   {
      DumpErrorHResult( hr, "pIRowset->GetData" );
      *pnHRESULT = hr;
      return FALSE;
   }   
   
/* -------------------------------------------------------------------- */
/*      Release the row handles from this grouping.                     */
/* -------------------------------------------------------------------- */
   hr = pIRowset->ReleaseRows( nRowsObtained, ahRows, NULL, NULL, NULL );
   if (FAILED(hr))
   {
      DumpErrorHResult( hr, "pIRowset->ReleaseRows" );
      *pnHRESULT = hr;
      return hr;
   } 

   return TRUE;
}

/************************************************************************/
/*                            GetFieldData()                            */
/************************************************************************/

void *OledbSupRowset::GetFieldData( int iColumn, 
                                    int * pnDBType, int * pnStatus, 
                                    int * pnSize )

{
   int    iBind;
   DBBINDING *poBinding;

/* -------------------------------------------------------------------- */
/*      Find the bound column that corresponds with ordinal iColumn.    */
/* -------------------------------------------------------------------- */
   for( iBind = 0; iBind < nBindings; iBind++ )
   {
      if( paoBindings[iBind].iOrdinal == iColumn )
         break;
   }

   if( iBind >= nBindings )
   {
      DumpErrorMsg( "GetFieldData() on unbound column." );
      return NULL;
   }

   poBinding = paoBindings + iBind;

/* -------------------------------------------------------------------- */
/*      Find the column data within the record.                         */
/* -------------------------------------------------------------------- */
   COLUMNDATA      *poCData;

   poCData = (COLUMNDATA *) (pabyRecord + poBinding->obLength);

   if( pnStatus != NULL )
      *pnStatus = poCData->dwStatus;

   if( pnSize != NULL )
      *pnSize = poCData->dwLength;

/* -------------------------------------------------------------------- */
/*      Collect auxilary information.                                   */
/* -------------------------------------------------------------------- */
   if( pnDBType != NULL )
      *pnDBType = poBinding->wType;

   return( poCData->bData );
}

/************************************************************************/
/*                          GetColumnOrdinal()                          */
/************************************************************************/

int OledbSupRowset::GetColumnOrdinal( const char * pszName )

{
   LPWSTR      pwszName;
   int         iReturn = -1;

   AnsiToUnicode( pszName, &pwszName );

   for( int i = 0; i < nColumns; i++ )
   {
      if( wcsicmp( pwszName, paoColumnInfo[i].pwszName ) == 0 )
      {
         iReturn = i;
         break;
      }
   }

   CoTaskMemFree( pwszName );

   return( iReturn );
}
