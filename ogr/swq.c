/******************************************************************************
 *
 * Component: OGDI Driver Support Library
 * Purpose: Generic SQL WHERE Expression Implementation.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 * 
 ******************************************************************************
 * Copyright (C) 2001 Information Interoperability Institute (3i)
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies, that
 * both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of 3i not be used 
 * in advertising or publicity pertaining to distribution of the software 
 * without specific, written prior permission.  3i makes no
 * representations about the suitability of this software for any purpose.
 * It is provided "as is" without express or implied warranty.
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "swq.h"

#ifndef SWQ_MALLOC
#define SWQ_MALLOC(x) malloc(x)
#define SWQ_FREE(x) free(x)
#endif

#ifdef _MSC_VER
#  define FORCE_CDECL  __cdecl
#else
#  define FORCE_CDECL 
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#ifndef FALSE
#  define FALSE 0
#endif

#if defined(_WIN32) && !defined(_WIN32_WCE)
#  define strcasecmp stricmp
#elif defined(_WIN32_WCE)
#  define strcasecmp _stricmp
#endif


#define SWQ_OP_IS_LOGICAL(op) ((op) == SWQ_OR || (op) == SWQ_AND || (op) == SWQ_NOT)
#define SWQ_OP_IS_POSTUNARY(op) ((op) == SWQ_ISNULL || (op) == SWQ_ISNOTNULL)

/************************************************************************/
/*                              swq_free()                              */
/************************************************************************/

void swq_free( void *pMemory )

{
    SWQ_FREE( pMemory );
}

/************************************************************************/
/*                             swq_malloc()                             */
/************************************************************************/

void *swq_malloc( int nSize )

{
    return SWQ_MALLOC(nSize);
}

/************************************************************************/
/*                            swq_realloc()                             */
/************************************************************************/

void *swq_realloc( void *old_mem, int old_size, int new_size )

{
    void *new_mem;

    new_mem = swq_malloc( new_size );

    if( old_mem != NULL )
    {
        memcpy( new_mem, old_mem, old_size < new_size ? old_size : new_size);
        SWQ_FREE( old_mem );
    }
    if (old_size <= new_size )
        memset( ((char *) new_mem) + old_size, 0, new_size - old_size );

    return new_mem;
}

/************************************************************************/
/*                           swq_get_errbuf()                           */
/************************************************************************/

#define SWQ_SIZEOF_ERRBUF   1024

#define SNPRINTF_ERR1(x)      do { char* pszErrBuf = swq_get_errbuf(); snprintf( pszErrBuf, SWQ_SIZEOF_ERRBUF, x); pszErrBuf[SWQ_SIZEOF_ERRBUF-1] = '\0'; } while(0)
#define SNPRINTF_ERR2(x,y)    do { char* pszErrBuf = swq_get_errbuf(); snprintf( pszErrBuf, SWQ_SIZEOF_ERRBUF, x, y); pszErrBuf[SWQ_SIZEOF_ERRBUF-1] = '\0'; } while(0)
#define SNPRINTF_ERR3(x,y,z)  do { char* pszErrBuf = swq_get_errbuf(); snprintf( pszErrBuf, SWQ_SIZEOF_ERRBUF, x, y, z); pszErrBuf[SWQ_SIZEOF_ERRBUF-1] = '\0'; } while(0)

char *swq_get_errbuf()

{
    char *pszStaticResult = (char *) CPLGetTLS( CTLS_SWQ_ERRBUF );
    if( pszStaticResult == NULL )
    {
        pszStaticResult = (char *) CPLMalloc(SWQ_SIZEOF_ERRBUF);
        CPLSetTLS( CTLS_SWQ_ERRBUF, pszStaticResult, TRUE );
    }

    return pszStaticResult;
}

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
        || c == '_' || c == '*' || ((unsigned char) c) > 127 )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                             swq_token()                              */
/************************************************************************/

static char *swq_token( const char *expression, char **next, int *is_literal )

{
    char        *token;
    int         i_token;

    if( is_literal != NULL )
        *is_literal = 0;

    while( *expression == ' ' || *expression == '\t'
           || *expression == 10 || *expression == 13 )
        expression++;

    if( *expression == '\0' )
    {
        *next = (char *) expression;
        return NULL; 
    }

/* -------------------------------------------------------------------- */
/*      Handle string constants.                                        */
/* -------------------------------------------------------------------- */
    if( *expression == '"' || *expression == '\'' )
    {
        expression++;

        token = (char *) SWQ_MALLOC(strlen(expression)+1);
        i_token = 0;

        while( *expression != '\0' )
        {
            if( *expression == '\\' && expression[1] == '"' )
                expression++;
            else if( *expression == '\\' && expression[1] == '\'' )
                expression++;
            else if( *expression == '\'' && expression[1] == '\'' )
                expression++;
            else if( *expression == '"' )
            {
                expression++;
                break;
            }
            else if( *expression == '\'' )
            {
                expression++;
                break;
            }
            
            token[i_token++] = *(expression++);
        }
        token[i_token] = '\0';

        if( is_literal != NULL )
            *is_literal = 1;
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
/*                             swq_strdup()                             */
/************************************************************************/

static char *swq_strdup( const char *input )

{
    char *result;

    result = (char *) SWQ_MALLOC(strlen(input)+1);
    strcpy( result, input );

    return result;
}

/************************************************************************/
/* ==================================================================== */
/*              WHERE clause parsing                                    */
/* ==================================================================== */
/************************************************************************/

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
            && (strcasecmp(tokens[*tokens_consumed+1],"LIKE") == 0 
                || strcasecmp(tokens[*tokens_consumed+1],"ILIKE") == 0) )
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

    if( strcasecmp(token,"ILIKE") == 0 )
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

static int swq_identify_field( const char *token, swq_field_list *field_list,
                               swq_field_type *this_type, int *table_id )

{
    int i;
    char table_name[128];
    const char *field_token = token;
    int   tables_enabled;

    if( field_list->table_count > 0 && field_list->table_ids != NULL )
        tables_enabled = TRUE;
    else
        tables_enabled = FALSE;

/* -------------------------------------------------------------------- */
/*      Parse out table name if present, and table support enabled.     */
/* -------------------------------------------------------------------- */
    table_name[0] = '\0';
    if( tables_enabled && strchr(token, '.') != NULL )
    {
        int dot_offset = (int)(strchr(token,'.') - token);

        if( dot_offset < sizeof(table_name) )
        {
            strncpy( table_name, token, dot_offset );
            table_name[dot_offset] = '\0';
            field_token = token + dot_offset + 1;
        }
    }

/* -------------------------------------------------------------------- */
/*      Search for matching field.                                      */
/* -------------------------------------------------------------------- */
    for( i = 0; i < field_list->count; i++ )
    {
        int  t_id = 0;

        if( strcasecmp( field_list->names[i], field_token ) != 0 )
            continue;

        /* Do the table specifications match? */
        if( tables_enabled )
        {
            t_id = field_list->table_ids[i];
            if( table_name[0] != '\0' 
                && strcasecmp(table_name,
                              field_list->table_defs[t_id].table_alias) != 0 )
                continue;

#ifdef notdef 
            if( t_id != 0 && table_name[0] == '\0' )
                continue;
#endif
        }

        /* We have a match, return various information */
        if( this_type != NULL )
        {
            if( field_list->types != NULL )
                *this_type = field_list->types[i];
            else
                *this_type = SWQ_OTHER;
        }
        
        if( table_id != NULL )
            *table_id = t_id;

        if( field_list->ids == NULL )
            return i;
        else
            return field_list->ids[i];
    }

/* -------------------------------------------------------------------- */
/*      No match, return failure.                                       */
/* -------------------------------------------------------------------- */
    if( this_type != NULL )
        *this_type = SWQ_OTHER;

    if( table_id != NULL )
        *table_id = 0;

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
        SNPRINTF_ERR1("IN argument doesn't start with '('." );
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

        if( tokens[*tokens_consumed] == NULL ||
            (strcasecmp(tokens[*tokens_consumed],",") != 0
             && strcasecmp(tokens[*tokens_consumed],")") != 0) )
        {
            SNPRINTF_ERR1("Contents of IN predicate missing comma or closing bracket." );
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
        SNPRINTF_ERR1("Contents of IN predicate missing closing bracket." );
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
swq_subexpr_compile( char **tokens, swq_field_list *field_list,
                     swq_expr **expr_out, int *tokens_consumed )

{
    swq_expr    *op;
    const char  *error;
    int         op_code = 0;

    *tokens_consumed = 0;
    *expr_out = NULL;

    if( tokens[0] == NULL || tokens[1] == NULL )
    {
        SNPRINTF_ERR1("Not enough tokens to complete expression." );
        return swq_get_errbuf();
    }
    
    op = (swq_field_op *) SWQ_MALLOC(sizeof(swq_field_op));
    memset( op, 0, sizeof(swq_field_op) );
    op->field_index = -1;

    if( strcmp(tokens[0],"(") == 0 )
    {
        int     sub_consumed = 0;

        error = swq_subexpr_compile( tokens + 1, field_list, 
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
            SNPRINTF_ERR1("Unclosed brackets, or incomplete expression.");
            return swq_get_errbuf();
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
            swq_identify_field( tokens[*tokens_consumed], field_list,
                                &(op->field_type), 
                                &(op->table_index) );

        if( op->field_index < 0 )
        {
            swq_expr_free( op );
            SNPRINTF_ERR2( "Failed to identify field:%s", tokens[*tokens_consumed] );
            return swq_get_errbuf();
        }

        (*tokens_consumed)++;
    }

    /*
    ** Identify the operation.
    */
    if( tokens[*tokens_consumed] == NULL || tokens[*tokens_consumed+1] == NULL)
    {
        swq_expr_free( op );
        SNPRINTF_ERR1( "Not enough tokens to complete expression." );
        return swq_get_errbuf();
    }
    
    op->operation = swq_identify_op( tokens, tokens_consumed );
    if( op->operation == SWQ_UNKNOWN )
    {
        swq_expr_free( op );
        SNPRINTF_ERR2( "Failed to identify operation:%s", tokens[*tokens_consumed] );
        return swq_get_errbuf();
    }

    if( SWQ_OP_IS_LOGICAL( op->operation ) 
        && op->first_sub_expr == NULL 
        && op->operation != SWQ_NOT )
    {
        swq_expr_free( op );
        SNPRINTF_ERR1( "Used logical operation with non-logical operand.");
        return swq_get_errbuf();
    }

    if( op->field_index != -1 && op->field_type == SWQ_STRING
        && (op->operation != SWQ_EQ && op->operation != SWQ_NE
            && op->operation != SWQ_GT && op->operation != SWQ_LT
            && op->operation != SWQ_GE && op->operation != SWQ_LE
            && op->operation != SWQ_LIKE && op->operation != SWQ_NOTLIKE
            && op->operation != SWQ_IN && op->operation != SWQ_NOTIN
            && op->operation != SWQ_ISNULL && op->operation != SWQ_ISNOTNULL ))
    {
        /* NOTE: the use of names[] here is wrong.  We should be looking
           up the field that matches op->field_index and op->table_index */

        SNPRINTF_ERR3(
            "Attempt to use STRING field `%s' with numeric comparison `%s'.",
            field_list->names[op->field_index], tokens[*tokens_consumed] );
        swq_expr_free( op );
        return swq_get_errbuf();
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
        SNPRINTF_ERR1( "Not enough tokens to complete expression." );
        swq_expr_free( op );
        return swq_get_errbuf();
    }
    
    else if( SWQ_OP_IS_LOGICAL( op->operation ) )
    {
        int     sub_consumed = 0;

        error = swq_subexpr_compile( tokens + *tokens_consumed, field_list,
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
            return swq_get_errbuf();
        }
    }

    /*
    ** Otherwise collect it as a literal value.
    */
    else
    {
        op->string_value = swq_strdup(tokens[*tokens_consumed]);
        op->int_value = atoi(op->string_value);
        op->float_value = CPLAtof(op->string_value);
        
        if( op->field_index != -1 
            && (op->field_type == SWQ_INTEGER || op->field_type == SWQ_FLOAT) 
            && op->string_value[0] != '-'
            && op->string_value[0] != '+'
            && op->string_value[0] != '.'
            && (op->string_value[0] < '0' || op->string_value[0] > '9') )
        {
            /* NOTE: the use of names[] here is wrong.  We should be looking
               up the field that matches op->field_index and op->table_index */

            SNPRINTF_ERR3( 
                     "Attempt to compare numeric field `%s' to non-numeric"
                     " value `%s' is illegal.", 
                     field_list->names[op->field_index], op->string_value );
            swq_expr_free( op );
            return swq_get_errbuf();
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
    op_code = SWQ_UNKNOWN;
    if( tokens[*tokens_consumed] != NULL )
        op_code = swq_identify_op( tokens, tokens_consumed );

    if( SWQ_OP_IS_LOGICAL(op_code) )
    {
        swq_expr *remainder = NULL;
        swq_expr *parent;
        int      sub_consumed;

        error = swq_subexpr_compile( tokens + *tokens_consumed + 1, field_list,
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
                              char **field_names, 
                              swq_field_type *field_types, 
                              swq_expr **expr_out )

{
    swq_field_list  field_list;

    field_list.count = field_count;
    field_list.names = field_names;
    field_list.types = field_types;
    field_list.table_ids = NULL;
    field_list.ids = NULL;
    
    field_list.table_count = 0;
    field_list.table_defs = NULL;

    return swq_expr_compile2( where_clause, &field_list, expr_out );
}


/************************************************************************/
/*                         swq_expr_compile2()                          */
/************************************************************************/

const char *swq_expr_compile2( const char *where_clause, 
                               swq_field_list *field_list,
                               swq_expr **expr_out )

{
#define TOKEN_BLOCK_SIZE 1024
    char        **token_list, *rest_of_expr;
    int         token_count = 0;
    int         tokens_consumed, i, token_list_size;
    const char *error;
    
    /*
    ** Collect token array.
    */
    token_list = (char **)SWQ_MALLOC(sizeof(char *) * TOKEN_BLOCK_SIZE);
    token_list_size = TOKEN_BLOCK_SIZE;
    rest_of_expr = (char *) where_clause;
    while( (token_list[token_count] = 
             swq_token(rest_of_expr,&rest_of_expr,NULL)) != NULL )
    {
        token_count++;
        if (token_count == token_list_size)
        {
            token_list = (char **)swq_realloc(token_list, 
                sizeof(char *) * token_list_size, 
                  sizeof(char *) * (token_list_size + TOKEN_BLOCK_SIZE));
            token_list_size += TOKEN_BLOCK_SIZE;
        }
    }
    
    /*
    ** Parse the expression.
    */
    *expr_out = NULL;
    error = 
        swq_subexpr_compile( token_list, field_list, expr_out, 
                             &tokens_consumed );

    for( i = 0; i < token_count; i++ )
        SWQ_FREE( token_list[i] );

    SWQ_FREE(token_list);

    if( error != NULL )
        return error;

    if( tokens_consumed < token_count )
    {
        swq_expr_free( *expr_out );
        *expr_out = NULL;
        SNPRINTF_ERR2( "Syntax error, %d extra tokens", 
                 token_count - tokens_consumed );
        return swq_get_errbuf();
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
}

/************************************************************************/
/*                           swq_expr_dump()                            */
/************************************************************************/

void swq_expr_dump( swq_expr *expr, FILE * fp, int depth )

{
    char        spaces[60];
    int         i;
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

/************************************************************************/
/* ==================================================================== */
/*              SELECT statement parsing                                */
/* ==================================================================== */
/************************************************************************/

/*
Supported SQL Syntax:

SELECT <field-list> FROM <table_def>
     [LEFT JOIN <table_def> 
      ON [<table_ref>.]<key_field> = [<table_ref>.].<key_field>]*
     [WHERE <where-expr>] 
     [ORDER BY <sort specification list>]

<field-list> ::= <column-spec> [ { , <column-spec> }... ]

<column-spec> ::= <field-spec> [ <as clause> ]
                 | CAST ( <field-spec> AS <data type> ) [ <as clause> ]

<field-spec> ::= [DISTINCT] <field_ref>
                 | <field_func> ( [DISTINCT] <field-ref> )
                 | Count(*)

<as clause> ::= [ AS ] <column_name>

<data type> ::= character [ ( field_length ) ]
                | float [ ( field_length ) ]
                | numeric [ ( field_length [, field_precision ] ) ]
                | integer [ ( field_length ) ]
                | date [ ( field_length ) ]
                | time [ ( field_length ) ]
                | timestamp [ ( field_length ) ]

<field-func> ::= AVG | MAX | MIN | SUM | COUNT

<field_ref>  ::= [<table_ref>.]field_name

<sort specification list> ::=
              <sort specification> [ { <comma> <sort specification> }... ]

<sort specification> ::= <sort key> [ <ordering specification> ]

<sort key> ::=  <field_ref>

<ordering specification> ::= ASC | DESC

<table_def> ::= ['<datasource name>'.]table_name [table_alias]

<table_ref> ::= table_name | table_alias
 */

static int swq_parse_table_def( swq_select *select_info, 
                                int *is_literal,
                                char **token, char **input );

static int swq_parse_typename( swq_col_def *col_def, 
                                int *is_literal,
                                char **token, char **input );

/************************************************************************/
/*                        swq_select_preparse()                         */
/************************************************************************/

const char *swq_select_preparse( const char *select_statement, 
                                 swq_select **select_info_ret )

{
    swq_select *select_info;
    char *token;
    char *input;
    int  is_literal;
    int  type_cast;
    swq_col_def  *swq_cols;

#define MAX_COLUMNS 250

    *select_info_ret = NULL;

    if (select_statement == NULL || select_statement[0] == '\0')
    {
        SNPRINTF_ERR1( "Empty SQL request string" );
        return swq_get_errbuf();
    }

/* -------------------------------------------------------------------- */
/*      Get first token. Ensure it is SELECT.                           */
/* -------------------------------------------------------------------- */
    token = swq_token( select_statement, &input, NULL );
    if( strcasecmp(token,"select") != 0 )
    {
        SWQ_FREE( token );
        SNPRINTF_ERR1( "Missing keyword SELECT" );
        return swq_get_errbuf();
    }
    SWQ_FREE( token );

/* -------------------------------------------------------------------- */
/*      allocate selection structure.                                   */
/* -------------------------------------------------------------------- */
    select_info = (swq_select *) SWQ_MALLOC(sizeof(swq_select));
    memset( select_info, 0, sizeof(swq_select) );

    select_info->raw_select = swq_strdup( select_statement );

/* -------------------------------------------------------------------- */
/*      Allocate a big field list.                                      */
/* -------------------------------------------------------------------- */
    swq_cols = (swq_col_def *) SWQ_MALLOC(sizeof(swq_col_def) * MAX_COLUMNS);
    memset( swq_cols, 0, sizeof(swq_col_def) * MAX_COLUMNS );

    select_info->column_defs = swq_cols;

/* -------------------------------------------------------------------- */
/*      Collect the field list, terminated by FROM keyword.             */
/* -------------------------------------------------------------------- */
    token = swq_token( input, &input, &is_literal );
    while( token != NULL 
           && (is_literal || strcasecmp(token,"FROM") != 0) )
    {
        char *next_token;
        int   next_is_literal;
        
        if( select_info->result_columns == MAX_COLUMNS )
        {
            SWQ_FREE( token );
            swq_select_free( select_info );
            SNPRINTF_ERR2(
                "More than MAX_COLUMNS (%d) columns in SELECT statement.", 
                     MAX_COLUMNS );
            return swq_get_errbuf();
        }

        /* Ensure that we have a comma before fields other than the first. */

        if( select_info->result_columns > 0 )
        {
            if( strcasecmp(token,",") != 0 )
            {
                SNPRINTF_ERR2(
                         "Missing comma after column %s in SELECT statement.", 
                         swq_cols[select_info->result_columns-1].field_name );
                SWQ_FREE( token );
                swq_select_free( select_info );
                return swq_get_errbuf();
            }

            SWQ_FREE( token );
            token = swq_token( input, &input, &is_literal );
        }

        /* set up some default values. */
        swq_cols[select_info->result_columns].field_precision = -1; 
        swq_cols[select_info->result_columns].target_type = SWQ_OTHER;
        select_info->result_columns++;

        next_token = swq_token( input, &input, &next_is_literal );

        /* Detect the type cast. */
        type_cast = 0;
        if (token != NULL && next_token != NULL &&strcasecmp(token,"CAST") == 0 
            && strcasecmp(next_token,"(") == 0)
        {
            type_cast = 1;
            SWQ_FREE( token );
            SWQ_FREE( next_token );
            token = swq_token( input, &input, &is_literal );
            next_token = swq_token( input, &input, &next_is_literal );
        }

        /*
        ** Handle function operators.
        */
        if( !is_literal && !next_is_literal && next_token != NULL 
            && strcasecmp(next_token,"(") == 0 )
        {
            SWQ_FREE( next_token );

            swq_cols[select_info->result_columns-1].col_func_name = token;

            token = swq_token( input, &input, &is_literal );

            if( token != NULL && !is_literal 
                && strcasecmp(token,"DISTINCT") == 0 )
            {
                swq_cols[select_info->result_columns-1].distinct_flag = 1;

                SWQ_FREE( token );
                token = swq_token( input, &input, &is_literal );
            }

            swq_cols[select_info->result_columns-1].field_name = token;

            token = swq_token( input, &input, &is_literal );

            if( token == NULL || strcasecmp(token,")") != 0 )
            {
                if( token != NULL )
                    SWQ_FREE( token );
                swq_select_free( select_info );
                return "Missing closing bracket in field function.";
            }

            SWQ_FREE( token );
            token = swq_token( input, &input, &is_literal );
        }

        /*
        ** Handle simple field.
        */
        else
        {
            if( token != NULL && !is_literal 
                && strcasecmp(token,"DISTINCT") == 0 )
            {
                swq_cols[select_info->result_columns-1].distinct_flag = 1;

                SWQ_FREE( token );
                token = next_token;
                is_literal = next_is_literal;

                next_token = swq_token( input, &input, &next_is_literal );
            }
            
            swq_cols[select_info->result_columns-1].field_name = token;
            token = next_token;
            is_literal = next_is_literal;
        }
        
        /* handle the type cast*/
        if (type_cast && token != NULL)
        {
            if (strcasecmp(token,"AS") != 0)
            {
                SWQ_FREE( token );
                swq_select_free( select_info );
                return "Missing 'AS' keyword in the type cast in SELECT statement.";
            }

            SWQ_FREE( token );
            token = swq_token( input, &input, &is_literal );

            /* processing the typename */
            if( swq_parse_typename( &swq_cols[select_info->result_columns-1], &is_literal, &token, &input) != 0 )
            {
                swq_select_free( select_info );
                return swq_get_errbuf();
            }

            if (token != NULL && strcasecmp(token,")") != 0)
            {
                if( token != NULL )
                    SWQ_FREE( token );
                swq_select_free( select_info );
                return "Missing closing bracket after the type cast in SELECT statement.";
            }

            SWQ_FREE( token );
            token = swq_token( input, &input, &is_literal );

            type_cast = 0;
        }
        
        /* Handle the field alias */
        if( token != NULL && strcasecmp(token,",") != 0 && strcasecmp(token,"from") != 0)
        {
            /* Skip field alias keyword. */
            if (strcasecmp(token,"AS") == 0)
            {
                SWQ_FREE( token );
                token = swq_token( input, &input, &is_literal );
                if (token == NULL)
                {
                    swq_select_free( select_info );
                    return "Unexpected terminator after the type cast in SELECT statement.";
                }
            }
            swq_cols[select_info->result_columns-1].field_alias = token;
            token = swq_token( input, &input, &is_literal );
        }
    }

    /* make a columns_def list that is just the right size. */
    select_info->column_defs = (swq_col_def *) 
        SWQ_MALLOC(sizeof(swq_col_def) * select_info->result_columns);
    memcpy( select_info->column_defs, swq_cols,
            sizeof(swq_col_def) * select_info->result_columns );
    SWQ_FREE( swq_cols );

/* -------------------------------------------------------------------- */
/*      Collect the table name from the FROM clause.                    */
/* -------------------------------------------------------------------- */
    if( token == NULL || strcasecmp(token,"FROM") != 0 )
    {
        SNPRINTF_ERR1( "Missing FROM clause in SELECT statement." );
        swq_select_free( select_info );
        return swq_get_errbuf();
    }

    SWQ_FREE( token );
    token = swq_token( input, &input, &is_literal );

    if( token == NULL )
    {
        SNPRINTF_ERR1( "Missing table name in FROM clause." );
        swq_select_free( select_info );
        return swq_get_errbuf();
    }

    if( swq_parse_table_def( select_info, &is_literal, &token, &input) != 0 )
    {
        swq_select_free( select_info );
        return swq_get_errbuf();
    }

/* -------------------------------------------------------------------- */
/*      Do we have a LEFT JOIN (or just JOIN) clause?                   */
/* -------------------------------------------------------------------- */
    while( token != NULL 
        && (strcasecmp(token,"LEFT") == 0 
            || strcasecmp(token,"JOIN") == 0) )
    {
        swq_join_def *join_info;

        if( strcasecmp(token,"LEFT") == 0 )
        {
            SWQ_FREE( token );
            token = swq_token( input, &input, &is_literal );

            if( token == NULL || strcasecmp(token,"JOIN") != 0 )
            {
                SNPRINTF_ERR1( "Missing JOIN keyword after LEFT." );
                swq_select_free( select_info );
                return swq_get_errbuf();
            }
        }

        SWQ_FREE( token );
        token = swq_token( input, &input, &is_literal );
        
        /* Create join definition structure */
        select_info->join_defs = (swq_join_def *) 
            swq_realloc( select_info->join_defs, 
                         sizeof(swq_join_def) * (select_info->join_count),
                         sizeof(swq_join_def) * (select_info->join_count+1) );

        join_info = select_info->join_defs + select_info->join_count++;

        /* Parse out target table */
        join_info->secondary_table = 
            swq_parse_table_def( select_info, &is_literal, &token, &input);
        
        if( join_info->secondary_table < 0 )
        {
            swq_select_free( select_info );
            return swq_get_errbuf();
        }

        /* Check for ON keyword */
        if( token == NULL )
            token = swq_token( input, &input, &is_literal );

        if( token == NULL || strcasecmp(token,"ON") != 0 )
        {
            swq_select_free( select_info );
            SNPRINTF_ERR1( "Corrupt JOIN clause, expecting ON keyword." );
            return swq_get_errbuf();
        }

        SWQ_FREE( token );

        join_info->primary_field_name = 
            swq_token( input, &input, &is_literal );
        
        token = swq_token( input, &input, &is_literal );
        if( token == NULL || strcasecmp(token,"=") != 0 )
        {
            swq_select_free( select_info );
            SNPRINTF_ERR1( "Corrupt JOIN clause, expecting '=' condition.");
            return swq_get_errbuf();
        }

        SWQ_FREE( token );

        join_info->op = SWQ_EQ;

        join_info->secondary_field_name = 
            swq_token( input, &input, &is_literal );

        if( join_info->secondary_field_name == NULL )
        {
            swq_select_free( select_info );
            SNPRINTF_ERR1( "Corrupt JOIN clause, missing secondary field.");
            return swq_get_errbuf();
        }

        token = swq_token( input, &input, &is_literal );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a WHERE clause?                                      */
/* -------------------------------------------------------------------- */
    if( token != NULL && strcasecmp(token,"WHERE") == 0 )
    {
        const char *where_base = input;

        while( *where_base == ' ' )
            where_base++;
        
        SWQ_FREE( token );
        
        token = swq_token( input, &input, &is_literal );
        while( token != NULL )
        {
            if( strcasecmp(token,"ORDER") == 0 && !is_literal )
            {
                break;
            }

            if( token != NULL )
            {
                SWQ_FREE( token );
            
                token = swq_token( input, &input, &is_literal );
            }
        }

        select_info->whole_where_clause = swq_strdup(where_base);

        if( input != NULL )
        {
            if( token != NULL )
                select_info->whole_where_clause[input - where_base - strlen(token)] = '\0';
            else
                select_info->whole_where_clause[input - where_base] = '\0';
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse ORDER BY clause.                                          */
/* -------------------------------------------------------------------- */
    if( token != NULL && strcasecmp(token,"ORDER") == 0 )
    {
        SWQ_FREE( token );
        
        token = swq_token( input, &input, &is_literal );

        if( token == NULL || strcasecmp(token,"BY") != 0 )
        {
            if( token != NULL )
                SWQ_FREE( token );

            SNPRINTF_ERR1( "ORDER BY clause missing BY keyword." );
            swq_select_free( select_info );
            return swq_get_errbuf();
        }

        SWQ_FREE( token );
        token = swq_token( input, &input, &is_literal );
        while( token != NULL 
               && (select_info->order_specs == 0 
                   || strcasecmp(token,",") == 0) )
        {
            swq_order_def  *old_defs = select_info->order_defs;
            swq_order_def  *def;

            if( select_info->order_specs != 0 )
            {
                SWQ_FREE( token );
                token = swq_token( input, &input, &is_literal );
            }

            select_info->order_defs = (swq_order_def *) 
                SWQ_MALLOC(sizeof(swq_order_def)*(select_info->order_specs+1));

            if( old_defs != NULL )
            {
                memcpy( select_info->order_defs, old_defs, 
                        sizeof(swq_order_def)*select_info->order_specs );
                SWQ_FREE( old_defs );
            }

            def = select_info->order_defs + select_info->order_specs;
            def->field_name = token;
            def->field_index = 0;
            def->ascending_flag = 1;

            token = swq_token( input, &input, &is_literal );
            if( token != NULL && strcasecmp(token,"DESC") == 0 )
            {
                SWQ_FREE( token );
                token = swq_token( input, &input, &is_literal );

                def->ascending_flag = 0;
            } 
            else if( token != NULL && strcasecmp(token,"ASC") == 0 )
            {
                SWQ_FREE( token );
                token = swq_token( input, &input, &is_literal );
            }

            select_info->order_specs++;
        }
    }

/* -------------------------------------------------------------------- */
/*      If we have anything left it indicates an error!                 */
/* -------------------------------------------------------------------- */
    if( token != NULL )
    {

        SNPRINTF_ERR2( 
                 "Failed to parse SELECT statement, extra input at %s token.", 
                 token );

        SWQ_FREE( token );
        swq_select_free( select_info );
        return swq_get_errbuf();
    }

    *select_info_ret = select_info;

    return NULL;
}

/************************************************************************/
/*                        swq_parse_typename()                          */
/************************************************************************/

static int swq_parse_typename( swq_col_def *col_def, 
                                int *is_literal,
                                char **token, char **input )

{
    int parse_length;
    int parse_precision;

    if( *token == NULL )
        *token = swq_token( *input, input, is_literal );

    if( *token == NULL )
    {
        SNPRINTF_ERR1( "Corrupt type name, insufficient tokens." );
        return -1;
    }
    
/* -------------------------------------------------------------------- */
/*      Check for the SQL92 typenames                                   */
/* -------------------------------------------------------------------- */
    parse_length = 0;
    parse_precision = 0;
    if( strcasecmp(*token,"character") == 0 )
    {
        col_def->target_type = SWQ_STRING;
        col_def->field_length = 1;
        parse_length = 1;
    }
    else if( strcasecmp(*token,"integer") == 0 )
    {
        col_def->target_type = SWQ_INTEGER;
        parse_length = 1;
    }
    else if( strcasecmp(*token,"float") == 0 )
    {
        col_def->target_type = SWQ_FLOAT;
        parse_length = 1;
    }
    else if( strcasecmp(*token,"numeric") == 0 )
    {
        col_def->target_type = SWQ_FLOAT;
        parse_length = 1;
        parse_precision = 1;
    }
    else if( strcasecmp(*token,"timestamp") == 0 )
    {
        col_def->target_type = SWQ_TIMESTAMP;
        parse_length = 1;
    }
    else if( strcasecmp(*token,"date") == 0 )
    {
        col_def->target_type = SWQ_DATE;
        parse_length = 1;
    }
    else if( strcasecmp(*token,"time") == 0 )
    {
        col_def->target_type = SWQ_TIME;
        parse_length = 1;
    }
    else
    {
        SNPRINTF_ERR2( "Unrecognized typename %s.", *token );
        SWQ_FREE( *token );
        *token = NULL;
        return -1;
    }
    
    SWQ_FREE( *token );
    *token = swq_token( *input, input, is_literal );
    
/* -------------------------------------------------------------------- */
/*      Check for the field length and precision                        */
/* -------------------------------------------------------------------- */
    if (parse_length && *token != NULL && strcasecmp(*token,"(") == 0)
    {
        SWQ_FREE( *token );
        *token = swq_token( *input, input, is_literal );
        
        if (*token != NULL) 
        {
            col_def->field_length = atoi( *token );
            SWQ_FREE( *token );
            *token = swq_token( *input, input, is_literal );
        }

        if (parse_precision && *token != NULL && strcasecmp(*token,",") == 0) 
        {
            SWQ_FREE( *token );
            *token = swq_token( *input, input, is_literal );
            if (*token != NULL) 
            {
                col_def->field_precision = atoi( *token );
                SWQ_FREE( *token );
                *token = swq_token( *input, input, is_literal );
            }
        }
        
        if (*token == NULL || strcasecmp(*token,")") != 0) 
        {
            if (*token != NULL)
            {
                SWQ_FREE( *token );
                *token = NULL;
            }
            SNPRINTF_ERR1( "Missing closing bracket in the field length specifier." );
            return -1;
        }

        SWQ_FREE( *token );
        *token = swq_token( *input, input, is_literal );
    }
    return  0;
}

/************************************************************************/
/*                        swq_parse_table_def()                         */
/*                                                                      */
/*      Supported table definition forms:                               */
/*                                                                      */
/*      <table_def> :== table_name                                      */
/*                   |  'data_source'.table_name                        */
/*                   |  table_name table_alias                          */
/*                   |  'data_source'.table_name table_alias            */
/************************************************************************/

static int swq_parse_table_def( swq_select *select_info, 
                                int *is_literal,
                                char **token, char **input )

{
    int  i;
    char *datasource = NULL;
    char *table = NULL;
    char *alias = NULL;

    if( *token == NULL )
        *token = swq_token( *input, input, is_literal );

    if( *token == NULL )
    {
        SNPRINTF_ERR1( "Corrupt table definition, insufficient tokens." );
        return -1;
    }
    
/* -------------------------------------------------------------------- */
/*      Do we have a datasource literal?                                */
/* -------------------------------------------------------------------- */
    if( *token != NULL && *is_literal )
    {
        datasource = *token;
        *token = swq_token( *input, input, is_literal );

        if( *token == NULL )
        {
            *token = datasource;
            datasource = NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Get the table name.  Remove the '.' used to qualify it          */
/*      relative to the datasource name if found.                       */
/* -------------------------------------------------------------------- */
    if( datasource != NULL && (*token)[0] != '.' )
    {
        table = datasource;
        datasource = NULL;
    }
    else if( (*token)[0] == '.' )
    {
        table = swq_strdup( (*token) + 1 );
        SWQ_FREE( *token );
        *token = swq_token( *input, input, is_literal );
    }
    else
    {
        table = *token;
        *token = swq_token( *input, input, is_literal );
    }

/* -------------------------------------------------------------------- */
/*      Was an alias provided?                                          */
/* -------------------------------------------------------------------- */
    if( *token != NULL && ! *is_literal
        && strcasecmp(*token,"ON") != 0
        && strcasecmp(*token,"ORDER") != 0
        && strcasecmp(*token,"WHERE") != 0
        && strcasecmp(*token,"LEFT") != 0
        && strcasecmp(*token,"JOIN") != 0 )
    {
        alias = *token;
        *token = swq_token( *input, input, is_literal );
    }

/* -------------------------------------------------------------------- */
/*      Does this match an existing table definition?                   */
/* -------------------------------------------------------------------- */
    for( i = 0; i < select_info->table_count; i++ )
    {
        swq_table_def *table_def = select_info->table_defs + i;

        if( datasource == NULL 
            && alias == NULL 
            && strcasecmp(table_def->table_alias,table) == 0 )
            return i;

        if( datasource != NULL && table_def->data_source != NULL
            && strcasecmp(datasource,table_def->data_source) == 0 
            && strcasecmp(table,table_def->table_name) == 0 )
            return i;
    }

/* -------------------------------------------------------------------- */
/*      Add a new entry to the tables table.                            */
/* -------------------------------------------------------------------- */
    select_info->table_defs = 
        swq_realloc( select_info->table_defs, 
                     sizeof(swq_table_def) * (select_info->table_count),
                     sizeof(swq_table_def) * (select_info->table_count+1) );

/* -------------------------------------------------------------------- */
/*      Populate the new entry.                                         */
/* -------------------------------------------------------------------- */
    if( alias == NULL )
        alias = swq_strdup( table );

    select_info->table_defs[select_info->table_count].data_source = datasource;
    select_info->table_defs[select_info->table_count].table_name = table;
    select_info->table_defs[select_info->table_count].table_alias = alias;
    
    select_info->table_count++;

    return select_info->table_count - 1;
}

/************************************************************************/
/*                     swq_select_expand_wildcard()                     */
/*                                                                      */
/*      This function replaces the '*' in a "SELECT *" with the list    */
/*      provided list of fields.  Itis used by swq_select_parse(),      */
/*      but may be called in advance by applications wanting the        */
/*      "default" field list to be different than the full list of      */
/*      fields.                                                         */
/************************************************************************/

const char *swq_select_expand_wildcard( swq_select *select_info, 
                                        swq_field_list *field_list )

{
    int isrc;

/* ==================================================================== */
/*      Check each pre-expansion field.                                 */
/* ==================================================================== */
    for( isrc = 0; isrc < select_info->result_columns; isrc++ )
    {
        const char *src_fieldname = select_info->column_defs[isrc].field_name;
        int itable, new_fields, i, iout;

        if( src_fieldname[strlen(src_fieldname)-1] != '*' )
            continue;

        /* We don't want to expand COUNT(*) */
        if( select_info->column_defs[isrc].col_func_name != NULL )
            continue;

/* -------------------------------------------------------------------- */
/*      Parse out the table name, verify it, and establish the          */
/*      number of fields to insert from it.                             */
/* -------------------------------------------------------------------- */
        if( strcmp(src_fieldname,"*") == 0 )
        {
            itable = -1;
            new_fields = field_list->count;
        }
        else if( strlen(src_fieldname) < 3 
                 || src_fieldname[strlen(src_fieldname)-2] != '.' )
        {
            SNPRINTF_ERR2( "Ill formatted field definition '%s'.",
                     src_fieldname );
            return swq_get_errbuf();
        }
        else
        {
            char *table_name = swq_strdup( src_fieldname );
            table_name[strlen(src_fieldname)-2] = '\0';

            for( itable = 0; itable < field_list->table_count; itable++ )
            {
                if( strcasecmp(table_name,
                        field_list->table_defs[itable].table_alias ) == 0 )
                    break;
            }
            
            if( itable == field_list->table_count )
            {
                SNPRINTF_ERR3( 
                         "Table %s not recognised from %s definition.", 
                         table_name, src_fieldname );
                swq_free( table_name );
                return swq_get_errbuf();
            }
            swq_free( table_name );
            
            /* count the number of fields in this table. */
            new_fields = 0;
            for( i = 0; i < field_list->count; i++ )
            {
                if( field_list->table_ids[i] == itable )
                    new_fields++;
            }
        }

        if (new_fields > 0)
        {
/* -------------------------------------------------------------------- */
/*      Reallocate the column list larger.                              */
/* -------------------------------------------------------------------- */
            SWQ_FREE( select_info->column_defs[isrc].field_name );
            select_info->column_defs = (swq_col_def *) 
                swq_realloc( select_info->column_defs, 
                            sizeof(swq_col_def) * select_info->result_columns, 
                            sizeof(swq_col_def) * 
                            (select_info->result_columns + new_fields - 1 ) );

/* -------------------------------------------------------------------- */
/*      Push the old definitions that came after the one to be          */
/*      replaced further up in the array.                               */
/* -------------------------------------------------------------------- */
            if (new_fields != 1)
            {
                for( i = select_info->result_columns-1; i > isrc; i-- )
                {
                    memcpy( select_info->column_defs + i + new_fields - 1,
                            select_info->column_defs + i,
                            sizeof( swq_col_def ) );
                }
            }

            select_info->result_columns += (new_fields - 1 );

/* -------------------------------------------------------------------- */
/*      Zero out all the stuff in the target column definitions.        */
/* -------------------------------------------------------------------- */
            memset( select_info->column_defs + isrc, 0, 
                    new_fields * sizeof(swq_col_def) );
        }
        else
        {
/* -------------------------------------------------------------------- */
/*      The wildcard expands to nothing                                 */
/* -------------------------------------------------------------------- */
            SWQ_FREE( select_info->column_defs[isrc].field_name );
            memmove( select_info->column_defs + isrc,
                     select_info->column_defs + isrc + 1,
                     sizeof( swq_col_def ) * (select_info->result_columns-1-isrc) );

            select_info->result_columns --;
        }

/* -------------------------------------------------------------------- */
/*      Assign the selected fields.                                     */
/* -------------------------------------------------------------------- */
        iout = isrc;
        
        for( i = 0; i < field_list->count; i++ )
        {
            swq_col_def *def;
            int compose = itable != -1;

            /* skip this field if it isn't in the target table.  */
            if( itable != -1 && field_list->table_ids != NULL 
                && itable != field_list->table_ids[i] )
                continue;

            /* set up some default values. */
            def = select_info->column_defs + iout;
            def->field_precision = -1; 
            def->target_type = SWQ_OTHER;

            /* does this field duplicate an earlier one? */
            if( field_list->table_ids != NULL 
                && field_list->table_ids[i] != 0 
                && !compose )
            {
                int other;

                for( other = 0; other < i; other++ )
                {
                    if( strcasecmp(field_list->names[i],
                                   field_list->names[other]) == 0 )
                    {
                        compose = 1;
                        break;
                    }
                }
            }

            if( !compose )
                def->field_name = swq_strdup( field_list->names[i] );
            else
            {
                int itable = field_list->table_ids[i];
                char *composed_name;
                const char *field_name = field_list->names[i];
                const char *table_alias = 
                    field_list->table_defs[itable].table_alias;

                composed_name = (char *) 
                    swq_malloc(strlen(field_name)+strlen(table_alias)+2);

                sprintf( composed_name, "%s.%s", table_alias, field_name );

                def->field_name = composed_name;
            }							

            iout++;

            /* All the other table info will be provided by the later
               parse operation. */
        }

        /* If there are several occurrences of '*', go on, but stay on the */
        /* same index in case '*' is expanded to nothing */
        /* (the -- is to compensate the fact that isrc will be incremented in */
        /*  the after statement of the for loop) */
        isrc --;
    }

    

    return NULL;
}

/************************************************************************/
/*                          swq_select_parse()                          */
/************************************************************************/

const char *swq_select_parse( swq_select *select_info, 
                              swq_field_list *field_list,
                              int parse_flags )

{
    int  i;
    const char *error;

    error = swq_select_expand_wildcard( select_info, field_list );
    if( error != NULL )
        return error;

/* -------------------------------------------------------------------- */
/*      Identify field information.                                     */
/* -------------------------------------------------------------------- */
    for( i = 0; i < select_info->result_columns; i++ )
    {
        swq_col_def *def = select_info->column_defs + i;
        swq_field_type  this_type;

        /* identify field */
        def->field_index = swq_identify_field( def->field_name, field_list,
                                               &this_type, 
                                               &(def->table_index) );

        /* record field type */
        def->field_type = this_type;

        /* identify column function if present */
        if( def->col_func_name != NULL )
        {
            if( strcasecmp(def->col_func_name,"AVG") == 0 )
                def->col_func = SWQCF_AVG;
            else if( strcasecmp(def->col_func_name,"MIN") == 0 )
                def->col_func = SWQCF_MIN;
            else if( strcasecmp(def->col_func_name,"MAX") == 0 )
                def->col_func = SWQCF_MAX;
            else if( strcasecmp(def->col_func_name,"SUM") == 0 )
                def->col_func = SWQCF_SUM;
            else if( strcasecmp(def->col_func_name,"COUNT") == 0 )
                def->col_func = SWQCF_COUNT;
            else
            {
                def->col_func = SWQCF_CUSTOM;
                if( !(parse_flags & SWQP_ALLOW_UNDEFINED_COL_FUNCS) )
                {
                    SNPRINTF_ERR2( "Unrecognised field function %s.",
                             def->col_func_name );
                    return swq_get_errbuf();
                }
            }

            if( (def->col_func == SWQCF_MIN 
                 || def->col_func == SWQCF_MAX
                 || def->col_func == SWQCF_AVG
                 || def->col_func == SWQCF_SUM)
                && this_type == SWQ_STRING )
            {
                SNPRINTF_ERR3( 
                     "Use of field function %s() on string field %s illegal.", 
                         def->col_func_name, def->field_name );
                return swq_get_errbuf();
            }
        }
        else
            def->col_func = SWQCF_NONE;

        if( def->field_index == -1 && def->col_func != SWQCF_COUNT )
        {
            SNPRINTF_ERR2( "Unrecognised field name %s.", 
                     def->field_name );
            return swq_get_errbuf();
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if we are producing a one row summary result or a set     */
/*      of records.  Generate an error if we get conflicting            */
/*      indications.                                                    */
/* -------------------------------------------------------------------- */
    select_info->query_mode = -1;
    for( i = 0; i < select_info->result_columns; i++ )
    {
        swq_col_def *def = select_info->column_defs + i;
        int this_indicator = -1;

        if( def->col_func == SWQCF_MIN 
            || def->col_func == SWQCF_MAX
            || def->col_func == SWQCF_AVG
            || def->col_func == SWQCF_SUM
            || def->col_func == SWQCF_COUNT )
            this_indicator = SWQM_SUMMARY_RECORD;
        else if( def->col_func == SWQCF_NONE )
        {
            if( def->distinct_flag )
                this_indicator = SWQM_DISTINCT_LIST;
            else
                this_indicator = SWQM_RECORDSET;
        }

        if( this_indicator != select_info->query_mode
             && this_indicator != -1
            && select_info->query_mode != -1 )
        {
            return "Field list implies mixture of regular recordset mode, summary mode or distinct field list mode.";
        }

        if( this_indicator != -1 )
            select_info->query_mode = this_indicator;
    }

    if( select_info->result_columns > 1 
        && select_info->query_mode == SWQM_DISTINCT_LIST )
    {
        return "SELECTing more than one DISTINCT field is a query not supported.";
    }
    else if (select_info->result_columns == 0)
    {
        select_info->query_mode = SWQM_RECORDSET;
    }

/* -------------------------------------------------------------------- */
/*      Process column names in JOIN specs.                             */
/* -------------------------------------------------------------------- */
    for( i = 0; i < select_info->join_count; i++ )
    {
        swq_join_def *def = select_info->join_defs + i;
        int          table_id;

        /* identify primary field */
        def->primary_field = swq_identify_field( def->primary_field_name,
                                                 field_list, NULL, &table_id );
        if( def->primary_field == -1 )
        {
            SNPRINTF_ERR2(
                     "Unrecognised primary field %s in JOIN clause..", 
                     def->primary_field_name );
            return swq_get_errbuf();
        }
        
        if( table_id != 0 )
        {
            SNPRINTF_ERR2( 
                     "Currently the primary key must come from the primary table in\n"
                     "JOIN, %s is not from the primary table.",
                     def->primary_field_name );
            return swq_get_errbuf();
        }
        
        /* identify secondary field */
        def->secondary_field = swq_identify_field( def->secondary_field_name,
                                                   field_list, NULL,&table_id);
        if( def->secondary_field == -1 )
        {
            SNPRINTF_ERR2( 
                     "Unrecognised secondary field %s in JOIN clause..", 
                     def->primary_field_name );
            return swq_get_errbuf();
        }
        
        if( table_id != def->secondary_table )
        {
            SNPRINTF_ERR3( 
                     "Currently the secondary key must come from the secondary table\n"
                     "listed in the JOIN.  %s is not from table %s..",
                     def->primary_field_name,
                     select_info->table_defs[def->secondary_table].table_name);
            return swq_get_errbuf();
        }
    }

/* -------------------------------------------------------------------- */
/*      Process column names in order specs.                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < select_info->order_specs; i++ )
    {
        swq_order_def *def = select_info->order_defs + i;

        /* identify field */
        def->field_index = swq_identify_field( def->field_name, field_list,
                                               NULL, &(def->table_index) );
        if( def->field_index == -1 )
        {
            SNPRINTF_ERR2( "Unrecognised field name %s in ORDER BY.", 
                     def->field_name );
            return swq_get_errbuf();
        }
    }

/* -------------------------------------------------------------------- */
/*      Parse the WHERE clause.                                         */
/* -------------------------------------------------------------------- */
    if( select_info->whole_where_clause != NULL )
    {
        const char *error;

        error = swq_expr_compile2( select_info->whole_where_clause, 
                                   field_list, &(select_info->where_expr) );

        if( error != NULL )
            return error;
    }

    return NULL;
}

/************************************************************************/
/*                        swq_select_summarize()                        */
/************************************************************************/

const char *
swq_select_summarize( swq_select *select_info, 
                      int dest_column, const char *value )

{
    swq_col_def *def = select_info->column_defs + dest_column;
    swq_summary *summary;

/* -------------------------------------------------------------------- */
/*      Do various checking.                                            */
/* -------------------------------------------------------------------- */
    if( !select_info->query_mode == SWQM_RECORDSET )
        return "swq_select_summarize() called on non-summary query.";

    if( dest_column < 0 || dest_column >= select_info->result_columns )
        return "dest_column out of range in swq_select_summarize().";

    if( def->col_func == SWQCF_NONE && !def->distinct_flag )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the summary information if this is the first row         */
/*      being processed.                                                */
/* -------------------------------------------------------------------- */
    if( select_info->column_summary == NULL )
    {
        int i;

        select_info->column_summary = (swq_summary *) 
            SWQ_MALLOC(sizeof(swq_summary) * select_info->result_columns);
        memset( select_info->column_summary, 0, 
                sizeof(swq_summary) * select_info->result_columns );

        for( i = 0; i < select_info->result_columns; i++ )
        {
            select_info->column_summary[i].min = 1e20;
            select_info->column_summary[i].max = -1e20;
        }
    }

/* -------------------------------------------------------------------- */
/*      If distinct processing is on, process that now.                 */
/* -------------------------------------------------------------------- */
    summary = select_info->column_summary + dest_column;
    
    if( def->distinct_flag )
    {
        int  i;

        /* This should be implemented with a much more complicated
           data structure to achieve any sort of efficiency. */
        for( i = 0; i < summary->count; i++ )
        {
            if( strcmp(value,summary->distinct_list[i]) == 0 )
                break;
        }
        
        if( i == summary->count )
        {
            char  **old_list = summary->distinct_list;
            
            summary->distinct_list = (char **) 
                SWQ_MALLOC(sizeof(char *) * (summary->count+1));
            memcpy( summary->distinct_list, old_list, 
                    sizeof(char *) * summary->count );
            summary->distinct_list[(summary->count)++] = 
                swq_strdup( value );

            SWQ_FREE(old_list);
        }
    }

/* -------------------------------------------------------------------- */
/*      Process various options.                                        */
/* -------------------------------------------------------------------- */

    switch( def->col_func )
    {
      case SWQCF_MIN:
        if( value != NULL && value[0] != '\0' )
        {
            double df_val = CPLAtof(value);
            if( df_val < summary->min )
                summary->min = df_val;
        }
        break;
      case SWQCF_MAX:
        if( value != NULL && value[0] != '\0' )
        {
            double df_val = CPLAtof(value);
            if( df_val > summary->max )
                summary->max = df_val;
        }
        break;
      case SWQCF_AVG:
      case SWQCF_SUM:
        if( value != NULL && value[0] != '\0' )
        {
            summary->count++;
            summary->sum += CPLAtof(value);
        }
        break;

      case SWQCF_COUNT:
        if( value != NULL && !def->distinct_flag )
            summary->count++;
        break;

      case SWQCF_NONE:
        break;

      case SWQCF_CUSTOM:
        return "swq_select_summarize() called on custom field function.";

      default:
        return "swq_select_summarize() - unexpected col_func";
    }

    return NULL;
}
/************************************************************************/
/*                      sort comparison functions.                      */
/************************************************************************/

static int FORCE_CDECL swq_compare_int( const void *item1, const void *item2 )
{
    int  v1, v2;

    v1 = atoi(*((const char **) item1));
    v2 = atoi(*((const char **) item2));

    if( v1 < v2 )
        return -1;
    else if( v1 == v2 )
        return 0;
    else
        return 1;
}

static int FORCE_CDECL swq_compare_real( const void *item1, const void *item2 )
{
    double  v1, v2;

    v1 = CPLAtof(*((const char **) item1));
    v2 = CPLAtof(*((const char **) item2));

    if( v1 < v2 )
        return -1;
    else if( v1 == v2 )
        return 0;
    else
        return 1;
}

static int FORCE_CDECL swq_compare_string( const void *item1, const void *item2 )
{
    return strcmp( *((const char **) item1), *((const char **) item2) );
}

/************************************************************************/
/*                    swq_select_finish_summarize()                     */
/*                                                                      */
/*      Call to complete summarize work.  Does stuff like ordering      */
/*      the distinct list for instance.                                 */
/************************************************************************/

const char *swq_select_finish_summarize( swq_select *select_info )

{
    int (FORCE_CDECL *compare_func)(const void *, const void*);
    int count = 0;
    char **distinct_list = NULL;

    if( select_info->query_mode != SWQM_DISTINCT_LIST 
        || select_info->order_specs == 0 )
        return NULL;

    if( select_info->order_specs > 1 )
        return "Can't ORDER BY a DISTINCT list by more than one key.";

    if( select_info->order_defs[0].field_index != 
        select_info->column_defs[0].field_index )
        return "Only selected DISTINCT field can be used for ORDER BY.";

    if( select_info->column_summary == NULL )
        return NULL;

    if( select_info->column_defs[0].field_type == SWQ_INTEGER )
        compare_func = swq_compare_int;
    else if( select_info->column_defs[0].field_type == SWQ_FLOAT )
        compare_func = swq_compare_real;
    else
        compare_func = swq_compare_string;

    distinct_list = select_info->column_summary[0].distinct_list;
    count = select_info->column_summary[0].count;

    qsort( distinct_list, count, sizeof(char *), compare_func );

/* -------------------------------------------------------------------- */
/*      Do we want the list ascending in stead of descending?           */
/* -------------------------------------------------------------------- */
    if( !select_info->order_defs[0].ascending_flag )
    {
        char *saved;
        int i;

        for( i = 0; i < count/2; i++ )
        {
            saved = distinct_list[i];
            distinct_list[i] = distinct_list[count-i-1];
            distinct_list[count-i-1] = saved;
        }
    }

    return NULL;
}

/************************************************************************/
/*                          swq_select_free()                           */
/************************************************************************/

void swq_select_free( swq_select *select_info )

{
    int i;

    if( select_info == NULL )
        return;

    if( select_info->where_expr != NULL )
        swq_expr_free(select_info->where_expr);

    if( select_info->raw_select != NULL )
        SWQ_FREE( select_info->raw_select );

    if( select_info->whole_where_clause != NULL )
        SWQ_FREE( select_info->whole_where_clause );

    for( i = 0; i < select_info->table_count; i++ )
    {
        swq_table_def *table_def = select_info->table_defs + i;

        if( table_def->data_source != NULL )
            SWQ_FREE( table_def->data_source );
        SWQ_FREE( table_def->table_name );
        SWQ_FREE( table_def->table_alias );
    }
    if( select_info->table_defs != NULL )
        SWQ_FREE( select_info->table_defs );

    for( i = 0; i < select_info->result_columns; i++ )
    {
        if( select_info->column_defs[i].field_name != NULL )
            SWQ_FREE( select_info->column_defs[i].field_name );
        if( select_info->column_defs[i].field_alias != NULL )
            SWQ_FREE( select_info->column_defs[i].field_alias );
        if( select_info->column_defs[i].col_func_name != NULL )
            SWQ_FREE( select_info->column_defs[i].col_func_name );

        if( select_info->column_summary != NULL 
            && select_info->column_summary[i].distinct_list != NULL )
        {
            int j;
            
            for( j = 0; j < select_info->column_summary[i].count; j++ )
                SWQ_FREE( select_info->column_summary[i].distinct_list[j] );

            SWQ_FREE( select_info->column_summary[i].distinct_list );
        }
    }

    if( select_info->column_defs != NULL )
        SWQ_FREE( select_info->column_defs );

    if( select_info->column_summary != NULL )
        SWQ_FREE( select_info->column_summary );

    for( i = 0; i < select_info->order_specs; i++ )
    {
        if( select_info->order_defs[i].field_name != NULL )
            SWQ_FREE( select_info->order_defs[i].field_name );
    }
    
    if( select_info->order_defs != NULL )
        SWQ_FREE( select_info->order_defs );

    for( i = 0; i < select_info->join_count; i++ )
    {
        SWQ_FREE( select_info->join_defs[i].primary_field_name );
        if( select_info->join_defs[i].secondary_field_name != NULL )
            SWQ_FREE( select_info->join_defs[i].secondary_field_name );
    }
    if( select_info->join_defs != NULL )
        SWQ_FREE( select_info->join_defs );

    SWQ_FREE( select_info );
}

/************************************************************************/
/*                         swq_reform_command()                         */
/*                                                                      */
/*      Rebuild the command string from the components in the           */
/*      swq_select structure.  The where expression is taken from       */
/*      the whole_where_clause instead of being reformed.  The          */
/*      results of forming the command are applied to the raw_select    */
/*      field.                                                          */
/************************************************************************/

#define CHECK_COMMAND( new_bytes ) grow_command( &command, &max_cmd_size, &cmd_size, new_bytes );

static void grow_command( char **p_command, int *max_cmd_size, int *cmd_size,
                          int new_bytes );

const char *swq_reform_command( swq_select *select_info )

{
    char        *command;
    int         max_cmd_size = 10;
    int         cmd_size = 0;
    int         i;

    command = SWQ_MALLOC(max_cmd_size);

    strcpy( command, "SELECT " );

/* -------------------------------------------------------------------- */
/*      Handle the field list.                                          */
/* -------------------------------------------------------------------- */
    for( i = 0; i < select_info->result_columns; i++ )
    {
        swq_col_def *def = select_info->column_defs + i;
        const char *distinct = "";

        if( def->distinct_flag )
            distinct = "DISTINCT ";

        if( i != 0 )
        {
            CHECK_COMMAND(3);
            strcat( command + cmd_size, ", " );
        }

        if( def->col_func_name != NULL )
        {
            CHECK_COMMAND( strlen(def->col_func_name) 
                           + strlen(def->field_name) + 15 );
            sprintf( command + cmd_size, "%s(%s%s)", 
                     def->col_func_name, distinct, def->field_name );
        }
        else
        {
            CHECK_COMMAND( strlen(def->field_name) + 15 );
            sprintf( command + cmd_size, "%s\"%s\"", 
                     distinct, def->field_name );
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle the FROM tablename.                                      */
/* -------------------------------------------------------------------- */
    if( select_info->table_count > 0 )
    {
        CHECK_COMMAND( 10 + strlen(select_info->table_defs[0].table_name) );
        sprintf( command + cmd_size, " FROM \"%s\"", 
                 select_info->table_defs[0].table_name );
    }

/* -------------------------------------------------------------------- */
/*      Handle the JOIN clause(s).                                      */
/* -------------------------------------------------------------------- */
    /* TODO notdef */

/* -------------------------------------------------------------------- */
/*      Add WHERE statement if it exists.                               */
/* -------------------------------------------------------------------- */
    if( select_info->whole_where_clause != NULL )
    {
        CHECK_COMMAND( 12 + strlen(select_info->whole_where_clause) );
        sprintf( command + cmd_size, " WHERE %s", 
                 select_info->whole_where_clause );
    }

/* -------------------------------------------------------------------- */
/*      Add order by clause(s) if appropriate.                          */
/* -------------------------------------------------------------------- */
    for( i = 0; i < select_info->order_specs; i++ )
    {
        swq_order_def *def = select_info->order_defs + i;

        if( i == 0 )
        {
            CHECK_COMMAND( 12 );
            sprintf( command + cmd_size, " ORDER BY " );
        }
        else
        {
            CHECK_COMMAND( 3 );
            sprintf( command + cmd_size, ", " );
        }

        CHECK_COMMAND( strlen(def->field_name)+1 );
        sprintf( command + cmd_size, "\"%s\"", def->field_name );

        CHECK_COMMAND( 6 );
        if( def->ascending_flag )
            strcat( command + cmd_size, " ASC" );
        else
            strcat( command + cmd_size, " DESC" );
    }

/* -------------------------------------------------------------------- */
/*      Assign back to the select info.                                 */
/* -------------------------------------------------------------------- */
    SWQ_FREE( select_info->raw_select );
    select_info->raw_select = command;

    return NULL;
}

/* helper for the swq_reform_command() function. */

static void grow_command( char **p_command, int *max_cmd_size, int *cmd_size,
                          int new_bytes )

{
    char *new_command;

    *cmd_size += strlen(*p_command + *cmd_size);

    if( *cmd_size + new_bytes < *max_cmd_size - 1 )
        return;

    *max_cmd_size = 2 * *max_cmd_size;
    if( *max_cmd_size < *cmd_size + new_bytes )
        *max_cmd_size = *cmd_size + new_bytes + 100;

    new_command = SWQ_MALLOC(*max_cmd_size);

    strcpy( new_command, *p_command );
    SWQ_FREE( *p_command );
    *p_command = new_command;
}
