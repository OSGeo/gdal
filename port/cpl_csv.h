/******************************************************************************
 *
 * Project:  Common Portability Library
 * Purpose:  Functions for reading and scanning CSV (comma separated,
 *           variable length text files holding tables) files.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef CPL_CSV_H_INCLUDED
#define CPL_CSV_H_INCLUDED

#include <stdio.h>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include <stdbool.h>

CPL_C_START

typedef enum
{
    CC_ExactString,
    CC_ApproxString,
    CC_Integer
} CSVCompareCriteria;

const char CPL_DLL *CSVFilename(const char *);

char CPL_DLL CSVDetectSeperator(const char *pszLine);

char CPL_DLL **CSVReadParseLine(FILE *fp);
char CPL_DLL **CSVReadParseLine2(FILE *fp, char chDelimiter);

char CPL_DLL **CSVReadParseLineL(VSILFILE *fp);
char CPL_DLL **CSVReadParseLine2L(VSILFILE *fp, char chDelimiter);

char CPL_DLL **CSVReadParseLine3L(VSILFILE *fp, size_t nMaxLineSize,
                                  const char *pszDelimiter, bool bHonourStrings,
                                  bool bKeepLeadingAndClosingQuotes,
                                  bool bMergeDelimiter, bool bSkipBOM);

char CPL_DLL **CSVScanLines(FILE *, int, const char *, CSVCompareCriteria);
char CPL_DLL **CSVScanLinesL(VSILFILE *, int, const char *, CSVCompareCriteria);
char CPL_DLL **CSVScanFile(const char *, int, const char *, CSVCompareCriteria);
char CPL_DLL **CSVScanFileByName(const char *, const char *, const char *,
                                 CSVCompareCriteria);
void CPL_DLL CSVRewind(const char *);
char CPL_DLL **CSVGetNextLine(const char *);
int CPL_DLL CSVGetFieldId(FILE *, const char *);
int CPL_DLL CSVGetFieldIdL(VSILFILE *, const char *);
int CPL_DLL CSVGetFileFieldId(const char *, const char *);

void CPL_DLL CSVDeaccess(const char *);

const char CPL_DLL *CSVGetField(const char *, const char *, const char *,
                                CSVCompareCriteria, const char *);

#ifndef DOXYGEN_XML
void CPL_DLL SetCSVFilenameHook(const char *(*)(const char *));
#endif

CPL_C_END

#endif /* ndef CPL_CSV_H_INCLUDED */
