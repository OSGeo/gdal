/******************************************************************************
 * $Id$
 *
 * Component: ODS formula Engine
 * Purpose:
 * Author: Even Rouault <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines-paris dot org>
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

#include <ctype.h>
#include <math.h>

#include "cpl_conv.h"
#include "ods_formula.h"
#include "ods_formula_parser.hpp"

#define YYSTYPE  ods_formula_node*

static const SingleOpStruct apsSingleOp[] =
{
    { "ABS", ODS_ABS, fabs },
    { "SQRT", ODS_SQRT, sqrt },
    { "COS", ODS_COS, cos },
    { "SIN", ODS_SIN, sin },
    { "TAN", ODS_TAN, tan },
    { "ACOS", ODS_ACOS, acos },
    { "ASIN", ODS_ASIN, asin },
    { "ATAN", ODS_ATAN, atan },
    { "EXP", ODS_EXP, exp },
    { "LN", ODS_LN, log },
    { "LOG", ODS_LOG, log10 },
    { "LOG10", ODS_LOG, log10 },
};

const SingleOpStruct* ODSGetSingleOpEntry(const char* pszName)
{
    for(size_t i = 0; i < sizeof(apsSingleOp) / sizeof(apsSingleOp[0]); i++)
    {
        if (EQUAL(pszName, apsSingleOp[i].pszName))
            return &apsSingleOp[i];
    }
    return NULL;
}

const SingleOpStruct* ODSGetSingleOpEntry(ods_formula_op eOp)
{
    for(size_t i = 0; i < sizeof(apsSingleOp) / sizeof(apsSingleOp[0]); i++)
    {
        if (eOp == apsSingleOp[i].eOp)
            return &apsSingleOp[i];
    }
    return NULL;
}

/************************************************************************/
/*                               swqlex()                               */
/*                                                                      */
/*      Read back a token from the input.                               */
/************************************************************************/

int ods_formulalex( YYSTYPE *ppNode, ods_formula_parse_context *context )
{
    const char *pszInput = context->pszNext;

    *ppNode = NULL;

/* -------------------------------------------------------------------- */
/*      Do we have a start symbol to return?                            */
/* -------------------------------------------------------------------- */
    if( context->nStartToken != 0 )
    {
        int nRet = context->nStartToken;
        context->nStartToken = 0;
        return nRet;
    }

/* -------------------------------------------------------------------- */
/*      Skip white space.                                               */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t'
           || *pszInput == 10 || *pszInput == 13 )
        pszInput++;

    if( *pszInput == '\0' )
    {
        context->pszNext = pszInput;
        return EOF; 
    }

/* -------------------------------------------------------------------- */
/*      Handle string constants.                                        */
/* -------------------------------------------------------------------- */
    if( *pszInput == '"' )
    {
        char *token;
        int i_token;

        pszInput++;

        token = (char *) CPLMalloc(strlen(pszInput)+1);
        i_token = 0;

        while( *pszInput != '\0' )
        {
            if( *pszInput == '\\' && pszInput[1] == '"' )
                pszInput++;
            else if( *pszInput == '\\' && pszInput[1] == '\'' )
                pszInput++;
            else if( *pszInput == '\'' && pszInput[1] == '\'' )
                pszInput++;
            else if( *pszInput == '"' )
            {
                pszInput++;
                break;
            }
            else if( *pszInput == '\'' )
            {
                pszInput++;
                break;
            }
            
            token[i_token++] = *(pszInput++);
        }
        token[i_token] = '\0';

        *ppNode = new ods_formula_node( token );
        CPLFree( token );

        context->pszNext = pszInput;

        return ODST_STRING;
    }

/* -------------------------------------------------------------------- */
/*      Handle numbers.                                                 */
/* -------------------------------------------------------------------- */
    else if( *pszInput >= '0' && *pszInput <= '9' )
    {
        CPLString osToken;
        const char *pszNext = pszInput + 1;

        osToken += *pszInput;

        // collect non-decimal part of number
        while( *pszNext >= '0' && *pszNext <= '9' )
            osToken += *(pszNext++);

        // collect decimal places.
        if( *pszNext == '.' )
        {
            osToken += *(pszNext++);
            while( *pszNext >= '0' && *pszNext <= '9' )
                osToken += *(pszNext++);
        }

        // collect exponent
        if( *pszNext == 'e' || *pszNext == 'E' )
        {
            osToken += *(pszNext++);
            if( *pszNext == '-' || *pszNext == '+' )
                osToken += *(pszNext++);
            while( *pszNext >= '0' && *pszNext <= '9' )
                osToken += *(pszNext++);
        }

        context->pszNext = pszNext;

        if( strstr(osToken,".") 
            || strstr(osToken,"e") 
            || strstr(osToken,"E") )
        {
            *ppNode = new ods_formula_node( CPLAtof(osToken) );
        }
        else
        {
            *ppNode = new ods_formula_node( atoi(osToken) );
        }

        return ODST_NUMBER;
    }

/* -------------------------------------------------------------------- */
/*      Handle alpha-numerics.                                          */
/* -------------------------------------------------------------------- */
    else if( *pszInput == '.' || isalnum( *pszInput ) )
    {
        int nReturn = ODST_IDENTIFIER;
        CPLString osToken;
        const char *pszNext = pszInput + 1;

        osToken += *pszInput;

        // collect text characters
        while( isalnum( *pszNext ) || *pszNext == '_' 
               || ((unsigned char) *pszNext) > 127 )
            osToken += *(pszNext++);

        context->pszNext = pszNext;

        /* Constants */
        if( EQUAL(osToken,"TRUE") )
        {
            *ppNode = new ods_formula_node( 1 );
            return ODST_NUMBER;
        }
        else if( EQUAL(osToken,"FALSE") )
        {
            *ppNode = new ods_formula_node( 0 );
            return ODST_NUMBER;
        }

        else if( EQUAL(osToken,"NOT") )
            nReturn = ODST_NOT;
        else if( EQUAL(osToken,"AND") )
            nReturn = ODST_AND;
        else if( EQUAL(osToken,"OR") )
            nReturn = ODST_OR;
        else if( EQUAL(osToken,"IF") )
            nReturn = ODST_IF;

        /* No-arg functions */
        else if( EQUAL(osToken,"PI") )
        {
            *ppNode = new ods_formula_node( ODS_PI );
            return ODST_FUNCTION_NO_ARG;
        }

        /* Single-arg functions */
        else if( EQUAL(osToken,"LEN") )
        {
            *ppNode = new ods_formula_node( ODS_LEN );
            return ODST_FUNCTION_SINGLE_ARG;
        }
        /*
        else if( EQUAL(osToken,"T") )
        {
            *ppNode = new ods_formula_node( ODS_T );
            return ODST_FUNCTION_SINGLE_ARG;
        }*/
        
        /* Tow-arg functions */
        else if( EQUAL(osToken,"MOD") )
        {
            *ppNode = new ods_formula_node( ODS_MODULUS );
            return ODST_FUNCTION_TWO_ARG;
        }
        else if( EQUAL(osToken,"LEFT") )
        {
            *ppNode = new ods_formula_node( ODS_LEFT );
            return ODST_FUNCTION_TWO_ARG;
        }
        else if( EQUAL(osToken,"RIGHT") )
        {
            *ppNode = new ods_formula_node( ODS_RIGHT );
            return ODST_FUNCTION_TWO_ARG;
        }

        /* Three-arg functions */
        else if( EQUAL(osToken,"MID") )
        {
            *ppNode = new ods_formula_node( ODS_MID );
            return ODST_FUNCTION_THREE_ARG;
        }

        /* Multiple-arg functions */
        else if( EQUAL(osToken,"SUM") )
        {
            *ppNode = new ods_formula_node( ODS_SUM );
            nReturn = ODST_FUNCTION_ARG_LIST;
        }
        else if( EQUAL(osToken,"AVERAGE") )
        {
            *ppNode = new ods_formula_node( ODS_AVERAGE );
            nReturn = ODST_FUNCTION_ARG_LIST;
        }
        else if( EQUAL(osToken,"MIN") )
        {
            *ppNode = new ods_formula_node( ODS_MIN );
            nReturn = ODST_FUNCTION_ARG_LIST;
        }
        else if( EQUAL(osToken,"MAX") )
        {
            *ppNode = new ods_formula_node( ODS_MAX );
            nReturn = ODST_FUNCTION_ARG_LIST;
        }
        else if( EQUAL(osToken,"COUNT") )
        {
            *ppNode = new ods_formula_node( ODS_COUNT );
            nReturn = ODST_FUNCTION_ARG_LIST;
        }
        else if( EQUAL(osToken,"COUNTA") )
        {
            *ppNode = new ods_formula_node( ODS_COUNTA );
            nReturn = ODST_FUNCTION_ARG_LIST;
        }

        else
        {
            const SingleOpStruct* psSingleOp = ODSGetSingleOpEntry(osToken);
            if (psSingleOp != NULL)
            {
                *ppNode = new ods_formula_node( psSingleOp->eOp );
                nReturn = ODST_FUNCTION_SINGLE_ARG;
            }
            else
            {
                *ppNode = new ods_formula_node( osToken );
                nReturn = ODST_IDENTIFIER;
            }
        }

        return nReturn;
    }

/* -------------------------------------------------------------------- */
/*      Handle special tokens.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        context->pszNext = pszInput+1;
        return *pszInput;
    }
}

/************************************************************************/
/*                        ods_formula_compile()                         */
/************************************************************************/

ods_formula_node* ods_formula_compile( const char *expr )

{
    ods_formula_parse_context context;

    context.pszInput = expr;
    context.pszNext = expr;
    context.nStartToken = ODST_START;

    if( ods_formulaparse( &context ) == 0 )
    {
        return context.poRoot;
    }
    else
    {
        delete context.poRoot;
        return NULL;
    }
}
