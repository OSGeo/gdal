/******************************************************************************
 * Copyright (c) 1998, Frank Warmerdam
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
 * cpl_csv.h
 *
 * Prototypes, and stuff for CSV access functions.
 *
 * $Log: 
 */

#ifndef CPL_CSV_H_INCLUDED
#define CPL_CSV_H_INCLUDED

#include "cpl_port.h"
#include "cpl_vsi.h"
#include "cpl_string.h"
#include "cpl_error.h"

CPL_C_START

typedef enum {
    CC_ExactString,
    CC_ApproxString,
    CC_Integer
} CSVCompareCriteria;

char CPL_DLL **CSVReadParseLine( FILE * );
char CPL_DLL **CSVScanLines( FILE *, int, const char *, CSVCompareCriteria );
char CPL_DLL **CSVScanFile( const char *, int, const char *,
                            CSVCompareCriteria );

#define CSVC_

CPL_C_END

#endif /* ndef CPL_CSV_H_INCLUDED */
