/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OLE DB support functions. 
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
 * Revision 1.2  1999/03/31 15:11:16  warmerda
 * Use char * instead of LPWSTR, better multi-provider support
 *
 * Revision 1.1  1999/03/30 19:07:59  warmerda
 * New
 *
 */

#include "oledb_sup.h"

/************************************************************************/
/*                            DumpErrorMsg()                            */
/************************************************************************/
void DumpErrorMsg( const char * pszMessage )

{
   fprintf( stderr, "%s\n", pszMessage );
}

/************************************************************************/
/*                          OleSupInitialize()                          */
/************************************************************************/

int OleSupInitialize()

{
   DWORD   dwVersion;
   HRESULT hr;
	
   dwVersion = OleBuildVersion();
   if (HIWORD(dwVersion) != rmm)
   {
      fprintf( stderr, 
        "Error: OLE version mismatch. Build version %ld, current version %ld\n",
               rmm, HIWORD(dwVersion) );

      return FALSE;
   }

   hr = OleInitialize( NULL );
   if (FAILED(hr))
   {
      DumpErrorMsg("Error: OleInitialize failed\n");
      return FALSE;
   }
     
   return TRUE;
}

/************************************************************************/
/*                         OleSupUninitialize()                         */
/************************************************************************/

int OleSupUninitialize()

{
   OleUninitialize();

   return TRUE;
}

/************************************************************************/
/*                           UnicodeToAnsi()                            */
/************************************************************************/
HRESULT UnicodeToAnsi(LPCOLESTR pszW, LPSTR* ppszA)
{
   ULONG cbAnsi, cCharacters;
   DWORD dwError;

   // If input is null then just return the same.
   if (pszW == NULL)
   {
       *ppszA = NULL;
       return NOERROR;
   }

   cCharacters = wcslen(pszW)+1;
   // Determine number of bytes to be allocated for ANSI string. An
   // ANSI string can have at most 2 bytes per character (for Double
   // Byte Character Strings.)
   cbAnsi = cCharacters*2;

   // Use of the OLE allocator is not required because the resultant
   // ANSI string will never be passed to another COM component. You
   // can use your own allocator.
   *ppszA = (LPSTR) CoTaskMemAlloc(cbAnsi);
   if (NULL == *ppszA)
      return E_OUTOFMEMORY;

   // Convert to ANSI.
   if (0 == WideCharToMultiByte(CP_ACP, 0, pszW, cCharacters, *ppszA,
                                cbAnsi, NULL, NULL))
   {
      dwError = GetLastError();
      CoTaskMemFree(*ppszA);
      *ppszA = NULL;
      return HRESULT_FROM_WIN32(dwError);
   }

   return NOERROR;
}
/************************************************************************/
/*                           AnsiToUnicode()                            */
/************************************************************************/
HRESULT AnsiToUnicode(LPCSTR pszA, LPOLESTR* ppszW)
{
   ULONG cCharacters;
   DWORD dwError;

   // If input is null then just return the same.
   if (NULL == pszA)
   {
      *ppszW = NULL;
      return NOERROR;
   }

   // Determine number of wide characters to be allocated for the
   // Unicode string.
   cCharacters =  strlen(pszA)+1;

   // Use of the OLE allocator is required if the resultant Unicode
   // string will be passed to another COM component and if that
   // component will free it. Otherwise you can use your own allocator.
   *ppszW = (LPOLESTR) CoTaskMemAlloc(cCharacters*2);
   if (NULL == *ppszW)
      return E_OUTOFMEMORY;

   // Covert to Unicode.
   if (0 == MultiByteToWideChar(CP_ACP, 0, pszA, cCharacters,
                                *ppszW, cCharacters))
   {
      dwError = GetLastError();
      CoTaskMemFree(*ppszW);
      *ppszW = NULL;
      return HRESULT_FROM_WIN32(dwError);
   }

   return NOERROR;
}
/************************************************************************/
/*                       OledbSupGetDataSource()                        */
/************************************************************************/

HRESULT OledbSupGetDataSource( REFCLSID phProviderCLSID, 
                               const char *pszDataSource,
                               IOpenRowset **ppIOpenRowset )

{
    IDBCreateSession    *pIDBCreateSession = NULL;
    IDBInitialize*	pIDBInit = NULL;
    IDBProperties*	pIDBProperties = NULL;
    DBPROPSET		dbPropSet[1];
    DBPROP		dbProp[1];

    HRESULT	hr;

    assert(ppIOpenRowset != NULL);
    *ppIOpenRowset = NULL;

    VariantInit(&(dbProp[0].vValue));

    // Create an instance of the SampProv sample data provider
    hr = CoCreateInstance( phProviderCLSID, NULL, CLSCTX_INPROC_SERVER, 
                           IID_IDBInitialize, (void **)&pIDBInit ); 
    if (FAILED(hr))
    {
        DumpErrorHResult( hr, "CoCreateInstance" );
        goto error;
    }

/* -------------------------------------------------------------------- */
/*      Select the data source.  We don't need to do this for some      */
/*      providers, it seems.                                            */
/* -------------------------------------------------------------------- */
    if( pszDataSource != NULL && strlen(pszDataSource) > 0 )
    {
        LPWSTR            pwszDataSource = NULL;
        // Initialize this provider with the path to the customer.csv file
        dbPropSet[0].rgProperties	= &dbProp[0];
        dbPropSet[0].cProperties	= 1;
        dbPropSet[0].guidPropertySet	= DBPROPSET_DBINIT;
       
        dbProp[0].dwPropertyID		= DBPROP_INIT_DATASOURCE;
        dbProp[0].dwOptions		= DBPROPOPTIONS_REQUIRED;
        dbProp[0].colid			= DB_NULLID;

        AnsiToUnicode( pszDataSource, &pwszDataSource );

        V_VT(&(dbProp[0].vValue))	= VT_BSTR;
        V_BSTR(&(dbProp[0].vValue))	= SysAllocString( pwszDataSource );
        CoTaskMemFree( pwszDataSource );

        if ( NULL == V_BSTR(&(dbProp[0].vValue)) )
        {
            DumpErrorMsg( "SysAllocString failed\n" );
            goto error;
        }

        hr = pIDBInit->QueryInterface( IID_IDBProperties, 
                                       (void**)&pIDBProperties);
        if (FAILED(hr))
        {
            DumpErrorHResult( hr, "IDBInitialize::QI for IDBProperties");
            goto error;
        }

        hr = pIDBProperties->SetProperties( 1, &dbPropSet[0]);
        if (FAILED(hr))
        {
            DumpErrorHResult( hr, "IDBProperties::SetProperties" );
            goto error;
        }
    }

/* -------------------------------------------------------------------- */
/*      Initialize the database.                                        */
/* -------------------------------------------------------------------- */
    hr = pIDBInit->Initialize();
    if (FAILED(hr))
    {
        DumpErrorHResult( hr, "IDBInitialize::Initialize" );
        goto error;
    }

    hr = pIDBInit->QueryInterface( IID_IDBCreateSession, 
                                   (void**)&pIDBCreateSession);
    pIDBInit->Release();
    pIDBInit = NULL;
        
    if (FAILED(hr))
    {
        DumpErrorHResult( hr, "IDBInitialize::QI for IDBCreateSession");
        goto error;
    }

    hr = pIDBCreateSession->CreateSession( NULL, IID_IOpenRowset, 
                                           (IUnknown**) ppIOpenRowset );    

    pIDBCreateSession->Release();
    pIDBCreateSession = NULL;
     
    if (FAILED(hr))
    {
        DumpErrorHResult( hr, "IDBCreateSession::CreateSession");
        goto error;
    }

    hr = ResultFromScode( S_OK );

  error:    
    VariantClear( &(dbProp[0].vValue) );

    if( pIDBProperties )
        pIDBProperties->Release();

    return hr;    
}

/************************************************************************/
/*                       OledbSupGetTableRowset()                       */
/*                                                                      */
/*      Get the rowset associated with a particular table name.         */
/************************************************************************/

HRESULT OledbSupGetTableRowset( IOpenRowset * pIOpenRowset, 
                                const char *pszTableName, 
                                IRowset ** ppIRowset )
{
   DBID            dbcolid;
   HRESULT         hr;

   assert(pIOpenRowset != NULL);
   assert(ppIRowset  != NULL );

   *ppIRowset = NULL;
    
   // tell the provider which table to open
   dbcolid.eKind           = DBKIND_NAME;
   AnsiToUnicode( pszTableName, &(dbcolid.uName.pwszName) );
    
   hr = pIOpenRowset->OpenRowset
      ( NULL,                 // pUnkOuter - we are not aggregating
        &dbcolid,             // pTableID -  the table we want
        NULL,					// pIndexID - the index we want
        IID_IRowset,          // riid - interface we want on the rowset object
        0,                    // cProperties - we are niave about props for now
        NULL,                 // prgProperties[]
        (IUnknown**)ppIRowset );

   CoTaskMemFree( dbcolid.uName.pwszName );

   if (FAILED(hr))
   {
      DumpErrorHResult( hr, "IOpenRowset::OpenRowset" );
      goto error;
   }
    
   // all went well
   return ResultFromScode( S_OK );

  error:
   return ResultFromScode( hr );
}

/************************************************************************/
/*                           GetNoteString()                            */
/************************************************************************/

char* GetNoteString( Note * rgNote, int cNote, DWORD dwValue )

{
	int j;

	assert(rgNote != NULL);

	// Scan a table of value/string,
	// return ptr to string found.

	for (j=0; j < cNote; j++) {
		if (rgNote[j].dwFlag == dwValue)
			return rgNote[j].szText;
	}
	return "<unknown>";
}

/************************************************************************/
/*                          DumpErrorHResult()                          */
/************************************************************************/
HRESULT DumpErrorHResult
	(
	HRESULT      hr_return,
	const char  *format,			// can be NULL
	... 
	)
{
   char     buff[100];
   int      cBytesWritten;
   va_list  argptr;

   //
   // Dump an error message.
   // Print the text of the HRESULT,
   // Return the HRESULT we were passed.

   // these result codes were generated from the oledberr.h 
   static Note ResultCodes[] = {
      // oledberr.h error codes
      NOTE(DB_E_BADACCESSORHANDLE),
      NOTE(DB_E_BADACCESSORHANDLE),
      NOTE(DB_E_ROWLIMITEXCEEDED),
      NOTE(DB_E_READONLYACCESSOR),
      NOTE(DB_E_SCHEMAVIOLATION),
      NOTE(DB_E_BADROWHANDLE),
      NOTE(DB_E_OBJECTOPEN),
      NOTE(DB_E_BADBINDINFO),
      NOTE(DB_SEC_E_PERMISSIONDENIED),
      NOTE(DB_E_NOTAREFERENCECOLUMN),
      NOTE(DB_E_NOCOMMAND),
      NOTE(DB_E_BADBOOKMARK),
      NOTE(DB_E_BADLOCKMODE),
      NOTE(DB_E_PARAMNOTOPTIONAL),
      NOTE(DB_E_BADRATIO),
      NOTE(DB_E_ERRORSINCOMMAND),
      NOTE(DB_E_BADSTARTPOSITION),
      NOTE(DB_E_NOTREENTRANT),
      NOTE(DB_E_NOAGGREGATION),
      NOTE(DB_E_DELETEDROW),
      NOTE(DB_E_CANTFETCHBACKWARDS),
      NOTE(DB_E_ROWSNOTRELEASED),
      NOTE(DB_E_BADSTORAGEFLAG),
      NOTE(DB_E_BADSTATUSVALUE),
      NOTE(DB_E_CANTSCROLLBACKWARDS),
      NOTE(DB_E_INTEGRITYVIOLATION),
      NOTE(DB_E_ABORTLIMITREACHED),
      NOTE(DB_E_DUPLICATEINDEXID),
      NOTE(DB_E_NOINDEX),
      NOTE(DB_E_INDEXINUSE),
      NOTE(DB_E_NOTABLE),
      NOTE(DB_E_CONCURRENCYVIOLATION),
      NOTE(DB_E_BADCOPY),
      NOTE(DB_E_BADPRECISION),
      NOTE(DB_E_BADSCALE),
      NOTE(DB_E_BADID),
      NOTE(DB_E_BADTYPE),
      NOTE(DB_E_DUPLICATECOLUMNID),
      NOTE(DB_E_DUPLICATETABLEID),
      NOTE(DB_E_TABLEINUSE),
      NOTE(DB_E_NOLOCALE),
      NOTE(DB_E_BADRECORDNUM),
      NOTE(DB_E_BOOKMARKSKIPPED),
      NOTE(DB_E_BADPROPERTYVALUE),
      NOTE(DB_E_INVALID),
      NOTE(DB_E_BADACCESSORFLAGS),
      NOTE(DB_E_BADSTORAGEFLAGS),
      NOTE(DB_E_BYREFACCESSORNOTSUPPORTED),
      NOTE(DB_E_NULLACCESSORNOTSUPPORTED),
      NOTE(DB_E_NOTPREPARED),
      NOTE(DB_E_BADACCESSORTYPE),
      NOTE(DB_E_WRITEONLYACCESSOR),
      NOTE(DB_SEC_E_AUTH_FAILED),
      NOTE(DB_E_CANCELED),
      NOTE(DB_E_BADSOURCEHANDLE),
      NOTE(DB_S_ROWLIMITEXCEEDED),
      NOTE(DB_S_COLUMNTYPEMISMATCH),
      NOTE(DB_S_TYPEINFOOVERRIDDEN),
      NOTE(DB_S_BOOKMARKSKIPPED),
      NOTE(DB_S_ENDOFROWSET),
      NOTE(DB_S_BUFFERFULL),
      NOTE(DB_S_CANTRELEASE),
      NOTE(DB_S_DIALECTIGNORED),
      NOTE(DB_S_UNWANTEDPHASE),
      NOTE(DB_S_COLUMNSCHANGED),
      NOTE(DB_S_ERRORSRETURNED),
      NOTE(DB_S_BADROWHANDLE),
      NOTE(DB_S_DELETEDROW),
      NOTE(DB_S_STOPLIMITREACHED),
      NOTE(DB_S_LOCKUPGRADED),
      NOTE(DB_S_PROPERTIESCHANGED),
      NOTE(DB_S_ERRORSOCCURRED),
      NOTE(DB_S_PARAMUNAVAILABLE),
      NOTE(DB_S_MULTIPLECHANGES),

      // winerr.h
      NOTE(E_UNEXPECTED),
      NOTE(E_NOTIMPL),
      NOTE(E_OUTOFMEMORY),
      NOTE(E_INVALIDARG),
      NOTE(E_NOINTERFACE),
      NOTE(E_POINTER),
      NOTE(E_HANDLE),
      NOTE(E_ABORT),
      NOTE(E_FAIL),
      NOTE(E_ACCESSDENIED),
      NOTE(S_OK),
      NOTE(S_FALSE),
      NOTE(E_UNEXPECTED),
      NOTE(E_NOTIMPL),
      NOTE(E_OUTOFMEMORY),
      NOTE(E_INVALIDARG),
      NOTE(E_NOINTERFACE),
      NOTE(E_POINTER),
      NOTE(E_HANDLE),
      NOTE(E_ABORT),
      NOTE(E_FAIL),
      NOTE(E_ACCESSDENIED),
      // BindMoniker Errors
      NOTE(MK_E_NOOBJECT),
      NOTE(MK_E_EXCEEDEDDEADLINE),
      NOTE(MK_E_CONNECTMANUALLY),
      NOTE(MK_E_INTERMEDIATEINTERFACENOTSUPPORTED),
      NOTE(STG_E_ACCESSDENIED),
      NOTE(MK_E_SYNTAX),
      NOTE(MK_E_CANTOPENFILE),
   };


   // Format the message.
   // Print name of hresult code.

   if (format)
   {
      va_start( argptr, format );
      cBytesWritten = _vsnprintf( buff, sizeof(buff), format, argptr );
      va_end( argptr );
   }
   else
      strcpy( buff, "" );

   fprintf( stderr, "%s: Returned %s\n", 
            buff, 
            GetNoteString( ResultCodes, NUMELEM(ResultCodes), 
                           GetScode(hr_return)) );

   return hr_return;
}

