/******************************************************************************
 *
 * Component: OGR SQL Engine
 * Purpose: Implementation of the swq_op_registrar class used to 
 *          represent operations possible in an SQL expression.
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

#include "cpl_conv.h"
#include "cpl_multiproc.h"
#include "swq.h"
#include <vector>

static void *hOperationsMutex = NULL;
static std::vector<swq_operation*>* papoOperations = NULL;

/************************************************************************/
/*                            GetOperator()                             */
/************************************************************************/

const swq_operation *swq_op_registrar::GetOperator( const char *pszName )

{
    unsigned int i;

    Initialize();

    for( i = 0; i < papoOperations->size(); i++ )
    {
        if( EQUAL(pszName,(*papoOperations)[i]->osName.c_str()) )
            return (*papoOperations)[i];
    }

    return NULL;
}

/************************************************************************/
/*                            GetOperator()                             */
/************************************************************************/

const swq_operation *swq_op_registrar::GetOperator( swq_op eOperator )

{
    unsigned int i;

    Initialize();

    for( i = 0; i < papoOperations->size(); i++ )
    {
        if( eOperator == (*papoOperations)[i]->eOperation )
            return (*papoOperations)[i];
    }

    return NULL;
}


/************************************************************************/
/*                            AddOperator()                             */
/************************************************************************/

void swq_op_registrar::AddOperator( const char *pszName, swq_op eOpCode,
                                    swq_op_evaluator pfnEvaluator,
                                    swq_op_checker pfnChecker )

{
    if( GetOperator( pszName ) != NULL )
        return;

    if( pfnEvaluator == NULL )
        pfnEvaluator = SWQGeneralEvaluator;
    if( pfnChecker == NULL )
        pfnChecker = SWQGeneralChecker;

    swq_operation *poOp = new swq_operation();

    poOp->eOperation = eOpCode;
    poOp->osName = pszName;
    poOp->pfnEvaluator = pfnEvaluator;
    poOp->pfnChecker = pfnChecker;

    papoOperations->push_back( poOp );
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
              (poOp) ? poOp->osName.c_str() : "" );
    return SWQ_ERROR;
}


/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

void swq_op_registrar::Initialize()

{
    CPLMutexHolderD( &hOperationsMutex );

    if( papoOperations )
        return;

    papoOperations = new std::vector<swq_operation*>;

    AddOperator( "OR", SWQ_OR );
    AddOperator( "AND", SWQ_AND );
    AddOperator( "NOT", SWQ_NOT );
    AddOperator( "=", SWQ_EQ );
    AddOperator( "<>", SWQ_NE );
    AddOperator( ">=", SWQ_GE );
    AddOperator( "<=", SWQ_LE );
    AddOperator( "<", SWQ_LT );
    AddOperator( ">", SWQ_GT );
    AddOperator( "LIKE", SWQ_LIKE );
    AddOperator( "IS NULL", SWQ_ISNULL );
    AddOperator( "IN", SWQ_IN );
    AddOperator( "BETWEEN", SWQ_BETWEEN );
    AddOperator( "+", SWQ_ADD );
    AddOperator( "-", SWQ_SUBTRACT );
    AddOperator( "*", SWQ_MULTIPLY );
    AddOperator( "/", SWQ_DIVIDE );
    AddOperator( "%", SWQ_MODULUS );
    AddOperator( "CONCAT", SWQ_CONCAT );
    AddOperator( "SUBSTR", SWQ_SUBSTR );

    AddOperator( "AVG", SWQ_AVG, NULL, SWQColumnFuncChecker );
    AddOperator( "MIN", SWQ_MIN, NULL, SWQColumnFuncChecker );
    AddOperator( "MAX", SWQ_MAX, NULL, SWQColumnFuncChecker );
    AddOperator( "COUNT", SWQ_COUNT, NULL, SWQColumnFuncChecker );
    AddOperator( "SUM", SWQ_SUM, NULL, SWQColumnFuncChecker );

    AddOperator( "CAST", SWQ_CAST, SWQCastEvaluator, SWQCastChecker );
}

/************************************************************************/
/*                            DeInitialize()                            */
/************************************************************************/

void swq_op_registrar::DeInitialize()

{
    {
        CPLMutexHolderD( &hOperationsMutex );
        
        if( papoOperations != NULL)
        {
            for( unsigned int i=0; i < papoOperations->size(); i++ )
                delete (*papoOperations)[i];
            
            delete papoOperations;
            papoOperations = NULL;
        }
    }

    CPLDestroyMutex( hOperationsMutex );
    hOperationsMutex = NULL;
}
