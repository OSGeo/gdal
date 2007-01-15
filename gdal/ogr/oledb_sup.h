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
 ****************************************************************************/

#ifndef OLEDB_SUP_H_INCLUDED
#define OLEDB_SUP_H_INCLUDED

#define WIN32_LEAN_AND_MEAN             // avoid the world
#define INC_OLE2                                // tell windows.h to always include ole2.h

#include <windows.h>                    // 
#include <ole2ver.h>                    // OLE2.0 build version
#include <cguid.h>                              // GUID_NULL
#include <stdio.h>                              // vsnprintf, etc.
#include <stddef.h>                             // offsetof
#include <stdarg.h>                             // va_arg
#include <assert.h>                             // assert

//      OLE DB headers
#include <oledb.h>
#include <oledberr.h>

/* -------------------------------------------------------------------- */
/*      General error reporting.                                        */
/* -------------------------------------------------------------------- */
void DumpErrorMsg( const char * );
HRESULT DumpErrorHResult( HRESULT, const char *, ... );

HRESULT AnsiToUnicode(LPCSTR pszA, LPOLESTR* ppszW);
HRESULT UnicodeToAnsi(LPCOLESTR ppszW, LPSTR *pszA );
HRESULT AnsiToBSTR( const char *, BSTR * );

extern "C" {
const char *VARIANTToString( VARIANT * );
}

/* -------------------------------------------------------------------- */
/*      Ole helper functions.                                           */
/* -------------------------------------------------------------------- */
int OleSupInitialize();
int OleSupUninitialize();

void OledbSupWriteColumnInfo( FILE *, DBCOLUMNINFO * );
void OledbSupDumpRow( FILE *, DBCOLUMNINFO *, int, DBBINDING *,
                      ULONG, ULONG, BYTE * );

/* -------------------------------------------------------------------- */
/*                       Constants from sampclnt.                       */
/* -------------------------------------------------------------------- */

// Alignment for placement of each column within memory.
// Rule of thumb is "natural" boundary, i.e. 4-byte member should be
// aligned on address that is multiple of 4.
// Worst case is double or __int64 (8 bytes).
#define COLUMN_ALIGNVAL 8

#define MAX_GUID_STRING     42  // size of a GUID, in characters
#define MAX_NAME_STRING     60  // size of DBCOLOD name or propid string
#define MAX_BINDINGS       100  // size of binding array
#define NUMROWS_CHUNK       20  // number of rows to grab at a time
#define DEFAULT_CBMAXLENGTH 40  // cbMaxLength for binding

//-----------------------------------
//      macros 
//------------------------------------

// Rounding amount is always a power of two.
#define ROUND_UP(   Size, Amount ) (((DWORD)(Size) +  ((Amount) - 1)) & ~((Amount) - 1))

#ifndef  NUMELEM
# define NUMELEM(p) (sizeof(p)/sizeof(*p))
#endif

//-----------------------------------
//      type and structure definitions 
//------------------------------------

// How to lay out each column in memory.
// Issue? we depend on the dwLength field being first in memory (see assert)
// is there another way to handle this?
struct COLUMNDATA 
        {
        DWORD           dwLength;       // length of data (not space allocated)
        DWORD           dwStatus;       // status of column
        BYTE            bData[1];       // data here and beyond
        };


// Lists of value/string pairs.
typedef struct {
        DWORD dwFlag;
        char *szText;
} Note;

char * GetNoteString( Note *, int, DWORD );

#define NOTE(s) { (DWORD) s, #s }

#endif /* ndef OLEDB_SUP_H_INCLUDED */
