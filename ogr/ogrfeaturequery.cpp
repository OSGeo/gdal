/******************************************************************************
 * $Id$
 * 
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of simple SQL WHERE style attributes queries
 *           for OGRFeatures.  
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.3  2001/07/19 18:25:07  warmerda
 * expanded tabs
 *
 * Revision 1.2  2001/07/18 05:03:05  warmerda
 * added CPL_CVSID
 *
 * Revision 1.1  2001/06/19 15:46:41  warmerda
 * New
 *
 */

#include <assert.h>
#include "ogr_feature.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

CPL_C_START
#include "swq.h"
CPL_C_END

/************************************************************************/
/*                          OGRFeatureQuery()                           */
/************************************************************************/

OGRFeatureQuery::OGRFeatureQuery()

{
    poTargetDefn = NULL;
    pSWQExpr = NULL;
}

/************************************************************************/
/*                          ~OGRFeatureQuery()                          */
/************************************************************************/

OGRFeatureQuery::~OGRFeatureQuery()

{
    if( pSWQExpr != NULL )
        swq_expr_free( (swq_expr *) pSWQExpr );
}

/************************************************************************/
/*                                Parse                                 */
/************************************************************************/

OGRErr OGRFeatureQuery::Compile( OGRFeatureDefn *poDefn, 
                                 const char * pszExpression )

{
/* -------------------------------------------------------------------- */
/*      Clear any existing expression.                                  */
/* -------------------------------------------------------------------- */
    if( pSWQExpr != NULL )
        swq_expr_free( (swq_expr *) pSWQExpr );

/* -------------------------------------------------------------------- */
/*      Build list of fields.                                           */
/* -------------------------------------------------------------------- */
    char        **papszFieldNames;
    swq_field_type *paeFieldTypes;
    int         iField;

    papszFieldNames = (char **) 
        CPLMalloc(sizeof(char *) * poDefn->GetFieldCount() );
    paeFieldTypes = (swq_field_type *) 
        CPLMalloc(sizeof(swq_field_type) * poDefn->GetFieldCount() );

    for( iField = 0; iField < poDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn    *poField = poDefn->GetFieldDefn( iField );

        papszFieldNames[iField] = (char *) poField->GetNameRef();

        switch( poField->GetType() )
        {
          case OFTInteger:
            paeFieldTypes[iField] = SWQ_INTEGER;
            break;

          case OFTReal:
            paeFieldTypes[iField] = SWQ_FLOAT;
            break;

          case OFTString:
            paeFieldTypes[iField] = SWQ_STRING;
            break;

          default:
            paeFieldTypes[iField] = SWQ_OTHER;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to parse.                                                   */
/* -------------------------------------------------------------------- */
    const char  *pszError;
    OGRErr      eErr = OGRERR_NONE;

    poTargetDefn = poDefn;
    pszError = swq_expr_compile( pszExpression, poDefn->GetFieldCount(),
                                 papszFieldNames, paeFieldTypes, 
                                 (swq_expr **) &pSWQExpr );
    if( pszError != NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s", pszError );
        eErr = OGRERR_CORRUPT_DATA;
        pSWQExpr = NULL;
    }

    CPLFree( papszFieldNames );
    CPLFree( paeFieldTypes );

    return eErr;
}

/************************************************************************/
/*                      OGRFeatureQueryEvaluator()                      */
/************************************************************************/

static int OGRFeatureQueryEvaluator( swq_field_op *op, OGRFeature *poFeature )

{
    OGRField    *psField = poFeature->GetRawFieldRef( op->field_index );

    switch( op->field_type )
    {
      case SWQ_INTEGER:
        switch( op->operation )
        {
          case SWQ_EQ:
            return psField->Integer == op->int_value;
          case SWQ_NE:
            return psField->Integer != op->int_value;
          case SWQ_LT:
            return psField->Integer < op->int_value;
          case SWQ_GT:
            return psField->Integer > op->int_value;
          case SWQ_LE:
            return psField->Integer <= op->int_value;
          case SWQ_GE:
            return psField->Integer >= op->int_value;
          default:
            assert( FALSE );
            return FALSE;
        }

      case SWQ_FLOAT:
        switch( op->operation )
        {
          case SWQ_EQ:
            return psField->Real == op->float_value;
          case SWQ_NE:
            return psField->Real != op->float_value;
          case SWQ_LT:
            return psField->Real < op->float_value;
          case SWQ_GT:
            return psField->Real > op->float_value;
          case SWQ_LE:
            return psField->Real <= op->float_value;
          case SWQ_GE:
            return psField->Real >= op->float_value;
          default:
            assert( FALSE );
            return FALSE;
        }

      case SWQ_STRING:
        switch( op->operation )
        {
          case SWQ_EQ:
            return EQUAL(psField->String,op->string_value);
          case SWQ_NE:
            return !EQUAL(psField->String,op->string_value);
          default:
            assert( FALSE );
            return FALSE;
        }

      default:
        assert( FALSE );
        return FALSE;
    }
}

/************************************************************************/
/*                              Evaluate()                              */
/************************************************************************/

int OGRFeatureQuery::Evaluate( OGRFeature *poFeature )

{
    if( pSWQExpr == NULL )
        return FALSE;

    return swq_expr_evaluate( (swq_expr *) pSWQExpr, 
                              (swq_op_evaluator) OGRFeatureQueryEvaluator, 
                              (void *) poFeature );
}
