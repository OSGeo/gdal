/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: swq_select class implementation.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "swq.h"
#include "swq_parser.hpp"
#include "ogr_geometry.h"

/************************************************************************/
/*                             swq_select()                             */
/************************************************************************/

swq_select::swq_select() :
    query_mode(0),
    raw_select(NULL),
    result_columns(0),
    column_defs(NULL),
    column_summary(NULL),
    table_count(0),
    table_defs(NULL),
    join_count(0),
    join_defs(NULL),
    where_expr(NULL),
    order_specs(0),
    order_defs(NULL),
    poOtherSelect(NULL)
{ }

/************************************************************************/
/*                            ~swq_select()                             */
/************************************************************************/

swq_select::~swq_select()

{
    delete where_expr;
    CPLFree( raw_select );

    for( int i = 0; i < table_count; i++ )
    {
        swq_table_def *table_def = table_defs + i;

        CPLFree( table_def->data_source );
        CPLFree( table_def->table_name );
        CPLFree( table_def->table_alias );
    }
    if( table_defs != NULL )
        CPLFree( table_defs );

    for( int i = 0; i < result_columns; i++ )
    {
        CPLFree( column_defs[i].table_name );
        CPLFree( column_defs[i].field_name );
        CPLFree( column_defs[i].field_alias );

        delete column_defs[i].expr;

        if( column_summary != NULL
            && column_summary[i].distinct_list != NULL )
        {
            for( int j = 0; j < column_summary[i].count; j++ )
                CPLFree( column_summary[i].distinct_list[j] );

            CPLFree( column_summary[i].distinct_list );
        }
    }

    CPLFree( column_defs );

    CPLFree( column_summary );

    for( int i = 0; i < order_specs; i++ )
    {
        CPLFree( order_defs[i].table_name );
        CPLFree( order_defs[i].field_name );
    }

    CPLFree( order_defs );

    for( int i = 0; i < join_count; i++ )
    {
        delete join_defs[i].poExpr;
    }
    CPLFree( join_defs );

    delete poOtherSelect;
}

/************************************************************************/
/*                              preparse()                              */
/*                                                                      */
/*      Parse the expression but without knowing the available          */
/*      tables and fields.                                              */
/************************************************************************/

CPLErr swq_select::preparse( const char *select_statement,
                             int bAcceptCustomFuncs )

{
/* -------------------------------------------------------------------- */
/*      Prepare a parser context.                                       */
/* -------------------------------------------------------------------- */
    swq_parse_context context;

    context.pszInput = select_statement;
    context.pszNext = select_statement;
    context.pszLastValid = select_statement;
    context.nStartToken = SWQT_SELECT_START;
    context.bAcceptCustomFuncs = bAcceptCustomFuncs;
    context.poCurSelect = this;

/* -------------------------------------------------------------------- */
/*      Do the parse.                                                   */
/* -------------------------------------------------------------------- */
    if( swqparse( &context ) != 0 )
    {
        delete context.poRoot;
        return CE_Failure;
    }

    postpreparse();

    return CE_None;
}

/************************************************************************/
/*                          postpreparse()                              */
/************************************************************************/

void swq_select::postpreparse()
{
/* -------------------------------------------------------------------- */
/*      Reorder the joins in the order they appear in the SQL string.   */
/* -------------------------------------------------------------------- */
    for(int i = 0; i < join_count / 2; i++)
    {
        swq_join_def sTmp;
        memcpy(&sTmp, &join_defs[i], sizeof(swq_join_def));
        memcpy(&join_defs[i], &join_defs[join_count - 1 - i], sizeof(swq_join_def));
        memcpy(&join_defs[join_count - 1 - i], &sTmp, sizeof(swq_join_def));
    }

    /* We make that strong assumption in ogr_gensql */
    for(int i = 0; i < join_count; i++)
    {
        CPLAssert(join_defs[i].secondary_table == i + 1);
    }

    if( poOtherSelect != NULL)
        poOtherSelect->postpreparse();
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void swq_select::Dump( FILE *fp )

{
    fprintf( fp, "SELECT Statement:\n" );

/* -------------------------------------------------------------------- */
/*      query mode.                                                     */
/* -------------------------------------------------------------------- */
    if( query_mode == SWQM_SUMMARY_RECORD )
        fprintf( fp, "  QUERY MODE: SUMMARY RECORD\n" );
    else if( query_mode == SWQM_RECORDSET )
        fprintf( fp, "  QUERY MODE: RECORDSET\n" );
    else if( query_mode == SWQM_DISTINCT_LIST )
        fprintf( fp, "  QUERY MODE: DISTINCT LIST\n" );
    else
        fprintf( fp, "  QUERY MODE: %d/unknown\n", query_mode );

/* -------------------------------------------------------------------- */
/*      column_defs                                                     */
/* -------------------------------------------------------------------- */
    fprintf( fp, "  Result Columns:\n" );
    for( int i = 0; i < result_columns; i++ )
    {
        swq_col_def *def = column_defs + i;

        fprintf( fp, "  Table name: %s\n", def->table_name );
        fprintf( fp, "  Name: %s\n", def->field_name );

        if( def->field_alias )
            fprintf( fp, "    Alias: %s\n", def->field_alias );

        if( def->col_func == SWQCF_NONE )
            /* nothing */;
        else if( def->col_func == SWQCF_AVG )
            fprintf( fp, "    Function: AVG\n" );
        else if( def->col_func == SWQCF_MIN )
            fprintf( fp, "    Function: MIN\n" );
        else if( def->col_func == SWQCF_MAX )
            fprintf( fp, "    Function: MAX\n" );
        else if( def->col_func == SWQCF_COUNT )
            fprintf( fp, "    Function: COUNT\n" );
        else if( def->col_func == SWQCF_SUM )
            fprintf( fp, "    Function: SUM\n" ); 
        else if( def->col_func == SWQCF_CUSTOM )
            fprintf( fp, "    Function: CUSTOM\n" );
        else
            fprintf( fp, "    Function: UNKNOWN!\n" );

        if( def->distinct_flag )
            fprintf( fp, "    DISTINCT flag set\n" );

        fprintf( fp, "    Field Index: %d, Table Index: %d\n", 
                 def->field_index, def->table_index );

        fprintf( fp, "    Field Type: %d\n", def->field_type );
        fprintf( fp, "    Target Type: %d\n", def->target_type );
        fprintf( fp, "    Target SubType: %d\n", def->target_subtype );
        fprintf( fp, "    Length: %d, Precision: %d\n",
                 def->field_length, def->field_precision );

        if( def->expr != NULL )
        {
            fprintf( fp, "    Expression:\n" );
            def->expr->Dump( fp, 3 );
        }
    }

/* -------------------------------------------------------------------- */
/*      table_defs                                                      */
/* -------------------------------------------------------------------- */
    fprintf( fp, "  Table Defs: %d\n", table_count );
    for( int i = 0; i < table_count; i++ )
    {
        fprintf( fp, "    datasource=%s, table_name=%s, table_alias=%s\n",
                 table_defs[i].data_source,
                 table_defs[i].table_name,
                 table_defs[i].table_alias );
    }

/* -------------------------------------------------------------------- */
/*      join_defs                                                       */
/* -------------------------------------------------------------------- */
    if( join_count > 0 )
        fprintf( fp, "  joins:\n" );

    for( int i = 0; i < join_count; i++ )
    {
        fprintf( fp, "  %d:\n", i );
        join_defs[i].poExpr->Dump( fp, 4 );
        fprintf( fp, "    Secondary Table: %d\n", 
                 join_defs[i].secondary_table );
    }

/* -------------------------------------------------------------------- */
/*      Where clause.                                                   */
/* -------------------------------------------------------------------- */
    if( where_expr != NULL )
    {
        fprintf( fp, "  WHERE:\n" );
        where_expr->Dump( fp, 2 );
    }

/* -------------------------------------------------------------------- */
/*      Order by                                                        */
/* -------------------------------------------------------------------- */

    for( int i = 0; i < order_specs; i++ )
    {
        fprintf( fp, "  ORDER BY: %s (%d/%d)",
                 order_defs[i].field_name,
                 order_defs[i].table_index,
                 order_defs[i].field_index );
        if( order_defs[i].ascending_flag )
            fprintf( fp, " ASC\n" );
        else
            fprintf( fp, " DESC\n" );
    }
}

/************************************************************************/
/*                               Unparse()                              */
/************************************************************************/

char* swq_select::Unparse()
{
    CPLString osSelect("SELECT ");
    if( query_mode == SWQM_DISTINCT_LIST )
        osSelect += "DISTINCT ";

    for( int i = 0; i < result_columns; i++ )
    {
        swq_col_def *def = column_defs + i;

        if( i > 0 )
            osSelect += ", ";

        if( def->expr != NULL && def->col_func == SWQCF_NONE )
        {
            char* pszTmp = def->expr->Unparse(NULL, '"');
            osSelect += pszTmp;
            CPLFree(pszTmp);
        }
        else
        {
            if( def->col_func == SWQCF_AVG )
                osSelect += "AVG(";
            else if( def->col_func == SWQCF_MIN )
                osSelect += "MIN(";
            else if( def->col_func == SWQCF_MAX )
                osSelect += "MAX(";
            else if( def->col_func == SWQCF_COUNT )
                osSelect += "COUNT(";
            else if( def->col_func == SWQCF_SUM )
                osSelect += "SUM(";

            if( def->distinct_flag && def->col_func == SWQCF_COUNT )
                osSelect += "DISTINCT ";

            if( (def->field_alias == NULL || table_count > 1) &&
                def->table_name != NULL && def->table_name[0] != '\0' )
            {
                osSelect += swq_expr_node::QuoteIfNecessary(def->table_name, '"');
                osSelect += ".";
            }
            osSelect += swq_expr_node::QuoteIfNecessary(def->field_name, '"');
        }

        if( def->field_alias != NULL &&
            strcmp(def->field_name, def->field_alias) != 0 )
        {
            osSelect += " AS ";
            osSelect += swq_expr_node::QuoteIfNecessary(def->field_alias, '"');
        }

        if( def->col_func != SWQCF_NONE )
            osSelect += ")";
    }

    osSelect += " FROM ";
    if( table_defs[0].data_source != NULL )
    {
        osSelect += "'";
        osSelect += table_defs[0].data_source;
        osSelect += "'.";
    }
    osSelect += swq_expr_node::QuoteIfNecessary(table_defs[0].table_name, '"');
    if( table_defs[0].table_alias != NULL &&
        strcmp(table_defs[0].table_name, table_defs[0].table_alias) != 0 )
    {
        osSelect += " AS ";
        osSelect += swq_expr_node::QuoteIfNecessary(table_defs[0].table_alias, '"');
    }

    for( int i = 0; i < join_count; i++ )
    {
        int iTable = join_defs[i].secondary_table;
        osSelect += " JOIN ";
        if( table_defs[iTable].data_source != NULL )
        {
            osSelect += "'";
            osSelect += table_defs[iTable].data_source;
            osSelect += "'.";
        }
        osSelect += swq_expr_node::QuoteIfNecessary(table_defs[iTable].table_name, '"');
        if( table_defs[iTable].table_alias != NULL &&
            strcmp(table_defs[iTable].table_name, table_defs[iTable].table_alias) != 0 )
        {
            osSelect += " AS ";
            osSelect += swq_expr_node::QuoteIfNecessary(table_defs[iTable].table_alias, '"');
        }
        osSelect += " ON ";
        char* pszTmp = join_defs[i].poExpr->Unparse(NULL, '"');
        osSelect += pszTmp;
        CPLFree(pszTmp);
    }

    if( where_expr != NULL )
    {
        osSelect += " WHERE ";
        char* pszTmp = where_expr->Unparse(NULL, '"');
        osSelect += pszTmp;
        CPLFree(pszTmp);
    }

    for( int i = 0; i < order_specs; i++ )
    {
        osSelect += " ORDER BY ";
        osSelect += swq_expr_node::QuoteIfNecessary(order_defs[i].field_name, '"');
        if( !order_defs[i].ascending_flag )
            osSelect += " DESC";
    }

    return CPLStrdup(osSelect);
}

/************************************************************************/
/*                             PushField()                              */
/*                                                                      */
/*      Create a new field definition by name and possibly alias.       */
/************************************************************************/

int swq_select::PushField( swq_expr_node *poExpr, const char *pszAlias,
                           int distinct_flag )

{
    if( query_mode == SWQM_DISTINCT_LIST && distinct_flag )
    {
        CPLError(CE_Failure, CPLE_NotSupported, "SELECT DISTINCT and COUNT(DISTINCT...) not supported together");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Grow the array.                                                 */
/* -------------------------------------------------------------------- */
    result_columns++;

    column_defs = (swq_col_def *) 
        CPLRealloc( column_defs, sizeof(swq_col_def) * result_columns );

    swq_col_def *col_def = column_defs + result_columns - 1;

    memset( col_def, 0, sizeof(swq_col_def) );

/* -------------------------------------------------------------------- */
/*      Try to capture a field name.                                    */
/* -------------------------------------------------------------------- */
    if( poExpr->eNodeType == SNT_COLUMN )
    {
        col_def->table_name = 
            CPLStrdup(poExpr->table_name ? poExpr->table_name : "");
        col_def->field_name = 
            CPLStrdup(poExpr->string_value);
    }
    else if( poExpr->eNodeType == SNT_OPERATION
             && (poExpr->nOperation == SWQ_CAST ||
                 (poExpr->nOperation >= SWQ_AVG &&
                  poExpr->nOperation <= SWQ_SUM))
             && poExpr->nSubExprCount >= 1
             && poExpr->papoSubExpr[0]->eNodeType == SNT_COLUMN )
    {
        col_def->table_name = 
            CPLStrdup(poExpr->papoSubExpr[0]->table_name ?
                        poExpr->papoSubExpr[0]->table_name : "");
        col_def->field_name = 
            CPLStrdup(poExpr->papoSubExpr[0]->string_value);
    }
    else
    {
        col_def->table_name = CPLStrdup("");
        col_def->field_name = CPLStrdup("");
    }

/* -------------------------------------------------------------------- */
/*      Initialize fields.                                              */
/* -------------------------------------------------------------------- */
    if( pszAlias != NULL )
        col_def->field_alias = CPLStrdup( pszAlias );
    else if( pszAlias == NULL && poExpr->eNodeType == SNT_OPERATION
             && poExpr->nSubExprCount >= 1
             && ( static_cast<swq_op>(poExpr->nOperation) == SWQ_CONCAT ||
                  static_cast<swq_op>(poExpr->nOperation) == SWQ_SUBSTR )
             && poExpr->papoSubExpr[0]->eNodeType == SNT_COLUMN )
    {
        const swq_operation *op = swq_op_registrar::GetOperator(
            static_cast<swq_op>(poExpr->nOperation) );

        col_def->field_alias = CPLStrdup( CPLSPrintf("%s_%s", op->pszName, 
                                    poExpr->papoSubExpr[0]->string_value));
    }

    col_def->table_index = -1;
    col_def->field_index = -1;
    col_def->field_type = SWQ_OTHER;
    col_def->field_precision = -1;
    col_def->target_type = SWQ_OTHER;
    col_def->target_subtype = OFSTNone;
    col_def->col_func = SWQCF_NONE;
    col_def->distinct_flag = distinct_flag;

/* -------------------------------------------------------------------- */
/*      Do we have a CAST operator in play?                             */
/* -------------------------------------------------------------------- */
    if( poExpr->eNodeType == SNT_OPERATION 
        && poExpr->nOperation == SWQ_CAST )
    {
        const char *pszTypeName = poExpr->papoSubExpr[1]->string_value;
        int parse_precision = 0;

        if( EQUAL(pszTypeName,"character") )
        {
            col_def->target_type = SWQ_STRING;
            col_def->field_length = 1;
        }
        else if( strcasecmp(pszTypeName,"boolean") == 0 )
        {
            col_def->target_type = SWQ_BOOLEAN;
        }
        else if( strcasecmp(pszTypeName,"integer") == 0 )
        {
            col_def->target_type = SWQ_INTEGER;
        }
        else if( strcasecmp(pszTypeName,"bigint") == 0 )
        {
            col_def->target_type = SWQ_INTEGER64;
        }
        else if( strcasecmp(pszTypeName,"smallint") == 0 )
        {
            col_def->target_type = SWQ_INTEGER;
            col_def->target_subtype = OFSTInt16;
        }
        else if( strcasecmp(pszTypeName,"float") == 0 )
        {
            col_def->target_type = SWQ_FLOAT;
        }
        else if( strcasecmp(pszTypeName,"numeric") == 0 )
        {
            col_def->target_type = SWQ_FLOAT;
            parse_precision = 1;
        }
        else if( strcasecmp(pszTypeName,"timestamp") == 0 )
        {
            col_def->target_type = SWQ_TIMESTAMP;
        }
        else if( strcasecmp(pszTypeName,"date") == 0 )
        {
            col_def->target_type = SWQ_DATE;
        }
        else if( strcasecmp(pszTypeName,"time") == 0 )
        {
            col_def->target_type = SWQ_TIME;
        }
        else if( strcasecmp(pszTypeName,"geometry") == 0 )
        {
            col_def->target_type = SWQ_GEOMETRY;
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unrecognized typename %s in CAST operator.", 
                      pszTypeName );
            CPLFree(col_def->table_name);
            col_def->table_name = NULL;
            CPLFree(col_def->field_name);
            col_def->field_name = NULL;
            CPLFree(col_def->field_alias);
            col_def->field_alias = NULL;
            result_columns--;
            return FALSE;
        }

        if( col_def->target_type == SWQ_GEOMETRY ) 
        {
            if( poExpr->nSubExprCount > 2 )
            {
                if( poExpr->papoSubExpr[2]->field_type != SWQ_STRING )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                      "First argument of CAST operator should be an geometry type identifier." );
                    CPLFree(col_def->table_name);
                    col_def->table_name = NULL;
                    CPLFree(col_def->field_name);
                    col_def->field_name = NULL;
                    CPLFree(col_def->field_alias);
                    col_def->field_alias = NULL;
                    result_columns--;
                    return FALSE;
                }

                col_def->eGeomType =
                    OGRFromOGCGeomType(poExpr->papoSubExpr[2]->string_value);

                // SRID
                if( poExpr->nSubExprCount > 3 )
                {
                    col_def->nSRID = (int)poExpr->papoSubExpr[3]->int_value;
                }
            }
        }
        else
        {
            // field width.
            if( poExpr->nSubExprCount > 2 )
            {
                if( poExpr->papoSubExpr[2]->field_type != SWQ_INTEGER )
                {
                    CPLError( CE_Failure, CPLE_AppDefined,
                      "First argument of CAST operator should be of integer type." );
                    CPLFree(col_def->table_name);
                    col_def->table_name = NULL;
                    CPLFree(col_def->field_name);
                    col_def->field_name = NULL;
                    CPLFree(col_def->field_alias);
                    col_def->field_alias = NULL;
                    result_columns--;
                    return FALSE;
                }
                col_def->field_length = (int)poExpr->papoSubExpr[2]->int_value;
            }

            // field width.
            if( poExpr->nSubExprCount > 3 && parse_precision )
            {
                col_def->field_precision = (int)poExpr->papoSubExpr[3]->int_value;
                if( col_def->field_precision == 0 )
                {
                    if( col_def->field_length < 10 )
                        col_def->target_type = SWQ_INTEGER;
                    else if( col_def->field_length < 19 )
                        col_def->target_type = SWQ_INTEGER64;
                }
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a special column function in play?                   */
/* -------------------------------------------------------------------- */
    if( poExpr->eNodeType == SNT_OPERATION
        && static_cast<swq_op>(poExpr->nOperation) >= SWQ_AVG
        && static_cast<swq_op>(poExpr->nOperation) <= SWQ_SUM )
    {
        if( poExpr->nSubExprCount != 1 )
        {
            const swq_operation *poOp =
                    swq_op_registrar::GetOperator( (swq_op)poExpr->nOperation );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Column Summary Function '%s' has wrong number of arguments.", 
                      poOp->pszName );
            CPLFree(col_def->table_name);
            col_def->table_name = NULL;
            CPLFree(col_def->field_name);
            col_def->field_name = NULL;
            CPLFree(col_def->field_alias);
            col_def->field_alias = NULL;
            result_columns--;
            return FALSE;
        }
        else if( poExpr->papoSubExpr[0]->eNodeType != SNT_COLUMN )
        {
            const swq_operation *poOp = 
                    swq_op_registrar::GetOperator( (swq_op)poExpr->nOperation );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Argument of column Summary Function '%s' should be a column.", 
                      poOp->pszName );
            CPLFree(col_def->table_name);
            col_def->table_name = NULL;
            CPLFree(col_def->field_name);
            col_def->field_name = NULL;
            CPLFree(col_def->field_alias);
            col_def->field_alias = NULL;
            result_columns--;
            return FALSE;
        }
        else
        {
            col_def->col_func = 
                (swq_col_func) poExpr->nOperation;

            swq_expr_node *poSubExpr = poExpr->papoSubExpr[0];

            poExpr->papoSubExpr[0] = NULL;
            poExpr->nSubExprCount = 0;
            delete poExpr;

            poExpr = poSubExpr;
        }
    }

    col_def->expr = poExpr;

    return TRUE;
}

/************************************************************************/
/*                            PushTableDef()                            */
/************************************************************************/

int swq_select::PushTableDef( const char *pszDataSource,
                              const char *pszName,
                              const char *pszAlias )

{
    table_count++;

    table_defs = (swq_table_def *) 
        CPLRealloc( table_defs, sizeof(swq_table_def) * table_count );

    if( pszDataSource != NULL )
        table_defs[table_count-1].data_source = CPLStrdup(pszDataSource);
    else
        table_defs[table_count-1].data_source = NULL;

    table_defs[table_count-1].table_name = CPLStrdup(pszName);

    if( pszAlias != NULL )
        table_defs[table_count-1].table_alias = CPLStrdup(pszAlias);
    else
        table_defs[table_count-1].table_alias = CPLStrdup(pszName);

    return table_count-1;
}

/************************************************************************/
/*                            PushOrderBy()                             */
/************************************************************************/

void swq_select::PushOrderBy( const char* pszTableName, const char *pszFieldName, int bAscending )

{
    order_specs++;
    order_defs = (swq_order_def *) 
        CPLRealloc( order_defs, sizeof(swq_order_def) * order_specs );

    order_defs[order_specs-1].table_name = CPLStrdup(pszTableName ? pszTableName : "");
    order_defs[order_specs-1].field_name = CPLStrdup(pszFieldName);
    order_defs[order_specs-1].table_index = -1;
    order_defs[order_specs-1].field_index = -1;
    order_defs[order_specs-1].ascending_flag = bAscending;
}

/************************************************************************/
/*                              PushJoin()                              */
/************************************************************************/

void swq_select::PushJoin( int iSecondaryTable, swq_expr_node* poExpr )

{
    join_count++;
    join_defs = (swq_join_def *) 
        CPLRealloc( join_defs, sizeof(swq_join_def) * join_count );

    join_defs[join_count-1].secondary_table = iSecondaryTable;
    join_defs[join_count-1].poExpr = poExpr;
}

/************************************************************************/
/*                             PushUnionAll()                           */
/************************************************************************/

void swq_select::PushUnionAll( swq_select* poOtherSelectIn )
{
    CPLAssert(poOtherSelect == NULL);
    poOtherSelect = poOtherSelectIn;
}

/************************************************************************/
/*                          expand_wildcard()                           */
/*                                                                      */
/*      This function replaces the '*' in a "SELECT *" with the list    */
/*      provided list of fields.  Itis used by swq_select_parse(),      */
/*      but may be called in advance by applications wanting the        */
/*      "default" field list to be different than the full list of      */
/*      fields.                                                         */
/************************************************************************/

CPLErr swq_select::expand_wildcard( swq_field_list *field_list,
                                    int bAlwaysPrefixWithTableName )

{
/* ==================================================================== */
/*      Check each pre-expansion field.                                 */
/* ==================================================================== */
    for( int isrc = 0; isrc < result_columns; isrc++ )
    {
        const char *src_tablename = column_defs[isrc].table_name;
        const char *src_fieldname = column_defs[isrc].field_name;
        int itable, new_fields, iout;

        if( *src_fieldname == '\0'
            || src_fieldname[strlen(src_fieldname)-1] != '*' )
            continue;

        /* We don't want to expand COUNT(*) */
        if( column_defs[isrc].col_func == SWQCF_COUNT )
            continue;

/* -------------------------------------------------------------------- */
/*      Parse out the table name, verify it, and establish the          */
/*      number of fields to insert from it.                             */
/* -------------------------------------------------------------------- */
        if( src_tablename[0] == 0 && strcmp(src_fieldname,"*") == 0 )
        {
            itable = -1;
            new_fields = field_list->count;
        }
        else
        {
            for( itable = 0; itable < field_list->table_count; itable++ )
            {
                if( strcasecmp(src_tablename,
                        field_list->table_defs[itable].table_alias ) == 0 )
                    break;
            }

            if( itable == field_list->table_count )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                         "Table %s not recognised from %s.%s definition.", 
                         src_tablename, src_tablename, src_fieldname );
                return CE_Failure;
            }

            /* count the number of fields in this table. */
            new_fields = 0;
            for( int i = 0; i < field_list->count; i++ )
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
            CPLFree( column_defs[isrc].table_name );
            CPLFree( column_defs[isrc].field_name );
            delete column_defs[isrc].expr;

            column_defs = (swq_col_def *) 
                CPLRealloc( column_defs, 
                            sizeof(swq_col_def) * 
                            (result_columns + new_fields - 1 ) );

/* -------------------------------------------------------------------- */
/*      Push the old definitions that came after the one to be          */
/*      replaced further up in the array.                               */
/* -------------------------------------------------------------------- */
            if (new_fields != 1)
            {
                for( int i = result_columns-1; i > isrc; i-- )
                {
                    memcpy( column_defs + i + new_fields - 1,
                            column_defs + i,
                            sizeof( swq_col_def ) );
                }
            }

            result_columns += (new_fields - 1 );

/* -------------------------------------------------------------------- */
/*      Zero out all the stuff in the target column definitions.        */
/* -------------------------------------------------------------------- */
            memset( column_defs + isrc, 0, 
                    new_fields * sizeof(swq_col_def) );
        }
        else
        {
/* -------------------------------------------------------------------- */
/*      The wildcard expands to nothing                                 */
/* -------------------------------------------------------------------- */
            CPLFree( column_defs[isrc].table_name );
            CPLFree( column_defs[isrc].field_name );
            delete column_defs[isrc].expr;

            memmove( column_defs + isrc,
                     column_defs + isrc + 1,
                     sizeof( swq_col_def ) * (result_columns-1-isrc) );

            result_columns --;
        }

/* -------------------------------------------------------------------- */
/*      Assign the selected fields.                                     */
/* -------------------------------------------------------------------- */
        iout = isrc;

        for( int i = 0; i < field_list->count; i++ )
        {
            swq_col_def *def;
            int compose = (itable != -1) || bAlwaysPrefixWithTableName;

            /* skip this field if it isn't in the target table.  */
            if( itable != -1 && itable != field_list->table_ids[i] )
                continue;

            /* set up some default values. */
            def = column_defs + iout;
            def->field_precision = -1; 
            def->target_type = SWQ_OTHER;
            def->target_subtype = OFSTNone;

            /* does this field duplicate an earlier one? */
            if( field_list->table_ids[i] != 0 
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

            int itable = field_list->table_ids[i];
            const char *field_name = field_list->names[i];
            const char *table_alias = 
                field_list->table_defs[itable].table_alias;

            def->table_name = CPLStrdup(table_alias);
            def->field_name = CPLStrdup(field_name);
            if( !compose )
                def->field_alias = CPLStrdup( field_list->names[i] );

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

    return CE_None;
}

/************************************************************************/
/*                       CheckCompatibleJoinExpr()                      */
/************************************************************************/

static bool CheckCompatibleJoinExpr( swq_expr_node* poExpr,
                                    int secondary_table,
                                    swq_field_list* field_list )
{
    if( poExpr->eNodeType == SNT_CONSTANT )
        return true;

    if( poExpr->eNodeType == SNT_COLUMN )
    {
        CPLAssert( poExpr->field_index != -1 );
        CPLAssert( poExpr->table_index != -1 );
        if( poExpr->table_index != 0 && poExpr->table_index != secondary_table )
        {
            if( poExpr->table_name )
                CPLError( CE_Failure, CPLE_AppDefined, 
                        "Field %s.%s in JOIN clause does not correspond to the primary table nor the joint (secondary) table.", 
                        poExpr->table_name,
                        poExpr->string_value );
            else
                CPLError( CE_Failure, CPLE_AppDefined, 
                        "Field %s in JOIN clause does not correspond to the primary table nor the joint (secondary) table.", 
                        poExpr->string_value );
            return false;
        }

        return true;
    }

    if( poExpr->eNodeType == SNT_OPERATION )
    {
        for(int i=0;i<poExpr->nSubExprCount;i++)
        {
            if( !CheckCompatibleJoinExpr( poExpr->papoSubExpr[i],
                                          secondary_table,
                                          field_list ) )
                return false;
        }
        return true;
    }

    return false;
}

/************************************************************************/
/*                               parse()                                */
/*                                                                      */
/*      This method really does post-parse processing.                  */
/************************************************************************/

CPLErr swq_select::parse( swq_field_list *field_list,
                          swq_select_parse_options* poParseOptions )
{
    int bAlwaysPrefixWithTableName = poParseOptions &&
                                     poParseOptions->bAlwaysPrefixWithTableName;
    CPLErr eError = expand_wildcard( field_list, bAlwaysPrefixWithTableName );
    if( eError != CE_None )
        return eError;

    swq_custom_func_registrar* poCustomFuncRegistrar = NULL;
    if( poParseOptions != NULL )
        poCustomFuncRegistrar = poParseOptions->poCustomFuncRegistrar;

/* -------------------------------------------------------------------- */
/*      Identify field information.                                     */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < result_columns; i++ )
    {
        swq_col_def *def = column_defs + i;

        if( def->expr != NULL && def->expr->eNodeType != SNT_COLUMN )
        {
            def->field_index = -1;
            def->table_index = -1;

            if( def->expr->Check( field_list, TRUE, FALSE, poCustomFuncRegistrar ) == SWQ_ERROR )
                return CE_Failure;

            def->field_type = def->expr->field_type;
        }
        else
        {
            swq_field_type  this_type;

            /* identify field */
            def->field_index = swq_identify_field( def->table_name,
                                                   def->field_name, field_list,
                                                   &this_type, 
                                                   &(def->table_index) );

            /* record field type */
            def->field_type = this_type;

            if( def->field_index == -1 && def->col_func != SWQCF_COUNT )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unrecognised field name %s.", 
                          def->table_name[0] ? CPLSPrintf("%s.%s", def->table_name, def->field_name) : def->field_name );
                return CE_Failure;
            }
        }

        /* identify column function if present */
        if( (def->col_func == SWQCF_MIN 
             || def->col_func == SWQCF_MAX
             || def->col_func == SWQCF_AVG
             || def->col_func == SWQCF_SUM)
            && (def->field_type == SWQ_STRING ||
                def->field_type == SWQ_GEOMETRY) )
        {
            // possibly this is already enforced by the checker?
            const swq_operation *op = swq_op_registrar::GetOperator( 
                (swq_op) def->col_func );
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Use of field function %s() on %s field %s illegal.", 
                      op->pszName,
                      SWQFieldTypeToString(def->field_type),
                      def->field_name );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if we are producing a one row summary result or a set     */
/*      of records.  Generate an error if we get conflicting            */
/*      indications.                                                    */
/* -------------------------------------------------------------------- */

    int bAllowDistinctOnMultipleFields = (
        poParseOptions && poParseOptions->bAllowDistinctOnMultipleFields );
    if( query_mode == SWQM_DISTINCT_LIST && result_columns > 1 &&
        !bAllowDistinctOnMultipleFields )
    {
            CPLError( CE_Failure, CPLE_NotSupported,
                        "SELECT DISTINCT not supported on multiple columns." );
            return CE_Failure;
    }

    for( int i = 0; i < result_columns; i++ )
    {
        swq_col_def *def = column_defs + i;
        int this_indicator = -1;

        if( query_mode == SWQM_DISTINCT_LIST && def->field_type == SWQ_GEOMETRY )
        {
            int bAllowDistinctOnGeometryField = (
                    poParseOptions && poParseOptions->bAllowDistinctOnGeometryField );
            if( !bAllowDistinctOnGeometryField )
            {
                CPLError( CE_Failure, CPLE_NotSupported,
                            "SELECT DISTINCT on a geometry not supported." );
                return CE_Failure;
            }
        }

        if( def->col_func == SWQCF_MIN 
            || def->col_func == SWQCF_MAX
            || def->col_func == SWQCF_AVG
            || def->col_func == SWQCF_SUM
            || def->col_func == SWQCF_COUNT )
        {
            this_indicator = SWQM_SUMMARY_RECORD;
            if( def->col_func == SWQCF_COUNT &&
                def->distinct_flag &&
                def->field_type == SWQ_GEOMETRY )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                          "SELECT COUNT DISTINCT on a geometry not supported." );
                return CE_Failure;
            }
        }
        else if( def->col_func == SWQCF_NONE )
        {
            if( query_mode == SWQM_DISTINCT_LIST )
            {
                def->distinct_flag = TRUE;
                this_indicator = SWQM_DISTINCT_LIST;
            }
            else
                this_indicator = SWQM_RECORDSET;
        }

        if( this_indicator != query_mode
             && this_indicator != -1
            && query_mode != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Field list implies mixture of regular recordset mode, summary mode or distinct field list mode." );
            return CE_Failure;
        }

        if( this_indicator != -1 )
            query_mode = this_indicator;
    }

    if (result_columns == 0)
    {
        query_mode = SWQM_RECORDSET;
    }

/* -------------------------------------------------------------------- */
/*      Process column names in JOIN specs.                             */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < join_count; i++ )
    {
        swq_join_def *def = join_defs + i;
        if( def->poExpr->Check( field_list, TRUE, TRUE, poCustomFuncRegistrar ) == SWQ_ERROR )
            return CE_Failure;
        if( !CheckCompatibleJoinExpr( def->poExpr, def->secondary_table, field_list ) )
            return CE_Failure;
    }

/* -------------------------------------------------------------------- */
/*      Process column names in order specs.                            */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < order_specs; i++ )
    {
        swq_order_def *def = order_defs + i;

        /* identify field */
        swq_field_type field_type;
        def->field_index = swq_identify_field( def->table_name,
                                               def->field_name, field_list,
                                               &field_type, &(def->table_index) );
        if( def->field_index == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unrecognised field name %s in ORDER BY.", 
                      def->table_name[0] ? CPLSPrintf("%s.%s", def->table_name, def->field_name) : def->field_name );
            return CE_Failure;
        }

        if( def->table_index != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot use field '%s' of a secondary table in a ORDER BY clause",
                      def->field_name );
            return CE_Failure;
        }

        if( field_type == SWQ_GEOMETRY )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot use geometry field '%s' in a ORDER BY clause",
                      def->field_name );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Post process the where clause, subbing in field indexes and     */
/*      doing final validation.                                         */
/* -------------------------------------------------------------------- */
    int bAllowFieldsInSecondaryTablesInWhere = FALSE;
    if( poParseOptions != NULL )
        bAllowFieldsInSecondaryTablesInWhere = poParseOptions->bAllowFieldsInSecondaryTablesInWhere;
    if( where_expr != NULL 
        && where_expr->Check( field_list, bAllowFieldsInSecondaryTablesInWhere, FALSE, poCustomFuncRegistrar ) == SWQ_ERROR )
    {
        return CE_Failure;
    }

    return CE_None;
}
