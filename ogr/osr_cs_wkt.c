/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  CS WKT parser
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <stdio.h>
#include <string.h>

#define EQUALN_CST(x,y)  (strncmp(x, y, strlen(y)) == 0)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#include "osr_cs_wkt.h"

#include "cpl_port.h"

/************************************************************************/
/*                        osr_cs_wkt_error()                            */
/************************************************************************/

void osr_cs_wkt_error( osr_cs_wkt_parse_context *context, const char *msg )
{
    int i, n;
    char* szPtr;
    sprintf(context->szErrorMsg,
            "Parsing error : %s. Error occured around:\n", msg );
    n = context->pszLastSuccess - context->pszInput;

    szPtr = context->szErrorMsg + strlen(context->szErrorMsg);
    for( i = MAX(0,n-40); i < n + 40 && context->pszInput[i]; i ++ )
        *(szPtr ++) = context->pszInput[i];
    *(szPtr ++) = '\n';
    for(i=0;i<MIN(n,40);i++)
        *(szPtr ++) = ' ';
    *(szPtr ++) = '^';
    *(szPtr ++) = '\0';
}

typedef struct
{
    const char* pszToken;
    int         nTokenVal;
} osr_cs_wkt_tokens;

#define PAIR(X) { #X, T_ ##X }

static const osr_cs_wkt_tokens tokens[] =
{
    PAIR(PARAM_MT),
    PAIR(PARAMETER),
    PAIR(CONCAT_MT),
    PAIR(INVERSE_MT),
    PAIR(PASSTHROUGH_MT),

    PAIR(PROJCS),
    PAIR(PROJECTION),
    PAIR(GEOGCS),
    PAIR(DATUM),
    PAIR(SPHEROID),
    PAIR(PRIMEM),
    PAIR(UNIT),
    PAIR(GEOCCS),
    PAIR(AUTHORITY),
    PAIR(VERT_CS),
    PAIR(VERT_DATUM),
    PAIR(COMPD_CS),
    PAIR(AXIS),
    PAIR(TOWGS84),
    PAIR(FITTED_CS),
    PAIR(LOCAL_CS),
    PAIR(LOCAL_DATUM),

    PAIR(EXTENSION)
};

/************************************************************************/
/*                         osr_cs_wkt_lex()                             */
/************************************************************************/

int osr_cs_wkt_lex(CPL_UNUSED YYSTYPE* pNode, osr_cs_wkt_parse_context *context)
{
    size_t i;
    const char *pszInput = context->pszNext;

/* -------------------------------------------------------------------- */
/*      Skip white space.                                               */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t'
           || *pszInput == 10 || *pszInput == 13 )
        pszInput++;

    context->pszLastSuccess = pszInput;

    if( *pszInput == '\0' )
    {
        context->pszNext = pszInput;
        return EOF; 
    }

/* -------------------------------------------------------------------- */
/*      Recognize node names.                                           */
/* -------------------------------------------------------------------- */
    for(i = 0; i < sizeof(tokens) / sizeof(tokens[0]); i++)
    {
        if( EQUALN_CST(pszInput, tokens[i].pszToken) )
        {
            context->pszNext = pszInput + strlen(tokens[i].pszToken);
            return tokens[i].nTokenVal;
        }
    }

/* -------------------------------------------------------------------- */
/*      Recognize double quoted strings.                                */
/* -------------------------------------------------------------------- */
    if( *pszInput == '"' )
    {
        pszInput ++;
        while( *pszInput != '\0' && *pszInput != '"' )
            pszInput ++;
        if(  *pszInput == '\0' )
        {
            context->pszNext = pszInput;
            return EOF; 
        }
        context->pszNext = pszInput + 1;
        return T_STRING;
    }

/* -------------------------------------------------------------------- */
/*      Recognize numerical values.                                     */
/* -------------------------------------------------------------------- */

    if( ((*pszInput == '-' || *pszInput == '+') &&
            pszInput[1] >= '0' && pszInput[1] <= '9' ) ||
        (*pszInput >= '0' && *pszInput <= '9') )
    {
        if( *pszInput == '-' || *pszInput == '+' )
            pszInput ++;

        // collect non-decimal part of number
        while( *pszInput >= '0' && *pszInput <= '9' )
            pszInput++;

        // collect decimal places.
        if( *pszInput == '.' )
        {
            pszInput++;
            while( *pszInput >= '0' && *pszInput <= '9' )
                pszInput++;
        }

        // collect exponent
        if( *pszInput == 'e' || *pszInput == 'E' )
        {
            pszInput++;
            if( *pszInput == '-' || *pszInput == '+' )
                pszInput++;
            while( *pszInput >= '0' && *pszInput <= '9' )
                pszInput++;
        }

        context->pszNext = pszInput;

        return T_NUMBER;
    }

/* -------------------------------------------------------------------- */
/*      Recognize identifiers.                                          */
/* -------------------------------------------------------------------- */
    if( (*pszInput >= 'A' && *pszInput <= 'Z') ||
        (*pszInput >= 'a' && *pszInput <= 'z'))
    {
        pszInput ++;
        while( (*pszInput >= 'A' && *pszInput <= 'Z') ||
               (*pszInput >= 'a' && *pszInput <= 'z') )
            pszInput ++;
        context->pszNext = pszInput;
        return T_IDENTIFIER;
    }

/* -------------------------------------------------------------------- */
/*      Handle special tokens.                                          */
/* -------------------------------------------------------------------- */
    context->pszNext = pszInput+1;
    return *pszInput;
}
