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
 * Revision 1.1  1999/07/25 01:57:18  warmerda
 * New
 *
 */

#ifndef OLEDB_SUP_H_INCLUDED
#define OLEDB_SUP_H_INCLUDED

#define WIN32_LEAN_AND_MEAN		// avoid the world
#define INC_OLE2				// tell windows.h to always include ole2.h

#include <windows.h>			// 
#include <ole2ver.h>			// OLE2.0 build version
#include <cguid.h>				// GUID_NULL
#include <stdio.h>				// vsnprintf, etc.
#include <stddef.h>				// offsetof
#include <stdarg.h>				// va_arg
#include <assert.h>				// assert

/* -------------------------------------------------------------------- */
/*      General error reporting.                                        */
/* -------------------------------------------------------------------- */
void DumpErrorMsg( const char * );
HRESULT DumpErrorHResult( HRESULT, const char *, ... );

HRESULT AnsiToUnicode(LPCSTR pszA, LPOLESTR* ppszW);
HRESULT UnicodeToAnsi(LPCOLESTR ppszW, LPSTR *pszA );
HRESULT AnsiToBSTR( const char *, BSTR * );

/* -------------------------------------------------------------------- */
/*      Ole helper functions.                                           */
/* -------------------------------------------------------------------- */
int OleSupInitialize();
int OleSupUninitialize();


//-----------------------------------
//	macros 
//------------------------------------

// Rounding amount is always a power of two.
#define ROUND_UP(   Size, Amount ) (((DWORD)(Size) +  ((Amount) - 1)) & ~((Amount) - 1))

#ifndef  NUMELEM
# define NUMELEM(p) (sizeof(p)/sizeof(*p))
#endif

//-----------------------------------
//	type and structure definitions 
//------------------------------------

// Lists of value/string pairs.
typedef struct {
	DWORD dwFlag;
	char *szText;
} Note;

char * GetNoteString( Note *, int, DWORD );

#define NOTE(s) { (DWORD) s, #s }

#endif /* ndef OLEDB_SUP_H_INCLUDED */


