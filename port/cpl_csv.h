/******************************************************************************
 * $Id$
 *
 * Project:  Common Portability Library
 * Purpose:  Functions for reading and scanning CSV (comma separated,
 *           variable length text files holding tables) files.
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

#ifndef CPL_CSV_H_INCLUDED
#define CPL_CSV_H_INCLUDED

#include <stdio.h>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include <stdbool.h>

CPL_C_START

typedef enum {
    CC_ExactString,
    CC_ApproxString,
    CC_Integer
} CSVCompareCriteria;

const char CPL_DLL *CSVFilename( const char * );

char CPL_DLL CSVDetectSeperator( const char *pszLine );

char CPL_DLL  **CSVReadParseLine( FILE *fp);
char CPL_DLL  **CSVReadParseLine2( FILE *fp, char chDelimiter );

char CPL_DLL  **CSVReadParseLineL( VSILFILE *fp);
char CPL_DLL  **CSVReadParseLine2L( VSILFILE *fp, char chDelimiter );

char CPL_DLL **CSVReadParseLine3L( VSILFILE *fp,
                                   size_t nMaxLineSize,
                                   const char* pszDelimiter,
                                   bool bHonourStrings,
                                   bool bKeepLeadingAndClosingQuotes,
                                   bool bMergeDelimiter,
                                   bool bSkipBOM );

char CPL_DLL **CSVScanLines( FILE *, int, const char *, CSVCompareCriteria );
char CPL_DLL **CSVScanLinesL( VSILFILE *, int, const char *, CSVCompareCriteria );
char CPL_DLL **CSVScanFile( const char *, int, const char *,
                            CSVCompareCriteria );
char CPL_DLL **CSVScanFileByName( const char *, const char *, const char *,
                                  CSVCompareCriteria );
char CPL_DLL **CSVGetNextLine( const char * );
int CPL_DLL CSVGetFieldId( FILE *, const char * );
int CPL_DLL CSVGetFieldIdL( VSILFILE *, const char * );
int CPL_DLL CSVGetFileFieldId( const char *, const char * );

void CPL_DLL CSVDeaccess( const char * );

const char CPL_DLL *CSVGetField( const char *, const char *, const char *,
                                 CSVCompareCriteria, const char * );

#ifndef DOXYGEN_XML
void CPL_DLL SetCSVFilenameHook( const char *(*)(const char *) );
#endif

CPL_C_END

#endif /* ndef CPL_CSV_H_INCLUDED */
