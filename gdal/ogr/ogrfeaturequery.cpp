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
 ****************************************************************************/

#include <assert.h>
#include "swq.h"
#include "ogr_feature.h"
#include "ogr_p.h"
#include "ogr_attrind.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*     Support for special attributes (feature query and selection)     */
/************************************************************************/

const char* SpecialFieldNames[SPECIAL_FIELD_COUNT] 
= {"FID", "OGR_GEOMETRY", "OGR_STYLE", "OGR_GEOM_WKT", "OGR_GEOM_AREA"};
const swq_field_type SpecialFieldTypes[SPECIAL_FIELD_COUNT] 
= {SWQ_INTEGER, SWQ_STRING, SWQ_STRING, SWQ_STRING, SWQ_FLOAT};

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
    delete (swq_expr_node *) pSWQExpr;
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
    {
        delete (swq_expr_node *) pSWQExpr;
        pSWQExpr = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Build list of fields.                                           */
/* -------------------------------------------------------------------- */
    char        **papszFieldNames;
    swq_field_type *paeFieldTypes;
    int         iField;
    int         nFieldCount = poDefn->GetFieldCount() + SPECIAL_FIELD_COUNT;

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

          case OFTDate:
          case OFTTime:
          case OFTDateTime:
            paeFieldTypes[iField] = SWQ_TIMESTAMP;
            break;

          default:
            paeFieldTypes[iField] = SWQ_OTHER;
            break;
        }
    }

    iField = 0;
    while (iField < SPECIAL_FIELD_COUNT)
    {
        papszFieldNames[poDefn->GetFieldCount() + iField] = (char *) SpecialFieldNames[iField];
        paeFieldTypes[poDefn->GetFieldCount() + iField] = SpecialFieldTypes[iField];
        ++iField;
    }

/* -------------------------------------------------------------------- */
/*      Try to parse.                                                   */
/* -------------------------------------------------------------------- */
    OGRErr      eErr = OGRERR_NONE;
    CPLErr      eCPLErr;

    poTargetDefn = poDefn;
    eCPLErr = swq_expr_compile( pszExpression, nFieldCount,
                                papszFieldNames, paeFieldTypes, 
                                (swq_expr_node **) &pSWQExpr );
    if( eCPLErr != CE_None )
    {
        eErr = OGRERR_CORRUPT_DATA;
        pSWQExpr = NULL;
    }

    CPLFree( papszFieldNames );
    CPLFree( paeFieldTypes );


    return eErr;
}

/************************************************************************/
/*                         OGRFeatureFetcher()                          */
/************************************************************************/

static swq_expr_node *OGRFeatureFetcher( swq_expr_node *op, void *pFeatureIn )

{
    OGRFeature *poFeature = (OGRFeature *) pFeatureIn;
    swq_expr_node *poRetNode = NULL;

    switch( op->field_type )
    {
      case SWQ_INTEGER:
      case SWQ_BOOLEAN:
        poRetNode = new swq_expr_node( 
            poFeature->GetFieldAsInteger(op->field_index) );
        break;

      case SWQ_FLOAT:
        poRetNode = new swq_expr_node( 
            poFeature->GetFieldAsDouble(op->field_index) );
        break;
        
      default:
        poRetNode = new swq_expr_node( 
            poFeature->GetFieldAsString(op->field_index) );
        break;
    }

    poRetNode->is_null = !(poFeature->IsFieldSet(op->field_index));

    return poRetNode;
}

/************************************************************************/
/*                              Evaluate()                              */
/************************************************************************/

int OGRFeatureQuery::Evaluate( OGRFeature *poFeature )

{
    if( pSWQExpr == NULL )
        return FALSE;

    swq_expr_node *poResult;

    poResult = ((swq_expr_node *) pSWQExpr)->Evaluate( OGRFeatureFetcher,
                                                       (void *) poFeature );

    if( poResult == NULL )
        return FALSE;

    CPLAssert( poResult->field_type == SWQ_BOOLEAN );

    int bLogicalResult = poResult->int_value;

    delete poResult;

    return bLogicalResult;
}

/************************************************************************/
/*                       EvaluateAgainstIndices()                       */
/*                                                                      */
/*      Attempt to return a list of FIDs matching the given             */
/*      attribute query conditions utilizing attribute indices.         */
/*      Returns NULL if the result cannot be computed from the          */
/*      available indices, or an "OGRNullFID" terminated list of        */
/*      FIDs if it can.                                                 */
/*                                                                      */
/*      For now we only support equality tests on a single indexed      */
/*      attribute field.  Eventually we should make this support        */
/*      multi-part queries with ranges.                                 */
/************************************************************************/

static int CompareLong(const void *a, const void *b)
{
	return (*(const long *)a) - (*(const long *)b);
}

long *OGRFeatureQuery::EvaluateAgainstIndices( OGRLayer *poLayer, 
                                               OGRErr *peErr )

{
    swq_expr_node *psExpr = (swq_expr_node *) pSWQExpr;
    OGRAttrIndex *poIndex;

    if( peErr != NULL )
        *peErr = OGRERR_NONE;

/* -------------------------------------------------------------------- */
/*      Does the expression meet our requirements?  Do we have an       */
/*      index on the targetted field?                                   */
/* -------------------------------------------------------------------- */
    if( psExpr == NULL 
        || psExpr->eNodeType != SNT_OPERATION
        || !(psExpr->nOperation == SWQ_EQ || psExpr->nOperation == SWQ_IN) 
        || poLayer->GetIndex() == NULL
        || psExpr->nSubExprCount < 2 )
        return NULL;

    swq_expr_node *poColumn = psExpr->papoSubExpr[0];
    swq_expr_node *poValue = psExpr->papoSubExpr[1];
    
    if( poColumn->eNodeType != SNT_COLUMN
        || poValue->eNodeType != SNT_CONSTANT )
        return NULL;

    poIndex = poLayer->GetIndex()->GetFieldIndex( poColumn->field_index );
    if( poIndex == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      OK, we have an index, now we need to query it.                  */
/* -------------------------------------------------------------------- */
    OGRField sValue;
    OGRFieldDefn *poFieldDefn;

    poFieldDefn = poLayer->GetLayerDefn()->GetFieldDefn(poColumn->field_index);

/* -------------------------------------------------------------------- */
/*      Handle the case of an IN operation.                             */
/* -------------------------------------------------------------------- */
    if (psExpr->nOperation == SWQ_IN)
    {
        int nFIDCount = 0, nLength;
        long *panFIDs = NULL;
        int iIN;

        for( iIN = 1; iIN < psExpr->nSubExprCount; iIN++ )
        {
            switch( poFieldDefn->GetType() )
            {
              case OFTInteger:
                if (psExpr->papoSubExpr[iIN]->field_type == SWQ_FLOAT)
                    sValue.Integer = (int) psExpr->papoSubExpr[iIN]->float_value;
                else
                    sValue.Integer = psExpr->papoSubExpr[iIN]->int_value;
                break;

              case OFTReal:
                sValue.Real = psExpr->papoSubExpr[iIN]->float_value;
                break;

              case OFTString:
                sValue.String = psExpr->papoSubExpr[iIN]->string_value;
                break;

              default:
                CPLAssert( FALSE );
                return NULL;
            }

            panFIDs = poIndex->GetAllMatches( &sValue, panFIDs, &nFIDCount, &nLength );
        }

        if (nFIDCount > 1)
        {
            /* the returned FIDs are expected to be in sorted order */
            qsort(panFIDs, nFIDCount, sizeof(long), CompareLong);
        }
        return panFIDs;
    }

/* -------------------------------------------------------------------- */
/*      Handle equality test.                                           */
/* -------------------------------------------------------------------- */
    switch( poFieldDefn->GetType() )
    {
      case OFTInteger:
        if (poValue->field_type == SWQ_FLOAT)
            sValue.Integer = (int) poValue->float_value;
        else
            sValue.Integer = poValue->int_value;
        break;
        
      case OFTReal:
        sValue.Real = poValue->float_value;
        break;
        
      case OFTString:
        sValue.String = poValue->string_value;
        break;

      default:
        CPLAssert( FALSE );
        return NULL;
    }

    int nFIDCount = 0, nLength = 0;
    long *panFIDs = poIndex->GetAllMatches( &sValue, NULL, &nFIDCount, &nLength );
    if (nFIDCount > 1)
    {
        /* the returned FIDs are expected to be in sorted order */
        qsort(panFIDs, nFIDCount, sizeof(long), CompareLong);
    }
    return panFIDs;
}

/************************************************************************/
/*                         OGRFieldCollector()                          */
/*                                                                      */
/*      Helper function for recursing through tree to satisfy           */
/*      GetUsedFields().                                                */
/************************************************************************/

char **OGRFeatureQuery::FieldCollector( void *pBareOp, 
                                        char **papszList )

{
    swq_expr_node *op = (swq_expr_node *) pBareOp;

/* -------------------------------------------------------------------- */
/*      References to tables other than the primarily are currently     */
/*      unsupported. Error out.                                         */
/* -------------------------------------------------------------------- */
    if( op->eNodeType == SNT_COLUMN )
    {
        if( op->table_index != 0 )
        {
            CSLDestroy( papszList );
            return NULL;
        }

/* -------------------------------------------------------------------- */
/*      Add the field name into our list if it is not already there.    */
/* -------------------------------------------------------------------- */
        const char *pszFieldName;

        if( op->field_index >= poTargetDefn->GetFieldCount()
            && op->field_index < poTargetDefn->GetFieldCount() + SPECIAL_FIELD_COUNT) 
            pszFieldName = SpecialFieldNames[op->field_index];
        else if( op->field_index >= 0 
                 && op->field_index < poTargetDefn->GetFieldCount() )
            pszFieldName = 
                poTargetDefn->GetFieldDefn(op->field_index)->GetNameRef();
        else
        {
            CSLDestroy( papszList );
            return NULL;
        }
        
        if( CSLFindString( papszList, pszFieldName ) == -1 )
            papszList = CSLAddString( papszList, pszFieldName );
    }

/* -------------------------------------------------------------------- */
/*      Add in fields from subexpressions.                              */
/* -------------------------------------------------------------------- */
    if( op->eNodeType == SNT_OPERATION )
    {
        for( int iSubExpr = 0; iSubExpr < op->nSubExprCount; iSubExpr++ )
        {
            papszList = FieldCollector( op->papoSubExpr[iSubExpr], papszList );
        }
    }

    return papszList;
}

/************************************************************************/
/*                           GetUsedFields()                            */
/************************************************************************/

/**
 * Returns lists of fields in expression.
 *
 * All attribute fields are used in the expression of this feature
 * query are returned as a StringList of field names.  This function would
 * primarily be used within drivers to recognise special case conditions
 * depending only on attribute fields that can be very efficiently 
 * fetched. 
 *
 * NOTE: If any fields in the expression are from tables other than the
 * primary table then NULL is returned indicating an error.  In succesful
 * use, no non-empty expression should return an empty list.
 *
 * @return list of field names.  Free list with CSLDestroy() when no longer
 * required.
 */

char **OGRFeatureQuery::GetUsedFields( )

{
    if( pSWQExpr == NULL )
        return NULL;

    
    return FieldCollector( pSWQExpr, NULL );
}



