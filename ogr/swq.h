/******************************************************************************
 *
 * Component: OGDI Driver Support Library
 * Purpose: Generic SQL WHERE Expression Evaluator Declarations.
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
 * Revision 1.4  2002/04/23 20:05:23  warmerda
 * added SELECT statement parsing
 *
 * Revision 1.3  2002/04/19 20:46:06  warmerda
 * added [NOT] IN, [NOT] LIKE and IS [NOT] NULL support
 *
 * Revision 1.2  2001/07/19 18:25:07  warmerda
 * expanded tabs
 *
 * Revision 1.1  2001/06/19 15:46:30  warmerda
 * New
 *
 */

#ifndef _SWQ_H_INCLUDED_
#define _SWQ_H_INCLUDED_

typedef enum {
    SWQ_OR,
    SWQ_AND,
    SWQ_NOT,
    SWQ_EQ,
    SWQ_NE,
    SWQ_GE,
    SWQ_LE,
    SWQ_LT,
    SWQ_GT,
    SWQ_LIKE,
    SWQ_NOTLIKE,
    SWQ_ISNULL,
    SWQ_ISNOTNULL,
    SWQ_IN,
    SWQ_NOTIN,
    SWQ_UNKNOWN
} swq_op;

typedef enum {
    SWQ_INTEGER,
    SWQ_FLOAT,
    SWQ_STRING, 
    SWQ_BOOLEAN,
    SWQ_OTHER
} swq_field_type;

typedef struct {
    swq_op      operation;

    /* only for logical expression on subexpression */
    struct swq_node_s  *first_sub_expr;
    struct swq_node_s  *second_sub_expr;

    /* only for binary field operations */
    int         field_index;
    swq_field_type field_type;
    char        *string_value;
    int         int_value;
    double      float_value;
} swq_field_op;

typedef swq_field_op swq_expr;

typedef int (*swq_op_evaluator)(swq_field_op *op, void *record_handle);

/* Compile an SQL WHERE clause into an internal form.  The field_list is
** the list of fields in the target 'table', used to render where into 
** field numbers instead of names. 
*/
const char *swq_expr_compile( const char *where_clause, 
                         int field_count,
                         char **field_list,
                         swq_field_type *field_types,
                         swq_expr **expr );

/*
** Evaluate an expression for a particular record using an application
** provided field operation evaluator, and abstract record handle. 
*/
int swq_expr_evaluate( swq_expr *expr, swq_op_evaluator fn_evaluator,
                       void *record_handle );

void swq_expr_free( swq_expr * );

int swq_test_like( const char *input, const char *pattern );


/****************************************************************************/

#define SWQP_ALLOW_UNDEFINED_COL_FUNCS 0x01

typedef enum {
    SWQCF_NONE,
    SWQCF_AVG,
    SWQCF_MIN,
    SWQCF_MAX,
    SWQCF_COUNT,
    SWQCF_SUM,
    SWQCF_CUSTOM
} swq_col_func;

typedef struct {
    swq_col_func col_func;
    char         *col_func_name;
    char         *field_name;
    int          field_index;
    int          distinct_flag;
} swq_col_def;

typedef struct {
    int         count;
    
    char        **distinct_list;
    double      sum;
    double      min;
    double      max;
} swq_summary;

typedef struct {
    char *field_name;
    int   field_index;
    int   ascending_flag;
} swq_order_def;

typedef struct {
    int   summary_record_only;

    char        *raw_select;

    int result_columns;
    swq_col_def *column_defs;
    swq_summary *column_summary;

    char        *whole_where_clause;
    swq_expr    *where_expr;

    char        *from_table;

    int         order_specs;
    swq_order_def *order_defs;    
} swq_select;

const char *swq_select_preparse( const char *select_statement, 
                                 swq_select **select_info );
const char *swq_select_parse( swq_select *select_info,
                              int field_count, 
                              char **field_list,
                              swq_field_type *field_types,
                              int parse_flags );
void swq_select_free( swq_select *select_info );

const char *swq_select_summarize( swq_select *select_info, 
                                  int dest_column, 
                                  const char *value );


#endif /* def _SWQ_H_INCLUDED_ */
