%{
/******************************************************************************
 *
 * Component: GDAL
 * Purpose:  Simplified C expression parser
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (C) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#define YYSTYPE  GDAL_c_expr_node*

/* Defining YYSTYPE_IS_TRIVIAL is needed because the parser is generated as a C++ file. */
/* See http://www.gnu.org/s/bison/manual/html_node/Memory-Management.html that suggests */
/* increase YYINITDEPTH instead, but this will consume memory. */
/* Setting YYSTYPE_IS_TRIVIAL overcomes this limitation, but might be fragile because */
/* it appears to be a non documented feature of Bison */
#define YYSTYPE_IS_TRIVIAL 1

%}

%define api.pure
%define parse.error verbose
%require "3.0"

%parse-param {GDAL_c_expr_parse_context *context}
%lex-param {GDAL_c_expr_parse_context *context}

%token C_EXPR_TOK_NUMBER
%token C_EXPR_TOK_IDENTIFIER
%token C_EXPR_TOK_FUNCTION_ZERO_ARG
%token C_EXPR_TOK_FUNCTION_SINGLE_ARG
%token C_EXPR_TOK_FUNCTION_TWO_ARG
%token C_EXPR_TOK_FUNCTION_MULTIPLE_ARG

%token C_EXPR_TOK_START

/* Cf https://en.cppreference.com/w/c/language/operator_precedence.html (in reverse order) */

%left C_EXPR_TOK_TERNARY_THEN
%left C_EXPR_TOK_TERNARY_ELSE
%left C_EXPR_TOK_OR
%left C_EXPR_TOK_AND

%left C_EXPR_TOK_BITWISE_OR
%left C_EXPR_TOK_BITWISE_AND

%left C_EXPR_TOK_EQ C_EXPR_TOK_NE
%left C_EXPR_TOK_LT C_EXPR_TOK_LE C_EXPR_TOK_GT C_EXPR_TOK_GE

%left '+' '-'
%left '*' '/' '%'
%left '^'
%left C_EXPR_TOK_UPLUS C_EXPR_TOK_UMINUS C_EXPR_TOK_NOT

/* Any grammar rule that does $$ =  must be listed afterwards */
/* as well as C_EXPR_TOK_NUMBER C_EXPR_TOK_IDENTIFIER that are allocated by gdal_c_expr_lex() */
%destructor { delete $$; } C_EXPR_TOK_NUMBER C_EXPR_TOK_IDENTIFIER C_EXPR_TOK_FUNCTION_ZERO_ARG C_EXPR_TOK_FUNCTION_SINGLE_ARG C_EXPR_TOK_FUNCTION_TWO_ARG C_EXPR_TOK_FUNCTION_MULTIPLE_ARG
%destructor { delete $$; } value_expr value_expr_list

%%

input:
    C_EXPR_TOK_START value_expr
        {
            context->poRoot.reset($2);
        }

value_expr:

    C_EXPR_TOK_NUMBER
        {
            $$ = $1;
        }

    | C_EXPR_TOK_IDENTIFIER
        {
            $$ = $1;
        }

    | C_EXPR_TOK_FUNCTION_ZERO_ARG '(' ')'
        {
            $$ = $1;
        }

    | C_EXPR_TOK_FUNCTION_SINGLE_ARG '(' value_expr ')'
        {
            $$ = $1;
            $$->PushSubExpression( $3 );
        }

    | C_EXPR_TOK_FUNCTION_TWO_ARG '(' value_expr ',' value_expr ')'
        {
            $$ = $1;
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
        }

    | C_EXPR_TOK_FUNCTION_MULTIPLE_ARG '(' value_expr_list ')'
        {
            $$ = $1;
            $3->ReverseSubExpressions();
            $$->apoSubExpr = std::move($3->apoSubExpr);
            delete $3;
        }

    | value_expr C_EXPR_TOK_AND value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_AND );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr C_EXPR_TOK_OR value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_OR );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr C_EXPR_TOK_BITWISE_AND value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_BITWISE_AND );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr C_EXPR_TOK_BITWISE_OR value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_BITWISE_OR );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | C_EXPR_TOK_NOT value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_NOT );
            $$->PushSubExpression( $2 );
        }

    | value_expr C_EXPR_TOK_TERNARY_THEN value_expr C_EXPR_TOK_TERNARY_ELSE value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_TERNARY );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
        }

    | '(' value_expr ')'
        {
            $$ = $2;
        }

    | value_expr C_EXPR_TOK_EQ value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_EQ );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr C_EXPR_TOK_NE value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_NE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr C_EXPR_TOK_LT value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_LT );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr C_EXPR_TOK_GT value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_GT );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr C_EXPR_TOK_LE value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_LE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr C_EXPR_TOK_GE value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_GE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | '+' value_expr %prec C_EXPR_TOK_UPLUS
        {
            $$ = $2;
        }

    | '-' value_expr %prec C_EXPR_TOK_UMINUS
        {
            if ($2->eNodeType == CENT_CONSTANT &&
                ($2->field_type == C_EXPR_FIELD_TYPE_INTEGER ||
                 $2->field_type == C_EXPR_FIELD_TYPE_FLOAT) &&
                !($2->field_type == C_EXPR_FIELD_TYPE_INTEGER &&
                  $2->int_value == INT64_MIN))
            {
                $$ = $2;
                $$->int_value *= -1;
                $$->float_value *= -1;
            }
            else
            {
                $$ = new GDAL_c_expr_node( C_EXPR_MULTIPLY );
                $$->PushSubExpression( new GDAL_c_expr_node(-1) );
                $$->PushSubExpression( $2 );
            }
        }

    | value_expr '^' value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_POWER );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '+' value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_ADD );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '-' value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_SUBTRACT );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '*' value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_MULTIPLY );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '/' value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_DIVIDE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '%' value_expr
        {
            $$ = new GDAL_c_expr_node( C_EXPR_MODULUS );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

value_expr_list:
    value_expr ',' value_expr_list
        {
            $$ = $3;
            $3->PushSubExpression( $1 );
        }

    | value_expr
            {
            $$ = new GDAL_c_expr_node( C_EXPR_LIST );
            $$->PushSubExpression( $1 );
        }
