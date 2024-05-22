/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: swq_select class implementation.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_core.h"
#include "ogr_geometry.h"
#include "swq_parser.hpp"

//! @cond Doxygen_Suppress
/************************************************************************/
/*                             swq_select()                             */
/************************************************************************/

swq_select::swq_select() = default;

/************************************************************************/
/*                            ~swq_select()                             */
/************************************************************************/

swq_select::~swq_select()

{
    delete where_expr;
    CPLFree(raw_select);

    for (int i = 0; i < table_count; i++)
    {
        swq_table_def *table_def = table_defs + i;

        CPLFree(table_def->data_source);
        CPLFree(table_def->table_name);
        CPLFree(table_def->table_alias);
    }
    CPLFree(table_defs);

    for (auto &col : column_defs)
    {
        CPLFree(col.table_name);
        CPLFree(col.field_name);
        CPLFree(col.field_alias);

        delete col.expr;
    }

    // cppcheck-suppress constVariableReference
    for (auto &entry : m_exclude_fields)
    {
        // cppcheck-suppress constVariableReference
        for (auto &col : entry.second)
        {
            CPLFree(col.table_name);
            CPLFree(col.field_name);
            CPLFree(col.field_alias);

            delete col.expr;
        }
    }

    for (int i = 0; i < order_specs; i++)
    {
        CPLFree(order_defs[i].table_name);
        CPLFree(order_defs[i].field_name);
    }

    CPLFree(order_defs);

    for (int i = 0; i < join_count; i++)
    {
        delete join_defs[i].poExpr;
    }
    CPLFree(join_defs);

    delete poOtherSelect;
}

/************************************************************************/
/*                              preparse()                              */
/*                                                                      */
/*      Parse the expression but without knowing the available          */
/*      tables and fields.                                              */
/************************************************************************/

CPLErr swq_select::preparse(const char *select_statement,
                            int bAcceptCustomFuncs)

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
    if (swqparse(&context) != 0)
    {
        delete context.poRoot;
        return CE_Failure;
    }

    // Restore poCurSelect as it might have been modified by UNION ALL
    context.poCurSelect = this;
    swq_fixup(&context);

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
    for (int i = 0; i < join_count / 2; i++)
    {
        swq_join_def sTmp;
        memcpy(&sTmp, &join_defs[i], sizeof(swq_join_def));
        memcpy(&join_defs[i], &join_defs[join_count - 1 - i],
               sizeof(swq_join_def));
        memcpy(&join_defs[join_count - 1 - i], &sTmp, sizeof(swq_join_def));
    }

    // We make that strong assumption in ogr_gensql.
    for (int i = 0; i < join_count; i++)
    {
        CPLAssert(join_defs[i].secondary_table == i + 1);
    }

    if (poOtherSelect != nullptr)
        poOtherSelect->postpreparse();
}

/************************************************************************/
/*                               Unparse()                              */
/************************************************************************/

char *swq_select::Unparse()
{
    CPLString osSelect("SELECT ");
    if (query_mode == SWQM_DISTINCT_LIST)
        osSelect += "DISTINCT ";

    for (int i = 0; i < result_columns(); i++)
    {
        swq_col_def *def = &column_defs[i];

        if (i > 0)
            osSelect += ", ";

        if (def->expr != nullptr && def->col_func == SWQCF_NONE)
        {
            char *pszTmp = def->expr->Unparse(nullptr, '"');
            osSelect += pszTmp;
            CPLFree(pszTmp);
        }
        else
        {
            if (def->col_func == SWQCF_AVG)
                osSelect += "AVG(";
            else if (def->col_func == SWQCF_MIN)
                osSelect += "MIN(";
            else if (def->col_func == SWQCF_MAX)
                osSelect += "MAX(";
            else if (def->col_func == SWQCF_COUNT)
                osSelect += "COUNT(";
            else if (def->col_func == SWQCF_SUM)
                osSelect += "SUM(";

            if (def->distinct_flag && def->col_func == SWQCF_COUNT)
                osSelect += "DISTINCT ";

            if ((def->field_alias == nullptr || table_count > 1) &&
                def->table_name != nullptr && def->table_name[0] != '\0')
            {
                osSelect +=
                    swq_expr_node::QuoteIfNecessary(def->table_name, '"');
                osSelect += ".";
            }
            osSelect += swq_expr_node::QuoteIfNecessary(def->field_name, '"');
            osSelect += ")";
        }

        if (def->field_alias != nullptr &&
            strcmp(def->field_name, def->field_alias) != 0)
        {
            osSelect += " AS ";
            osSelect += swq_expr_node::QuoteIfNecessary(def->field_alias, '"');
        }
    }

    osSelect += " FROM ";
    if (table_defs[0].data_source != nullptr)
    {
        osSelect += "'";
        osSelect += table_defs[0].data_source;
        osSelect += "'.";
    }
    osSelect += swq_expr_node::QuoteIfNecessary(table_defs[0].table_name, '"');
    if (table_defs[0].table_alias != nullptr &&
        strcmp(table_defs[0].table_name, table_defs[0].table_alias) != 0)
    {
        osSelect += " AS ";
        osSelect +=
            swq_expr_node::QuoteIfNecessary(table_defs[0].table_alias, '"');
    }

    for (int i = 0; i < join_count; i++)
    {
        int iTable = join_defs[i].secondary_table;
        osSelect += " JOIN ";
        if (table_defs[iTable].data_source != nullptr)
        {
            osSelect += "'";
            osSelect += table_defs[iTable].data_source;
            osSelect += "'.";
        }
        osSelect +=
            swq_expr_node::QuoteIfNecessary(table_defs[iTable].table_name, '"');
        if (table_defs[iTable].table_alias != nullptr &&
            strcmp(table_defs[iTable].table_name,
                   table_defs[iTable].table_alias) != 0)
        {
            osSelect += " AS ";
            osSelect += swq_expr_node::QuoteIfNecessary(
                table_defs[iTable].table_alias, '"');
        }
        osSelect += " ON ";
        char *pszTmp = join_defs[i].poExpr->Unparse(nullptr, '"');
        osSelect += pszTmp;
        CPLFree(pszTmp);
    }

    if (where_expr != nullptr)
    {
        osSelect += " WHERE ";
        char *pszTmp = where_expr->Unparse(nullptr, '"');
        osSelect += pszTmp;
        CPLFree(pszTmp);
    }

    if (order_specs > 0)
    {
        osSelect += " ORDER BY ";
        for (int i = 0; i < order_specs; i++)
        {
            if (i > 0)
                osSelect += ", ";
            osSelect +=
                swq_expr_node::QuoteIfNecessary(order_defs[i].field_name, '"');
            if (!order_defs[i].ascending_flag)
                osSelect += " DESC";
        }
    }

    if (limit >= 0)
    {
        osSelect += " LIMIT ";
        osSelect += CPLSPrintf(CPL_FRMT_GIB, limit);
    }

    if (offset > 0)
    {
        osSelect += " OFFSET ";
        osSelect += CPLSPrintf(CPL_FRMT_GIB, offset);
    }

    return CPLStrdup(osSelect);
}

/************************************************************************/
/*                             PushField()                              */
/*                                                                      */
/*      Create a new field definition by name and possibly alias.       */
/************************************************************************/

int swq_select::PushField(swq_expr_node *poExpr, const char *pszAlias,
                          int distinct_flag)

{
    if (query_mode == SWQM_DISTINCT_LIST && distinct_flag)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SELECT DISTINCT and COUNT(DISTINCT...) "
                 "not supported together");
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Grow the array.                                                 */
    /* -------------------------------------------------------------------- */

    column_defs.emplace_back();
    swq_col_def *col_def = &column_defs.back();

    memset(col_def, 0, sizeof(swq_col_def));

    /* -------------------------------------------------------------------- */
    /*      Try to capture a field name.                                    */
    /* -------------------------------------------------------------------- */
    if (poExpr->eNodeType == SNT_COLUMN)
    {
        col_def->table_name =
            CPLStrdup(poExpr->table_name ? poExpr->table_name : "");
        col_def->field_name = CPLStrdup(poExpr->string_value);

        // Associate a column list from an EXCEPT () clause with its associated
        // wildcard
        if (EQUAL(col_def->field_name, "*"))
        {
            auto it = m_exclude_fields.find(-1);

            if (it != m_exclude_fields.end())
            {
                int curr_asterisk_pos =
                    static_cast<int>(column_defs.size() - 1);
                m_exclude_fields[curr_asterisk_pos] = std::move(it->second);
                m_exclude_fields.erase(it);
            }
        }
    }
    else if (poExpr->eNodeType == SNT_OPERATION &&
             (poExpr->nOperation == SWQ_CAST ||
              (poExpr->nOperation >= SWQ_AVG &&
               poExpr->nOperation <= SWQ_SUM)) &&
             poExpr->nSubExprCount >= 1 &&
             poExpr->papoSubExpr[0]->eNodeType == SNT_COLUMN)
    {
        col_def->table_name = CPLStrdup(poExpr->papoSubExpr[0]->table_name
                                            ? poExpr->papoSubExpr[0]->table_name
                                            : "");
        col_def->field_name = CPLStrdup(poExpr->papoSubExpr[0]->string_value);
    }
    else
    {
        col_def->table_name = CPLStrdup("");
        col_def->field_name = CPLStrdup("");
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize fields.                                              */
    /* -------------------------------------------------------------------- */
    if (pszAlias != nullptr)
        col_def->field_alias = CPLStrdup(pszAlias);
    else if (poExpr->eNodeType == SNT_OPERATION && poExpr->nSubExprCount >= 1 &&
             (static_cast<swq_op>(poExpr->nOperation) == SWQ_CONCAT ||
              static_cast<swq_op>(poExpr->nOperation) == SWQ_SUBSTR) &&
             poExpr->papoSubExpr[0]->eNodeType == SNT_COLUMN)
    {
        const swq_operation *op = swq_op_registrar::GetOperator(
            static_cast<swq_op>(poExpr->nOperation));

        col_def->field_alias = CPLStrdup(CPLSPrintf(
            "%s_%s", op->pszName, poExpr->papoSubExpr[0]->string_value));
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
    if (poExpr->eNodeType == SNT_OPERATION && poExpr->nOperation == SWQ_CAST)
    {
        const char *pszTypeName = poExpr->papoSubExpr[1]->string_value;
        int parse_precision = 0;

        if (EQUAL(pszTypeName, "character"))
        {
            col_def->target_type = SWQ_STRING;
            col_def->field_length = 1;
        }
        else if (strcasecmp(pszTypeName, "boolean") == 0)
        {
            col_def->target_type = SWQ_BOOLEAN;
        }
        else if (strcasecmp(pszTypeName, "integer") == 0)
        {
            col_def->target_type = SWQ_INTEGER;
        }
        else if (strcasecmp(pszTypeName, "bigint") == 0)
        {
            col_def->target_type = SWQ_INTEGER64;
        }
        else if (strcasecmp(pszTypeName, "smallint") == 0)
        {
            col_def->target_type = SWQ_INTEGER;
            col_def->target_subtype = OFSTInt16;
        }
        else if (strcasecmp(pszTypeName, "float") == 0)
        {
            col_def->target_type = SWQ_FLOAT;
        }
        else if (strcasecmp(pszTypeName, "numeric") == 0)
        {
            col_def->target_type = SWQ_FLOAT;
            parse_precision = 1;
        }
        else if (strcasecmp(pszTypeName, "timestamp") == 0)
        {
            col_def->target_type = SWQ_TIMESTAMP;
        }
        else if (strcasecmp(pszTypeName, "date") == 0)
        {
            col_def->target_type = SWQ_DATE;
        }
        else if (strcasecmp(pszTypeName, "time") == 0)
        {
            col_def->target_type = SWQ_TIME;
        }
        else if (strcasecmp(pszTypeName, "geometry") == 0)
        {
            col_def->target_type = SWQ_GEOMETRY;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unrecognized typename %s in CAST operator.", pszTypeName);
            CPLFree(col_def->table_name);
            col_def->table_name = nullptr;
            CPLFree(col_def->field_name);
            col_def->field_name = nullptr;
            CPLFree(col_def->field_alias);
            col_def->field_alias = nullptr;
            column_defs.pop_back();
            return FALSE;
        }

        if (col_def->target_type == SWQ_GEOMETRY)
        {
            if (poExpr->nSubExprCount > 2)
            {
                if (poExpr->papoSubExpr[2]->field_type != SWQ_STRING)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "First argument of CAST operator should be "
                             "a geometry type identifier.");
                    CPLFree(col_def->table_name);
                    col_def->table_name = nullptr;
                    CPLFree(col_def->field_name);
                    col_def->field_name = nullptr;
                    CPLFree(col_def->field_alias);
                    col_def->field_alias = nullptr;
                    column_defs.pop_back();
                    return FALSE;
                }

                col_def->eGeomType =
                    OGRFromOGCGeomType(poExpr->papoSubExpr[2]->string_value);

                // SRID
                if (poExpr->nSubExprCount > 3)
                {
                    col_def->nSRID =
                        static_cast<int>(poExpr->papoSubExpr[3]->int_value);
                }
            }
        }
        else
        {
            // field width.
            if (poExpr->nSubExprCount > 2)
            {
                if (poExpr->papoSubExpr[2]->field_type != SWQ_INTEGER)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "First argument of CAST operator should be of "
                             "integer type.");
                    CPLFree(col_def->table_name);
                    col_def->table_name = nullptr;
                    CPLFree(col_def->field_name);
                    col_def->field_name = nullptr;
                    CPLFree(col_def->field_alias);
                    col_def->field_alias = nullptr;
                    column_defs.pop_back();
                    return FALSE;
                }
                col_def->field_length =
                    static_cast<int>(poExpr->papoSubExpr[2]->int_value);
            }

            // field width.
            if (poExpr->nSubExprCount > 3 && parse_precision)
            {
                col_def->field_precision =
                    static_cast<int>(poExpr->papoSubExpr[3]->int_value);
                if (col_def->field_precision == 0)
                {
                    if (col_def->field_length < 10)
                        col_def->target_type = SWQ_INTEGER;
                    else if (col_def->field_length < 19)
                        col_def->target_type = SWQ_INTEGER64;
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have a special column function in play?                   */
    /* -------------------------------------------------------------------- */
    if (poExpr->eNodeType == SNT_OPERATION &&
        static_cast<swq_op>(poExpr->nOperation) >= SWQ_AVG &&
        static_cast<swq_op>(poExpr->nOperation) <= SWQ_SUM)
    {
        if (poExpr->nSubExprCount != 1)
        {
            const swq_operation *poOp = swq_op_registrar::GetOperator(
                static_cast<swq_op>(poExpr->nOperation));
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Column Summary Function '%s' has "
                     "wrong number of arguments.",
                     poOp->pszName);
            CPLFree(col_def->table_name);
            col_def->table_name = nullptr;
            CPLFree(col_def->field_name);
            col_def->field_name = nullptr;
            CPLFree(col_def->field_alias);
            col_def->field_alias = nullptr;
            column_defs.pop_back();
            return FALSE;
        }
        else if (poExpr->papoSubExpr[0]->eNodeType != SNT_COLUMN)
        {
            const swq_operation *poOp = swq_op_registrar::GetOperator(
                static_cast<swq_op>(poExpr->nOperation));
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Argument of column Summary Function '%s' "
                     "should be a column.",
                     poOp->pszName);
            CPLFree(col_def->table_name);
            col_def->table_name = nullptr;
            CPLFree(col_def->field_name);
            col_def->field_name = nullptr;
            CPLFree(col_def->field_alias);
            col_def->field_alias = nullptr;
            column_defs.pop_back();
            return FALSE;
        }
        else
        {
            col_def->col_func = static_cast<swq_col_func>(poExpr->nOperation);

            swq_expr_node *poSubExpr = poExpr->papoSubExpr[0];

            poExpr->papoSubExpr[0] = nullptr;
            poExpr->nSubExprCount = 0;
            delete poExpr;

            poExpr = poSubExpr;
        }
    }

    col_def->expr = poExpr;

    return TRUE;
}

int swq_select::PushExcludeField(swq_expr_node *poExpr)
{
    if (poExpr->eNodeType != SNT_COLUMN)
    {
        return FALSE;
    }

    // Check if this column has already been excluded
    for (const auto &col_def : m_exclude_fields[-1])
    {
        if (EQUAL(poExpr->string_value, col_def.field_name) &&
            EQUAL(poExpr->table_name ? poExpr->table_name : "",
                  col_def.table_name))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Field %s.%s repeated in EXCEPT/EXCLUDE expression.",
                     col_def.table_name, col_def.field_name);

            return FALSE;
        }
    }

    m_exclude_fields[-1].emplace_back();
    swq_col_def *col_def = &m_exclude_fields[-1].back();
    memset(col_def, 0, sizeof(swq_col_def));

    col_def->table_name =
        CPLStrdup(poExpr->table_name ? poExpr->table_name : "");
    col_def->field_name = CPLStrdup(poExpr->string_value);
    col_def->table_index = -1;
    col_def->field_index = -1;

    delete poExpr;

    return TRUE;
}

/************************************************************************/
/*                            PushTableDef()                            */
/************************************************************************/

int swq_select::PushTableDef(const char *pszDataSource, const char *pszName,
                             const char *pszAlias)

{
    table_count++;

    table_defs = static_cast<swq_table_def *>(
        CPLRealloc(table_defs, sizeof(swq_table_def) * table_count));

    if (pszDataSource != nullptr)
        table_defs[table_count - 1].data_source = CPLStrdup(pszDataSource);
    else
        table_defs[table_count - 1].data_source = nullptr;

    table_defs[table_count - 1].table_name = CPLStrdup(pszName);

    if (pszAlias != nullptr)
        table_defs[table_count - 1].table_alias = CPLStrdup(pszAlias);
    else
        table_defs[table_count - 1].table_alias = CPLStrdup(pszName);

    return table_count - 1;
}

/************************************************************************/
/*                            PushOrderBy()                             */
/************************************************************************/

void swq_select::PushOrderBy(const char *pszTableName, const char *pszFieldName,
                             int bAscending)

{
    order_specs++;
    order_defs = static_cast<swq_order_def *>(
        CPLRealloc(order_defs, sizeof(swq_order_def) * order_specs));

    order_defs[order_specs - 1].table_name =
        CPLStrdup(pszTableName ? pszTableName : "");
    order_defs[order_specs - 1].field_name = CPLStrdup(pszFieldName);
    order_defs[order_specs - 1].table_index = -1;
    order_defs[order_specs - 1].field_index = -1;
    order_defs[order_specs - 1].ascending_flag = bAscending;
}

/************************************************************************/
/*                              PushJoin()                              */
/************************************************************************/

void swq_select::PushJoin(int iSecondaryTable, swq_expr_node *poExpr)

{
    join_count++;
    join_defs = static_cast<swq_join_def *>(
        CPLRealloc(join_defs, sizeof(swq_join_def) * join_count));

    join_defs[join_count - 1].secondary_table = iSecondaryTable;
    join_defs[join_count - 1].poExpr = poExpr;
}

/************************************************************************/
/*                             PushUnionAll()                           */
/************************************************************************/

void swq_select::PushUnionAll(swq_select *poOtherSelectIn)
{
    CPLAssert(poOtherSelect == nullptr);
    poOtherSelect = poOtherSelectIn;
}

/************************************************************************/
/*                             SetLimit()                               */
/************************************************************************/

void swq_select::SetLimit(GIntBig nLimit)

{
    limit = nLimit;
}

/************************************************************************/
/*                            SetOffset()                               */
/************************************************************************/

void swq_select::SetOffset(GIntBig nOffset)

{
    offset = nOffset;
}

/************************************************************************/
/*                          expand_wildcard()                           */
/*                                                                      */
/*      This function replaces the '*' in a "SELECT *" with the list    */
/*      provided list of fields.  It is used by swq_select::parse(),    */
/*      but may be called in advance by applications wanting the        */
/*      "default" field list to be different than the full list of      */
/*      fields.                                                         */
/************************************************************************/

CPLErr swq_select::expand_wildcard(swq_field_list *field_list,
                                   int bAlwaysPrefixWithTableName)

{
    int columns_added = 0;

    /* ==================================================================== */
    /*      Check each pre-expansion field.                                 */
    /* ==================================================================== */
    for (int isrc = 0; isrc < result_columns(); isrc++)
    {
        const char *src_tablename = column_defs[isrc].table_name;
        const char *src_fieldname = column_defs[isrc].field_name;
        int itable;

        if (*src_fieldname == '\0' ||
            src_fieldname[strlen(src_fieldname) - 1] != '*')
            continue;

        // Don't want to expand COUNT(*).
        if (column_defs[isrc].col_func == SWQCF_COUNT)
            continue;

        // Parse out the table name and verify it
        if (src_tablename[0] == 0 && strcmp(src_fieldname, "*") == 0)
        {
            itable = -1;
        }
        else
        {
            for (itable = 0; itable < field_list->table_count; itable++)
            {
                if (EQUAL(src_tablename,
                          field_list->table_defs[itable].table_alias))
                    break;
            }

            if (itable == field_list->table_count)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Table %s not recognised from %s.%s definition.",
                         src_tablename, src_tablename, src_fieldname);
                return CE_Failure;
            }
        }

        // Assign the selected fields. */
        std::vector<swq_col_def> expanded_columns;
        for (int i = 0; i < field_list->count; i++)
        {
            bool compose = (itable != -1) || bAlwaysPrefixWithTableName;

            // Skip this field if it isn't in the target table.
            if (itable != -1 && itable != field_list->table_ids[i])
                continue;

            auto table_id = field_list->table_ids[i];

            // Skip this field if we've excluded it with SELECT * EXCEPT ()
            if (IsFieldExcluded(isrc - columns_added,
                                field_list->table_defs[table_id].table_name,
                                field_list->names[i]))
            {
                if (field_list->types[i] == SWQ_GEOMETRY)
                {
                    // Need to store the fact that we explicitly excluded
                    // the geometry so we can prevent it from being implicitly
                    // included by OGRGenSQLResultsLayer
                    bExcludedGeometry = true;
                }

                continue;
            }

            // Set up some default values.
            expanded_columns.emplace_back();
            swq_col_def *def = &expanded_columns.back();
            def->field_precision = -1;
            def->target_type = SWQ_OTHER;
            def->target_subtype = OFSTNone;

            // Does this field duplicate an earlier one?
            if (field_list->table_ids[i] != 0 && !compose)
            {
                for (int other = 0; other < i; other++)
                {
                    if (EQUAL(field_list->names[i], field_list->names[other]))
                    {
                        compose = true;
                        break;
                    }
                }
            }

            int field_itable = field_list->table_ids[i];
            const char *field_name = field_list->names[i];
            const char *table_alias =
                field_list->table_defs[field_itable].table_alias;

            def->table_name = CPLStrdup(table_alias);
            def->field_name = CPLStrdup(field_name);
            if (!compose)
                def->field_alias = CPLStrdup(field_list->names[i]);

            // All the other table info will be provided by the later
            // parse operation.
        }

        // Splice expanded_columns in at the position of '*'
        CPLFree(column_defs[isrc].table_name);
        CPLFree(column_defs[isrc].field_name);
        CPLFree(column_defs[isrc].field_alias);
        delete column_defs[isrc].expr;
        auto pos = column_defs.erase(std::next(column_defs.begin(), isrc));

        column_defs.insert(pos, expanded_columns.begin(),
                           expanded_columns.end());

        columns_added += static_cast<int>(expanded_columns.size()) - 1;

        const auto it = m_exclude_fields.find(isrc);
        if (it != m_exclude_fields.end())
        {
            if (!it->second.empty())
            {
                const auto &field = it->second.front();
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Field %s specified in EXCEPT/EXCLUDE expression not found",
                    field.field_name);
                return CE_Failure;
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                       CheckCompatibleJoinExpr()                      */
/************************************************************************/

static bool CheckCompatibleJoinExpr(swq_expr_node *poExpr, int secondary_table,
                                    swq_field_list *field_list)
{
    if (poExpr->eNodeType == SNT_CONSTANT)
        return true;

    if (poExpr->eNodeType == SNT_COLUMN)
    {
        CPLAssert(poExpr->field_index != -1);
        CPLAssert(poExpr->table_index != -1);
        if (poExpr->table_index != 0 && poExpr->table_index != secondary_table)
        {
            if (poExpr->table_name)
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %s.%s in JOIN clause does not correspond to "
                         "the primary table nor the joint (secondary) table.",
                         poExpr->table_name, poExpr->string_value);
            else
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %s in JOIN clause does not correspond to the "
                         "primary table nor the joint (secondary) table.",
                         poExpr->string_value);
            return false;
        }

        return true;
    }

    if (poExpr->eNodeType == SNT_OPERATION)
    {
        for (int i = 0; i < poExpr->nSubExprCount; i++)
        {
            if (!CheckCompatibleJoinExpr(poExpr->papoSubExpr[i],
                                         secondary_table, field_list))
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

CPLErr swq_select::parse(swq_field_list *field_list,
                         swq_select_parse_options *poParseOptions)
{
    int bAlwaysPrefixWithTableName =
        poParseOptions && poParseOptions->bAlwaysPrefixWithTableName;
    CPLErr eError = expand_wildcard(field_list, bAlwaysPrefixWithTableName);
    if (eError != CE_None)
        return eError;

    swq_custom_func_registrar *poCustomFuncRegistrar = nullptr;
    if (poParseOptions != nullptr)
        poCustomFuncRegistrar = poParseOptions->poCustomFuncRegistrar;

    /* -------------------------------------------------------------------- */
    /*      Identify field information.                                     */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < result_columns(); i++)
    {
        swq_col_def *def = &column_defs[i];

        if (def->expr != nullptr && def->expr->eNodeType != SNT_COLUMN)
        {
            def->field_index = -1;
            def->table_index = -1;

            if (def->expr->Check(field_list, TRUE, FALSE,
                                 poCustomFuncRegistrar) == SWQ_ERROR)
                return CE_Failure;

            def->field_type = def->expr->field_type;
        }
        else
        {
            swq_field_type this_type;

            // Identify field.
            def->field_index =
                swq_identify_field(def->table_name, def->field_name, field_list,
                                   &this_type, &(def->table_index));

            // Record field type.
            def->field_type = this_type;

            if (def->field_index == -1 && !(def->col_func == SWQCF_COUNT &&
                                            strcmp(def->field_name, "*") == 0))
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined, "Unrecognized field name %s.",
                    def->table_name[0]
                        ? CPLSPrintf("%s.%s", def->table_name, def->field_name)
                        : def->field_name);
                return CE_Failure;
            }
        }

        // Identify column function if present.
        if (((def->col_func == SWQCF_MIN || def->col_func == SWQCF_MAX ||
              def->col_func == SWQCF_AVG || def->col_func == SWQCF_SUM) &&
             def->field_type == SWQ_GEOMETRY) ||
            ((def->col_func == SWQCF_AVG || def->col_func == SWQCF_SUM) &&
             def->field_type == SWQ_STRING))
        {
            // Possibly this is already enforced by the checker?
            const swq_operation *op = swq_op_registrar::GetOperator(
                static_cast<swq_op>(def->col_func));
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Use of field function %s() on %s field %s illegal.",
                     op->pszName, SWQFieldTypeToString(def->field_type),
                     def->field_name);
            return CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Check if we are producing a one row summary result or a set     */
    /*      of records.  Generate an error if we get conflicting            */
    /*      indications.                                                    */
    /* -------------------------------------------------------------------- */

    int bAllowDistinctOnMultipleFields =
        (poParseOptions && poParseOptions->bAllowDistinctOnMultipleFields);
    if (query_mode == SWQM_DISTINCT_LIST && result_columns() > 1 &&
        !bAllowDistinctOnMultipleFields)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SELECT DISTINCT not supported on multiple columns.");
        return CE_Failure;
    }

    for (int i = 0; i < result_columns(); i++)
    {
        swq_col_def *def = &column_defs[i];
        int this_indicator = -1;

        if (query_mode == SWQM_DISTINCT_LIST && def->field_type == SWQ_GEOMETRY)
        {
            const bool bAllowDistinctOnGeometryField =
                poParseOptions && poParseOptions->bAllowDistinctOnGeometryField;
            if (!bAllowDistinctOnGeometryField)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "SELECT DISTINCT on a geometry not supported.");
                return CE_Failure;
            }
        }

        if (def->col_func == SWQCF_MIN || def->col_func == SWQCF_MAX ||
            def->col_func == SWQCF_AVG || def->col_func == SWQCF_SUM ||
            def->col_func == SWQCF_COUNT)
        {
            this_indicator = SWQM_SUMMARY_RECORD;
            if (def->col_func == SWQCF_COUNT && def->distinct_flag &&
                def->field_type == SWQ_GEOMETRY)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "SELECT COUNT DISTINCT on a geometry not supported.");
                return CE_Failure;
            }
        }
        else if (def->col_func == SWQCF_NONE)
        {
            if (query_mode == SWQM_DISTINCT_LIST)
            {
                def->distinct_flag = TRUE;
                this_indicator = SWQM_DISTINCT_LIST;
            }
            else
                this_indicator = SWQM_RECORDSET;
        }

        if (this_indicator != query_mode && this_indicator != -1 &&
            query_mode != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Field list implies mixture of regular recordset mode, "
                     "summary mode or distinct field list mode.");
            return CE_Failure;
        }

        if (this_indicator != -1)
            query_mode = this_indicator;
    }

    if (result_columns() == 0)
    {
        query_mode = SWQM_RECORDSET;
    }

    /* -------------------------------------------------------------------- */
    /*      Process column names in JOIN specs.                             */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < join_count; i++)
    {
        swq_join_def *def = join_defs + i;
        if (def->poExpr->Check(field_list, TRUE, TRUE, poCustomFuncRegistrar) ==
            SWQ_ERROR)
            return CE_Failure;
        if (!CheckCompatibleJoinExpr(def->poExpr, def->secondary_table,
                                     field_list))
            return CE_Failure;
    }

    /* -------------------------------------------------------------------- */
    /*      Process column names in order specs.                            */
    /* -------------------------------------------------------------------- */
    for (int i = 0; i < order_specs; i++)
    {
        swq_order_def *def = order_defs + i;

        // Identify field.
        swq_field_type field_type;
        def->field_index =
            swq_identify_field(def->table_name, def->field_name, field_list,
                               &field_type, &(def->table_index));
        if (def->field_index == -1)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unrecognized field name %s in ORDER BY.",
                     def->table_name[0]
                         ? CPLSPrintf("%s.%s", def->table_name, def->field_name)
                         : def->field_name);
            return CE_Failure;
        }

        if (def->table_index != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot use field '%s' of a secondary table in "
                     "an ORDER BY clause",
                     def->field_name);
            return CE_Failure;
        }

        if (field_type == SWQ_GEOMETRY)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot use geometry field '%s' in an ORDER BY clause",
                     def->field_name);
            return CE_Failure;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Post process the where clause, subbing in field indexes and     */
    /*      doing final validation.                                         */
    /* -------------------------------------------------------------------- */
    int bAllowFieldsInSecondaryTablesInWhere = FALSE;
    if (poParseOptions != nullptr)
        bAllowFieldsInSecondaryTablesInWhere =
            poParseOptions->bAllowFieldsInSecondaryTablesInWhere;
    if (where_expr != nullptr &&
        where_expr->Check(field_list, bAllowFieldsInSecondaryTablesInWhere,
                          FALSE, poCustomFuncRegistrar) == SWQ_ERROR)
    {
        return CE_Failure;
    }

    return CE_None;
}

bool swq_select::IsFieldExcluded(int src_index, const char *pszTableName,
                                 const char *pszFieldName)
{
    auto list_it = m_exclude_fields.find(src_index);

    if (list_it == m_exclude_fields.end())
    {
        return false;
    }

    auto &excluded_fields = list_it->second;

    auto it = std::partition(
        excluded_fields.begin(), excluded_fields.end(),
        [pszTableName, pszFieldName](const swq_col_def &exclude_field)
        {
            if (!(EQUAL(exclude_field.table_name, "") ||
                  EQUAL(pszTableName, exclude_field.table_name)))
            {
                return true;
            }

            return !EQUAL(pszFieldName, exclude_field.field_name);
        });

    if (it != excluded_fields.end())
    {
        CPLFree(it->table_name);
        CPLFree(it->field_name);
        CPLFree(it->field_alias);

        delete it->expr;

        excluded_fields.erase(it);
        return true;
    }

    return false;
}

//! @endcond
