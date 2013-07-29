/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: swq_select class implementation.
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

#include "swq.h"
#include "swq_parser.hpp"

/************************************************************************/
/*                             swq_select()                             */
/************************************************************************/

swq_select::swq_select()

{
    query_mode = 0;
    raw_select = NULL;

    result_columns = 0;
    column_defs = NULL;
    column_summary = NULL;
    
    table_count = 0;
    table_defs = NULL;
    
    join_count = 0;
    join_defs = NULL;
    
    where_expr = NULL;

    order_specs = 0;
    order_defs = NULL;

    poOtherSelect = NULL;
}

/************************************************************************/
/*                            ~swq_select()                             */
/************************************************************************/

swq_select::~swq_select()

{
    int i;

    delete where_expr;
    CPLFree( raw_select );

    for( i = 0; i < table_count; i++ )
    {
        swq_table_def *table_def = table_defs + i;

        CPLFree( table_def->data_source );
        CPLFree( table_def->table_name );
        CPLFree( table_def->table_alias );
    }
    if( table_defs != NULL )
        CPLFree( table_defs );

    for( i = 0; i < result_columns; i++ )
    {
        CPLFree( column_defs[i].field_name );
        CPLFree( column_defs[i].field_alias );

        delete column_defs[i].expr;

        if( column_summary != NULL 
            && column_summary[i].distinct_list != NULL )
        {
            int j;
            
            for( j = 0; j < column_summary[i].count; j++ )
                CPLFree( column_summary[i].distinct_list[j] );

            CPLFree( column_summary[i].distinct_list );
        }
    }

    CPLFree( column_defs );

    CPLFree( column_summary );

    for( i = 0; i < order_specs; i++ )
    {
        CPLFree( order_defs[i].field_name );
    }
    
    CPLFree( order_defs );

    for( i = 0; i < join_count; i++ )
    {
        CPLFree( join_defs[i].primary_field_name );
        CPLFree( join_defs[i].secondary_field_name );
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

CPLErr swq_select::preparse( const char *select_statement )

{
/* -------------------------------------------------------------------- */
/*      Prepare a parser context.                                       */
/* -------------------------------------------------------------------- */
    swq_parse_context context;

    context.pszInput = select_statement;
    context.pszNext = select_statement;
    context.nStartToken = SWQT_SELECT_START;
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
    int i;
    for(i = 0; i < join_count / 2; i++)
    {
        swq_join_def sTmp;
        memcpy(&sTmp, &join_defs[i], sizeof(swq_join_def));
        memcpy(&join_defs[i], &join_defs[join_count - 1 - i], sizeof(swq_join_def));
        memcpy(&join_defs[join_count - 1 - i], &sTmp, sizeof(swq_join_def));
    }

    /* We make that strong assumption in ogr_gensql */
    for(i = 0; i < join_count; i++)
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
    int i;

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
    for( i = 0; i < result_columns; i++ )
    {
        swq_col_def *def = column_defs + i;

        
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
    for( i = 0; i < table_count; i++ )
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

    for( i = 0; i < join_count; i++ )
    {
        fprintf( fp, "  %d:\n", i );
        fprintf( fp, "    Primary Field: %s/%d\n", 
                 join_defs[i].primary_field_name,
                 join_defs[i].primary_field );

        fprintf( fp, "    Operation: %d\n", 
                 join_defs[i].op );

        fprintf( fp, "    Secondary Field: %s/%d\n", 
                 join_defs[i].secondary_field_name,
                 join_defs[i].secondary_field );
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

    for( i = 0; i < order_specs; i++ )
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
/*                             PushField()                              */
/*                                                                      */
/*      Create a new field definition by name and possibly alias.       */
/************************************************************************/

int swq_select::PushField( swq_expr_node *poExpr, const char *pszAlias,
                           int distinct_flag )

{
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
        col_def->field_name = 
            CPLStrdup(poExpr->string_value);
    else if( poExpr->eNodeType == SNT_OPERATION
             && poExpr->nSubExprCount >= 1
             && poExpr->papoSubExpr[0]->eNodeType == SNT_COLUMN )
        col_def->field_name = 
            CPLStrdup(poExpr->papoSubExpr[0]->string_value);
    else
        col_def->field_name = CPLStrdup("");

/* -------------------------------------------------------------------- */
/*      Initialize fields.                                              */
/* -------------------------------------------------------------------- */
    if( pszAlias != NULL )
        col_def->field_alias = CPLStrdup( pszAlias );

    col_def->table_index = -1;
    col_def->field_index = -1;
    col_def->field_type = SWQ_OTHER;
    col_def->field_precision = -1;
    col_def->target_type = SWQ_OTHER;
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
        else if( strcasecmp(pszTypeName,"integer") == 0 )
        {
            col_def->target_type = SWQ_INTEGER;
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
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unrecognized typename %s in CAST operator.", 
                      pszTypeName );
            CPLFree(col_def->field_name);
            col_def->field_name = NULL;
            CPLFree(col_def->field_alias);
            col_def->field_alias = NULL;
            result_columns--;
            return FALSE;
        }

        // field width.
        if( poExpr->nSubExprCount > 2 )
        {
            col_def->field_length = poExpr->papoSubExpr[2]->int_value;
        }

        // field width.
        if( poExpr->nSubExprCount > 3 && parse_precision )
        {
            col_def->field_precision = poExpr->papoSubExpr[3]->int_value;
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we have a special column function in play?                   */
/* -------------------------------------------------------------------- */
    if( poExpr->eNodeType == SNT_OPERATION 
        && poExpr->nOperation >= SWQ_AVG
        && poExpr->nOperation <= SWQ_SUM )
    {
        if( poExpr->nSubExprCount != 1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Column Summary Function '%s' has wrong number of arguments.", 
                      poExpr->string_value ? poExpr->string_value : "(null)");
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

void swq_select::PushOrderBy( const char *pszFieldName, int bAscending )

{
    order_specs++;
    order_defs = (swq_order_def *) 
        CPLRealloc( order_defs, sizeof(swq_order_def) * order_specs );

    order_defs[order_specs-1].field_name = CPLStrdup(pszFieldName);
    order_defs[order_specs-1].table_index = -1;
    order_defs[order_specs-1].field_index = -1;
    order_defs[order_specs-1].ascending_flag = bAscending;
}

/************************************************************************/
/*                              PushJoin()                              */
/************************************************************************/

void swq_select::PushJoin( int iSecondaryTable,
                           const char *pszPrimaryField,
                           const char *pszSecondaryField )

{
    join_count++;
    join_defs = (swq_join_def *) 
        CPLRealloc( join_defs, sizeof(swq_join_def) * join_count );

    join_defs[join_count-1].secondary_table = iSecondaryTable;
    join_defs[join_count-1].primary_field_name = CPLStrdup(pszPrimaryField);
    join_defs[join_count-1].primary_field = -1;
    join_defs[join_count-1].op = SWQ_EQ;
    join_defs[join_count-1].secondary_field_name = CPLStrdup(pszSecondaryField);
    join_defs[join_count-1].secondary_field = -1;
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

CPLErr swq_select::expand_wildcard( swq_field_list *field_list )

{
    int isrc;

/* ==================================================================== */
/*      Check each pre-expansion field.                                 */
/* ==================================================================== */
    for( isrc = 0; isrc < result_columns; isrc++ )
    {
        const char *src_fieldname = column_defs[isrc].field_name;
        int itable, new_fields, i, iout;

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
        if( strcmp(src_fieldname,"*") == 0 )
        {
            itable = -1;
            new_fields = field_list->count;
        }
        else if( strlen(src_fieldname) < 3 
                 || src_fieldname[strlen(src_fieldname)-2] != '.' )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Ill formatted field definition '%s'.",
                     src_fieldname );
            return CE_Failure;
        }
        else
        {
            char *table_name = CPLStrdup( src_fieldname );
            table_name[strlen(src_fieldname)-2] = '\0';

            for( itable = 0; itable < field_list->table_count; itable++ )
            {
                if( strcasecmp(table_name,
                        field_list->table_defs[itable].table_alias ) == 0 )
                    break;
            }
            
            if( itable == field_list->table_count )
            {
                CPLError( CE_Failure, CPLE_AppDefined,
                         "Table %s not recognised from %s definition.", 
                         table_name, src_fieldname );
                CPLFree( table_name );
                return CE_Failure;
            }
            CPLFree( table_name );
            
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
                for( i = result_columns-1; i > isrc; i-- )
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
        
        for( i = 0; i < field_list->count; i++ )
        {
            swq_col_def *def;
            int compose = itable != -1;

            /* skip this field if it isn't in the target table.  */
            if( itable != -1 && field_list->table_ids != NULL 
                && itable != field_list->table_ids[i] )
                continue;

            /* set up some default values. */
            def = column_defs + iout;
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
                def->field_name = CPLStrdup( field_list->names[i] );
            else
            {
                int itable = field_list->table_ids[i];
                char *composed_name;
                const char *field_name = field_list->names[i];
                const char *table_alias = 
                    field_list->table_defs[itable].table_alias;

                composed_name = (char *) 
                    CPLMalloc(strlen(field_name)+strlen(table_alias)+2);

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

    return CE_None;
}

/************************************************************************/
/*                               parse()                                */
/*                                                                      */
/*      This method really does post-parse processing.                  */
/************************************************************************/

CPLErr swq_select::parse( swq_field_list *field_list,
                          int parse_flags )

{
    int  i;
    CPLErr eError;

    eError = expand_wildcard( field_list );
    if( eError != CE_None )
        return eError;

    
/* -------------------------------------------------------------------- */
/*      Identify field information.                                     */
/* -------------------------------------------------------------------- */
    for( i = 0; i < result_columns; i++ )
    {
        swq_col_def *def = column_defs + i;

        if( def->expr != NULL && def->expr->eNodeType != SNT_COLUMN )
        {
            def->field_index = -1;
            def->table_index = -1;

            if( def->expr->Check( field_list, TRUE ) == SWQ_ERROR )
                return CE_Failure;
                
            def->field_type = def->expr->field_type;

            // If the field was changed from string constant to 
            // column field then adopt the name. 
            if( def->expr->eNodeType == SNT_COLUMN )
            {
                def->field_index = def->expr->field_index;
                def->table_index = def->expr->table_index;

                CPLFree( def->field_name );
                def->field_name = CPLStrdup(def->expr->string_value);
            }
        }
        else
        {
            swq_field_type  this_type;

            /* identify field */
            def->field_index = swq_identify_field( def->field_name, field_list,
                                                   &this_type, 
                                                   &(def->table_index) );
            
            /* record field type */
            def->field_type = this_type;
            
            if( def->field_index == -1 && def->col_func != SWQCF_COUNT )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unrecognised field name %s.", 
                          def->field_name );
                return CE_Failure;
            }
        }

        /* identify column function if present */
        if( (def->col_func == SWQCF_MIN 
             || def->col_func == SWQCF_MAX
             || def->col_func == SWQCF_AVG
             || def->col_func == SWQCF_SUM)
            && def->field_type == SWQ_STRING )
        {
            // possibly this is already enforced by the checker?
            const swq_operation *op = swq_op_registrar::GetOperator( 
                (swq_op) def->col_func );
            
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Use of field function %s() on string field %s illegal.", 
                          op->osName.c_str(), def->field_name );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Check if we are producing a one row summary result or a set     */
/*      of records.  Generate an error if we get conflicting            */
/*      indications.                                                    */
/* -------------------------------------------------------------------- */
    query_mode = -1;
    for( i = 0; i < result_columns; i++ )
    {
        swq_col_def *def = column_defs + i;
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

        if( this_indicator != query_mode
             && this_indicator != -1
            && query_mode != -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Field list implies mixture of regular recordset mode, summary mode or distinct field list mode." );
            return CE_Failure;
        }

        if( this_indicator != -1 )
            query_mode = this_indicator;
    }

    if( result_columns > 1 
        && query_mode == SWQM_DISTINCT_LIST )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "SELECTing more than one DISTINCT field is a query not supported." );
        return CE_Failure;
    }
    else if (result_columns == 0)
    {
        query_mode = SWQM_RECORDSET;
    }

/* -------------------------------------------------------------------- */
/*      Process column names in JOIN specs.                             */
/* -------------------------------------------------------------------- */
    for( i = 0; i < join_count; i++ )
    {
        swq_join_def *def = join_defs + i;
        int          table_id;

        /* identify primary field */
        def->primary_field = swq_identify_field( def->primary_field_name,
                                                 field_list, NULL, &table_id );
        if( def->primary_field == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                     "Unrecognised primary field %s in JOIN clause..", 
                     def->primary_field_name );
            return CE_Failure;
        }
        
        if( table_id != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                     "Currently the primary key must come from the primary table in\n"
                     "JOIN, %s is not from the primary table.",
                     def->primary_field_name );
            return CE_Failure;
        }
        
        /* identify secondary field */
        def->secondary_field = swq_identify_field( def->secondary_field_name,
                                                   field_list, NULL,&table_id);
        if( def->secondary_field == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                     "Unrecognised secondary field %s in JOIN clause..", 
                     def->secondary_field_name );
            return CE_Failure;
        }
        
        if( table_id != def->secondary_table )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                     "Currently the secondary key must come from the secondary table\n"
                     "listed in the JOIN.  %s is not from table %s..",
                     def->secondary_field_name,
                     table_defs[def->secondary_table].table_name);
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Process column names in order specs.                            */
/* -------------------------------------------------------------------- */
    for( i = 0; i < order_specs; i++ )
    {
        swq_order_def *def = order_defs + i;

        /* identify field */
        swq_field_type field_type;
        def->field_index = swq_identify_field( def->field_name, field_list,
                                               &field_type, &(def->table_index) );
        if( def->field_index == -1 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unrecognised field name %s in ORDER BY.", 
                     def->field_name );
            return CE_Failure;
        }

        if( def->table_index != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Cannot use field '%s' of a secondary table in a ORDER BY clause",
                      def->field_name );
            return CE_Failure;
        }
    }

/* -------------------------------------------------------------------- */
/*      Post process the where clause, subbing in field indexes and     */
/*      doing final validation.                                         */
/* -------------------------------------------------------------------- */
    if( where_expr != NULL 
        && where_expr->Check( field_list, FALSE ) == SWQ_ERROR )
    {
        return CE_Failure;
    }
    
    return CE_None;
}
