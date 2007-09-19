/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  OLE DB support functions. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
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
 ****************************************************************************/

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
/*                             AnsiToBSTR()                             */
/*                                                                      */
/*      Convert an ANSI string to Unicode, and then to a BSTR.          */
/************************************************************************/

HRESULT AnsiToBSTR( const char *pszInput, BSTR * ppszOutput )

{
    HRESULT      hr;
    LPOLESTR     pszUnicode;

    hr = AnsiToUnicode( pszInput, &pszUnicode );
    if( FAILED(hr) )
        return hr;

    *ppszOutput = SysAllocString( pszUnicode );
    CoTaskMemFree( pszUnicode );

    return ResultFromScode( S_OK );
}

/************************************************************************/
/*                        GetNoteStringBitvals()                        */
/************************************************************************/

static char*    GetNoteStringBitvals
        (
        Note*   rgNote,
        int     cNote,
        DWORD   dwValue 
        )
{
        static char buff[400];
        int j;

        assert(rgNote != NULL);

        // Make a string that combines all the bits ORed together.

        strcpy(buff, "");
        for (j=0; j < cNote; j++) {
                if (rgNote[j].dwFlag & dwValue) {
                        if (buff[0])
                                strcat( buff, " | " );
                        strcat( buff, rgNote[j].szText );
                }
        }
        assert(strlen(buff) < sizeof(buff));
        return buff;
}

/************************************************************************/
/*                           GetNoteString()                            */
/************************************************************************/

char* GetNoteString( Note * rgNote, int cNote, DWORD dwValue )

{
        int j;
        static char unknown[128];

        assert(rgNote != NULL);

        // Scan a table of value/string,
        // return ptr to string found.

        for (j=0; j < cNote; j++) {
                if (rgNote[j].dwFlag == dwValue)
                        return rgNote[j].szText;
        }
        sprintf( unknown, "<unknown:hr=%X>", dwValue );

        return unknown;
}

/************************************************************************/
/*                          DumpErrorHResult()                          */
/************************************************************************/
HRESULT DumpErrorHResult
        (
        HRESULT      hr_return,
        const char  *format,                    // can be NULL
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

/************************************************************************/
/*                          WriteColumnInfo()                           */
/*                                                                      */
/*      Dump info about one column to selected file handle.  Just       */
/*      used by DumpColumnsInfo().                                      */
/************************************************************************/

void OledbSupWriteColumnInfo(FILE *fp, DBCOLUMNINFO* p )

{
    DBID            *pCol;
    DBKIND      eKind;
    wchar_t     wszGuidBuff[MAX_GUID_STRING];
    wchar_t     wszNameBuff[MAX_GUID_STRING];    
    
    static char *szDbcolkind[] = { "Guid+Name", "Guid+PropID", "Name", 
                                   "Guid+Name", "Guid+PropID", "PropID", "Guid" };

    assert(p != NULL);

    // For DBTYPEENUM.  Doesn't need to be in order.
    // Below we mask off the high bits.
    static Note typenotes[] = 
    {
        NOTE(DBTYPE_EMPTY),
        NOTE(DBTYPE_NULL),
        NOTE(DBTYPE_I2),
        NOTE(DBTYPE_I4),
        NOTE(DBTYPE_R4),
        NOTE(DBTYPE_R8),
        NOTE(DBTYPE_CY),
        NOTE(DBTYPE_DATE),
        NOTE(DBTYPE_BSTR),
        NOTE(DBTYPE_IDISPATCH),
        NOTE(DBTYPE_ERROR),
        NOTE(DBTYPE_BOOL),
        NOTE(DBTYPE_VARIANT),
        NOTE(DBTYPE_IUNKNOWN),
        NOTE(DBTYPE_DECIMAL),
        NOTE(DBTYPE_UI1),
        NOTE(DBTYPE_ARRAY),
        NOTE(DBTYPE_BYREF),
        NOTE(DBTYPE_I1),
        NOTE(DBTYPE_UI2),
        NOTE(DBTYPE_UI4),
        NOTE(DBTYPE_I8),
        NOTE(DBTYPE_UI8),
        NOTE(DBTYPE_GUID),
        NOTE(DBTYPE_VECTOR),
        NOTE(DBTYPE_RESERVED),
        NOTE(DBTYPE_BYTES),
        NOTE(DBTYPE_STR),
        NOTE(DBTYPE_WSTR),
        NOTE(DBTYPE_NUMERIC),
        NOTE(DBTYPE_UDT),
        NOTE(DBTYPE_DBDATE),
        NOTE(DBTYPE_DBTIME),
        NOTE(DBTYPE_DBTIMESTAMP),
    };

    static Note flagnotes[] = 
    {
        NOTE(DBCOLUMNFLAGS_ISBOOKMARK),
        NOTE(DBCOLUMNFLAGS_MAYDEFER),
        NOTE(DBCOLUMNFLAGS_WRITE),
        NOTE(DBCOLUMNFLAGS_WRITEUNKNOWN),
        NOTE(DBCOLUMNFLAGS_ISFIXEDLENGTH),
        NOTE(DBCOLUMNFLAGS_ISNULLABLE),
        NOTE(DBCOLUMNFLAGS_MAYBENULL),
        NOTE(DBCOLUMNFLAGS_ISLONG),
        NOTE(DBCOLUMNFLAGS_ISROWID),
        NOTE(DBCOLUMNFLAGS_ISROWVER),
        NOTE(DBCOLUMNFLAGS_CACHEDEFERRED),
    };

    pCol = & p->columnid;
    eKind = pCol->eKind;

    // stringize GUID for pretty printing
    switch (eKind)
    {
        case DBKIND_GUID_NAME:
        case DBKIND_GUID_PROPID:
        case DBKIND_GUID:
            StringFromGUID2( pCol->uGuid.guid, wszGuidBuff, sizeof(wszGuidBuff) );
            break;
        case DBKIND_PGUID_NAME:
        case DBKIND_PGUID_PROPID:          
            StringFromGUID2( *(pCol->uGuid.pguid), wszGuidBuff, sizeof(wszGuidBuff) );
            break;
        default:
            wcscpy( wszGuidBuff, L"<none>" );
            break;    
    }
        
    // stringize name or propID for pretty printing   
    switch (eKind)
    {
        case DBKIND_GUID_NAME:
        case DBKIND_NAME:
        case DBKIND_PGUID_NAME:
            swprintf( wszNameBuff, L"[name=%.50S]", pCol->uName.pwszName ? pCol->uName.pwszName : L"(unknown)" );
            break;
        case DBKIND_GUID_PROPID:
        case DBKIND_PGUID_PROPID:
        case DBKIND_PROPID:
            swprintf( wszNameBuff, L"[propid=%lu]", pCol->uName.ulPropid );
            break;
        default:
            wcscpy( wszNameBuff, L"" );
            break;    
    }   

    // pretty print column info
    fprintf( fp, "ColumnId [kind=%.40s] [guid=%.40S] %.60S\n", 
              szDbcolkind[eKind], wszGuidBuff, wszNameBuff );

    // Now move on to other stuff...
    // Name in DBCOLUMNINFO different than name in DBCOLUMNID (maybe).
    fprintf(fp, "  Name          = '%.50S'\n", p->pwszName );
    fprintf(fp, "  iOrdinal      = %d\n", p->iOrdinal);
    fprintf(fp, "  wType         = %.100s\n", 
            GetNoteString( typenotes, NUMELEM(typenotes),
                           p->wType & (~DBTYPE_BYREF) & (~DBTYPE_ARRAY) & (~DBTYPE_VECTOR) ) );
    if (p->wType & DBTYPE_BYREF)
        fprintf(fp, "      (BYREF)\n");
    if (p->wType & DBTYPE_ARRAY)
        fprintf(fp, "      (ARRAY)\n");
    if (p->wType & DBTYPE_VECTOR)
        fprintf(fp, "      (VECTOR)\n");
    fprintf(fp, "  ulColumnSize  = %ld\n", p->ulColumnSize );
    fprintf(fp, "  bPrecision    = %d\n",  p->bPrecision );
    fprintf(fp, "  bScale        = %d\n",  p->bScale );
    fprintf(fp, "  dwFlags       = %s\n\n",
             GetNoteStringBitvals( flagnotes, NUMELEM(flagnotes), p->dwFlags ) );
        

}

/************************************************************************/
/*                             PrintColumn                              */
/************************************************************************/

static void PrintColumn
        (
        FILE          *fp,
        DBCOLUMNINFO  *pColumnInfo,
        COLUMNDATA    *pColumn,
        DBBINDING     *rgBind,
        ULONG          iBind,
        ULONG          cMaxColWidth 
        )
{
    void*       p;
    ULONG   ulPrintWidth;
    ULONG   ulPrintPrecision;
    DWORD   dwStatus;
    DWORD   dwLength;
    BOOL    fDidVariant;
    BOOL    fIsUnicode;
    char*       sFormat;
    HRESULT hr;
        
    assert(pColumn != NULL);
    assert(rgBind != NULL);

/* -------------------------------------------------------------------- */
/*      Print out the column name.                                      */
/* -------------------------------------------------------------------- */
    if( pColumnInfo->pwszName == NULL )
        fprintf( fp, "(anon) = " );
    else
        fprintf( fp, "%S = ", pColumnInfo->pwszName );
    
/* -------------------------------------------------------------------- */
/*      Print value.                                                    */
/* -------------------------------------------------------------------- */

    fDidVariant = FALSE;
    fIsUnicode  = FALSE;
    dwStatus = pColumn->dwStatus;
    dwLength = pColumn->dwLength;

    if (dwStatus == DBSTATUS_S_ISNULL)
    {
        p = "<null>";
        dwLength = strlen( (char *) p);
    }
    else if (dwStatus == DBBINDSTATUS_UNSUPPORTEDCONVERSION)
    {
        p = "<unsupportedconversion>";
        dwLength = strlen( (char *) p);
    }    
    else
    {
        switch (rgBind[iBind].wType) 
        {
            case DBTYPE_STR:
                // We have a string in our buffer, so use it.
                p = (void *) &pColumn->bData;
                break;

            case DBTYPE_BYTES:
                static char out_string[100];
                int      ii;

                sprintf( out_string, "(BLOB:%dbytes:0x", dwLength );
                for( ii = 0; ii < 8 && ii < (int) dwLength; ii++ )
                {
                    sprintf( out_string + strlen(out_string), 
                             "%02x", pColumn->bData[ii] );
                }
                if( ii << dwLength )
                    strcat( out_string, "...");
            
                strcat( out_string, ")" );
                // We have a string in our buffer, so use it.
                p = (void *) out_string;;
                break;

            case DBTYPE_VARIANT:
                // We have a variant in our buffer, so convert to string.
                p = (void *) &pColumn->bData;
                hr = VariantChangeTypeEx(
                    (VARIANT *) p,                      // Destination (convert in place)
                    (VARIANT *) p,                      // Source
                    LOCALE_SYSTEM_DEFAULT,      // LCID
                    0,                                          // dwFlags
                    VT_BSTR );
                if (FAILED(hr))
                {
                    DumpErrorHResult( hr, "VariantChangeTypeEx, field %d", iBind );
                    return;
                }
                p = (wchar_t *) (((VARIANT *)p)->bstrVal) ;
                dwLength = ((DWORD *)p)[-1] / sizeof(wchar_t);
                fDidVariant = TRUE;
                fIsUnicode  = TRUE;
                break;

            default:
                p = "??? unknown type ???";
                break;
        }
    }

    // Print the column.
    // If it has been truncated or rounded, print a '#' in
    // the far right-hand column.
    ulPrintWidth     = min( cMaxColWidth, rgBind[iBind].cbMaxLen );
    ulPrintPrecision = min( cMaxColWidth, dwLength );
    if (dwStatus == DBSTATUS_S_TRUNCATED ||  cMaxColWidth < dwLength)
    {
        ulPrintWidth--;
        ulPrintPrecision--;
    }

    sFormat = fIsUnicode ? "%-*.*S" : "%-*.*s";

    fprintf( fp, sFormat, ulPrintWidth, ulPrintPrecision, p );

    if (dwStatus == DBSTATUS_S_TRUNCATED ||  cMaxColWidth < dwLength)
        fprintf( fp, "#" );
    fprintf( fp, "\n" );
    // Free memory used by the variant.
    if (fDidVariant)
        VariantClear( (VARIANT *) &pColumn->bData );
}

/************************************************************************/
/*                          OledbSupDumpRow()                           */
/************************************************************************/

void OledbSupDumpRow
        (
    FILE        *fp,
    DBCOLUMNINFO* paoColumnInfo,
    int         nColumns,
    DBBINDING*  rgBind,
    ULONG       cBind,
    ULONG       cMaxColWidth,
    BYTE*       pData
    )
{
    ULONG            iBind;
    COLUMNDATA*      pColumn;
    DBCOLUMNINFO*      pColumnInfo;
    int              i;
    
    assert(rgBind);
    assert( offsetof(COLUMNDATA, dwLength) == 0);       
    
    // Print each column we're bound to.
    for (iBind=0; iBind < cBind; iBind++)
    {
        pColumnInfo = NULL;
        for( i = 0; i < nColumns; i++ )
        {
            if( paoColumnInfo[i].iOrdinal == rgBind[iBind].iOrdinal )
                pColumnInfo = paoColumnInfo + i;
        }

        pColumn = (COLUMNDATA *) (pData + rgBind[iBind].obLength);
        PrintColumn( fp, pColumnInfo, pColumn, rgBind, iBind, cMaxColWidth );
    }
    fprintf( fp, "\n" );
}    

/************************************************************************/
/*                          VARIANTTOString()                           */
/************************************************************************/

const char *VARIANTToString( VARIANT * psV )

{
    static char      szResult[5120];
    VARIANT          sDest;
    HRESULT          hr;
    int              dwLength;

    VariantInit( &sDest );

    hr = VariantChangeTypeEx( &sDest, psV, LOCALE_SYSTEM_DEFAULT, 0, VT_BSTR );

    if( FAILED(hr) )
        return "Translation failed";

    dwLength = sDest.bstrVal[-1] / sizeof(wchar_t);
    sprintf( szResult, "%*.*S", dwLength, dwLength, sDest.bstrVal );

    VariantClear( &sDest );

    return szResult;
}
