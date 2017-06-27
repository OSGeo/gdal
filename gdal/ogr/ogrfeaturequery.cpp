/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implementation of simple SQL WHERE style attributes queries
 *           for OGRFeatures.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_feature.h"
#include "swq.h"

#include <cstddef>
#include <cstdlib>
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_string.h"
#include "ogr_attrind.h"
#include "ogr_core.h"
#include "ogr_p.h"
#include "ogrsf_frmts.h"

//! @cond Doxygen_Suppress

CPL_CVSID("$Id$")

/************************************************************************/
/*     Support for special attributes (feature query and selection)     */
/************************************************************************/

const char *const SpecialFieldNames[SPECIAL_FIELD_COUNT] = {
    "FID", "OGR_GEOMETRY", "OGR_STYLE", "OGR_GEOM_WKT", "OGR_GEOM_AREA"};
const swq_field_type SpecialFieldTypes[SPECIAL_FIELD_COUNT] = {
    SWQ_INTEGER, SWQ_STRING, SWQ_STRING, SWQ_STRING, SWQ_FLOAT};

/************************************************************************/
/*                          OGRFeatureQuery()                           */
/************************************************************************/

OGRFeatureQuery::OGRFeatureQuery() :
    poTargetDefn(NULL),
    pSWQExpr(NULL)
{}

/************************************************************************/
/*                          ~OGRFeatureQuery()                          */
/************************************************************************/

OGRFeatureQuery::~OGRFeatureQuery()

{
    delete static_cast<swq_expr_node *>(pSWQExpr);
}

/************************************************************************/
/*                                Parse                                 */
/************************************************************************/

OGRErr
OGRFeatureQuery::Compile( OGRFeatureDefn *poDefn,
                          const char * pszExpression,
                          int bCheck,
                          swq_custom_func_registrar *poCustomFuncRegistrar )

{
    // Clear any existing expression.
    if( pSWQExpr != NULL )
    {
        delete static_cast<swq_expr_node *>(pSWQExpr);
        pSWQExpr = NULL;
    }

    // Build list of fields.
    const int nFieldCount =
        poDefn->GetFieldCount() + SPECIAL_FIELD_COUNT +
        poDefn->GetGeomFieldCount();

    char **papszFieldNames = static_cast<char **>(
        CPLMalloc(sizeof(char *) * nFieldCount ));
    swq_field_type *paeFieldTypes = static_cast<swq_field_type *>(
        CPLMalloc(sizeof(swq_field_type) * nFieldCount));

    for( int iField = 0; iField < poDefn->GetFieldCount(); iField++ )
    {
        OGRFieldDefn *poField = poDefn->GetFieldDefn(iField);

        papszFieldNames[iField] = const_cast<char *>(poField->GetNameRef());

        switch( poField->GetType() )
        {
          case OFTInteger:
          {
              if( poField->GetSubType() == OFSTBoolean )
                  paeFieldTypes[iField] = SWQ_BOOLEAN;
              else
                  paeFieldTypes[iField] = SWQ_INTEGER;
              break;
          }

          case OFTInteger64:
          {
              if( poField->GetSubType() == OFSTBoolean )
                  paeFieldTypes[iField] = SWQ_BOOLEAN;
              else
                  paeFieldTypes[iField] = SWQ_INTEGER64;
              break;
          }

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

    int iField = 0;
    while( iField < SPECIAL_FIELD_COUNT )
    {
        papszFieldNames[poDefn->GetFieldCount() + iField] =
            const_cast<char *>(SpecialFieldNames[iField]);
        paeFieldTypes[poDefn->GetFieldCount() + iField] =
            (iField == SPF_FID) ? SWQ_INTEGER64 : SpecialFieldTypes[iField];
        ++iField;
    }

    for( iField = 0; iField < poDefn->GetGeomFieldCount(); iField++ )
    {
        OGRGeomFieldDefn *poField = poDefn->GetGeomFieldDefn(iField);
        const int iDstField =
            poDefn->GetFieldCount() + SPECIAL_FIELD_COUNT + iField;

        papszFieldNames[iDstField] = const_cast<char *>(poField->GetNameRef());
        if( *papszFieldNames[iDstField] == '\0' )
            papszFieldNames[iDstField] =
                const_cast<char *>(OGR_GEOMETRY_DEFAULT_NON_EMPTY_NAME);
        paeFieldTypes[iDstField] = SWQ_GEOMETRY;
    }

    // Try to parse.
    poTargetDefn = poDefn;
    const CPLErr eCPLErr =
        swq_expr_compile(pszExpression, nFieldCount,
                         papszFieldNames, paeFieldTypes,
                         bCheck,
                         poCustomFuncRegistrar,
                         reinterpret_cast<swq_expr_node **>(&pSWQExpr));

    OGRErr eErr = OGRERR_NONE;
    if( eCPLErr != CE_None )
    {
        eErr = OGRERR_CORRUPT_DATA;
        pSWQExpr = NULL;
    }

    CPLFree(papszFieldNames);
    CPLFree(paeFieldTypes);

    return eErr;
}

/************************************************************************/
/*                         OGRFeatureFetcher()                          */
/************************************************************************/

static swq_expr_node *OGRFeatureFetcher( swq_expr_node *op, void *pFeatureIn )

{
    OGRFeature *poFeature = static_cast<OGRFeature *>(pFeatureIn);

    if( op->field_type == SWQ_GEOMETRY )
    {
        const int iField =
            op->field_index -
            (poFeature->GetFieldCount() + SPECIAL_FIELD_COUNT);
        swq_expr_node *poRetNode =
            new swq_expr_node(poFeature->GetGeomFieldRef(iField));
        return poRetNode;
    }

    swq_expr_node *poRetNode = NULL;
    switch( op->field_type )
    {
      case SWQ_INTEGER:
      case SWQ_BOOLEAN:
        poRetNode = new swq_expr_node(
            poFeature->GetFieldAsInteger(op->field_index) );
        break;

      case SWQ_INTEGER64:
        poRetNode = new swq_expr_node(
            poFeature->GetFieldAsInteger64(op->field_index) );
        break;

      case SWQ_FLOAT:
        poRetNode = new swq_expr_node(
            poFeature->GetFieldAsDouble(op->field_index) );
        break;

      case SWQ_TIMESTAMP:
        poRetNode = new swq_expr_node(
            poFeature->GetFieldAsString(op->field_index) );
        poRetNode->MarkAsTimestamp();
        break;

      default:
        poRetNode = new swq_expr_node(
            poFeature->GetFieldAsString(op->field_index) );
        break;
    }

    poRetNode->is_null = !(poFeature->IsFieldSetAndNotNull(op->field_index));

    return poRetNode;
}

/************************************************************************/
/*                              Evaluate()                              */
/************************************************************************/

int OGRFeatureQuery::Evaluate( OGRFeature *poFeature )

{
    if( pSWQExpr == NULL )
        return FALSE;

    swq_expr_node *poResult =
        static_cast<swq_expr_node *>(pSWQExpr)->
            Evaluate(OGRFeatureFetcher, poFeature);

    if( poResult == NULL )
        return FALSE;

    bool bLogicalResult = false;
    if( poResult->field_type == SWQ_INTEGER ||
        poResult->field_type == SWQ_INTEGER64 ||
        poResult->field_type == SWQ_BOOLEAN )
        bLogicalResult = CPL_TO_BOOL(static_cast<int>(poResult->int_value));

    delete poResult;

    return bLogicalResult;
}

/************************************************************************/
/*                            CanUseIndex()                             */
/************************************************************************/

int OGRFeatureQuery::CanUseIndex( OGRLayer *poLayer )
{
    swq_expr_node *psExpr = static_cast<swq_expr_node *>(pSWQExpr);

    // Do we have an index on the targeted layer?
    if( poLayer->GetIndex() == NULL )
        return FALSE;

    return CanUseIndex(psExpr, poLayer);
}

int OGRFeatureQuery::CanUseIndex( swq_expr_node *psExpr,
                                  OGRLayer *poLayer )
{
    // Does the expression meet our requirements?
    if( psExpr == NULL || psExpr->eNodeType != SNT_OPERATION )
        return FALSE;

    if( (psExpr->nOperation == SWQ_OR || psExpr->nOperation == SWQ_AND) &&
         psExpr->nSubExprCount == 2 )
    {
        return CanUseIndex(psExpr->papoSubExpr[0], poLayer) &&
               CanUseIndex(psExpr->papoSubExpr[1], poLayer);
    }

    if( !(psExpr->nOperation == SWQ_EQ || psExpr->nOperation == SWQ_IN)
        || psExpr->nSubExprCount < 2 )
        return FALSE;

    swq_expr_node *poColumn = psExpr->papoSubExpr[0];
    swq_expr_node *poValue = psExpr->papoSubExpr[1];

    if( poColumn->eNodeType != SNT_COLUMN
        || poValue->eNodeType != SNT_CONSTANT )
        return FALSE;

    OGRAttrIndex *poIndex =
        poLayer->GetIndex()->GetFieldIndex(poColumn->field_index);
    if( poIndex == NULL )
        return FALSE;

    // Have an index.
    return TRUE;
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

static int CompareGIntBig( const void *pa, const void *pb )
{
    const GIntBig a = *(reinterpret_cast<const GIntBig *>(pa));
    const GIntBig b = *(reinterpret_cast<const GIntBig *>(pb));
    if( a < b )
        return -1;
    else if( a > b )
        return 1;
    else
        return 0;
}

GIntBig *OGRFeatureQuery::EvaluateAgainstIndices( OGRLayer *poLayer,
                                                  OGRErr *peErr )

{
    swq_expr_node *psExpr = static_cast<swq_expr_node *>(pSWQExpr);

    if( peErr != NULL )
        *peErr = OGRERR_NONE;

    // Do we have an index on the targeted layer?
    if( poLayer->GetIndex() == NULL )
        return NULL;

    GIntBig nFIDCount = 0;
    return EvaluateAgainstIndices(psExpr, poLayer, nFIDCount);
}

// The input arrays must be sorted.
static
GIntBig* OGRORGIntBigArray( GIntBig panFIDList1[], GIntBig nFIDCount1,
                            GIntBig panFIDList2[], GIntBig nFIDCount2,
                            GIntBig& nFIDCount )
{
    const GIntBig nMaxCount = nFIDCount1 + nFIDCount2;
    GIntBig* panFIDList = static_cast<GIntBig *>(
        CPLMalloc(static_cast<size_t>(nMaxCount + 1) * sizeof(GIntBig)));
    nFIDCount = 0;

    for( GIntBig i1 = 0, i2 = 0; i1<nFIDCount1 || i2<nFIDCount2; )
    {
        if( i1 < nFIDCount1 && i2 < nFIDCount2 )
        {
            const GIntBig nVal1 = panFIDList1[i1];
            const GIntBig nVal2 = panFIDList2[i2];
            if( nVal1 < nVal2 )
            {
                if( i1 + 1 < nFIDCount1 && panFIDList1[i1+1] <= nVal2 )
                {
                    panFIDList[nFIDCount++] = nVal1;
                    i1++;
                }
                else
                {
                    panFIDList[nFIDCount++] = nVal1;
                    panFIDList[nFIDCount++] = nVal2;
                    i1++;
                    i2++;
                }
            }
            else if( nVal1 == nVal2 )
            {
                panFIDList[nFIDCount++] = nVal1;
                i1++;
                i2++;
            }
            else
            {
                if( i2 + 1 < nFIDCount2 && panFIDList2[i2+1] <= nVal1 )
                {
                    panFIDList[nFIDCount++] = nVal2;
                    i2++;
                }
                else
                {
                    panFIDList[nFIDCount++] = nVal2;
                    panFIDList[nFIDCount++] = nVal1;
                    i1++;
                    i2++;
                }
            }
        }
        else if( i1 < nFIDCount1 )
        {
            const GIntBig nVal1 = panFIDList1[i1];
            panFIDList[nFIDCount++] = nVal1;
            i1++;
        }
        else if( i2 < nFIDCount2 )
        {
            const GIntBig nVal2 = panFIDList2[i2];
            panFIDList[nFIDCount++] = nVal2;
            i2++;
        }
    }

    panFIDList[nFIDCount] = OGRNullFID;

    return panFIDList;
}

// The input arrays must be sorted.
static
GIntBig* OGRANDGIntBigArray( GIntBig panFIDList1[], GIntBig nFIDCount1,
                             GIntBig panFIDList2[], GIntBig nFIDCount2,
                             GIntBig& nFIDCount )
{
    GIntBig nMaxCount = std::max(nFIDCount1, nFIDCount2);
    GIntBig* panFIDList = static_cast<GIntBig *>(
        CPLMalloc(static_cast<size_t>(nMaxCount + 1) * sizeof(GIntBig)));
    nFIDCount = 0;

    for( GIntBig i1 = 0, i2 = 0; i1 < nFIDCount1 && i2 < nFIDCount2; )
    {
        const GIntBig nVal1 = panFIDList1[i1];
        const GIntBig nVal2 = panFIDList2[i2];
        if( nVal1 < nVal2 )
        {
            if( i1+1 < nFIDCount1 && panFIDList1[i1+1] <= nVal2 )
            {
                i1++;
            }
            else
            {
                i1++;
                i2++;
            }
        }
        else if( nVal1 == nVal2 )
        {
            panFIDList[nFIDCount++] = nVal1;
            i1++;
            i2++;
        }
        else
        {
            if( i2 + 1 < nFIDCount2 && panFIDList2[i2+1] <= nVal1 )
            {
                i2++;
            }
            else
            {
                i1++;
                i2++;
            }
        }
    }

    panFIDList[nFIDCount] = OGRNullFID;

    return panFIDList;
}

GIntBig *OGRFeatureQuery::EvaluateAgainstIndices( swq_expr_node *psExpr,
                                                  OGRLayer *poLayer,
                                                  GIntBig& nFIDCount )
{
    // Does the expression meet our requirements?
    if( psExpr == NULL ||
        psExpr->eNodeType != SNT_OPERATION )
        return NULL;

    if( (psExpr->nOperation == SWQ_OR || psExpr->nOperation == SWQ_AND) &&
         psExpr->nSubExprCount == 2 )
    {
        GIntBig nFIDCount1 = 0;
        GIntBig nFIDCount2 = 0;
        GIntBig* panFIDList1 =
            EvaluateAgainstIndices(psExpr->papoSubExpr[0], poLayer, nFIDCount1);
        GIntBig* panFIDList2 =
            panFIDList1 == NULL ? NULL :
            EvaluateAgainstIndices(psExpr->papoSubExpr[1], poLayer, nFIDCount2);
        GIntBig* panFIDList = NULL;
        if( panFIDList1 != NULL && panFIDList2 != NULL )
        {
            if( psExpr->nOperation == SWQ_OR )
                panFIDList = OGRORGIntBigArray(panFIDList1, nFIDCount1,
                                            panFIDList2, nFIDCount2, nFIDCount);
            else if( psExpr->nOperation == SWQ_AND )
                panFIDList = OGRANDGIntBigArray(panFIDList1, nFIDCount1,
                                            panFIDList2, nFIDCount2, nFIDCount);
        }
        CPLFree(panFIDList1);
        CPLFree(panFIDList2);
        return panFIDList;
    }

    if( !(psExpr->nOperation == SWQ_EQ || psExpr->nOperation == SWQ_IN)
        || psExpr->nSubExprCount < 2 )
        return NULL;

    swq_expr_node *poColumn = psExpr->papoSubExpr[0];
    swq_expr_node *poValue = psExpr->papoSubExpr[1];

    if( poColumn->eNodeType != SNT_COLUMN
        || poValue->eNodeType != SNT_CONSTANT )
        return NULL;

    OGRAttrIndex *poIndex =
        poLayer->GetIndex()->GetFieldIndex(poColumn->field_index);
    if( poIndex == NULL )
        return NULL;

    // Have an index, now we need to query it.
    OGRField sValue;
    OGRFieldDefn *poFieldDefn =
        poLayer->GetLayerDefn()->GetFieldDefn(poColumn->field_index);

    // Handle the case of an IN operation.
    if( psExpr->nOperation == SWQ_IN )
    {
        int nLength = 0;
        GIntBig *panFIDs = NULL;
        nFIDCount = 0;

        for( int iIN = 1; iIN < psExpr->nSubExprCount; iIN++ )
        {
            switch( poFieldDefn->GetType() )
            {
              case OFTInteger:
                if( psExpr->papoSubExpr[iIN]->field_type == SWQ_FLOAT )
                    sValue.Integer =
                        static_cast<int>(psExpr->papoSubExpr[iIN]->float_value);
                else
                    sValue.Integer =
                        static_cast<int>(psExpr->papoSubExpr[iIN]->int_value);
                break;

              case OFTInteger64:
                if( psExpr->papoSubExpr[iIN]->field_type == SWQ_FLOAT )
                    sValue.Integer64 = static_cast<GIntBig>(
                        psExpr->papoSubExpr[iIN]->float_value);
                else
                    sValue.Integer64 = psExpr->papoSubExpr[iIN]->int_value;
                break;

              case OFTReal:
                sValue.Real = psExpr->papoSubExpr[iIN]->float_value;
                break;

              case OFTString:
                sValue.String = psExpr->papoSubExpr[iIN]->string_value;
                break;

              default:
                CPLAssert(false);
                return NULL;
            }

            int nFIDCount32 = static_cast<int>(nFIDCount);
            panFIDs = poIndex->GetAllMatches(&sValue, panFIDs,
                                             &nFIDCount32, &nLength);
            nFIDCount = nFIDCount32;
        }

        if( nFIDCount > 1 )
        {
            // The returned FIDs are expected to be in sorted order.
            qsort(panFIDs, static_cast<size_t>(nFIDCount),
                  sizeof(GIntBig), CompareGIntBig);
        }
        return panFIDs;
    }

    // Handle equality test.
    switch( poFieldDefn->GetType() )
    {
      case OFTInteger:
        if( poValue->field_type == SWQ_FLOAT )
            sValue.Integer = static_cast<int>(poValue->float_value);
        else
            sValue.Integer = static_cast<int>(poValue->int_value);
        break;

      case OFTInteger64:
        if( poValue->field_type == SWQ_FLOAT )
            sValue.Integer64 = static_cast<GIntBig>(poValue->float_value);
        else
            sValue.Integer64 = poValue->int_value;
        break;

      case OFTReal:
        sValue.Real = poValue->float_value;
        break;

      case OFTString:
        sValue.String = poValue->string_value;
        break;

      default:
        CPLAssert(false);
        return NULL;
    }

    int nLength = 0;
    int nFIDCount32 = 0;
    GIntBig* panFIDs =
        poIndex->GetAllMatches(&sValue, NULL, &nFIDCount32, &nLength);
    nFIDCount = nFIDCount32;
    if( nFIDCount > 1 )
    {
        // The returned FIDs are expected to be sorted.
        // TODO(schwehr): Use std::sort.
        qsort(panFIDs, static_cast<size_t>(nFIDCount),
              sizeof(GIntBig), CompareGIntBig);
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
    swq_expr_node *op = static_cast<swq_expr_node *>(pBareOp);

    // References to tables other than the primarily are currently unsupported.
    // Error out.
    if( op->eNodeType == SNT_COLUMN )
    {
        if( op->table_index != 0 )
        {
            CSLDestroy( papszList );
            return NULL;
        }

        // Add the field name into our list if it is not already there.
        const char *pszFieldName = NULL;

        if( op->field_index >= poTargetDefn->GetFieldCount()
            && op->field_index <
            poTargetDefn->GetFieldCount() + SPECIAL_FIELD_COUNT )
        {
            pszFieldName =
                SpecialFieldNames[op->field_index -
                                  poTargetDefn->GetFieldCount()];
        }
        else if( op->field_index >= 0
                 && op->field_index < poTargetDefn->GetFieldCount() )
        {
            pszFieldName =
                poTargetDefn->GetFieldDefn(op->field_index)->GetNameRef();
        }
        else
        {
            CSLDestroy(papszList);
            return NULL;
        }

        if( CSLFindString(papszList, pszFieldName) == -1 )
            papszList = CSLAddString(papszList, pszFieldName);
    }

    // Add in fields from subexpressions.
    if( op->eNodeType == SNT_OPERATION )
    {
        for( int iSubExpr = 0; iSubExpr < op->nSubExprCount; iSubExpr++ )
        {
            papszList = FieldCollector(op->papoSubExpr[iSubExpr], papszList);
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
 * primary table then NULL is returned indicating an error.  In successful
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

//! @endcond
