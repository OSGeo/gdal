/******************************************************************************
 *
 * Component: OGDI Driver Support Library
 * Purpose: Generic SQL WHERE Expression Evaluator Declarations.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 * 
 ******************************************************************************
 * Copyright (C) 2001 Information Interoperability Institute (3i)
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#ifndef _SWQ_H_INCLUDED_
#define _SWQ_H_INCLUDED_

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_core.h"

#if defined(_WIN32) && !defined(_WIN32_WCE)
#  define strcasecmp stricmp
#elif defined(_WIN32_WCE)
#  define strcasecmp _stricmp
#endif

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
    SWQ_ISNULL,
    SWQ_IN,
    SWQ_BETWEEN,
    SWQ_ADD,
    SWQ_SUBTRACT,
    SWQ_MULTIPLY,
    SWQ_DIVIDE,
    SWQ_MODULUS,
    SWQ_CONCAT,
    SWQ_SUBSTR,
    SWQ_HSTORE_GET_VALUE,
    SWQ_AVG,
    SWQ_MIN,
    SWQ_MAX,
    SWQ_COUNT,
    SWQ_SUM,
    SWQ_CAST,
    SWQ_FUNC_DEFINED,
    SWQ_UNKNOWN
} swq_op;

typedef enum {
    SWQ_INTEGER,
    SWQ_INTEGER64,
    SWQ_FLOAT,
    SWQ_STRING, 
    SWQ_BOOLEAN,  // integer
    SWQ_DATE,     // string
    SWQ_TIME,     // string
    SWQ_TIMESTAMP,// string
    SWQ_GEOMETRY,
    SWQ_NULL,
    SWQ_OTHER,
    SWQ_ERROR
} swq_field_type;

#define SWQ_IS_INTEGER(x) ((x) == SWQ_INTEGER || (x) == SWQ_INTEGER64)

typedef enum {
    SNT_CONSTANT,
    SNT_COLUMN, 
    SNT_OPERATION
} swq_node_type;


class swq_field_list;
class swq_expr_node;
class swq_select;
class OGRGeometry;

typedef swq_expr_node *(*swq_field_fetcher)( swq_expr_node *op,
                                             void *record_handle );
typedef swq_expr_node *(*swq_op_evaluator)(swq_expr_node *op,
                                           swq_expr_node **sub_field_values );
typedef swq_field_type (*swq_op_checker)( swq_expr_node *op,
                                          int bAllowMismatchTypeOnFieldComparison );

class swq_expr_node {
    static CPLString   QuoteIfNecessary( const CPLString &, char chQuote = '\'' );
    static CPLString   Quote( const CPLString &, char chQuote = '\'' );
public:
    swq_expr_node();

    swq_expr_node( const char * );
    swq_expr_node( int );
    swq_expr_node( GIntBig );
    swq_expr_node( double );
    swq_expr_node( OGRGeometry* );
    swq_expr_node( swq_op );

    ~swq_expr_node();

    void           Initialize();
    CPLString      UnparseOperationFromUnparsedSubExpr(char** apszSubExpr);
    char          *Unparse( swq_field_list *, char chColumnQuote );
    void           Dump( FILE *fp, int depth );
    swq_field_type Check( swq_field_list *, int bAllowFieldsInSecondaryTables,
                          int bAllowMismatchTypeOnFieldComparison );
    swq_expr_node* Evaluate( swq_field_fetcher pfnFetcher, 
                             void *record );
    swq_expr_node* Clone();

    void           ReplaceBetweenByGEAndLERecurse();

    swq_node_type eNodeType;
    swq_field_type field_type;

    /* only for SNT_OPERATION */
    void        PushSubExpression( swq_expr_node * );
    void        ReverseSubExpressions();
    int         nOperation;
    int         nSubExprCount;
    swq_expr_node **papoSubExpr;

    /* only for SNT_COLUMN */
    int         field_index;
    int         table_index;
    char        *table_name;

    /* only for SNT_CONSTANT */
    int         is_null;
    GIntBig     int_value;
    double      float_value;
    OGRGeometry *geometry_value;
    
    /* shared by SNT_COLUMN and SNT_CONSTANT */
    char        *string_value; /* column name when SNT_COLUMN */

};

typedef struct {
    const char*      pszName;
    swq_op           eOperation;
    swq_op_evaluator pfnEvaluator;
    swq_op_checker   pfnChecker;
} swq_operation;

class swq_op_registrar {
public:
    static const swq_operation *GetOperator( const char * );
    static const swq_operation *GetOperator( swq_op eOperation );
};

typedef struct {
    char       *data_source;
    char       *table_name;
    char       *table_alias;
} swq_table_def;

class swq_field_list {
public:
    int count;
    char **names;
    swq_field_type *types;
    int *table_ids;
    int *ids;

    int table_count;
    swq_table_def *table_defs;
};

class swq_parse_context {
public:
    swq_parse_context() : nStartToken(0), poRoot(NULL), poCurSelect(NULL) {}

    int        nStartToken;
    const char *pszInput;
    const char *pszNext;
    const char *pszLastValid;

    swq_expr_node *poRoot;

    swq_select    *poCurSelect;
};

/* Compile an SQL WHERE clause into an internal form.  The field_list is
** the list of fields in the target 'table', used to render where into 
** field numbers instead of names. 
*/
int swqparse( swq_parse_context *context );
int swqlex( swq_expr_node **ppNode, swq_parse_context *context );
void swqerror( swq_parse_context *context, const char *msg );

int swq_identify_field( const char* table_name,
                        const char *token, swq_field_list *field_list,
                        swq_field_type *this_type, int *table_id );

CPLErr swq_expr_compile( const char *where_clause, 
                         int field_count,
                         char **field_list,
                         swq_field_type *field_types,
                         swq_expr_node **expr_root );

CPLErr swq_expr_compile2( const char *where_clause, 
                          swq_field_list *field_list, 
                          swq_expr_node **expr_root );

/*
** Evaluation related.
*/
int swq_test_like( const char *input, const char *pattern );

swq_expr_node *SWQGeneralEvaluator( swq_expr_node *, swq_expr_node **);
swq_field_type SWQGeneralChecker( swq_expr_node *node, int bAllowMismatchTypeOnFieldComparison );
swq_expr_node *SWQCastEvaluator( swq_expr_node *, swq_expr_node **);
swq_field_type SWQCastChecker( swq_expr_node *node, int bAllowMismatchTypeOnFieldComparison );
const char*    SWQFieldTypeToString( swq_field_type field_type );

/****************************************************************************/

#define SWQP_ALLOW_UNDEFINED_COL_FUNCS 0x01

#define SWQM_SUMMARY_RECORD  1
#define SWQM_RECORDSET       2
#define SWQM_DISTINCT_LIST   3

typedef enum {
    SWQCF_NONE = 0,
    SWQCF_AVG = SWQ_AVG,
    SWQCF_MIN = SWQ_MIN,
    SWQCF_MAX = SWQ_MAX,
    SWQCF_COUNT = SWQ_COUNT,
    SWQCF_SUM = SWQ_SUM,
    SWQCF_CUSTOM
} swq_col_func;

typedef struct {
    swq_col_func col_func;
    char         *table_name;
    char         *field_name;
    char         *field_alias;
    int          table_index;
    int          field_index;
    swq_field_type field_type;
    swq_field_type target_type;
    OGRFieldSubType target_subtype;
    int          field_length;
    int          field_precision;
    int          distinct_flag;
    OGRwkbGeometryType eGeomType;
    int          nSRID;
    swq_expr_node *expr;
} swq_col_def;

typedef struct {
    GIntBig     count;
    
    char        **distinct_list; /* items of the list can be NULL */
    double      sum;
    double      min;
    double      max;
    char        szMin[32];
    char        szMax[32];
} swq_summary;

typedef struct {
    char *table_name;
    char *field_name;
    int   table_index;
    int   field_index;
    int   ascending_flag;
} swq_order_def;

typedef struct {
    int        secondary_table;
    swq_expr_node  *poExpr;
} swq_join_def;

class swq_select
{
public:
    swq_select();
    ~swq_select();

    int         query_mode;

    char        *raw_select;

    int         PushField( swq_expr_node *poExpr, const char *pszAlias=NULL,
                           int distinct_flag = FALSE );
    int         result_columns;
    swq_col_def *column_defs;
    swq_summary *column_summary;

    int         PushTableDef( const char *pszDataSource,
                              const char *pszTableName,
                              const char *pszAlias );
    int         table_count;
    swq_table_def *table_defs;

    void        PushJoin( int iSecondaryTable, swq_expr_node* poExpr );
    int         join_count;
    swq_join_def *join_defs;

    swq_expr_node *where_expr;

    void        PushOrderBy( const char* pszTableName, const char *pszFieldName, int bAscending );
    int         order_specs;
    swq_order_def *order_defs;

    swq_select *poOtherSelect;
    void        PushUnionAll( swq_select* poOtherSelectIn );

    CPLErr      preparse( const char *select_statement );
    void        postpreparse();
    CPLErr      expand_wildcard( swq_field_list *field_list );
    CPLErr      parse( swq_field_list *field_list, int parse_flags );

    void        Dump( FILE * );
};

CPLErr swq_select_parse( swq_select *select_info,
                         swq_field_list *field_list,
                         int parse_flags );

const char *swq_select_finish_summarize( swq_select *select_info );
const char *swq_select_summarize( swq_select *select_info, 
                                  int dest_column, 
                                  const char *value );

int swq_is_reserved_keyword(const char* pszStr);

char* OGRHStoreGetValue(const char* pszHStore, const char* pszSearchedKey);

#endif /* def _SWQ_H_INCLUDED_ */
