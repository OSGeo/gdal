/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: Implementation of the swq_expr_node class used to represent a
 *          node in an SQL expression.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#ifndef DOXYGEN_SKIP

#include "cpl_port.h"
#include "ogr_swq.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "ogr_geometry.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           swq_expr_node()                            */
/************************************************************************/

swq_expr_node::swq_expr_node() = default;

/************************************************************************/
/*                          swq_expr_node(int)                          */
/************************************************************************/

swq_expr_node::swq_expr_node( int nValueIn ):
    int_value(nValueIn)
{
}

/************************************************************************/
/*                        swq_expr_node(GIntBig)                        */
/************************************************************************/

swq_expr_node::swq_expr_node( GIntBig nValueIn ):
    field_type(SWQ_INTEGER64),
    int_value(nValueIn)
{
}

/************************************************************************/
/*                        swq_expr_node(double)                         */
/************************************************************************/

swq_expr_node::swq_expr_node( double dfValueIn ):
    field_type(SWQ_FLOAT),
    float_value(dfValueIn)
{
}

/************************************************************************/
/*                        swq_expr_node(const char*)                    */
/************************************************************************/

swq_expr_node::swq_expr_node( const char *pszValueIn ):
    field_type(SWQ_STRING),
    is_null(pszValueIn == nullptr),
    string_value(CPLStrdup( pszValueIn ? pszValueIn : "" ))
{
}

/************************************************************************/
/*                      swq_expr_node(OGRGeometry *)                    */
/************************************************************************/

swq_expr_node::swq_expr_node( OGRGeometry *poGeomIn ):
    field_type(SWQ_GEOMETRY),
    is_null(poGeomIn == nullptr),
    geometry_value(poGeomIn ? poGeomIn->clone() : nullptr)
{
}

/************************************************************************/
/*                        swq_expr_node(swq_op)                         */
/************************************************************************/

swq_expr_node::swq_expr_node( swq_op eOp ):
    eNodeType(SNT_OPERATION),
    nOperation(eOp)
{
}

/************************************************************************/
/*                           ~swq_expr_node()                           */
/************************************************************************/

swq_expr_node::~swq_expr_node()

{
    CPLFree( table_name );
    CPLFree( string_value );

    for( int i = 0; i < nSubExprCount; i++ )
        delete papoSubExpr[i];
    CPLFree( papoSubExpr );
    delete geometry_value;
}

/************************************************************************/
/*                          MarkAsTimestamp()                           */
/************************************************************************/

void swq_expr_node::MarkAsTimestamp()

{
    CPLAssert( eNodeType == SNT_CONSTANT );
    CPLAssert( field_type == SWQ_STRING );
    field_type = SWQ_TIMESTAMP;
}

/************************************************************************/
/*                         PushSubExpression()                          */
/************************************************************************/

void swq_expr_node::PushSubExpression( swq_expr_node *child )

{
    nSubExprCount++;
    papoSubExpr = static_cast<swq_expr_node **>(
        CPLRealloc( papoSubExpr, sizeof(void*) * nSubExprCount ));

    papoSubExpr[nSubExprCount-1] = child;
}

/************************************************************************/
/*                       ReverseSubExpressions()                        */
/************************************************************************/

void swq_expr_node::ReverseSubExpressions()

{
    for( int i = 0; i < nSubExprCount / 2; i++ )
    {
        std::swap(papoSubExpr[i], papoSubExpr[nSubExprCount - i - 1]);
    }
}

/************************************************************************/
/*                               Check()                                */
/*                                                                      */
/*      Check argument types, etc.                                      */
/************************************************************************/

swq_field_type
swq_expr_node::Check( swq_field_list *poFieldList,
                      int bAllowFieldsInSecondaryTables,
                      int bAllowMismatchTypeOnFieldComparison,
                      swq_custom_func_registrar* poCustomFuncRegistrar,
                      int nDepth )

{
    if( nDepth == 32 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too many recursion levels in expression");
        return SWQ_ERROR;
    }

/* -------------------------------------------------------------------- */
/*      Otherwise we take constants literally.                          */
/* -------------------------------------------------------------------- */
    if( eNodeType == SNT_CONSTANT )
        return field_type;

/* -------------------------------------------------------------------- */
/*      If this is intended to be a field definition, but has not       */
/*      yet been looked up, we do so now.                               */
/* -------------------------------------------------------------------- */
    if( eNodeType == SNT_COLUMN && field_index == -1 )
    {
        field_index =
            swq_identify_field( table_name, string_value, poFieldList,
                                &field_type, &table_index );

        if( field_index < 0 )
        {
            if( table_name )
                CPLError( CE_Failure, CPLE_AppDefined,
                      R"("%s"."%s" not recognised as an available field.)",
                      table_name, string_value );
            else
                CPLError( CE_Failure, CPLE_AppDefined,
                      "\"%s\" not recognised as an available field.",
                      string_value );

            return SWQ_ERROR;
        }

        if( !bAllowFieldsInSecondaryTables && table_index != 0 )
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Cannot use field '%s' of a secondary table in this context",
                string_value );
            return SWQ_ERROR;
        }
    }

    if( eNodeType == SNT_COLUMN )
        return field_type;

/* -------------------------------------------------------------------- */
/*      We are dealing with an operation - fetch the definition.        */
/* -------------------------------------------------------------------- */
    const swq_operation *poOp =
        (nOperation == SWQ_CUSTOM_FUNC && poCustomFuncRegistrar != nullptr ) ?
            poCustomFuncRegistrar->GetOperator(string_value) :
            swq_op_registrar::GetOperator(nOperation);

    if( poOp == nullptr )
    {
        if( nOperation == SWQ_CUSTOM_FUNC )
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Check(): Unable to find definition for operator %s.",
                      string_value );
        else
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Check(): Unable to find definition for operator %d.",
                      nOperation );
        return SWQ_ERROR;
    }

/* -------------------------------------------------------------------- */
/*      Check subexpressions first.                                     */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nSubExprCount; i++ )
    {
        if( papoSubExpr[i]->Check(poFieldList, bAllowFieldsInSecondaryTables,
                                  bAllowMismatchTypeOnFieldComparison,
                                  poCustomFuncRegistrar,
                                  nDepth + 1) == SWQ_ERROR )
            return SWQ_ERROR;
    }

/* -------------------------------------------------------------------- */
/*      Check this node.                                                */
/* -------------------------------------------------------------------- */
    field_type = poOp->pfnChecker( this, bAllowMismatchTypeOnFieldComparison );

    return field_type;
}

/************************************************************************/
/*                                Dump()                                */
/************************************************************************/

void swq_expr_node::Dump( FILE * fp, int depth )

{
    char spaces[60] = {};

    {
        int i = 0;  // Used after for.
        for( ; i < depth*2 && i < static_cast<int>(sizeof(spaces)) - 1; i++ )
            spaces[i] = ' ';
        spaces[i] = '\0';
    }

    if( eNodeType == SNT_COLUMN )
    {
        fprintf( fp, "%s  Field %d\n", spaces, field_index );
        return;
    }

    if( eNodeType == SNT_CONSTANT )
    {
        if( field_type == SWQ_INTEGER || field_type == SWQ_INTEGER64 ||
            field_type == SWQ_BOOLEAN )
            fprintf( fp, "%s  " CPL_FRMT_GIB "\n", spaces, int_value );
        else if( field_type == SWQ_FLOAT )
            fprintf( fp, "%s  %.15g\n", spaces, float_value );
        else if( field_type == SWQ_GEOMETRY )
        {
            if( geometry_value == nullptr )
                fprintf( fp, "%s  (null)\n", spaces );
            else
            {
                char* pszWKT = nullptr;
                geometry_value->exportToWkt(&pszWKT);
                fprintf( fp, "%s  %s\n", spaces, pszWKT );
                CPLFree(pszWKT);
            }
        }
        else
            fprintf( fp, "%s  %s\n", spaces, string_value );
        return;
    }

    CPLAssert( eNodeType == SNT_OPERATION );

    const swq_operation *op_def =
        swq_op_registrar::GetOperator( nOperation );
    if( op_def )
        fprintf( fp, "%s%s\n", spaces, op_def->pszName );
    else
        fprintf( fp, "%s%s\n", spaces, string_value );

    for( int i = 0; i < nSubExprCount; i++ )
        papoSubExpr[i]->Dump( fp, depth+1 );
}

/************************************************************************/
/*                       QuoteIfNecessary()                             */
/*                                                                      */
/*      Add quoting if necessary to unparse a string.                   */
/************************************************************************/

CPLString swq_expr_node::QuoteIfNecessary( const CPLString &osExpr,
                                           char chQuote )

{
    if( osExpr[0] == '_' )
        return Quote(osExpr, chQuote);
    if( osExpr == "*" )
        return osExpr;

    for( int i = 0; i < static_cast<int>(osExpr.size()); i++ )
    {
        char ch = osExpr[i];
        if( (!(isalnum(static_cast<int>(ch)) || ch == '_')) || ch == '.' )
        {
            return Quote(osExpr, chQuote);
        }
    }

    if( swq_is_reserved_keyword(osExpr) )
    {
        return Quote(osExpr, chQuote);
    }

    return osExpr;
}

/************************************************************************/
/*                               Quote()                                */
/*                                                                      */
/*      Add quoting necessary to unparse a string.                      */
/************************************************************************/

CPLString swq_expr_node::Quote( const CPLString &osTarget, char chQuote )

{
    CPLString osNew;

    osNew += chQuote;

    for( int i = 0; i < static_cast<int>(osTarget.size()); i++ )
    {
        if( osTarget[i] == chQuote )
        {
            osNew += chQuote;
            osNew += chQuote;
        }
        else
            osNew += osTarget[i];
    }
    osNew += chQuote;

    return osNew;
}

/************************************************************************/
/*                              Unparse()                               */
/************************************************************************/

char *swq_expr_node::Unparse( swq_field_list *field_list, char chColumnQuote )

{
    CPLString osExpr;

/* -------------------------------------------------------------------- */
/*      Handle constants.                                               */
/* -------------------------------------------------------------------- */
    if( eNodeType == SNT_CONSTANT )
    {
        if( is_null )
            return CPLStrdup("NULL");

        if( field_type == SWQ_INTEGER || field_type == SWQ_INTEGER64 ||
            field_type == SWQ_BOOLEAN )
            osExpr.Printf( CPL_FRMT_GIB, int_value );
        else if( field_type == SWQ_FLOAT )
        {
            osExpr.Printf( "%.15g", float_value );
            // Make sure this is interpreted as a floating point value
            // and not as an integer later.
            if( strchr(osExpr, '.') == nullptr && strchr(osExpr, 'e') == nullptr &&
                strchr(osExpr, 'E') == nullptr )
                osExpr += '.';
        }
        else
        {
            osExpr = Quote( string_value );
        }

        return CPLStrdup(osExpr);
    }

/* -------------------------------------------------------------------- */
/*      Handle columns.                                                 */
/* -------------------------------------------------------------------- */
    if( eNodeType == SNT_COLUMN )
    {
        if( field_list == nullptr )
        {
            if( table_name )
                osExpr.Printf(
                    "%s.%s",
                    QuoteIfNecessary(table_name, chColumnQuote).c_str(),
                    QuoteIfNecessary(string_value, chColumnQuote).c_str() );
            else
                osExpr.Printf(
                    "%s",
                    QuoteIfNecessary(string_value, chColumnQuote).c_str() );
        }
        else if( field_index != -1
            && table_index < field_list->table_count
            && table_index > 0 )
        {
            // We deliberately browse through the list starting from the end
            // This is for the case where the FID column exists both as
            // FID and then real_fid_name. We want real_fid_name to be used
            for( int i = field_list->count - 1; i >= 0; i-- )
            {
                if( field_list->table_ids[i] == table_index &&
                    field_list->ids[i] == field_index )
                {
                    osExpr.Printf( "%s.%s",
                                   QuoteIfNecessary(field_list->table_defs[table_index].table_name, chColumnQuote).c_str(),
                                   QuoteIfNecessary(field_list->names[i], chColumnQuote).c_str() );
                    break;
                }
            }
        }
        else if( field_index != -1 )
        {
            // We deliberately browse through the list starting from the end
            // This is for the case where the FID column exists both as
            // FID and then real_fid_name. We want real_fid_name to be used
            for( int i = field_list->count - 1; i >= 0; i-- )
            {
                if( field_list->table_ids[i] == table_index &&
                    field_list->ids[i] == field_index )
                {
                    osExpr.Printf( "%s", QuoteIfNecessary(field_list->names[i], chColumnQuote).c_str() );
                    break;
                }
            }
        }

        if( osExpr.empty() )
        {
            return CPLStrdup(CPLSPrintf("%c%c", chColumnQuote, chColumnQuote));
        }

        // The string is just alphanum and not a reserved SQL keyword,
        // no needs to quote and escape.
        return CPLStrdup(osExpr.c_str());
    }

/* -------------------------------------------------------------------- */
/*      Operation - start by unparsing all the subexpressions.          */
/* -------------------------------------------------------------------- */
    std::vector<char*> apszSubExpr;
    apszSubExpr.reserve(nSubExprCount);
    for( int i = 0; i < nSubExprCount; i++ )
        apszSubExpr.push_back( papoSubExpr[i]->Unparse(field_list, chColumnQuote) );

    osExpr = UnparseOperationFromUnparsedSubExpr(&apszSubExpr[0]);

/* -------------------------------------------------------------------- */
/*      cleanup subexpressions.                                         */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nSubExprCount; i++ )
        CPLFree( apszSubExpr[i] );

    return CPLStrdup( osExpr.c_str() );
}

/************************************************************************/
/*                  UnparseOperationFromUnparsedSubExpr()               */
/************************************************************************/

CPLString swq_expr_node::UnparseOperationFromUnparsedSubExpr(char** apszSubExpr)
{
    CPLString osExpr;

/* -------------------------------------------------------------------- */
/*      Put things together in a fashion depending on the operator.     */
/* -------------------------------------------------------------------- */
    const swq_operation *poOp =
        swq_op_registrar::GetOperator( nOperation );

    if( poOp == nullptr && nOperation != SWQ_CUSTOM_FUNC )
    {
        CPLAssert( false );
        return osExpr;
    }

    switch( nOperation )
    {
      // Binary infix operators.
      case SWQ_OR:
      case SWQ_AND:
      case SWQ_EQ:
      case SWQ_NE:
      case SWQ_GT:
      case SWQ_LT:
      case SWQ_GE:
      case SWQ_LE:
      case SWQ_LIKE:
      case SWQ_ILIKE:
      case SWQ_ADD:
      case SWQ_SUBTRACT:
      case SWQ_MULTIPLY:
      case SWQ_DIVIDE:
      case SWQ_MODULUS:
        CPLAssert( nSubExprCount >= 2 );
        if( papoSubExpr[0]->eNodeType == SNT_COLUMN ||
            papoSubExpr[0]->eNodeType == SNT_CONSTANT )
        {
            osExpr += apszSubExpr[0];
        }
        else
        {
            osExpr += "(";
            osExpr += apszSubExpr[0];
            osExpr += ")";
        }
        osExpr += " ";
        osExpr += poOp->pszName;
        osExpr += " ";
        if( papoSubExpr[1]->eNodeType == SNT_COLUMN ||
            papoSubExpr[1]->eNodeType == SNT_CONSTANT )
        {
            osExpr += apszSubExpr[1];
        }
        else
        {
            osExpr += "(";
            osExpr += apszSubExpr[1];
            osExpr += ")";
        }
        if( (nOperation == SWQ_LIKE || nOperation == SWQ_ILIKE) && nSubExprCount == 3 )
            osExpr += CPLSPrintf( " ESCAPE (%s)", apszSubExpr[2] );
        break;

      case SWQ_NOT:
        CPLAssert( nSubExprCount == 1 );
        osExpr.Printf( "NOT (%s)", apszSubExpr[0] );
        break;

      case SWQ_ISNULL:
        CPLAssert( nSubExprCount == 1 );
        osExpr.Printf( "%s IS NULL", apszSubExpr[0] );
        break;

      case SWQ_IN:
        osExpr.Printf( "%s IN (", apszSubExpr[0] );
        for( int i = 1; i < nSubExprCount; i++ )
        {
            if( i > 1 )
                osExpr += ",";
            osExpr += "(";
            osExpr += apszSubExpr[i];
            osExpr += ")";
        }
        osExpr += ")";
        break;

      case SWQ_BETWEEN:
        CPLAssert( nSubExprCount == 3 );
        osExpr.Printf( "%s %s (%s) AND (%s)",
                       apszSubExpr[0],
                       poOp->pszName,
                       apszSubExpr[1],
                       apszSubExpr[2] );
        break;

      case SWQ_CAST:
        osExpr = "CAST(";
        for( int i = 0; i < nSubExprCount; i++ )
        {
            if( i == 1 )
                osExpr += " AS ";
            else if( i > 2 )
                osExpr += ", ";

            const int nLen = static_cast<int>(strlen(apszSubExpr[i]));
            if( (i == 1 &&
                (apszSubExpr[i][0] == '\'' &&
                 nLen > 2 && apszSubExpr[i][nLen-1] == '\'')) ||
                (i == 2 && EQUAL(apszSubExpr[1], "'GEOMETRY")) )
            {
                apszSubExpr[i][nLen-1] = '\0';
                osExpr += apszSubExpr[i] + 1;
            }
            else
                osExpr += apszSubExpr[i];

            if( i == 1 && nSubExprCount > 2)
                osExpr += "(";
            else if( i > 1 && i == nSubExprCount - 1 )
                osExpr += ")";
        }
        osExpr += ")";
        break;

      default: // function style.
        if( nOperation != SWQ_CUSTOM_FUNC )
            osExpr.Printf( "%s(", poOp->pszName );
        else
            osExpr.Printf( "%s(", string_value );
        for( int i = 0; i < nSubExprCount; i++ )
        {
            if( i > 0 )
                osExpr += ",";
            osExpr += "(";
            osExpr += apszSubExpr[i];
            osExpr += ")";
        }
        osExpr += ")";
        break;
    }

    return osExpr;
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

swq_expr_node *swq_expr_node::Clone()
{
    swq_expr_node* poRetNode = new swq_expr_node();

    poRetNode->eNodeType = eNodeType;
    poRetNode->field_type = field_type;
    if( eNodeType == SNT_OPERATION )
    {
        poRetNode->nOperation = nOperation;
        poRetNode->nSubExprCount = nSubExprCount;
        poRetNode->papoSubExpr = static_cast<swq_expr_node **>(
                CPLMalloc( sizeof(void*) * nSubExprCount ));
        for( int i = 0; i < nSubExprCount; i++ )
            poRetNode->papoSubExpr[i] = papoSubExpr[i]->Clone();
    }
    else if( eNodeType == SNT_COLUMN )
    {
        poRetNode->field_index = field_index;
        poRetNode->table_index = table_index;
        poRetNode->table_name = table_name ? CPLStrdup(table_name) : nullptr;
    }
    else if( eNodeType == SNT_CONSTANT )
    {
        poRetNode->is_null = is_null;
        poRetNode->int_value = int_value;
        poRetNode->float_value = float_value;
        if( geometry_value )
            poRetNode->geometry_value = geometry_value->clone();
        else
            poRetNode->geometry_value = nullptr;
    }
    poRetNode->string_value = string_value ? CPLStrdup(string_value) : nullptr;
    return poRetNode;
}

/************************************************************************/
/*                              Evaluate()                              */
/************************************************************************/

swq_expr_node *swq_expr_node::Evaluate( swq_field_fetcher pfnFetcher,
                                        void *pRecord )

{
    return Evaluate(pfnFetcher, pRecord, 0);
}

swq_expr_node *swq_expr_node::Evaluate( swq_field_fetcher pfnFetcher,
                                        void *pRecord, int nRecLevel )

{
    swq_expr_node *poRetNode = nullptr;
    if( nRecLevel == 32 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Too many recursion levels in expression");
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Duplicate ourselves if we are already a constant.               */
/* -------------------------------------------------------------------- */
    if( eNodeType == SNT_CONSTANT )
    {
        return Clone();
    }

/* -------------------------------------------------------------------- */
/*      If this is a field value from a record, fetch and return it.    */
/* -------------------------------------------------------------------- */
    if( eNodeType == SNT_COLUMN )
    {
        return pfnFetcher( this, pRecord );
    }

/* -------------------------------------------------------------------- */
/*      This is an operation, collect the arguments keeping track of    */
/*      which we will need to free.                                     */
/* -------------------------------------------------------------------- */
    std::vector<swq_expr_node*> apoValues;
    std::vector<int> anValueNeedsFree;
    bool bError = false;
    apoValues.reserve(nSubExprCount);
    for( int i = 0; i < nSubExprCount && !bError; i++ )
    {
        if( papoSubExpr[i]->eNodeType == SNT_CONSTANT )
        {
            // avoid duplication.
            apoValues.push_back( papoSubExpr[i] );
            anValueNeedsFree.push_back( FALSE );
        }
        else
        {
            swq_expr_node* poSubExprVal =
                papoSubExpr[i]->Evaluate(pfnFetcher, pRecord, nRecLevel + 1);
            if( poSubExprVal == nullptr )
                bError = true;
            else
            {
                apoValues.push_back(poSubExprVal);
                anValueNeedsFree.push_back( TRUE );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Fetch the operator definition and function.                     */
/* -------------------------------------------------------------------- */
    if( !bError )
    {
        const swq_operation *poOp =
            swq_op_registrar::GetOperator( nOperation );
        if( poOp == nullptr )
        {
            if( nOperation == SWQ_CUSTOM_FUNC )
                CPLError( CE_Failure, CPLE_AppDefined,
                        "Evaluate(): Unable to find definition for operator %s.",
                        string_value );
            else
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Evaluate(): Unable to find definition for operator %d.",
                    nOperation );
            poRetNode = nullptr;
        }
        else
            poRetNode = poOp->pfnEvaluator( this, &(apoValues[0]) );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < static_cast<int>(apoValues.size()); i++ )
    {
        if( anValueNeedsFree[i] )
            delete apoValues[i];
    }

    return poRetNode;
}

/************************************************************************/
/*                      ReplaceBetweenByGEAndLERecurse()                */
/************************************************************************/

void swq_expr_node::ReplaceBetweenByGEAndLERecurse()
{
    if( eNodeType != SNT_OPERATION )
        return;

    if( nOperation != SWQ_BETWEEN )
    {
        for( int i = 0; i < nSubExprCount; i++ )
            papoSubExpr[i]->ReplaceBetweenByGEAndLERecurse();
        return;
    }

    if( nSubExprCount != 3 )
        return;

    swq_expr_node* poExpr0 = papoSubExpr[0];
    swq_expr_node* poExpr1 = papoSubExpr[1];
    swq_expr_node* poExpr2 = papoSubExpr[2];

    nSubExprCount = 2;
    nOperation = SWQ_AND;
    papoSubExpr[0] = new swq_expr_node(SWQ_GE);
    papoSubExpr[0]->PushSubExpression(poExpr0);
    papoSubExpr[0]->PushSubExpression(poExpr1);
    papoSubExpr[1] = new swq_expr_node(SWQ_LE);
    papoSubExpr[1]->PushSubExpression(poExpr0->Clone());
    papoSubExpr[1]->PushSubExpression(poExpr2);
}

#endif  // #ifndef DOXYGEN_SKIP
