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
 * Revision 1.6  2002/04/29 19:31:55  warmerda
 * added support for FID field
 *
 * Revision 1.5  2002/04/19 20:46:06  warmerda
 * added [NOT] IN, [NOT] LIKE and IS [NOT] NULL support
 *
 * Revision 1.4  2001/10/25 16:41:01  danmo
 * Fixed OGRFeatureQueryEvaluator() crash with string fields with unset value
 *
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
    int         nFieldCount = poDefn->GetFieldCount()+1;

    papszFieldNames = (char **) 
        CPLMalloc(sizeof(char *) * nFieldCount );
    paeFieldTypes = (swq_field_type *) 
        CPLMalloc(sizeof(swq_field_type) * nFieldCount );

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

    papszFieldNames[nFieldCount-1] = "FID";
    paeFieldTypes[nFieldCount-1] = SWQ_INTEGER;

/* -------------------------------------------------------------------- */
/*      Try to parse.                                                   */
/* -------------------------------------------------------------------- */
    const char  *pszError;
    OGRErr      eErr = OGRERR_NONE;

    poTargetDefn = poDefn;
    pszError = swq_expr_compile( pszExpression, nFieldCount,
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
    OGRField sFID;
    OGRField *psField;

    if( op->field_index == poFeature->GetDefnRef()->GetFieldCount() )
    {
        sFID.Integer = poFeature->GetFID();
        psField = &sFID;
    }
    else
        psField = poFeature->GetRawFieldRef( op->field_index );

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
          case SWQ_ISNULL:
            return !poFeature->IsFieldSet( op->field_index );

          case SWQ_IN:
          {
              const char *pszSrc;
              
              pszSrc = op->string_value;
              while( *pszSrc != '\0' )
              {
                  if( atoi(pszSrc) == psField->Integer )
                      return TRUE;
                  pszSrc += strlen(pszSrc) + 1;
              }

              return FALSE;
          }

          default:
            CPLDebug( "OGRFeatureQuery", 
                      "Illegal operation (%d) on integer field.",
                      op->operation );
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
          case SWQ_ISNULL:
            return !poFeature->IsFieldSet( op->field_index );
          case SWQ_IN:
          {
              const char *pszSrc;
              
              pszSrc = op->string_value;
              while( *pszSrc != '\0' )
              {
                  if( atof(pszSrc) == psField->Integer )
                      return TRUE;
                  pszSrc += strlen(pszSrc) + 1;
              }

              return FALSE;
          }

          default:
            CPLDebug( "OGRFeatureQuery", 
                      "Illegal operation (%d) on float field.",
                      op->operation );
            return FALSE;
        }

      case SWQ_STRING:
        switch( op->operation )
        {
          case SWQ_EQ:
            if (psField->Set.nMarker1 == OGRUnsetMarker
                && psField->Set.nMarker2 == OGRUnsetMarker )
            {
                return (op->string_value[0] == '\0');
            }
            else
            {
                return EQUAL(psField->String,op->string_value);
            }
          case SWQ_NE:
            if (psField->Set.nMarker1 == OGRUnsetMarker
                && psField->Set.nMarker2 == OGRUnsetMarker )
            {
                return (op->string_value[0] != '\0');
            }
            else
            {
                return !EQUAL(psField->String,op->string_value);
            }

          case SWQ_ISNULL:
            return !poFeature->IsFieldSet( op->field_index );

          case SWQ_LIKE:
            if (psField->Set.nMarker1 != OGRUnsetMarker
                || psField->Set.nMarker2 != OGRUnsetMarker )
                return swq_test_like(psField->String, op->string_value);
            else
                return FALSE;

          case SWQ_IN:
          {
              const char *pszSrc;

              if( !poFeature->IsFieldSet(op->field_index) )
                  return FALSE;
              
              pszSrc = op->string_value;
              while( *pszSrc != '\0' )
              {
                  if( EQUAL(pszSrc,psField->String) )
                      return TRUE;
                  pszSrc += strlen(pszSrc) + 1;
              }

              return FALSE;
          }

          default:
            CPLDebug( "OGRFeatureQuery", 
                      "Illegal operation (%d) on string field.",
                      op->operation );
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
