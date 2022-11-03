%{
/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: expression and select parser grammar.
 *          Requires Bison 2.4.0 or newer to process.  Use "make parser" target.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
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

#include "cpl_port.h"
#include "ogr_swq.h"

#include <cstdlib>
#include <cstring>
#include <limits>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_geometry.h"


#define YYSTYPE swq_expr_node *

/* Defining YYSTYPE_IS_TRIVIAL is needed because the parser is generated as a C++ file. */
/* See http://www.gnu.org/s/bison/manual/html_node/Memory-Management.html that suggests */
/* increase YYINITDEPTH instead, but this will consume memory. */
/* Setting YYSTYPE_IS_TRIVIAL overcomes this limitation, but might be fragile because */
/* it appears to be a non documented feature of Bison */
#define YYSTYPE_IS_TRIVIAL 1
%}

%define api.pure
/* if the next %define is commented out, Bison 2.4 should be sufficient */
/* but will produce less prettier error messages */
%define parse.error verbose
%require "3.0"

%parse-param {swq_parse_context *context}
%lex-param {swq_parse_context *context}

%token SWQT_INTEGER_NUMBER      "integer number"
%token SWQT_FLOAT_NUMBER        "floating point number"
%token SWQT_STRING              "string"
%token SWQT_IDENTIFIER          "identifier"
%token SWQT_IN                  "IN"
%token SWQT_LIKE                "LIKE"
%token SWQT_ILIKE               "ILIKE"
%token SWQT_ESCAPE              "ESCAPE"
%token SWQT_BETWEEN             "BETWEEN"
%token SWQT_NULL                "NULL"
%token SWQT_IS                  "IS"
%token SWQT_SELECT              "SELECT"
%token SWQT_LEFT                "LEFT"
%token SWQT_JOIN                "JOIN"
%token SWQT_WHERE               "WHERE"
%token SWQT_ON                  "ON"
%token SWQT_ORDER               "ORDER"
%token SWQT_BY                  "BY"
%token SWQT_FROM                "FROM"
%token SWQT_AS                  "AS"
%token SWQT_ASC                 "ASC"
%token SWQT_DESC                "DESC"
%token SWQT_DISTINCT            "DISTINCT"
%token SWQT_CAST                "CAST"
%token SWQT_UNION               "UNION"
%token SWQT_ALL                 "ALL"
%token SWQT_LIMIT               "LIMIT"
%token SWQT_OFFSET              "OFFSET"

%token SWQT_VALUE_START
%token SWQT_SELECT_START

%token END 0                    "end of string"

%token SWQT_NOT                 "NOT"
%token SWQT_OR                  "OR"
%token SWQT_AND                 "AND"

%left SWQT_OR
%left SWQT_AND
%left SWQT_NOT

%left '=' '<' '>' '!' SWQT_BETWEEN SWQT_IN SWQT_LIKE SWQT_ILIKE SWQT_IS
%left SWQT_ESCAPE

%left '+' '-'
%left '*' '/' '%'
%left SWQT_UMINUS

%token SWQT_RESERVED_KEYWORD    "reserved keyword"

/* Any grammar rule that does $$ = must be listed afterwards */
/* as well as SWQT_INTEGER_NUMBER SWQT_FLOAT_NUMBER SWQT_STRING SWQT_IDENTIFIER that are allocated by swqlex() */
%destructor { delete $$; } SWQT_INTEGER_NUMBER SWQT_FLOAT_NUMBER SWQT_STRING SWQT_IDENTIFIER
%destructor { delete $$; } value_expr_list field_value value_expr value_expr_non_logical type_def table_def

%%

input:
    | SWQT_VALUE_START value_expr
        {
            context->poRoot = $2;
            swq_fixup(context);
        }

    | SWQT_SELECT_START select_statement
        {
            context->poRoot = $2;
            swq_fixup(context);
        }

value_expr:
    value_expr_non_logical
        {
            $$ = $1;
        }

    | value_expr SWQT_AND value_expr
        {
            $$ = swq_create_and_or_or( SWQ_AND, $1, $3 );
        }

    | value_expr SWQT_OR value_expr
        {
            $$ = swq_create_and_or_or( SWQ_OR, $1, $3 );
        }

    | SWQT_NOT value_expr
        {
            $$ = new swq_expr_node( SWQ_NOT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $2 );
        }

    | value_expr '=' value_expr
        {
            $$ = new swq_expr_node( SWQ_EQ );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '<' '>' value_expr
        {
            $$ = new swq_expr_node( SWQ_NE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '!' '=' value_expr
        {
            $$ = new swq_expr_node( SWQ_NE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '<' value_expr
        {
            $$ = new swq_expr_node( SWQ_LT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '>' value_expr
        {
            $$ = new swq_expr_node( SWQ_GT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr '<' '=' value_expr
        {
            $$ = new swq_expr_node( SWQ_LE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '=' '<' value_expr
        {
            $$ = new swq_expr_node( SWQ_LE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '=' '>' value_expr
        {
            $$ = new swq_expr_node( SWQ_LE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr '>' '=' value_expr
        {
            $$ = new swq_expr_node( SWQ_GE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $4 );
        }

    | value_expr SWQT_LIKE value_expr
        {
            $$ = new swq_expr_node( SWQ_LIKE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr SWQT_NOT SWQT_LIKE value_expr
        {
            swq_expr_node *like = new swq_expr_node( SWQ_LIKE );
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression( $1 );
            like->PushSubExpression( $4 );

            $$ = new swq_expr_node( SWQ_NOT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( like );
        }

    | value_expr SWQT_LIKE value_expr SWQT_ESCAPE value_expr
        {
            $$ = new swq_expr_node( SWQ_LIKE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
        }

    | value_expr SWQT_NOT SWQT_LIKE value_expr SWQT_ESCAPE value_expr
        {
            swq_expr_node *like = new swq_expr_node( SWQ_LIKE );
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression( $1 );
            like->PushSubExpression( $4 );
            like->PushSubExpression( $6 );

            $$ = new swq_expr_node( SWQ_NOT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( like );
        }

    | value_expr SWQT_ILIKE value_expr
        {
            $$ = new swq_expr_node( SWQ_ILIKE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr SWQT_NOT SWQT_ILIKE value_expr
        {
            swq_expr_node *like = new swq_expr_node( SWQ_ILIKE );
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression( $1 );
            like->PushSubExpression( $4 );

            $$ = new swq_expr_node( SWQ_NOT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( like );
        }

    | value_expr SWQT_ILIKE value_expr SWQT_ESCAPE value_expr
        {
            $$ = new swq_expr_node( SWQ_ILIKE );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
        }

    | value_expr SWQT_NOT SWQT_ILIKE value_expr SWQT_ESCAPE value_expr
        {
            swq_expr_node *like = new swq_expr_node( SWQ_ILIKE );
            like->field_type = SWQ_BOOLEAN;
            like->PushSubExpression( $1 );
            like->PushSubExpression( $4 );
            like->PushSubExpression( $6 );

            $$ = new swq_expr_node( SWQ_NOT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( like );
        }

    | value_expr SWQT_IN '(' value_expr_list ')'
        {
            $$ = $4;
            $$->field_type = SWQ_BOOLEAN;
            $$->nOperation = SWQ_IN;
            $$->PushSubExpression( $1 );
            $$->ReverseSubExpressions();
        }

    | value_expr SWQT_NOT SWQT_IN '(' value_expr_list ')'
        {
            swq_expr_node *in = $5;
            in->field_type = SWQ_BOOLEAN;
            in->nOperation = SWQ_IN;
            in->PushSubExpression( $1 );
            in->ReverseSubExpressions();

            $$ = new swq_expr_node( SWQ_NOT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( in );
        }

    | value_expr SWQT_BETWEEN value_expr_non_logical SWQT_AND value_expr_non_logical
        {
            $$ = new swq_expr_node( SWQ_BETWEEN );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
            $$->PushSubExpression( $5 );
        }

    | value_expr SWQT_NOT SWQT_BETWEEN value_expr_non_logical SWQT_AND value_expr_non_logical
        {
            swq_expr_node *between = new swq_expr_node( SWQ_BETWEEN );
            between->field_type = SWQ_BOOLEAN;
            between->PushSubExpression( $1 );
            between->PushSubExpression( $4 );
            between->PushSubExpression( $6 );

            $$ = new swq_expr_node( SWQ_NOT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( between );
        }

    | value_expr SWQT_IS SWQT_NULL
        {
            $$ = new swq_expr_node( SWQ_ISNULL );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( $1 );
        }

    | value_expr SWQT_IS SWQT_NOT SWQT_NULL
        {
            swq_expr_node *isnull = new swq_expr_node( SWQ_ISNULL );
            isnull->field_type = SWQ_BOOLEAN;
            isnull->PushSubExpression( $1 );

            $$ = new swq_expr_node( SWQ_NOT );
            $$->field_type = SWQ_BOOLEAN;
            $$->PushSubExpression( isnull );
        }

value_expr_list:
    value_expr ',' value_expr_list
        {
            $$ = $3;
            $3->PushSubExpression( $1 );
        }

    | value_expr
            {
            $$ = new swq_expr_node( SWQ_ARGUMENT_LIST ); /* temporary value */
            $$->PushSubExpression( $1 );
        }

field_value:
    SWQT_IDENTIFIER
        {
            $$ = $1;  // validation deferred.
            $$->eNodeType = SNT_COLUMN;
            $$->field_index = -1;
            $$->table_index = -1;
        }

    | SWQT_IDENTIFIER '.' SWQT_IDENTIFIER
        {
            $$ = $1;  // validation deferred.
            $$->eNodeType = SNT_COLUMN;
            $$->field_index = -1;
            $$->table_index = -1;
            $$->table_name = $$->string_value;
            $$->string_value = CPLStrdup($3->string_value);
            delete $3;
            $3 = nullptr;
        }

value_expr_non_logical:
    SWQT_INTEGER_NUMBER
        {
            $$ = $1;
        }

    | SWQT_FLOAT_NUMBER
        {
            $$ = $1;
        }

    | SWQT_STRING
        {
            $$ = $1;
        }
    | field_value
        {
            $$ = $1;
        }

    | '(' value_expr ')'
        {
            $$ = $2;
        }

    | SWQT_NULL
        {
            $$ = new swq_expr_node(static_cast<const char*>(nullptr));
        }

    | '-' value_expr_non_logical %prec SWQT_UMINUS
        {
            if ($2->eNodeType == SNT_CONSTANT)
            {
                if( $2->field_type == SWQ_FLOAT &&
                    $2->string_value &&
                    strcmp($2->string_value, "9223372036854775808") == 0 )
                {
                    $$ = $2;
                    $$->field_type = SWQ_INTEGER64;
                    $$->int_value = std::numeric_limits<GIntBig>::min();
                    $$->float_value = static_cast<double>(std::numeric_limits<GIntBig>::min());
                }
                // - (-9223372036854775808) cannot be represented on int64
                // the classic overflow is that its negation is itself.
                else if( $2->field_type == SWQ_INTEGER64 &&
                         $2->int_value == std::numeric_limits<GIntBig>::min() )
                {
                    $$ = $2;
                }
                else
                {
                    $$ = $2;
                    $$->int_value *= -1;
                    $$->float_value *= -1;
                }
            }
            else
            {
                $$ = new swq_expr_node( SWQ_MULTIPLY );
                $$->PushSubExpression( new swq_expr_node(-1) );
                $$->PushSubExpression( $2 );
            }
        }

    | value_expr_non_logical '+' value_expr_non_logical
        {
            $$ = new swq_expr_node( SWQ_ADD );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr_non_logical '-' value_expr_non_logical
        {
            $$ = new swq_expr_node( SWQ_SUBTRACT );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr_non_logical '*' value_expr_non_logical
        {
            $$ = new swq_expr_node( SWQ_MULTIPLY );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr_non_logical '/' value_expr_non_logical
        {
            $$ = new swq_expr_node( SWQ_DIVIDE );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | value_expr_non_logical '%' value_expr_non_logical
        {
            $$ = new swq_expr_node( SWQ_MODULUS );
            $$->PushSubExpression( $1 );
            $$->PushSubExpression( $3 );
        }

    | SWQT_IDENTIFIER '(' value_expr_list ')'
        {
            const swq_operation *poOp =
                    swq_op_registrar::GetOperator( $1->string_value );

            if( poOp == nullptr )
            {
                if( context->bAcceptCustomFuncs )
                {
                    $$ = $3;
                    $$->eNodeType = SNT_OPERATION;
                    $$->nOperation = SWQ_CUSTOM_FUNC;
                    $$->string_value = CPLStrdup($1->string_value);
                    $$->ReverseSubExpressions();
                    delete $1;
                }
                else
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                                    "Undefined function '%s' used.",
                                    $1->string_value );
                    delete $1;
                    delete $3;
                    YYERROR;
                }
            }
            else
            {
                $$ = $3;
                $$->eNodeType = SNT_OPERATION;
                $$->nOperation = poOp->eOperation;
                $$->ReverseSubExpressions();
                delete $1;
            }
        }

    | SWQT_CAST '(' value_expr SWQT_AS type_def ')'
        {
            $$ = $5;
            $$->PushSubExpression( $3 );
            $$->ReverseSubExpressions();
        }

type_def:
    SWQT_IDENTIFIER
    {
        $$ = new swq_expr_node( SWQ_CAST );
        $$->PushSubExpression( $1 );
    }

    | SWQT_IDENTIFIER '(' SWQT_INTEGER_NUMBER ')'
    {
        $$ = new swq_expr_node( SWQ_CAST );
        $$->PushSubExpression( $3 );
        $$->PushSubExpression( $1 );
    }

    | SWQT_IDENTIFIER '(' SWQT_INTEGER_NUMBER ',' SWQT_INTEGER_NUMBER ')'
    {
        $$ = new swq_expr_node( SWQ_CAST );
        $$->PushSubExpression( $5 );
        $$->PushSubExpression( $3 );
        $$->PushSubExpression( $1 );
    }

    /* e.g. GEOMETRY(POINT) */
    | SWQT_IDENTIFIER '(' SWQT_IDENTIFIER ')'
    {
        OGRwkbGeometryType eType = OGRFromOGCGeomType($3->string_value);
        if( !EQUAL($1->string_value, "GEOMETRY") ||
            (wkbFlatten(eType) == wkbUnknown &&
            !STARTS_WITH_CI($3->string_value, "GEOMETRY")) )
        {
            yyerror (context, "syntax error");
            delete $1;
            delete $3;
            YYERROR;
        }
        $$ = new swq_expr_node( SWQ_CAST );
        $$->PushSubExpression( $3 );
        $$->PushSubExpression( $1 );
    }

    /* e.g. GEOMETRY(POINT,4326) */
    | SWQT_IDENTIFIER '(' SWQT_IDENTIFIER ',' SWQT_INTEGER_NUMBER ')'
    {
        OGRwkbGeometryType eType = OGRFromOGCGeomType($3->string_value);
        if( !EQUAL($1->string_value, "GEOMETRY") ||
            (wkbFlatten(eType) == wkbUnknown &&
            !STARTS_WITH_CI($3->string_value, "GEOMETRY")) )
        {
            yyerror (context, "syntax error");
            delete $1;
            delete $3;
            delete $5;
            YYERROR;
        }
        $$ = new swq_expr_node( SWQ_CAST );
        $$->PushSubExpression( $5 );
        $$->PushSubExpression( $3 );
        $$->PushSubExpression( $1 );
    }

select_statement:
    select_core opt_union_all
    | '(' select_core ')' opt_union_all

select_core:
    SWQT_SELECT select_field_list SWQT_FROM table_def opt_joins opt_where opt_order_by opt_limit opt_offset
    {
        delete $4;
    }

    | SWQT_SELECT SWQT_DISTINCT select_field_list SWQT_FROM table_def opt_joins opt_where opt_order_by opt_limit opt_offset
    {
        context->poCurSelect->query_mode = SWQM_DISTINCT_LIST;
        delete $5;
    }

opt_union_all:
    | union_all select_statement

union_all: SWQT_UNION SWQT_ALL
    {
        swq_select* poNewSelect = new swq_select();
        context->poCurSelect->PushUnionAll(poNewSelect);
        context->poCurSelect = poNewSelect;
    }

select_field_list:
    column_spec
    | column_spec ',' select_field_list

column_spec:
    value_expr
        {
            if( !context->poCurSelect->PushField( $1 ) )
            {
                delete $1;
                YYERROR;
            }
        }

    | value_expr as_clause
        {
            if( !context->poCurSelect->PushField( $1, $2->string_value ) )
            {
                delete $1;
                delete $2;
                YYERROR;
            }
            delete $2;
        }

    | '*'
        {
            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup( "*" );
            poNode->table_index = -1;
            poNode->field_index = -1;

            if( !context->poCurSelect->PushField( poNode ) )
            {
                delete poNode;
                YYERROR;
            }
        }

    | SWQT_IDENTIFIER '.' '*'
        {
            CPLString osTableName = $1->string_value;

            delete $1;
            $1 = nullptr;

            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->table_name = CPLStrdup(osTableName );
            poNode->string_value = CPLStrdup( "*" );
            poNode->table_index = -1;
            poNode->field_index = -1;

            if( !context->poCurSelect->PushField( poNode ) )
            {
                delete poNode;
                YYERROR;
            }
        }

    | SWQT_IDENTIFIER '(' '*' ')'
        {
                // special case for COUNT(*), confirm it.
            if( !EQUAL($1->string_value, "COUNT") )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Syntax Error with %s(*).",
                        $1->string_value );
                delete $1;
                YYERROR;
            }

            delete $1;
            $1 = nullptr;

            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup( "*" );
            poNode->table_index = -1;
            poNode->field_index = -1;

            swq_expr_node *count = new swq_expr_node( SWQ_COUNT );
            count->PushSubExpression( poNode );

            if( !context->poCurSelect->PushField( count ) )
            {
                delete count;
                YYERROR;
            }
        }

    | SWQT_IDENTIFIER '(' '*' ')' as_clause
        {
                // special case for COUNT(*), confirm it.
            if( !EQUAL($1->string_value, "COUNT") )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Syntax Error with %s(*).",
                        $1->string_value );
                delete $1;
                delete $5;
                YYERROR;
            }

            delete $1;
            $1 = nullptr;

            swq_expr_node *poNode = new swq_expr_node();
            poNode->eNodeType = SNT_COLUMN;
            poNode->string_value = CPLStrdup( "*" );
            poNode->table_index = -1;
            poNode->field_index = -1;

            swq_expr_node *count = new swq_expr_node( SWQ_COUNT );
            count->PushSubExpression( poNode );

            if( !context->poCurSelect->PushField( count, $5->string_value ) )
            {
                delete count;
                delete $5;
                YYERROR;
            }

            delete $5;
        }

    | SWQT_IDENTIFIER '(' SWQT_DISTINCT field_value ')'
        {
                // special case for COUNT(DISTINCT x), confirm it.
            if( !EQUAL($1->string_value, "COUNT") )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "DISTINCT keyword can only be used in COUNT() operator." );
                delete $1;
                delete $4;
                    YYERROR;
            }

            delete $1;

            swq_expr_node *count = new swq_expr_node( SWQ_COUNT );
            count->PushSubExpression( $4 );

            if( !context->poCurSelect->PushField( count, nullptr, TRUE ) )
            {
                delete count;
                YYERROR;
            }
        }

    | SWQT_IDENTIFIER '(' SWQT_DISTINCT field_value ')' as_clause
        {
            // special case for COUNT(DISTINCT x), confirm it.
            if( !EQUAL($1->string_value, "COUNT") )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                        "DISTINCT keyword can only be used in COUNT() operator." );
                delete $1;
                delete $4;
                delete $6;
                YYERROR;
            }

            swq_expr_node *count = new swq_expr_node( SWQ_COUNT );
            count->PushSubExpression( $4 );

            if( !context->poCurSelect->PushField( count, $6->string_value, TRUE ) )
            {
                delete $1;
                delete count;
                delete $6;
                YYERROR;
            }

            delete $1;
            delete $6;
        }

as_clause:
    SWQT_AS SWQT_IDENTIFIER
        {
            delete $1;
            $$ = $2;
        }

    | SWQT_IDENTIFIER

opt_where:
    | SWQT_WHERE value_expr
        {
            context->poCurSelect->where_expr = $2;
        }

opt_joins:
    | SWQT_JOIN table_def SWQT_ON value_expr opt_joins
        {
            context->poCurSelect->PushJoin( static_cast<int>($2->int_value),
                                            $4 );
            delete $2;
        }
    | SWQT_LEFT SWQT_JOIN table_def SWQT_ON value_expr opt_joins
        {
            context->poCurSelect->PushJoin( static_cast<int>($3->int_value),
                                            $5 );
            delete $3;
        }

opt_order_by:
    | SWQT_ORDER SWQT_BY sort_spec_list

sort_spec_list:
    sort_spec ',' sort_spec_list
    | sort_spec

sort_spec:
    field_value
        {
            context->poCurSelect->PushOrderBy( $1->table_name, $1->string_value, TRUE );
            delete $1;
            $1 = nullptr;
        }
    | field_value SWQT_ASC
        {
            context->poCurSelect->PushOrderBy( $1->table_name, $1->string_value, TRUE );
            delete $1;
            $1 = nullptr;
        }
    | field_value SWQT_DESC
        {
            context->poCurSelect->PushOrderBy( $1->table_name, $1->string_value, FALSE );
            delete $1;
            $1 = nullptr;
        }

opt_limit:
    | SWQT_LIMIT SWQT_INTEGER_NUMBER
    {
        context->poCurSelect->SetLimit( $2->int_value );
        delete $2;
        $2 = nullptr;
    }

opt_offset:
    | SWQT_OFFSET SWQT_INTEGER_NUMBER
    {
        context->poCurSelect->SetOffset( $2->int_value );
        delete $2;
        $2 = nullptr;
    }

table_def:
    SWQT_IDENTIFIER
    {
        const int iTable =
            context->poCurSelect->PushTableDef( nullptr, $1->string_value,
                                                nullptr );
        delete $1;

        $$ = new swq_expr_node( iTable );
    }

    | SWQT_IDENTIFIER as_clause
    {
        const int iTable =
            context->poCurSelect->PushTableDef( nullptr, $1->string_value,
                                                $2->string_value );
        delete $1;
        delete $2;

        $$ = new swq_expr_node( iTable );
    }

    | SWQT_STRING '.' SWQT_IDENTIFIER
    {
        const int iTable =
            context->poCurSelect->PushTableDef( $1->string_value,
                                                $3->string_value, nullptr );
        delete $1;
        delete $3;

        $$ = new swq_expr_node( iTable );
    }

    | SWQT_STRING '.' SWQT_IDENTIFIER as_clause
    {
        const int iTable =
            context->poCurSelect->PushTableDef( $1->string_value,
                                                $3->string_value,
                                                $4->string_value );
        delete $1;
        delete $3;
        delete $4;

        $$ = new swq_expr_node( iTable );
    }

    | SWQT_IDENTIFIER '.' SWQT_IDENTIFIER
    {
        const int iTable =
            context->poCurSelect->PushTableDef( $1->string_value,
                                                $3->string_value, nullptr );
        delete $1;
        delete $3;

        $$ = new swq_expr_node( iTable );
    }

    | SWQT_IDENTIFIER '.' SWQT_IDENTIFIER as_clause
    {
        const int iTable =
            context->poCurSelect->PushTableDef( $1->string_value,
                                                $3->string_value,
                                                $4->string_value );
        delete $1;
        delete $3;
        delete $4;

        $$ = new swq_expr_node( iTable );
    }
