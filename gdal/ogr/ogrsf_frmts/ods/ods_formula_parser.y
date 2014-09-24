%{
/******************************************************************************
 * $Id$
 *
 * Component: OGR ODS Formula Engine
 * Purpose: expression and select parser grammar.
 *          Requires Bison 2.4.0 or newer to process.  Use "make parser" target.
 * Author:   Even Rouault, even dot rouault at mines dash paris dot org
 * 
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ods_formula.h"

#define YYSTYPE  ods_formula_node*

/* Defining YYSTYPE_IS_TRIVIAL is needed because the parser is generated as a C++ file. */ 
/* See http://www.gnu.org/s/bison/manual/html_node/Memory-Management.html that suggests */ 
/* increase YYINITDEPTH instead, but this will consume memory. */ 
/* Setting YYSTYPE_IS_TRIVIAL overcomes this limitation, but might be fragile because */ 
/* it appears to be a non documented feature of Bison */ 
#define YYSTYPE_IS_TRIVIAL 1

static void ods_formulaerror( CPL_UNUSED ods_formula_parse_context *context, const char *msg )
{
    CPLError( CE_Failure, CPLE_AppDefined, 
              "Formula Parsing Error: %s", msg );
}

%}

%define api.pure
%require "2.4.0"

%parse-param {ods_formula_parse_context *context}
%lex-param {ods_formula_parse_context *context}

%token ODST_NUMBER
%token ODST_STRING
%token ODST_IDENTIFIER
%token ODST_FUNCTION_NO_ARG
%token ODST_FUNCTION_SINGLE_ARG
%token ODST_FUNCTION_TWO_ARG
%token ODST_FUNCTION_THREE_ARG
%token ODST_FUNCTION_ARG_LIST

%token ODST_START

%left ODST_NOT
%left ODST_OR
%left ODST_AND
%left ODST_IF

%left '+' '-' '&'
%left '*' '/' '%'
%left ODST_UMINUS

/* Any grammar rule that does $$ =  must be listed afterwards */
/* as well as ODST_NUMBER ODST_STRING ODST_IDENTIFIER that are allocated by ods_formulalex() */
%destructor { delete $$; } ODST_NUMBER ODST_STRING ODST_IDENTIFIER ODST_FUNCTION_NO_ARG ODST_FUNCTION_SINGLE_ARG ODST_FUNCTION_TWO_ARG ODST_FUNCTION_THREE_ARG ODST_FUNCTION_ARG_LIST
%destructor { delete $$; } value_expr value_expr_list cell_range value_expr_and_cell_range_list

%%

input:
    ODST_START value_expr
        {
            context->poRoot = $2;
        }

comma: ',' | ';'

value_expr:

    ODST_NUMBER
        {
            $$ = $1;
        }

    | ODST_STRING
        {
            $$ = $1;
        }

    | ODST_FUNCTION_NO_ARG '(' ')'
        {
            $$ = $1;
        }

    | ODST_FUNCTION_SINGLE_ARG '(' value_expr ')'
        {
            $$ = $1;
            $$->PushSubExpression( $3 );
        }

    | ODST_FUNCTION_TWO_ARG '(' value_expr comma value_expr ')'
        {
            $$ = $1;
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
        }

    | ODST_FUNCTION_THREE_ARG '(' value_expr comma value_expr comma value_expr ')'
        {
            $$ = $1;
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
            $$->PushSubExpression( $7 );
        }

    | ODST_AND '(' value_expr_list ')'
        {
            $$ = new ods_formula_node( ODS_AND );
            $3->ReverseSubExpressions();
            $$->PushSubExpression( $3 );
        }

    | ODST_OR '(' value_expr_list ')'
        {
            $$ = new ods_formula_node( ODS_OR );
            $3->ReverseSubExpressions();
            $$->PushSubExpression( $3 );
        }

    | ODST_NOT '(' value_expr ')'
        {
            $$ = new ods_formula_node( ODS_NOT );
            $$->PushSubExpression( $3 );
        }

    | ODST_IF '(' value_expr comma value_expr ')'
        {
            $$ = new ods_formula_node( ODS_IF );
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
        }

    | ODST_IF '(' value_expr comma value_expr comma value_expr ')'
        {
            $$ = new ods_formula_node( ODS_IF );
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
            $$->PushSubExpression( $7 );
        }

    | ODST_FUNCTION_ARG_LIST '(' value_expr_and_cell_range_list ')'
        {
            $$ = $1;
            $3->ReverseSubExpressions();
            $$->PushSubExpression( $3 );
        }

    | '(' value_expr ')'
        {
            $$ = $2;
        }

    | value_expr '=' value_expr
        {
            $$ = new ods_formula_node( ODS_EQ );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '<' '>' value_expr
        {
            $$ = new ods_formula_node( ODS_NE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '!' '=' value_expr
        {
            $$ = new ods_formula_node( ODS_NE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '<' value_expr
        {
            $$ = new ods_formula_node( ODS_LT );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '>' value_expr
        {
            $$ = new ods_formula_node( ODS_GT );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '<' '=' value_expr
        {
            $$ = new ods_formula_node( ODS_LE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '=' '<' value_expr
        {
            $$ = new ods_formula_node( ODS_LE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '=' '>' value_expr
        {
            $$ = new ods_formula_node( ODS_LE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '>' '=' value_expr
        {
            $$ = new ods_formula_node( ODS_GE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | '-' value_expr %prec ODST_UMINUS
        {
            if ($2->eNodeType == SNT_CONSTANT)
            {
                $$ = $2;
                $$->int_value *= -1;
                $$->float_value *= -1;
            }
            else
            {
                $$ = new ods_formula_node( ODS_MULTIPLY );
                $$->PushSubExpression( new ods_formula_node(-1) );
                $$->PushSubExpression( $2 );
            }
        }

    | value_expr '+' value_expr
        {
            $$ = new ods_formula_node( ODS_ADD );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '-' value_expr
        {
            $$ = new ods_formula_node( ODS_SUBTRACT );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '&' value_expr
        {
            $$ = new ods_formula_node( ODS_CONCAT );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '*' value_expr
        {
            $$ = new ods_formula_node( ODS_MULTIPLY );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '/' value_expr
        {
            $$ = new ods_formula_node( ODS_DIVIDE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '%' value_expr
        {
            $$ = new ods_formula_node( ODS_MODULUS );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

     | '[' ODST_IDENTIFIER ']'
        {
            $$ = new ods_formula_node( ODS_CELL );
            $$->PushSubExpression( $2 );
        }

value_expr_list:
    value_expr comma value_expr_list
        {
            $$ = $3;
            $3->PushSubExpression( $1 );
        }

    | value_expr
            {
            $$ = new ods_formula_node( ODS_LIST );
            $$->PushSubExpression( $1 );
        }

value_expr_and_cell_range_list:
    value_expr comma value_expr_and_cell_range_list
        {
            $$ = $3;
            $3->PushSubExpression( $1 );
        }

    | value_expr
            {
            $$ = new ods_formula_node( ODS_LIST );
            $$->PushSubExpression( $1 );
        }
    | cell_range comma value_expr_and_cell_range_list
        {
            $$ = $3;
            $3->PushSubExpression( $1 );
        }

    | cell_range
            {
            $$ = new ods_formula_node( ODS_LIST );
            $$->PushSubExpression( $1 );
        }

cell_range:
    '[' ODST_IDENTIFIER ':' ODST_IDENTIFIER ']'
        {
            $$ = new ods_formula_node( ODS_CELL_RANGE );
            $$->PushSubExpression( $2 );
            $$->PushSubExpression( $4 );
        }
