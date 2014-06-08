/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: Implementation of the swq_op_registrar class used to 
 *          represent operations possible in an SQL expression.
 * Author: Frank Warmerdam <warmerdam@pobox.com>
 * 
 ******************************************************************************
 * Copyright (C) 2010 Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "swq.h"

static swq_field_type SWQColumnFuncChecker( swq_expr_node *poNode );

static const swq_operation swq_apsOperations[] =
{
    { "OR", SWQ_OR, SWQGeneralEvaluator, SWQGeneralChecker },
    { "AND", SWQ_AND, SWQGeneralEvaluator, SWQGeneralChecker },
    { "NOT", SWQ_NOT , SWQGeneralEvaluator, SWQGeneralChecker },
    { "=", SWQ_EQ , SWQGeneralEvaluator, SWQGeneralChecker },
    { "<>", SWQ_NE , SWQGeneralEvaluator, SWQGeneralChecker },
    { ">=", SWQ_GE , SWQGeneralEvaluator, SWQGeneralChecker },
    { "<=", SWQ_LE , SWQGeneralEvaluator, SWQGeneralChecker },
    { "<", SWQ_LT , SWQGeneralEvaluator, SWQGeneralChecker },
    { ">", SWQ_GT , SWQGeneralEvaluator, SWQGeneralChecker },
    { "LIKE", SWQ_LIKE , SWQGeneralEvaluator, SWQGeneralChecker },
    { "IS NULL", SWQ_ISNULL , SWQGeneralEvaluator, SWQGeneralChecker },
    { "IN", SWQ_IN , SWQGeneralEvaluator, SWQGeneralChecker },
    { "BETWEEN", SWQ_BETWEEN , SWQGeneralEvaluator, SWQGeneralChecker },
    { "+", SWQ_ADD , SWQGeneralEvaluator, SWQGeneralChecker },
    { "-", SWQ_SUBTRACT , SWQGeneralEvaluator, SWQGeneralChecker },
    { "*", SWQ_MULTIPLY , SWQGeneralEvaluator, SWQGeneralChecker },
    { "/", SWQ_DIVIDE , SWQGeneralEvaluator, SWQGeneralChecker },
    { "%", SWQ_MODULUS , SWQGeneralEvaluator, SWQGeneralChecker },
    { "CONCAT", SWQ_CONCAT , SWQGeneralEvaluator, SWQGeneralChecker },
    { "SUBSTR", SWQ_SUBSTR , SWQGeneralEvaluator, SWQGeneralChecker },
    { "HSTORE_GET_VALUE", SWQ_HSTORE_GET_VALUE , SWQGeneralEvaluator, SWQGeneralChecker },

    { "AVG", SWQ_AVG, SWQGeneralEvaluator, SWQColumnFuncChecker },
    { "MIN", SWQ_MIN, SWQGeneralEvaluator, SWQColumnFuncChecker },
    { "MAX", SWQ_MAX, SWQGeneralEvaluator, SWQColumnFuncChecker },
    { "COUNT", SWQ_COUNT, SWQGeneralEvaluator, SWQColumnFuncChecker },
    { "SUM", SWQ_SUM, SWQGeneralEvaluator, SWQColumnFuncChecker },

    { "CAST", SWQ_CAST, SWQCastEvaluator, SWQCastChecker }
};

#define N_OPERATIONS (sizeof(swq_apsOperations) / sizeof(swq_apsOperations[0]))

/************************************************************************/
/*                            GetOperator()                             */
/************************************************************************/

const swq_operation *swq_op_registrar::GetOperator( const char *pszName )

{
    unsigned int i;
    for( i = 0; i < N_OPERATIONS; i++ )
    {
        if( EQUAL(pszName,swq_apsOperations[i].pszName) )
            return &(swq_apsOperations[i]);
    }

    return NULL;
}

/************************************************************************/
/*                            GetOperator()                             */
/************************************************************************/

const swq_operation *swq_op_registrar::GetOperator( swq_op eOperator )

{
    unsigned int i;

    for( i = 0; i < N_OPERATIONS; i++ )
    {
        if( eOperator == swq_apsOperations[i].eOperation )
            return &(swq_apsOperations[i]);
    }

    return NULL;
}

/************************************************************************/
/*                        SWQColumnFuncChecker()                        */
/*                                                                      */
/*      Column summary functions are not legal in any context except    */
/*      as a root operator on column definitions.  They are removed     */
/*      from this tree before checking so we just need to issue an      */
/*      error if they are used in any other context.                    */
/************************************************************************/

static swq_field_type SWQColumnFuncChecker( swq_expr_node *poNode )
{
    const swq_operation *poOp =
            swq_op_registrar::GetOperator((swq_op)poNode->nOperation);
    CPLError( CE_Failure, CPLE_AppDefined,
              "Column Summary Function '%s' found in an inappropriate context.",
              (poOp) ? poOp->pszName : "" );
    return SWQ_ERROR;
}
