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
 * Revision 1.1  1999/07/30 11:38:01  warmerda
 * New
 *
 * Revision 1.1  1999/07/25 01:57:24  warmerda
 * New
 *
 * Revision 1.5  1999/05/21 02:38:06  warmerda
 * added AnsiToBSTR()
 *
 * Revision 1.4  1999/04/07 11:52:44  warmerda
 * Added PrintColumn().
 *
 * Revision 1.3  1999/04/01 17:52:58  warmerda
 * added reporting of column info
 *
 * Revision 1.2  1999/03/31 15:11:16  warmerda
 * Use char * instead of LPWSTR, better multi-provider support
 *
 * Revision 1.1  1999/03/30 19:07:59  warmerda
 * New
 *
 */

#include "com_util.h"

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

static char*	GetNoteStringBitvals
	(
	Note* 	rgNote,
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

