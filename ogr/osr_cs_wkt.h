/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  CS WKT parser
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013 Even Rouault, <even dot rouault at mines dash paris dot org>
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

#ifndef _OSR_CS_WKT_H_INCLUDED_
#define _OSR_CS_WKT_H_INCLUDED_

#include "osr_cs_wkt_parser.h"

typedef struct
{
    const char *pszInput;
    const char *pszLastSuccess;
    const char *pszNext;
    char        szErrorMsg[512];
} osr_cs_wkt_parse_context;

#ifdef __cplusplus
extern "C" {
#endif

void osr_cs_wkt_error( osr_cs_wkt_parse_context *context, const char *msg );
int osr_cs_wkt_lex(YYSTYPE* pNode, osr_cs_wkt_parse_context *context);
int osr_cs_wkt_parse(osr_cs_wkt_parse_context *context);

#ifdef __cplusplus
}
#endif

#endif /*  _OSR_CS_WKT_H_INCLUDED_ */
