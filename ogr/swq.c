/******************************************************************************
 *
 * Component: OGDI Driver Support Library
 * Purpose: Generic SQL WHERE Expression Implementation.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 * 
 ******************************************************************************
 * Copyright (C) 2001 Information Interoperability Institute (3i)
 * Permission to use, copy, modify and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies, that
 * both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of 3i not be used 
 * in advertising or publicity pertaining to distribution of the software 
 * without specific, written prior permission.  3i makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.5  2002/04/19 20:46:06  warmerda
 * added [NOT] IN, [NOT] LIKE and IS [NOT] NULL support
 *
 * Revision 1.4  2002/03/01 04:13:40  warmerda
 * Made swq_error static.
 *
 * Revision 1.3  2001/11/07 12:45:42  danmo
 * Use #ifdef _WIN32 instead of WIN32 for strcasecmp check
 *
 * Revision 1.2  2001/06/26 00:59:39  warmerda
 * fixed strcasecmp on WIN32
 *
 * Revision 1.1  2001/06/19 15:46:30  warmerda
 * New
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "swq.h"

#ifndef SWQ_MALLOC
#define SWQ_MALLOC(x) malloc(x)
#define SWQ_FREE(x) free(x)
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#ifndef FALSE
#  define FALSE 0
#endif

#ifdef _WIN32
#  define strcasecmp stricmp
#endif

static char	swq_error[1024];

#define SWQ_OP_IS_LOGICAL(op) ((op) == SWQ_OR || (op) == SWQ_AND || (op) == SWQ_NOT)
#define SWQ_OP_IS_POSTUNARY(op) ((op) == SWQ_ISNULL || (op) == SWQ_ISNOTNULL)

/************************************************************************/
/*                           swq_isalphanum()                           */
/*                                                                      */
/*      Is the passed character in the set of things that could         */
/*      occur in an alphanumeric token, or a number?                    */
/************************************************************************/

static int swq_isalphanum( char c )

{

    if( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '.' || c == '+' || c == '-'
        || c == '_' )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           swq_test_like()                            */
/*                                                                      */
/*      Does input match pattern?                                       */
/************************************************************************/

int swq_test_like( const char *input, const char *pattern )

{
    if( input == NULL || pattern == NULL )
        return 0;

    while( *input != '\0' )
    {
        if( *pattern == '\0' )
            return 0;

        else if( *pattern == '_' )
        {
            input++;
            pattern++;
        }
        else if( *pattern == '%' )
        {
            int   eat;

            if( pattern[1] == '\0' )
                return 1;

            /* try eating varying amounts of the input till we get a positive*/
            for( eat = 0; input[eat] != '\0'; eat++ )
            {
                if( swq_test_like(input+eat,pattern+1) )
                    return 1;
            }

            return 0;
        }
        else
        {
            if( tolower(*pattern) != tolower(*input) )
                return 0;
            else
            {
                input++;
                pattern++;
            }
        }
    }

    if( *pattern != '\0' && strcmp(pattern,"%") != 0 )
        return 0;
    else
        return 1;
}

/************************************************************************/
/*                             swq_token()                              */
/************************************************************************/

static char *swq_token( const char *expression, char **next )

{
    char	*token;
    int		i_token;

    while( *expression == ' ' || *expression == '\t' )
        expression++;

    if( *expression == '\0' )
    {
        *next = (char *) expression;
        return NULL; 
    }

/* -------------------------------------------------------------------- */
/*      Handle string constants.                                        */
/* -------------------------------------------------------------------- */
    if( *expression == '"' )
    {
        expression++;

        token = (char *) SWQ_MALLOC(strlen(expression)+1);
        i_token = 0;

        while( *expression != '\0' )
        {
            if( *expression == '\\' && expression[1] == '"' )
                expression++;
            else if( *expression == '"' )
            {
                expression++;
                break;
            }
            
            token[i_token++] = *(expression++);
        }
        token[i_token] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Handle alpha-numerics.                                          */
/* -------------------------------------------------------------------- */
    else if( swq_isalphanum( *expression ) )
    {
        token = (char *) SWQ_MALLOC(strlen(expression)+1);
        i_token = 0;

        while( swq_isalphanum( *expression ) )
        {
            token[i_token++] = *(expression++);
        }

        token[i_token] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Handle special tokens.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        token = (char *) SWQ_MALLOC(3);
        token[0] = *expression;
        token[1] = '\0';
        expression++;

        /* special logic to group stuff like '>=' into one token. */

        if( (*token == '<' || *token == '>' || *token == '=' || *token == '!')
           && (*expression == '<' || *expression == '>' || *expression == '='))
        {
            token[1] = *expression;
            token[2] = '\0';
            expression++;
        }
    }

    *next = (char *) expression;

    return token;
}

/************************************************************************/
/*                          swq_identify_op()                           */
/************************************************************************/

static swq_op swq_identify_op( char **tokens, int *tokens_consumed )

{
    const char *token = tokens[*tokens_consumed];

    if( strcasecmp(token,"OR") == 0 )
        return SWQ_OR;
    
    if( strcasecmp(token,"AND") == 0 )
        return SWQ_AND;
    
    if( strcasecmp(token,"NOT") == 0 )
    {
        if( tokens[*tokens_consumed+1] != NULL
            && strcasecmp(tokens[*tokens_consumed+1],"LIKE") == 0 )
        {
            *tokens_consumed += 1;
            return SWQ_NOTLIKE;
        }
        else if( tokens[*tokens_consumed+1] != NULL
            && strcasecmp(tokens[*tokens_consumed+1],"IN") == 0 )
        {
            *tokens_consumed += 1;
            return SWQ_NOTIN;
        }
        else
            return SWQ_NOT;
    }
    
    if( strcasecmp(token,"<=") == 0 )
        return SWQ_LE;

    if( strcasecmp(token,">=") == 0 )
        return SWQ_GE;
    
    if( strcasecmp(token,"=") == 0 )
        return SWQ_EQ;
    
    if( strcasecmp(token,"!=") == 0 )
        return SWQ_NE;
    
    if( strcasecmp(token,"<>") == 0 )
        return SWQ_NE;
    
    if( strcasecmp(token,"<") == 0 )
        return SWQ_LT;
    
    if( strcasecmp(token,">") == 0 )
        return SWQ_GT;

    if( strcasecmp(token,"LIKE") == 0 )
        return SWQ_LIKE;

    if( strcasecmp(token,"IN") == 0 )
        return SWQ_IN;

    if( strcasecmp(token,"IS") == 0 )
    {
        if( tokens[*tokens_consumed+1] == NULL )
            return SWQ_UNKNOWN;
        else if( strcasecmp(tokens[*tokens_consumed+1],"NULL") == 0 )
        {
            *tokens_consumed += 1;
            return SWQ_ISNULL;
        }
        else if( strcasecmp(tokens[*tokens_consumed+1],"NOT") == 0
                 && tokens[*tokens_consumed+2] != NULL
                 && strcasecmp(tokens[*tokens_consumed+2],"NULL") == 0 )
        {
            *tokens_consumed += 2;
            return SWQ_ISNOTNULL;
        }
        else 
            return SWQ_UNKNOWN;
    }

    return SWQ_UNKNOWN;
}

/************************************************************************/
/*                         swq_identify_field()                         */
/************************************************************************/

static int swq_identify_field( const char *token,
                               int field_count,
                               char **field_list, 
                               swq_field_type *field_types, 
                               swq_field_type *this_type )

{
    int	i;

    for( i = 0; i < field_count; i++ )
    {
        if( strcasecmp( field_list[i], token ) == 0 )
        {
            if( field_types != NULL )
                *this_type = field_types[i];
            else
                *this_type = SWQ_OTHER;
            
            return i;
        }
    }

    *this_type = SWQ_OTHER;
    return -1;
}

/************************************************************************/
/*                         swq_parse_in_list()                          */
/*                                                                      */
/*      Parse the argument list to the IN predicate. Might be used      */
/*      something like:                                                 */
/*                                                                      */
/*        WHERE color IN ('Red', 'Green', 'Blue')                       */
/************************************************************************/

static char *swq_parse_in_list( char **tokens, int *tokens_consumed )

{
    int   i, text_off = 2;
    char *result;
    
    if( tokens[*tokens_consumed] == NULL
        || strcasecmp(tokens[*tokens_consumed],"(") != 0 )
    {
        sprintf( swq_error, "IN argument doesn't start with '('." );
        return NULL;
    }

    *tokens_consumed += 1;

    /* Establish length of all tokens plus separators. */

    for( i = *tokens_consumed; 
         tokens[i] != NULL && strcasecmp(tokens[i],")") != 0; 
         i++ )
    {
        text_off += strlen(tokens[i]) + 1;
    }
    
    result = (char *) SWQ_MALLOC(text_off);

    /* Actually capture all the arguments. */

    text_off = 0;
    while( tokens[*tokens_consumed] != NULL 
           && strcasecmp(tokens[*tokens_consumed],")") != 0 )
    {
        strcpy( result + text_off, tokens[*tokens_consumed] );
        text_off += strlen(tokens[*tokens_consumed]) + 1;

        *tokens_consumed += 1;

        if( strcasecmp(tokens[*tokens_consumed],",") != 0
            && strcasecmp(tokens[*tokens_consumed],")") != 0 )
        {
            sprintf( swq_error, 
               "Contents of IN predicate missing comma or closing bracket." );
            SWQ_FREE( result );
            return NULL;
        }
        else if( strcasecmp(tokens[*tokens_consumed],",") == 0 )
            *tokens_consumed += 1;
    }

    /* add final extra terminating zero char */
    result[text_off] = '\0';

    if( tokens[*tokens_consumed] == NULL )
    {
        sprintf( swq_error, 
                 "Contents of IN predicate missing closing bracket." );
        SWQ_FREE( result );
        return NULL;
    }

    *tokens_consumed += 1;

    return result;
}

/************************************************************************/
/*                        swq_subexpr_compile()                         */
/************************************************************************/

static const char *
swq_subexpr_compile( char **tokens,
                     int field_count,
                     char **field_list, 
                     swq_field_type *field_types, 
                     swq_expr **expr_out,
                     int *tokens_consumed )

{
    swq_expr	*op;
    const char  *error;
    int         op_code;

    *tokens_consumed = 0;
    *expr_out = NULL;

    if( tokens[0] == NULL || tokens[1] == NULL )
    {
        sprintf( swq_error, "Not enough tokens to complete expression." );
        return swq_error;
    }
    
    op = (swq_field_op *) SWQ_MALLOC(sizeof(swq_field_op));
    memset( op, 0, sizeof(swq_field_op) );
    op->field_index = -1;

    if( strcmp(tokens[0],"(") == 0 )
    {
        int	sub_consumed = 0;

        error = swq_subexpr_compile( tokens + 1, field_count, field_list, 
                                     field_types, 
                                     (swq_expr **) &(op->first_sub_expr), 
                                     &sub_consumed );
        if( error != NULL )
        {
            swq_expr_free( op );
            return error;
        }

        if( strcmp(tokens[sub_consumed+1],")") != 0 )
        {
            swq_expr_free( op );
            sprintf(swq_error,"Unclosed brackets, or incomplete expression.");
            return swq_error;
        }

        *tokens_consumed += sub_consumed + 2;

        /* If we are at the end of the tokens, we should return our subnode */
        if( tokens[*tokens_consumed] == NULL
            || strcmp(tokens[*tokens_consumed],")") == 0 )
        {
            *expr_out = (swq_expr *) op->first_sub_expr;
            op->first_sub_expr = NULL;
            swq_expr_free( op );
            return NULL;
        }
    }
    else if( strcasecmp(tokens[0],"NOT") == 0 )
    {
        /* do nothing, the NOT will be collected as the operation */
    }
    else
    {
        op->field_index = 
            swq_identify_field( tokens[*tokens_consumed], 
                                field_count, field_list, field_types, 
                                &(op->field_type) );

        if( op->field_index < 0 )
        {
            swq_expr_free( op );
            sprintf( swq_error, "Failed to identify field:" );
            strncat( swq_error, tokens[*tokens_consumed], 
                     sizeof(swq_error) - strlen(swq_error) - 1 );
            return swq_error;
        }

        (*tokens_consumed)++;
    }

    /*
    ** Identify the operation.
    */
    if( tokens[*tokens_consumed] == NULL || tokens[*tokens_consumed+1] == NULL)
    {
        sprintf( swq_error, "Not enough tokens to complete expression." );
        return swq_error;
    }
    
    op->operation = swq_identify_op( tokens, tokens_consumed );
    if( op->operation == SWQ_UNKNOWN )
    {
        swq_expr_free( op );
        sprintf( swq_error, "Failed to identify operation:" );
        strncat( swq_error, tokens[*tokens_consumed], 
                 sizeof(swq_error) - strlen(swq_error) - 1 );
        return swq_error;
    }

    if( SWQ_OP_IS_LOGICAL( op->operation ) 
        && op->first_sub_expr == NULL 
        && op->operation != SWQ_NOT )
    {
        swq_expr_free( op );
        strcpy( swq_error, "Used logical operation with non-logical operand.");
        return swq_error;
    }

    if( op->field_index != -1 && op->field_type == SWQ_STRING
        && (op->operation != SWQ_EQ && op->operation != SWQ_NE
            && op->operation != SWQ_LIKE && op->operation != SWQ_NOTLIKE
            && op->operation != SWQ_IN && op->operation != SWQ_NOTIN
            && op->operation != SWQ_ISNULL && op->operation != SWQ_ISNOTNULL ))
    {
        sprintf( swq_error, 
            "Attempt to use STRING field `%s' with numeric comparison `%s'.",
            field_list[op->field_index], tokens[*tokens_consumed] );
        swq_expr_free( op );
        return swq_error;
    }

    (*tokens_consumed)++;

    /*
    ** Collect the second operand as a subexpression.
    */
    
    if( SWQ_OP_IS_POSTUNARY(op->operation) )
    {
        /* we don't need another argument. */
    }

    else if( tokens[*tokens_consumed] == NULL )
    {
        sprintf( swq_error, "Not enough tokens to complete expression." );
        return swq_error;
    }
    
    else if( SWQ_OP_IS_LOGICAL( op->operation ) )
    {
        int	sub_consumed = 0;

        error = swq_subexpr_compile( tokens + *tokens_consumed, 
                                     field_count, field_list, field_types, 
                                     (swq_expr **) &(op->second_sub_expr), 
                                     &sub_consumed );
        if( error != NULL )
        {
            swq_expr_free( op );
            return error;
        }

        *tokens_consumed += sub_consumed;
    }

    /* The IN predicate has a complex argument syntax. */
    else if( op->operation == SWQ_IN || op->operation == SWQ_NOTIN )
    {
        op->string_value = swq_parse_in_list( tokens, tokens_consumed );
        if( op->string_value == NULL )
        {
            swq_expr_free( op );
            return swq_error;
        }
    }

    /*
    ** Otherwise collect it as a literal value.
    */
    else
    {
        op->string_value = (char *) 
            SWQ_MALLOC(strlen(tokens[*tokens_consumed])+1);
        strcpy( op->string_value, tokens[*tokens_consumed] );
        op->int_value = atoi(op->string_value);
        op->float_value = atof(op->string_value);
        
        if( op->field_index != -1 
            && (op->field_type == SWQ_INTEGER || op->field_type == SWQ_FLOAT) 
            && op->string_value[0] != '-'
            && op->string_value[0] != '+'
            && op->string_value[0] != '.'
            && (op->string_value[0] < '0' || op->string_value[0] > '9') )
        {
            sprintf( swq_error, 
                     "Attempt to compare numeric field `%s' to non-numeric"
                     " value `%s' is illegal.", 
                     field_list[op->field_index], op->string_value );
            swq_expr_free( op );
            return swq_error;
        }

        (*tokens_consumed)++;
    }

    *expr_out = op;

    /* Transform stuff like A NOT LIKE X into NOT (A LIKE X) */
    if( op->operation == SWQ_NOTLIKE
        || op->operation == SWQ_ISNOTNULL 
        || op->operation == SWQ_NOTIN )
    {
        if( op->operation == SWQ_NOTLIKE )
            op->operation = SWQ_LIKE;
        else if( op->operation == SWQ_NOTIN )
            op->operation = SWQ_IN;
        else if( op->operation == SWQ_ISNOTNULL )
            op->operation = SWQ_ISNULL;

        op = (swq_field_op *) SWQ_MALLOC(sizeof(swq_field_op));
        memset( op, 0, sizeof(swq_field_op) );
        op->field_index = -1;
        op->second_sub_expr = (struct swq_node_s *) *expr_out;
        op->operation = SWQ_NOT;

        *expr_out = op;
    }

    op = NULL;
    
    /*
    ** Are we part of an unparantized logical expression chain?  If so, 
    ** grab the remainder of the expression at "this level" and add to the
    ** local tree. 
    */
    if( tokens[*tokens_consumed] != NULL
        && (op_code == swq_identify_op( tokens, tokens_consumed ))
        && SWQ_OP_IS_LOGICAL(op_code) )
    {
        swq_expr *remainder = NULL;
        swq_expr *parent;
        int	 sub_consumed;

        error = swq_subexpr_compile( tokens + *tokens_consumed + 1, 
                                     field_count, field_list, field_types, 
                                     &remainder, &sub_consumed );
        if( error != NULL )
        {
            swq_expr_free( *expr_out );
            *expr_out = NULL;
            return error;
        }

        parent = (swq_field_op *) SWQ_MALLOC(sizeof(swq_field_op));
        memset( parent, 0, sizeof(swq_field_op) );
        parent->field_index = -1;

        parent->first_sub_expr = (struct swq_node_s *) *expr_out;
        parent->second_sub_expr = (struct swq_node_s *) remainder;
        parent->operation = op_code;

        *expr_out = parent;

        *tokens_consumed += sub_consumed + 1;
    }

    return NULL;
}

/************************************************************************/
/*                          swq_expr_compile()                          */
/************************************************************************/

const char *swq_expr_compile( const char *where_clause, 
                              int field_count,
                              char **field_list, 
                              swq_field_type *field_types, 
                              swq_expr **expr_out )

{
#define MAX_TOKEN 1024
    char	*token_list[MAX_TOKEN], *rest_of_expr;
    int		token_count = 0;
    int		tokens_consumed, i;
    const char *error;
    
    /*
    ** Collect token array.
    */
    rest_of_expr = (char *) where_clause;
    while( token_count < MAX_TOKEN )
    {
        token_list[token_count] = swq_token( rest_of_expr, &rest_of_expr );
        if( token_list[token_count] == NULL )
            break;

        token_count++;
    }
    token_list[token_count] = NULL;
    
    /*
    ** Parse the expression.
    */
    *expr_out = NULL;
    error = 
        swq_subexpr_compile( token_list, field_count, field_list, field_types, 
                             expr_out, &tokens_consumed );

    for( i = 0; i < token_count; i++ )
        SWQ_FREE( token_list[i] );

    if( error != NULL )
        return error;

    if( tokens_consumed < token_count )
    {
        swq_expr_free( *expr_out );
        *expr_out = NULL;
        sprintf( swq_error, "Syntax error, %d extra tokens", 
                 token_count - tokens_consumed );
        return swq_error;
    }

    return NULL;
}

/************************************************************************/
/*                           swq_expr_free()                            */
/************************************************************************/

void swq_expr_free( swq_expr *expr )

{
    if( expr == NULL )
        return;

    if( expr->first_sub_expr != NULL )
        swq_expr_free( (swq_expr *) expr->first_sub_expr );
    if( expr->second_sub_expr != NULL )
        swq_expr_free( (swq_expr *) expr->second_sub_expr );

    if( expr->string_value != NULL )
        SWQ_FREE( expr->string_value );

    SWQ_FREE( expr );
}

/************************************************************************/
/*                         swq_expr_evaluate()                          */
/************************************************************************/

int swq_expr_evaluate( swq_expr *expr, swq_op_evaluator fn_evaluator, 
                       void *record_handle )

{
    if( expr->operation == SWQ_OR )
    {
        return swq_expr_evaluate( (swq_expr *) expr->first_sub_expr, 
                                  fn_evaluator, 
                                  record_handle) 
            || swq_expr_evaluate( (swq_expr *) expr->second_sub_expr, 
                                  fn_evaluator, 
                                  record_handle);
    }
    else if( expr->operation == SWQ_AND )
    {
        return swq_expr_evaluate( (swq_expr *) expr->first_sub_expr, 
                                  fn_evaluator, 
                                  record_handle) 
            && swq_expr_evaluate( (swq_expr *) expr->second_sub_expr, 
                                  fn_evaluator, 
                                  record_handle);
    }
    else if( expr->operation == SWQ_NOT )
    {
        return !swq_expr_evaluate( (swq_expr *) expr->second_sub_expr, 
                                  fn_evaluator, 
                                   record_handle);
    }
    else
    {
        return fn_evaluator( expr, record_handle );
    }

    return FALSE;
}

/************************************************************************/
/*                           swq_expr_dump()                            */
/************************************************************************/

void swq_expr_dump( swq_expr *expr, FILE * fp, int depth )

{
    char	spaces[60];
    int		i;
    const char  *op_name = "unknown";

    for( i = 0; i < depth*2 && i < sizeof(spaces); i++ )
        spaces[i] = ' ';
    spaces[i] = '\0';

    /*
    ** first term.
    */
    if( expr->first_sub_expr != NULL )
        swq_expr_dump( (swq_expr *) expr->first_sub_expr, fp, depth + 1 );
    else
        fprintf( fp, "%s  Field %d\n", spaces, expr->field_index );

    /*
    ** Operation.
    */
    if( expr->operation == SWQ_OR )
        op_name = "OR";
    if( expr->operation == SWQ_AND )
        op_name = "AND";
    if( expr->operation == SWQ_NOT)
        op_name = "NOT";
    if( expr->operation == SWQ_GT )
        op_name = ">";
    if( expr->operation == SWQ_LT )
        op_name = "<";
    if( expr->operation == SWQ_EQ )
        op_name = "=";
    if( expr->operation == SWQ_NE )
        op_name = "!=";
    if( expr->operation == SWQ_GE )
        op_name = ">=";
    if( expr->operation == SWQ_LE )
        op_name = "<=";
    if( expr->operation == SWQ_LIKE )
        op_name = "LIKE";
    if( expr->operation == SWQ_ISNULL )
        op_name = "IS NULL";
    if( expr->operation == SWQ_IN )
        op_name = "IN";

    fprintf( fp, "%s%s\n", spaces, op_name );

    /*
    ** Second term.
    */
    if( expr->second_sub_expr != NULL )
        swq_expr_dump( (swq_expr *) expr->second_sub_expr, fp, depth + 1 );
    else if( expr->operation == SWQ_IN || expr->operation == SWQ_NOTIN )
    {
        const char *src;

        fprintf( fp, "%s  (\"%s\"", spaces, expr->string_value );
        src = expr->string_value + strlen(expr->string_value) + 1;
        while( *src != '\0' )
        {
            fprintf( fp, ",\"%s\"", src );
            src += strlen(src) + 1;
        }

        fprintf( fp, ")\n" );
    }
    else if( expr->string_value != NULL )
        fprintf( fp, "%s  %s\n", spaces, expr->string_value );
}


