/******************************************************************************
 *
 * Component: OGDI Driver Support Library
 * Purpose: Generic SQL WHERE Expression Implementation.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (C) 2001 Information Interoperability Institute (3i)
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_port.h"
#include "swq.h"

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <algorithm>
#include <limits>
#include <string>

#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_time.h"
#include "swq_parser.hpp"

CPL_CVSID("$Id$")

#define YYSTYPE swq_expr_node *

/************************************************************************/
/*                               swqlex()                               */
/************************************************************************/

void swqerror( swq_parse_context *context, const char *msg )
{
    CPLString osMsg;
    osMsg.Printf( "SQL Expression Parsing Error: %s. Occurred around :\n",
                  msg );

    int n = static_cast<int>(context->pszLastValid - context->pszInput);

    for( int i = std::max(0, n - 40);
         i < n + 40 && context->pszInput[i] != '\0';
         i++ )
        osMsg += context->pszInput[i];
    osMsg += "\n";
    for( int i = 0; i < std::min(n, 40); i++ )
        osMsg += " ";
    osMsg += "^";

    CPLError( CE_Failure, CPLE_AppDefined, "%s", osMsg.c_str() );
}

/************************************************************************/
/*                               swqlex()                               */
/*                                                                      */
/*      Read back a token from the input.                               */
/************************************************************************/

int swqlex( YYSTYPE *ppNode, swq_parse_context *context )
{
    const char *pszInput = context->pszNext;

    *ppNode = NULL;

/* -------------------------------------------------------------------- */
/*      Do we have a start symbol to return?                            */
/* -------------------------------------------------------------------- */
    if( context->nStartToken != 0 )
    {
        int nRet = context->nStartToken;
        context->nStartToken = 0;
        return nRet;
    }

/* -------------------------------------------------------------------- */
/*      Skip white space.                                               */
/* -------------------------------------------------------------------- */
    while( *pszInput == ' ' || *pszInput == '\t'
           || *pszInput == 10 || *pszInput == 13 )
        pszInput++;

    context->pszLastValid = pszInput;

    if( *pszInput == '\0' )
    {
        context->pszNext = pszInput;
        return EOF;
    }

/* -------------------------------------------------------------------- */
/*      Handle string constants.                                        */
/* -------------------------------------------------------------------- */
    if( *pszInput == '"' || *pszInput == '\'' )
    {
        char chQuote = *pszInput;
        bool bFoundEndQuote = false;

        int nRet = *pszInput == '"' ? SWQT_IDENTIFIER : SWQT_STRING;

        pszInput++;

        char *token = static_cast<char *>(CPLMalloc(strlen(pszInput) + 1));
        int i_token = 0;

        while( *pszInput != '\0' )
        {
            if( chQuote == '"' && *pszInput == '\\' && pszInput[1] == '"' )
                pszInput++;
            else if( chQuote == '\'' && *pszInput == '\\' &&
                     pszInput[1] == '\'' )
                pszInput++;
            else if( chQuote == '\'' && *pszInput == '\'' &&
                     pszInput[1] == '\'' )
                pszInput++;
            else if( *pszInput == chQuote )
            {
                pszInput++;
                bFoundEndQuote = true;
                break;
            }

            token[i_token++] = *(pszInput++);
        }
        token[i_token] = '\0';

        if( !bFoundEndQuote )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Did not find end-of-string character");
            CPLFree( token );
            return 0;
        }

        *ppNode = new swq_expr_node( token );
        CPLFree( token );

        context->pszNext = pszInput;

        return nRet;
    }

/* -------------------------------------------------------------------- */
/*      Handle numbers.                                                 */
/* -------------------------------------------------------------------- */
    else if( *pszInput >= '0' && *pszInput <= '9' )
    {
        CPLString osToken;
        const char *pszNext = pszInput + 1;

        osToken += *pszInput;

        // collect non-decimal part of number
        while( *pszNext >= '0' && *pszNext <= '9' )
            osToken += *(pszNext++);

        // collect decimal places.
        if( *pszNext == '.' )
        {
            osToken += *(pszNext++);
            while( *pszNext >= '0' && *pszNext <= '9' )
                osToken += *(pszNext++);
        }

        // collect exponent
        if( *pszNext == 'e' || *pszNext == 'E' )
        {
            osToken += *(pszNext++);
            if( *pszNext == '-' || *pszNext == '+' )
                osToken += *(pszNext++);
            while( *pszNext >= '0' && *pszNext <= '9' )
                osToken += *(pszNext++);
        }

        context->pszNext = pszNext;

        if( strstr(osToken, ".")
            || strstr(osToken, "e")
            || strstr(osToken, "E") )
        {
            *ppNode = new swq_expr_node( CPLAtof(osToken) );
            return SWQT_FLOAT_NUMBER;
        }
        else
        {
            GIntBig nVal = CPLAtoGIntBig(osToken);
            if( CPL_INT64_FITS_ON_INT32(nVal) )
                *ppNode = new swq_expr_node( static_cast<int>(nVal) );
            else
                *ppNode = new swq_expr_node( nVal );
            return SWQT_INTEGER_NUMBER;
        }
    }

/* -------------------------------------------------------------------- */
/*      Handle alpha-numerics.                                          */
/* -------------------------------------------------------------------- */
    else if( isalnum( *pszInput ) )
    {
        int nReturn = SWQT_IDENTIFIER;
        CPLString osToken;
        const char *pszNext = pszInput + 1;

        osToken += *pszInput;

        // collect text characters
        while( isalnum( *pszNext ) || *pszNext == '_'
               || ((unsigned char) *pszNext) > 127 )
            osToken += *(pszNext++);

        context->pszNext = pszNext;

        if( EQUAL(osToken, "IN") )
            nReturn = SWQT_IN;
        else if( EQUAL(osToken, "LIKE") )
            nReturn = SWQT_LIKE;
        else if( EQUAL(osToken, "ILIKE") )
            nReturn = SWQT_LIKE;
        else if( EQUAL(osToken, "ESCAPE") )
            nReturn = SWQT_ESCAPE;
        else if( EQUAL(osToken, "NULL") )
            nReturn = SWQT_NULL;
        else if( EQUAL(osToken, "IS") )
            nReturn = SWQT_IS;
        else if( EQUAL(osToken, "NOT") )
            nReturn = SWQT_NOT;
        else if( EQUAL(osToken, "AND") )
            nReturn = SWQT_AND;
        else if( EQUAL(osToken, "OR") )
            nReturn = SWQT_OR;
        else if( EQUAL(osToken, "BETWEEN") )
            nReturn = SWQT_BETWEEN;
        else if( EQUAL(osToken, "SELECT") )
            nReturn = SWQT_SELECT;
        else if( EQUAL(osToken, "LEFT") )
            nReturn = SWQT_LEFT;
        else if( EQUAL(osToken, "JOIN") )
            nReturn = SWQT_JOIN;
        else if( EQUAL(osToken, "WHERE") )
            nReturn = SWQT_WHERE;
        else if( EQUAL(osToken, "ON") )
            nReturn = SWQT_ON;
        else if( EQUAL(osToken, "ORDER") )
            nReturn = SWQT_ORDER;
        else if( EQUAL(osToken, "BY") )
            nReturn = SWQT_BY;
        else if( EQUAL(osToken, "FROM") )
            nReturn = SWQT_FROM;
        else if( EQUAL(osToken, "AS") )
            nReturn = SWQT_AS;
        else if( EQUAL(osToken, "ASC") )
            nReturn = SWQT_ASC;
        else if( EQUAL(osToken, "DESC") )
            nReturn = SWQT_DESC;
        else if( EQUAL(osToken, "DISTINCT") )
            nReturn = SWQT_DISTINCT;
        else if( EQUAL(osToken, "CAST") )
            nReturn = SWQT_CAST;
        else if( EQUAL(osToken, "UNION") )
            nReturn = SWQT_UNION;
        else if( EQUAL(osToken, "ALL") )
            nReturn = SWQT_ALL;
        else if( EQUAL(osToken, "LIMIT") )
            nReturn = SWQT_LIMIT;
        else if( EQUAL(osToken, "OFFSET") )
            nReturn = SWQT_OFFSET;

        // Unhandled by OGR SQL.
        else if( EQUAL(osToken, "LIMIT") ||
                 EQUAL(osToken, "OUTER") ||
                 EQUAL(osToken, "INNER") )
            nReturn = SWQT_RESERVED_KEYWORD;

        else
        {
            *ppNode = new swq_expr_node( osToken );
            nReturn = SWQT_IDENTIFIER;
        }

        return nReturn;
    }

/* -------------------------------------------------------------------- */
/*      Handle special tokens.                                          */
/* -------------------------------------------------------------------- */
    else
    {
        context->pszNext = pszInput+1;
        return *pszInput;
    }
}

/************************************************************************/
/*                        swq_select_summarize()                        */
/************************************************************************/

const char *
swq_select_summarize( swq_select *select_info,
                      int dest_column, const char *value )

{
    swq_col_def *def = select_info->column_defs + dest_column;

/* -------------------------------------------------------------------- */
/*      Do various checking.                                            */
/* -------------------------------------------------------------------- */
    if( select_info->query_mode == SWQM_RECORDSET )
        return "swq_select_summarize() called on non-summary query.";

    if( dest_column < 0 || dest_column >= select_info->result_columns )
        return "dest_column out of range in swq_select_summarize().";

    if( def->col_func == SWQCF_NONE && !def->distinct_flag )
        return NULL;

    if( select_info->query_mode == SWQM_DISTINCT_LIST &&
        select_info->order_specs > 0 )
    {
        if( select_info->order_specs > 1 )
            return "Can't ORDER BY a DISTINCT list by more than one key.";

        if( select_info->order_defs[0].field_index !=
            select_info->column_defs[0].field_index )
            return "Only selected DISTINCT field can be used for ORDER BY.";
    }

/* -------------------------------------------------------------------- */
/*      Create the summary information if this is the first row         */
/*      being processed.                                                */
/* -------------------------------------------------------------------- */
    if( select_info->column_summary.empty() )
    {
        select_info->column_summary.resize(select_info->result_columns);
        for( int i = 0; i < select_info->result_columns; i++ )
        {
            if( def->distinct_flag )
            {
                swq_summary::Comparator oComparator;
                if( select_info->order_specs > 0 )
                {
                    CPLAssert( select_info->order_specs ==1 );
                    CPLAssert( select_info->result_columns == 1 );
                    oComparator.bSortAsc =
                        CPL_TO_BOOL(select_info->order_defs[0].ascending_flag);
                }
                if( select_info->column_defs[i].field_type == SWQ_INTEGER ||
-                   select_info->column_defs[i].field_type == SWQ_INTEGER64 )
                {
                    oComparator.eType = SWQ_INTEGER64;
                }
                else if( select_info->column_defs[i].field_type == SWQ_FLOAT )
                {
                    oComparator.eType = SWQ_FLOAT;
                }
                else
                {
                    oComparator.eType = SWQ_STRING;
                }
                select_info->column_summary[i].oSetDistinctValues =
                    std::set<CPLString, swq_summary::Comparator>(oComparator);
            }
            select_info->column_summary[i].min =
                std::numeric_limits<double>::infinity();
            select_info->column_summary[i].max =
                -std::numeric_limits<double>::infinity();
            select_info->column_summary[i].osMin = "9999/99/99 99:99:99";
            select_info->column_summary[i].osMax = "0000/00/00 00:00:00";
        }
    }

/* -------------------------------------------------------------------- */
/*      If distinct processing is on, process that now.                 */
/* -------------------------------------------------------------------- */
    swq_summary& summary = select_info->column_summary[dest_column];

    if( def->distinct_flag )
    {
        if( value == NULL )
            value = SZ_OGR_NULL;
        try
        {
            if( summary.oSetDistinctValues.find(value) ==
                    summary.oSetDistinctValues.end() )
            {
                summary.oSetDistinctValues.insert(value);
                if( select_info->order_specs == 0 )
                {
                    // If not sorted, keep values in their original order
                    summary.oVectorDistinctValues.push_back(value);
                }
                summary.count ++;
            }
        }
        catch( std::bad_alloc& )
        {
            return "Out of memory";
        }

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Process various options.                                        */
/* -------------------------------------------------------------------- */

    switch( def->col_func )
    {
      case SWQCF_MIN:
        if( value != NULL && value[0] != '\0' )
        {
            if( def->field_type == SWQ_DATE ||
                def->field_type == SWQ_TIME ||
                def->field_type == SWQ_TIMESTAMP )
            {
                if( strcmp( value, summary.osMin ) < 0 )
                {
                    summary.osMin = value;
                }
            }
            else
            {
                double df_val = CPLAtof(value);
                if( df_val < summary.min )
                    summary.min = df_val;
            }
            summary.count++;
        }
        break;
      case SWQCF_MAX:
        if( value != NULL && value[0] != '\0' )
        {
            if( def->field_type == SWQ_DATE ||
                def->field_type == SWQ_TIME ||
                def->field_type == SWQ_TIMESTAMP )
            {
                if( strcmp( value, summary.osMax ) > 0 )
                {
                    summary.osMax = value;
                }
            }
            else
            {
                double df_val = CPLAtof(value);
                if( df_val > summary.max )
                    summary.max = df_val;
            }
            summary.count++;
        }
        break;
      case SWQCF_AVG:
      case SWQCF_SUM:
        if( value != NULL && value[0] != '\0' )
        {
            if( def->field_type == SWQ_DATE ||
                def->field_type == SWQ_TIME ||
                def->field_type == SWQ_TIMESTAMP )
            {
                OGRField sField;
                if( OGRParseDate( value, &sField, 0 ) )
                {
                    struct tm brokendowntime;
                    brokendowntime.tm_year = sField.Date.Year - 1900;
                    brokendowntime.tm_mon = sField.Date.Month - 1;
                    brokendowntime.tm_mday = sField.Date.Day;
                    brokendowntime.tm_hour = sField.Date.Hour;
                    brokendowntime.tm_min = sField.Date.Minute;
                    brokendowntime.tm_sec =
                        static_cast<int>(sField.Date.Second);
                    summary.count++;
                    summary.sum += CPLYMDHMSToUnixTime(&brokendowntime);
                    summary.sum += fmod(static_cast<double>(
                                                    sField.Date.Second), 1.0);
                }
            }
            else
            {
                summary.count++;
                summary.sum += CPLAtof(value);
            }
        }
        break;

      case SWQCF_COUNT:
        if( value != NULL )
            summary.count++;
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

static bool Compare (swq_field_type eType,
                     const CPLString& a,
                     const CPLString& b)
{
    if( a == SZ_OGR_NULL )
        return b != SZ_OGR_NULL;
    else if( b == SZ_OGR_NULL )
        return false;
    else
    {
        if( eType == SWQ_INTEGER64 )
            return CPLAtoGIntBig(a) < CPLAtoGIntBig(b);
        else if( eType == SWQ_FLOAT )
            return CPLAtof(a) < CPLAtof(b);
        else if( eType == SWQ_STRING )
            return a < b;
        else
        {
            CPLAssert( false );
            return false;
        }
    }
}

#ifndef DOXYGEN_SKIP
bool swq_summary::Comparator::operator() (const CPLString& a,
                                          const CPLString& b) const
{
    if( bSortAsc )
    {
        return Compare(eType, a, b);
    }
    else
    {
        return Compare(eType, b, a);
    }
}
#endif

/************************************************************************/
/*                         swq_identify_field()                         */
/************************************************************************/
int swq_identify_field_internal( const char* table_name,
                                 const char *field_token,
                                 swq_field_list *field_list,
                                 swq_field_type *this_type, int *table_id,
                                 int bOneMoreTimeOK );

int swq_identify_field( const char* table_name, const char *field_token,
                                 swq_field_list *field_list,
                                 swq_field_type *this_type, int *table_id )

{
    return swq_identify_field_internal(table_name, field_token, field_list,
                                       this_type, table_id, TRUE);
}

int swq_identify_field_internal( const char* table_name,
                                 const char *field_token,
                                 swq_field_list *field_list,
                                 swq_field_type *this_type, int *table_id,
                                 int bOneMoreTimeOK )

{
    if( table_name == NULL )
        table_name = "";

    int tables_enabled;

    if( field_list->table_count > 0 && field_list->table_ids != NULL )
        tables_enabled = TRUE;
    else
        tables_enabled = FALSE;

/* -------------------------------------------------------------------- */
/*      Search for matching field.                                      */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < field_list->count; i++ )
    {
        if( !EQUAL( field_list->names[i], field_token ) )
            continue;

        int t_id = 0;

        // Do the table specifications match?/
        if( tables_enabled )
        {
            t_id = field_list->table_ids[i];
            if( table_name[0] != '\0'
                && !EQUAL(table_name, field_list->table_defs[t_id].table_alias))
                continue;

            // if( t_id != 0 && table_name[0] == '\0' )
            //     continue;
        }
        else if( table_name[0] != '\0' )
            break;

        // We have a match, return various information.
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
/*      When there is no ambiguity, try to accept quoting errors...     */
/* -------------------------------------------------------------------- */
    if( bOneMoreTimeOK &&
        !CPLTestBool(CPLGetConfigOption("OGR_SQL_STRICT", "FALSE")) )
    {
        if( table_name[0] )
        {
            CPLString osAggregatedName(
                CPLSPrintf("%s.%s", table_name, field_token));

            // Check there's no table called table_name, or a field called with
            // the aggregated name.
            int i = 0;  // Used after for.
            for( ; i < field_list->count; i++ )
            {
                if( tables_enabled )
                {
                    int t_id = field_list->table_ids[i];
                    if( EQUAL(table_name,
                              field_list->table_defs[t_id].table_alias) )
                        break;
                }
            }
            if( i == field_list->count )
            {
                int ret = swq_identify_field_internal( NULL,
                                            osAggregatedName,
                                            field_list,
                                            this_type, table_id, FALSE );
                if( ret >= 0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Passed field name %s.%s should have been "
                             "surrounded by double quotes. "
                             "Accepted since there is no ambiguity...",
                             table_name, field_token);
                }
                return ret;
            }
        }
        else
        {
            // If the fieldname is a.b (and there's no . in b), then
            // it might be an error in providing it as being quoted where it
            // should not have been quoted.
            const char* pszDot = strchr(field_token, '.');
            if( pszDot && strchr(pszDot+1, '.') == NULL )
            {
                CPLString osTableName(field_token);
                osTableName.resize(pszDot - field_token);
                CPLString osFieldName(pszDot + 1);

                int ret = swq_identify_field_internal( osTableName,
                                            osFieldName,
                                            field_list,
                                            this_type, table_id, FALSE );
                if( ret >= 0 )
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                            "Passed field name %s should NOT have been "
                             "surrounded by double quotes. "
                            "Accepted since there is no ambiguity...",
                            field_token);
                }
                return ret;
            }
        }
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
/*                          swq_expr_compile()                          */
/************************************************************************/

CPLErr swq_expr_compile( const char *where_clause,
                         int field_count,
                         char **field_names,
                         swq_field_type *field_types,
                         int bCheck,
                         swq_custom_func_registrar* poCustomFuncRegistrar,
                         swq_expr_node **expr_out )

{
    swq_field_list field_list;

    field_list.count = field_count;
    field_list.names = field_names;
    field_list.types = field_types;
    field_list.table_ids = NULL;
    field_list.ids = NULL;

    field_list.table_count = 0;
    field_list.table_defs = NULL;

    return swq_expr_compile2( where_clause, &field_list,
                              bCheck, poCustomFuncRegistrar, expr_out );
}

/************************************************************************/
/*                         swq_expr_compile2()                          */
/************************************************************************/

CPLErr swq_expr_compile2( const char *where_clause,
                          swq_field_list *field_list,
                          int bCheck,
                          swq_custom_func_registrar* poCustomFuncRegistrar,
                          swq_expr_node **expr_out )

{
    swq_parse_context context;

    context.pszInput = where_clause;
    context.pszNext = where_clause;
    context.pszLastValid = where_clause;
    context.nStartToken = SWQT_VALUE_START;
    context.bAcceptCustomFuncs = poCustomFuncRegistrar != NULL;

    if( swqparse( &context ) == 0 &&
        bCheck &&
        context.poRoot->Check( field_list, FALSE, FALSE,
                               poCustomFuncRegistrar ) != SWQ_ERROR )
    {
        *expr_out = context.poRoot;

        return CE_None;
    }
    else
    {
        delete context.poRoot;
        *expr_out = NULL;
        return CE_Failure;
    }
}

/************************************************************************/
/*                        swq_is_reserved_keyword()                     */
/************************************************************************/

static const char* const apszSQLReservedKeywords[] = {
    "OR",
    "AND",
    "NOT",
    "LIKE",
    "IS",
    "NULL",
    "IN",
    "BETWEEN",
    "CAST",
    "DISTINCT",
    "ESCAPE",
    "SELECT",
    "LEFT",
    "JOIN",
    "WHERE",
    "ON",
    "ORDER",
    "BY",
    "FROM",
    "AS",
    "ASC",
    "DESC",
    "UNION",
    "ALL"
};

int swq_is_reserved_keyword(const char* pszStr)
{
    for( int i = 0;
         i < (int)(sizeof(apszSQLReservedKeywords) / sizeof(char*));
         i++ )
    {
        if( EQUAL(pszStr, apszSQLReservedKeywords[i]) )
            return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                          SWQFieldTypeToString()                      */
/************************************************************************/

const char* SWQFieldTypeToString( swq_field_type field_type )
{
    switch( field_type )
    {
        case SWQ_INTEGER:   return "integer";
        case SWQ_INTEGER64: return "bigint";
        case SWQ_FLOAT:     return "float";
        case SWQ_STRING:    return "string";
        case SWQ_BOOLEAN:   return "boolean";
        case SWQ_DATE:      return "date";
        case SWQ_TIME:      return "time";
        case SWQ_TIMESTAMP: return "timestamp";
        case SWQ_GEOMETRY:  return "geometry";
        case SWQ_NULL:      return "null";
        default: return "unknown";
    }
}
